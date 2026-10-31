#!/usr/bin/env python3
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026, Olivier Cochard-Labbé
#
"""Build the sword fight's two fencers in Blender and render their
sprite layers.

The game draws each fencer as layers, frame by frame, all 141x90 and on
the same corner (grp/my* the player, grp/he* the opponent): the body,
the weapon (rapier, cutlass or longsword), the wounds (head, chest,
legs) and, for the opponent, a head of its own per scene.  Its 16 moves
(guard, step back, slash, lunge, each in the middle, low and high;
advance, retreat, fall, and the last) have the frames of BANKS.

A fencer here is a jointed figure, posed frame by frame from key poses
in a low, wide stance: each attack winds up, strikes on the frame the
pack's table says and recovers, the whole body going with the blade.
He is seen from the side, turned a little toward the viewer, without
perspective.  Each layer is
rendered alone, the others hiding what they cover (holdouts), so a
weapon passing behind its fencer is cut as the game's is.

    blender -b -P fencer.py -- --side my --out DIR [--frame 317x203]
                            [--blend MODEL.blend] [--moves 0,1,...]
                            [--margin PX] [--steps JSON] [--frames JSON]
                            [--strikes JSON] [--layers body,...]

writes DIR/LAYER/MOVE.FRAME.png for every layer of that side (or only
those of --layers, to look at).
"""

import argparse
import json
import math
import os
import sys

import bpy
from mathutils import Euler, Matrix, Quaternion, Vector

# The photographic materials (../materials.py, copied beside this file
# on the render host).
sys.path[:0] = [os.path.dirname(os.path.abspath(__file__)),
                os.path.dirname(os.path.dirname(os.path.abspath(__file__)))]
import materials			# noqa: E402

# What each part of the clothes is made of (materials.LIBRARY), None a
# plain material; the skin a little translucent.
FABRIC = {"shirt": "fencer-shirt", "sash": "fencer-sash",
          "breeches": "fencer-breeches", "boots": "fencer-boots",
          "hair": None, "skin": None}

BANKS = [6, 6, 6, 6, 6, 6, 6, 6, 6, 4, 4, 4, 6, 6, 10, 6]
# The fall's last frames (14.3 on), the arms raised in surrender, are
# 141x114, 24 lines taller than the others (the game draws them 24
# higher, so the feet stay where they were).
TALL_MOVE, TALL_FROM, TALL = 14, 3, 114 / 90
FRAME_M = 2.75		# the frame's width in metres (141 pixels)
# Where each fencer stands in his frame, as in the game's: the player
# right of the middle facing left, the opponent left of it facing right
# (their bodies' middles at about 110 and 30 of the frame's 141 pixels).
CAMERA_X = {"my": -0.77, "he": 0.79}
SIDE = "my"

# The two fencers' clothes: (shirt, sash, breeches, boots, hair).
LOOKS = {
    "my": {"shirt": (0.97, 0.96, 0.93), "sash": (0.9, 0.02, 0.015),
           "breeches": (0.06, 0.035, 0.02), "boots": (0.03, 0.025, 0.02),
           "hair": (0.22, 0.12, 0.05), "skin": (0.62, 0.40, 0.28)},
    "he": {"shirt": (0.06, 0.30, 0.10), "sash": (0.9, 0.02, 0.015),
           "breeches": (0.02, 0.02, 0.025), "boots": (0.02, 0.018, 0.015),
           "hair": (0.03, 0.025, 0.02), "skin": (0.55, 0.35, 0.24)},
}
# The fencers' clothes cut full: a billowing shirt, a wide sash and baggy
# breeches puffed at the knee (metres off the body, at the fullest), so
# that they fill their frames.
BAGGY = {"shirt": 0.05, "sash": 0.055, "breeches": 0.09, "boots": 0.012}
# The shirt's collar this high (metres): a low one, the neck seen.
NECKLINE = 1.42
# The opponent's heads, one per scene (hehead1 the deck's, hehead2 the
# alley's, hehead3 the fort's): what he wears on it.
HEADS = {"head1": "kerchief", "head2": "hat", "head3": "morion"}
# Their hair (human.HAIR): the player's long to the shoulders, the
# opponent's from under what he wears on his head.
HAIR = {"my": "long", "he": "under"}
# The opponent's kerchief's knot and its two ends (human.py's rest:
# metres from the knot, half sizes, tilts back and apart in degrees).
KNOT = (("knot", (0, 0, 0), (0.022, 0.018, 0.018), (0, 0)),
        ("end1", (0.012, 0.012, -0.05), (0.012, 0.004, 0.045), (-20, 15)),
        ("end2", (-0.012, 0.016, -0.045), (0.012, 0.004, 0.04), (-25, -15)))


def material(name, colour, rough=0.7, metal=0.0):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    bsdf = m.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = (*colour, 1.0)
    bsdf.inputs["Roughness"].default_value = rough
    bsdf.inputs["Metallic"].default_value = metal
    return m


def joint(name, parent, at):
    """A joint: an empty at "at" (in its parent's space) that its limbs
    hang from; it turns about the view's axis (Y)."""
    ob = bpy.data.objects.new(name, None)
    bpy.context.collection.objects.link(ob)
    ob.parent = parent
    ob.location = at
    return ob


ENDS = {}		# a limb's rounded ends, by the limb's name


def capsule(name, parent, length, r0, r1, mat, up=False, depth=0.0):
    """A limb from its joint, down (or up) "length" metres, tapering
    from r0 to r1, its ends rounded (ENDS)."""
    sign = 1 if up else -1
    bpy.ops.mesh.primitive_cone_add(vertices=16, radius1=r0, radius2=r1,
                                    depth=length,
                                    location=(0, depth, sign * length / 2))
    ob = bpy.context.object
    ob.name = name
    if not up:
        ob.rotation_euler = (math.pi, 0, 0)
    ob.data.materials.append(mat)
    for end, r in ((0, r0), (sign * length, r1)):
        bpy.ops.mesh.primitive_uv_sphere_add(radius=r, segments=16,
                                             ring_count=8,
                                             location=(0, depth, end))
        s = bpy.context.object
        s.name = name + "-end"
        s.data.materials.append(mat)
        s.parent = parent
        ENDS.setdefault(ob.name, []).append(s)
    ob.parent = parent
    return ob


def blob(name, parent, at, size, mat):
    """A rounded box (a head, a hand, a foot)."""
    bpy.ops.mesh.primitive_uv_sphere_add(radius=1, segments=24,
                                         ring_count=12, location=at)
    ob = bpy.context.object
    ob.name = name
    ob.scale = size
    ob.data.materials.append(mat)
    ob.parent = parent
    return ob


