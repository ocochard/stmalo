#!/usr/bin/env python3
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026, Olivier Cochard-Labbé
#
"""A clothed, rigged human for the pack's figures.

The body is Blender's realistic male base mesh (the Human Base Meshes
bundle, CC0, fetched on the render host by materials.fetch_command()).
It is dressed by regions of its own surface (shirt, sash, breeches,
boots, the skin bare) in materials.py's photographic materials, the
clothes a little loose; a kerchief and hair are added on the head.  An
armature is fitted to it and bound by bone heat, then its arms and legs
are straightened to hang as the fencers' figure's do (fencer.py: limbs
hanging at 0), so the same pose angles drive it: each joint turns about
the figure's side axis, positive towards its front.

The figure faces +X, its right ("front", nearer a camera at -Y) at -Y,
as fencer.py's.  Blender only.
"""

import math
import os

import bpy
from mathutils import Matrix, Quaternion, Vector

import materials

BUNDLE = os.path.join(materials.LIB, "human", "human-base-meshes-bundle-v1.4.1",
                      "human_base_meshes_bundle.blend")
BODY = "GEO-body_male_realistic"

# The joints, in the mesh's A-pose (metres; the mesh centred, facing -Y,
# its right at -X), measured on it.
SHOULDER = (0.17, 0.0, 1.40)
ARM_TIP = (0.44, -0.095, 0.763)
HIP = (0.09, 0.0, 0.89)
KNEE = (0.12, -0.01, 0.48)
ANKLE = (0.15, 0.02, 0.08)
TOE = (0.17, -0.13, 0.02)
ROOT, CHEST, NECK, CROWN = 0.95, 1.40, 1.52, 1.70
UPPER, FORE, HAND = 0.30, 0.26, 0.12
SMOOTH_CHEST = 10	# passes softening the chest under the shirt
SMOOTH_LEGS = 12	# and the thighs under full breeches
# The boots, their own meshes over the body's legs (boots()): the shaft
# this far (metres) off the calf, its top flaring this much wider from
# CUFF up; the foot a round-toed shoe this far off the foot, on a sole.
BOOT = {"shaft": 0.012, "cuff": 0.022, "foot": 0.012, "sole": 0.015}
CUFF = 0.36
FOLDS = 0.35		# full breeches' folds, a share of their fullness
# Full breeches puff out most at this height (metres), gathered into the
# boots below and at the waist above: (bottom, fullest, top).
PUFF = (0.42, 0.54, 0.94)
# The jaw, dropped to open the mouth (pose()'s p["mouth"], 0 to 1, up to
# JAW degrees): a bone turning about the hinge (HINGE: metres behind the
# face's front, height) and carrying the face below where the lips meet
# (LIPS, a height), the cheeks less and less up to them, not the neck.
JAW, HINGE, LIPS = 12, (0.13, 1.53), 1.502
OPEN = 5		# the fingers' curl in an open hand (degrees)


def _folds(me, push, index):
    """Full breeches hang in folds: their cloth standing off further and
    nearer by turns round each leg, the folds running down it and
    twisting a little."""
    on = set()
    for f in me.polygons:
        if f.material_index == index:
            on.update(f.vertices)
    for i in on:
        c = me.vertices[i].co
        a = math.atan2(c.y, abs(c.x) - 0.1)	# round the leg's middle
        push[i] *= 1 + FOLDS * math.sin(7 * a + 9 * c.z)


def _puff(z):
    """How full breeches are at height z, 1 at their fullest."""
    lo, mid, hi = PUFF
    if z <= mid:
        t = max(0.0, (z - lo) / (mid - lo))
        return t * t * (3 - 2 * t)
    t = min(1.0, (z - mid) / (hi - mid))
    return 1 - 0.7 * t * t * (3 - 2 * t)

# Each joint of the pose (fencer.py's names) and its bone.
BONES = {"spine": "spine", "neck": "neck",
         "front-shoulder": "upper.R", "front-elbow": "fore.R",
         "front-wrist": "hand.R", "back-shoulder": "upper.L",
         "back-elbow": "fore.L", "back-wrist": "hand.L",
         "front-hip": "thigh.R", "front-knee": "shin.R",
         "front-ankle": "foot.R", "back-hip": "thigh.L",
         "back-knee": "shin.L", "back-ankle": "foot.L"}


def _ramp(v, a, b):
    """0 below a, 1 above b, smoothly between."""
    t = min(1.0, max(0.0, (v - a) / (b - a)))
    return t * t * (3 - 2 * t)


def _face_front(me):
    """The face's front (its nose tip's y), at rest."""
    return min(v.co.y for v in me.vertices
               if 1.445 < v.co.z < 1.553 and abs(v.co.x) < 0.075)


def _jaw(c, front):
    """How much of the jaw's turn the rest point "c" follows, 0 to 1."""
    if c.z > LIPS:
        return 0.0
    x = abs(c.x)
    cheek = _ramp(x, 0.022, 0.035) * _ramp(c.z, LIPS - 0.03, LIPS)
    return (1 - cheek) * _ramp(c.z, 1.41, 1.45) * \
        (1 - _ramp(c.y, front + 0.07, front + 0.11)) * \
        (1 - _ramp(x, 0.06, 0.075))


