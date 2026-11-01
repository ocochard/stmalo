#!/usr/bin/env python3
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026, Olivier Cochard-Labbé
#
"""Generate the open pack's art with ComfyUI.

Each asset of assets.toml is rendered from its text prompt alone (never
from original art, so the result can carry an open licence) with
Qwen-Image-2512, resized to its size at the pack's art height, and
written to the pack with a provenance record: model files, prompt, seed
and workflow, enough to render it again.

    artgen.py [--server URL] [--pack DIR] [--draft] [--seed N] [ID ...]

Without IDs, every asset is rendered.  --draft uses the 8-step Lightning
LoRA, for trying prompts; finals use the full sampler.
"""

import argparse
import datetime
import hashlib
import json
import math
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time
import tomllib
import urllib.parse
import urllib.request
import uuid

HERE = os.path.dirname(os.path.abspath(__file__))
SCREEN_W, SCREEN_H = 640, 480
SKETCH_W = 1472			# layouts/sketch.py's width
SHIP_RENDER = (832, 624)	# a ship model's heading as rendered
DAMAGE_RENDER = (1304, 776)	# a ship model's side view as rendered

MODELS = {
    # DiffSynth's Canny patch: lighter than the InstantX Union ControlNet,
    # which with the model exhausts framework2's shared-memory GPU.
    "controlnet": "qwen_image_canny_diffsynth_controlnet.safetensors",
    "unet": "qwen_image_2512_fp8_e4m3fn.safetensors",
    "clip": "qwen_2.5_vl_7b_fp8_scaled.safetensors",
    "vae": "qwen_image_vae.safetensors",
    "lightning": "Qwen-Image-2512-Lightning-8steps-V1.0-bf16.safetensors",
    # Qwen-Image-Edit-2511 puts a character from their portrait into a
    # scene, keeping their face (the cast pass).
    "edit": "qwen_image_edit_2511_int8_convrot.safetensors",
    "edit_lightning":
        "Qwen-Image-Edit-2511-Lightning-8steps-V1.0-bf16.safetensors",
    # BiRefNet (MIT) cuts the added person out of the edited crop.
    "matte": "birefnet.safetensors",
}
# The cast pass: its model's own settings (the ComfyUI template's).
EDIT_PROFILES = {
    "final": {"steps": 40, "cfg": 4.0, "lora": False},
    "draft": {"steps": 8, "cfg": 1.0, "lora": True},
}

# Sampling: the full model for finals, Lightning for drafts.
PROFILES = {
    "final": {"steps": 40, "cfg": 2.5, "lora": False},
    "draft": {"steps": 8, "cfg": 1.0, "lora": True},
}
SHIFT = 3.1


def generation_size(w, h, area=1.0):
    """Qwen-Image works best near 1.6 megapixels ("area" times that):
    keep the aspect, use multiples of 16."""
    scale = (area * 1472 * 1104 / (w * h)) ** 0.5
    return (max(16, round(w * scale / 16) * 16),
            max(16, round(h * scale / 16) * 16))


def characters(text, spec):
    """Put each "{name}" of [characters] into a prompt, so a person looks
    the same in every picture they appear in."""
    for name, desc in spec.get("characters", {}).items():
        text = text.replace("{%s}" % name, " ".join(desc.split()))
    if "{" in text:
        sys.exit("unknown character in: " + text.strip()[:60])
    return text


def workflow(asset, profile, seed, out_w, out_h, prefix, spec):
    """The ComfyUI API graph for one asset.  A framed asset is rendered
    at its frame's size and placed on a plain field of the full size."""
    p = PROFILES[profile]
    k = spec["pack"]["art_height"] / SCREEN_H
    frame = asset.get("frame")
    prompt = characters(asset["prompt"], spec)
    if frame:
        fx, fy, fw, fh = (round(v * k) for v in frame)
        if asset.get("gilded", True):
            prompt = prompt.strip() + " " + spec["pack"]["framed"].strip()
        rw, rh = fw, fh
    elif asset.get("scene_area"):
        # The scene rendered at the shape of the part of the screen it
        # shows (the street above the duel panel), then put there.
        sx_, sy_, rw, rh = (round(v * k) for v in asset["scene_area"])
    else:
        rw, rh = out_w, out_h
    if spec["pack"].get("style"):
        prompt = prompt.strip() + " " + spec["pack"]["style"].strip()
    # A ControlNet encodes its sketch in one piece at the render's size,
    # which the shared-memory GPU may refuse: such assets render smaller.
    gw, gh = generation_size(rw, rh, asset.get("gen_area", 1.0))
    grade = spec.get("grade") if asset.get("grade", True) else None
    trim = asset.get("trim", spec["pack"].get("frame_trim", 0.0)
                     if frame else 0.0)
    g = {
        "unet": {"class_type": "UNETLoader", "inputs": {
            "unet_name": MODELS["unet"], "weight_dtype": "fp8_e4m3fn"}},
        "clip": {"class_type": "CLIPLoader", "inputs": {
            "clip_name": MODELS["clip"], "type": "qwen_image"}},
        "vae": {"class_type": "VAELoader", "inputs": {
            "vae_name": MODELS["vae"]}},
        "pos": {"class_type": "CLIPTextEncode", "inputs": {
            "text": prompt, "clip": ["clip", 0]}},
        "neg": {"class_type": "CLIPTextEncode", "inputs": {
            "text": asset.get("negative", DEFAULT_NEGATIVE),
            "clip": ["clip", 0]}},
        "latent": {"class_type": "EmptySD3LatentImage", "inputs": {
            "width": gw, "height": gh, "batch_size": 1}},
        # Tiled: one full-size decode needs a single large allocation,
        # which the shared-memory GPU of framework2 refuses.
        "decode": {"class_type": "VAEDecodeTiled", "inputs": {
            "samples": ["sample", 0], "vae": ["vae", 0],
            "tile_size": 512, "overlap": 64, "temporal_size": 64,
            "temporal_overlap": 8}},
        "resize": {"class_type": "ImageScale", "inputs": {
            "image": ["trim", 0] if trim else ["decode", 0],
            "upscale_method": "lanczos",
            "width": rw, "height": rh, "crop": "center"}},
        "save": {"class_type": "SaveImage", "inputs": {
            "images": ["resize", 0], "filename_prefix": prefix}},
    }
    if trim:
        # The model may leave some wall or table around the subject: cut
        # into the render so the subject reaches the edges.
        cx, cy = round(gw * trim), round(gh * trim)
        g["trim"] = {"class_type": "ImageCrop", "inputs": {
            "image": ["decode", 0], "width": gw - 2 * cx,
            "height": gh - 2 * cy, "x": cx, "y": cy}}
    picture = ["resize", 0]
    if asset.get("scene_area"):
        g["screen"] = {"class_type": "EmptyImage", "inputs": {
            "width": out_w, "height": out_h, "batch_size": 1,
            "color": 0}}
        g["onscreen"] = {"class_type": "ImageCompositeMasked", "inputs": {
            "destination": ["screen", 0], "source": picture,
            "x": sx_, "y": sy_, "resize_source": False}}
        picture = ["onscreen", 0]
        g["save"]["inputs"]["images"] = picture
        rw, rh = out_w, out_h
    # Pieces the screen needs at exact places (the board its title is
    # written on, page-turn arrows...), rendered on their own and placed
    # there: the scene's model will not put them there.  "plaque" is a
    # piece with the pack's plaque prompt.
    pieces = list(asset.get("pieces", []))
    if asset.get("plaque"):
        pieces.insert(0, {"rect": asset["plaque"],
                          "prompt": spec["pack"]["plaque"]})
    if pieces:
        g["pshift"] = {"class_type": "ModelSamplingAuraFlow", "inputs": {
            "model": ["lora", 0] if p["lora"] else ["unet", 0],
            "shift": SHIFT}}
    for n, piece in enumerate(pieces):
        px, py, pw, ph = (round(v * k) for v in piece["rect"])
        if "mirror" in piece:
            # The same piece as an earlier one, flipped left to right
            # (a pair of page-turn arrows that must match).
            m = piece["mirror"] + (1 if asset.get("plaque") else 0)
            g["presize%d" % n] = {"class_type": "ImageFlip", "inputs": {
                "image": ["presize%d" % m, 0],
                "flip_method": "y-axis: horizontally"}}
            piece = dict(pieces[m], **piece)
            place_piece(g, n, piece, picture, px, py, pw, ph)
            picture = ["pplace%d" % n, 0]
            g["save"]["inputs"]["images"] = picture
            continue
        if "colour" in piece:
            # A plain field (the panel text is written on), not drawn:
            # the model puts an object on any cloth it is asked for.
            g["presize%d" % n] = {"class_type": "EmptyImage", "inputs": {
                "width": pw, "height": ph, "batch_size": 1,
                "color": int(piece["colour"].lstrip("#"), 16)}}
            place_piece(g, n, piece, picture, px, py, pw, ph)
            picture = ["pplace%d" % n, 0]
            g["save"]["inputs"]["images"] = picture
            continue
        # "upright": rendered a quarter turn round and turned back (the
        # model draws wood grain running up far more readily).
        up = piece.get("upright")
        pgw, pgh = generation_size(ph, pw, 0.25) if up else \
            generation_size(pw, ph, 0.25)
        ptext = piece["prompt"].strip()
        if spec["pack"].get("style"):
            ptext += " " + spec["pack"]["style"].strip()
        g["ppos%d" % n] = {"class_type": "CLIPTextEncode", "inputs": {
            "text": ptext, "clip": ["clip", 0]}}
        g["platent%d" % n] = {"class_type": "EmptySD3LatentImage",
                              "inputs": {"width": pgw, "height": pgh,
                                         "batch_size": 1}}
        g["psample%d" % n] = {"class_type": "KSampler", "inputs": {
            "model": ["pshift", 0], "seed": seed + n, "steps": p["steps"],
            "cfg": p["cfg"], "sampler_name": "euler", "scheduler": "simple",
            "positive": ["ppos%d" % n, 0], "negative": ["neg", 0],
            "latent_image": ["platent%d" % n, 0], "denoise": 1.0}}
        g["pdecode%d" % n] = {"class_type": "VAEDecodeTiled", "inputs": {
            "samples": ["psample%d" % n, 0], "vae": ["vae", 0],
            "tile_size": 512, "overlap": 64, "temporal_size": 64,
            "temporal_overlap": 8}}
        decoded = ["pdecode%d" % n, 0]
        if up:
            g["pturn%d" % n] = {"class_type": "ImageRotate", "inputs": {
                "image": decoded, "rotation": "90 degrees"}}
            decoded = ["pturn%d" % n, 0]
        g["presize%d" % n] = {"class_type": "ImageScale", "inputs": {
            "image": decoded, "upscale_method": "lanczos",
            "width": pw, "height": ph, "crop": "center"}}
        place_piece(g, n, piece, picture, px, py, pw, ph)
        picture = ["pplace%d" % n, 0]
        g["save"]["inputs"]["images"] = picture
    for n, (shade, colour) in enumerate(asset.get("uploaded_bevel", [])):
        # Exact edges the model will not draw where the game needs them
        # (the duel panel's boxes, the arrow pad's arrows): light and
        # shadow laid over the picture through a mask drawn from the
        # measured layout.
        g["bvfill%d" % n] = {"class_type": "EmptyImage", "inputs": {
            "width": rw, "height": rh, "batch_size": 1, "color": colour}}
        g["bvmask%d" % n] = {"class_type": "LoadImage", "inputs": {
            "image": shade}}
        g["bvfit%d" % n] = {"class_type": "ImageScale", "inputs": {
            "image": ["bvmask%d" % n, 0], "upscale_method": "bilinear",
            "width": rw, "height": rh, "crop": "disabled"}}
        g["bvm%d" % n] = {"class_type": "ImageToMask", "inputs": {
            "image": ["bvfit%d" % n, 0], "channel": "red"}}
        g["bvlay%d" % n] = {"class_type": "ImageCompositeMasked", "inputs": {
            "destination": picture, "source": ["bvfill%d" % n, 0],
            "x": 0, "y": 0, "resize_source": False,
            "mask": ["bvm%d" % n, 0]}}
        picture = ["bvlay%d" % n, 0]
        g["save"]["inputs"]["images"] = picture
    if grade and grade.get("uploaded"):
        # One colour grade for the series: match the colours of the
        # reference picture, then a touch of contrast.
        g["ref"] = {"class_type": "LoadImage", "inputs": {
            "image": grade["uploaded"]}}
        g["transfer"] = {"class_type": "ColorTransfer", "inputs": {
            "image_target": picture, "image_ref": ["ref", 0],
            "method": grade.get("method", "reinhard_lab"),
            "source_stats": "per_frame",
            "strength": grade.get("strength", 0.5)}}
        g["contrast"] = {"class_type": "AdjustContrast", "inputs": {
            "images": ["transfer", 0],
            "factor": grade.get("contrast", 1.0)}}
        picture = ["contrast", 0]
        g["save"]["inputs"]["images"] = picture
    if frame:
        g["field"] = {"class_type": "EmptyImage", "inputs": {
            "width": out_w, "height": out_h, "batch_size": 1,
            "color": int(asset["field"].lstrip("#"), 16)}}
        g["place"] = {"class_type": "ImageCompositeMasked", "inputs": {
            "destination": ["field", 0], "source": picture,
            "x": fx, "y": fy, "resize_source": False}}
        g["save"]["inputs"]["images"] = ["place", 0]
    model = ["unet", 0]
    if asset.get("uploaded_control"):
        # A line sketch of the composition, drawn for this pack: the
        # model follows it early, then renders freely.
        g["cpatch"] = {"class_type": "ModelPatchLoader", "inputs": {
            "name": MODELS["controlnet"]}}
        g["sketch"] = {"class_type": "LoadImage", "inputs": {
            "image": asset["uploaded_control"]}}
        # At the generation's size, which the control needs (a sketch
        # of another shape runs out of memory).
        g["sketchfit"] = {"class_type": "ImageScale", "inputs": {
            "image": ["sketch", 0], "upscale_method": "bilinear",
            "width": gw, "height": gh, "crop": "disabled"}}
        if asset.get("scene_area"):
            # The sketch is of the whole screen: only the scene's part.
            ks = SKETCH_W / SCREEN_W
            ax, ay, aw, ah = (round(v * ks) for v in asset["scene_area"])
            g["sketchpart"] = {"class_type": "ImageCrop", "inputs": {
                "image": ["sketch", 0], "width": aw, "height": ah,
                "x": ax, "y": ay}}
            g["sketchfit"]["inputs"]["image"] = ["sketchpart", 0]
        g["control"] = {"class_type": "QwenImageDiffsynthControlnet",
                        "inputs": {
                            "model": model, "model_patch": ["cpatch", 0],
                            "vae": ["vae", 0], "image": ["sketchfit", 0],
                            "strength": asset.get("control_strength", 0.7),
                            "start_percent": 0.0,
                            "end_percent": asset.get("control_end", 0.6)}}
        model = ["control", 0]
    if p["lora"]:
        g["lora"] = {"class_type": "LoraLoaderModelOnly", "inputs": {
            "model": model, "lora_name": MODELS["lightning"],
            "strength_model": 1.0}}
        model = ["lora", 0]
    g["shift"] = {"class_type": "ModelSamplingAuraFlow", "inputs": {
        "model": model, "shift": SHIFT}}
    positive, negative = ["pos", 0], ["neg", 0]
    if asset.get("uploaded_inpaint"):
        # Repaint a masked region the model keeps getting wrong, without
        # the sketch, and keep every other pixel of the render.
        g["ishift"] = {"class_type": "ModelSamplingAuraFlow", "inputs": {
            "model": ["lora", 0] if p["lora"] else ["unet", 0],
            "shift": SHIFT}}
        g["imask"] = {"class_type": "LoadImageMask", "inputs": {
            "image": asset["uploaded_inpaint"], "channel": "red"}}
        base = ["decode", 0]
        fill = asset.get("inpaint_fill")
        if fill:
            # Cover the region with a patch of plain background first, so
            # a light repaint blends it in instead of redrawing what the
            # surroundings suggest.  fill = [from_x, from_y, x, y, w, h].
            g["patch"] = {"class_type": "ImageCrop", "inputs": {
                "image": base, "width": fill[4], "height": fill[5],
                "x": fill[0], "y": fill[1]}}
            g["prefill"] = {"class_type": "ImageCompositeMasked",
                            "inputs": {
                                "destination": base, "source": ["patch", 0],
                                "x": fill[2], "y": fill[3],
                                "resize_source": False}}
            base = ["prefill", 0]
        g["ienc"] = {"class_type": "VAEEncodeTiled", "inputs": {
            "pixels": base, "vae": ["vae", 0], "tile_size": 512,
            "overlap": 64, "temporal_size": 64, "temporal_overlap": 8}}
        g["inoise"] = {"class_type": "SetLatentNoiseMask", "inputs": {
            "samples": ["ienc", 0], "mask": ["imask", 0]}}
        iprompt = characters(asset["inpaint_prompt"], spec).strip()
        if spec["pack"].get("style"):
            iprompt += " " + spec["pack"]["style"].strip()
        g["ipos"] = {"class_type": "CLIPTextEncode", "inputs": {
            "text": iprompt, "clip": ["clip", 0]}}
        g["isample"] = {"class_type": "KSampler", "inputs": {
            "model": ["ishift", 0], "seed": seed, "steps": p["steps"],
            "cfg": p["cfg"], "sampler_name": "euler", "scheduler": "simple",
            "positive": ["ipos", 0], "negative": ["neg", 0],
            "latent_image": ["inoise", 0],
            "denoise": asset.get("inpaint_denoise", 1.0)}}
        g["idecode"] = {"class_type": "VAEDecodeTiled", "inputs": {
            "samples": ["isample", 0], "vae": ["vae", 0], "tile_size": 512,
            "overlap": 64, "temporal_size": 64, "temporal_overlap": 8}}
        g["repaint"] = {"class_type": "ImageCompositeMasked", "inputs": {
            "destination": base, "source": ["idecode", 0],
            "x": 0, "y": 0, "resize_source": False,
            "mask": ["imask", 0]}}
        rendered = ["repaint", 0]
    else:
        rendered = ["decode", 0]
    if trim:
        g["trim"]["inputs"]["image"] = rendered
    else:
        g["resize"]["inputs"]["image"] = rendered
    g["sample"] = {"class_type": "KSampler", "inputs": {
        "model": ["shift", 0], "seed": seed, "steps": p["steps"],
        "cfg": p["cfg"], "sampler_name": "euler", "scheduler": "simple",
        "positive": positive, "negative": negative,
        "latent_image": ["latent", 0], "denoise": 1.0}}
    return g


DEFAULT_NEGATIVE = ("text, letters, words, watermark, signature, logo, "
                    "frame, border, cartoon, illustration, painting, "
                    "anime, 3d render, lowres, blurry, deformed")


def reachable(server):
    """The server's URL with the first address that answers: the host
    may resolve to addresses the server does not listen on (IPv6 for a
    ComfyUI on 0.0.0.0), each of which would stall every request."""
    u = urllib.parse.urlsplit(server)
    port = u.port or 80
    for family, kind, proto, _, addr in socket.getaddrinfo(
            u.hostname, port, type=socket.SOCK_STREAM):
        try:
            with socket.socket(family, kind, proto) as sock:
                sock.settimeout(2)
                sock.connect(addr)
        except OSError:
            continue
        host = "[%s]" % addr[0] if family == socket.AF_INET6 else addr[0]
        return "%s://%s:%d" % (u.scheme, host, port)
    sys.exit("cannot reach " + server)


def add_plate(g):
    """Also save the picture as it was before its people were repainted
    (node "plate"): the same finishing as the picture, from the empty
    room.  The cast pass fills from it where a replaced person stood."""
    after = set()
    grew = True
    while grew:
        grew = False
        for name, node in g.items():
            if name in after:
                continue
            for v in node["inputs"].values():
                if isinstance(v, list) and v and (
                        v[0] == "repaint" or v[0] in after):
                    after.add(name)
                    grew = True
                    break
    for name in after:
        node = json.loads(json.dumps(g[name]))
        for key, v in node["inputs"].items():
            if isinstance(v, list) and v and v[0] == "repaint":
                node["inputs"][key] = ["decode", 0]
            elif isinstance(v, list) and v and v[0] in after:
                node["inputs"][key] = [v[0] + "_plate", v[1]]
        if name == "save":
            node["inputs"]["filename_prefix"] += "-plate"
        g[name + "_plate"] = node
    g["plate"] = g.pop("save_plate")


def place_piece(g, n, piece, picture, px, py, pw, ph):
    """Put piece n on the picture; with a "mask" (a shape drawn white on
    black), only that shape of it, so a round button shows no corners."""
    g["pplace%d" % n] = {"class_type": "ImageCompositeMasked", "inputs": {
        "destination": picture, "source": ["presize%d" % n, 0],
        "x": px, "y": py, "resize_source": False}}
    if piece.get("uploaded_mask"):
        g["pmask%d" % n] = {"class_type": "LoadImage", "inputs": {
            "image": piece["uploaded_mask"]}}
        g["pmscale%d" % n] = {"class_type": "ImageScale", "inputs": {
            "image": ["pmask%d" % n, 0], "upscale_method": "bilinear",
            "width": pw, "height": ph, "crop": "disabled"}}
        g["pmconv%d" % n] = {"class_type": "ImageToMask", "inputs": {
            "image": ["pmscale%d" % n, 0], "channel": "red"}}
        g["pplace%d" % n]["inputs"]["mask"] = ["pmconv%d" % n, 0]