class Fencer:
    """The figure: its joints, and its parts sorted into layers."""

    def __init__(self, side):
        look = LOOKS[side]
        m = {k: materials.pbr(side + "-" + k, FABRIC.get(k), v,
                              rough=0.55 if k == "skin" else 0.8,
                              sss=0.15 if k == "skin" else 0.0)
             for k, v in look.items()}
        self.side = side
        self.layers = {"body": [], "weapon": {}, "wounds": {}, "heads": {}}
        body = self.layers["body"]

        def keep(ob):
            body.append(ob)
            body.extend(ENDS.get(ob.name, []))
            return ob

        self.root = joint(side + "-root", None, (0, 0, 0.9))
        j = self.j = {}
        # The torso up from the hips, the head on it.
        j["spine"] = joint("spine", self.root, (0, 0, 0))
        keep(capsule("torso", j["spine"], 0.52, 0.17, 0.2, m["shirt"],
                     up=True))
        keep(capsule("sash", j["spine"], 0.1, 0.18, 0.18, m["sash"],
                     up=True))
        # The neck from inside the shoulders up into the head, so they
        # join at every pose.
        j["neck"] = joint("neck", j["spine"], (0, 0, 0.50))
        keep(capsule("neckp", j["neck"], 0.2, 0.075, 0.07, m["skin"],
                     up=True))
        head = [blob("head", j["neck"], (0.02, 0, 0.23), (0.1, 0.1, 0.12),
                     m["skin"])]
        head.append(blob("hair", j["neck"], (-0.02, 0, 0.27),
                         (0.105, 0.105, 0.09), m["hair"]))
        if side == "my":
            # Long hair to the shoulders, part of the body.
            head.append(blob("locks", j["neck"], (-0.07, 0, 0.17),
                             (0.06, 0.09, 0.1), m["hair"]))
            body.extend(head)
        else:
            # His head is a layer of its own per scene, over his body.
            for name, wear in HEADS.items():
                parts = [blob(name + "-face", j["neck"], (0.02, 0, 0.23),
                              (0.1, 0.1, 0.12), m["skin"]),
                         blob(name + "-beard", j["neck"], (0.05, 0, 0.16),
                              (0.07, 0.09, 0.05), m["hair"])]
                parts += self.headwear(name, wear, j["neck"])
                self.layers["heads"][name] = parts
            for ob in head:
                bpy.data.objects.remove(ob, do_unlink=True)
        # Arms: the sword arm in front of the body, the other behind.
        for arm, d in (("front", -0.22), ("back", 0.22)):
            j[arm + "-shoulder"] = joint(arm + "-shoulder", j["spine"],
                                         (0, d, 0.47))
            keep(capsule(arm + "-upper", j[arm + "-shoulder"], 0.3, 0.075,
                         0.065, m["shirt"]))
            j[arm + "-elbow"] = joint(arm + "-elbow",
                                      j[arm + "-shoulder"], (0, 0, -0.3))
            keep(capsule(arm + "-fore", j[arm + "-elbow"], 0.27, 0.06,
                         0.045, m["shirt"]))
            j[arm + "-wrist"] = joint(arm + "-wrist", j[arm + "-elbow"],
                                      (0, 0, -0.27))
            keep(blob(arm + "-hand", j[arm + "-wrist"], (0, 0, -0.05),
                      (0.04, 0.04, 0.055), m["skin"]))
        # Legs: the front leg nearer the view.
        for leg, d in (("front", -0.12), ("back", 0.12)):
            j[leg + "-hip"] = joint(leg + "-hip", self.root, (0, d, 0))
            keep(capsule(leg + "-thigh", j[leg + "-hip"], 0.45, 0.1, 0.075,
                         m["breeches"]))
            j[leg + "-knee"] = joint(leg + "-knee", j[leg + "-hip"],
                                     (0, 0, -0.45))
            keep(capsule(leg + "-shin", j[leg + "-knee"], 0.42, 0.07, 0.055,
                         m["boots"]))
            j[leg + "-ankle"] = joint(leg + "-ankle", j[leg + "-knee"],
                                      (0, 0, -0.42))
            keep(blob(leg + "-foot", j[leg + "-ankle"], (0.08, 0, -0.04),
                      (0.13, 0.05, 0.045), m["boots"]))
        self.weapons(j["front-wrist"])
        self.wounds(m)

    def headwear(self, name, wear, neck):
        cloth = material(name + "-wear", {"kerchief": (0.55, 0.05, 0.04),
                                          "hat": (0.05, 0.04, 0.03),
                                          "morion": (0.55, 0.55, 0.58)}[wear],
                         0.3 if wear == "morion" else 0.85,
                         0.9 if wear == "morion" else 0.0)
        if wear == "kerchief":
            return [blob(name + "-kerchief", neck, (-0.01, 0, 0.29),
                         (0.11, 0.11, 0.07), cloth)]
        if wear == "hat":
            return [blob(name + "-crown", neck, (0, 0, 0.35),
                         (0.08, 0.08, 0.07), cloth),
                    blob(name + "-brim", neck, (0, 0, 0.31),
                         (0.2, 0.2, 0.015), cloth)]
        return [blob(name + "-helmet", neck, (0, 0, 0.31),
                     (0.12, 0.12, 0.09), cloth),
                blob(name + "-comb", neck, (0, 0, 0.39),
                     (0.08, 0.01, 0.05), cloth),
                blob(name + "-rim", neck, (0, 0, 0.27),
                     (0.19, 0.12, 0.02), cloth)]

    def weapons(self, wrist):
        """The three blades in the sword hand, pointing on from the
        forearm."""
        steel = material("steel", (0.75, 0.75, 0.78), 0.2, 1.0)
        brass = material("brass", (0.7, 0.5, 0.18), 0.3, 1.0)
        grip = joint("grip", wrist, (0, 0, -0.06))
        self.j["grip"] = grip
        for name, length, width, guard in (("rapier", 1.0, 0.012, "cup"),
                                           ("cutlass", 0.72, 0.035, "shell"),
                                           ("sword", 0.95, 0.03, "cross")):
            blade = capsule(name + "-blade", grip, length, width,
                            width * 0.4, steel)
            parts = [blade] + ENDS.get(blade.name, [])
            if guard == "cup":
                parts.append(blob(name + "-cup", grip, (0, 0, -0.02),
                                  (0.06, 0.06, 0.03), steel))
            elif guard == "shell":
                parts.append(blob(name + "-shell", grip, (0, 0, -0.02),
                                  (0.05, 0.07, 0.025), brass))
            else:
                cross = capsule(name + "-cross", grip, 0.01, 0.015, 0.015,
                                brass)
                parts += [cross] + ENDS.get(cross.name, [])
                parts.append(blob(name + "-quillons", grip, (0, 0, -0.01),
                                  (0.012, 0.12, 0.012), brass))
            self.layers["weapon"][name] = parts

    def wounds(self, m):
        blood = material("blood", (0.35, 0.0, 0.0), 0.5)
        j = self.j
        self.layers["wounds"] = {
            "head": [blob("cut-head", j["neck"], (0.06, -0.11, 0.26),
                          (0.04, 0.01, 0.03), blood)],
            "chest": [blob("cut-chest", j["spine"], (0.12, -0.2, 0.35),
                           (0.07, 0.01, 0.05), blood)],
            "legs": [blob("cut-legs", j["front-hip"], (0.05, -0.1, -0.25),
                          (0.06, 0.01, 0.06), blood)],
        }

    def pose(self, p):
        """Set a pose: angles in degrees, positive towards the fencer's
        front, limbs hanging at 0; the hips at (x, height)."""
        self.root.location = (p.get("x", 0.0), 0, p.get("hips", 0.9))
        for name, ob in self.j.items():
            if name in ("grip",):
                continue
            ob.rotation_euler = (0, -math.radians(p.get(name, 0.0)), 0)
        self.j["grip"].rotation_euler = (
            0, -math.radians(p.get("blade", 0.0)), 0)


def blood_material(name, centre, r):
    """Wet blood soaked into cloth: dark red, glossy where fresh, its
    edge ragged by noise, thinning away from the cut and down."""
    m = bpy.data.materials.new("blood-" + name)
    m.use_nodes = True
    m.blend_method = "HASHED" if hasattr(m, "blend_method") else None
    nt = m.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    bsdf.inputs["Roughness"].default_value = 0.18
    coord = nt.nodes.new("ShaderNodeTexCoord")
    # The distance from the cut, stretched downward as blood runs.
    sub = nt.nodes.new("ShaderNodeVectorMath")
    sub.operation = "SUBTRACT"
    sub.inputs[1].default_value = tuple(centre)
    nt.links.new(coord.outputs["Object"], sub.inputs[0])
    sc = nt.nodes.new("ShaderNodeVectorMath")
    sc.operation = "MULTIPLY"
    sc.inputs[1].default_value = (1 / r, 1 / r, 1 / (r * 1.7))
    nt.links.new(sub.outputs[0], sc.inputs[0])
    ln = nt.nodes.new("ShaderNodeVectorMath")
    ln.operation = "LENGTH"
    nt.links.new(sc.outputs[0], ln.inputs[0])
    noise = nt.nodes.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = 3.0 / r
    noise.inputs["Detail"].default_value = 10.0
    noise.inputs["Roughness"].default_value = 0.65
    nt.links.new(coord.outputs["Object"], noise.inputs["Vector"])
    edge = nt.nodes.new("ShaderNodeMath")
    edge.operation = "MULTIPLY_ADD"
    edge.inputs[1].default_value = 0.8
    edge.inputs[2].default_value = -0.4
    nt.links.new(noise.outputs["Fac"], edge.inputs[0])
    d = nt.nodes.new("ShaderNodeMath")
    d.operation = "ADD"
    nt.links.new(ln.outputs["Value"], d.inputs[0])
    nt.links.new(edge.outputs[0], d.inputs[1])
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].position = 0.55
    ramp.color_ramp.elements[0].color = (1, 1, 1, 1)
    ramp.color_ramp.elements[1].position = 0.95
    ramp.color_ramp.elements[1].color = (0, 0, 0, 1)
    nt.links.new(d.outputs[0], ramp.inputs["Fac"])
    nt.links.new(ramp.outputs["Color"], bsdf.inputs["Alpha"])
    # Darker, nearly black where it is thickest, at the cut.
    col = nt.nodes.new("ShaderNodeValToRGB")
    col.color_ramp.elements[0].position = 0.0
    col.color_ramp.elements[0].color = (0.06, 0.0, 0.0, 1)
    col.color_ramp.elements[1].position = 0.8
    col.color_ramp.elements[1].color = (0.32, 0.01, 0.01, 1)
    nt.links.new(d.outputs[0], col.inputs["Fac"])
    nt.links.new(col.outputs["Color"], bsdf.inputs["Base Color"])
    return m


def tube(name, pts, radius, mat, parent):
    """A rod along "pts" (metres, in the parent's space), round, of
    "radius", made a mesh."""
    cu = bpy.data.curves.new(name, "CURVE")
    cu.dimensions = "3D"
    cu.bevel_depth = radius
    cu.bevel_resolution = 4
    cu.use_fill_caps = True
    sp = cu.splines.new("NURBS")
    sp.points.add(len(pts) - 1)
    for p_, xyz in zip(sp.points, pts):
        p_.co = (*xyz, 1)
    sp.use_endpoint_u = True
    sp.order_u = min(4, len(pts))
    cu.materials.append(mat)
    tmp = bpy.data.objects.new(name + "-curve", cu)
    bpy.context.collection.objects.link(tmp)
    # A mesh, as the layers' passes show and hide only meshes.
    dg = bpy.context.evaluated_depsgraph_get()
    me = bpy.data.meshes.new_from_object(tmp.evaluated_get(dg))
    bpy.data.objects.remove(tmp, do_unlink=True)
    ob = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(ob)
    ob.parent = parent
    for f in me.polygons:
        f.use_smooth = True
    return ob