def jaw(ob, rob):
    """The jaw bone, under the head, and the lower face weighted to it
    (its share taken from the bones it had); the face on the head."""
    me = ob.data
    front = _face_front(me)
    bpy.context.view_layer.objects.active = rob
    bpy.ops.object.mode_set(mode="EDIT")
    eb = rob.data.edit_bones
    b = eb.new("jaw")
    b.head = Vector((0, front + HINGE[0], HINGE[1]))
    b.tail = Vector((0, front + 0.03, LIPS - 0.04))
    b.parent = eb["head"]
    bpy.ops.object.mode_set(mode="OBJECT")
    # The face, to under the chin, carried by the head alone: shared
    # with the neck, it smeared when the head turned far.
    neck, head = ob.vertex_groups["neck"].index, ob.vertex_groups["head"]
    for v in me.vertices:
        f = _ramp(v.co.z, LIPS - 0.1, LIPS - 0.06) * \
            (1 - _ramp(v.co.y, front + 0.12, front + 0.17))
        n = [e for e in v.groups if e.group == neck]
        if f <= 0 or not n:
            continue
        head.add([v.index], n[0].weight * f, "ADD")
        n[0].weight *= 1 - f
    g = ob.vertex_groups.new(name="jaw")
    for v in me.vertices:
        w = _jaw(v.co, front)
        if w <= 0:
            continue
        for e in v.groups:
            e.weight *= 1 - w
        g.add([v.index], w, "REPLACE")


def _side(p, s):
    """A left-side point for side s (+1 left, -1 right)."""
    return Vector((s * p[0], p[1], p[2]))


def _load():
    with bpy.data.libraries.load(BUNDLE, link=False) as (src, dst):
        dst.objects = [BODY]
    ob = dst.objects[0]
    bpy.context.collection.objects.link(ob)
    for m in list(ob.modifiers):
        ob.modifiers.remove(m)
    for child in list(bpy.data.objects):
        if child.parent == ob:
            bpy.data.objects.remove(child, do_unlink=True)
    # Centred on the origin, its feet on the ground.
    me = ob.data
    mw = ob.matrix_world.copy()
    me.transform(mw)
    ob.matrix_world = Matrix.Identity(4)
    xs = [v.co.x for v in me.vertices]
    ys = [v.co.y for v in me.vertices]
    cx, cy = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
    me.transform(Matrix.Translation((-cx, -cy, 0)))
    return ob


# The sash between these heights (metres): a broad one, seen above the
# shirt's hem blousing over it.
SASH = (0.9, 1.02)


def _region(c, n, kerchief=True):
    """The part of the clothes a face at "c" (its normal "n") is in."""
    x, y, z = abs(c.x), c.y, c.z
    if kerchief and (z > 1.6 or (z > 1.53 and y > 0.0)):
        return "kerchief"			# the crown and the back
    if (z > 1.53 and y > 0.02) or (not kerchief and z > 1.62):
        return "hair"
    if z > 1.47:
        return "skin"				# the face and neck
    if x > 0.33 and z < 0.92:
        return "skin"				# the hands
    if x > 0.24 and z > 0.75:
        return "shirt"				# the arms, sleeved
    if z > SASH[1]:
        return "shirt"
    if z > SASH[0]:
        return "sash"
    if z > 0.42:
        return "breeches"
    return "boots"


# How far each part of the clothes stands off the body (metres).
LOOSE = {"shirt": 0.018, "sash": 0.022, "breeches": 0.012, "boots": 0.008,
         "skin": 0.0, "hair": 0.008, "kerchief": 0.014}
# Between these heights (metres) a shirt cut fuller than LOOSE's comes
# back to it, so that it still closes round the neck.
COLLAR = (1.12, 1.32)


