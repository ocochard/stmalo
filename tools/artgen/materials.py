#!/usr/bin/env python3
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026, Olivier Cochard-Labbé
#
"""Photographic materials and sky for the pack's Blender models.

The models (fencers/fencer.py, fencers/walker.py, ships/ship.py) are
rendered with real scanned materials, so their renders are the pack's
pictures as they are, with no image model over them: colour, roughness
and normal maps from ambientCG, and a sky from Poly Haven, both public
domain (CC0).  LIBRARY names what each part wears; the files are fetched
on the render host into $FLIBUSTIER_CC0 (~/flibustier-cc0) by fetch(),
and the pack's provenance records the asset names.

Blender only (bpy); a model calls pbr() and sky(), which fall back to a
plain colour when the files are missing.
"""

import glob
import os

LIB = os.environ.get("FLIBUSTIER_CC0", os.path.expanduser("~/flibustier-cc0"))
RES = "2K"	# the materials' resolution: the fencers fill the screen

# What each kind of part is made of: an ambientCG material, and how many
# times its picture repeats in a metre.
LIBRARY = {
    "linen": ("Fabric061", 3.0),
    "wool": ("Fabric066", 3.5),
    "leather": ("Leather026", 5.0),
    "planks": ("Planks023A", 1.2),
    "canvas": ("Fabric086", 2.0),
    "rope": ("Rope001", 8.0),
    "iron": ("Metal041B", 2.0),
    "gilt": ("Metal048C", 3.0),
    # The fencers' clothes: cloths photographed by the image model for
    # this pack (artgen.py, fencer_fabrics()), plain where not made yet.
    "fencer-shirt": ("flibustier-shirt", 4.0),
    "fencer-sash": ("flibustier-sash", 4.0),
    "fencer-breeches": ("flibustier-breeches", 4.0),
    "fencer-boots": ("flibustier-boots", 5.0),
}
SKY = "kloofendal_48d_partly_cloudy_puresky"	# a Poly Haven HDRI

_means = {}


def _file(asset, kind):
    found = sorted(glob.glob(os.path.join(LIB, asset + "_" + RES,
                                          "*_%s.jpg" % kind))) or sorted(
        glob.glob(os.path.join(LIB, asset, "*_%s.jpg" % kind)))
    return found[0] if found else None


def _mean(image):
    """The picture's mean brightness, to tint it without darkening
    (sampled: the host's Blender may have no numpy)."""
    if image.name not in _means:
        px = image.pixels[:]
        step = 4 * 97
        vals = [(px[i] + px[i + 1] + px[i + 2]) / 3
                for i in range(0, len(px) - 3, step)]
        _means[image.name] = max(0.05, sum(vals) / max(1, len(vals)))
    return _means[image.name]


def pbr(name, kind, colour=None, rough=0.7, metal=0.0, sss=0.0):
    """A material of LIBRARY's "kind" (or a plain one if None), tinted to
    "colour": its picture's light and dark kept, its hue the colour's."""
    import bpy
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    bsdf.inputs["Roughness"].default_value = rough
    bsdf.inputs["Metallic"].default_value = metal
    if colour is not None:
        bsdf.inputs["Base Color"].default_value = (*colour, 1.0)
    if sss and "Subsurface Weight" in bsdf.inputs:
        bsdf.inputs["Subsurface Weight"].default_value = sss
    if kind is None or kind not in LIBRARY:
        return m
    asset, repeat = LIBRARY[kind]
    col = _file(asset, "Color")
    if col is None:
        return m
    coord = nt.nodes.new("ShaderNodeTexCoord")
    mapping = nt.nodes.new("ShaderNodeMapping")
    mapping.inputs["Scale"].default_value = (repeat, repeat, repeat)
    nt.links.new(coord.outputs["Object"], mapping.inputs["Vector"])

    def tex(path, data):
        t = nt.nodes.new("ShaderNodeTexImage")
        t.image = bpy.data.images.load(path, check_existing=True)
        if data:
            t.image.colorspace_settings.name = "Non-Color"
        t.projection = "BOX"
        t.projection_blend = 0.3
        nt.links.new(mapping.outputs["Vector"], t.inputs["Vector"])
        return t

    c = tex(col, False)
    if colour is None:
        nt.links.new(c.outputs["Color"], bsdf.inputs["Base Color"])
    else:
        bw = nt.nodes.new("ShaderNodeRGBToBW")
        nt.links.new(c.outputs["Color"], bw.inputs["Color"])
        norm = nt.nodes.new("ShaderNodeMath")
        norm.operation = "DIVIDE"
        norm.inputs[1].default_value = _mean(c.image)
        nt.links.new(bw.outputs["Val"], norm.inputs[0])
        mix = nt.nodes.new("ShaderNodeMix")
        mix.data_type = "RGBA"
        mix.blend_type = "MULTIPLY"
        mix.inputs["Factor"].default_value = 1.0
        mix.inputs["A"].default_value = (*colour, 1.0)
        nt.links.new(norm.outputs["Value"], mix.inputs["B"])
        nt.links.new(mix.outputs["Result"], bsdf.inputs["Base Color"])
    r = _file(asset, "Roughness")
    if r:
        nt.links.new(tex(r, True).outputs["Color"], bsdf.inputs["Roughness"])
    n = _file(asset, "NormalGL")
    if n:
        nm = nt.nodes.new("ShaderNodeNormalMap")
        nm.inputs["Strength"].default_value = 0.45	# cloth, not terry
        nt.links.new(tex(n, True).outputs["Color"], nm.inputs["Color"])
        nt.links.new(nm.outputs["Normal"], bsdf.inputs["Normal"])
    if metal and _file(asset, "Metalness"):
        nt.links.new(tex(_file(asset, "Metalness"), True).outputs["Color"],
                     bsdf.inputs["Metallic"])
    return m