def blade(name, length, width, thick, mat, parent, curve=0.0, point=0.12,
          edge_back=False, fuller=False):
    """A blade down from the guard (-Z), its flat to the view (width along
    X, thickness along Y): a lens section, tapering to its point over the
    last "point" of its length; bowed forward by "curve" metres at its
    tip; "edge_back" a cutlass's thick back, "fuller" a groove down a
    longsword's middle (shown as a darker flat)."""
    import bmesh
    bm = bmesh.new()
    n = 24
    rings = []
    for i in range(n + 1):
        t = i / n
        z = -0.01 - t * length
        w = width * (1 - 0.35 * t)
        if t > 1 - point:
            w *= max(0.02, (1 - t) / point)
        x0 = curve * t * t
        th = thick * (1 - 0.5 * t)
        sec = [(x0 + w / 2, 0, z), (x0, th / 2, z), (x0 - w / 2 *
               (0.3 if edge_back else 1), th / 2 if edge_back else 0, z),
               (x0, -th / 2, z)]
        if edge_back:
            sec = [(x0 + w / 2, 0, z), (x0, th / 2, z),
                   (x0 - w / 2, th / 2, z), (x0 - w / 2, -th / 2, z),
                   (x0, -th / 2, z)]
        rings.append([bm.verts.new(v) for v in sec])
    for a_, b_ in zip(rings, rings[1:]):
        k = len(a_)
        for j in range(k):
            bm.faces.new((a_[j], a_[(j + 1) % k], b_[(j + 1) % k], b_[j]))
    bm.faces.new(rings[0][::-1])
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(ob)
    ob.data.materials.append(mat)
    ob.parent = parent
    for f in me.polygons:
        f.use_smooth = True
    return ob


def steel_material(name, rough=0.22):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b_ = m.node_tree.nodes["Principled BSDF"]
    b_.inputs["Base Color"].default_value = (0.78, 0.78, 0.8, 1)
    b_.inputs["Metallic"].default_value = 1.0
    b_.inputs["Roughness"].default_value = rough
    return m


def swords(parent):
    """The three blades of the fight, at the grip (their hilts above it,
    their blades down from it), as (name, [objects]).  After period
    pieces: a swept-hilt rapier (a long slender blade, quillons, a knuckle
    bow and side rings, a wire grip, a pear pommel); a sea cutlass (a
    short broad curved blade, a brass shell guard and knuckle bow, a
    wooden grip); a straight double-edged sword (a fullered blade, a
    straight crossguard, a leather grip, a wheel pommel)."""
    steel = steel_material("blade-steel")
    dark = steel_material("hilt-steel", 0.35)
    brass = materials.pbr("hilt-brass", "gilt", (0.75, 0.55, 0.22), 0.3,
                          1.0)
    grip = materials.pbr("grip-leather", "leather", (0.12, 0.07, 0.04),
                         0.6)
    wood = materials.pbr("grip-wood", "planks", (0.3, 0.17, 0.08), 0.6)
    out = {}

    def ball(name, at, size, mat):
        ob = blob(name, parent, at, size, mat)
        for f in ob.data.polygons:
            f.use_smooth = True
        return ob

    def handle(name, length, r, mat):
        bpy.ops.mesh.primitive_cylinder_add(vertices=16, radius=r,
                                            depth=length,
                                            location=(0, 0, length / 2))
        ob = bpy.context.object
        ob.name = name
        ob.data.materials.append(mat)
        ob.parent = parent
        for f in ob.data.polygons:
            f.use_smooth = True
        return ob

    # The rapier.
    out["rapier"] = [
        blade("rapier-blade", 1.02, 0.022, 0.007, steel, parent,
              point=0.1),
        handle("rapier-grip", 0.1, 0.013, dark),
        ball("rapier-pommel", (0, 0, 0.12), (0.022, 0.022, 0.028), dark),
        tube("rapier-quillons", [(0, -0.12, -0.01), (0, -0.05, -0.02),
                                 (0, 0.05, -0.02), (0, 0.12, 0.0)],
             0.005, dark, parent),
        tube("rapier-bow", [(0, 0.02, -0.02), (0.07, 0.03, 0.0),
                            (0.07, 0.03, 0.08), (0.0, 0.0, 0.11)],
             0.0045, dark, parent),
        tube("rapier-ring", [(0.0, 0.0, -0.02), (0.05, -0.03, -0.05),
                             (0.0, -0.05, -0.08), (-0.05, -0.03, -0.05),
                             (0.0, 0.0, -0.02)], 0.004, dark, parent),
    ]
    # The cutlass.
    bpy.ops.mesh.primitive_uv_sphere_add(radius=1, segments=32,
                                         ring_count=16,
                                         location=(0.0, 0.0, -0.02))
    shell = bpy.context.object
    shell.name = "cutlass-shell"
    shell.scale = (0.075, 0.06, 0.03)
    shell.data.materials.append(brass)
    shell.parent = parent
    for f in shell.data.polygons:
        f.use_smooth = True
    out["cutlass"] = [
        blade("cutlass-blade", 0.7, 0.042, 0.008, steel, parent,
              curve=0.06, point=0.18, edge_back=True),
        handle("cutlass-grip", 0.11, 0.015, wood),
        ball("cutlass-cap", (0, 0, 0.115), (0.02, 0.02, 0.015), brass),
        shell,
        tube("cutlass-bow", [(0.06, 0.0, -0.02), (0.07, 0.0, 0.05),
                             (0.04, 0.0, 0.11), (0.0, 0.0, 0.12)],
             0.005, brass, parent),
    ]
    # The straight sword.
    out["sword"] = [
        blade("sword-blade", 0.92, 0.042, 0.008, steel, parent,
              point=0.14, fuller=True),
        handle("sword-grip", 0.14, 0.015, grip),
        ball("sword-pommel", (0, 0, 0.16), (0.03, 0.012, 0.03), dark),
        tube("sword-cross", [(0.11, 0.0, -0.01), (0.04, 0.0, -0.012),
                             (-0.04, 0.0, -0.012), (-0.11, 0.0, -0.01)],
             0.007, dark, parent),
    ]
    return out


GRIP_SHIFT = (-0.028, 0.0, 0.0)	# the grip from the hand bone, at rest
# The sash's end, as the original's hangs by his back leg: its width and
# thickness (metres), its top above the back hip, its bottom above the
# deck; how far behind the hip joint it hangs, the body hiding its top
# as in the original, swung out or not.
SASH_TAIL = (0.04, 0.006, 0.05, 0.2)
TAIL_BEHIND = 0.02