def dress(ob, look, kerchief=(0.6, 0.05, 0.04), loose=None,
          neckline=None):
    """The clothes, by regions of the body, a little loose (or as far
    off as "loose" says, by part), the shirt's collar at height
    "neckline" if given; the kerchief over the crown, the hair below
    it."""
    me = ob.data
    names = ["skin", "shirt", "sash", "breeches", "boots", "hair",
             "kerchief"]
    loose = dict(LOOSE, **(loose or {}))
    colour = dict(look, kerchief=kerchief or look["hair"])
    fabric = {"kerchief": "wool"}
    from fencer import FABRIC
    for name in names:
        me.materials.append(materials.skin("human-skin", colour[name])
                            if name == "skin" else materials.pbr(
            "human-" + name, fabric.get(name, FABRIC.get(name)),
            colour[name], rough=0.8))
    push = [0.0] * len(me.vertices)
    for f in me.polygons:
        r = _region(f.center, f.normal, kerchief is not None)
        if r == "shirt" and neckline and f.center.z > neckline and \
                abs(f.center.x) < 0.1:
            r = "skin"
        f.material_index = names.index(r)
        off = loose[r]
        if r == "shirt" and f.center.z > COLLAR[0]:
            t = min(1.0, (f.center.z - COLLAR[0]) / (COLLAR[1] - COLLAR[0]))
            off += (min(off, LOOSE["shirt"]) - off) * t
        if r == "breeches" and off > LOOSE["breeches"]:
            off = LOOSE["breeches"] + (off - LOOSE["breeches"]) * \
                _puff(f.center.z)
        for v in f.vertices:
            push[v] = max(push[v], off)
    # The shirt hangs loose over the torso, not along its muscles: the
    # chest and belly smoothed flat (the body's sculpted pectorals would
    # read as a woman's), then the cloth pushed off it.
    import bmesh
    bm = bmesh.new()
    bm.from_mesh(me)
    bm.verts.ensure_lookup_table()
    torso = [v for v in bm.verts if abs(v.co.x) < 0.2 and
             1.02 < v.co.z < 1.44 and push[v.index] > 0]
    for _ in range(SMOOTH_CHEST):
        bmesh.ops.smooth_vert(bm, verts=torso, factor=0.4,
                              use_axis_x=True, use_axis_y=True)
    # Full breeches hide the legs' muscles too: the thighs smoothed round
    # before the cloth puffs off them.
    if loose["breeches"] > LOOSE["breeches"]:
        legs = [v for v in bm.verts if PUFF[0] < v.co.z < PUFF[2] and
                abs(v.co.x) < 0.25]
        for _ in range(SMOOTH_LEGS):
            bmesh.ops.smooth_vert(bm, verts=legs, factor=0.5,
                                  use_axis_x=True, use_axis_y=True)
    bm.to_mesh(me)
    bm.free()
    me.update()
    # The cloth's thickness eased across each edge, so no step shows
    # where one piece meets the next (the collar, the hems).
    near = [[] for _ in me.vertices]
    for e in me.edges:
        a_, b_ = e.vertices
        near[a_].append(b_)
        near[b_].append(a_)
    for _ in range(6):
        push = [(p + sum(push[n] for n in near[i])) / (1 + len(near[i]))
                for i, p in enumerate(push)]
    if loose["breeches"] > LOOSE["breeches"]:
        _folds(me, push, names.index("breeches"))
    for v in me.vertices:
        v.co += v.normal * push[v.index]
    for f in me.polygons:
        f.use_smooth = True
    # The rest position kept on every point, for the clothes' edges and
    # the hair to follow it as the body moves.
    rest = me.attributes.new("rest", "FLOAT_VECTOR", "POINT")
    for v in me.vertices:
        rest.data[v.index].vector = v.co
    smooth_edges(me, names, neckline or 1.47)
    sub = ob.modifiers.new("smooth", "SUBSURF")
    sub.levels = sub.render_levels = 1


def _rest_z(nt):
    """Nodes giving the rest position's x, y, z (rest attribute)."""
    at = nt.nodes.new("ShaderNodeAttribute")
    at.attribute_name = "rest"
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(at.outputs["Vector"], sep.inputs["Vector"])
    return sep


def smooth_edges(me, names, collar=1.47):
    """Each piece of clothing's edge drawn in its material, not along the
    mesh's faces: a material shows the next one's fabric past its edge
    (a straight hem, softened, a little ragged), so the faces' steps do
    not show.  Done for the shirt's hem over the sash, the sash's over
    the breeches, the boots' tops and the collar (at height "collar")."""
    def mixin(own, other, axis, edge, below):
        """In material "own", "other" shows where rest[axis] is past
        "edge" (below it when "below")."""
        m = me.materials[names.index(own)]
        o = me.materials[names.index(other)]
        nt = m.node_tree
        out = nt.nodes["Material Output"]
        mine = out.inputs["Surface"].links[0].from_socket
        # The other's shader, copied in.
        theirs = o.node_tree.nodes["Principled BSDF"]
        group = nt.nodes.new("ShaderNodeBsdfPrincipled")
        for i in theirs.inputs:
            if i.name in group.inputs and not i.is_linked and \
                    hasattr(i, "default_value"):
                try:
                    group.inputs[i.name].default_value = i.default_value
                except (TypeError, AttributeError):
                    pass
        # Its colour by its own picture: copy its colour chain's end.
        base = theirs.inputs["Base Color"]
        if base.is_linked:
            # The other's colour, as a flat tint (its texture is close).
            group.inputs["Base Color"].default_value = \
                tuple(base.links[0].from_node.inputs["A"].default_value) \
                if "A" in base.links[0].from_node.inputs else \
                (0.5, 0.5, 0.5, 1)
        sep = _rest_z(nt)
        noise = nt.nodes.new("ShaderNodeTexNoise")
        noise.inputs["Scale"].default_value = 40.0
        at = nt.nodes.new("ShaderNodeAttribute")
        at.attribute_name = "rest"
        nt.links.new(at.outputs["Vector"], noise.inputs["Vector"])
        wob = nt.nodes.new("ShaderNodeMath")
        wob.operation = "MULTIPLY_ADD"
        wob.inputs[1].default_value = 0.006
        wob.inputs[2].default_value = edge - 0.003
        nt.links.new(noise.outputs["Fac"], wob.inputs[0])
        st = nt.nodes.new("ShaderNodeMapRange")
        st.interpolation_type = "SMOOTHSTEP"
        nt.links.new(sep.outputs["XYZ"[axis]], st.inputs["Value"])
        lo = nt.nodes.new("ShaderNodeMath")
        lo.operation = "SUBTRACT"
        lo.inputs[1].default_value = 0.002
        hi = nt.nodes.new("ShaderNodeMath")
        hi.operation = "ADD"
        hi.inputs[1].default_value = 0.002
        nt.links.new(wob.outputs[0], lo.inputs[0])
        nt.links.new(wob.outputs[0], hi.inputs[0])
        nt.links.new(lo.outputs[0], st.inputs["From Min"])
        nt.links.new(hi.outputs[0], st.inputs["From Max"])
        if below:
            st.inputs["To Min"].default_value = 1.0
            st.inputs["To Max"].default_value = 0.0
        # Only on the trunk and legs: the hands and sleeves, out at the
        # A-pose's sides, keep their own.
        ax = nt.nodes.new("ShaderNodeMath")
        ax.operation = "ABSOLUTE"
        nt.links.new(sep.outputs["X"], ax.inputs[0])
        inner = nt.nodes.new("ShaderNodeMath")
        inner.operation = "LESS_THAN"
        inner.inputs[1].default_value = 0.2
        nt.links.new(ax.outputs[0], inner.inputs[0])
        both = nt.nodes.new("ShaderNodeMath")
        both.operation = "MULTIPLY"
        nt.links.new(st.outputs["Result"], both.inputs[0])
        nt.links.new(inner.outputs[0], both.inputs[1])
        mix = nt.nodes.new("ShaderNodeMixShader")
        nt.links.new(both.outputs[0], mix.inputs["Fac"])
        nt.links.new(mine, mix.inputs[1])
        nt.links.new(group.outputs["BSDF"], mix.inputs[2])
        nt.links.new(mix.outputs["Shader"], out.inputs["Surface"])

    mixin("shirt", "sash", 2, SASH[1], True)	# the shirt's hem
    mixin("sash", "shirt", 2, SASH[1], False)
    mixin("sash", "breeches", 2, SASH[0], True)
    mixin("breeches", "sash", 2, SASH[0], False)
    mixin("breeches", "boots", 2, 0.42, True)
    mixin("boots", "breeches", 2, 0.42, False)
    mixin("shirt", "skin", 2, collar, False)	# the collar
    mixin("skin", "shirt", 2, collar, True)


