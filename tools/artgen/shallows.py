#!/usr/bin/env python3
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026, Olivier Cochard-Labbé
#
"""Paint open shallows into the sailing map's tile sheet.

The original sheet's shallows are only the halo along each coast; the
open world has banks and reefs away from the shore.  These tiles are
painted over the sheet's surf tiles (which the open map does not use):

- 4 solid shallows, then the straight edges with shallows on the north,
  east, south and west half, the outer corners with shallows in the NE,
  SE, SW and NW quarter, the inner corners with open sea in those
  quarters (the first index of each group of four, from "first").

Each is drawn on an open-sea tile of the same frame, so its sea sides
meet the open sea as that does (the sheet's sea is periodic at the
tiles' edges); the shallows are that sea lit turquoise over a sandy
bank, the bank's pattern periodic on the tile, so shallows meet
shallows seamlessly; the edge between them runs through the middle of
each side, softly, the same in every tile.  The shallows stay in the
turquoise of the sheet's coastal halo, which tools/opendata classifies
from the art: water (blue > red + 15, blue >= green + 15), then shallows
(green >= 0.62 x blue, green - red >= 45); classify_safe() keeps every
shallows pixel inside both.

So the reefs read as a hazard, rock heads break the surface here and
there, each ringed by surf whose foam swells and ebbs over the sea's
frames.  They are dark and pale teal rather than grey and white, so
that they too classify as shallows (a pixel read as land would turn
the whole tile's shallows to sea), and they keep clear of the tiles'
edges and of the shallows' soft edge, so the pieces still meet.
"""

import math
import random

import tilemaker

TURQUOISE = (46, 128, 162)


def classify_safe(c):
    """A shallows colour moved, if need be, into tools/opendata's
    shallows class, with margins: water (blue > red + 15, blue >= green
    + 15), then shallows (green >= 0.62 x blue, green - red >= 45)."""
    r, g, b = c
    b = min(255.0, max(b, g + 20, r + 20))
    g = min(g, b - 20)
    g = max(g, 0.65 * b)
    r = min(r, g - 50)
    return (max(0.0, r), g, b)


SAND = (200, 188, 140)
ROCK = (8, 62, 82)		# a rock head in shade
ROCK_LIT = (34, 100, 124)	# and lit, from the upper left
FOAM = (185, 235, 255)
SURF = 0.16			# the surf's reach around a rock, x side