class HumanFencer(Fencer):
    """The same fencer on the pack's real body (human.py): one rig for
    both, the player's mirrored to face left with his sword arm still
    nearer the view.  Its layers are Fencer's: the body (the
    opponent's headless), his heads one per scene (a beard, and a
    kerchief, a hat or a morion), the three blades in the sword hand, and
    the wounds."""

    def __init__(self, side):
        import human
        self.side = side
        look = LOOKS[side]
        self.layers = {"body": [], "weapon": {}, "wounds": {}, "heads": {}}
        rob, meshes = human.build(look, kerchief=None, loose=BAGGY,
                                   neckline=NECKLINE, style=HAIR[side])
        body = meshes[0]
        self.rob = self.root = rob
        self.j = {}
        bpy.context.view_layer.update()
        # The sword's grip, from the sword hand, turned by "blade".
        bpy.context.view_layer.objects.active = rob
        bpy.ops.object.mode_set(mode="EDIT")
        # In the closed hand: at the palm, below the knuckles; the left
        # one when he is fitted to the original's frontal stance
        # (human.pose()'s straddle).
        self.hand = "L" if TRACE else "R"
        hand = rob.data.edit_bones["hand." + self.hand]
        g = rob.data.edit_bones.new("grip")
        g.head = hand.head + (hand.tail - hand.head) * 0.7
        g.tail = g.head + Vector((0, 0, -0.06))
        g.parent = hand
        g.use_deform = False
        bpy.ops.object.mode_set(mode="OBJECT")
        bpy.context.view_layer.update()
        if side == "he":
            self.split_heads(body, look)
        self.layers["body"].append(body)
        self.layers["body"] += human.worn(rob)
        self.tail = self.sash_tail()
        self.layers["body"].append(self.tail)
        if side != "he":		# his hair is on his heads
            self.layers["body"] += human.strands(rob)
        # The blades, built at the grip's rest place, then carried by it.
        bpy.context.view_layer.update()
        grip_at = rob.matrix_world @ rob.data.bones["grip"].head_local
        # In the fist, not on its knuckles: a little towards the palm.
        tmp = joint("grip-rest", None, tuple(grip_at + Vector(GRIP_SHIFT)))
        self.j = {}
        self.layers["weapon"] = swords(tmp)
        bpy.context.view_layer.update()
        # Each piece's place about the grip, for the sword let go.
        self.rest = {ob: tmp.matrix_world.inverted() @ ob.matrix_world
                     for parts in self.layers["weapon"].values()
                     for ob in parts}
        for parts in self.layers["weapon"].values():
            for ob in parts:
                self.carry(ob, "grip")
        self.held = {ob: ob.matrix_basis.copy() for ob in self.rest}
        # The hilt's place about the grip bone, and the longsword's tip
        # about the hilt (the original's weapon layer traced is its
        # sword), for fit().
        g = rob.matrix_world @ rob.data.bones["grip"].matrix_local
        self.hilt = g.inverted() @ tmp.matrix_world
        inv = tmp.matrix_world.inverted()
        self.tip = max((inv @ ob.matrix_world @ v.co
                        for ob in self.layers["weapon"]["sword"]
                        for v in ob.data.vertices), key=lambda v: v.length)
        bpy.data.objects.remove(tmp, do_unlink=True)
        self.j.pop("grip", None)
        self.wounds_on_body()

    def sash_tail(self):
        """The sash's end, a band hanging from its knot: a box one
        metre long, set in place by hang()."""
        import bmesh
        me = bpy.data.meshes.new("sash-tail")
        bm = bmesh.new()
        bmesh.ops.create_cube(bm, size=1.0)
        for v in bm.verts:
            v.co = Vector((v.co.x * SASH_TAIL[0], v.co.y * SASH_TAIL[1],
                           v.co.z - 0.5))
        bm.to_mesh(me)
        bm.free()
        # Plain, in the sash's red: the band has no texture coordinates.
        me.materials.append(materials.pbr("sash-tail", None,
                                          LOOKS[self.side]["sash"], 0.8))
        ob = bpy.data.objects.new("sash-tail", me)
        bpy.context.collection.objects.link(ob)
        return ob

    def hang(self, end=None):
        """The sash's end hanging from behind his back hip (TAIL_BEHIND):
        swung across the picture to "end" (world) as the traced original's
        is, else straight down to SASH_TAIL[3] above the deck."""
        bpy.context.view_layer.update()
        hip = self.joints()["back-hip"]
        top = Vector((hip.x, hip.y + TAIL_BEHIND, hip.z + SASH_TAIL[2]))
        d = Vector((end[0], 0, end[2])) - Vector((top.x, 0, top.z)) \
            if end else Vector((0, 0, SASH_TAIL[3] - top.z))
        self.tail.location = top
        self.tail.rotation_euler = (0, math.atan2(-d.x, -d.z), 0)
        self.tail.scale = (1, 1, max(0.05, d.length))

    def carry(self, ob, bone):
        """Parent "ob" to the rig's "bone", where it is now."""
        bpy.context.view_layer.update()
        mw = ob.matrix_world.copy()
        ob.parent = self.rob
        ob.parent_type = "BONE"
        ob.parent_bone = bone
        bpy.context.view_layer.update()
        ob.matrix_world = mw

    def knot(self, name, m, body):
        """The kerchief's knot at the back of the head, low, and its two
        ends hanging from it a little apart (KNOT), in material "m",
        carried by the head."""
        import human
        c, r = human._skull(body.data)
        at = Vector((c.x, c.y + r.y * 0.9, c.z - r.z * 0.45))
        out = []
        for part, off, size, tilt in KNOT:
            bpy.ops.mesh.primitive_uv_sphere_add(segments=16, ring_count=8)
            ob = bpy.context.active_object
            ob.name = "%s-knot-%s" % (name, part)
            turn = Euler((math.radians(tilt[0]), 0, math.radians(tilt[1])))
            ob.matrix_world = body.matrix_world @ \
                Matrix.Translation(at + Vector(off)) @ \
                turn.to_matrix().to_4x4() @ Matrix.Diagonal((*size, 1))
            ob.data.materials.append(m)
            for f in ob.data.polygons:
                f.use_smooth = True
            self.carry(ob, "head")
            out.append(ob)
        return out

    def split_heads(self, body, look):
        """The opponent's heads, one per scene: each a copy of his whole
        body, all but the head clear, a hair proud of it so it is drawn
        over his body's own head; with its headwear.  Nothing is cut, so
        there is no seam and the strands stay on."""
        import human
        clear = bpy.data.materials.new("clear")
        clear.use_nodes = True
        nt = clear.node_tree
        nt.nodes.remove(nt.nodes["Principled BSDF"])
        t = nt.nodes.new("ShaderNodeBsdfTransparent")
        nt.links.new(t.outputs[0], nt.nodes["Material Output"].inputs[0])
        rest = body.data.attributes["rest"]
        for name, wear in HEADS.items():
            h = body.copy()
            h.data = body.data.copy()
            h.name = "%s-head" % name
            bpy.context.collection.objects.link(h)
            me = h.data
            me.materials.append(clear)
            ci = len(me.materials) - 1
            for f in me.polygons:
                z = sum(rest.data[v].vector.z for v in f.vertices) / \
                    len(f.vertices)
                if z < 1.45:
                    f.material_index = ci
            for v in me.vertices:
                v.co += v.normal * 0.0015
            parts = [h] + human.strands(self.rob)
            if wear == "kerchief":
                # Under the kerchief, no hair standing through it.
                for mod in list(h.modifiers):
                    if mod.name == "hair-hair":
                        h.modifiers.remove(mod)
                m = materials.pbr(name + "-kerchief", "wool",
                                  (0.025, 0.025, 0.03), 0.85)
                me.materials.append(m)
                k = len(me.materials) - 1
                for f in me.polygons:
                    c = [rest.data[v].vector for v in f.vertices]
                    cz = sum(p.z for p in c) / len(c)
                    cy = sum(p.y for p in c) / len(c)
                    if cz > 1.6 or (cz > 1.53 and cy > 0.0):
                        f.material_index = k
                # Bare under it, as the original's: no hair, no beard,
                # only its knot at the back of the head.
                parts = [h] + self.knot(name, m, body)
            else:
                tmp = joint(name + "-rest", None, (0, -0.02, 1.33))
                for ob in Fencer.headwear(self, name, wear, tmp):
                    for f in ob.data.polygons:
                        f.use_smooth = True
                    sub = ob.modifiers.new("smooth", "SUBSURF")
                    sub.levels = sub.render_levels = 2
                    if wear == "morion":
                        ob.data.materials[0] = materials.pbr(
                            name + "-steel", "iron", (0.55, 0.55, 0.58),
                            0.35, 1.0)
                    else:
                        ob.data.materials[0] = materials.pbr(
                            name + "-felt", "wool", (0.05, 0.04, 0.03), 0.9)
                    bpy.context.view_layer.update()
                    self.carry(ob, "head")
                    parts.append(ob)
                bpy.data.objects.remove(tmp, do_unlink=True)
            self.layers["heads"][name] = parts
        # His body's own head is always under one of these: its hair
        # would stand out round a kerchief.
        for mod in list(body.modifiers):
            if mod.type == "PARTICLE_SYSTEM":
                body.modifiers.remove(mod)

    def wounds_on_body(self):
        """Each wound a stain on the clothes: the body's own surface
        round the cut, a hair off it, so it bends with the body; wet dark
        blood, ragged at its rim and running down."""
        import bmesh
        body = self.layers["body"][0]
        out = {}
        for name, at, r in (("head", (0.05, -0.07, 1.62), 0.035),
                            ("chest", (0.06, -0.14, 1.26), 0.075),
                            ("legs", (0.09, -0.08, 0.66), 0.07)):
            src = body if name != "head" else \
                self.layers["heads"].get("head1", [body])[0]
            ob = src.copy()
            ob.data = src.data.copy()
            ob.name = "cut-" + name
            bpy.context.collection.objects.link(ob)
            c = Vector(at)
            bm = bmesh.new()
            bm.from_mesh(ob.data)
            # Running down: the stain reaches lower than it does up.
            drop = [f for f in bm.faces if not (
                (f.calc_center_median() - c).length < r * 1.6 and
                (f.calc_center_median() - c).z > -r * 1.6 and
                (f.calc_center_median() - c).z < r * 0.9)]
            bmesh.ops.delete(bm, geom=drop, context="FACES")
            for v in bm.verts:
                v.co += v.normal * 0.003
            bm.to_mesh(ob.data)
            bm.free()
            ob.data.materials.clear()
            ob.data.materials.append(blood_material(name, c, r))
            for m in ob.modifiers:
                if m.type == "SUBSURF":
                    ob.modifiers.remove(m)
            out[name] = [ob]
        self.layers["wounds"] = out

    def pose(self, p):
        import human
        for ob, m in self.held.items():
            ob.matrix_basis = m
        self.pose_rig(p)
        if p.get("straddle"):
            self.look(p.get("look", LOOK))
        if not p.get("placed"):
            human.ground(self.rob)
        self.hang(p.get("tail"))
        if p.get("free"):
            self.free(p["free"])
        elif p.get("let_go"):
            self.let_go(p["let_go"])

    def pose_rig(self, p):
        """The rig alone in pose "p", the blade turned in the hand."""
        import human
        human.pose(self.rob, at(p, twist=p.get("twist", 0) + TWIST),
                   180 if self.side == "my" else 0)
        g = self.rob.pose.bones["grip"]
        axis = g.bone.matrix_local.to_3x3().inverted() @ Vector((1, 0, 0))
        g.rotation_mode = "AXIS_ANGLE"
        g.rotation_axis_angle = (-math.radians(p.get("blade", 0.0)), *axis)

    def look(self, a):
        """The head turned about its length so that the face looks "a"
        degrees from the viewer toward the opponent (away from him if
        negative), whatever the chest's twist."""
        pb = self.rob.pose.bones["head"]
        front = pb.bone.matrix_local.to_3x3().inverted() @ Vector((0, -1, 0))
        ahead = -1 if self.side == "my" else 1

        def yaw(a):
            pb.rotation_mode = "QUATERNION"
            pb.rotation_quaternion = Quaternion((0, 1, 0), math.radians(a))
            bpy.context.view_layer.update()
            f = (self.rob.matrix_world @ pb.matrix).to_3x3() @ front
            return math.degrees(math.atan2(ahead * f.x, -f.y))
        y0 = yaw(0)
        slope = (yaw(30) - y0) / 30
        yaw((a - y0) / slope)

    def joints(self):
        """The posed rig's joints and blade ends, in the world, by
        trace.py's names, the figure straddling (human.pose()): the
        limbs on the opponent's side "front-"."""
        mw = self.rob.matrix_world
        pb = self.rob.pose.bones

        def head(b):
            return mw @ pb[b].head
        out = {"neck": head("neck"),
               "nose": mw @ pb["head"].head.lerp(pb["head"].tail, 0.45)}
        for limb, arm, leg in (("front", "L", "L"), ("back", "R", "R")):
            out[limb + "-shoulder"] = head("upper." + arm)
            h = pb["hand." + arm]	# its palm, toward the body's middle
            out[limb + "-palm"] = (mw @ h.matrix).to_3x3() @ (
                h.bone.matrix_local.to_3x3().inverted() @
                Vector((-1 if arm == "L" else 1, 0, 0)))
            b = pb["upper." + arm]
            out[limb + "-arm-back"] = (mw @ b.matrix).to_3x3() @ (
                b.bone.matrix_local.to_3x3().inverted() @ Vector((0, 1, 0)))
            out[limb + "-elbow"] = head("fore." + arm)
            out[limb + "-wrist"] = head("hand." + arm)
            out[limb + "-hand"] = mw @ pb["fingers." + arm].tail
            out[limb + "-hip"] = head("thigh." + leg)
            out[limb + "-knee"] = head("shin." + leg)
            out[limb + "-ankle"] = head("foot." + leg)
            out[limb + "-toe"] = mw @ pb["foot." + leg].tail
        hilt = mw @ pb["grip"].matrix @ self.hilt
        out["hilt"] = hilt.translation
        out["tip"] = hilt @ self.tip
        return out

    def fit(self, t, start, hold=None):
        """The pose putting this figure's joints and blade where the
        traced frame "t" (trace.py) has the original's, seen by the
        camera, from pose "start" (the last frame's, so that the fit
        moves on from it); the angles in "hold" kept as given."""
        hold = hold or {}
        h = t["height"]
        pts = {k: v[:2] for k, v in t["points"].items() if v[2] >= FIT_SURE}
        blade = t["blade"]
        ahead = -1 if self.side == "my" else 1	# toward the opponent

        def far(a, b):
            return math.hypot(a[0] - b[0], a[1] - b[1])
        # The traced chains: the arm whose hand is at the hilt holds the
        # sword, without one the arm on the opponent's side is the front
        # one (else the arms cross); the leg out toward the opponent too.
        arms = [c for c in "rl" if c + "-wrist" in pts]
        sword = min(arms, key=lambda c: far(pts[c + "-wrist"], blade[0])) \
            if blade and arms else None
        held = sword is not None and \
            far(pts[sword + "-wrist"], blade[0]) < FIT_HOLD
        if not held:
            ends = [c for c in "rl" if c + "-shoulder" in pts]
            sword = max(ends, key=lambda c: ahead *
                        pts[c + "-shoulder"][0]) if ends else \
                ("r" if self.side == "my" else "l")
        legs = [c for c in "rl" if c + "-ankle" in pts]
        front = max(legs, key=lambda c: ahead * pts[c + "-ankle"][0]) \
            if legs else "l"
        chain = {"front": {"arm": sword, "leg": front},
                 "back": {"arm": "l" if sword == "r" else "r",
                          "leg": "l" if front == "r" else "r"}}
        # Of the hands hold opens, those raised above their shoulder: the
        # others stay fists (one held to the chest, its tip found across
        # the shirt).
        opened = {}
        for limb in ("front", "back"):
            c = chain[limb]["arm"]
            tip, top = pts.get(c + "-hand"), pts.get(c + "-shoulder")
            opened[limb] = bool(hold.get("open")) and bool(tip and top) \
                and tip[1] < top[1]
        want = {}
        for name, w in FIT_WEIGHTS.items():
            if name in ("neck", "nose"):
                key = name
            elif name in ("hilt", "tip"):
                if held:
                    want[name] = (blade[name == "tip"], w)
                continue
            else:
                limb, joint = name.split("-", 1)
                # The open hands reached for by their tips, the pose
                # model marking their wrists down the forearm; the fists
                # by their wrists.
                if (joint == "hand") != opened[limb] and \
                        joint in ("hand", "wrist"):
                    continue
                part = "leg" if joint in ("hip", "knee", "ankle", "toe") \
                    else "arm"
                key = chain[limb][part] + "-" + joint
            if key in pts:
                want[name] = (pts[key], w)
        s = FRAME_M / 141

        def seen(v):
            return ((v.x - CAMERA_X[SIDE]) / s + 70.5, h - (v.z + 0.04) / s)

        def cost(p):
            self.pose_rig(p)
            bpy.context.view_layer.update()
            got = self.joints()
            e = sum(w * far(seen(got[n]), at_) ** 2
                    for n, (at_, w) in want.items())
            # The picture shows no depth: the hands kept from going
            # behind him (away from the camera, +Y), the arms across
            # the picture rather than at the camera (but the sword's
            # forearm, the blade foreshortened as the original's is),
            # the back of each upper arm never turned to it.
            for n in ("front-wrist", "back-wrist", "front-elbow",
                      "back-elbow"):
                d = got[n].y - got["neck"].y - FIT_BEHIND
                if d > 0:
                    e += FIT_DEPTH * d * d
            for (limb, a, b), flat in tilt.items():
                v = (got[limb + "-" + b] - got[limb + "-" + a])
                d = abs(v.normalized().y) - flat
                if d > 0:
                    e += FIT_TILT * d * d
            if held:	# a sword held thumb up, not on an open palm
                d = got["front-palm"].z - FIT_PALM
                if d > 0:
                    e += FIT_TILT * d * d
            for limb in ("front", "back"):
                d = -got[limb + "-arm-back"].y - FIT_FLAT
                if d > 0:
                    e += FIT_TILT * d * d
            # Kept near the last frame's pose, so it moves smoothly.
            return e + FIT_KEEP * sum(
                ((p[k] - start.get(k, 0.0)) / FIT_STEP[k]) ** 2
                for k in FIT_STEP)

        hidden = [ob for ob in bpy.data.objects
                  if ob.type != "ARMATURE" and not ob.hide_viewport]
        for ob in hidden:
            ob.hide_viewport = True
        p = at(start, straddle=True, turn=TURN, placed=True,
               twist=start.get("twist", -TWIST),
               grip=100 if held else 10)
        p.pop("free", None)
        p.pop("let_go", None)
        for k in list(CRY) + ["look", "front-open", "back-open"]:
            p.pop(k, None)
        for k in FIT_STEP:
            p.setdefault(k, 0.0)
        p.update(hold)
        if "open" in p:
            p.update({k + "-open": v for k, v in opened.items()})
        # How far each arm bone may point out of the picture: as far
        # as the original draws it shorter than its full length (a fist
        # held out at the viewer), FIT_FLAT where it is drawn full.
        tilt = {}
        for limb in ("front", "back"):
            c = chain[limb]["arm"]
            for a, b in (("shoulder", "elbow"), ("elbow", "wrist")):
                if held and limb == "front" and a == "elbow":
                    continue	# the blade foreshortened as drawn
                ta, tb = pts.get(c + "-" + a), pts.get(c + "-" + b)
                shown = far(ta, tb) / drawn_length(a, b) if ta and tb \
                    else 1.0
                tilt[(limb, a, b)] = min(FIT_FLAT_MAX, max(
                    FIT_FLAT, math.sqrt(max(0.0, 1 - shown * shown)) +
                    FIT_FLAT_SLACK))
        limits = dict(FIT_LIMITS)
        if held:
            # The sword hand's knuckles kept up: rolled further, the
            # blade hangs under an open palm.
            limits["front-roll"] = FIT_GRIP_ROLL
            low, high = FIT_GRIP_ROLL
            if not low <= p["front-roll"] <= high:
                p["front-roll"] = (low + high) / 2

        def fit_keys(keys):
            return descend(p, [k for k in keys if k not in hold], cost,
                           limits)
        for keys in FIT_STAGES[:2]:
            p = fit_keys(keys)
        p = fit_keys(("neck",))
        # Each arm from a few rolls, the best kept: a hand raised needs
        # the elbow's bend turned up, which no small step finds.  The
        # sword arm only from its last roll, which stays in its range.
        for limb in ("front", "back"):
            keys = [k for k in FIT_STEP if k.startswith(limb + "-") and
                    k.split("-", 1)[1] in FIT_ARM] + \
                (["blade"] if limb == "front" else [])
            rolls = (p[limb + "-roll"],) if held and limb == "front" \
                else (p[limb + "-roll"], -80, 80)
            tries = [descend(at(p, **{limb + "-roll": r}), keys, cost,
                             limits) for r in rolls]
            p = min(tries, key=cost)
        p = fit_keys(FIT_STAGES[-1])
        for ob in hidden:
            ob.hide_viewport = False
        for leg in ("front", "back"):
            p[leg + "-ankle"] = -(p[leg + "-hip"] + p[leg + "-knee"])
        p.pop("tail", None)
        if t.get("tail"):
            p["tail"] = list(self.world(*t["tail"], h, s))
        if not held:
            if blade:
                p["free"] = [list(self.world(x, y, h, s)) for x, y in blade]
            else:
                p["free"] = "gone"
        return p

    def world(self, x, y, h, s):
        """The point of the picture at frame pixel (x, y), in the
        fencer's plane."""
        return Vector((CAMERA_X[SIDE] + (x - 70.5) * s, 0.0,
                       (h - y) * s - 0.04))

    def free(self, ends):
        """The sword out of his hand, its hilt and point at "ends"
        (world), or "gone" out of the picture."""
        flip = Matrix.Diagonal((-1 if self.side == "my" else 1, 1, 1, 1))
        if ends == "gone":
            m = Matrix.Translation((0, 0, -50))
        else:
            hilt, tip = (flip @ Vector(v) for v in ends)
            q = self.tip.normalized().rotation_difference(
                (tip - hilt).normalized())
            m = flip @ Matrix.Translation(hilt) @ q.to_matrix().to_4x4()
        for ob in self.rest:
            ob.matrix_world = m @ self.rest[ob]

    def let_go(self, t):
        """The sword dropped: "t" of the way from the hand (1 and past:
        landed), tossed a little up and turning over, to lying on the
        deck where his front foot stood, its point back under his
        stance (not under his opponent's) and its flat tilted to the
        view."""
        bpy.context.view_layer.update()
        # Worked out facing right: the player's figure is mirrored.
        flip = Matrix.Diagonal((-1 if self.side == "my" else 1, 1, 1, 1))
        first = next(iter(self.rest))
        hand = flip @ first.matrix_world @ self.rest[first].inverted()
        # The deck: the soles' lowest point, the body as posed.
        ev = self.layers["body"][0].evaluated_get(
            bpy.context.evaluated_depsgraph_get())
        floor = min((ev.matrix_world @ Vector(c)).z for c in ev.bound_box)
        # Lying: the blade (the grip's -Z) along the fight's line,
        # pointing back, its flat (the grip's Y) LIE_FLAT degrees up from
        # facing the view.
        a = math.radians(LIE_FLAT)
        z = Vector((1, 0, 0))
        y = Vector((0, -math.cos(a), math.sin(a)))
        lie = Matrix((y.cross(z), y, z)).transposed().to_4x4()
        # Where his front foot stood on guard, so it stays put while he
        # steps back from it.
        lie.translation = (STANCE["front"] + LIE_AHEAD, -LIE_NEAR, floor)
        # Landed, it bounces: its hilt kicks up off the deck, the point
        # staying on it, a little less each frame.
        after = round((t - 1) * FALLING) if t > 1 else 0
        rock = math.radians(LIE_ROCK[after - 1]) \
            if 0 < after <= len(LIE_ROCK) else 0.0
        t = min(t, 1.0)
        q = hand.to_quaternion().slerp(lie.to_quaternion(), t)
        for parts in self.layers["weapon"].values():
            # Each blade's lowest point on the deck.
            low = min((lie @ self.rest[ob] @ Vector(c)).z
                      for ob in parts for c in ob.bound_box)
            where = hand.translation.lerp(
                lie.translation + Vector((0, 0, floor - low)), t)
            where.z += LIE_TOSS * math.sin(math.pi * t)
            m = q.to_matrix().to_4x4()
            m.translation = where
            if rock:
                # About the point, the blade's end farthest back (-X).
                tip = min((m @ self.rest[ob] @ Vector(c)).x
                          for ob in parts for c in ob.bound_box)
                pivot = Vector((tip, where.y, floor))
                m = Matrix.Translation(pivot) @ \
                    Matrix.Rotation(-rock, 4, "Y") @ \
                    Matrix.Translation(-pivot) @ m
            m = flip @ m
            for ob in parts:
                ob.matrix_world = m @ self.rest[ob]