# The hair, combed over the skull and falling in wavy locks: by style,
# (each strand's length, the lowest its end hangs, metres).  "long" to
# the shoulders all round, parted on the crown; "under" only from below
# a headwear's rim, at the back and sides.
HAIR = {"long": (0.30, 1.40), "under": (0.22, 1.43)}
LOCKS, PER_LOCK = 900, 9	# the hair's locks and strands in each
CLUMP = 0.3		# how far a lock's strands gather at their ends
BEARD = 5000		# a dark-haired man's full beard's strands


def strand_material(name, dark, redness=0.35):
    m = bpy.data.materials.new("strands-" + name)
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.remove(nt.nodes["Principled BSDF"])
    h = nt.nodes.new("ShaderNodeBsdfHairPrincipled")
    h.parametrization = "MELANIN"
    h.inputs["Melanin"].default_value = 0.97 if dark else 0.7
    h.inputs["Melanin Redness"].default_value = redness
    h.inputs["Roughness"].default_value = 0.4
    nt.links.new(h.outputs[0], nt.nodes["Material Output"].inputs[0])
    return m


def _skull(me):
    """The skull as an ellipsoid (centre, semi-axes), at rest, from the
    head's width and back: the nose and chin would stretch a box."""
    pts = [v.co for v in me.vertices if v.co.z > 1.56]
    a = (max(p.x for p in pts) - min(p.x for p in pts)) / 2
    b, c = a * 1.22, a * 1.12
    centre = Vector(((max(p.x for p in pts) + min(p.x for p in pts)) / 2,
                     max(p.y for p in pts) - b, max(p.z for p in pts) - c))
    return centre, Vector((a, b, c))


def _comb(skull, root, length, end_z, lift, rng, wave=None):
    """A strand from the skull's surface in direction "root", "lift"
    off it: over the skull it is combed back and down, parted on the
    crown; past the skull's widest it hangs, flaring a little, in waves,
    until "length" or "end_z"."""
    c, ax = skull

    def onto(d):
        d = d.normalized()
        k = 1 / math.sqrt(sum((d[i] / ax[i]) ** 2 for i in range(3)))
        return c + d * (k + lift)

    p = onto(root)
    pts, walked, step = [p], 0.0, 0.008
    side = 1 if root.x >= 0 else -1
    # Loose waves, each lock its own (wavelength, phase, depth).
    wl, phase, depth = wave or (0.07, rng.random() * 2 * math.pi, 0.004)
    hang = None
    while walked < length and p.z > end_z:
        d = p - c
        n = Vector((d.x / ax.x ** 2, d.y / ax.y ** 2,
                    d.z / ax.z ** 2)).normalized()
        if hang is None and p.z > c.z - 0.01:
            # Combed back off the brow (the face at -Y), then down.
            g = Vector((0.7 * side * max(0.0, n.z),
                        0.3 + 1.6 * max(0.0, -n.y), -1.0))
            t = g - n * g.dot(n)
            if t.length < 1e-4:
                t = Vector((side, 0, 0))
            q = onto(p + t.normalized() * step - c)
        else:
            if hang is None:
                hang = walked
            out = Vector((d.x, d.y, 0))
            out = out.normalized() if out.length > 1e-6 else out
            s = walked - hang
            q = p + (Vector((0, 0, -1)) + out * 0.12).normalized() * step
            q += out * depth * math.cos(s / wl * 2 * math.pi + phase) * \
                min(1.0, s / 0.05)
        walked += (q - p).length
        p = q
        pts.append(p)
    return pts


def _curves(name, strands, r0, r1, mat):
    cv = bpy.data.hair_curves.new(name)
    strands = [s for s in strands if len(s) >= 2]
    cv.add_curves([len(s) for s in strands])
    cv.attributes["position"].data.foreach_set(
        "vector", [x for s in strands for p in s for x in p])
    r = cv.attributes.get("radius") or \
        cv.attributes.new("radius", "FLOAT", "POINT")
    r.data.foreach_set("value", [
        r0 + (r1 - r0) * i / (len(s) - 1)
        for s in strands for i in range(len(s))])
    cv.materials.append(mat)
    ob = bpy.data.objects.new(name, cv)
    bpy.context.collection.objects.link(ob)
    return ob


