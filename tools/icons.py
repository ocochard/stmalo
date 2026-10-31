#!/usr/bin/env python3
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026, Olivier Cochard-Labbe
#
"""Render the program's icon in every size and format the systems want,
from desktop/flibustier.svg.

desktop/icons/flibustier-N.png for the freedesktop icon theme (and the
window icon the game sets itself), desktop/flibustier.ico for Windows
(built into the .exe) and desktop/flibustier.icns for a macOS bundle.
Both containers hold PNG images.  Needs rsvg-convert (librsvg).
Run it again after changing the SVG, and commit what it writes.
"""
import os
import struct
import subprocess

top = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
desk = os.path.join(top, "desktop")
svg = os.path.join(desk, "flibustier.svg")

THEME = (16, 24, 32, 48, 64, 128, 256, 512)
ICO = (16, 24, 32, 48, 64, 256)
ICNS = (("icp4", 16), ("icp5", 32), ("icp6", 64), ("ic07", 128),
        ("ic08", 256), ("ic09", 512), ("ic10", 1024), ("ic11", 32),
        ("ic12", 64), ("ic13", 512), ("ic14", 1024))


def render(n):
    return subprocess.run(["rsvg-convert", "-w", str(n), "-h", str(n), svg],
                          check=True, capture_output=True).stdout


def ico(images):
    """An ICO directory followed by its PNG images."""
    head = struct.pack("<HHH", 0, 1, len(images))
    off = len(head) + 16 * len(images)
    dirs, data = b"", b""
    for n, png in images:
        dirs += struct.pack("<BBBBHHII", n % 256, n % 256, 0, 0, 1, 32,
                            len(png), off + len(data))
        data += png
    return head + dirs + data


def icns(images):
    body = b"".join(struct.pack(">4sI", t.encode(), 8 + len(png)) + png
                    for t, png in images)
    return struct.pack(">4sI", b"icns", 8 + len(body)) + body


cache = {}
for n in set(THEME + ICO + tuple(n for _, n in ICNS)):
    cache[n] = render(n)
os.makedirs(os.path.join(desk, "icons"), exist_ok=True)
for n in THEME:
    with open(os.path.join(desk, "icons", "flibustier-%d.png" % n),
              "wb") as f:
        f.write(cache[n])
with open(os.path.join(desk, "flibustier.ico"), "wb") as f:
    f.write(ico([(n, cache[n]) for n in ICO]))
with open(os.path.join(desk, "flibustier.icns"), "wb") as f:
    f.write(icns([(t, cache[n]) for t, n in ICNS]))