# The key poses.  Angles: the shoulders and elbows of each arm, the spine
# and neck, and the blade in the hand.  The legs are not angles but
# places, resolved by plant(): "_hip" how far the hips have gone forward
# from the guard's, "_front" and "_back" each foot's, "_drop" how much
# lower the hips are, "_lift" the front foot's height above the ground
# (metres), so that a key pose keeps its feet where they stand.
GUARD = {"hips": 0.86, "spine": 6, "neck": -6,
         "front-shoulder": 72, "front-elbow": 18, "front-wrist": 0,
         "blade": 5, "back-shoulder": -45, "back-elbow": 25,
         "back-wrist": 20, "grip": 100}
# The guard's stance: the ankles this far ahead of the hips (the back
# one behind), the hips this share of a straight leg above them: low and
# wide, so that every move carries the whole body.
STANCE = {"front": 0.56, "back": -0.56, "height": 0.70}
# The back foot on its ball, the heel this many degrees up.
HEEL = 10
# The fencers' chests turned this many degrees from the side toward the
# viewer, so that the shirt shows broad; the hips and legs stay side on,
# the stance spread across the picture.
TWIST = -40
# The sword let go in the surrender lies on the deck, its hilt this far
# (metres) ahead of where his front foot stood on guard, nearer the view
# than he is, its flat this
# many degrees up from facing the view; tossed this high on its way.
LIE_AHEAD, LIE_NEAR, LIE_FLAT, LIE_TOSS = 0.1, 0.3, 35, 0.25
# The hilt's kick off the deck (degrees, about the point) on the frames
# after the dropped sword lands.
LIE_ROCK = (14, 5)
# The fall's frame the sword leaves the hand after, and how many frames
# it takes to reach the deck.
LET_GO, FALLING = 2, 3
# Once it has, he cries out with his hands up: the mouth open, the
# fingers spread straight from unbent wrists, as the original's.
CRY = {"mouth": 1.0, "open": True, "front-wrist": 0.0, "back-wrist": 0.0}
# On these frames of the fall (first, last), he turns his head to watch
# it fly off behind him, then back to his opponent.
WATCH = {"my": (4, 6), "he": (4, 7)}