def hair(ob, look, style):
    """Hair combed in locks of strands over the skull (style, HAIR) and,
    for a dark-haired man, a full beard: Blender hair curves, their
    colour by melanin, each a fresh object (strands(rig) finds them)."""
    import random
    rng = random.Random(1660)
    me = ob.data
    dark = sum(look["hair"]) < 0.2
    skull = _skull(me)
    length, end_z = HAIR[style]
    out = []
    locks = []
    while len(locks) < LOCKS:
        d = Vector((rng.gauss(0, 1), rng.gauss(0, 1), rng.gauss(0, 1)))
        d.normalize()
        if style == "under":
            ok = -0.35 < d.z < 0.25 and d.y > -0.05
        else:
            # Not the brow and face, nor in front of the ears.
            ok = (d.z > 0.05 and not (d.y < -0.3 and d.z < 0.5)) or \
                (-0.4 < d.z <= 0.05 and d.y > 0.1)
        if ok:
            locks.append(d)
    strands = []
    for d in locks:
        lift = 0.004 + rng.random() * 0.012
        wave = (0.05 + 0.05 * rng.random(), rng.random() * 2 * math.pi,
                0.002 + 0.005 * rng.random())
        reach = length * (0.75 + 0.35 * rng.random())
        guide = _comb(skull, d, reach, end_z, lift, rng, wave)
        for _ in range(PER_LOCK):
            j = d + Vector([rng.gauss(0, 0.07) for _ in range(3)])
            w = (wave[0] * (0.9 + 0.2 * rng.random()),
                 wave[1] + rng.gauss(0, 0.5), wave[2] * rng.random())
            ch = _comb(skull, j, reach * (0.85 + 0.2 * rng.random()), end_z,
                       lift + rng.gauss(0, 0.003), rng, w)
            n = min(len(ch), len(guide))
            # Gathered a little toward the lock's guide at their ends.
            strands.append([ch[i].lerp(guide[i], CLUMP * i / max(1, n - 1))
                            for i in range(n)])
    out.append(_curves("strands-hair", strands, 0.0005, 0.0002,
                       strand_material("hair", dark)))
    if dark:
        c = skull[0]
        face = [v for v in me.vertices if 1.445 < v.co.z < 1.553 and
                v.co.y < c.y - 0.01 and abs(v.co.x) < 0.075]
        front = min(v.co.y for v in face)
        face = [v for v in face if not (
            LIPS - 0.012 < v.co.z < 1.523 and abs(v.co.x) < 0.024 and
            v.co.y < front + 0.014)]		# the lips bare
        strands, chin = [], []
        jf = _face_front(me)
        for _ in range(BEARD):
            v = face[rng.randrange(len(face))]
            p = v.co + Vector([rng.gauss(0, 0.003) for _ in range(3)])
            n = v.normal
            # Short and lying close on the cheeks, longer at the chin,
            # combed down.
            ln = (0.012 + 0.022 * min(1, max(0, (1.52 - p.z) / 0.05))) * \
                (0.7 + 0.5 * rng.random())
            if 1.523 < p.z < 1.545 and abs(p.x) < 0.03:
                ln *= 0.4	# the moustache clear of the open mouth
            pts = [p + n * 0.001]
            for i in range(5):
                t = (i + 1) / 5
                d = (n * 0.35 * (1 - t) + Vector((0, -0.1, -1))).normalized()
                pts.append(pts[-1] + d * ln / 5)
            # Below the lips, carried by the jaw.
            (chin if _jaw(v.co, jf) > 0.5 else
             strands).append(pts)
        beard = strand_material("beard", True, 0.1)
        out.append(_curves("strands-beard", strands, 0.0003, 0.0001,
                           beard))
        out.append(_curves("strands-beard-jaw", chin, 0.0003, 0.0001,
                           beard))
    return out


def strands(rob):
    """The hair and beard of the figure on rig "rob"."""
    return [o for o in rob.children if o.type == "CURVES"]


def _mesh(name, verts, faces, mat):
    me = bpy.data.meshes.new(name)
    me.from_pydata([tuple(v) for v in verts], [], faces)
    for f in me.polygons:
        f.use_smooth = True
    me.materials.append(mat)
    ob = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(ob)
    sub = ob.modifiers.new("smooth", "SUBSURF")
    sub.levels = sub.render_levels = 1
    return ob