def cast_workflow(entry, spec, profile, seed, scene, ref, prefix):
    """Add one character to the scene, first step: crop the scene around
    their spot and let the edit model put the person of their portrait
    in the middle of the crop; the edited crop is the result.  The edit
    model redraws the whole crop, so matte_workflow() then pastes only the
    person.  Two jobs, as the matte does not fit next to the edit model
    on framework2's shared-memory GPU."""
    p = EDIT_PROFILES[profile]
    k = spec["pack"]["art_height"] / SCREEN_H
    x, y, w, h = (round(v * k) for v in entry["rect"])
    who = characters("{%s}" % entry["character"], spec) \
        if entry.get("character") else ""
    if ref is not None:
        rx, ry, rw, rh = (round(v * k) for v in entry["ref_crop"])
        who = ("the person from image 2, %s: the very same person, with "
               "the same face, hair, figure and clothes as in image 2" % who)
        if entry.get("ref_face"):
            who += (" (image 3 is a close-up of the same person's face: "
                    "keep exactly that face and expression)")
    if entry.get("instruction"):
        # A change to what is there (the cabin's maps taken away).
        prompt = (
            "Image 1 is a part of %s. %s Keep everything else of image 1 "
            "exactly as it is, with the same light and colours. "
            "Photograph, realistic." % (
                entry.get("scene", "the scene"),
                " ".join(entry["instruction"].split())))
    elif entry.get("thing"):
        # An object, not a person (the cabin's maps).
        prompt = (
            "Image 1 is a part of %s. Add to image 1 %s, in the middle of "
            "image 1, at the right scale. Add nothing else. Keep "
            "everything else of image 1 exactly as it is, with the same "
            "light and colours. Photograph, realistic." % (
                entry.get("scene", "the scene"),
                " ".join(entry["thing"].split())))
    elif entry.get("mode") == "replace":
        # The scene already has someone there, at the right size and
        # place: give them this character's looks.
        prompt = (
            "Image 1 is a part of %s with one person in it. Replace that "
            "person with %s, %s. Keep the same pose, position and size as "
            "the person in image 1. Keep everything else of image 1 "
            "exactly as it is, with the same light and colours. "
            "Photograph, realistic." % (
                entry.get("scene", "the scene"), who,
                entry["action"].strip()))
    else:
        prompt = (
            "Image 1 is a part of %s. Add to image 1 exactly one person, "
            "%s, %s, in the middle of image 1, at the right scale for the "
            "room. Add nobody else. Keep everything else of image 1 "
            "exactly as it is, with the same light and colours. "
            "Photograph, realistic." % (
                entry.get("scene", "the scene"), who,
                entry["action"].strip()))
    g = {
        "unet": {"class_type": "UNETLoader", "inputs": {
            "unet_name": MODELS["edit"], "weight_dtype": "default"}},
        "clip": {"class_type": "CLIPLoader", "inputs": {
            "clip_name": MODELS["clip"], "type": "qwen_image"}},
        "vae": {"class_type": "VAELoader", "inputs": {
            "vae_name": MODELS["vae"]}},
        "shift": {"class_type": "ModelSamplingAuraFlow", "inputs": {
            "model": ["unet", 0], "shift": SHIFT}},
        "norm": {"class_type": "CFGNorm", "inputs": {
            "model": ["shift", 0], "strength": 1.0}},
        "scene": {"class_type": "LoadImage", "inputs": {"image": scene}},
        "crop": {"class_type": "ImageCrop", "inputs": {
            "image": ["scene", 0], "width": w, "height": h, "x": x,
            "y": y}},
        "scale": {"class_type": "FluxKontextImageScale", "inputs": {
            "image": ["crop", 0]}},
        "enc": {"class_type": "VAEEncodeTiled", "inputs": {
            "pixels": ["scale", 0], "vae": ["vae", 0], "tile_size": 512,
            "overlap": 64, "temporal_size": 64, "temporal_overlap": 8}},
        "pos": {"class_type": "TextEncodeQwenImageEditPlus", "inputs": {
            "clip": ["clip", 0], "prompt": prompt, "vae": ["vae", 0],
            "image1": ["scale", 0]}},
        "neg": {"class_type": "TextEncodeQwenImageEditPlus", "inputs": {
            "clip": ["clip", 0], "prompt": "", "vae": ["vae", 0],
            "image1": ["scale", 0]}},
        "posm": {"class_type": "FluxKontextMultiReferenceLatentMethod",
                 "inputs": {"conditioning": ["pos", 0],
                            "reference_latents_method":
                            "index_timestep_zero"}},
        "negm": {"class_type": "FluxKontextMultiReferenceLatentMethod",
                 "inputs": {"conditioning": ["neg", 0],
                            "reference_latents_method":
                            "index_timestep_zero"}},
        "decode": {"class_type": "VAEDecodeTiled", "inputs": {
            "samples": ["sample", 0], "vae": ["vae", 0], "tile_size": 512,
            "overlap": 64, "temporal_size": 64, "temporal_overlap": 8}},
        "back": {"class_type": "ImageScale", "inputs": {
            "image": ["decode", 0], "upscale_method": "lanczos",
            "width": w, "height": h, "crop": "center"}},
        "save": {"class_type": "SaveImage", "inputs": {
            "images": ["back", 0], "filename_prefix": prefix}},
    }
    if ref is not None:
        g["ref"] = {"class_type": "LoadImage", "inputs": {"image": ref}}
        g["person"] = {"class_type": "ImageCrop", "inputs": {
            "image": ["ref", 0], "width": rw, "height": rh, "x": rx,
            "y": ry}}
        for side in ("pos", "neg"):
            g[side]["inputs"]["image2"] = ["person", 0]
        if entry.get("ref_face"):
            fx, fy, fw, fh = (round(v * k) for v in entry["ref_face"])
            g["face"] = {"class_type": "ImageCrop", "inputs": {
                "image": ["ref", 0], "width": fw, "height": fh, "x": fx,
                "y": fy}}
            for side in ("pos", "neg"):
                g[side]["inputs"]["image3"] = ["face", 0]
    model = ["norm", 0]
    if p["lora"]:
        g["lora"] = {"class_type": "LoraLoaderModelOnly", "inputs": {
            "model": model, "lora_name": MODELS["edit_lightning"],
            "strength_model": 1.0}}
        model = ["lora", 0]
    g["sample"] = {"class_type": "KSampler", "inputs": {
        "model": model, "seed": seed, "steps": p["steps"], "cfg": p["cfg"],
        "sampler_name": "euler", "scheduler": "simple",
        "positive": ["posm", 0], "negative": ["negm", 0],
        "latent_image": ["enc", 0], "denoise": 1.0}}
    return g


def soften(g, name, mask, soft):
    """A mask's edge pulled in a little and blurred; returns its node."""
    g[name + "in"] = {"class_type": "GrowMask", "inputs": {
        "mask": mask, "expand": -soft, "tapered_corners": True}}
    g[name + "img"] = {"class_type": "MaskToImage", "inputs": {
        "mask": [name + "in", 0]}}
    g[name + "blur"] = {"class_type": "ImageBlur", "inputs": {
        "image": [name + "img", 0], "blur_radius": 2 * soft + 1,
        "sigma": float(soft)}}
    g[name] = {"class_type": "ImageToMask", "inputs": {
        "image": [name + "blur", 0], "channel": "red"}}
    return [name, 0]


def matte_workflow(entry, spec, scene, crop, prefix):
    """Add one character to the scene, second step: match the edited
    crop's colours to the scene's (the edit model drifts paler and
    bluer), cut the person out of it with a soft edge and paste only
    them onto the scene at their spot (and over the person they
    replace)."""
    k = spec["pack"]["art_height"] / SCREEN_H
    x, y, w, h = (round(v * k) for v in entry["rect"])
    soft = max(1, round(k))
    g = {
        "scene": {"class_type": "LoadImage", "inputs": {"image": scene}},
        "crop": {"class_type": "LoadImage", "inputs": {"image": crop}},
        "old": {"class_type": "ImageCrop", "inputs": {
            "image": ["scene", 0], "width": w, "height": h, "x": x, "y": y}},
        "match": {"class_type": "ColorTransfer", "inputs": {
            "image_target": ["crop", 0], "image_ref": ["old", 0],
            "method": "reinhard_lab", "source_stats": "per_frame",
            "strength": 1.0}},
        "matter": {"class_type": "LoadBackgroundRemovalModel", "inputs": {
            "bg_removal_name": MODELS["matte"]}},
        "matte": {"class_type": "RemoveBackground", "inputs": {
            "bg_removal_model": ["matter", 0], "image": ["crop", 0]}},
        # Solid wherever the matte is fairly sure: it is unsure of dark
        # clothes on a dark floor, which then let the room show through.
        "solid": {"class_type": "ThresholdMask", "inputs": {
            "mask": ["matte", 0], "value": 0.2}},
        # Tight on the person: a pixel in, then softened, so no band of
        # the edit's own background comes along.
        "shrink": {"class_type": "GrowMask", "inputs": {
            "mask": ["solid", 0], "expand": -soft,
            "tapered_corners": True}},
        "mimage": {"class_type": "MaskToImage", "inputs": {
            "mask": ["shrink", 0]}},
        "mblur": {"class_type": "ImageBlur", "inputs": {
            "image": ["mimage", 0], "blur_radius": 2 * soft + 1,
            "sigma": float(soft)}},
        "edge": {"class_type": "ImageToMask", "inputs": {
            "image": ["mblur", 0], "channel": "red"}},
        "paste": {"class_type": "ImageCompositeMasked", "inputs": {
            "destination": ["scene", 0], "source": ["match", 0],
            "x": x, "y": y, "resize_source": False, "mask": ["edge", 0]}},
        "save": {"class_type": "SaveImage", "inputs": {
            "images": ["paste", 0], "filename_prefix": prefix}},
    }
    if entry.get("uploaded_shape"):
        # Where the person being replaced was painted (white on black, at
        # the generation size): paste over that shape, so none of them
        # shows, and take the new person only near it ("reach" pixels,
        # 640x480), so nobody else from the edited crop comes along.
        g["shape"] = {"class_type": "LoadImage", "inputs": {
            "image": entry["uploaded_shape"]}}
        g["sfit"] = {"class_type": "ImageScale", "inputs": {
            "image": ["shape", 0], "upscale_method": "bilinear",
            "width": round(SCREEN_W * k), "height": round(SCREEN_H * k),
            "crop": "center"}}
        g["scrop"] = {"class_type": "ImageCrop", "inputs": {
            "image": ["sfit", 0], "width": w, "height": h, "x": x, "y": y}}
        g["smask"] = {"class_type": "ImageToMask", "inputs": {
            "image": ["scrop", 0], "channel": "red"}}
        g["near"] = {"class_type": "GrowMask", "inputs": {
            "mask": ["smask", 0], "expand": round(entry.get("reach", 8) * k),
            "tapered_corners": True}}
        g["cover"] = {"class_type": "GrowMask", "inputs": {
            "mask": ["smask", 0], "expand": round(3 * k),
            "tapered_corners": True}}
        # The person: all of their shape (the matte misses dark legs on
        # a dark floor), plus what the matte finds just around it.
        g["within"] = {"class_type": "MaskComposite", "inputs": {
            "destination": ["solid", 0], "source": ["smask", 0],
            "x": 0, "y": 0, "operation": "or"}}
        g["mine"] = {"class_type": "MaskComposite", "inputs": {
            "destination": ["within", 0], "source": ["near", 0],
            "x": 0, "y": 0, "operation": "and"}}
        if entry.get("uploaded_plate"):
            # The empty room where the old person stood, then the new
            # person over it.
            g["plate"] = {"class_type": "LoadImage", "inputs": {
                "image": entry["uploaded_plate"]}}
            g["pcrop"] = {"class_type": "ImageCrop", "inputs": {
                "image": ["plate", 0], "width": w, "height": h, "x": x,
                "y": y}}
            g["under"] = {"class_type": "ImageCompositeMasked", "inputs": {
                "destination": ["scene", 0], "source": ["pcrop", 0],
                "x": x, "y": y, "resize_source": False,
                "mask": soften(g, "coveredge", ["cover", 0], soft)}}
            g["paste"]["inputs"]["destination"] = ["under", 0]
            g["shrink"]["inputs"]["mask"] = ["mine", 0]
        else:
            g["both"] = {"class_type": "MaskComposite", "inputs": {
                "destination": ["mine", 0], "source": ["cover", 0],
                "x": 0, "y": 0, "operation": "or"}}
            g["shrink"]["inputs"]["mask"] = ["both", 0]
    return g


def label(entry):
    """What a cast entry adds: its character, or the start of its thing."""
    return entry.get("character") or " ".join(entry["thing"].split()[:6])


def add_cast(args, spec, entries, scene_path, out, profile, seed, name,
             plate=None):
    """Add each character of "entries" to the picture at scene_path, in
    turn, writing the result to "out".  Returns their provenance and
    whether all went well."""
    cast = []
    if not entries:
        return cast, True
    # Another model: unload the scene's first, as both do not fit.
    free(args.server)
    current = scene_path
    plate_up = upload(args.server, plate, "flibustier-cast-plate.png") \
        if plate else None
    for n, entry in enumerate(entries):
        ref = ref_path = None
        if entry.get("ref"):
            refs = [r for r in spec["asset"] if r["id"] == entry["ref"]]
            if not refs or not os.path.exists(
                    os.path.join(args.pack, refs[0]["file"])):
                print("no portrait %s for the cast" % entry["ref"],
                      file=sys.stderr)
                return cast, False
            ref_path = os.path.join(args.pack, refs[0]["file"])
            ref = upload(args.server, ref_path, "flibustier-cast-ref.png")
        scene = upload(args.server, current, "flibustier-cast-scene.png")
        if plate_up:
            entry["uploaded_plate"] = plate_up
        if entry.get("shape"):
            entry["uploaded_shape"] = upload(
                args.server, os.path.join(HERE, entry["shape"]),
                "flibustier-shape-" + os.path.basename(entry["shape"]))
        cgraph = cast_workflow(entry, spec, profile, seed + n, scene, ref,
                               "flibustier/" + name.replace("/", "-") + "-cast")
        try:
            crop_png = render(args.server, cgraph)
            crop_path = out + ".cast.png"
            with open(crop_path, "wb") as f:
                f.write(crop_png)
            free(args.server)
            crop = upload(args.server, crop_path, "flibustier-cast-crop.png")
            os.remove(crop_path)
            png = render(args.server, matte_workflow(
                entry, spec, scene, crop, "flibustier/" +
                name.replace("/", "-") + "-matte"))
            free(args.server)
        except RuntimeError as e:
            print("cast %s failed: %s" % (label(entry), e),
                  file=sys.stderr)
            return cast, False
        os.makedirs(os.path.dirname(out), exist_ok=True)
        with open(out, "wb") as f:
            f.write(png)
        current = out
        cast.append({
            "character": label(entry), "portrait": entry.get("ref"),
            "portrait_sha256": hashlib.sha256(
                open(ref_path, "rb").read()).hexdigest()
            if ref_path else None,
            "ref_crop": entry.get("ref_crop"), "rect": entry["rect"],
            "seed": seed + n,
            "prompt": cgraph["pos"]["inputs"]["prompt"],
            "workflow_sha256": hashlib.sha256(json.dumps(
                cgraph, sort_keys=True).encode()).hexdigest()})
    return cast, True


def crop_workflow(image, rect, k, prefix):
    """Cut rect (640x480 pixels) out of an uploaded picture."""
    x, y, w, h = (round(v * k) for v in rect)
    return {
        "image": {"class_type": "LoadImage", "inputs": {"image": image}},
        "crop": {"class_type": "ImageCrop", "inputs": {
            "image": ["image", 0], "width": w, "height": h, "x": x,
            "y": y}},
        "save": {"class_type": "SaveImage", "inputs": {
            "images": ["crop", 0], "filename_prefix": prefix}},
    }


def match_workflow(image, edited, rect, k, prefix, feather=0):
    """An edited crop of rect (640x480 pixels) of an uploaded picture,
    its colours matched to that crop; with "feather" (640x480 pixels),
    faded into the crop towards its edges, so a patch drawn over the
    picture shows no seam."""
    x, y, w, h = (round(v * k) for v in rect)
    g = {
        "image": {"class_type": "LoadImage", "inputs": {"image": image}},
        "edited": {"class_type": "LoadImage", "inputs": {"image": edited}},
        "crop": {"class_type": "ImageCrop", "inputs": {
            "image": ["image", 0], "width": w, "height": h, "x": x,
            "y": y}},
        "match": {"class_type": "ColorTransfer", "inputs": {
            "image_target": ["edited", 0], "image_ref": ["crop", 0],
            "method": "reinhard_lab", "source_stats": "per_frame",
            "strength": 1.0}},
        "save": {"class_type": "SaveImage", "inputs": {
            "images": ["match", 0], "filename_prefix": prefix}},
    }
    if feather:
        f = round(feather * k)
        g.update({
            "solid": {"class_type": "SolidMask", "inputs": {
                "value": 1.0, "width": w, "height": h}},
            "fade": {"class_type": "FeatherMask", "inputs": {
                "mask": ["solid", 0], "left": f, "top": f, "right": f,
                "bottom": f}},
            "blend": {"class_type": "ImageCompositeMasked", "inputs": {
                "destination": ["crop", 0], "source": ["match", 0],
                "x": 0, "y": 0, "resize_source": False,
                "mask": ["fade", 0]}},
        })
        g["save"]["inputs"]["images"] = ["blend", 0]
    return g


def edit_paste_workflow(image, edited, mask, rect, k, prefix):
    """Paste an edited crop of rect (640x480 pixels) back onto an
    uploaded picture, colours matched, only inside "mask" (a whole-
    picture mask at the generation size)."""
    x, y, w, h = (round(v * k) for v in rect)
    g = match_workflow(image, edited, rect, k, prefix)
    g.update({
        "mask": {"class_type": "LoadImage", "inputs": {"image": mask}},
        "mfit": {"class_type": "ImageScale", "inputs": {
            "image": ["mask", 0], "upscale_method": "bilinear",
            "width": round(SCREEN_W * k), "height": round(SCREEN_H * k),
            "crop": "center"}},
        "mcrop": {"class_type": "ImageCrop", "inputs": {
            "image": ["mfit", 0], "width": w, "height": h, "x": x,
            "y": y}},
        "cmask": {"class_type": "ImageToMask", "inputs": {
            "image": ["mcrop", 0], "channel": "red"}},
        "paste": {"class_type": "ImageCompositeMasked", "inputs": {
            "destination": ["image", 0], "source": ["match", 0],
            "x": x, "y": y, "resize_source": False, "mask": ["cmask", 0]}},
    })
    g["save"]["inputs"]["images"] = ["paste", 0]
    return g


def atlas_workflow(images, sizes, prefix):
    """Lay uploaded frames side by side, left to right, top aligned."""
    g = {"canvas": {"class_type": "EmptyImage", "inputs": {
        "width": sum(w for w, _ in sizes),
        "height": max(h for _, h in sizes), "batch_size": 1, "color": 0}}}
    last, x = ["canvas", 0], 0
    for n, (image, (w, _)) in enumerate(zip(images, sizes)):
        g["frame%d" % n] = {"class_type": "LoadImage", "inputs": {
            "image": image}}
        g["put%d" % n] = {"class_type": "ImageCompositeMasked", "inputs": {
            "destination": last, "source": ["frame%d" % n, 0], "x": x,
            "y": 0, "resize_source": False}}
        last, x = ["put%d" % n, 0], x + w
    g["save"] = {"class_type": "SaveImage", "inputs": {
        "images": last, "filename_prefix": prefix}}
    return g


def repaint_workflow(spec, profile, seed, scene, mask, rect, prompt,
                     prefix):
    """Repaint the white of "mask" (a whole-picture mask at the
    generation size) inside "rect" (640x480 pixels) of an uploaded
    picture with the scene model, and return the whole picture."""
    p = PROFILES[profile]
    k = spec["pack"]["art_height"] / SCREEN_H
    x, y, w, h = (round(v * k) for v in rect)
    gw, gh = generation_size(w, h, 0.6)
    text = characters(prompt, spec).strip()
    if spec["pack"].get("style"):
        text += " " + spec["pack"]["style"].strip()
    g = {
        "unet": {"class_type": "UNETLoader", "inputs": {
            "unet_name": MODELS["unet"], "weight_dtype": "fp8_e4m3fn"}},
        "clip": {"class_type": "CLIPLoader", "inputs": {
            "clip_name": MODELS["clip"], "type": "qwen_image"}},
        "vae": {"class_type": "VAELoader", "inputs": {
            "vae_name": MODELS["vae"]}},
        "pos": {"class_type": "CLIPTextEncode", "inputs": {
            "text": text, "clip": ["clip", 0]}},
        "neg": {"class_type": "CLIPTextEncode", "inputs": {
            "text": DEFAULT_NEGATIVE, "clip": ["clip", 0]}},
        "scene": {"class_type": "LoadImage", "inputs": {"image": scene}},
        "crop": {"class_type": "ImageCrop", "inputs": {
            "image": ["scene", 0], "width": w, "height": h, "x": x, "y": y}},
        "up": {"class_type": "ImageScale", "inputs": {
            "image": ["crop", 0], "upscale_method": "lanczos",
            "width": gw, "height": gh, "crop": "disabled"}},
        "enc": {"class_type": "VAEEncodeTiled", "inputs": {
            "pixels": ["up", 0], "vae": ["vae", 0], "tile_size": 512,
            "overlap": 64, "temporal_size": 64, "temporal_overlap": 8}},
        "mask": {"class_type": "LoadImage", "inputs": {"image": mask}},
        "mfit": {"class_type": "ImageScale", "inputs": {
            "image": ["mask", 0], "upscale_method": "bilinear",
            "width": round(SCREEN_W * k), "height": round(SCREEN_H * k),
            "crop": "center"}},
        "mcrop": {"class_type": "ImageCrop", "inputs": {
            "image": ["mfit", 0], "width": w, "height": h, "x": x, "y": y}},
        "mup": {"class_type": "ImageScale", "inputs": {
            "image": ["mcrop", 0], "upscale_method": "bilinear",
            "width": gw, "height": gh, "crop": "disabled"}},
        "lmask": {"class_type": "ImageToMask", "inputs": {
            "image": ["mup", 0], "channel": "red"}},
        "cmask": {"class_type": "ImageToMask", "inputs": {
            "image": ["mcrop", 0], "channel": "red"}},
        "noise": {"class_type": "SetLatentNoiseMask", "inputs": {
            "samples": ["enc", 0], "mask": ["lmask", 0]}},
        "decode": {"class_type": "VAEDecodeTiled", "inputs": {
            "samples": ["sample", 0], "vae": ["vae", 0], "tile_size": 512,
            "overlap": 64, "temporal_size": 64, "temporal_overlap": 8}},
        "down": {"class_type": "ImageScale", "inputs": {
            "image": ["decode", 0], "upscale_method": "lanczos",
            "width": w, "height": h, "crop": "disabled"}},
        # Match the repaint's colours to its surroundings: the draft
        # model paints lighter, which shows at the mask's edge.
        "match": {"class_type": "ColorTransfer", "inputs": {
            "image_target": ["down", 0], "image_ref": ["crop", 0],
            "method": "reinhard_lab", "source_stats": "per_frame",
            "strength": 1.0}},
        "paste": {"class_type": "ImageCompositeMasked", "inputs": {
            "destination": ["scene", 0], "source": ["match", 0],
            "x": x, "y": y, "resize_source": False, "mask": ["cmask", 0]}},
        "save": {"class_type": "SaveImage", "inputs": {
            "images": ["paste", 0], "filename_prefix": prefix}},
    }
    model = ["unet", 0]
    if p["lora"]:
        g["lora"] = {"class_type": "LoraLoaderModelOnly", "inputs": {
            "model": model, "lora_name": MODELS["lightning"],
            "strength_model": 1.0}}
        model = ["lora", 0]
    g["shift"] = {"class_type": "ModelSamplingAuraFlow", "inputs": {
        "model": model, "shift": SHIFT}}
    g["sample"] = {"class_type": "KSampler", "inputs": {
        "model": ["shift", 0], "seed": seed, "steps": p["steps"],
        "cfg": p["cfg"], "sampler_name": "euler", "scheduler": "simple",
        "positive": ["pos", 0], "negative": ["neg", 0],
        "latent_image": ["noise", 0], "denoise": 1.0}}
    return g


def derive_sheet(args, spec, a, profile, seed):
    """A sprite sheet whose frames are patches of another asset's picture
    with a person added on each frame's spot (the tavern's visitors)."""
    k = spec["pack"]["art_height"] / SCREEN_H
    src = [r for r in spec["asset"] if r["id"] == a["from"]]
    if not src or not os.path.exists(os.path.join(args.pack,
                                                   src[0]["file"])):
        print("%s: no %s to derive from" % (a["id"], a["from"]),
              file=sys.stderr)
        return None
    src_path = os.path.join(args.pack, src[0]["file"])
    work = os.path.join(args.pack, a["atlas"] + ".work.png")
    frames, sizes, offsets, record = [], [], [], []
    for n, fr in enumerate(a["frames"]):
        # Cut from the picture, or from it before its last touches.
        base = os.path.join(args.pack, src[0]["before_finish"]) \
            if fr.get("before_finish") else src_path
        picture = base
        cast = []
        start = base
        plate = None
        steps = fr.get("repaint", [])
        if steps:
            # Paint, in turn, what the person sits at and then someone of
            # the right size on the spot, for the cast pass to give their
            # looks; it fills from the picture before that someone.
            free(args.server)
        for m, step in enumerate(steps):
            plate = start
            up = upload(args.server, start, "flibustier-derive-source.png")
            mask = upload(args.server, os.path.join(HERE, step["mask"]),
                          "flibustier-repaint-" + os.path.basename(step["mask"]))
            try:
                png = render(args.server, repaint_workflow(
                    spec, profile, seed + 300 + 10 * n + m, up, mask,
                    step["rect"], step["prompt"],
                    "flibustier/" + a["id"].replace("/", "-") + "-repaint"))
            except RuntimeError as e:
                print("%s: repaint failed: %s" % (a["id"], e),
                      file=sys.stderr)
                return None
            step_path = work + ".%d.png" % m
            with open(step_path, "wb") as f:
                f.write(png)
            start = step_path
        if fr.get("character"):
            entry = dict(fr)
            cast, ok = add_cast(args, spec, [entry], start, work, profile,
                                seed + 200 + n, a["id"], plate)
            if not ok:
                return None
            picture = work
        else:
            picture = start		# repainted only, or as it is
        if fr.get("add"):
            # Objects put into the frame, each cut out, so the rest of
            # it stays the picture's.
            added, ok = add_cast(args, spec, [dict(t) for t in fr["add"]],
                                 picture, work, profile,
                                 seed + 250 + 10 * n, a["id"])
            if not ok:
                return None
            cast += added
            picture = work
        up = upload(args.server, picture, "flibustier-derive-source.png")
        if fr.get("instruction"):
            # The frame is the edit model's change of that part of the
            # picture, its colours matched back to it.
            free(args.server)
            try:
                png = render(args.server, cast_workflow(
                    fr, spec, profile, seed + 200 + n, up, None,
                    "flibustier/" + a["id"].replace("/", "-") + "-edit"))
                edited = os.path.join(args.pack, a["atlas"] + ".edit.png")
                with open(edited, "wb") as f:
                    f.write(png)
                free(args.server)
                eup = upload(args.server, edited, "flibustier-edit.png")
                os.remove(edited)
                png = render(args.server, match_workflow(
                    up, eup, fr["rect"], k,
                    "flibustier/" + a["id"].replace("/", "-"),
                    fr.get("feather", 0)))
            except RuntimeError as e:
                print("%s: edit failed: %s" % (a["id"], e), file=sys.stderr)
                return None
        else:
            png = render(args.server, crop_workflow(
                up, fr["rect"], k, "flibustier/" + a["id"].replace("/", "-")))
        frame = os.path.join(args.pack, a["atlas"] + ".%d.png" % n)
        with open(frame, "wb") as f:
            f.write(png)
        frames.append(upload(args.server, frame,
                             "flibustier-frame-%d.png" % n))
        os.remove(frame)
        sizes.append(tuple(round(v * k) for v in fr["rect"][2:]))
        # Drawn by the game at "at" (640x480), the frame's top-left when
        # it covers more than the original frame's place.
        at = fr.get("at", fr["rect"][:2])
        offsets.append(tuple(round((v - w) * k) for v, w in
                             zip(fr["rect"][:2], at)))
        record.append({"rect": fr["rect"], "at": fr.get("at"),
                       "cast": cast or None})
    for left in [work] + [work + ".%d.png" % m for m in range(4)]:
        if os.path.exists(left):
            os.remove(left)
    atlas = render(args.server, atlas_workflow(
        frames, sizes, "flibustier/" + a["id"].replace("/", "-") + "-atlas"))
    with open(os.path.join(args.pack, a["atlas"]), "wb") as f:
        f.write(atlas)
    lines = ["# Generated by tools/artgen/artgen.py from %s." % a["from"],
             "[sheet]", "image = " + os.path.basename(a["atlas"]), "",
             "[frames]"]
    x = 0
    for n, ((w, h), (ox, oy)) in enumerate(zip(sizes, offsets)):
        lines.append("%d = %d 0 %d %d" % (n, x, w, h) +
                     (" %d %d" % (ox, oy) if ox or oy else ""))
        x += w
    with open(os.path.join(args.pack, a["file"]), "w") as f:
        f.write("\n".join(lines) + "\n")
    return record