def crying(k):
    """The fall's frame k as CRY has it, the head turned back on WATCH's;
    nothing before the sword is let go."""
    if k <= LET_GO:
        return {}
    first, last = WATCH[SIDE]
    return dict(CRY, look=-LOOK) if first <= k <= last else dict(CRY)


# The traced original (trace.py, --trace): each frame's joints and
# blade, by "MOVE.FRAME"; the figure fitted to them (HumanFencer.fit()),
# its hips turned this many degrees toward the viewer, its legs bent
# out across the picture as the original's are.
TRACE = {}
DRAWN = {}	# drawn_length()'s, by joints
TURN = 80
# His head turned this many degrees from the viewer toward the opponent.
LOOK = 40
FIT_SURE = 0.3		# the pose model's confidence a joint is fitted from
FIT_HOLD = 10		# the hand this near the hilt (pixels) holds it
# How much each point counts.
FIT_WEIGHTS = {"neck": 1.0, "hilt": 3.0, "tip": 2.0,
               "front-shoulder": 1.0, "front-elbow": 1.0,
               "front-wrist": 1.0, "back-shoulder": 1.0,
               "back-elbow": 1.0, "back-wrist": 1.0, "front-hip": 1.0,
               "front-knee": 1.0, "front-ankle": 2.0, "front-toe": 0.5,
               "back-hip": 1.0, "back-knee": 1.0, "back-ankle": 2.0,
               "back-toe": 0.5, "front-hand": 2.0, "back-hand": 2.0}
# Each fitted angle's or place's first step (degrees, metres); and how
# much a step away from the last frame's pose costs, in squared pixels.
FIT_STEP = {"x": 0.03, "hips": 0.03, "twist": 4, "spine": 6, "neck": 6,
            "front-hip": 8, "front-knee": 8, "back-hip": 8,
            "back-knee": 8, "front-shoulder": 10, "front-spread": 10,
            "front-elbow": 10, "front-wrist": 10, "front-turn": 10,
            "blade": 10,
            "back-shoulder": 10, "back-spread": 10, "back-elbow": 10,
            "front-roll": 15, "back-roll": 15}
FIT_KEEP = 0.5
# How far behind his neck (metres) a hand or elbow may go, and what each
# square metre past it costs, in squared pixels.
FIT_BEHIND, FIT_DEPTH = 0.05, 2e4
# How far an arm's bone may point at or away from the camera (the sine
# of its angle out of the picture), and the cost of each unit past it
# squared, in squared pixels; the same for the back of an upper arm
# turning to the camera.  A bone the original draws short may point out
# as far as that shortening says, and this much (sine) further, but
# never past FIT_FLAT_MAX: a forearm end on hides its hand in the cuff.
FIT_FLAT, FIT_TILT, FIT_FLAT_SLACK, FIT_FLAT_MAX = 0.5, 2e4, 0.15, 0.8
# How far up (the sine) the sword hand's palm may face, at FIT_TILT's
# cost past it.
FIT_PALM = 0.3
# The joints' reach (degrees): knees bend only forward, elbows a little
# past straight at most; the chest never turned so far that he shows
# his back, the head bent at most a little back (the pose model's nose
# is the face's, not the head's middle: fitted, it tipped him to look up).
FIT_LIMITS = {"front-knee": (-150, 0), "back-knee": (-150, 0),
              "front-elbow": (-10, 150), "back-elbow": (-10, 150),
              "twist": (20, 80), "spine": (-5, 20), "neck": (-5, 30),
              "front-roll": (-120, 120), "back-roll": (-120, 120),
              "front-turn": (-90, 90)}
# The sword arm's roll while it holds the sword.
FIT_GRIP_ROLL = (-90, 90)
# The chest's twist, once each frame of a move is fitted, averaged with
# its neighbours' with these weights (the pose model's shoulders jitter),
# the frames fitted again with it held.
FIT_SMOOTH = (1, 2, 1)
# An arm's fitted joints.
FIT_ARM = ("shoulder", "spread", "elbow", "wrist", "roll", "turn")
# Fitted in turns: the body's place and lean, the legs, the arms and
# blade, then all together.
FIT_STAGES = [("x", "hips", "twist", "spine"),
              ("front-hip", "front-knee", "back-hip", "back-knee"),
              ("neck", "front-shoulder", "front-spread", "front-elbow",
               "front-wrist", "front-turn", "blade", "back-shoulder",
               "back-spread",
               "back-elbow", "front-roll", "back-roll"),
              tuple(FIT_STEP)]


def descend(p, keys, cost, limits=FIT_LIMITS, sweeps=40):
    """Pose "p" changed along "keys", one at a time, while "cost"
    falls, each step halved when none helps, within "limits"."""
    step = {k: FIT_STEP[k] for k in keys}
    best = cost(p)
    for _ in range(sweeps):
        moved = False
        for k in keys:
            for sign in (1, -1):
                q = dict(p)
                q[k] = p[k] + sign * step[k]
                low, high = limits.get(k, (-1e9, 1e9))
                if not low <= q[k] <= high:
                    continue
                e = cost(q)
                if e < best:
                    p, best, moved = q, e, True
                    break
        if not moved:
            step = {k: v / 2 for k, v in step.items()}
            if all(step[k] < FIT_STEP[k] / 16 for k in keys):
                break
    return p