def open_sea_tiles(rows, side, columns, frame_rows, skip):
    """The sheet's tiles of plain open sea in frame 0, most even first:
    blue-dominant, no turquoise, no land."""
    found = []
    for t in range(frame_rows * columns):
        if t in skip:
            continue
        x0, y0 = (t % columns) * side, (t // columns) * side
        n = blue = 0
        ok = True
        tot = [0, 0, 0]
        for y in range(y0, y0 + side, 3):
            r = rows[y]
            for x in range(x0, x0 + side, 3):
                c = r[x * 4:x * 4 + 3]
                if not (c[2] > c[1] + 15 and c[2] > c[0] + 25):
                    ok = False
                    break
                for i in range(3):
                    tot[i] += c[i]
                n += 1
            if not ok:
                break
        if ok:
            found.append((t, tot[2] / n))
    found.sort(key=lambda p: -p[1])
    return [t for t, _ in found]


def smooth(e0, e1, v):
    t = min(1.0, max(0.0, (v - e0) / (e1 - e0)))
    return t * t * (3 - 2 * t)


def wob(u, side, phase=0.0):
    """A wobble along an edge, zero at both its ends."""
    return 1.4 * math.sin(math.pi * u / side) * math.sin(
        2 * math.pi * u / side + phase)


def mask(kind, q, x, y, side):
    """The shallows' share at (x, y) for tile kind ("solid", "edge",
    "outer", "inner") and quarter or side q (0 N/NE, 1 E/SE, 2 S/SW,
    3 W/NW)."""
    h = side / 2
    s = side * 0.07		# the edge's softness
    if kind == "solid":
        return 1.0
    if kind == "edge":
        d = (h - y + wob(x, side), x - h + wob(y, side, 1),
             y - h + wob(x, side, 2), h - x + wob(y, side, 3))[q]
        return smooth(-s, s, d)
    # Corners: the quarter (NE: x > h, y < h ...), its inner corner
    # rounded; both its sides reach the tile's edges at the middle.
    sx = 1 if q in (0, 1) else -1
    sy = -1 if q in (0, 3) else 1
    dx, dy = sx * (x - h), sy * (y - h)
    r = side * 0.22
    if dx >= r and dy >= r:
        d = min(dx, dy) if min(dx, dy) < r else r
        d = min(dx, dy)
    elif dx < r and dy < r and dx > -s and dy > -s:
        d = r - math.hypot(r - dx, r - dy)
    else:
        d = min(dx, dy)
    m = smooth(-s, s, d)
    return m if kind == "outer" else 1.0 - m


def bank(x, y, side, v):
    """The sandy bank's brightness, 0-1: periodic on the tile, so it
    meets itself; a variant's own pattern only inside the tile."""
    a = 2 * math.pi / side
    p = 0.5 + 0.16 * (math.sin(a * x + 0.7) * math.sin(a * y * 2 + 1.9)) \
        + 0.08 * math.sin(a * (x - y) * 2 + 0.4)
    if v:
        w = math.sin(math.pi * x / side) * math.sin(math.pi * y / side)
        p += 0.25 * w * math.sin(a * (x * (1 + v) - y * (2 + v)) + v)
    return min(1.0, max(0.0, p))


def rocks(kind, q, side, seed):
    """The tile's rock heads, (x, y, radius, phase, phase): two or
    three clusters on solid shallows, one elsewhere, each well inside
    the shallows with its surf and clear of the tile's edges."""
    rng = random.Random(seed)
    reach = SURF * side
    want = rng.choice((2, 3)) if kind == "solid" else 1
    found = []
    for _ in range(400):
        if len(found) >= want * 2:
            break
        r = side * rng.uniform(0.05, 0.09)
        m = 1.4 * r + reach + 1
        x, y = rng.uniform(m, side - m), rng.uniform(m, side - m)
        if any(mask(kind, q, x + m * math.cos(a), y + m * math.sin(a),
                    side) < 0.999 for a in
               [k * math.pi / 4 for k in range(8)] + [0]) or \
                mask(kind, q, x, y, side) < 0.999:
            continue
        if any(math.hypot(x - u, y - v) < r + w + reach for u, v, w, _, _
               in found):
            continue
        found.append((x, y, r, rng.uniform(0, 6.3), rng.uniform(0, 6.3)))
        # A smaller head beside it, sharing its surf.
        a = rng.uniform(0, 2 * math.pi)
        r2 = r * rng.uniform(0.45, 0.7)
        x2, y2 = x + (r + r2) * 0.8 * math.cos(a), \
            y + (r + r2) * 0.8 * math.sin(a)
        m2 = 1.4 * r2 + reach + 1
        if m2 <= x2 <= side - m2 and m2 <= y2 <= side - m2 and \
                mask(kind, q, x2, y2, side) >= 0.999:
            found.append((x2, y2, r2, rng.uniform(0, 6.3),
                          rng.uniform(0, 6.3)))
    return found


def reef(heads, x, y, side, f, frames):
    """(rock share, its light 0-1, foam share) at (x, y) in frame f."""
    rock = foam = 0.0
    light = 0.5
    reach = SURF * side
    for u, v, r, p1, p2 in heads:
        dx, dy = x + 0.5 - u, y + 0.5 - v
        d = math.hypot(dx, dy)
        a = math.atan2(dy, dx)
        edge = r * (1 + 0.22 * math.sin(3 * a + p1)
                    + 0.12 * math.sin(5 * a + p2))
        out = d - edge
        cover = smooth(0.6, -0.6, out)
        if cover > rock:
            rock = cover
            light = 0.5 - 0.5 * (dx + dy) / (1.414 * max(edge, 1e-6)) \
                - 0.25 * max(0.0, d / max(edge, 1e-6) - 0.6)
        if 0 < out < reach:
            # Breakers rolling out from the rock, swelling and ebbing.
            t = out / reach
            wave = 0.5 + 0.5 * math.cos(2 * math.pi * (
                1.6 * t - f / frames) + p1 + 0.8 * math.sin(2 * a + p2))
            foam = max(foam, (1 - t) ** 1.3 * (0.35 + 0.65 * wave))
    return rock, min(1.0, max(0.0, light)), foam


def paint(path, side, columns, frames, frame_rows, first, skip):
    w, h, ch, rows = tilemaker.png_read(open(path, "rb").read())
    if ch != 4:
        rows = [bytearray(b for i in range(w) for b in
                          (*r[i * ch:i * ch + 3], 255)) for r in rows]
    seas = open_sea_tiles(rows, side, columns, frame_rows, skip)
    if len(seas) < 4:
        raise RuntimeError("fewer than 4 open-sea tiles to paint on")
    kinds = [("solid", v) for v in range(4)] + \
        [(k, q) for k in ("edge", "outer", "inner") for q in range(4)]
    for n, (kind, q) in enumerate(kinds):
        t = first + n
        base = seas[(q if kind == "solid" else n) % 4]
        bx, by = (base % columns) * side, (base // columns) * side
        tx, ty = (t % columns) * side, (t // columns) * side
        heads = rocks(kind, q, side, t)
        for f in range(frames):
            fo = f * frame_rows * side
            for y in range(side):
                src = rows[fo + by + y]
                dst = rows[fo + ty + y]
                for x in range(side):
                    c = src[(bx + x) * 4:(bx + x) * 4 + 3]
                    m = mask(kind, q, x, y, side)
                    if m > 0:
                        b = bank(x, y, side, q if kind == "solid" else 0)
                        sh = [c[i] + (TURQUOISE[i] - c[i]) * 0.7
                              for i in range(3)]
                        sh = [sh[i] + (SAND[i] - sh[i]) * 0.18 * b
                              for i in range(3)]
                        sh = [v * (0.92 + 0.16 * b) for v in sh]
                        rk, lt, fm = reef(heads, x, y, side, f, frames)
                        sh = [sh[i] + (FOAM[i] - sh[i]) * fm
                              for i in range(3)]
                        stone = [ROCK[i] + (ROCK_LIT[i] - ROCK[i]) * lt
                                 for i in range(3)]
                        sh = [sh[i] + (stone[i] - sh[i]) * rk
                              for i in range(3)]
                        sh = classify_safe(sh)
                        c = [round(c[i] + (sh[i] - c[i]) * m)
                             for i in range(3)]
                    o = (tx + x) * 4
                    dst[o:o + 4] = bytes((*[min(255, max(0, v))
                                            for v in c], 255))
    tilemaker.png_write(path, w, h, rows)
    return seas[:4]


if __name__ == "__main__":
    import sys
    # The pack's tiles/map: 36-pixel tiles, 64 a row, 8 frames of 4
    # rows; the open shallows on tiles 40-55, the open sea drawn from
    # tiles other than those and the crests' 56-59.
    if len(sys.argv) != 2:
        sys.exit("usage: shallows.py PACK/art/map.png")
    paint(sys.argv[1], 36, 64, 8, 4, 40, set(range(40, 60)))