def free(server):
    """Let ComfyUI unload its models: two big ones do not fit together."""
    call(server, "/free", json.dumps({"unload_models": True,
                                      "free_memory": True}).encode())


def call(server, path, data=None):
    req = urllib.request.Request(server + path, data=data, headers={
        "Content-Type": "application/json"} if data else {})
    with urllib.request.urlopen(req, timeout=60) as r:
        return r.read()


def render(server, graph, nodes=None):
    """Queue "graph", wait for it, return the saved image's bytes; with
    "nodes", a dict of the images those SaveImage nodes saved."""
    client = str(uuid.uuid4())
    reply = json.loads(call(server, "/prompt", json.dumps(
        {"prompt": graph, "client_id": client}).encode()))
    if reply.get("node_errors"):
        raise RuntimeError(json.dumps(reply["node_errors"]))
    pid = reply["prompt_id"]
    while True:
        hist = json.loads(call(server, "/history/" + pid))
        if pid in hist:
            entry = hist[pid]
            status = entry.get("status", {})
            if status.get("status_str") == "error":
                for kind, msg in status.get("messages", []):
                    if kind == "execution_error":
                        raise RuntimeError("%s: %s" % (
                            msg.get("node_type"),
                            msg.get("exception_message", "").split("\n")[0]))
                raise RuntimeError("generation failed")
            got = {}
            for node, out in entry.get("outputs", {}).items():
                for img in out.get("images", [])[:1]:
                    q = urllib.parse.urlencode({
                        "filename": img["filename"],
                        "subfolder": img.get("subfolder", ""),
                        "type": img.get("type", "output")})
                    got[node] = call(server, "/view?" + q)
            if nodes:
                if any(n not in got for n in nodes):
                    raise RuntimeError("no image in the output")
                return {n: got[n] for n in nodes}
            if not got:
                raise RuntimeError("no image in the output")
            return got.get("save", next(iter(got.values())))
        time.sleep(2)


def upload(server, path, name):
    """Put a local picture into ComfyUI's input folder as "name"."""
    boundary = uuid.uuid4().hex
    with open(path, "rb") as f:
        data = f.read()
    body = b"".join([
        b"--" + boundary.encode() + b"\r\n",
        b'Content-Disposition: form-data; name="image"; filename="' +
        name.encode() + b'"\r\n',
        b"Content-Type: image/png\r\n\r\n", data, b"\r\n",
        b"--" + boundary.encode() + b"\r\n",
        b'Content-Disposition: form-data; name="overwrite"\r\n\r\n',
        b"true\r\n",
        b"--" + boundary.encode() + b"--\r\n"])
    req = urllib.request.Request(server + "/upload/image", data=body,
                                 headers={"Content-Type":
                                          "multipart/form-data; boundary=" +
                                          boundary})
    with urllib.request.urlopen(req, timeout=60) as r:
        return json.loads(r.read())["name"]