def drawn_length(a, b):
    """An arm bone's full length (joints "a" to "b") as the original
    draws it, pixels: its length across the picture in most traced
    frames, either arm, shorter only where it points at the viewer."""
    key = (a, b)
    if key not in DRAWN:
        got = sorted(math.hypot(q[c + "-" + a][0] - q[c + "-" + b][0],
                                q[c + "-" + a][1] - q[c + "-" + b][1])
                     for t in TRACE.values() for q in [t["points"]]
                     for c in "rl" if c + "-" + a in q and c + "-" + b in q
                     and min(q[c + "-" + a][2], q[c + "-" + b][2]) >=
                     FIT_SURE)
        DRAWN[key] = got[int(len(got) * 0.9)] if got else 1e9
    return DRAWN[key]


def smoothed(f, move, frames, start):
    """The traced frames of "move" fitted (HumanFencer.fit()) from pose
    "start", by frame: twice, the second time with the chest's twist
    smoothed over the move (FIT_SMOOTH); crying out once the sword is
    let go (CRY)."""
    traced = [k for k in frames if "%d.%d" % (move, k) in TRACE]
    cry = {k: crying(k) if move == TALL_MOVE else {} for k in traced}
    first, last = {}, start
    for k in traced:
        last = first[k] = f.fit(TRACE["%d.%d" % (move, k)], last,
                                cry[k])
    out, last = {}, start
    for i, k in enumerate(traced):
        near = [(w, first[traced[j]]["twist"])
                for w, j in zip(FIT_SMOOTH, range(i - 1, i + 2))
                if 0 <= j < len(traced)]
        twist = sum(w * v for w, v in near) / sum(w for w, _ in near)
        last = out[k] = f.fit(TRACE["%d.%d" % (move, k)], last,
                              dict(cry[k], twist=twist))
        print("fit %d.%d" % (move, k), flush=True)
    return out


def at(base, **changes):
    p = dict(base)
    p.update(changes)
    return p


def line(base, level):
    """The guard's sword arm at the middle, low or high line."""
    return at(base, **{"front-shoulder": (72, 42, 112)[level],
                       "front-elbow": (18, 8, -6)[level],
                       "blade": (5, 10, -20)[level]})


def plant(p, origin=0.0):
    """Pose "p" with its leg places made angles, the game having moved
    the frame "origin" metres forward by now."""
    l1, l2 = legs()
    high = STANCE["height"] * (l1 + l2) - p.get("_drop", 0.0)
    q = {k: v for k, v in p.items() if not k.startswith("_")}
    hip = p.get("_hip", 0.0)
    for leg, lift in (("front", p.get("_lift", 0.0)), ("back", 0.0)):
        q[leg + "-hip"], q[leg + "-knee"] = reach(
            STANCE[leg] + p.get("_" + leg, 0.0) - hip, high - lift)
        q[leg + "-ankle"] = -(q[leg + "-hip"] + q[leg + "-knee"])
    q["back-ankle"] -= HEEL
    q["x"] = hip - origin
    return q


def bob(base, n, k):
    """The guard's breathing: frame k of n, from settled to risen, so
    that even two frames differ."""
    s = -math.cos(2 * math.pi * k / n) if n > 1 else 0.0
    return at(base, _drop=0.02 - 0.02 * s, spine=base["spine"] + 2 * s,
              **{"front-shoulder": base["front-shoulder"] + 5 * s,
                 "blade": base["blade"] - 6 * s,
                 "back-shoulder": base["back-shoulder"] - 6 * s,
                 "back-elbow": base["back-elbow"] + 8 * s})


def mix(a, b, t):
    keys = set(a) | set(b)
    return {k: a.get(k, 0.0) + (b.get(k, 0.0) - a.get(k, 0.0)) * t
            for k in keys}


def keyed(keys, n, k):
    """Frame k of n through key poses spread evenly."""
    if n == 1:
        return keys[0]
    t = k / (n - 1) * (len(keys) - 1)
    i = min(int(t), len(keys) - 2)
    return mix(keys[i], keys[i + 1], t - i)


def strike(lead, hit, back, n, k, s):
    """Frame k of an attack of n frames striking on frame s: through the
    "lead" key poses up to the blow, "hit" on it, then back to "back"."""
    if k < s:
        return keyed(lead, s, k) if s > 1 else lead[0]
    if k == s or n - 1 == s:
        return hit
    return mix(hit, back, (k - s) / (n - 1 - s))


# How far the game moves a stepping fencer on each frame of a move,
# forward, in the frame's pixels (--steps), how many frames of each
# bank it shows (--frames) and on which one each attack strikes
# (--strikes), all from the pack's data/sword table.  The frames carry
# the body between those moves and plant the feet, so he walks rather
# than slides; a bank's frames past the shown ones repeat the last.
STEPS = {}
FRAMES = []
STRIKES = {}


def legs():
    """The human's thigh and shin, in metres (human.py)."""
    import human
    return (math.dist(human.HIP, human.KNEE),
            math.dist(human.KNEE, human.ANKLE))


def ankle(hip, knee):
    """Where a leg at these angles puts its ankle: (ahead of the hip,
    below it)."""
    l1, l2 = legs()
    a, b = math.radians(hip), math.radians(hip + knee)
    return (l1 * math.sin(a) + l2 * math.sin(b),
            l1 * math.cos(a) + l2 * math.cos(b))


def reach(dx, dz):
    """The hip and knee angles putting the ankle "dx" ahead of the hip
    and "dz" below it, the knee bent forward."""
    l1, l2 = legs()
    d = min(math.hypot(dx, dz), 0.99 * (l1 + l2))
    toward = math.atan2(dx, dz)
    hip = toward + math.acos((l1 * l1 + d * d - l2 * l2) / (2 * l1 * d))
    shin = toward - math.acos((l2 * l2 + d * d - l1 * l1) / (2 * l2 * d))
    return math.degrees(hip), math.degrees(shin - hip)


def stride(g, move, n, k):
    """Frame k of move "move" stepping from guard "g" as far as STEPS
    moves it: the trailing foot closes up, then the other steps out, each
    planted while the other moves, so the stance is never wider than
    the guard's; the hips sink where it is too wide for the legs."""
    steps = [v * FRAME_M / 141 for v in STEPS.get(move, [0] * n)]
    whole = sum(steps)
    if whole == 0:
        return g
    s = (k + 1) / n			# the body's way through the step
    origin = sum(steps[:k + 1])		# where the game has moved the frame
    body = s * whole
    lead, follow = ("back", "front") if whole > 0 else ("front", "back")
    start = {leg: ankle(g[leg + "-hip"], g[leg + "-knee"])
             for leg in ("front", "back")}
    ground = max(dz for _, dz in start.values())  # the hips above it
    most = max(math.hypot(*v) for v in start.values())
    p = dict(g)
    feet, hips = {}, ground
    for leg, t in ((lead, min(1, 2 * s)), (follow, max(0, 2 * s - 1))):
        dx, dz = start[leg]
        x = dx + (3 - 2 * t) * t * t * whole - body
        z = ground - dz + 0.06 * math.sin(math.pi * t)	# above the ground
        feet[leg] = (x, z)
        if math.hypot(x, hips - z) > most:
            hips = z + math.sqrt(max(0, most * most - x * x))
    for leg, (x, z) in feet.items():
        p[leg + "-hip"], p[leg + "-knee"] = reach(x, hips - z)
    p["hips"] = g["hips"] - (ground - hips)
    p["x"] = body - origin
    return p


def origin(move, k):
    """How far the game has moved the frame by frame k of a move."""
    return sum(STEPS.get(move, [])[:k + 1]) * FRAME_M / 141