def boots(ob, rob):
    """Riding boots over the body's legs, their own meshes: a shaft round
    each calf (an ellipse a little off it at each height, the calf's
    muscles not followed), its top flaring into a cuff under the
    breeches, carried by the shin; a round-toed foot on a flat sole,
    carried by the foot.  Returns {bone: object}."""
    me = ob.data
    mat = next(m for m in me.materials if m.name.startswith("human-boots"))
    index = list(me.materials).index(mat)
    on = set()
    for f in me.polygons:
        if f.material_index == index:
            on.update(f.vertices)
    out = {}
    seg = 24
    for side, s in (("L", 1), ("R", -1)):
        pts = [me.vertices[i].co for i in on
               if me.vertices[i].co.x * s > 0]
        ankle = rob.data.bones["shin." + side].tail_local
        foot_top = ankle.z + 0.02
        # The shaft: rings from the ankle up past the breeches' hem.
        rings, top = [], PUFF[0] + 0.02
        n = 14
        for k in range(n + 1):
            z = foot_top + (top - foot_top) * k / n
            near = [p for p in pts if abs(p.z - min(z, PUFF[0] - 0.01)) <
                    0.012] or rings[-1][3]
            cx = sum(p.x for p in near) / len(near)
            cy = sum(p.y for p in near) / len(near)
            rings.append([z, max(abs(p.x - cx) for p in near),
                          max(abs(p.y - cy) for p in near), near, cx, cy])
        # Round and even along the shaft, then off the leg and flared.
        for i, r in enumerate(rings):
            win = rings[max(0, i - 3):i + 4]
            r[1] = sum(w[1] for w in win) / len(win)
            r[2] = sum(w[2] for w in win) / len(win)
        verts, faces = [], []
        for z, rx, ry, _, cx, cy in rings:
            t = min(1.0, max(0.0, (z - CUFF) / (top - CUFF)))
            grow = BOOT["shaft"] + BOOT["cuff"] * t * t * (3 - 2 * t)
            for j in range(seg):
                a = 2 * math.pi * j / seg
                verts.append(Vector((cx + (rx + grow) * math.cos(a),
                                     cy + (ry + grow) * math.sin(a), z)))
        # The cuff's lip: turned in and down a little, so the top has a
        # thickness.
        z, rx, ry, _, cx, cy = rings[-1]
        for j in range(seg):
            a = 2 * math.pi * j / seg
            verts.append(Vector((cx + (rx + BOOT["shaft"]) * math.cos(a),
                                 cy + (ry + BOOT["shaft"]) * math.sin(a),
                                 z - 0.02)))
        for k in range(len(rings)):
            for j in range(seg):
                a, b = k * seg + j, k * seg + (j + 1) % seg
                faces.append((a, b, b + seg, a + seg))
        out["shin." + side] = _mesh("boot-shaft." + side, verts, faces, mat)
        # The foot: a shoe round the body's foot, along it (heel to
        # toe), tall at the heel and instep, lower and narrower to a
        # round toe, flat beneath.
        toe = rob.data.bones["foot." + side].tail_local
        u = Vector((toe.x - ankle.x, toe.y - ankle.y, 0)).normalized()
        v = Vector((-u.y, u.x, 0))
        feet = [p for p in pts if p.z < foot_top + 0.01]
        us = [(p - ankle).dot(u) for p in feet]
        vs = [(p - ankle).dot(v) for p in feet]
        m = BOOT["foot"]
        u0, u1 = min(us) - m, max(us) + m
        v0, v1 = min(vs) - m, max(vs) + m
        bottom = min(p.z for p in feet) - BOOT["sole"] / 3
        high = foot_top + m - bottom
        verts, faces = [], []
        rows = 12
        for i in range(rows + 1):
            th = -math.pi / 2 + math.pi * i / rows
            for j in range(seg):
                ph = 2 * math.pi * j / seg

                def sp(w, e=0.6):
                    return math.copysign(abs(w) ** e, w)
                x = sp(math.cos(th)) * sp(math.cos(ph))	# heel -1, toe 1
                y = sp(math.cos(th)) * sp(math.sin(ph))
                z = sp(math.sin(th), 0.8)
                t = (x + 1) / 2
                width = 1 - 0.3 * t * t
                h = high * (1 - 0.55 * t * t)
                p = ankle + u * ((u0 + u1) / 2 + x * (u1 - u0) / 2) + \
                    v * ((v0 + v1) / 2 + y * width * (v1 - v0) / 2)
                verts.append(Vector((p.x, p.y,
                                     bottom + max(0.0, (z + 1) / 2) * h)))
        for i in range(rows):
            for j in range(seg):
                a, b = i * seg + j, i * seg + (j + 1) % seg
                faces.append((a, b, b + seg, a + seg))
        out["foot." + side] = _mesh("boot-foot." + side, verts, faces, mat)
    return out


def worn(rob):
    """The boots of the figure on rig "rob"."""
    return [o for o in rob.children if o.name.startswith("boot-")]