def git_rev():
    try:
        return subprocess.run(["git", "-C", HERE, "rev-parse", "--short",
                               "HEAD"], capture_output=True, text=True,
                              check=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return "unknown"


def ship_frame_workflow(spec, profile, seed, render, prompt, denoise, out_w,
                        out_h, prefix, size=None):
    """One rendered heading of a ship model made a photograph: laid on a
    plain sea blue, redrawn at "denoise" with the render's own edges
    holding its shape, cut out again and brought to the frame's size,
    with its alpha."""
    p = PROFILES[profile]
    text = prompt.strip()
    if spec["pack"].get("style"):
        text += " " + spec["pack"]["style"].strip()
    g = {
        "unet": {"class_type": "UNETLoader", "inputs": {
            "unet_name": MODELS["unet"], "weight_dtype": "fp8_e4m3fn"}},
        "clip": {"class_type": "CLIPLoader", "inputs": {
            "clip_name": MODELS["clip"], "type": "qwen_image"}},
        "vae": {"class_type": "VAELoader", "inputs": {
            "vae_name": MODELS["vae"]}},
        "pos": {"class_type": "CLIPTextEncode", "inputs": {
            "text": text, "clip": ["clip", 0]}},
        "neg": {"class_type": "CLIPTextEncode", "inputs": {
            "text": DEFAULT_NEGATIVE, "clip": ["clip", 0]}},
        "render": {"class_type": "LoadImage", "inputs": {"image": render}},
        "shape": {"class_type": "InvertMask", "inputs": {
            "mask": ["render", 1]}},
        "sea": {"class_type": "EmptyImage", "inputs": {
            "width": (size or SHIP_RENDER)[0],
            "height": (size or SHIP_RENDER)[1],
            "batch_size": 1, "color": 0x2A5A7A}},
    }
    g.update({
        "onsea": {"class_type": "ImageCompositeMasked", "inputs": {
            "destination": ["sea", 0], "source": ["render", 0],
            "x": 0, "y": 0, "resize_source": False, "mask": ["shape", 0]}},
        "edges": {"class_type": "Canny", "inputs": {
            "image": ["onsea", 0], "low_threshold": 0.2,
            "high_threshold": 0.5}},
        "enc": {"class_type": "VAEEncodeTiled", "inputs": {
            "pixels": ["onsea", 0], "vae": ["vae", 0], "tile_size": 512,
            "overlap": 64, "temporal_size": 64, "temporal_overlap": 8}},
        "cpatch": {"class_type": "ModelPatchLoader", "inputs": {
            "name": MODELS["controlnet"]}},
        "control": {"class_type": "QwenImageDiffsynthControlnet",
                    "inputs": {
                        "model": ["unet", 0], "model_patch": ["cpatch", 0],
                        "vae": ["vae", 0], "image": ["edges", 0],
                        "strength": 0.9, "start_percent": 0.0,
                        "end_percent": 0.8}},
        "decode": {"class_type": "VAEDecodeTiled", "inputs": {
            "samples": ["sample", 0], "vae": ["vae", 0], "tile_size": 512,
            "overlap": 64, "temporal_size": 64, "temporal_overlap": 8}},
        "save": {"class_type": "SaveImage", "inputs": {
            "images": ["decode", 0], "filename_prefix": prefix}},
    })
    model = ["control", 0]
    if p["lora"]:
        g["lora"] = {"class_type": "LoraLoaderModelOnly", "inputs": {
            "model": model, "lora_name": MODELS["lightning"],
            "strength_model": 1.0}}
        model = ["lora", 0]
    g["shift"] = {"class_type": "ModelSamplingAuraFlow", "inputs": {
        "model": model, "shift": SHIFT}}
    g["sample"] = {"class_type": "KSampler", "inputs": {
        "model": ["shift", 0], "seed": seed, "steps": p["steps"],
        "cfg": p["cfg"], "sampler_name": "euler", "scheduler": "simple",
        "positive": ["pos", 0], "negative": ["neg", 0],
        "latent_image": ["enc", 0], "denoise": denoise}}
    return g


def cutout_workflow(image, out_w, out_h, prefix):
    """The ship of a redrawn heading cut out of its sea and brought to the
    frame's size, with its alpha (a job of its own: the matte does not
    fit beside the scene model)."""
    return {
        "image": {"class_type": "LoadImage", "inputs": {"image": image}},
        "matter": {"class_type": "LoadBackgroundRemovalModel", "inputs": {
            "bg_removal_name": MODELS["matte"]}},
        "matte": {"class_type": "RemoveBackground", "inputs": {
            "bg_removal_model": ["matter", 0], "image": ["image", 0]}},
        "small": {"class_type": "ImageScale", "inputs": {
            "image": ["image", 0], "upscale_method": "lanczos",
            "width": out_w, "height": out_h, "crop": "disabled"}},
        "mimg": {"class_type": "MaskToImage", "inputs": {
            "mask": ["matte", 0]}},
        "msmall": {"class_type": "ImageScale", "inputs": {
            "image": ["mimg", 0], "upscale_method": "bilinear",
            "width": out_w, "height": out_h, "crop": "disabled"}},
        "alpha": {"class_type": "ImageToMask", "inputs": {
            "image": ["msmall", 0], "channel": "red"}},
        # JoinImageWithAlpha takes a mask (1 = clear), as LoadImage gives.
        "clear": {"class_type": "InvertMask", "inputs": {
            "mask": ["alpha", 0]}},
        "rgba": {"class_type": "JoinImageWithAlpha", "inputs": {
            "image": ["small", 0], "alpha": ["clear", 0]}},
        "save": {"class_type": "SaveImage", "inputs": {
            "images": ["rgba", 0], "filename_prefix": prefix}},
    }


def rgba_atlas_workflow(images, sizes, prefix):
    """Frames with alpha side by side, left to right, top aligned, the
    atlas keeping their alpha."""
    tw, th = sum(w for w, _ in sizes), max(h for _, h in sizes)
    g = {"canvas": {"class_type": "EmptyImage", "inputs": {
        "width": tw, "height": th, "batch_size": 1, "color": 0}},
        "clear": {"class_type": "SolidMask", "inputs": {
            "value": 1.0, "width": tw, "height": th}}}
    img, msk, x = ["canvas", 0], ["clear", 0], 0
    for n, (image, (w, _)) in enumerate(zip(images, sizes)):
        g["f%d" % n] = {"class_type": "LoadImage", "inputs": {
            "image": image}}
        g["p%d" % n] = {"class_type": "ImageCompositeMasked", "inputs": {
            "destination": img, "source": ["f%d" % n, 0], "x": x, "y": 0,
            "resize_source": False}}
        # Masks are 1 where clear: the frame's over the clear canvas.
        g["m%d" % n] = {"class_type": "MaskComposite", "inputs": {
            "destination": msk, "source": ["f%d" % n, 1], "x": x, "y": 0,
            "operation": "multiply"}}
        img, msk, x = ["p%d" % n, 0], ["m%d" % n, 0], x + w
    g["rgba"] = {"class_type": "JoinImageWithAlpha", "inputs": {
        "image": img, "alpha": msk}}
    g["save"] = {"class_type": "SaveImage", "inputs": {
        "images": ["rgba", 0], "filename_prefix": prefix}}
    return g


def write_sheet(path, atlas, sizes, comment):
    """A .sheet naming "atlas" and frames side by side of "sizes"."""
    lines = ["# " + comment, "[sheet]", "image = " + atlas, "", "[frames]"]
    x = 0
    for n, (w, h) in enumerate(sizes):
        lines.append("%d = %d 0 %d %d" % (n, x, w, h))
        x += w
    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")


def bow_words(k, heads=16):
    """Which way the ship of heading k (clockwise from north, seen from
    the south) points in the picture: the redraw otherwise takes a big
    lateen sail at the stern for the jibs and turns the ship round."""
    names = ["away from us, straight up the picture",
             "up and to the right", "to the upper right",
             "to the right and a little up", "to the right",
             "to the right and a little down", "to the lower right",
             "down and to the right", "towards us, straight down the picture",
             "down and to the left", "to the lower left",
             "to the left and a little down", "to the left",
             "to the left and a little up", "to the upper left",
             "up and to the left"]
    where = names[round(k * 16 / heads) % 16]
    return ("The ship sails %s: its bow, bowsprit and jibs point %s, its "
            "stern and stern windows are at the opposite end." % (
                where, where))


def fetch_library(host):
    """The models' photographic materials on the render host (CC0, from
    ambientCG and Poly Haven; materials.py), fetched once."""
    import materials
    subprocess.run(["ssh", "-o", "BatchMode=yes", host, "bash -s"],
                   input=materials.fetch_command().encode(), check=True)


def redrawn(spec, a):
    """Whether an asset's Blender renders are redrawn by the image model
    (the pack's "redraw_renders", or the asset's own "redraw"): the
    redraw blurs small sprites and changes what the model drew, so the
    pack keeps the renders as they are."""
    return a.get("redraw", spec["pack"].get("redraw_renders", True))


def make_ship(args, spec, a, profile, seed):
    """A ship's headings: its model rendered by Blender on the render
    host, each heading made a photograph and cut out, laid in one atlas
    with its .sheet."""
    k = spec["pack"]["art_height"] / SCREEN_H
    side = "damage" in a
    sh = a["damage"] if side else a["ship"]
    heads = sh.get("levels", 5) if side else sh.get("headings", 16)
    fw, fh = (round(v * k) for v in sh.get("frame", [78, 58]))
    size = DAMAGE_RENDER if side else SHIP_RENDER
    what = ("--class '%s' --damage %d" % (sh["class"], heads)) if side \
        else ("--size %d --headings %d" % (sh["size"], heads))
    name = (lambda n: "d%d.png" % n) if side else (lambda n: "%d.png" % n)
    prompts = [a["prompt"].strip() + " " + " ".join(
        spec["damage"]["levels"][n].split()) for n in range(heads)] \
        if side else [a["prompt"].strip() + " " + bow_words(n, heads)
                      for n in range(heads)]
    host = args.render_host
    work = "flibustier-ships/%s" % a["id"].replace("/", "-")
    local = os.path.join(args.pack, a["atlas"] + ".frames")
    script = os.path.join(HERE, "ships", "ship.py")
    try:
        subprocess.run(["ssh", "-o", "BatchMode=yes", host,
                        "mkdir -p %s" % work], check=True,
                       stdin=subprocess.DEVNULL)
        fetch_library(host)
        subprocess.run(["scp", "-q", script,
                        os.path.join(HERE, "materials.py"),
                        "%s:%s/" % (host, work)], check=True)
        subprocess.run(["ssh", "-o", "BatchMode=yes", host,
                        "cd %s && rm -rf out && blender -b -P ship.py -- "
                        "%s --out out --frame %dx%d --blend model.blend "
                        "> blender.log 2>&1" % (work, what, size[0],
                                                 size[1])],
                       check=True, stdin=subprocess.DEVNULL)
        os.makedirs(local, exist_ok=True)
        subprocess.run(["scp", "-q", "%s:%s/out/*.png" % (host, work),
                        local + "/"], check=True)
        # The model itself, beside its provenance, to open in Blender.
        mdir = os.path.join(args.pack, "provenance", "ships")
        os.makedirs(mdir, exist_ok=True)
        subprocess.run(["scp", "-q", "%s:%s/model.blend" % (host, work),
                        os.path.join(mdir, a["id"].replace("/", "-") +
                                     ".blend")], check=True)
    except subprocess.CalledProcessError as e:
        print("%s: blender on %s failed: %s" % (a["id"], host, e),
              file=sys.stderr)
        return None
    free(args.server)
    prefix = "flibustier/" + a["id"].replace("/", "-")
    redraw = redrawn(spec, a)
    try:
        if not redraw:
            # The renders as they are, at the frame's size.
            frames = []
            for n in range(heads):
                path = os.path.join(local, "frame-%d.png" % n)
                subprocess.run(["magick", os.path.join(local, name(n)),
                                "-filter", "Lanczos", "-resize",
                                "%dx%d!" % (fw, fh), "-depth", "8", path],
                               check=True)
                frames.append(upload(args.server, path,
                                     "flibustier-ship-frame-%d.png" % n))
        # Every heading redrawn first, the scene model loaded once; one
        # seed for all, so they look alike.
        for n in range(heads if redraw else 0):
            up = upload(args.server, os.path.join(local, name(n)),
                        "flibustier-ship-render.png")
            # A damaged ship redrawn more lightly: at full strength the
            # model puts back the masts the damage took away.
            dn = a.get("denoise", 0.75)
            if side and n >= 2:
                dn = a.get("damage_denoise", spec["damage"].get("denoise",
                                                                dn))
            png = render(args.server, ship_frame_workflow(
                spec, profile, seed, up, prompts[n], dn, fw, fh, prefix,
                size))
            with open(os.path.join(local, "photo-%d.png" % n), "wb") as f:
                f.write(png)
        if redraw:
            free(args.server)
            frames = []
        for n in range(heads if redraw else 0):
            up = upload(args.server, os.path.join(local, "photo-%d.png" % n),
                        "flibustier-ship-photo.png")
            path = os.path.join(local, "frame-%d.png" % n)
            with open(path, "wb") as f:
                f.write(render(args.server, cutout_workflow(
                    up, fw, fh, prefix + "-cut")))
            frames.append(upload(args.server, path,
                                 "flibustier-ship-frame-%d.png" % n))
    except RuntimeError as e:
        print("%s: %s" % (a["id"], e), file=sys.stderr)
        return None
    sizes = [(fw, fh)] * heads
    atlas = render(args.server, rgba_atlas_workflow(
        frames, sizes, "flibustier/" + a["id"].replace("/", "-") +
        "-atlas"))
    with open(os.path.join(args.pack, a["atlas"]), "wb") as f:
        f.write(atlas)
    write_sheet(os.path.join(args.pack, a["file"]),
                os.path.basename(a["atlas"]), sizes,
                "Generated by tools/artgen/artgen.py from a ship model.")
    shutil.rmtree(local)
    return {"frames": heads, "frame": [fw, fh],
            "size": sh.get("size"), "class": sh.get("class"),
            "prompts": prompts if redraw else None, "redrawn": redraw,
            "blend": "provenance/ships/%s.blend" % a["id"].replace("/", "-"),
            "denoise": a.get("denoise", 0.75) if redraw else None,
            "model_sha256": hashlib.sha256(
                open(script, "rb").read()).hexdigest()}


def texture_workflow(spec, profile, seed, prompt, side, prefix,
                     styled=True):
    """A square photographic texture from a prompt (in the pack's style
    if "styled")."""
    p = PROFILES[profile]
    text = prompt.strip()
    if styled and spec["pack"].get("style"):
        text += " " + spec["pack"]["style"].strip()
    g = {
        "unet": {"class_type": "UNETLoader", "inputs": {
            "unet_name": MODELS["unet"], "weight_dtype": "fp8_e4m3fn"}},
        "clip": {"class_type": "CLIPLoader", "inputs": {
            "clip_name": MODELS["clip"], "type": "qwen_image"}},
        "vae": {"class_type": "VAELoader", "inputs": {
            "vae_name": MODELS["vae"]}},
        "pos": {"class_type": "CLIPTextEncode", "inputs": {
            "text": text, "clip": ["clip", 0]}},
        "neg": {"class_type": "CLIPTextEncode", "inputs": {
            "text": DEFAULT_NEGATIVE, "clip": ["clip", 0]}},
        "latent": {"class_type": "EmptySD3LatentImage", "inputs": {
            "width": 1328, "height": 1328, "batch_size": 1}},
        "decode": {"class_type": "VAEDecodeTiled", "inputs": {
            "samples": ["sample", 0], "vae": ["vae", 0], "tile_size": 512,
            "overlap": 64, "temporal_size": 64, "temporal_overlap": 8}},
        "small": {"class_type": "ImageScale", "inputs": {
            "image": ["decode", 0], "upscale_method": "lanczos",
            "width": side, "height": side, "crop": "center"}},
        "save": {"class_type": "SaveImage", "inputs": {
            "images": ["small", 0], "filename_prefix": prefix}},
    }
    model = ["unet", 0]
    if p["lora"]:
        g["lora"] = {"class_type": "LoraLoaderModelOnly", "inputs": {
            "model": model, "lora_name": MODELS["lightning"],
            "strength_model": 1.0}}
        model = ["lora", 0]
    g["shift"] = {"class_type": "ModelSamplingAuraFlow", "inputs": {
        "model": model, "shift": SHIFT}}
    g["sample"] = {"class_type": "KSampler", "inputs": {
        "model": ["shift", 0], "seed": seed, "steps": p["steps"],
        "cfg": p["cfg"], "sampler_name": "euler", "scheduler": "simple",
        "positive": ["pos", 0], "negative": ["neg", 0],
        "latent_image": ["latent", 0], "denoise": 1.0}}
    return g


def make_tiles(args, spec, a, profile, seed):
    """The sailing map's tiles: photographic textures laid through the
    coastline of each tile they replace (tilemaker.py)."""
    import tilemaker
    terrain = os.path.join(os.path.expanduser(args.world),
                           a["tiles"].get("terrain", "map-terrain.pgm"))
    if not os.path.exists(terrain):
        print("%s: no %s: run flibustier --export-world %s first" % (
            a["id"], terrain, args.world), file=sys.stderr)
        return None
    k = spec["pack"]["art_height"] / SCREEN_H
    t = a["tiles"]
    side = round(t.get("tile", 16) * k)
    prefix = "flibustier/" + a["id"].replace("/", "-")
    free(args.server)
    textures, record = {}, []
    try:
        for n, tex in enumerate(a["textures"]):
            png = render(args.server, texture_workflow(
                spec, profile, seed + n, tex["prompt"],
                tex.get("size", 1024), prefix + "-" + tex["name"]))
            if tex.get("cutout"):
                path = os.path.join(args.pack, a["atlas"] + ".tex.png")
                with open(path, "wb") as f:
                    f.write(png)
                free(args.server)
                up = upload(args.server, path, "flibustier-texture.png")
                os.remove(path)
                size = tex.get("size", 1024)
                png = render(args.server, cutout_workflow(
                    up, size, size, prefix + "-cut"))
                free(args.server)
            textures[tex["name"]] = png
            record.append({"name": tex["name"], "seed": seed + n,
                           "prompt": tex["prompt"].strip(),
                           "colour": tex.get("colour"),
                           "detail": tex.get("detail", 0.8),
                           "cutout": bool(tex.get("cutout"))})
    except RuntimeError as e:
        print("%s: %s" % (a["id"], e), file=sys.stderr)
        return None
    looks = {tex["name"]: (tuple(int(tex["colour"][i:i + 2], 16)
                                 for i in (1, 3, 5))
                           if tex.get("colour") else None,
                           tex.get("detail", 0.8))
             for tex in a["textures"]}
    frames = t.get("frames", 1)
    # The cut-out textures and the tiles they stand on: "towns" the
    # houses, "place" any others by texture.
    placed = {"town": set(t.get("towns", []))}
    for name, tiles in t.get("place", {}).items():
        placed[name] = set(tiles)
    # As many tiles a row as keep the sheet about square: 16 for one
    # frame, 64 for eight (a PNG decoder may refuse a side past 8192).
    columns = 16 * 2 ** round(math.log2(math.sqrt(frames))) \
        if frames > 1 else 16
    columns = min(256, columns)
    shallow = t.get("shallows")
    atlas = tilemaker.compose(terrain, textures, side, placed, seed, looks,
                              frames=frames, columns=columns,
                              shallow_colour=tuple(int(shallow[i:i + 2], 16)
                                                   for i in (1, 3, 5))
                              if shallow else None,
                              crests_on=t.get("crests", True),
                              flatten=t.get("flatten", 12),
                              natural=t.get("natural", False))
    tilemaker.png_write(os.path.join(args.pack, a["atlas"]), columns * side,
                        -(-256 // columns) * side * frames, atlas)
    write_tiles(args.pack, a, side, frames, t.get("period", 4))
    return {"textures": record, "tile": side,
            "terrain_sha256": hashlib.sha256(
                open(terrain, "rb").read()).hexdigest()}


def write_tiles(pack, a, side, frames, period):
    """The tile sheet's manifest, with what tools/opendata adds beside
    it: the terrain mask <name>-terrain.png, classified from these
    tiles' own art, and <name>-sections.ini, their [ground] or [coast]
    sections (asset-packs.md, "Replacement tiles")."""
    path = os.path.join(pack, a["file"])
    name = os.path.splitext(os.path.basename(a["file"]))[0]
    mask = name + "-terrain.png"
    sections = os.path.join(os.path.dirname(path), name + "-sections.ini")
    with open(path, "w") as f:
        f.write("# Generated by tools/artgen/artgen.py from textures laid "
                "through the\n# original tiles' coastlines (tilemaker.py)."
                "\n[tiles]\nimage = %s\ntile = %d\n" % (
                    os.path.basename(a["atlas"]), side))
        if frames > 1:
            f.write("frames = %d\nperiod = %d\n" % (frames, period))
        if os.path.exists(os.path.join(os.path.dirname(path), mask)):
            f.write("terrain = %s\n" % mask)
        if os.path.exists(sections):
            with open(sections) as g:
                f.write("\n" + g.read())


def make_pictures(args, spec, a, profile, seed, height):
    """Frames each a picture of its own prompt (the land battle's troops
    on each ground), one seed for all so they look alike, laid in one
    atlas with its .sheet."""
    k = spec["pack"]["art_height"] / SCREEN_H
    lw, lh = a["size"]
    w, h = round(lw * k), round(lh * k)
    prefix = "flibustier/" + a["id"].replace("/", "-")
    frames, record = [], []
    try:
        for n, prompt in enumerate(a["pictures"]):
            one = dict(a, prompt=characters(prompt, spec))
            one.pop("pictures")
            png = render(args.server, workflow(one, profile, seed, w, h,
                                               prefix, spec))
            path = os.path.join(args.pack, a["atlas"] + ".%d.png" % n)
            with open(path, "wb") as f:
                f.write(png)
            frames.append(upload(args.server, path,
                                 "flibustier-frame-%d.png" % n))
            os.remove(path)
            record.append(" ".join(prompt.split()))
        atlas = render(args.server, atlas_workflow(
            frames, [(w, h)] * len(frames), prefix + "-atlas"))
    except RuntimeError as e:
        print("%s: %s" % (a["id"], e), file=sys.stderr)
        return None
    with open(os.path.join(args.pack, a["atlas"]), "wb") as f:
        f.write(atlas)
    write_sheet(os.path.join(args.pack, a["file"]),
                os.path.basename(a["atlas"]), [(w, h)] * len(frames),
                "Generated by tools/artgen/artgen.py, a picture a frame.")
    return record


def make_chain(args, spec, a, profile, seed, graph):
    """Frames that each change the one before (the fort shot to pieces):
    the first rendered from the prompt ("graph"), each next one the edit
    model's change of it by its step's instruction, its colours matched
    back; laid in one atlas with its .sheet."""
    k = spec["pack"]["art_height"] / SCREEN_H
    lw, lh = a["size"]
    w, h = round(lw * k), round(lh * k)
    prefix = "flibustier/" + a["id"].replace("/", "-")
    work = os.path.join(args.pack, a["atlas"] + ".frames")
    os.makedirs(work, exist_ok=True)
    paths, record = [os.path.join(work, "0.png")], []
    try:
        with open(paths[0], "wb") as f:
            f.write(render(args.server, graph))
        for n, step in enumerate(a["steps"], 1):
            free(args.server)
            up = upload(args.server, paths[-1], "flibustier-chain.png")
            entry = {"rect": [0, 0, lw, lh], "scene": a.get("scene",
                                                         "the picture"),
                     "instruction": step}
            edited = render(args.server, cast_workflow(
                entry, spec, profile, seed + n, up, None, prefix + "-edit"))
            epath = os.path.join(work, "edit.png")
            with open(epath, "wb") as f:
                f.write(edited)
            free(args.server)
            eup = upload(args.server, epath, "flibustier-chain-edit.png")
            paths.append(os.path.join(work, "%d.png" % n))
            with open(paths[-1], "wb") as f:
                f.write(render(args.server, match_workflow(
                    up, eup, [0, 0, lw, lh], k, prefix)))
            record.append({"seed": seed + n,
                           "instruction": " ".join(step.split())})
        frames = [upload(args.server, pth, "flibustier-frame-%d.png" % n)
                  for n, pth in enumerate(paths)]
        atlas = render(args.server, atlas_workflow(
            frames, [(w, h)] * len(frames), prefix + "-atlas"))
    except RuntimeError as e:
        print("%s: %s" % (a["id"], e), file=sys.stderr)
        return None
    with open(os.path.join(args.pack, a["atlas"]), "wb") as f:
        f.write(atlas)
    write_sheet(os.path.join(args.pack, a["file"]),
                os.path.basename(a["atlas"]), [(w, h)] * len(frames),
                "Generated by tools/artgen/artgen.py, each frame an edit "
                "of the one before.")
    shutil.rmtree(work)
    return record


def outline_workflow(photo, render, out_w, out_h, prefix, expand=-2):
    """A redrawn frame cut to its render's own outline, grown by
    "expand" pixels of the render (a third of the frame's; a body a
    little in, past the redraw's backdrop at its edge, a head a little out over the
    body it sits on, which is the same photograph) and softened: the figure keeps exactly the render's shape, so no head
    from a reference, no lost leg and no shrunken figure come along, and
    the redraw's background stays out."""
    return {
        "photo": {"class_type": "LoadImage", "inputs": {"image": photo}},
        "render": {"class_type": "LoadImage", "inputs": {"image": render}},
        "shape": {"class_type": "InvertMask", "inputs": {
            "mask": ["render", 1]}},
        "inner": {"class_type": "GrowMask", "inputs": {
            "mask": ["shape", 0], "expand": expand,
            "tapered_corners": True}},
        "mimg": {"class_type": "MaskToImage", "inputs": {
            "mask": ["inner", 0]}},
        "mblur": {"class_type": "ImageBlur", "inputs": {
            "image": ["mimg", 0], "blur_radius": 2, "sigma": 1.0}},
        "msmall": {"class_type": "ImageScale", "inputs": {
            "image": ["mblur", 0], "upscale_method": "bilinear",
            "width": out_w, "height": out_h, "crop": "disabled"}},
        "alpha": {"class_type": "ImageToMask", "inputs": {
            "image": ["msmall", 0], "channel": "red"}},
        "clear": {"class_type": "InvertMask", "inputs": {
            "mask": ["alpha", 0]}},
        "small": {"class_type": "ImageScale", "inputs": {
            "image": ["photo", 0], "upscale_method": "lanczos",
            "width": out_w, "height": out_h, "crop": "disabled"}},
        "rgba": {"class_type": "JoinImageWithAlpha", "inputs": {
            "image": ["small", 0], "alpha": ["clear", 0]}},
        "save": {"class_type": "SaveImage", "inputs": {
            "images": ["rgba", 0], "filename_prefix": prefix}},
    }


def silhouette_workflow(photo, render, out_w, out_h, prefix):
    """A redrawn frame cut to the figure: where the matte finds the man
    in the redraw, near its render's outline, so the redraw's background
    and anything it invents away from the figure are left out, at the
    frame's size, with its alpha."""
    return {
        "photo": {"class_type": "LoadImage", "inputs": {"image": photo}},
        "render": {"class_type": "LoadImage", "inputs": {"image": render}},
        "shape": {"class_type": "InvertMask", "inputs": {
            "mask": ["render", 1]}},
        "near": {"class_type": "GrowMask", "inputs": {
            "mask": ["shape", 0], "expand": 10, "tapered_corners": True}},
        "matter": {"class_type": "LoadBackgroundRemovalModel", "inputs": {
            "bg_removal_name": MODELS["matte"]}},
        "matte": {"class_type": "RemoveBackground", "inputs": {
            "bg_removal_model": ["matter", 0], "image": ["photo", 0]}},
        "man": {"class_type": "MaskComposite", "inputs": {
            "destination": ["matte", 0], "source": ["near", 0],
            "x": 0, "y": 0, "operation": "multiply"}},
        "mimg": {"class_type": "MaskToImage", "inputs": {
            "mask": ["man", 0]}},
        "msmall": {"class_type": "ImageScale", "inputs": {
            "image": ["mimg", 0], "upscale_method": "bilinear",
            "width": out_w, "height": out_h, "crop": "disabled"}},
        "alpha": {"class_type": "ImageToMask", "inputs": {
            "image": ["msmall", 0], "channel": "red"}},
        "clear": {"class_type": "InvertMask", "inputs": {
            "mask": ["alpha", 0]}},
        "small": {"class_type": "ImageScale", "inputs": {
            "image": ["photo", 0], "upscale_method": "lanczos",
            "width": out_w, "height": out_h, "crop": "disabled"}},
        "rgba": {"class_type": "JoinImageWithAlpha", "inputs": {
            "image": ["small", 0], "alpha": ["clear", 0]}},
        "save": {"class_type": "SaveImage", "inputs": {
            "images": ["rgba", 0], "filename_prefix": prefix}},
    }


def shrink_workflow(image, out_w, out_h, prefix):
    """A rendered frame at the frame's size, its alpha kept."""
    return {
        "image": {"class_type": "LoadImage", "inputs": {"image": image}},
        "small": {"class_type": "ImageScale", "inputs": {
            "image": ["image", 0], "upscale_method": "lanczos",
            "width": out_w, "height": out_h, "crop": "disabled"}},
        "mimg": {"class_type": "MaskToImage", "inputs": {
            "mask": ["image", 1]}},
        "msmall": {"class_type": "ImageScale", "inputs": {
            "image": ["mimg", 0], "upscale_method": "bilinear",
            "width": out_w, "height": out_h, "crop": "disabled"}},
        "m": {"class_type": "ImageToMask", "inputs": {
            "image": ["msmall", 0], "channel": "red"}},
        "rgba": {"class_type": "JoinImageWithAlpha", "inputs": {
            "image": ["small", 0], "alpha": ["m", 0]}},
        "save": {"class_type": "SaveImage", "inputs": {
            "images": ["rgba", 0], "filename_prefix": prefix}},
    }


# The fencers drawn this much lower (640x480 pixels) than the game draws
# its own (SW_Y 140): there they stand on the scenes' open floor, which
# in the pack's alley, fort and deck lies lower than the original's.
FENCER_DROP = 60
FENCER_BANKS = [6, 6, 6, 6, 6, 6, 6, 6, 6, 4, 4, 4, 6, 6, 10, 6]
FENCER_RENDER = (951, 609)	# a fight frame as rendered (3 x 317x203)
# The game's pixels drawn beyond each side of its 141-pixel frame (the
# sheet's frames start this far left): a stance's rear foot reaches out
# of the frame, the player's to the right, the opponent's to the left.
FENCER_MARGIN = 24
# The surrender's frames (14.3 on) are 114 lines high, not 90.
FENCER_TALL = (14, 3, 114)


def fencer_size(m, f, k, render=False):
    """A fight frame's size, in the pack's pixels or as rendered."""
    tall = m == FENCER_TALL[0] and f >= FENCER_TALL[1]
    wide = (141 + 2 * FENCER_MARGIN) / 141
    if render:
        return (round(FENCER_RENDER[0] * wide),
                round(FENCER_RENDER[1] * (FENCER_TALL[2] / 90 if tall else
                                          1)))
    return (round((141 + 2 * FENCER_MARGIN) * k),
            round((FENCER_TALL[2] if tall else 90) * k))


def fencer_steps(pack):
    """How many frames of each move the pack's sword fight shows, how far
    it moves a fencer forward on each of them in the moves that step, and
    on which frame each attack strikes (its data/sword table), so the
    frames walk and strike with it: (frames, steps, strikes), None
    without the table."""
    path = os.path.join(pack, "data", "sword.json")
    try:
        with open(path) as fp:
            t = json.load(fp)
    except OSError as e:
        print("fencers: %s" % e, file=sys.stderr)
        return None
    frames = [min(n, b) for n, b in zip(t["frames"], FENCER_BANKS)]
    steps = {}
    for m, (fwd, back) in enumerate(zip(t["step_in"], t["step_back"])):
        net = [i - b for i, b in zip(fwd, back)][:frames[m]]
        if any(net):
            steps[m] = net
    strikes = {m: row.index(next(v for v in row if v))
               for m, row in enumerate(t["strike"]) if any(row)}
    return frames, steps, strikes


def fencer_fabrics(args, spec, a, profile, seed):
    """The fencers' clothes' cloths, photographed by the image model
    (each a close-up of the cloth alone, not in the pack's film style:
    its blur and grain would smear the weave), made materials by
    fencers/fabric.py and copied to the render host, where
    materials.pbr() finds them; one already made from the same prompt
    and seed is kept.  Returns what was made, for the provenance, or
    None on failure."""
    host = args.render_host
    made = {}
    fabrics = a["fencer"].get("fabrics", {})
    if isinstance(fabrics, str):	# another fencer's: made with its seed
        owner = next(o for o in spec["asset"] if o["id"] == fabrics)
        fabrics, seed = owner["fencer"]["fabrics"], owner["seed"]
    for n, (name, f) in enumerate(sorted(fabrics.items())):
        s = seed + n
        stamp = hashlib.sha256(json.dumps(
            [f["prompt"].strip(), s, profile, f.get("rough", 0.75),
             f.get("relief", 6.0)]).encode()).hexdigest()
        lib = "$HOME/flibustier-cc0/flibustier-%s" % name
        made[name] = {"prompt": f["prompt"].strip(), "seed": s}
        try:
            have = subprocess.run(
                ["ssh", "-o", "BatchMode=yes", host,
                 "cat %s/stamp 2>/dev/null" % lib], capture_output=True,
                text=True, stdin=subprocess.DEVNULL).stdout.strip()
            if have == stamp:
                continue
            free(args.server)
            png = render(args.server, texture_workflow(
                spec, profile, s, f["prompt"], f.get("size", 1024),
                "flibustier/fabric-" + name, styled=False))
            out = os.path.join(args.pack, "art", "fabric-" + name)
            os.makedirs(out, exist_ok=True)
            picture = os.path.join(out, "photo.png")
            with open(picture, "wb") as fp:
                fp.write(png)
            subprocess.run([sys.executable, os.path.join(
                HERE, "fencers", "fabric.py"), picture, out,
                "flibustier-" + name, "--rough", str(f.get("rough", 0.75)),
                "--relief", str(f.get("relief", 6.0))], check=True)
            os.remove(picture)
            with open(os.path.join(out, "stamp"), "w") as fp:
                fp.write(stamp + "\n")
            subprocess.run(["ssh", "-o", "BatchMode=yes", host,
                            "mkdir -p %s" % lib], check=True,
                           stdin=subprocess.DEVNULL)
            subprocess.run(["scp", "-q"] + [
                os.path.join(out, n) for n in sorted(os.listdir(out))] +
                ["%s:%s/" % (host, lib.replace("$HOME/", ""))], check=True)
            shutil.rmtree(out)
        except (subprocess.CalledProcessError, RuntimeError) as e:
            print("%s: fabric %s: %s" % (a["id"], name, e), file=sys.stderr)
            return None
    return made


def fencer_trace(args, side, moves, work):
    """The original fencer's frames traced (fencers/trace.py) from the
    player's game data into "work", for fencer.py to pose its figure
    the same way: the JSON's path, or None.  Never kept in the pack."""
    binary = os.path.join(os.path.dirname(os.path.dirname(HERE)), "build",
                          "flibustier")
    if not os.path.exists(binary) or not args.data:
        print("fencers: tracing needs the built flibustier and --data",
              file=sys.stderr)
        return None
    out = os.path.join(work, "trace-%s.json" % side)
    try:
        subprocess.run([sys.executable, os.path.join(
            HERE, "fencers", "trace.py"), "--flibustier", binary,
            "--data", os.path.expanduser(args.data), "--server",
            args.server, "--side", side, "--work", os.path.join(
                work, "frames"), out] + (["--moves", ",".join(
                    map(str, moves))] if moves else []), check=True)
    except subprocess.CalledProcessError as e:
        print("fencers: tracing failed: %s" % e, file=sys.stderr)
        return None
    return out


def make_fencer(args, spec, a, profile, seed):
    """A fencer's sprite groups: its layers rendered by fencers/fencer.py
    on the render host, the body and heads redrawn as photographs (one
    seed for all) and cut to their render's outline, the weapons and
    wounds as rendered, each group's frames in a grid atlas (a move a
    row) with its .sheet."""
    k = spec["pack"]["art_height"] / SCREEN_H
    side = a["fencer"]["side"]
    host = args.render_host
    work = "flibustier-fencers/%s" % side
    local = os.path.join(args.pack, "art", "fencer-%s.frames" % side)
    script = os.path.join(HERE, "fencers", "fencer.py")
    moves = a["fencer"].get("moves")
    mdir = os.path.join(args.pack, "provenance", "fencers")
    table = fencer_steps(args.pack)
    if table is None:
        return None
    shown, steps, strikes = table
    fabrics = fencer_fabrics(args, spec, a, profile, seed)
    if fabrics is None:
        return None
    tmp = tempfile.mkdtemp(prefix="flibustier-trace-")
    try:
        trace = fencer_trace(args, side, moves, tmp)
        if trace is None:
            return None
        subprocess.run(["ssh", "-o", "BatchMode=yes", host,
                        "mkdir -p %s" % work], check=True,
                       stdin=subprocess.DEVNULL)
        fetch_library(host)
        subprocess.run(["scp", "-q", script,
                        os.path.join(HERE, "fencers", "human.py"),
                        os.path.join(HERE, "materials.py"), trace,
                        "%s:%s/" % (host, work)], check=True)
        subprocess.run(["ssh", "-o", "BatchMode=yes", host,
                        "cd %s && rm -rf out && blender -b -P fencer.py -- "
                        "--side %s --out out --frame %dx%d --margin %d "
                        "--steps '%s' --frames '%s' --strikes '%s' "
                        "--trace %s --blend model.blend %s "
                        "> blender.log 2>&1; s=$?; rm -f %s; exit $s" % (
                            work, side, *fencer_size(0, 0, k, render=True),
                            FENCER_MARGIN,
                            *(json.dumps(v, separators=(",", ":"))
                              for v in (steps, shown, strikes)),
                            os.path.basename(trace),
                            "--moves " + ",".join(map(str, moves))
                            if moves else "", os.path.basename(trace))],
                       check=True, stdin=subprocess.DEVNULL)
        if os.path.exists(local):
            shutil.rmtree(local)
        os.makedirs(local)
        subprocess.run(["scp", "-q", "-r", "%s:%s/out/." % (host, work),
                        local], check=True)
        os.makedirs(mdir, exist_ok=True)
        subprocess.run(["scp", "-q", "%s:%s/model.blend" % (host, work),
                        os.path.join(mdir, "%s.blend" % side)], check=True)
    except subprocess.CalledProcessError as e:
        print("%s: blender on %s failed: %s" % (a["id"], host, e),
              file=sys.stderr)
        return None
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    prefix = "flibustier/" + a["id"].replace("/", "-")
    frames = [(m, f) for m in (moves or range(len(FENCER_BANKS)))
              for f in range(FENCER_BANKS[m])]
    record = {}
    fc = dict(a["fencer"])
    if not redrawn(spec, a):
        fc["redraw"], fc["photo"] = {}, {}
    free(args.server)
    try:
        # The redraws: each reference render (a fencer, the opponent with
        # each of his heads) made a photograph, every frame; the image
        # model and the matte take turns, never both loaded.
        for ref, text in fc["redraw"].items():
            prompt = characters(text, spec)
            for m, f in frames:
                rw, rh = fencer_size(m, f, k, render=True)
                rup = upload(args.server, os.path.join(
                    local, ref, "%d.%d.png" % (m, f)),
                    "flibustier-fencer-ref-%s.png" % side)
                png = render(args.server, ship_frame_workflow(
                    spec, profile, seed, rup, prompt,
                    a.get("denoise", 0.6), rw, rh, prefix, (rw, rh)))
                with open(os.path.join(local, "%s-photo-%d.%d.png" % (
                        ref, m, f)), "wb") as fp:
                    fp.write(png)
        free(args.server)
        for layer, (gid, gfile) in fc["groups"].items():
            ref = fc["photo"].get(layer)
            out = {}
            for m, f in frames:
                fw, fh = fencer_size(m, f, k)
                src = os.path.join(local, layer, "%d.%d.png" % (m, f))
                up = upload(args.server, src,
                            "flibustier-fencer-%s.png" % side)
                if ref:
                    # Cut from its photograph to its own render's outline.
                    pup = upload(args.server, os.path.join(
                        local, "%s-photo-%d.%d.png" % (ref, m, f)),
                        "flibustier-photo-%s.png" % side)
                    png = render(args.server, outline_workflow(
                        pup, up, fw, fh, prefix + "-cut",
                        -9 if layer == "body" else 3))
                else:
                    png = render(args.server, shrink_workflow(
                        up, fw, fh, prefix + "-small"))
                path = os.path.join(local, "%s-%d.%d.png" % (layer, m, f))
                with open(path, "wb") as fp:
                    fp.write(png)
                out[(m, f)] = upload(
                    args.server, path,
                    "flibustier-frame-%s-%d-%d.png" % (side, m, f))
            # The atlas: move m on row m, frame f in column f.
            cols = max(FENCER_BANKS)
            fw = fencer_size(0, 0, k)[0]
            rows = [max(fencer_size(m, f, k)[1]
                        for f in range(FENCER_BANKS[m]))
                    for m in range(len(FENCER_BANKS))]
            top = [sum(rows[:m]) for m in range(len(rows))]
            g = {"canvas": {"class_type": "EmptyImage", "inputs": {
                "width": cols * fw, "height": sum(rows),
                "batch_size": 1, "color": 0}},
                "clear": {"class_type": "SolidMask", "inputs": {
                    "value": 1.0, "width": cols * fw,
                    "height": sum(rows)}}}
            img, msk = ["canvas", 0], ["clear", 0]
            for n, ((m, f), image) in enumerate(sorted(out.items())):
                g["f%d" % n] = {"class_type": "LoadImage", "inputs": {
                    "image": image}}
                g["p%d" % n] = {"class_type": "ImageCompositeMasked",
                                "inputs": {"destination": img,
                                           "source": ["f%d" % n, 0],
                                           "x": f * fw, "y": top[m],
                                           "resize_source": False}}
                g["m%d" % n] = {"class_type": "MaskComposite", "inputs": {
                    "destination": msk, "source": ["f%d" % n, 1],
                    "x": f * fw, "y": top[m], "operation": "multiply"}}
                img, msk = ["p%d" % n, 0], ["m%d" % n, 0]
            g["rgba"] = {"class_type": "JoinImageWithAlpha", "inputs": {
                "image": img, "alpha": msk}}
            g["save"] = {"class_type": "SaveImage", "inputs": {
                "images": ["rgba", 0], "filename_prefix": prefix + "-atlas"}}
            atlas = os.path.splitext(gfile)[0] + ".png"
            with open(os.path.join(args.pack, atlas), "wb") as fp:
                fp.write(render(args.server, g))
            lines = ["# Generated by tools/artgen/artgen.py from the "
                     "fencer model (fencers/fencer.py).", "[sheet]",
                     "image = " + os.path.basename(atlas), "", "[frames]"]
            for m in range(len(FENCER_BANKS)):
                for f in range(FENCER_BANKS[m]):
                    lines.append("%d.%d = %d %d %d %d %d %d" % (
                        (m, f, f * fw, top[m]) + fencer_size(m, f, k) +
                        (-round(FENCER_MARGIN * k), round(FENCER_DROP * k))))
            with open(os.path.join(args.pack, gfile), "w") as fp:
                fp.write("\n".join(lines) + "\n")
            record[gid] = {"layer": layer, "photo": ref}
            print("%s: %s done" % (a["id"], gid))
    except RuntimeError as e:
        print("%s: %s" % (a["id"], e), file=sys.stderr)
        return None
    # The renders and photographs stay (art/fencer-SIDE.frames, not
    # part of the pack), to cut again without redrawing.
    return {"groups": record, "frame": list(fencer_size(0, 0, k)),
            "redraw": {r: " ".join(characters(t, spec).split())
                       for r, t in fc["redraw"].items()},
            "render": list(fencer_size(0, 0, k, render=True)),
            "denoise": a.get("denoise", 0.6),
            "blend": "provenance/fencers/%s.blend" % side,
            "model_sha256": hashlib.sha256(open(script, "rb").read())
            .hexdigest(), "poses": "fitted at render time to the frames "
            "traced from the player's own game data (fencers/trace.py)",
            "frames": shown, "steps": steps,
            "fabrics": fabrics}


def frame_bounds(path, asset, height):
    """The frame's outer edge in a rendered framed picture, in its frame
    rectangle: where the wall the model drew around it ends.  A row or
    column belongs to the frame once most of its pixels are gilt."""
    import tilemaker
    k = height / SCREEN_H
    fx, fy, fw, fh = (round(v * k) for v in asset["frame"])
    w, h, ch, rows = tilemaker.png_read(open(path, "rb").read())

    def px(x, y):
        r = rows[y]
        return r[x * ch], r[x * ch + 1], r[x * ch + 2]

    # The gilded frame is warm, the wall the model leaves around it is
    # not (dark slate or grey): a pixel is the frame's when its red
    # clearly outweighs its blue.
    def off(c):
        return c[0] - c[2] > 24

    def frame_row(y):
        return sum(off(px(fx + x, fy + y)) for x in range(0, fw, 2)) * 2 \
            > fw * 0.85

    def frame_col(x):
        return sum(off(px(fx + x, fy + y)) for y in range(0, fh, 2)) * 2 \
            > fh * 0.85

    top = next((y for y in range(fh) if frame_row(y)), 0)
    bottom = next((y for y in range(fh - 1, -1, -1) if frame_row(y)), fh - 1)
    left = next((x for x in range(fw) if frame_col(x)), 0)
    right = next((x for x in range(fw - 1, -1, -1) if frame_col(x)), fw - 1)
    return fx + left, fy + top, right - left + 1, bottom - top + 1


def frame_fit_workflow(image, bounds, asset, height, out_w, out_h, prefix):
    """A framed picture cut at its frame's outer edge ("bounds", found by
    frame_bounds()) and set back in the frame's place on the field."""
    k = height / SCREEN_H
    fx, fy, fw, fh = (round(v * k) for v in asset["frame"])
    bx, by, bw, bh = bounds
    return {
        "image": {"class_type": "LoadImage", "inputs": {"image": image}},
        "framed": {"class_type": "ImageCrop", "inputs": {
            "image": ["image", 0], "width": bw, "height": bh, "x": bx,
            "y": by}},
        "size": {"class_type": "ImageScale", "inputs": {
            "image": ["framed", 0], "upscale_method": "lanczos",
            "width": fw, "height": fh, "crop": "disabled"}},
        "field": {"class_type": "EmptyImage", "inputs": {
            "width": out_w, "height": out_h, "batch_size": 1,
            "color": int(asset["field"].lstrip("#"), 16)}},
        "place": {"class_type": "ImageCompositeMasked", "inputs": {
            "destination": ["field", 0], "source": ["size", 0],
            "x": fx, "y": fy, "resize_source": False}},
        "save": {"class_type": "SaveImage", "inputs": {
            "images": ["place", 0], "filename_prefix": prefix}},
    }


PORTRAIT_RENDER = (768, 768)		# a ship's portrait as rendered
# The encounter's spyglass views: 5 of 200x200 on black, at these
# places of the picture, shown in turn (screens.c, encounter_picture).
PORTRAIT_VIEWS = [(1, 1), (202, 1), (403, 1), (1, 202), (202, 202)]
PORTRAIT_VIEW = 200


def portrait_sheet_workflow(sea, ship, mask, k, prefix):
    """A ship portrait's 5 spyglass views: the same sea and sky in every
    view, the glass held still, and the ship (cut out) riding the swell
    on it, rising, falling and drifting a little from view to view; each
    seen through the round glass, on black."""
    side = round(PORTRAIT_VIEW * k)
    win = round(PORTRAIT_RENDER[0] * 0.9)
    # The ship placed `base` further on than her swing, so no offset is
    # negative (the composite takes none), and the window with her.
    base = 16
    off = (PORTRAIT_RENDER[0] - win) // 2 + base
    g = {
        "sea": {"class_type": "LoadImage", "inputs": {"image": sea}},
        "ship": {"class_type": "LoadImage", "inputs": {"image": ship}},
        "solid": {"class_type": "InvertMask", "inputs": {
            "mask": ["ship", 1]}},
        "glass": {"class_type": "LoadImage", "inputs": {"image": mask}},
        "gfit": {"class_type": "ImageScale", "inputs": {
            "image": ["glass", 0], "upscale_method": "bilinear",
            "width": side, "height": side, "crop": "disabled"}},
        "gmask": {"class_type": "ImageToMask", "inputs": {
            "image": ["gfit", 0], "channel": "red"}},
        "canvas": {"class_type": "EmptyImage", "inputs": {
            "width": round(SCREEN_W * k), "height": round(SCREEN_H * k),
            "batch_size": 1, "color": 0}},
    }
    last = ["canvas", 0]
    for n, (vx, vy) in enumerate(PORTRAIT_VIEWS):
        t = 2 * math.pi * n / len(PORTRAIT_VIEWS)
        dx = round(PORTRAIT_RENDER[0] * 0.012 * math.cos(t))
        dy = round(PORTRAIT_RENDER[1] * 0.02 * math.sin(t))
        g["on%d" % n] = {"class_type": "ImageCompositeMasked", "inputs": {
            "destination": ["sea", 0], "source": ["ship", 0],
            "x": base + dx, "y": base + dy, "resize_source": False,
            "mask": ["solid", 0]}}
        g["c%d" % n] = {"class_type": "ImageCrop", "inputs": {
            "image": ["on%d" % n, 0], "width": win, "height": win,
            "x": off, "y": off}}
        g["s%d" % n] = {"class_type": "ImageScale", "inputs": {
            "image": ["c%d" % n, 0], "upscale_method": "lanczos",
            "width": side, "height": side, "crop": "disabled"}}
        g["p%d" % n] = {"class_type": "ImageCompositeMasked", "inputs": {
            "destination": last, "source": ["s%d" % n, 0],
            "x": round(vx * k), "y": round(vy * k), "resize_source": False,
            "mask": ["gmask", 0]}}
        last = ["p%d" % n, 0]
    g["save"] = {"class_type": "SaveImage", "inputs": {
        "images": last, "filename_prefix": prefix}}
    return g


def views_sheet_workflow(views, mask, k, prefix):
    """A ship portrait's 5 spyglass views from 5 pictures at sea (the
    waves moving, the ship riding them), each seen through the glass."""
    side = round(PORTRAIT_VIEW * k)
    win = round(PORTRAIT_RENDER[0] * 0.9)
    off = (PORTRAIT_RENDER[0] - win) // 2
    g = {
        "glass": {"class_type": "LoadImage", "inputs": {"image": mask}},
        "gfit": {"class_type": "ImageScale", "inputs": {
            "image": ["glass", 0], "upscale_method": "bilinear",
            "width": side, "height": side, "crop": "disabled"}},
        "gmask": {"class_type": "ImageToMask", "inputs": {
            "image": ["gfit", 0], "channel": "red"}},
        "canvas": {"class_type": "EmptyImage", "inputs": {
            "width": round(SCREEN_W * k), "height": round(SCREEN_H * k),
            "batch_size": 1, "color": 0}},
    }
    last = ["canvas", 0]
    for n, ((vx, vy), view) in enumerate(zip(PORTRAIT_VIEWS, views)):
        g["v%d" % n] = {"class_type": "LoadImage", "inputs": {
            "image": view}}
        g["c%d" % n] = {"class_type": "ImageCrop", "inputs": {
            "image": ["v%d" % n, 0], "width": win, "height": win,
            "x": off, "y": off}}
        g["s%d" % n] = {"class_type": "ImageScale", "inputs": {
            "image": ["c%d" % n, 0], "upscale_method": "lanczos",
            "width": side, "height": side, "crop": "disabled"}}
        g["p%d" % n] = {"class_type": "ImageCompositeMasked", "inputs": {
            "destination": last, "source": ["s%d" % n, 0],
            "x": round(vx * k), "y": round(vy * k), "resize_source": False,
            "mask": ["gmask", 0]}}
        last = ["p%d" % n, 0]
    g["save"] = {"class_type": "SaveImage", "inputs": {
        "images": last, "filename_prefix": prefix}}
    return g


def make_portrait(args, spec, a, profile, seed):
    """A ship's encounter portrait: its class's model rendered on the
    render host as a lookout sees it, redrawn as a photograph at sea
    with its sky, then laid out as the 5 spyglass views."""
    k = spec["pack"]["art_height"] / SCREEN_H
    cls = a["portrait"]["class"]
    host = args.render_host
    work = "flibustier-ships/%s" % a["id"].replace("/", "-")
    local = os.path.join(args.pack, a["file"] + ".work")
    script = os.path.join(HERE, "ships", "ship.py")
    try:
        subprocess.run(["ssh", "-o", "BatchMode=yes", host,
                        "mkdir -p %s" % work], check=True,
                       stdin=subprocess.DEVNULL)
        fetch_library(host)
        subprocess.run(["scp", "-q", script,
                        os.path.join(HERE, "materials.py"),
                        "%s:%s/" % (host, work)], check=True)
        subprocess.run(["ssh", "-o", "BatchMode=yes", host,
                        "cd %s && rm -rf out && blender -b -P ship.py -- "
                        "--class '%s' --portrait --views %d --out out "
                        "--frame %dx%d > blender.log 2>&1" % (
                            work, cls, len(PORTRAIT_VIEWS),
                            *PORTRAIT_RENDER)],
                       check=True, stdin=subprocess.DEVNULL)
        os.makedirs(local, exist_ok=True)
        subprocess.run(["scp", "-q", "%s:%s/out/view*.png" % (host, work),
                        local + "/"], check=True)
    except subprocess.CalledProcessError as e:
        print("%s: blender on %s failed: %s" % (a["id"], host, e),
              file=sys.stderr)
        return None
    free(args.server)
    prefix = "flibustier/" + a["id"].replace("/", "-")
    redraw = redrawn(spec, a)
    try:
        # Each view lightly redrawn, one seed for all, so the waves and
        # the ship stay the render's and only texture and light come in.
        views = []
        for n in range(len(PORTRAIT_VIEWS)):
            path = os.path.join(local, "view%d.png" % n)
            if redraw:
                up = upload(args.server, path, "flibustier-view.png")
                with open(path, "wb") as f:
                    f.write(render(args.server, ship_frame_workflow(
                        spec, profile, seed, up, a["prompt"],
                        a.get("view_denoise", 0.3), *PORTRAIT_RENDER,
                        prefix, PORTRAIT_RENDER)))
            views.append(upload(args.server, path,
                                "flibustier-view-%d.png" % n))
        free(args.server)
        mask = upload(args.server, os.path.join(HERE, "layouts",
                                                "spyglass.png"),
                      "flibustier-spyglass.png")
        sheet = render(args.server, views_sheet_workflow(
            views, mask, k, prefix + "-views"))
    except RuntimeError as e:
        print("%s: %s" % (a["id"], e), file=sys.stderr)
        return None
    with open(os.path.join(args.pack, a["file"]), "wb") as f:
        f.write(sheet)
    shutil.rmtree(local)
    return {"class": cls, "redrawn": redraw, "views": "at sea, Blender",
            "denoise": a.get("view_denoise", 0.3) if redraw else None,
            "prompt": a["prompt"].strip() if redraw else None,
            "model_sha256": hashlib.sha256(open(script, "rb").read())
            .hexdigest()}


FLAG_FRAME = 200			# a flag's frame, 4 x 2 on its picture
FLAG_RENDER = 450			# a flag frame as rendered


def flag_sheet_workflow(frames, sky, mask, k, prefix):
    """A flag's 8 frames, each the waving flag over the sky, seen through
    the spyglass, laid 4 by 2 on black as the game reads them."""
    side = round(FLAG_FRAME * k)
    g = {
        "sky": {"class_type": "LoadImage", "inputs": {"image": sky}},
        "skyfit": {"class_type": "ImageScale", "inputs": {
            "image": ["sky", 0], "upscale_method": "lanczos",
            "width": side, "height": side, "crop": "center"}},
        "glass": {"class_type": "LoadImage", "inputs": {"image": mask}},
        "gfit": {"class_type": "ImageScale", "inputs": {
            "image": ["glass", 0], "upscale_method": "bilinear",
            "width": side, "height": side, "crop": "disabled"}},
        "gmask": {"class_type": "ImageToMask", "inputs": {
            "image": ["gfit", 0], "channel": "red"}},
        "canvas": {"class_type": "EmptyImage", "inputs": {
            "width": 4 * side, "height": 2 * side, "batch_size": 1,
            "color": 0}},
        "black": {"class_type": "EmptyImage", "inputs": {
            "width": side, "height": side, "batch_size": 1, "color": 0}},
    }
    last = ["canvas", 0]
    for n, frame in enumerate(frames):
        g["f%d" % n] = {"class_type": "LoadImage", "inputs": {
            "image": frame}}
        g["fs%d" % n] = {"class_type": "ImageScale", "inputs": {
            "image": ["f%d" % n, 0], "upscale_method": "lanczos",
            "width": side, "height": side, "crop": "disabled"}}
        g["fm%d" % n] = {"class_type": "ImageScale", "inputs": {
            "image": ["f%dm" % n, 0], "upscale_method": "bilinear",
            "width": side, "height": side, "crop": "disabled"}}
        g["f%dm" % n] = {"class_type": "MaskToImage", "inputs": {
            "mask": ["f%di" % n, 0]}}
        g["f%di" % n] = {"class_type": "InvertMask", "inputs": {
            "mask": ["f%d" % n, 1]}}
        g["fa%d" % n] = {"class_type": "ImageToMask", "inputs": {
            "image": ["fm%d" % n, 0], "channel": "red"}}
        g["on%d" % n] = {"class_type": "ImageCompositeMasked", "inputs": {
            "destination": ["skyfit", 0], "source": ["fs%d" % n, 0],
            "x": 0, "y": 0, "resize_source": False,
            "mask": ["fa%d" % n, 0]}}
        g["view%d" % n] = {"class_type": "ImageCompositeMasked", "inputs": {
            "destination": ["black", 0], "source": ["on%d" % n, 0],
            "x": 0, "y": 0, "resize_source": False, "mask": ["gmask", 0]}}
        g["p%d" % n] = {"class_type": "ImageCompositeMasked", "inputs": {
            "destination": last, "source": ["view%d" % n, 0],
            "x": (n % 4) * side, "y": (n // 4) * side,
            "resize_source": False}}
        last = ["p%d" % n, 0]
    g["save"] = {"class_type": "SaveImage", "inputs": {
        "images": last, "filename_prefix": prefix}}
    return g


def design_workflow(spec, profile, seed, prompt, w, h, prefix):
    """A flag's design, flat, filling a w x h picture."""
    g = texture_workflow(spec, profile, seed, prompt, w, prefix)
    g["latent"]["inputs"].update(width=1248, height=832)
    g["small"]["inputs"].update(width=w, height=h, crop="disabled")
    return g


def flatten_inks(path, inks):
    """Bring a design to its few inks, each pixel the nearest: a period
    flag's figures are flat paint, which the image model shades like a
    photograph however it is asked."""
    import tilemaker
    w, h, ch, rows = tilemaker.png_read(open(path, "rb").read())
    pal = [tuple(int(c[i:i + 2], 16) for i in (1, 3, 5)) for c in inks]
    out = []
    for r in rows:
        line = bytearray(w * 4)
        for x in range(w):
            c = (r[x * ch], r[x * ch + 1], r[x * ch + 2])
            best = min(pal, key=lambda q: (q[0] - c[0]) ** 2 * 3 +
                       (q[1] - c[1]) ** 2 * 4 + (q[2] - c[2]) ** 2 * 2)
            line[x * 4:x * 4 + 4] = bytes((*best, 255))
        out.append(line)
    tilemaker.png_write(path, w, h, out)


def make_flag(args, spec, a, profile, seed):
    """A ship's colours: the design painted flat, waved on its staff in
    Blender on the render host (flags/flag.py), and each of the 8 frames
    set over the spyglass's sky."""
    k = spec["pack"]["art_height"] / SCREEN_H
    fl = a["flag"]
    host = args.render_host
    work = "flibustier-flags/%s" % a["id"].replace("/", "-")
    local = os.path.join(args.pack, a["file"] + ".work")
    script = os.path.join(HERE, "flags", "flag.py")
    prefix = "flibustier/" + a["id"].replace("/", "-")
    os.makedirs(local, exist_ok=True)
    free(args.server)
    try:
        design = render(args.server, design_workflow(
            spec, profile, seed, fl["design"], 1200, 800,
            prefix + "-design"))
        dpath = os.path.join(local, "design.png")
        with open(dpath, "wb") as f:
            f.write(design)
        if fl.get("inks"):
            flatten_inks(dpath, fl["inks"])
        sky = render(args.server, texture_workflow(
            spec, profile, spec["flags"]["seed"], spec["flags"]["sky"],
            FLAG_RENDER, prefix + "-sky"))
        spath = os.path.join(local, "sky.png")
        with open(spath, "wb") as f:
            f.write(sky)
    except RuntimeError as e:
        print("%s: %s" % (a["id"], e), file=sys.stderr)
        return None
    try:
        subprocess.run(["ssh", "-o", "BatchMode=yes", host,
                        "mkdir -p %s" % work], check=True,
                       stdin=subprocess.DEVNULL)
        subprocess.run(["scp", "-q", script, dpath, "%s:%s/" % (host, work)],
                       check=True)
        subprocess.run(["ssh", "-o", "BatchMode=yes", host,
                        "cd %s && rm -rf out && blender -b -P flag.py -- "
                        "--design design.png --out out --frame %d %s "
                        "> blender.log 2>&1" % (
                            work, FLAG_RENDER,
                            "--torn" if fl.get("torn") else "")],
                       check=True, stdin=subprocess.DEVNULL)
        subprocess.run(["scp", "-q", "%s:%s/out/*.png" % (host, work),
                        local + "/"], check=True)
    except subprocess.CalledProcessError as e:
        print("%s: blender on %s failed: %s" % (a["id"], host, e),
              file=sys.stderr)
        return None
    try:
        frames = [upload(args.server, os.path.join(local, "%d.png" % n),
                         "flibustier-flag-%d.png" % n) for n in range(8)]
        sup = upload(args.server, spath, "flibustier-flag-sky.png")
        mask = upload(args.server, os.path.join(HERE, "layouts",
                                                "spyglass.png"),
                      "flibustier-spyglass.png")
        sheet = render(args.server, flag_sheet_workflow(
            frames, sup, mask, k, prefix + "-sheet"))
    except RuntimeError as e:
        print("%s: %s" % (a["id"], e), file=sys.stderr)
        return None
    with open(os.path.join(args.pack, a["file"]), "wb") as f:
        f.write(sheet)
    shutil.rmtree(local)
    return {"design": " ".join(fl["design"].split()),
            "torn": bool(fl.get("torn")),
            "model_sha256": hashlib.sha256(open(script, "rb").read())
            .hexdigest()}


def trim_alpha(path):
    """Crop a picture with alpha to what it shows, so its top left is
    the shape's top left (a pointer's tip)."""
    import tilemaker
    w, h, ch, rows = tilemaker.png_read(open(path, "rb").read())
    xs, ys = [], []
    for y, r in enumerate(rows):
        for x in range(w):
            if r[x * ch + 3] > 64:
                xs.append(x)
                ys.append(y)
    x0, x1, y0, y1 = min(xs), max(xs) + 1, min(ys), max(ys) + 1
    out = [bytearray(r[x0 * ch:x1 * ch]) for r in rows[y0:y1]]
    tilemaker.png_write(path, x1 - x0, y1 - y0, out)
    return x1 - x0, y1 - y0


def make_sprite(args, spec, a, profile, seed):
    """A small sprite from a prompt (the mouse pointer): rendered, cut
    out, cropped to its shape and brought to its size in 640x480 pixels
    at the pack's art height, with its .sheet."""
    k = spec["pack"]["art_height"] / SCREEN_H
    fw, fh = (round(v * k) for v in a["sprite"]["size"])
    prefix = "flibustier/" + a["id"].replace("/", "-")
    work = os.path.join(args.pack, a["atlas"] + ".work.png")
    free(args.server)
    try:
        png = render(args.server, texture_workflow(
            spec, profile, seed, a["prompt"], 1024, prefix))
        with open(work, "wb") as f:
            f.write(png)
        free(args.server)
        up = upload(args.server, work, "flibustier-sprite.png")
        with open(work, "wb") as f:
            f.write(render(args.server, cutout_workflow(
                up, 1024, 1024, prefix + "-cut")))
        trim_alpha(work)
        up = upload(args.server, work, "flibustier-sprite-cut.png")
        png = render(args.server, shrink_workflow(up, fw, fh,
                                                  prefix + "-small"))
    except RuntimeError as e:
        print("%s: %s" % (a["id"], e), file=sys.stderr)
        return None
    os.remove(work)
    with open(os.path.join(args.pack, a["atlas"]), "wb") as f:
        f.write(png)
    write_sheet(os.path.join(args.pack, a["file"]),
                os.path.basename(a["atlas"]), [(fw, fh)],
                "Generated by tools/artgen/artgen.py.")
    return {"size": [fw, fh], "prompt": a["prompt"].strip()}


def make_clouds(args, spec, a, profile, seed):
    """The sailing map's clouds: each a photograph of a cloud seen from
    above on black, its brightness its opacity, at its frame's size;
    then their shadows, the same shapes dark and fainter, in one sheet
    (the game draws a shadow the frame 3 further on)."""
    import tilemaker
    k = spec["pack"]["art_height"] / SCREEN_H
    fw, fh = (round(v * k) for v in a["clouds"]["size"])
    prefix = "flibustier/" + a["id"].replace("/", "-")
    work = os.path.join(args.pack, a["atlas"] + ".work.png")
    clouds = []
    free(args.server)
    try:
        for n, prompt in enumerate(a["clouds"]["prompts"]):
            g = texture_workflow(spec, profile, seed + n, prompt, fw,
                                 prefix)
            g["latent"]["inputs"].update(width=1536, height=512)
            g["small"]["inputs"].update(width=fw, height=fh,
                                        crop="disabled")
            with open(work, "wb") as f:
                f.write(render(args.server, g))
            w, h, ch, rows = tilemaker.png_read(open(work, "rb").read())
            clouds.append(rows)
    except RuntimeError as e:
        print("%s: %s" % (a["id"], e), file=sys.stderr)
        return None
    os.remove(work)
    n = len(clouds)
    shadow = a["clouds"].get("shadow", 0.45)
    atlas = [bytearray(2 * n * fw * 4) for _ in range(fh)]
    for i, rows in enumerate(clouds):
        for y in range(fh):
            for x in range(fw):
                r, g_, b = rows[y][x * 3:x * 3 + 3]
                # Brightness above the black is the cloud; its edge
                # ramps in softly.
                lum = max(r, g_, b)
                al = max(0, min(255, (lum - 24) * 255 // 180))
                o = (i * fw + x) * 4
                atlas[y][o:o + 4] = bytes((min(255, r + 20),
                                           min(255, g_ + 20),
                                           min(255, b + 20), al))
                o = ((n + i) * fw + x) * 4
                atlas[y][o:o + 4] = bytes((10, 18, 30, int(al * shadow)))
    tilemaker.png_write(os.path.join(args.pack, a["atlas"]), 2 * n * fw,
                        fh, atlas)
    write_sheet(os.path.join(args.pack, a["file"]),
                os.path.basename(a["atlas"]), [(fw, fh)] * (2 * n),
                "Generated by tools/artgen/artgen.py: the clouds, then "
                "their shadows.")
    return {"size": [fw, fh], "prompts": [" ".join(p.split())
                                          for p in a["clouds"]["prompts"]],
            "shadow": shadow}


def make_board(args, spec, a, profile, seed):
    """The trade boards, every language's three banks: the materials and
    objects painted (the goods and the cannon cut out), then each frame
    composed by boards.py, its words set in the pack's font."""
    import boards
    k = spec["pack"]["art_height"] / SCREEN_H
    bd = a["board"]
    prefix = "flibustier/" + a["id"].replace("/", "-")
    work = os.path.join(args.pack, "art", "boards.work")
    os.makedirs(work, exist_ok=True)
    src = {}
    free(args.server)
    try:
        # Each painting kept under its prompt's hash, so a change paints
        # again only what changed.
        # A painting's seed is the asset's plus its place among those
        # without one of their own ("seeds"), so adding one keeps the
        # others' seeds.
        fresh = []
        own = bd.get("seeds", {})
        order = sorted(k_ for k_ in bd["paint"] if k_ not in own)
        for key, prompt in sorted(bd["paint"].items()):
            n = own[key] if key in own else order.index(key)
            tag = hashlib.sha256(("%s %d %s %s" % (
                prompt, seed + n, profile,
                key in bd["cut"])).encode()).hexdigest()[:12]
            path = os.path.join(work, "src-%s-%s.png" % (key, tag))
            src[key] = path
            if os.path.exists(path):
                continue
            with open(path, "wb") as f:
                f.write(render(args.server, texture_workflow(
                    spec, profile, seed + n, prompt, 768,
                    prefix + "-" + key)))
            fresh.append(key)
        free(args.server)
        for key in bd["cut"]:
            if key not in fresh:
                continue
            up = upload(args.server, src[key], "flibustier-board-src.png")
            with open(src[key], "wb") as f:
                f.write(render(args.server, cutout_workflow(
                    up, 768, 768, prefix + "-cut")))
    except RuntimeError as e:
        print("%s: %s" % (a["id"], e), file=sys.stderr)
        return None
    for key in bd.get("flat", {}):
        if key in fresh:
            flatten_inks(src[key], bd["flat"][key])
    for alias, key in bd.get("same", {}).items():
        src[alias] = src[key]
    font = os.path.join(args.pack, bd["font"])
    out = {}
    for (lang, words), take in [(w, t) for t in (False, True)
                                for w in bd["words"].items()]:
        names = bd["take_banks" if take else "banks"][lang]
        banks = boards.compose(src, words, font, k,
                               os.path.join(work, lang),
                               os.path.join(work, lang), take=take)
        for bank, (atlas, sizes) in banks.items():
            name = names[bank]
            png = os.path.join(args.pack, "art", "%s.png" % name)
            shutil.move(atlas, png)
            write_sheet(os.path.join(args.pack, "art", "%s.sheet" % name),
                        os.path.basename(png), sizes,
                        "Generated by tools/artgen/artgen.py (boards.py).")
            out["spr/" + name] = "art/%s.sheet" % name
    # The paintings stay (art/boards.work, not part of the pack).
    for lang in bd["words"]:
        shutil.rmtree(os.path.join(work, lang))
    return {"banks": out, "paint": {k_: " ".join(v.split())
                                    for k_, v in bd["paint"].items()},
            "font": bd["font"]}


def make_cave(args, spec, a, profile, seed):
    """The treasure cave's piles: painted into the cave picture one after
    another, in the order the game draws them, each through its mask and
    onto the cave with the piles before it; the pile cut from its frame
    of that step by the matte, inside its mask, and laid on the cave for
    the next.  However many piles show, they make the cave with that
    many."""
    k = spec["pack"]["art_height"] / SCREEN_H
    cv = a["cave"]
    src = [r for r in spec["asset"] if r["id"] == cv["from"]][0]
    cur = os.path.join(args.pack, src["file"])
    work = os.path.join(args.pack, "art", "tcave.work")
    os.makedirs(work, exist_ok=True)
    prefix = "flibustier/" + a["id"].replace("/", "-")
    full = "%dx%d!" % (round(SCREEN_W * k), round(SCREEN_H * k))
    out = {}
    for n, pile in enumerate(cv["piles"]):
        x, y, w, h = pile["frame"]
        m = cv.get("margin", 40)
        ctx = [max(0, x - m), max(0, y - m)]
        ctx += [min(SCREEN_W, x + w + m) - ctx[0],
                min(SCREEN_H, y + h + m) - ctx[1]]
        X, Y, W, H = (round(v * k) for v in (x, y, w, h))
        maskfile = os.path.join(HERE, pile["mask"])
        painted = os.path.join(work, "painted-%d.png" % n)
        crop = os.path.join(work, "crop-%d.png" % n)
        cut = os.path.join(work, "cut-%d.png" % n)
        try:
            free(args.server)
            up = upload(args.server, cur, "flibustier-cave.png")
            mup = upload(args.server, maskfile,
                         "flibustier-" + os.path.basename(pile["mask"]))
            with open(painted, "wb") as f:
                f.write(render(args.server, repaint_workflow(
                    spec, profile, seed + n, up, mup, ctx,
                    cv["prompt"].replace("{pile}", pile["prompt"].strip()),
                    prefix)))
            subprocess.run(["magick", painted, "-crop", "%dx%d+%d+%d" % (
                W, H, X, Y), "+repage", crop], check=True)
            free(args.server)
            cup = upload(args.server, crop, "flibustier-cave-pile.png")
            with open(cut, "wb") as f:
                f.write(render(args.server, cutout_workflow(
                    cup, W, H, prefix + "-cut")))
        except RuntimeError as e:
            print("%s: pile %d: %s" % (a["id"], n, e), file=sys.stderr)
            return None
        # Its alpha: the matte's, inside the mask.
        name = cv["banks"][n]
        atlas = os.path.join(args.pack, "art", "%s.png" % name)
        subprocess.run(["magick", cut, "(", "+clone", "-alpha", "extract",
                        "(", maskfile, "-resize", full, "-crop",
                        "%dx%d+%d+%d" % (W, H, X, Y), "+repage",
                        "-colorspace", "gray", ")", "-compose", "multiply",
                        "-composite", ")", "-alpha", "off", "-compose",
                        "copy-opacity", "-composite", "-depth", "8", atlas],
                       check=True)
        nxt = os.path.join(work, "cave-%d.png" % n)
        subprocess.run(["magick", cur, atlas, "-geometry", "+%d+%d" % (X, Y),
                        "-compose", "over", "-composite", nxt], check=True)
        cur = nxt
        write_sheet(os.path.join(args.pack, "art", "%s.sheet" % name),
                    os.path.basename(atlas), [(W, H)],
                    "Generated by tools/artgen/artgen.py (the cave's "
                    "pile %d)." % n)
        out["spr/" + name] = "art/%s.sheet" % name
        print("%s: pile %d painted" % (a["id"], n))
    shutil.copy(cur, os.path.join(work, "all.png"))
    return {"banks": out, "piles": [" ".join(p["prompt"].split())
                                    for p in cv["piles"]]}


def make_title(args, spec, a, profile, seed):
    """The title's logo (spr/title and spr/title1): its paintings (the
    coin cut out, the gilt) then titles.py's lettering and layout."""
    import titles
    k = spec["pack"]["art_height"] / SCREEN_H
    t = a["title"]
    prefix = "flibustier/" + a["id"].replace("/", "-")
    work = os.path.join(args.pack, "art", "title.work")
    os.makedirs(work, exist_ok=True)
    src = {}
    free(args.server)
    try:
        for n, (key, prompt) in enumerate(sorted(t["paint"].items())):
            path = os.path.join(work, "src-%s.png" % key)
            with open(path, "wb") as f:
                f.write(render(args.server, texture_workflow(
                    spec, profile, seed + n, prompt, 768,
                    prefix + "-" + key)))
            src[key] = path
        free(args.server)
        for key in t.get("cut", []):
            up = upload(args.server, src[key], "flibustier-title-src.png")
            with open(src[key], "wb") as f:
                f.write(render(args.server, cutout_workflow(
                    up, 768, 768, prefix + "-cut")))
    except RuntimeError as e:
        print("%s: %s" % (a["id"], e), file=sys.stderr)
        return None
    font = os.path.expanduser(t["font"])
    halves = titles.compose(src, t, font, k, os.path.join(work, "logo"),
                            os.path.join(work, "half"))
    out = {}
    for half, name in zip(halves, t["banks"]):
        png = os.path.join(args.pack, "art", "%s.png" % name)
        shutil.move(half, png)
        r = subprocess.run(["magick", "identify", "-format", "%w %h", png],
                           capture_output=True, text=True, check=True)
        write_sheet(os.path.join(args.pack, "art", "%s.sheet" % name),
                    os.path.basename(png),
                    [tuple(int(v) for v in r.stdout.split())],
                    "Generated by tools/artgen/artgen.py (titles.py).")
        out["spr/" + name] = "art/%s.sheet" % name
    return {"banks": out, "word": t["word"], "font": t["font"],
            "paint": {k_: " ".join(v.split())
                      for k_, v in t["paint"].items()}}


def make_lettering(args, spec, a, profile, seed):
    """A picture or a one-frame sprite of lettering (the intro's studio
    names): its paintings, then lettering.py's composition."""
    import lettering
    k = spec["pack"]["art_height"] / SCREEN_H
    t = a["lettering"]
    prefix = "flibustier/" + a["id"].replace("/", "-")
    name = os.path.splitext(os.path.basename(a["file"]))[0]
    work = os.path.join(args.pack, "art", name + ".work")
    os.makedirs(work, exist_ok=True)
    src = {}
    free(args.server)
    try:
        for n, (key, prompt) in enumerate(sorted(t.get("paint",
                                                       {}).items())):
            path = os.path.join(work, "src-%s.png" % key)
            with open(path, "wb") as f:
                f.write(render(args.server, texture_workflow(
                    spec, profile, seed + n, prompt, 768,
                    prefix + "-" + key)))
            src[key] = path
    except RuntimeError as e:
        print("%s: %s" % (a["id"], e), file=sys.stderr)
        return None
    sprite = a["file"].endswith(".sheet")
    png = os.path.join(args.pack, "art", name + ".png")
    lettering.compose(src, t, k, work, png, not sprite)
    if sprite:
        r = subprocess.run(["magick", "identify", "-format", "%w %h", png],
                           capture_output=True, text=True, check=True)
        write_sheet(os.path.join(args.pack, a["file"]),
                    os.path.basename(png),
                    [tuple(int(v) for v in r.stdout.split())],
                    "Generated by tools/artgen/artgen.py (lettering.py).")
    return {"lines": [dict(ln) for ln in t["lines"]],
            "paint": {k_: " ".join(v.split())
                      for k_, v in t.get("paint", {}).items()}}


def make_bigs(args, spec, a, profile, seed):
    """The identity screen's big pictures, each its own bank: objects
    painted and cut out (the abilities' symbols), a plate painted whole,
    the captain's portraits in their oval, and the nations' flags waved
    by flags/flag.py from their encounter designs, without a staff."""
    import boards
    k = spec["pack"]["art_height"] / SCREEN_H
    work = os.path.join(args.pack, "art", "big.work")
    os.makedirs(work, exist_ok=True)
    prefix = "flibustier/" + a["id"].replace("/", "-")
    host = args.render_host
    script = os.path.join(HERE, "flags", "flag.py")
    out = {}
    groups = {}
    for n, it in enumerate(a["bigs"]):
        # A small picture "from" a big one is the same painting, at its
        # own size: a frame of a bank of several ("frame").
        pn = n if "from" not in it else [
            i for i, b in enumerate(a["bigs"]) if b["bank"] == it["from"]][0]
        base = a["bigs"][pn]
        name, kind = it["bank"], base["kind"]
        w, h = it["size"]
        W, H = round(w * k), round(h * k)
        # Each painting kept under its description's hash (and the flag
        # model's), so a change paints again only what changed.
        tag = hashlib.sha256(json.dumps([base, seed + pn, profile,
            hashlib.sha256(open(script, "rb").read()).hexdigest()
            if kind == "flag" else "", base.get("crop", 0.1)
            if kind == "flag" else 0],
            sort_keys=True).encode()).hexdigest()[:12]
        src = os.path.join(work, "%s-%s.png" % (base["bank"], tag))
        png = os.path.join(args.pack, "art", "%s.png" % name)
        if "frame" in it:
            png = os.path.join(work, "%s-frame%d.png" % (name, it["frame"]))
            groups.setdefault(name, []).append((it["frame"], png, (W, H)))
        try:
            if os.path.exists(src):
                pass
            elif kind == "flag":
                free(args.server)
                design = [r for r in spec["asset"]
                          if r["id"] == it["design_of"]][0]["flag"]["design"]
                dpath = os.path.join(work, "%s-design.png" % name)
                with open(dpath, "wb") as f:
                    f.write(render(args.server, design_workflow(
                        spec, profile, seed, design, 1200, 800,
                        prefix + "-design")))
                # The cloth only: the photograph's edges keep its
                # backdrop and a pole's sleeve.
                c = it.get("crop", 0.1)
                boards.magick(dpath, "-gravity", "center", "-crop",
                              "%dx%d+0+0" % (round(1200 * (1 - 2 * c)),
                                             round(800 * (1 - 2 * c))),
                              "+repage", dpath)
                remote = "flibustier-flags/big-%s" % name
                subprocess.run(["ssh", "-o", "BatchMode=yes", host,
                                "mkdir -p %s" % remote], check=True,
                               stdin=subprocess.DEVNULL)
                subprocess.run(["scp", "-q", script, dpath,
                                "%s:%s/" % (host, remote)], check=True)
                subprocess.run(["ssh", "-o", "BatchMode=yes", host,
                                "cd %s && rm -rf out && blender -b -P "
                                "flag.py -- --design %s --out out --frame "
                                "900 --frames 1 --no-staff --tilt %g "
                                "> blender.log 2>&1" % (
                                    remote, os.path.basename(dpath),
                                    it.get("tilt", -35))],
                               check=True, stdin=subprocess.DEVNULL)
                subprocess.run(["scp", "-q", "%s:%s/out/0.png" % (
                    host, remote), src], check=True)
            else:
                free(args.server)
                with open(src, "wb") as f:
                    f.write(render(args.server, texture_workflow(
                        spec, profile, seed + n, it["prompt"], 1024,
                        prefix + "-" + name)))
                if kind == "object":
                    free(args.server)
                    up = upload(args.server, src, "flibustier-big-src.png")
                    with open(src, "wb") as f:
                        f.write(render(args.server, cutout_workflow(
                            up, 1024, 1024, prefix + "-cut")))
        except (RuntimeError, subprocess.CalledProcessError) as e:
            print("%s: %s: %s" % (a["id"], name, e), file=sys.stderr)
            return None
        if kind in ("object", "flag"):
            boards.fit(png, src, w, h, k)
        elif kind == "plate":
            # The whole painting, its edges burnt away softly.
            boards.magick(src, "-resize", "%dx%d^" % (W, H), "-gravity",
                          "center", "-extent", "%dx%d" % (W, H), "(",
                          "-size", "%dx%d" % (W, H), "xc:black", "-fill",
                          "white", "-draw", "rectangle %d,%d %d,%d" % (
                              round(W * .05), round(H * .05),
                              round(W * .95), round(H * .95)),
                          "-blur", "0x%g" % (k * 3), ")", "-alpha", "off",
                          "-compose", "copy-opacity", "-composite", png)
        else:		# portrait: in the oval, its rim soft
            boards.magick(src, "-resize", "%dx%d^" % (W, H), "-gravity",
                          "north", "-extent", "%dx%d" % (W, H), "(",
                          "-size", "%dx%d" % (W, H), "xc:black", "-fill",
                          "white", "-draw", "ellipse %d,%d %d,%d 0,360" % (
                              W // 2, H // 2, W // 2 - round(k),
                              H // 2 - round(k)),
                          "-blur", "0x%g" % k, ")", "-alpha", "off",
                          "-compose", "copy-opacity", "-composite", png)
        boards.magick(png, "-depth", 8, png)
        if "frame" in it:
            continue
        write_sheet(os.path.join(args.pack, "art", "%s.sheet" % name),
                    os.path.basename(png), [(W, H)],
                    "Generated by tools/artgen/artgen.py (%s)." % kind)
        out["spr/" + name] = "art/%s.sheet" % name
        print("%s: %s done" % (a["id"], name))
    for name, frames in groups.items():
        frames.sort()
        atlas = boards.strip([f[1] for f in frames],
                             os.path.join(args.pack, "art", name))
        write_sheet(os.path.join(args.pack, "art", "%s.sheet" % name),
                    os.path.basename(atlas), [f[2] for f in frames],
                    "Generated by tools/artgen/artgen.py (the big "
                    "pictures, small).")
        out["spr/" + name] = "art/%s.sheet" % name
        print("%s: %s done" % (a["id"], name))
    return {"banks": out, "items": {
        it["bank"]: " ".join(it.get("prompt", it.get("design_of", "")).split())
        for it in a["bigs"]}}


def make_tmap(args, spec, a, profile, seed):
    """The treasure map's wood, frame, torn edges and marks: the wood and
    paper painted, the pieces in the original's shapes read from the
    player's game data (tmap.py)."""
    import tmap
    k = spec["pack"]["art_height"] / SCREEN_H
    t = a["tmap"]
    binary = os.path.join(os.path.dirname(os.path.dirname(HERE)), "build",
                          "flibustier")
    if not os.path.exists(binary) or not args.data:
        print("%s: needs the built flibustier and --data" % a["id"],
              file=sys.stderr)
        return None
    prefix = "flibustier/" + a["id"].replace("/", "-")
    work = os.path.join(args.pack, "art", "tmap.work")
    os.makedirs(work, exist_ok=True)
    src = {}
    free(args.server)
    try:
        for n, (key, prompt) in enumerate(sorted(t["paint"].items())):
            path = os.path.join(work, "src-%s.png" % key)
            with open(path, "wb") as f:
                f.write(render(args.server, texture_workflow(
                    spec, profile, seed + n, prompt, 1024,
                    prefix + "-" + key)))
            src[key] = path
    except RuntimeError as e:
        print("%s: %s" % (a["id"], e), file=sys.stderr)
        return None
    art = os.path.join(args.pack, "art")
    sizes = tmap.compose(binary, os.path.expanduser(args.data), src, k,
                         os.path.join(work, "pieces"), art)
    write_sheet(os.path.join(art, "treasmap.sheet"), "treasmap.png", sizes,
                "Generated by tools/artgen/artgen.py (tmap.py), in the "
                "original's shapes.")
    return {"files": ["art/wood.png", "art/mapframe.png",
                      "art/treasmap.sheet"],
            "paint": {k_: " ".join(v.split())
                      for k_, v in t["paint"].items()}}


def make_walker(args, spec, a, profile, seed):
    """The landing party on the sailing map (spr/sailmisc frames 0-31):
    the fencers' figure walking in eight directions, rendered on the
    render host (fencers/walker.py), each frame redrawn as a photograph
    and cut to its render's outline; the bank's other frames, which the
    game does not draw, left clear at their sizes."""
    import boards
    k = spec["pack"]["art_height"] / SCREEN_H
    wk = a["walker"]
    host = args.render_host
    remote = "flibustier-walker"
    local = os.path.join(args.pack, "art", "walker.work")
    rw, rh = wk["render"]
    fw, fh = (round(v * k) for v in wk["frame"])
    here = os.path.join(HERE, "fencers")
    try:
        subprocess.run(["ssh", "-o", "BatchMode=yes", host,
                        "mkdir -p %s" % remote], check=True,
                       stdin=subprocess.DEVNULL)
        fetch_library(host)
        subprocess.run(["scp", "-q", os.path.join(here, "fencer.py"),
                        os.path.join(here, "walker.py"),
                        os.path.join(here, "human.py"),
                        os.path.join(HERE, "materials.py"),
                        "%s:%s/" % (host, remote)], check=True)
        subprocess.run(["ssh", "-o", "BatchMode=yes", host,
                        "cd %s && rm -rf out && blender -b -P walker.py -- "
                        "--out out --frame %dx%d --blend model.blend "
                        "> blender.log 2>&1" % (remote, rw, rh)],
                       check=True, stdin=subprocess.DEVNULL)
        # The model itself, beside its provenance, to open in Blender.
        mdir = os.path.join(args.pack, "provenance", "fencers")
        os.makedirs(mdir, exist_ok=True)
        subprocess.run(["scp", "-q", "%s:%s/model.blend" % (host, remote),
                        os.path.join(mdir, "walker.blend")], check=True)
        if os.path.exists(local):
            shutil.rmtree(local)
        os.makedirs(local)
        subprocess.run(["scp", "-q", "-r", "%s:%s/out/." % (host, remote),
                        local], check=True)
    except subprocess.CalledProcessError as e:
        print("%s: blender on %s failed: %s" % (a["id"], host, e),
              file=sys.stderr)
        return None
    prefix = "flibustier/" + a["id"].replace("/", "-")
    prompt = characters(wk.get("prompt", ""), spec)
    frames = []
    if not redrawn(spec, a):
        # The renders as they are, brought to the frame's size.
        for n in range(32):
            path = os.path.join(local, "cut-%d.png" % n)
            boards.magick(os.path.join(local, "%d.png" % n), "-filter",
                          "Lanczos", "-resize", "%dx%d!" % (fw, fh),
                          "-depth", 8, path)
            frames.append((path, (fw, fh)))
    free(args.server)
    try:
        for n in range(32 if redrawn(spec, a) else 0):
            up = upload(args.server, os.path.join(local, "%d.png" % n),
                        "flibustier-walker.png")
            with open(os.path.join(local, "photo-%d.png" % n), "wb") as f:
                f.write(render(args.server, ship_frame_workflow(
                    spec, profile, seed, up, prompt, a.get("denoise", 0.5),
                    rw, rh, prefix, (rw, rh))))
        free(args.server)
        for n in range(32 if redrawn(spec, a) else 0):
            up = upload(args.server, os.path.join(local, "%d.png" % n),
                        "flibustier-walker.png")
            pup = upload(args.server, os.path.join(local, "photo-%d.png" % n),
                         "flibustier-photo.png")
            path = os.path.join(local, "cut-%d.png" % n)
            with open(path, "wb") as f:
                f.write(render(args.server, outline_workflow(
                    pup, up, fw, fh, prefix + "-cut", -2)))
            frames.append((path, (fw, fh)))
    except RuntimeError as e:
        print("%s: %s" % (a["id"], e), file=sys.stderr)
        return None
    for n, (w, h) in enumerate(wk.get("others", [])):
        path = os.path.join(local, "clear-%d.png" % n)
        W, H = max(1, round(w * k)), max(1, round(h * k))
        boards.magick("-size", "%dx%d" % (W, H), "xc:none", path)
        frames.append((path, (W, H)))
    atlas = boards.strip([p for p, _ in frames],
                         os.path.join(args.pack, a["atlas"][:-4]))
    write_sheet(os.path.join(args.pack, a["file"]), os.path.basename(atlas),
                [s for _, s in frames],
                "Generated by tools/artgen/artgen.py (fencers/walker.py).")
    return {"render": [rw, rh], "frame": [fw, fh],
            "blend": "provenance/fencers/walker.blend",
            "redrawn": bool(redrawn(spec, a)), "prompt":
            " ".join(prompt.split()) or None,
            "model_sha256": hashlib.sha256(open(os.path.join(
                here, "walker.py"), "rb").read()).hexdigest()}


def read_manifest(path):
    """pack.ini's [assets] entries, in order: [(id, file)]."""
    out, section = [], None
    if not os.path.exists(path):
        return out
    for line in open(path):
        line = line.strip()
        if line.startswith("["):
            section = line
        elif section == "[assets]" and "=" in line and \
                not line.startswith("#"):
            k, v = (x.strip() for x in line.split("=", 1))
            out.append((k, v))
    return out


def make_layout(args, spec, a, profile, seed):
    """A layout screen composed from paintings (the ship panel): each
    material painted once, then laid out by boards.py's pieces at the
    places the engine draws on."""
    import boards
    k = spec["pack"]["art_height"] / SCREEN_H
    ly = a["layout"]
    prefix = "flibustier/" + a["id"].replace("/", "-")
    work = os.path.join(args.pack, "art", a["id"].replace("/", "-") +
                        ".work")
    os.makedirs(work, exist_ok=True)
    src = {}
    free(args.server)
    try:
        for n, (key, prompt) in enumerate(sorted(ly["paint"].items())):
            path = os.path.join(work, "src-%s.png" % key)
            if not os.path.exists(path):
                g = texture_workflow(spec, profile, seed + n, prompt,
                                     1024, prefix + "-" + key)
                with open(path, "wb") as f:
                    f.write(render(args.server, g))
            src[key] = path
    except RuntimeError as e:
        print("%s: %s" % (a["id"], e), file=sys.stderr)
        return None
    W, H = round(SCREEN_W * k), round(SCREEN_H * k)
    out = os.path.join(args.pack, a["file"])
    boards.magick(src[ly["back"]], "-resize", "%dx%d^" % (W, H), "-gravity",
                  "center", "-extent", "%dx%d" % (W, H), out)
    for piece in ly["pieces"]:
        x, y, w, h = piece["rect"]
        tmp = os.path.join(work, "piece.png")
        if piece.get("picture"):
            # A view (the sea and sky behind a ship), in a thin rim.
            boards.magick(src[piece["picture"]], "-resize", "%dx%d^" % (
                round(w * k), round(h * k)), "-gravity", piece.get(
                    "gravity", "center"), "-extent", "%dx%d" % (
                    round(w * k), round(h * k)), tmp)
            if piece.get("rim"):
                rim = os.path.join(work, "rim.png")
                boards.panel(rim, src[piece["rim"]], w + 4, h + 4, k,
                             rim=src[piece["rim"]], inset=True)
                boards.magick(rim, tmp, "-geometry", "+%d+%d" % (
                    round(2 * k), round(2 * k)), "-composite", tmp)
                x, y = x - 2, y - 2
        elif piece.get("arrow"):
            # A brass arrow pointing right, with a soft shadow.
            W2, H2 = round(w * k), round(h * k)
            tri = "polygon 0,0 %d,%d 0,%d" % (W2 - 1, H2 // 2, H2 - 1)
            face = os.path.join(work, "arrow-face.png")
            mask = os.path.join(work, "arrow-mask.png")
            boards.magick(src[piece["arrow"]], "-resize", "%dx%d^" % (
                W2, H2), "-gravity", "center", "-extent", "%dx%d" % (
                W2, H2), "-alpha", "off", face)
            boards.magick("-size", "%dx%d" % (W2, H2), "xc:black", "-fill",
                          "white", "-draw", tri, mask)
            boards.magick(face, mask, "-alpha", "off", "-compose",
                          "copy-opacity", "-composite", tmp)
        else:
            boards.panel(tmp, src[piece["face"]], w, h, k,
                         rim=src.get(piece.get("rim")),
                         inset=piece.get("inset", False))
        boards.magick(out, tmp, "-geometry", "+%d+%d" % (round(x * k),
                      round(y * k)), "-composite", out)
    boards.magick(out, "-depth", 8, out)
    return {"paint": {k_: " ".join(v.split())
                      for k_, v in ly["paint"].items()}}


def make_citymap(args, spec, a, profile, seed):
    """The Map of the Caribbean: its land the world's own, read by
    helpers/landmask.c from the pack being made (its open map/<year>)
    over the player's game data (never stored), painted and laid out
    with its panels by citymap.py."""
    import citymap
    k = spec["pack"]["art_height"] / SCREEN_H
    cm = a["citymap"]
    root = os.path.dirname(os.path.dirname(HERE))
    work = os.path.join(args.pack, "art", "citymap.work")
    os.makedirs(work, exist_ok=True)
    helper = os.path.join(work, "landmask")
    land = os.path.join(work, "land.pgm")
    try:
        subprocess.run(["cc", "-I", os.path.join(root, "src"), "-o", helper,
                        os.path.join(HERE, "helpers", "landmask.c"),
                        os.path.join(root, "build", "libflibustier_core.a"),
                        "-L/usr/local/lib", "-lfreetype", "-lz", "-lm",
                        "-lc++"], check=True)
        subprocess.run([helper, os.path.expanduser(args.data or ""), land,
                        str(round(citymap.MAP_W * k)),
                        str(round(citymap.MAP_H * k)), args.pack],
                       check=True)
    except subprocess.CalledProcessError as e:
        print("%s: land mask: %s" % (a["id"], e), file=sys.stderr)
        return None
    prefix = "flibustier/" + a["id"].replace("/", "-")
    src = {}
    free(args.server)
    try:
        for n, (key, prompt) in enumerate(sorted(cm["paint"].items())):
            path = os.path.join(work, "src-%s.png" % key)
            if not os.path.exists(path):
                with open(path, "wb") as f:
                    f.write(render(args.server, texture_workflow(
                        spec, profile, seed + n, prompt, 1024,
                        prefix + "-" + key)))
                if key in cm.get("cut", []):
                    free(args.server)
                    up = upload(args.server, path, "flibustier-cm.png")
                    with open(path, "wb") as f:
                        f.write(render(args.server, cutout_workflow(
                            up, 1024, 1024, prefix + "-cut")))
            src[key] = path
    except RuntimeError as e:
        print("%s: %s" % (a["id"], e), file=sys.stderr)
        return None
    citymap.compose(land, src, os.path.join(args.pack, cm["font"]), k,
                    os.path.join(work, "pieces"),
                    os.path.join(args.pack, a["file"]))
    return {"paint": {k_: " ".join(v.split())
                      for k_, v in cm["paint"].items()}}


def make_fightflags(args, spec, a, profile, seed):
    """The sword fight's pieces (spr/miscswrd): its flags, eight waving
    frames each (the player's Jolly Roger, then Spain, England, France,
    Holland and the pirates), from the encounter flags' designs, waved
    by flags/flag.py without a staff, at the original frames' size; the
    other frames (the fight's small marks) kept clear at their sizes."""
    import boards
    k = spec["pack"]["art_height"] / SCREEN_H
    ff = a["fightflags"]
    host = args.render_host
    script = os.path.join(HERE, "flags", "flag.py")
    work = os.path.join(args.pack, "art", "miscswrd.work")
    os.makedirs(work, exist_ok=True)
    prefix = "flibustier/" + a["id"].replace("/", "-")
    fw, fh = (round(v * k) for v in ff["frame"])
    frames = []
    for n, (w, h) in enumerate(ff["before"]):
        path = os.path.join(work, "clear-%d.png" % n)
        boards.magick("-size", "%dx%d" % (max(1, round(w * k)),
                                          max(1, round(h * k))), "xc:none",
                      path)
        frames.append((path, (max(1, round(w * k)), max(1, round(h * k)))))
    for n, of in enumerate(ff["flags"]):
        design = [r for r in spec["asset"] if r["id"] == of][0]["flag"]
        name = of.replace("/", "-")
        dpath = os.path.join(work, "%s-design.png" % name)
        remote = "flibustier-flags/fight-%s" % name
        try:
            if not os.path.exists(dpath):
                free(args.server)
                with open(dpath, "wb") as f:
                    f.write(render(args.server, design_workflow(
                        spec, profile, seed, design["design"], 1200, 800,
                        prefix + "-design")))
                if design.get("inks"):
                    flatten_inks(dpath, design["inks"])
                boards.magick(dpath, "-gravity", "center", "-crop",
                              "960x640+0+0", "+repage", dpath)
            subprocess.run(["ssh", "-o", "BatchMode=yes", host,
                            "mkdir -p %s" % remote], check=True,
                           stdin=subprocess.DEVNULL)
            subprocess.run(["scp", "-q", script, dpath,
                            "%s:%s/" % (host, remote)], check=True)
            subprocess.run(["ssh", "-o", "BatchMode=yes", host,
                            "cd %s && rm -rf out && blender -b -P flag.py -- "
                            "--design %s --out out --frame 450 --no-staff "
                            "--tilt 0 %s > blender.log 2>&1" % (
                                remote, os.path.basename(dpath),
                                "--torn" if design.get("torn") else "")],
                           check=True, stdin=subprocess.DEVNULL)
            subprocess.run(["scp", "-q", "%s:%s/out/*.png" % (host, remote),
                            work + "/"], check=True)
        except (RuntimeError, subprocess.CalledProcessError) as e:
            print("%s: %s: %s" % (a["id"], of, e), file=sys.stderr)
            return None
        for m in range(8):
            path = os.path.join(work, "%s-%d.png" % (name, m))
            boards.magick(os.path.join(work, "%d.png" % m), "-trim",
                          "+repage", "-resize", "%dx%d!" % (fw, fh), path)
            frames.append((path, (fw, fh)))
    atlas = boards.strip([p for p, _ in frames],
                         os.path.join(args.pack, a["atlas"][:-4]))
    write_sheet(os.path.join(args.pack, a["file"]),
                os.path.basename(atlas), [s for _, s in frames],
                "Generated by tools/artgen/artgen.py (flags/flag.py).")
    return {"flags": ff["flags"], "frame": [fw, fh]}


def make_flown(args, spec, a, profile, seed):
    """Another asset's picture with a nation's flag flown over it (the
    town under each nation): the flag's design waved upright on its
    staff by flags/flag.py in a "wind" from 0 to 1, one frame, set with
    the staff's top at "staff", the whole "width" wide and the staff
    down to "foot"; "flip" flies it to the left of its staff (its back
    seen)."""
    import boards
    k = spec["pack"]["art_height"] / SCREEN_H
    fl = a["flown"]
    on = [r for r in spec["asset"] if r["id"] == fl["on"]][0]
    base = os.path.join(args.pack, on["file"])
    if not os.path.exists(base):
        print("%s: no %s yet" % (a["id"], on["file"]), file=sys.stderr)
        return None
    design = [r for r in spec["asset"]
              if r["id"] == fl["design_of"]][0]["flag"]
    host = args.render_host
    script = os.path.join(HERE, "flags", "flag.py")
    work = os.path.join(args.pack, "art", "flown.work")
    os.makedirs(work, exist_ok=True)
    name = fl["design_of"].replace("/", "-")
    dpath = os.path.join(work, "%s-design.png" % name)
    remote = "flibustier-flags/flown-%s" % name
    flag = os.path.join(work, "%s-0.png" % name)
    try:
        if not os.path.exists(dpath):
            free(args.server)
            with open(dpath, "wb") as f:
                f.write(render(args.server, design_workflow(
                    spec, profile, seed, design["design"], 1200, 800,
                    "flibustier/" + a["id"].replace("/", "-") +
                    "-design")))
            if design.get("inks"):
                flatten_inks(dpath, design["inks"])
            boards.magick(dpath, "-gravity", "center", "-crop",
                          "960x640+0+0", "+repage", dpath)
        subprocess.run(["ssh", "-o", "BatchMode=yes", host,
                        "mkdir -p %s" % remote], check=True,
                       stdin=subprocess.DEVNULL)
        subprocess.run(["scp", "-q", script, dpath,
                        "%s:%s/" % (host, remote)], check=True)
        subprocess.run(["ssh", "-o", "BatchMode=yes", host,
                        "cd %s && rm -rf out && blender -b -P flag.py -- "
                        "--design %s --out out --frame 900 --frames 1 "
                        "--tilt 0 --wind %g %s > blender.log 2>&1" % (
                            remote, os.path.basename(dpath),
                            fl.get("wind", 1),
                            "--torn" if design.get("torn") else "")],
                       check=True, stdin=subprocess.DEVNULL)
        subprocess.run(["scp", "-q", "%s:%s/out/0.png" % (host, remote),
                        flag], check=True)
    except (RuntimeError, subprocess.CalledProcessError) as e:
        print("%s: %s" % (a["id"], e), file=sys.stderr)
        return None
    # The render's staff runs off its bottom edge: the flag scaled to its
    # width, the staff's last row drawn on (or cut) down to the foot;
    # dulled and softened a little, as the photograph's distance is.
    x, y = fl["staff"]
    w = round(fl["width"] * k)
    h = round((fl["foot"] - y) * k)
    boards.magick(flag, "-trim", "+repage", "-resize", "%dx" % w,
                  *(["-flop"] if fl.get("flip") else []), flag)
    r = subprocess.run(["magick", "identify", "-format", "%h", flag],
                       capture_output=True, text=True, check=True)
    fh = int(r.stdout)
    if fh < h:
        boards.magick(flag, "(", "+clone", "-gravity", "south", "-crop",
                      "%dx1+0+0" % w, "+repage", "-scale",
                      "%dx%d!" % (w, h - fh), ")", "-append", flag)
    boards.magick(base, "(", flag, "-gravity", "northwest", "-crop",
                  "%dx%d+0+0" % (w, h), "+repage", "-modulate", "95,80",
                  "-blur", "0x0.6", ")", "-gravity",
                  "northwest", "-geometry", "+%d+%d" % (
                      round(x * k) - (w - 2 if fl.get("flip") else 1),
                      round(y * k)), "-composite",
                  "-depth", "8", os.path.join(args.pack, a["file"]))
    return {"on": fl["on"], "on_sha256": hashlib.sha256(
                open(base, "rb").read()).hexdigest(),
            "design_of": fl["design_of"],
            "design": " ".join(design["design"].split()),
            "staff": fl["staff"], "width": fl["width"], "foot": fl["foot"],
            "wind": fl.get("wind", 1), "flip": fl.get("flip", False),
            "model": "flags/flag.py"}


def make_uimisc(args, spec, a, profile, seed):
    """The interface's small pieces, one bank a language (ui/misc,
    ui/miscf, ui/miscg): the map's marks (a town's dot, the chosen town's
    and the player's), drawn; and the plaques shown over the screen
    (Quit, Paused, a blank one, Click to continue, Disembark), a painted
    plank with brass nails, their words set in the pack's font."""
    import boards
    k = spec["pack"]["art_height"] / SCREEN_H
    um = a["uimisc"]
    prefix = "flibustier/" + a["id"].replace("/", "-")
    work = os.path.join(args.pack, "art", "uimisc.work")
    os.makedirs(work, exist_ok=True)
    src = {}
    free(args.server)
    try:
        for n, (key, prompt) in enumerate(sorted(um["paint"].items())):
            path = os.path.join(work, "src-%s.png" % key)
            if not os.path.exists(path):
                with open(path, "wb") as f:
                    f.write(render(args.server, texture_workflow(
                        spec, profile, seed + n, prompt, 768,
                        prefix + "-" + key)))
            src[key] = path
    except RuntimeError as e:
        print("%s: %s" % (a["id"], e), file=sys.stderr)
        return None
    font = os.path.join(args.pack, um["font"])
    px = boards.px

    def mark(path, kind):
        S = px(8, k)
        c = S / 2
        if kind == "dot":	# a town: a red wax dot with a gold rim
            boards.magick("-size", "%dx%d" % (S, S), "xc:none", "-fill",
                          "#d8a840", "-draw", "circle %g,%g %g,%g" % (
                              c, c, c, c * 0.25), "-fill", "#a01808",
                          "-draw", "circle %g,%g %g,%g" % (c, c, c, c * 0.45),
                          "-fill", "#f0c060", "-draw", "circle %g,%g %g,%g" % (
                              c * 0.8, c * 0.8, c * 0.8, c * 0.65), path)
        elif kind == "chosen":	# the chosen town: a bigger, bright dot
            boards.magick("-size", "%dx%d" % (S, S), "xc:none", "-fill",
                          "#ffd860", "-draw", "circle %g,%g %g,%g" % (
                              c, c, c, 0.2), "-fill", "#e02010", "-draw",
                          "circle %g,%g %g,%g" % (c, c, c, c * 0.3), path)
        else:			# the player: a dark ring
            boards.magick("-size", "%dx%d" % (S, S), "xc:none", "-fill",
                          "none", "-stroke", "#101010", "-strokewidth",
                          str(max(1, round(k * 1.1))), "-draw",
                          "circle %g,%g %g,%g" % (c, c, c, c * 0.25), path)

    def plaque(path, words, w, h):
        base = os.path.join(work, "plank.png")
        boards.panel(base, src["plank"], w, h, k, rim=src["plank"])
        # A pale golden plank, so dark words read on it.
        boards.magick(base, "-modulate", "175,80", "-level", "8%,100%",
                      base)
        # Brass nails at the corners.
        r = max(2, round(1.6 * k))
        for x, y in ((3, 3), (w - 3, 3), (3, h - 3), (w - 3, h - 3)):
            boards.magick(base, "-fill", "#c8a050", "-stroke", "#5a4010",
                          "-strokewidth", "1", "-draw",
                          "circle %d,%d %d,%d" % (px(x, k), px(y, k),
                                                  px(x, k) + r, px(y, k)),
                          base)
        if words:
            # The same letter size on every plaque: a line's height is
            # at most 40% of the plaque's.
            n = words.count("\n") + 1
            boards.lettered(path, base, words, "#241206", w * 0.92,
                            min(h * 0.9, h * 0.42 * n), font, k, lines=n,
                            shadow=False)
        else:
            shutil.copy(base, path)

    out = {}
    for lang, words in um["words"].items():
        frames = []
        p = os.path.join(work, "%s-0.png" % lang)
        boards.magick("-size", "%dx%d" % (px(2, k), px(2, k)), "xc:none", p)
        frames.append(p)
        for n, kind in enumerate(("dot", "chosen", "player")):
            p = os.path.join(work, "%s-%d.png" % (lang, n + 1))
            mark(p, kind)
            frames.append(p)
        for n, (key, (w, h)) in enumerate(zip(
                ("quit", "paused", "", "continue", "disembark"),
                ((103, 31), (103, 31), (97, 29), (103, 31), (103, 31)))):
            p = os.path.join(work, "%s-%d.png" % (lang, n + 4))
            plaque(p, words.get(key, "") if key else "", w, h)
            frames.append(p)
        name = um["banks"][lang]
        atlas = boards.strip(frames, os.path.join(args.pack, "art", name))
        sizes = []
        for p in frames:
            r = subprocess.run(["magick", "identify", "-format", "%w %h", p],
                               capture_output=True, text=True, check=True)
            sizes.append(tuple(int(v) for v in r.stdout.split()))
        write_sheet(os.path.join(args.pack, "art", "%s.sheet" % name),
                    os.path.basename(atlas), sizes,
                    "Generated by tools/artgen/artgen.py (the interface's "
                    "pieces).")
        out["ui/" + name] = "art/%s.sheet" % name
    return {"banks": out, "words": um["words"]}


def make_btlmisc(args, spec, a, profile, seed):
    """The battles' small sprites (cur/sbtlmisc, cur/lbtlmisc): their
    pieces painted and cut out, placed where the original's are, read
    from the player's game data at make time (btlmisc.py)."""
    import btlmisc
    k = spec["pack"]["art_height"] / SCREEN_H
    bm = a["btlmisc"]
    binary = os.path.join(os.path.dirname(os.path.dirname(HERE)), "build",
                          "flibustier")
    if not args.data or not os.path.exists(binary):
        print("%s: needs the built flibustier and --data" % a["id"],
              file=sys.stderr)
        return None
    prefix = "flibustier/" + a["id"].replace("/", "-")
    work = os.path.join(args.pack, "art", "btlmisc.work")
    os.makedirs(work, exist_ok=True)
    src = {}
    try:
        for n, (key, prompt) in enumerate(sorted(bm["paint"].items())):
            path = os.path.join(work, "src-%s.png" % key)
            src[key] = path
            if os.path.exists(path):
                continue
            free(args.server)
            with open(path, "wb") as f:
                f.write(render(args.server, texture_workflow(
                    spec, profile, seed + n, prompt, 768,
                    prefix + "-" + key)))
            if key in bm.get("glow", []):
                # Spray and fire: painted on black, their brightness
                # their opacity (a matte keeps only solid shapes).
                subprocess.run(["magick", path, "(", "+clone",
                                "-colorspace", "gray", "-level", "8%,70%",
                                ")", "-alpha", "off", "-compose",
                                "copy-opacity", "-composite", path],
                               check=True)
                continue
            free(args.server)
            up = upload(args.server, path, "flibustier-btl.png")
            with open(path, "wb") as f:
                f.write(render(args.server, cutout_workflow(
                    up, 768, 768, prefix + "-cut")))
    except RuntimeError as e:
        print("%s: %s" % (a["id"], e), file=sys.stderr)
        return None
    art = os.path.join(args.pack, "art")
    sea, land = btlmisc.compose(binary, os.path.expanduser(args.data), src,
                                k, os.path.join(work, "pieces"), art)
    write_sheet(os.path.join(art, "sbtlmisc.sheet"), "sbtlmisc.png", sea,
                "Generated by tools/artgen/artgen.py (btlmisc.py).")
    write_sheet(os.path.join(art, "lbtlmisc.sheet"), "lbtlmisc.png", land,
                "Generated by tools/artgen/artgen.py (btlmisc.py).")
    return {"paint": {k_: " ".join(v.split())
                      for k_, v in bm["paint"].items()}}


def write_manifest(pack, spec):
    """pack.ini: merged, not rewritten.  The entries already there stay
    in their order (another spec, another session, or a hand may have
    added them); the assets of this spec that have art in the pack are
    set or added; an entry is dropped only when its file is gone."""
    lines = ["# Generated by tools/artgen/artgen.py; see provenance/.",
             "[pack]", "name = " + spec["pack"]["name"],
             "license = " + spec["pack"]["license"],
             "art_height = %d" % spec["pack"]["art_height"], "",
             "[assets]"]
    mine = []
    for a in spec["asset"]:
        if a.get("tmap"):
            for ident, f in (("pic/wood", "art/wood.png"),
                             ("pic/mapframe", "art/mapframe.png"),
                             ("spr/treasmap", "art/treasmap.sheet")):
                if os.path.exists(os.path.join(pack, f)):
                    mine.append("%s = %s" % (ident, f))
            continue
        if a.get("bigs"):
            for name in dict.fromkeys(it["bank"] for it in a["bigs"]):
                sheet = "art/%s.sheet" % name
                if os.path.exists(os.path.join(pack, sheet)):
                    mine.append("spr/%s = %s" % (name, sheet))
            continue
        if a.get("btlmisc"):
            for ident, f in (("cur/sbtlmisc", "art/sbtlmisc.sheet"),
                             ("cur/lbtlmisc", "art/lbtlmisc.sheet")):
                if os.path.exists(os.path.join(pack, f)):
                    mine.append("%s = %s" % (ident, f))
            continue
        if a.get("uimisc"):
            for name in a["uimisc"]["banks"].values():
                sheet = "art/%s.sheet" % name
                if os.path.exists(os.path.join(pack, sheet)):
                    mine.append("ui/%s = %s" % (name, sheet))
            continue
        if a.get("cave") or a.get("title"):
            for name in (a.get("cave") or a.get("title"))["banks"]:
                sheet = "art/%s.sheet" % name
                if os.path.exists(os.path.join(pack, sheet)):
                    mine.append("spr/%s = %s" % (name, sheet))
            continue
        if a.get("board"):
            for banks in list(a["board"]["banks"].values()) + list(
                    a["board"].get("take_banks", {}).values()):
                for name in banks.values():
                    sheet = "art/%s.sheet" % name
                    if os.path.exists(os.path.join(pack, sheet)):
                        mine.append("spr/%s = %s" % (name, sheet))
            continue
        if a.get("fencer"):
            for gid, gfile in a["fencer"]["groups"].values():
                if os.path.exists(os.path.join(pack, gfile)):
                    mine.append("%s = %s" % (gid, gfile))
            continue
        if os.path.exists(os.path.join(pack, a["file"])):
            mine.append("%s = %s" % (a["id"], a["file"]))
    path = os.path.join(pack, "pack.ini")
    mine = [tuple(x.strip() for x in m.split("=", 1)) for m in mine]
    have = dict(mine)
    entries, seen = [], set()
    for ident, f in read_manifest(path):
        if ident in seen:
            continue
        seen.add(ident)
        f = have.get(ident, f)
        if os.path.exists(os.path.join(pack, f)):
            entries.append((ident, f))
    for ident, f in mine:
        if ident not in seen:
            seen.add(ident)
            entries.append((ident, f))
    lines += ["%s = %s" % e for e in entries]
    with open(os.path.join(pack, "pack.ini"), "w") as f:
        f.write("\n".join(lines) + "\n")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--server", default=os.environ.get(
        "FLIBUSTIER_COMFYUI", "http://framework2:8188"))
    ap.add_argument("--pack", default=os.path.expanduser(
        "~/flibustier-openpack"))
    ap.add_argument("--spec", default=os.path.join(HERE, "assets.toml"))
    ap.add_argument("--draft", action="store_true")
    ap.add_argument("--seed", type=int, help="override every seed")
    ap.add_argument("--keep-loaded", action="store_true",
                    help="do not unload ComfyUI's models first (for runs "
                    "back to back)")
    ap.add_argument("--render-host", default="framework2",
                    help="where Blender renders the ship models (ssh)")
    ap.add_argument("--world", default="~/stmalo-originals/world-export",
                    help="flibustier --export-world of the original data, "
                    "for the map tiles' coastlines (local only)")
    ap.add_argument("--data", default=os.environ.get("FLIBUSTIER_DATA"),
                    help="the player's game data, to read shapes from "
                    "(never stored in the pack)")
    ap.add_argument("ids", nargs="*")
    args = ap.parse_args()
    args.server = reachable(args.server)

    with open(args.spec, "rb") as f:
        spec = tomllib.load(f)
    height = spec["pack"]["art_height"]
    assets = [a for a in spec["asset"] if not args.ids or
              a["id"] in args.ids]
    missing = set(args.ids) - {a["id"] for a in assets}
    if missing:
        sys.exit("unknown asset: " + ", ".join(sorted(missing)))
    profile = "draft" if args.draft else "final"
    failed = []
    system = json.loads(call(args.server, "/system_stats"))["system"]
    # The colour grade's reference: a picture already in the pack.
    grade = spec.get("grade")
    if grade:
        ref = os.path.join(args.pack, grade["reference"])
        if os.path.exists(ref):
            grade["uploaded"] = upload(args.server, ref,
                                       "flibustier-grade-reference.png")
            grade["reference_sha256"] = hashlib.sha256(
                open(ref, "rb").read()).hexdigest()
        else:
            print("no grade reference %s yet: rendering ungraded" % ref,
                  file=sys.stderr)
    # Other workflows' models may fill the GPU: let ComfyUI unload them.
    if not args.keep_loaded:
        call(args.server, "/free", json.dumps(
            {"unload_models": True, "free_memory": True}).encode())
    for a in assets:
        lw, lh = a.get("size", [SCREEN_W, SCREEN_H])
        ow = round(lw * height / SCREEN_H)
        oh = round(lh * height / SCREEN_H)
        seed = args.seed if args.seed is not None else a["seed"]
        if a.get("bevel"):
            # Its light and its shadow, in that order.
            a["uploaded_bevel"] = [
                (upload(args.server, os.path.join(HERE, a["bevel"] + side
                                                  + ".png"),
                        "flibustier-bevel-" + os.path.basename(a["bevel"])
                        + side + ".png"), colour)
                for side, colour in (("-light", 0xFFE7A8),
                                     ("-dark", 0x241204))]
        if a.get("inpaint_mask"):
            a["uploaded_inpaint"] = upload(
                args.server, os.path.join(HERE, a["inpaint_mask"]),
                "flibustier-inpaint-" + os.path.basename(a["inpaint_mask"]))
        for piece in a.get("pieces", []):
            if piece.get("mask"):
                piece["uploaded_mask"] = upload(
                    args.server, os.path.join(HERE, piece["mask"]),
                    "flibustier-piece-" + os.path.basename(piece["mask"]))
        if a.get("control"):
            a["uploaded_control"] = upload(
                args.server, os.path.join(HERE, a["control"]),
                "flibustier-control-" + os.path.basename(a["control"]))
        name = os.path.splitext(a["file"])[0]
        if a.get("frames_of"):
            # Some headings of another asset's ship (the sea battle's two
            # banks): a .sheet on that asset's atlas, nothing rendered.
            src = [r for r in spec["asset"] if r["id"] == a["frames_of"]][0]
            if not os.path.exists(os.path.join(args.pack, src["atlas"])):
                print("%s: no %s yet" % (a["id"], src["atlas"]),
                      file=sys.stderr)
                failed.append(a["id"])
                continue
            fw, fh = (round(v * height / SCREEN_H)
                      for v in src["ship"].get("frame", [78, 58]))
            first, count = a["frames"]
            lines = ["# Headings %d to %d of %s." % (
                first, first + count - 1, src["id"]), "[sheet]",
                "image = " + os.path.basename(src["atlas"]), "", "[frames]"]
            for n in range(count):
                lines.append("%d = %d 0 %d %d" % (n, (first + n) * fw, fw,
                                                   fh))
            with open(os.path.join(args.pack, a["file"]), "w") as f:
                f.write("\n".join(lines) + "\n")
            print("%s: headings %d-%d of %s" % (a["id"], first,
                                                first + count - 1, src["id"]))
            continue
        if a.get("tiles"):
            t0 = time.time()
            record = make_tiles(args, spec, a, profile, seed)
            if record is None:
                failed.append(a["id"])
                continue
            pdir = os.path.join(args.pack, "provenance")
            os.makedirs(pdir, exist_ok=True)
            with open(os.path.join(pdir, a["id"].replace("/", "-") +
                                   ".json"), "w") as f:
                json.dump(dict(record, id=a["id"], file=a["file"],
                               atlas=a["atlas"], profile=profile, seed=seed,
                               composer="tilemaker.py",
                               comfyui=system.get("comfyui_version"),
                               generator=git_rev(),
                               date=datetime.datetime.now(
                                   datetime.timezone.utc)
                               .isoformat(timespec="seconds")), f, indent=2)
                f.write("\n")
            print("%s: %s tiles, %.0f s" % (a["id"], profile,
                                            time.time() - t0))
            continue
        if a.get("sprite") or a.get("clouds") or a.get("board") or \
                a.get("cave") or a.get("title") or a.get("bigs") or \
                a.get("tmap") or a.get("walker") or a.get("lettering") or \
                a.get("layout") or a.get("citymap") or \
                a.get("fightflags") or a.get("uimisc") or \
                a.get("btlmisc") or a.get("flown"):
            t0 = time.time()
            record = (make_clouds if a.get("clouds") else make_board
                      if a.get("board") else make_cave if a.get("cave")
                      else make_title if a.get("title")
                      else make_lettering if a.get("lettering")
                      else make_bigs if a.get("bigs")
                      else make_tmap if a.get("tmap")
                      else make_walker if a.get("walker")
                      else make_layout if a.get("layout")
                      else make_citymap if a.get("citymap")
                      else make_fightflags if a.get("fightflags")
                      else make_uimisc if a.get("uimisc")
                      else make_btlmisc if a.get("btlmisc")
                      else make_flown if a.get("flown")
                      else make_sprite)(args, spec, a, profile, seed)
            if record is None:
                failed.append(a["id"])
                continue
            pdir = os.path.join(args.pack, "provenance")
            os.makedirs(pdir, exist_ok=True)
            with open(os.path.join(pdir, a["id"].replace("/", "-") +
                                   ".json"), "w") as f:
                json.dump(dict(record, id=a["id"], file=a["file"],
                               profile=profile, seed=seed,
                               comfyui=system.get("comfyui_version"),
                               generator=git_rev(),
                               date=datetime.datetime.now(
                                   datetime.timezone.utc)
                               .isoformat(timespec="seconds")), f, indent=2)
                f.write("\n")
            print("%s: %s sprite, %.0f s" % (a["id"], profile,
                                            time.time() - t0))
            continue
        if a.get("flag"):
            t0 = time.time()
            record = make_flag(args, spec, a, profile, seed)
            if record is None:
                failed.append(a["id"])
                continue
            pdir = os.path.join(args.pack, "provenance")
            os.makedirs(pdir, exist_ok=True)
            with open(os.path.join(pdir, a["id"].replace("/", "-") +
                                   ".json"), "w") as f:
                json.dump(dict(record, id=a["id"], file=a["file"],
                               profile=profile, seed=seed,
                               model="flags/flag.py",
                               comfyui=system.get("comfyui_version"),
                               generator=git_rev(),
                               date=datetime.datetime.now(
                                   datetime.timezone.utc)
                               .isoformat(timespec="seconds")), f, indent=2)
                f.write("\n")
            print("%s: %s flag, %.0f s" % (a["id"], profile,
                                          time.time() - t0))
            continue
        if a.get("portrait"):
            t0 = time.time()
            record = make_portrait(args, spec, a, profile, seed)
            if record is None:
                failed.append(a["id"])
                continue
            pdir = os.path.join(args.pack, "provenance")
            os.makedirs(pdir, exist_ok=True)
            with open(os.path.join(pdir, a["id"].replace("/", "-") +
                                   ".json"), "w") as f:
                json.dump(dict(record, id=a["id"], file=a["file"],
                               profile=profile, seed=seed,
                               model="ships/ship.py",
                               comfyui=system.get("comfyui_version"),
                               generator=git_rev(),
                               date=datetime.datetime.now(
                                   datetime.timezone.utc)
                               .isoformat(timespec="seconds")), f, indent=2)
                f.write("\n")
            print("%s: %s portrait, %.0f s" % (a["id"], profile,
                                              time.time() - t0))
            continue
        if a.get("fencer"):
            t0 = time.time()
            record = make_fencer(args, spec, a, profile, seed)
            if record is None:
                failed.append(a["id"])
                continue
            pdir = os.path.join(args.pack, "provenance")
            os.makedirs(pdir, exist_ok=True)
            with open(os.path.join(pdir, a["id"].replace("/", "-") +
                                   ".json"), "w") as f:
                json.dump(dict(record, id=a["id"], profile=profile,
                               seed=seed, model="fencers/fencer.py",
                               comfyui=system.get("comfyui_version"),
                               generator=git_rev(),
                               date=datetime.datetime.now(
                                   datetime.timezone.utc)
                               .isoformat(timespec="seconds")), f, indent=2)
                f.write("\n")
            print("%s: %s fencer, %.0f s" % (a["id"], profile,
                                            time.time() - t0))
            continue
        if a.get("ship") or a.get("damage"):
            t0 = time.time()
            record = make_ship(args, spec, a, profile, seed)
            if record is None:
                failed.append(a["id"])
                continue
            pdir = os.path.join(args.pack, "provenance")
            os.makedirs(pdir, exist_ok=True)
            with open(os.path.join(pdir, a["id"].replace("/", "-") +
                                   ".json"), "w") as f:
                json.dump(dict(record, id=a["id"], file=a["file"],
                               atlas=a["atlas"], profile=profile, seed=seed,
                               prompt=a["prompt"].strip(),
                               model="ships/ship.py",
                               comfyui=system.get("comfyui_version"),
                               generator=git_rev(),
                               date=datetime.datetime.now(
                                   datetime.timezone.utc)
                               .isoformat(timespec="seconds")), f, indent=2)
                f.write("\n")
            print("%s: %s ship, %d frames, %.0f s" % (
                a["id"], profile, record["frames"], time.time() - t0))
            continue
        if a.get("from"):
            t0 = time.time()
            record = derive_sheet(args, spec, a, profile, seed)
            if record is None:
                failed.append(a["id"])
                continue
            pdir = os.path.join(args.pack, "provenance")
            os.makedirs(pdir, exist_ok=True)
            with open(os.path.join(pdir, a["id"].replace("/", "-") +
                                   ".json"), "w") as f:
                json.dump({
                    "id": a["id"], "file": a["file"], "atlas": a["atlas"],
                    "from": a["from"], "profile": profile, "seed": seed,
                    "frames": record, "comfyui": system.get(
                        "comfyui_version"), "generator": git_rev(),
                    "date": datetime.datetime.now(datetime.timezone.utc)
                    .isoformat(timespec="seconds")}, f, indent=2)
                f.write("\n")
            print("%s: %s from %s, %d frames, %.0f s" % (
                a["id"], profile, a["from"], len(a["frames"]),
                time.time() - t0))
            continue
        graph = workflow(a, profile, seed, ow, oh, "flibustier/" +
                         name.replace("/", "-"), spec)
        if a.get("pictures"):
            t0 = time.time()
            record = make_pictures(args, spec, a, profile, seed, height)
            if record is None:
                failed.append(a["id"])
                continue
            pdir = os.path.join(args.pack, "provenance")
            os.makedirs(pdir, exist_ok=True)
            with open(os.path.join(pdir, a["id"].replace("/", "-") +
                                   ".json"), "w") as f:
                json.dump({"id": a["id"], "file": a["file"],
                           "atlas": a["atlas"], "profile": profile,
                           "seed": seed, "prompts": record,
                           "comfyui": system.get("comfyui_version"),
                           "generator": git_rev(),
                           "date": datetime.datetime.now(
                               datetime.timezone.utc)
                           .isoformat(timespec="seconds")}, f, indent=2)
                f.write("\n")
            print("%s: %s, %d pictures, %.0f s" % (
                a["id"], profile, len(record), time.time() - t0))
            continue
        if a.get("steps"):
            t0 = time.time()
            free(args.server)
            record = make_chain(args, spec, a, profile, seed, graph)
            if record is None:
                failed.append(a["id"])
                continue
            pdir = os.path.join(args.pack, "provenance")
            os.makedirs(pdir, exist_ok=True)
            with open(os.path.join(pdir, a["id"].replace("/", "-") +
                                   ".json"), "w") as f:
                json.dump({"id": a["id"], "file": a["file"],
                           "atlas": a["atlas"], "profile": profile,
                           "seed": seed,
                           "prompt": graph["pos"]["inputs"]["text"],
                           "steps": record,
                           "comfyui": system.get("comfyui_version"),
                           "generator": git_rev(),
                           "date": datetime.datetime.now(
                               datetime.timezone.utc)
                           .isoformat(timespec="seconds")}, f, indent=2)
                f.write("\n")
            print("%s: %s, %d frames, %.0f s" % (
                a["id"], profile, len(record) + 1, time.time() - t0))
            continue
        plated = bool(a.get("cast")) and "repaint" in graph
        if plated:
            add_plate(graph)
        t0 = time.time()
        try:
            pngs = render(args.server, graph,
                          ["save", "plate"] if plated else ["save"])
        except RuntimeError as e:
            print("%s: failed: %s" % (a["id"], e), file=sys.stderr)
            failed.append(a["id"])
            continue
        out = os.path.join(args.pack, a["file"])
        os.makedirs(os.path.dirname(out), exist_ok=True)
        with open(out, "wb") as f:
            f.write(pngs["save"])
        plate = None
        if plated:
            plate = out + ".plate.png"
            with open(plate, "wb") as f:
                f.write(pngs["plate"])
        if a.get("frame_fit"):
            # Cut the framed picture at its own edge, wherever the model
            # put it, instead of by a fixed trim.
            bounds = frame_bounds(out, a, height)
            up = upload(args.server, out, "flibustier-framed.png")
            try:
                png = render(args.server, frame_fit_workflow(
                    up, bounds, a, height, ow, oh, "flibustier/" +
                    name.replace("/", "-") + "-fit"))
            except RuntimeError as e:
                print("%s: frame fit failed: %s" % (a["id"], e),
                      file=sys.stderr)
                failed.append(a["id"])
                continue
            with open(out, "wb") as f:
                f.write(png)
        if a.get("subject"):
            # The figure cut out and set, smaller, into its strip of the
            # screen (where the game writes nothing), on an empty wall.
            sj = a["subject"]
            kk = height / SCREEN_H
            free(args.server)
            try:
                up = upload(args.server, out, "flibustier-subject.png")
                cut = render(args.server, cutout_workflow(
                    up, ow, oh, "flibustier/" + name.replace("/", "-") +
                    "-subject"))
                free(args.server)
                back = render(args.server, texture_workflow(
                    spec, profile, seed + 7, sj["back"], 1024,
                    "flibustier/" + name.replace("/", "-") + "-wall"))
            except RuntimeError as e:
                print("%s: subject failed: %s" % (a["id"], e),
                      file=sys.stderr)
                failed.append(a["id"])
                continue
            cpath, bpath = out + ".cut.png", out + ".wall.png"
            open(cpath, "wb").write(cut)
            open(bpath, "wb").write(back)
            x, y, w, h = (round(v * kk) for v in sj["rect"])
            subprocess.run(["magick", "(", bpath, "-resize", "%dx%d^" % (
                ow, oh), "-gravity", "center", "-extent", "%dx%d" % (
                ow, oh), ")", "(", cpath, "-trim", "+repage", "-resize",
                "%dx%d" % (w, h), ")", "-gravity", "southwest", "-geometry",
                "+%d+%d" % (x, oh - y - h), "-composite", "-depth", "8",
                out], check=True)
            os.remove(cpath)
            os.remove(bpath)
        if a.get("matte_on"):
            # Its object cut out by the matte and set on a plain colour:
            # the model keeps a backdrop round it however it is asked.
            free(args.server)
            up = upload(args.server, out, "flibustier-matte-src.png")
            try:
                png = render(args.server, cutout_workflow(
                    up, ow, oh, "flibustier/" + name.replace("/", "-") +
                    "-matte"))
            except RuntimeError as e:
                print("%s: matte failed: %s" % (a["id"], e), file=sys.stderr)
                failed.append(a["id"])
                continue
            with open(out, "wb") as f:
                f.write(png)
            if a.get("frame"):
                # The cut-out grown to fill the frame's place, centred.
                kk = height / SCREEN_H
                fx, fy, fw, fh = (round(v * kk) for v in a["frame"])
                subprocess.run(["magick", "-size", "%dx%d" % (ow, oh),
                                "xc:none", "(", out, "-trim", "+repage",
                                "-resize", "%dx%d" % (fw, fh), ")",
                                "-gravity", "northwest", "-geometry",
                                "+%d+%d" % (fx, fy), "-composite", out],
                               check=True)
                subprocess.run(["magick", out, "-gravity", "northwest",
                                "(", "+clone", "-alpha", "extract", ")",
                                "+delete", out], check=True)
            subprocess.run(["magick", out, "-background", a["matte_on"],
                            "-flatten", out], check=True)
        for w in a.get("words", []):
            # Words set on the picture in the pack's font (the model
            # cannot write), with a dark edge so they read as carved.
            import boards
            kk = height / SCREEN_H
            x, y, ww, hh = w["rect"]
            t = out + ".words.png"
            boards.text(t, w["words"], w.get("ink", "#f0d690"), ww, hh,
                        os.path.join(args.pack, w["font"]), kk,
                        halo=w.get("halo", "#2a1200c0") or None)
            if w.get("sag"):
                # Bowed down in the middle, as on a hanging cloth.
                boards.magick(t, "-background", "none", "-virtual-pixel",
                              "transparent", "-wave", "%gx%d" % (
                                  -w["sag"] * kk, round(ww * kk * 2)),
                              "-gravity", "north", "-crop", "%dx%d+0+%d" % (
                                  round(ww * kk), round((hh + abs(
                                      w["sag"])) * kk), 0), "+repage", t)
            if w.get("opacity"):
                # Painted on: a little of the cloth shows through.
                boards.magick(t, "-channel", "A", "-evaluate", "multiply",
                              str(w["opacity"]), "+channel", t)
            boards.magick(out, t, "-geometry", "+%d+%d" % (
                round(x * kk), round(y * kk)), "-composite", out)
            os.remove(t)
        cast, ok = add_cast(args, spec, a.get("cast", []), out, out,
                            profile, seed + 100, name, plate)
        if plate:
            os.remove(plate)
        # Last touches the rest of the picture is kept around: repaint
        # each step's mask (the left table widened into a sprite frame).
        k_art = height / SCREEN_H
        for m, step in enumerate(a.get("touchup", []) if ok else []):
            # Early repaints, before the picture is kept for the sprite
            # frames cut from it (the town's sky line).
            up = upload(args.server, out, "flibustier-touchup.png")
            mask = upload(args.server, os.path.join(HERE, step["mask"]),
                          "flibustier-touchup-" + os.path.basename(
                              step["mask"]))
            try:
                png = render(args.server, repaint_workflow(
                    spec, profile, seed + 450 + m, up, mask, step["rect"],
                    step["prompt"], "flibustier/" + name.replace("/", "-") +
                    "-touchup"))
            except RuntimeError as e:
                print("%s: touch-up failed: %s" % (a["id"], e),
                      file=sys.stderr)
                ok = False
                break
            with open(out, "wb") as f:
                f.write(png)
        if a.get("before_finish") and ok:
            # The picture as rendered, before its last touches take out
            # what a sprite frame brings back (the town's harbour).
            keep = os.path.join(args.pack, a["before_finish"])
            os.makedirs(os.path.dirname(keep), exist_ok=True)
            shutil.copyfile(out, keep)
        for m, step in enumerate(a.get("finish", []) if ok else []):
            if step.get("instruction"):
                # The edit model changes that part (it keeps what is
                # around better than a repaint), pasted inside the mask.
                free(args.server)
                up = upload(args.server, out, "flibustier-finish-source.png")
                try:
                    png = render(args.server, cast_workflow(
                        step, spec, profile, seed + 400 + m, up, None,
                        "flibustier/" + name.replace("/", "-") + "-edit"))
                    edited = out + ".edit.png"
                    with open(edited, "wb") as f:
                        f.write(png)
                    free(args.server)
                    eup = upload(args.server, edited, "flibustier-edit.png")
                    os.remove(edited)
                    mask = upload(args.server, os.path.join(
                        HERE, step["mask"]), "flibustier-finish-" +
                        os.path.basename(step["mask"]))
                    png = render(args.server, edit_paste_workflow(
                        up, eup, mask, step["rect"], k_art,
                        "flibustier/" + name.replace("/", "-") + "-edit"))
                except RuntimeError as e:
                    print("%s: finish failed: %s" % (a["id"], e),
                          file=sys.stderr)
                    failed.append(a["id"])
                    break
                with open(out, "wb") as f:
                    f.write(png)
                continue
            if step.get("thing"):
                # An object added by the edit model and cut out, so it
                # keeps its own edges.
                added, ok = add_cast(args, spec, [dict(step)], out, out,
                                     profile, seed + 400 + m, name)
                cast += added
                if not ok:
                    break
                continue
            free(args.server)
            up = upload(args.server, out, "flibustier-finish-source.png")
            mask = upload(args.server, os.path.join(HERE, step["mask"]),
                          "flibustier-finish-" + os.path.basename(step["mask"]))
            try:
                png = render(args.server, repaint_workflow(
                    spec, profile, seed + 400 + m, up, mask, step["rect"],
                    step["prompt"], "flibustier/" + name.replace("/", "-") +
                    "-finish"))
            except RuntimeError as e:
                print("%s: finish failed: %s" % (a["id"], e),
                      file=sys.stderr)
                failed.append(a["id"])
                break
            with open(out, "wb") as f:
                f.write(png)
        if not ok:
            failed.append(a["id"])
        prov = {
            "id": a["id"], "file": a["file"], "size": [ow, oh],
            "profile": profile, "seed": seed,
            "prompt": graph["pos"]["inputs"]["text"],
            "frame": a.get("frame"), "field": a.get("field"),
            "control": {"sketch": a["control"],
                        "sha256": hashlib.sha256(open(os.path.join(
                            HERE, a["control"]), "rb").read()).hexdigest(),
                        "strength": a.get("control_strength", 0.7),
                        "end": a.get("control_end", 0.6)}
            if a.get("control") else None,
            "inpaint": {"mask": a["inpaint_mask"],
                        "sha256": hashlib.sha256(open(os.path.join(
                            HERE, a["inpaint_mask"]), "rb").read())
                        .hexdigest(),
                        "prompt": a["inpaint_prompt"].strip()}
            if a.get("inpaint_mask") else None,
            "grade": {k: v for k, v in grade.items() if k != "uploaded"}
            if grade and grade.get("uploaded") and a.get("grade", True)
            else None,
            "source": a.get("source", "text"),
            "bevel": {"masks": a["bevel"], "sha256": [hashlib.sha256(open(
                os.path.join(HERE, a["bevel"] + side + ".png"), "rb")
                .read()).hexdigest() for side in ("-light", "-dark")]}
            if a.get("bevel") else None,
            "cast": cast or None,
            "finish": [{"mask": f["mask"], "sha256": hashlib.sha256(open(
                os.path.join(HERE, f["mask"]), "rb").read()).hexdigest(),
                        "rect": f["rect"], "prompt": f["prompt"].strip()}
                       if f.get("prompt") else
                       {"mask": f["mask"], "rect": f["rect"],
                        "instruction": " ".join(f["instruction"].split())}
                       if f.get("instruction") else
                       {"thing": " ".join(f["thing"].split()),
                        "rect": f["rect"]}
                       for f in a.get("finish", [])] or None,
            "negative": a.get("negative", DEFAULT_NEGATIVE),
            "models": {k: v for k, v in MODELS.items()
                       if (k != "lightning" or PROFILES[profile]["lora"])
                       and (k != "controlnet" or a.get("control"))
                       and (k not in ("edit", "matte") or a.get("cast"))
                       and (k != "edit_lightning" or (
                           a.get("cast") and EDIT_PROFILES[profile]["lora"]))},
            "workflow_sha256": hashlib.sha256(json.dumps(
                graph, sort_keys=True).encode()).hexdigest(),
            "comfyui": system.get("comfyui_version"),
            "generator": git_rev(),
            "date": datetime.datetime.now(datetime.timezone.utc)
            .isoformat(timespec="seconds"),
        }
        pdir = os.path.join(args.pack, "provenance")
        os.makedirs(pdir, exist_ok=True)
        with open(os.path.join(pdir, a["id"].replace("/", "-") + ".json"),
                  "w") as f:
            json.dump(prov, f, indent=2)
            f.write("\n")
        print("%s: %s %dx%d seed %d, %.0f s" % (a["id"], profile, ow, oh,
                                                seed, time.time() - t0))
    write_manifest(args.pack, spec)
    if failed:
        sys.exit("failed: " + ", ".join(failed))


if __name__ == "__main__":
    main()