def move_pose(move, k):
    """Move "move" (SWM_*), frame k."""
    n = FRAMES[move] if FRAMES else BANKS[move]
    k = min(k, n - 1)
    level = move % 3
    g = line(GUARD, level if move < 12 else 0)
    if 3 <= move < 6 or move in (12, 13):	# step back, advance, retreat
        return stride(plant(g), move, n, k)
    net = origin(move, n - 1)
    if move < 3:				# guard
        p = bob(g, n, k)
    elif move < 9:				# slash: wind up, cut, recover
        high = (125, 110, 130)[level]
        low = (55, 25, 85)[level]
        fs = g["front-shoulder"]
        lift = at(g, _hip=-0.05, _drop=0.0, spine=0, **{
            "front-shoulder": (fs + high) / 2 - 40, "front-elbow": 100,
            "blade": (100, 135, 100)[level], "back-shoulder": -60,
            "back-elbow": 70})
        coil = at(g, _hip=-0.09, _front=-0.03, spine=-8, neck=-2, **{
            "front-shoulder": high, "front-elbow": 115, "blade": 40,
            "back-shoulder": -80, "back-elbow": 50})
        drive = at(g, _hip=0.07, _front=0.10, _lift=0.05, _drop=0.05,
                   spine=10, **{"front-shoulder": (high + low) / 2 + 10,
                                "front-elbow": -10, "blade": 0,
                                "back-shoulder": -70, "back-elbow": 30})
        cut = at(g, _hip=0.15, _front=0.22, _drop=0.12, spine=20, **{
            "front-shoulder": low, "front-elbow": 10, "blade": 20,
            "back-shoulder": -85, "back-elbow": 15})
        p = strike([g, lift, coil, drive], cut, g, n, k,
                   STRIKES.get(move, n * 2 // 3))
    elif move < 12:				# lunge: coil, spring, recover
        arm = (88, 62, 112)[level]
        coil = at(g, _hip=-0.05, _drop=0.03, spine=2, **{
            "front-shoulder": min(g["front-shoulder"] - 25, 70),
            "front-elbow": 20, "blade": g["blade"] + 60,
            "back-shoulder": -55})
        spring = at(g, _hip=0.12, _front=0.26, _lift=0.07, _drop=0.06,
                    spine=10, **{"front-shoulder": arm - 5,
                                 "front-elbow": 15,
                                 "blade": (0, 10, -12)[level],
                                 "back-shoulder": -70, "back-elbow": 30})
        out = at(g, _hip=0.26, _front=0.52, _drop=0.17, spine=16, **{
            "front-shoulder": arm, "front-elbow": 0,
            "blade": (0, 10, -12)[level], "back-shoulder": -80,
            "back-elbow": 5})
        back = at(g, _hip=net, _front=net, _back=net)
        p = strike([coil, spring], out, back, n, k,
                   STRIKES.get(move, n // 2))
    elif move == 14:			# the fall: struck, then yielding
        hit = at(g, _hip=-0.08, _drop=0.04, spine=-22, neck=-15, **{
            "front-shoulder": 20, "front-elbow": 10, "blade": 40})
        # Reeling back from the blow, the front foot drawn in, the sword
        # sagging and the free arm thrown out for balance.
        reel = at(hit, _hip=-0.15, _front=-0.12, _lift=0.04, _drop=0.06,
                  spine=-28, neck=-18, **{
                      "front-shoulder": 5, "front-elbow": 30, "blade": 65,
                      "back-shoulder": -75, "back-elbow": 45})
        # The sword flung from the opening hand.
        fling = at(reel, _front=-0.08, _lift=0.0, _drop=0.02, spine=-10,
                   neck=-6, grip=40, **{
                       "front-shoulder": 120, "front-elbow": 35,
                       "blade": 90, "back-shoulder": 30, "back-elbow": 70,
                       "back-spread": 25})
        # Yielding, the open hands up and stepping back from his sword:
        # the near hand raised high toward the opponent (spread, it
        # would cross the face) and waving, the far one at the chest,
        # then both up and out; the body bobbing, the head turning.
        def yielding(fs, fe, fw, bs, be, bsp, bw, hip, drop, spine, neck):
            return at(fling, _hip=hip, _front=-0.10 + hip, _lift=0.0,
                      _drop=drop, spine=spine, neck=neck, grip=10, **{
                          "front-shoulder": fs, "front-elbow": fe,
                          "front-wrist": fw, "back-shoulder": bs,
                          "back-elbow": be, "back-spread": bsp,
                          "back-wrist": bw})
        p = keyed([g, hit, reel, fling,
                   yielding(150, 25, 20, 35, 115, 0, 10,
                            -0.06, -0.01, 4, -4),
                   yielding(125, 60, -35, 60, 100, 15, 30,
                            -0.10, -0.06, -4, 6),
                   yielding(155, 10, 30, 30, 120, 0, 0,
                            -0.12, -0.02, 6, -6),
                   yielding(110, 80, -20, 125, 55, 45, 20,
                            -0.14, -0.08, 0, 4),
                   yielding(95, 100, 25, 105, 85, 55, -25,
                            -0.15, -0.03, 3, -2)], n, k)
        if k > LET_GO:
            p["let_go"] = (k - LET_GO) / FALLING
            p.update(crying(k))
    else:
        p = bob(g, n, k)			# the last: back on guard
    return plant(p, origin(move, k))


def scene(fw, fh, margin=0):
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    # No grain on cloth or skin: denoised where this Blender has a
    # denoiser (the render host's build has none), else enough light
    # paths, adaptively, the bright stray ones clamped.
    denoisers = [i.identifier for i in sc.cycles.bl_rna.properties[
        "denoiser"].enum_items]
    if denoisers:
        sc.cycles.samples = 96
        sc.cycles.use_denoising = True
        sc.cycles.denoiser = denoisers[0]
    else:
        sc.cycles.use_denoising = False
        sc.cycles.samples = 256
        sc.cycles.use_adaptive_sampling = True
        sc.cycles.adaptive_threshold = 0.02
        sc.cycles.sample_clamp_indirect = 3.0
    sc.cycles.device = "CPU"
    sc.render.film_transparent = True
    sc.render.resolution_x = fw
    sc.render.resolution_y = fh
    sc.render.image_settings.file_format = "PNG"
    sc.render.image_settings.color_mode = "RGBA"
    sc.view_settings.view_transform = "Standard"
    materials.sky(sc, 1.3)
    sun = bpy.data.lights.new("sun", "SUN")
    sun.energy = 3.0
    sob = bpy.data.objects.new("sun", sun)
    sob.rotation_euler = (math.radians(55), math.radians(-30),
                          math.radians(-25))
    bpy.context.collection.objects.link(sob)
    cam = bpy.data.cameras.new("cam")
    cam.type = "ORTHO"
    # The game's frame and "margin" of its pixels each side, for the
    # feet a wide stance puts out of it.
    cam.ortho_scale = FRAME_M * (141 + 2 * margin) / 141
    cob = bpy.data.objects.new("cam", cam)
    # From the front of the fight's line (-Y), level; the ground near
    # the frame's bottom, the fencer a little right of its middle.
    high = cam.ortho_scale * fh / fw
    cob.location = (CAMERA_X[SIDE], -20, high / 2 - 0.04)
    cob["fw"], cob["fh"] = fw, fh
    cob.rotation_euler = (math.pi / 2, 0, 0)
    bpy.context.collection.objects.link(cob)
    sc.camera = cob


def frame_size(move, k):
    """A frame taller for the surrender, the camera raised with it so the
    ground stays at the frame's bottom."""
    sc = bpy.context.scene
    cob = sc.camera
    fw, fh = cob["fw"], cob["fh"]
    if move == TALL_MOVE and k >= TALL_FROM:
        th = round(fh * TALL)
    else:
        th = fh
    sc.render.resolution_y = th
    wide = cob.data.ortho_scale
    high = wide * fh / fw
    cob.location.z = high / 2 - 0.04 + wide * (th - fh) / fw / 2


def show(objects, holdouts):
    """Render only "objects", "holdouts" hiding what they cover."""
    shown = set(objects) | set(holdouts)
    for ob in bpy.data.objects:
        if ob.type not in ("MESH", "CURVES"):
            continue
        ob.hide_render = ob not in shown
        ob.is_holdout = ob in holdouts


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    ap = argparse.ArgumentParser()
    ap.add_argument("--side", choices=("my", "he"), default="my")
    ap.add_argument("--out", required=True)
    ap.add_argument("--frame", default="317x203",
                    help="pixels of a frame (141x90 at 1080 lines)")
    ap.add_argument("--margin", type=int, default=0,
                    help="game pixels rendered beyond each side of the "
                    "frame (the frame then wider)")
    ap.add_argument("--steps", help="JSON: each stepping move's forward "
                    "move on each frame, in the frame's pixels")
    ap.add_argument("--frames", help="JSON: the frames the game shows of "
                    "each move")
    ap.add_argument("--strikes", help="JSON: the frame each attack "
                    "strikes on")
    ap.add_argument("--trace", help="JSON: the original's traced frames "
                    "(trace.py), the figure posed to match them")
    ap.add_argument("--moves", help="only these moves, e.g. 0,9")
    ap.add_argument("--layers", help="only these layers, e.g. body")
    ap.add_argument("--blend")
    ap.add_argument("--sample", action="store_true",
                    help="only each move's middle frame, to look at")
    ap.add_argument("--mannequin", action="store_true",
                    help="the old jointed figure, not the real body")
    args = ap.parse_args(argv)
    fw, fh = (int(v) for v in args.frame.split("x"))
    global SIDE
    SIDE = args.side
    if args.steps:
        STEPS.update({int(m): v for m, v in json.loads(args.steps).items()})
    if args.frames:
        FRAMES.extend(json.loads(args.frames))
    if args.strikes:
        STRIKES.update({int(m): v for m, v in
                        json.loads(args.strikes).items()})
    if args.trace:
        with open(args.trace) as fp:
            TRACE.update(json.load(fp))

    bpy.ops.wm.read_factory_settings(use_empty=True)
    f = (Fencer if args.mannequin else HumanFencer)(args.side)
    if args.side == "my":
        f.root.scale = (-1, 1, 1)	# facing his opponent, to the left
    scene(fw, fh, args.margin)
    f.pose(plant(GUARD))
    if args.blend:
        bpy.ops.wm.save_as_mainfile(filepath=os.path.abspath(args.blend),
                                    compress=True)
    body = f.layers["body"]
    heads = [ob for parts in f.layers["heads"].values() for ob in parts]
    passes = [("body", body, [])]
    # The opponent with each of his heads on, the picture redrawn: the
    # body is cut from the first (a body alone the redraw gives a
    # fencing mask), each head from its own, so head and body match.
    for name, parts in f.layers["heads"].items():
        passes.append(("bodyref" + name[4:], body + parts, []))
    for name, parts in f.layers["heads"].items():
        passes.append((name, parts, body))
    for name, parts in f.layers["weapon"].items():
        passes.append((name, parts, body + heads))
    for name, parts in f.layers["wounds"].items():
        passes.append((name, parts, []))
    if args.layers:
        passes = [q for q in passes if q[0] in args.layers.split(",")]
    moves = range(len(BANKS)) if not args.moves else \
        [int(v) for v in args.moves.split(",")]
    last = plant(GUARD)
    for move in moves:
        frames = [BANKS[move] // 2] if args.sample else range(BANKS[move])
        fitted = smoothed(f, move, frames, last)
        for k in frames:
            if k in fitted:
                last = fitted[k]
                f.pose(last)
            else:
                f.pose(move_pose(move, k))
            frame_size(move, k)
            for name, objects, holdouts in passes:
                show(objects, holdouts)
                d = os.path.join(args.out, name)
                os.makedirs(d, exist_ok=True)
                bpy.context.scene.render.filepath = os.path.join(
                    d, "%d.%d.png" % (move, k))
                bpy.ops.render.render(write_still=True)


if __name__ == "__main__":	# fencers/walker.py imports the figure
    main()