def rig(ob, parts):
    """The armature, bound by bone heat, then its arms and legs turned
    to hang straight down and that made its rest."""
    arm = bpy.data.armatures.new("human-rig")
    rob = bpy.data.objects.new("human-rig", arm)
    bpy.context.collection.objects.link(rob)
    bpy.context.view_layer.objects.active = rob
    bpy.ops.object.mode_set(mode="EDIT")
    eb = arm.edit_bones

    def bone(name, head, tail, parent=None):
        b = eb.new(name)
        b.head, b.tail = Vector(head), Vector(tail)
        b.roll = 0
        if parent:
            b.parent = eb[parent]
            b.use_connect = False
        return b

    bone("spine", (0, 0, ROOT), (0, 0, CHEST))
    bone("neck", (0, 0, CHEST), (0, 0, NECK), "spine")
    bone("head", (0, 0, NECK), (0, 0, CROWN), "neck")
    bone("pelvis", (0, 0, ROOT), (0, 0, ROOT - 0.06))
    for s, side in ((1, "L"), (-1, "R")):
        sh = _side(SHOULDER, s)
        d = (_side(ARM_TIP, s) - sh).normalized()
        el, wr = sh + d * UPPER, sh + d * (UPPER + FORE)
        bone("upper." + side, sh, el, "spine")
        bone("fore." + side, el, wr, "upper." + side)
        bone("hand." + side, wr, wr + d * HAND, "fore." + side)
        # The fingers, from the knuckles, to close the hand.
        bone("fingers." + side, wr + d * 0.09, wr + d * 0.2,
             "hand." + side)
        bone("thigh." + side, _side(HIP, s), _side(KNEE, s), "pelvis")
        bone("shin." + side, _side(KNEE, s), _side(ANKLE, s),
             "thigh." + side)
        bone("foot." + side, _side(ANKLE, s), _side(TOE, s),
             "shin." + side)
    bpy.ops.object.mode_set(mode="OBJECT")
    # Bound by bone heat.
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    ob.select_set(True)
    rob.select_set(True)
    bpy.context.view_layer.objects.active = rob
    bpy.ops.object.parent_set(type="ARMATURE_AUTO")
    # The limbs straightened: each chain turned to hang down, the foot
    # forward; then that pose made the mesh's and the rig's rest.
    bpy.ops.object.mode_set(mode="POSE")
    for name in ("upper.L", "upper.R", "fore.L", "fore.R", "hand.L",
                 "hand.R", "fingers.L", "fingers.R", "thigh.L", "thigh.R",
                 "shin.L", "shin.R"):
        pb = rob.pose.bones[name]
        bpy.context.view_layer.update()
        cur = (pb.tail - pb.head).normalized()
        want = Vector((0, 0, -1))
        q = cur.rotation_difference(want)
        mat = pb.matrix.copy()
        loc = mat.to_translation()
        pb.matrix = (Matrix.Translation(loc) @ q.to_matrix().to_4x4() @
                     Matrix.Translation(-loc) @ mat)
        bpy.context.view_layer.update()
    bpy.ops.object.mode_set(mode="OBJECT")
    bpy.context.view_layer.objects.active = ob
    for m in ob.modifiers:
        if m.type == "ARMATURE":
            # Below the smoothing, applied: the mesh keeps the pose.
            bpy.ops.object.modifier_move_to_index(modifier=m.name, index=0)
            bpy.ops.object.modifier_apply(modifier=m.name)
    bpy.context.view_layer.objects.active = rob
    bpy.ops.object.mode_set(mode="POSE")
    bpy.ops.pose.armature_apply(selected=False)
    bpy.ops.object.mode_set(mode="OBJECT")
    mod = ob.modifiers.new("rig", "ARMATURE")
    mod.object = rob
    ob.modifiers.move(len(ob.modifiers) - 1, 0)
    for pb in rob.pose.bones:
        pb.rotation_mode = "XYZ"
    return rob


def build(look, kerchief=(0.6, 0.05, 0.04), loose=None, neckline=None,
          style=None):
    """The dressed, rigged human: (rig, [its meshes]); "kerchief" its
    colour, or None for bare hair; "loose" and "neckline" as dress()
    takes them; "style" the hair's (HAIR), by default "under" a
    kerchief, else "long"."""
    ob = _load()
    dress(ob, look, kerchief, loose, neckline)
    parts = []
    rob = rig(ob, parts)
    # The boots, each piece carried by its bone.
    bpy.context.view_layer.update()
    for bone, b in boots(ob, rob).items():
        b.parent = rob
        b.parent_type = "BONE"
        b.parent_bone = bone
        bpy.context.view_layer.update()
        b.matrix_world = Matrix.Identity(4)
    jaw(ob, rob)
    # The hair last, carried by the head (the chin's by the jaw).
    bpy.context.view_layer.update()
    for h in hair(ob, look, style or ("under" if kerchief else "long")):
        h.parent = rob
        h.parent_type = "BONE"
        h.parent_bone = "jaw" if h.name.endswith("-jaw") else "head"
        bpy.context.view_layer.update()
        h.matrix_world = Matrix.Identity(4)
    # Facing +X, as fencer.py's figure.
    rob.rotation_euler = (0, 0, math.pi / 2)
    return rob, [ob] + parts


def swap(name):
    """The bone on the other side."""
    if name[-2:] in (".L", ".R"):
        return name[:-1] + ("R" if name[-1] == "L" else "L")
    return name


