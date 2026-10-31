#!/usr/bin/env python3
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026, Olivier Cochard-Labbé
#
"""Make a photographed fabric a material the fencers' clothes can wear.

The image model's close-up of a cloth (artgen.py, fencer_fabrics()) does
not repeat and is lit unevenly; here its edges are cut, its lighting
taken away and it is made to tile, by blending it with itself shifted
half its width across the bands near its left and right edges, then the
same up and down, so its edges meet what is past them.  Its roughness
and normal maps are derived from its light and dark: the weave's
threads catch the light and stand out.  Written as materials.py looks
for them: DIR/NAME_Color.jpg, NAME_Roughness.jpg, NAME_NormalGL.jpg.

    fabric.py PICTURE DIR NAME [--rough R] [--relief K]

Python 3 with numpy, and ImageMagick's magick to read and write the
pictures.
"""

import argparse
import os
import subprocess

import numpy as np

BAND = 0.25	# each edge's blended band, a share of the side
CROP = 0.06	# the picture's edges cut off (the model leaves a border)
LIGHT = 48	# the radius (pixels) of the lighting taken away


def ramp(n):
    """0 at the edges, 1 from BAND in, smoothly."""
    d = np.minimum(np.arange(n), n - 1 - np.arange(n)) / (BAND * n)
    d = np.clip(d, 0, 1)
    return d * d * (3 - 2 * d)


def tileable(px):
    h, w, _ = px.shape
    wx = ramp(w)[None, :, None]
    px = wx * px + (1 - wx) * np.roll(px, w // 2, axis=1)
    wy = ramp(h)[:, None, None]
    return wy * px + (1 - wy) * np.roll(px, h // 2, axis=0)


def blur(a, r):
    """A box blur of radius r, wrapping round."""
    for axis in (0, 1):
        n = a.shape[axis]
        c = np.cumsum(np.concatenate(
            [np.take(a, range(n - r - 1, n), axis=axis), a,
             np.take(a, range(r), axis=axis)], axis=axis), axis=axis)
        a = (np.take(c, range(2 * r + 1, n + 2 * r + 1), axis=axis) -
             np.take(c, range(0, n), axis=axis)) / (2 * r + 1)
    return a


def load(path):
    """The picture as rows of RGB in 0..1, top row first."""
    w, h = (int(v) for v in subprocess.run(
        ["magick", "identify", "-format", "%w %h", path + "[0]"],
        capture_output=True, text=True, check=True).stdout.split())
    raw = subprocess.run(["magick", path + "[0]", "-depth", "8", "rgb:-"],
                         capture_output=True, check=True).stdout
    return np.frombuffer(raw, np.uint8).reshape(h, w, 3) / 255.0


def save(path, px):
    h, w = px.shape[:2]
    if px.shape[2] == 1:
        px = np.repeat(px, 3, axis=2)
    raw = (np.clip(px, 0, 1) * 255 + 0.5).astype(np.uint8).tobytes()
    subprocess.run(["magick", "-size", "%dx%d" % (w, h), "-depth", "8",
                    "rgb:-", "-quality", "95", path], input=raw, check=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("picture")
    ap.add_argument("dir")
    ap.add_argument("name")
    ap.add_argument("--rough", type=float, default=0.75,
                    help="the cloth's roughness, on average")
    ap.add_argument("--relief", type=float, default=6.0,
                    help="how much the light and dark stand out")
    args = ap.parse_args()
    px = load(args.picture)
    h, w = px.shape[:2]
    px = px[round(h * CROP):h - round(h * CROP),
            round(w * CROP):w - round(w * CROP)]
    # The photograph's lighting (glare, shade) taken away, so only the
    # cloth's own light and dark repeat.
    lum = px @ np.array([0.2126, 0.7152, 0.0722])
    low = blur(blur(lum, LIGHT), LIGHT)
    px = np.clip(px * (lum.mean() / np.maximum(low, 0.02))[:, :, None],
                 0, 1)
    px = tileable(px)
    os.makedirs(args.dir, exist_ok=True)
    out = os.path.join(args.dir, args.name + "_%s.jpg")
    save(out % "Color", px)
    # The height: the light, its slow changes (the picture's lighting)
    # taken away, so only the weave stands out.
    lum = px @ np.array([0.2126, 0.7152, 0.0722])
    height = lum - blur(lum, 24)
    height /= max(1e-6, np.abs(height).max())
    # Raised threads catch more light and are less rough.
    rough = np.clip(args.rough - 0.15 * height, 0.05, 1.0)
    save(out % "Roughness", rough[:, :, None])
    dx = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) / 2
    # Rows run down; the normal's green (OpenGL) points up the picture.
    dy = (np.roll(height, 1, axis=0) - np.roll(height, -1, axis=0)) / 2
    n = np.stack([-dx * args.relief, -dy * args.relief,
                  np.ones_like(dx)], axis=2)
    n /= np.linalg.norm(n, axis=2, keepdims=True)
    save(out % "NormalGL", n * 0.5 + 0.5)


if __name__ == "__main__":
    main()