def skin(name, colour):
    """Skin, procedural: its tone varying in soft blotches (redder,
    paler), light scattered under it, a fine bump of pores and a sheen
    that varies."""
    import bpy
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    coord = nt.nodes.new("ShaderNodeTexCoord")
    big = nt.nodes.new("ShaderNodeTexNoise")
    big.inputs["Scale"].default_value = 18.0
    big.inputs["Detail"].default_value = 4.0
    nt.links.new(coord.outputs["Object"], big.inputs["Vector"])
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    r, g, b_ = colour
    ramp.color_ramp.elements[0].position = 0.35
    ramp.color_ramp.elements[0].color = (r * 0.9, g * 0.82, b_ * 0.8, 1)
    ramp.color_ramp.elements[1].position = 0.7
    ramp.color_ramp.elements[1].color = (min(1, r * 1.12), g * 1.02,
                                         b_ * 0.98, 1)
    nt.links.new(big.outputs["Fac"], ramp.inputs["Fac"])
    nt.links.new(ramp.outputs["Color"], bsdf.inputs["Base Color"])
    if "Subsurface Weight" in bsdf.inputs:
        bsdf.inputs["Subsurface Weight"].default_value = 0.25
        bsdf.inputs["Subsurface Radius"].default_value = (0.9, 0.35, 0.2)
        bsdf.inputs["Subsurface Scale"].default_value = 0.006
    pores = nt.nodes.new("ShaderNodeTexNoise")
    pores.inputs["Scale"].default_value = 900.0
    pores.inputs["Detail"].default_value = 2.0
    nt.links.new(coord.outputs["Object"], pores.inputs["Vector"])
    bump = nt.nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.12
    bump.inputs["Distance"].default_value = 0.0005
    nt.links.new(pores.outputs["Fac"], bump.inputs["Height"])
    nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    rough = nt.nodes.new("ShaderNodeMapRange")
    rough.inputs["To Min"].default_value = 0.38
    rough.inputs["To Max"].default_value = 0.58
    nt.links.new(big.outputs["Fac"], rough.inputs["Value"])
    nt.links.new(rough.outputs["Result"], bsdf.inputs["Roughness"])
    return m


def sky(scene, strength=1.0, fallback=(0.6, 0.65, 0.75)):
    """The world lit by the sky's photograph (only its light: the film
    stays transparent), or a plain colour without it."""
    import bpy
    world = bpy.data.worlds.new("sky")
    world.use_nodes = True
    nt = world.node_tree
    bg = nt.nodes["Background"]
    bg.inputs["Strength"].default_value = strength
    path = os.path.join(LIB, "%s_1k.hdr" % SKY)
    if os.path.exists(path):
        env = nt.nodes.new("ShaderNodeTexEnvironment")
        env.image = bpy.data.images.load(path, check_existing=True)
        nt.links.new(env.outputs["Color"], bg.inputs["Color"])
    else:
        bg.inputs["Color"].default_value = (*fallback, 1)
    scene.world = world
    return world


def fetch_command():
    """A shell command that fetches the library on the render host, once."""
    cmds = ['mkdir -p "%s" && cd "%s"' % (LIB.replace(
        os.path.expanduser("~"), "$HOME"), LIB.replace(
        os.path.expanduser("~"), "$HOME"))]
    for asset, _ in sorted(set(LIBRARY.values())):
        d = asset + "_" + RES
        cmds.append('[ -d %s ] || { curl -sfL -o %s.zip '
                    '"https://ambientcg.com/get?file=%s_%s-JPG.zip" && '
                    'mkdir %s && (cd %s && unzip -q ../%s.zip) && '
                    'rm %s.zip; }' % (d, d, asset, RES, d, d, d, d))
    # The human body: Blender's Human Base Meshes bundle (CC0).
    cmds.append('[ -d human ] || { mkdir human && (cd human && curl -sfL '
                '-o hbm.zip "https://download.blender.org/demo/asset-bundles/'
                'human-base-meshes/human-base-meshes-bundle-v1.4.1.zip" && '
                'unzip -q hbm.zip && rm hbm.zip); }')
    cmds.append('[ -f %s_1k.hdr ] || curl -sfL -o %s_1k.hdr '
                '"https://dl.polyhaven.org/file/ph-assets/HDRIs/hdr/1k/'
                '%s_1k.hdr"' % ((SKY,) * 3))
    return " && ".join(cmds)