def pose(rob, p, heading=0.0):
    """Set fencer.py's pose "p" (degrees, positive towards the front,
    the hips at p["hips"] metres, p["x"] ahead), the figure turned to
    "heading".  With p["straddle"], the hips are turned p["turn"]
    degrees toward the viewer (-Y) and the legs bend across the picture,
    each knee out: the front leg's toward "heading", the back leg's away
    from it, both legs' angles positive outward; the front limbs are
    then the left ones, on the side "heading" goes to, the sword in the
    left hand."""
    h = math.radians(heading)
    rob.location = (p.get("x", 0.0) * math.cos(h), 0,
                    p.get("hips", 0.9) - 0.9)
    turn = -math.cos(h) * math.radians(p.get("turn", 0.0))
    rob.rotation_euler = (0, 0, math.pi / 2 + h + turn)
    straddle = p.get("straddle")
    for key, name in BONES.items():
        if straddle:
            name = swap(name)
        pb = rob.pose.bones[name]
        a = math.radians(p.get(key, 0.0))
        # About the figure's side axis (the rig's X), in the bone's own
        # frame: a limb hanging down swings to the front (-Y) for a
        # positive angle, one standing up leans to it.
        up = pb.bone.tail_local.z > pb.bone.head_local.z
        axis = pb.bone.matrix_local.to_3x3().inverted() @ Vector((1, 0, 0))
        pb.rotation_mode = "AXIS_ANGLE"
        pb.rotation_axis_angle = (a if up else -a, *axis)
    if straddle:
        # Each thigh turned about the upright so that its swing to the
        # front goes out across the picture instead.
        inv = rob.matrix_basis.to_3x3().inverted()
        for side, s in (("L", 1), ("R", -1)):
            d = inv @ Vector((s * math.cos(h), s * math.sin(h), 0))
            pb = rob.pose.bones["thigh." + side]
            swing = Quaternion(Vector(pb.rotation_axis_angle[1:]),
                               pb.rotation_axis_angle[0])
            up = pb.bone.matrix_local.to_3x3().inverted() @ \
                Vector((0, 0, 1))
            pb.rotation_mode = "QUATERNION"
            pb.rotation_quaternion = Quaternion(
                up, math.atan2(d.x, -d.y)) @ swing
    # The chest turned about the spine (p["twist"], degrees), the arms
    # and head with it, the hips and legs staying as they are.
    if p.get("twist"):
        pb = rob.pose.bones["spine"]
        bend = Quaternion(Vector(pb.rotation_axis_angle[1:]),
                          pb.rotation_axis_angle[0])
        pb.rotation_mode = "QUATERNION"
        pb.rotation_quaternion = bend @ Quaternion(
            (0, 1, 0), math.radians(p["twist"]))
    # The arms raised out to the sides (p["front-spread"],
    # p["back-spread"], degrees away from the body), about the figure's
    # forward axis (the rig's Y), after their swing.
    for key, name in (("front-spread", "upper.R"),
                      ("back-spread", "upper.L")):
        if not p.get(key):
            continue
        if straddle:
            name = swap(name)
        s = 1 if name.endswith("L") else -1
        pb = rob.pose.bones[name]
        swing = Quaternion(Vector(pb.rotation_axis_angle[1:]),
                           pb.rotation_axis_angle[0])
        axis = pb.bone.matrix_local.to_3x3().inverted() @ Vector((0, 1, 0))
        pb.rotation_mode = "QUATERNION"
        pb.rotation_quaternion = Quaternion(
            axis, s * math.radians(p[key])) @ swing
    # The upper arms rolled about their length (p["front-roll"],
    # p["back-roll"], degrees, the elbow's bend turned up for positive),
    # before their swing and spread: a raised hand.
    for key, name in (("front-roll", "upper.R"), ("back-roll", "upper.L")):
        if not p.get(key):
            continue
        if straddle:
            name = swap(name)
        s = 1 if name.endswith("L") else -1
        pb = rob.pose.bones[name]
        if pb.rotation_mode == "AXIS_ANGLE":
            q = Quaternion(Vector(pb.rotation_axis_angle[1:]),
                           pb.rotation_axis_angle[0])
        else:
            q = pb.rotation_quaternion.copy()
        pb.rotation_mode = "QUATERNION"
        pb.rotation_quaternion = q @ Quaternion(
            (0, 1, 0), s * math.radians(p[key]))
    # The sword hand turned about the forearm (p["front-turn"],
    # degrees), after its bend: the blade aimed with the wrist rather
    # than the whole arm rolled.
    if p.get("front-turn"):
        name = swap("hand.R") if straddle else "hand.R"
        s = 1 if name.endswith("L") else -1
        pb = rob.pose.bones[name]
        q = Quaternion(Vector(pb.rotation_axis_angle[1:]),
                       pb.rotation_axis_angle[0]) \
            if pb.rotation_mode == "AXIS_ANGLE" else \
            pb.rotation_quaternion.copy()
        pb.rotation_mode = "QUATERNION"
        pb.rotation_quaternion = q @ Quaternion(
            (0, 1, 0), s * math.radians(p["front-turn"]))
    # The hands closed: the sword hand round its grip (p["grip"] degrees
    # of curl, open once it lets go), the other loosely, each open with
    # p["front-open"] or p["back-open"] (by default p["open"]); the
    # fingers curl towards the body's middle (about the rig's Y).
    sword = "L" if straddle else "R"

    def opened(limb):
        return p.get(limb + "-open", p.get("open"))
    for side, curl in ((sword, OPEN if opened("front")
                        else p.get("grip", 100)),
                       ("R" if straddle else "L", OPEN if opened("back")
                        else 45)):
        pb = rob.pose.bones["fingers." + side]
        axis = pb.bone.matrix_local.to_3x3().inverted() @ Vector((0, 1, 0))
        pb.rotation_mode = "AXIS_ANGLE"
        pb.rotation_axis_angle = (math.radians(curl if side == "L" else
                                               -curl), *axis)
    # The mouth opened, the jaw dropped about the head's side axis.
    pb = rob.pose.bones["jaw"]
    axis = pb.bone.matrix_local.to_3x3().inverted() @ Vector((1, 0, 0))
    pb.rotation_mode = "AXIS_ANGLE"
    pb.rotation_axis_angle = (math.radians(JAW * p.get("mouth", 0.0)),
                              *axis)


def ground(rob, height=None):
    """Lower or raise the posed figure so its lower foot stands on the
    ground: its ankle at the rest's height above it."""
    bpy.context.view_layer.update()
    rest = ANKLE[2] if height is None else height
    low = min((rob.matrix_world @ rob.pose.bones[b].head).z
              for b in ("foot.L", "foot.R"))
    rob.location.z += rest - low
