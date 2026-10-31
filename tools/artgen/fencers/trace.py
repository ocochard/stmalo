#!/usr/bin/env python3
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026, Olivier Cochard-Labbé
#
"""Where the original fencer's joints and blade are, frame by frame, so
that fencer.py can pose the pack's fencer the same way.

Read at render time from the player's own game data (flibustier
--export), never stored in the repo or the pack: each frame of the
original's body (with, for the opponent, his head) is given to a pose
model (SDPose, through ComfyUI) for its joints, and the blade is found
on the weapon's own layer: its line through its pixels, its hilt the end
nearer a hand; each hand's tip, the drawn pixel farthest on from the
wrist; the end of his sash, its red pixel farthest from his back hip.  Written as JSON, by "MOVE.FRAME": the
frame's height and each point in the frame's pixels (x right, y down,
from its top left), with the pose model's confidence.

    trace.py --flibustier BIN --data DIR --server URL --side my OUT.json
             [--moves 0,14] [--work DIR]

Python 3 with numpy, and ImageMagick's magick.
"""

import argparse
import json
import os
import subprocess
import time
import urllib.request
import uuid

import numpy as np

BANKS = [6, 6, 6, 6, 6, 6, 6, 6, 6, 4, 4, 4, 6, 6, 10, 6]
# The pose model's joints (OpenPose order), by the names fencer.py fits.
JOINTS = ["nose", "neck", "r-shoulder", "r-elbow", "r-wrist",
          "l-shoulder", "l-elbow", "l-wrist", "r-hip", "r-knee",
          "r-ankle", "l-hip", "l-knee", "l-ankle"]
# Its feet: big toe, small toe and heel, right then left.
FEET = ["l-toe", "l-small-toe", "l-heel", "r-toe", "r-small-toe",
        "r-heel"]
PAD, ZOOM = 200, 4	# the frame in a square this many pixels, enlarged
CHECKER = (0x3C, 0x5A)	# the empty pixels of flibustier --export, 8x8
TAIL_BELOW = 4		# the sash's end looked for this far below the shoulders
# A hand's tip looked for on from the wrist, within this angle (degrees)
# of the forearm's line and this many forearms' lengths.
HAND_CONE, HAND_REACH = 30, 2.0


def load(path):
    w, h = (int(v) for v in subprocess.run(
        ["magick", "identify", "-format", "%w %h", path],
        capture_output=True, text=True, check=True).stdout.split())
    raw = subprocess.run(["magick", path, "-depth", "8", "rgb:-"],
                         capture_output=True, check=True).stdout
    return np.frombuffer(raw, np.uint8).reshape(h, w, 3).astype(int)


def save(path, px):
    h, w = px.shape[:2]
    subprocess.run(["magick", "-size", "%dx%d" % (w, h), "-depth", "8",
                    "rgb:-", path], input=px.clip(0, 255).astype(
                        np.uint8).tobytes(), check=True)


def drawn(px):
    """The frame's drawn pixels: not the export's checkered ground."""
    h, w = px.shape[:2]
    y, x = np.mgrid[0:h, 0:w]
    ground = np.where((x // 8 + y // 8) % 2 == 0, *CHECKER)[:, :, None]
    return np.abs(px - ground).sum(axis=2) > 6


def export(args, gid, frame, path):
    subprocess.run([args.flibustier, "--data", args.data, "--export", gid,
                    path, "--frame", str(frame)], check=True,
                   stdout=subprocess.DEVNULL)
    return load(path)


def post(server, path, data, ctype):
    req = urllib.request.Request(server + path, data=data,
                                 headers={"Content-Type": ctype})
    with urllib.request.urlopen(req) as r:
        return json.load(r)


def upload(server, path, name):
    b = uuid.uuid4().hex
    body = (("--%s\r\nContent-Disposition: form-data; name=\"image\"; "
             "filename=\"%s\"\r\nContent-Type: image/png\r\n\r\n" %
             (b, name)).encode() + open(path, "rb").read() +
            ("\r\n--%s\r\nContent-Disposition: form-data; "
             "name=\"overwrite\"\r\n\r\ntrue\r\n--%s--\r\n" %
             (b, b)).encode())
    return post(server, "/upload/image", body,
                "multipart/form-data; boundary=" + b)["name"]


def keypoints(server, path, name):
    """The pose model's people on the picture: OpenPose frames."""
    g = {"1": {"class_type": "CheckpointLoaderSimple", "inputs": {
        "ckpt_name": "sdpose_wholebody_fp16.safetensors"}},
        "2": {"class_type": "LoadImage", "inputs": {
            "image": upload(server, path, name)}},
        "3": {"class_type": "SDPoseKeypointExtractor", "inputs": {
            "model": ["1", 0], "vae": ["1", 2], "image": ["2", 0],
            "batch_size": 1}},
        "4": {"class_type": "PreviewAny", "inputs": {"source": ["3", 0]}}}
    pid = post(server, "/prompt", json.dumps({"prompt": g}).encode(),
               "application/json")["prompt_id"]
    while True:
        with urllib.request.urlopen(server + "/history/" + pid) as r:
            h = json.load(r)
        if pid in h and h[pid].get("status", {}).get("completed") \
                is not None:
            break
        time.sleep(0.5)
    out = h[pid]["outputs"].get("4")
    if not out:
        raise RuntimeError("SDPose: %s" % h[pid]["status"])
    return json.loads(out["text"][0])[0]["people"]


def blade(mask, hands):
    """The blade's (hilt, tip) on the weapon's layer: its pixels' line,
    the hilt the end nearer one of the hands; None without one."""
    ys, xs = np.nonzero(mask)
    if len(xs) < 4:
        return None
    pts = np.stack([xs, ys], axis=1).astype(float) + 0.5
    mid = pts.mean(axis=0)
    _, _, vt = np.linalg.svd(pts - mid)
    d = vt[0]
    t = (pts - mid) @ d
    ends = [mid + d * t.min(), mid + d * t.max()]
    if hands:
        near = [min(np.hypot(*(e - h)) for h in hands) for e in ends]
        if near[1] < near[0]:
            ends.reverse()
    return [list(ends[0]), list(ends[1])]


def tail(px, shown, pts, side):
    """The end of his sash, hanging or flying from its knot at his back
    hip (the one away from the opponent): the body's red pixel farthest
    from that hip, below the shoulders (not the lips); None without
    one."""
    def sure(j):
        return j in pts and pts[j][2] > 0.3
    hips = [pts[j][:2] for j in ("r-hip", "l-hip") if sure(j)]
    tops = [pts[j][1] for j in ("r-shoulder", "l-shoulder") if sure(j)]
    if not hips or not tops:
        return None
    back = max(hips, key=lambda p: p[0] if side == "my" else -p[0])
    r, g, b = px[:, :, 0], px[:, :, 1], px[:, :, 2]
    red = shown & (r > 150) & (g < 80) & (b < 80)
    red[:int(max(tops)) + TAIL_BELOW] = False
    ys, xs = np.nonzero(red)
    if len(ys) == 0:
        return None
    d = np.hypot(xs + 0.5 - back[0], ys + 0.5 - back[1])
    i = d.argmax()
    return [float(xs[i]) + 0.5, float(ys[i]) + 0.5]


def hands(shown, pts):
    """Each hand's tip (as "r-hand", "l-hand"): the body's drawn pixel
    farthest on from the wrist along the forearm's line, as the pose
    model marks the wrist where the forearm is, the open hand past it."""
    ys, xs = np.nonzero(shown)
    px = np.stack([xs, ys], axis=1).astype(float) + 0.5
    out = {}
    for c in "rl":
        e, w = pts.get(c + "-elbow"), pts.get(c + "-wrist")
        if not e or not w or min(e[2], w[2]) < 0.3:
            continue
        d = np.array(w[:2]) - np.array(e[:2])
        n = np.hypot(*d)
        if n < 1:
            continue
        v = px - np.array(w[:2])
        r = np.hypot(v[:, 0], v[:, 1])
        on = v @ (d / n)
        ok = (r > 0) & (r < HAND_REACH * n) & \
            (on > r * np.cos(np.radians(HAND_CONE)))
        if ok.any():
            i = np.argmax(np.where(ok, on, -1))
            out[c + "-hand"] = [float(px[i, 0]), float(px[i, 1]),
                                min(e[2], w[2])]
    return out


def trace(args, move, k, work):
    n = sum(BANKS[:move]) + k
    path = os.path.join(work, "%s-%%s-%d.png" % (args.side, n))
    body = export(args, "grp/%sbody" % args.side, n, path % "body")
    shown = drawn(body)
    px = np.where(shown[:, :, None], body, 128)
    if args.side == "he":
        head = export(args, "grp/hehead1", n, path % "head")
        on = drawn(head)
        px[on] = head[on]
    h, w = shown.shape
    ox, oy = (PAD - w) // 2, PAD - h - 20
    big = np.zeros((PAD, PAD, 3), int) + 128
    big[oy:oy + h, ox:ox + w] = px
    big = big.repeat(ZOOM, 0).repeat(ZOOM, 1)
    save(path % "pose", big)
    people = keypoints(args.server, path % "pose",
                       "flibustier-trace-%s.png" % args.side)
    pts = {}
    if people:
        p = max(people, key=lambda q: sum(q["pose_keypoints_2d"][2::3]))
        for names, flat in ((JOINTS, p["pose_keypoints_2d"]),
                            (FEET, p.get("foot_keypoints_2d") or [])):
            for i, name in enumerate(names):
                x, y, c = flat[3 * i:3 * i + 3]
                pts[name] = [x / ZOOM - ox, y / ZOOM - oy, c]
    sword = drawn(export(args, "grp/%ssword" % args.side, n,
                         path % "sword"))
    held = [pts[j][:2] for j in ("r-wrist", "l-wrist")
            if j in pts and pts[j][2] > 0.3]
    pts.update(hands(shown, pts))
    return {"height": h, "points": pts, "blade": blade(sword, held),
            "tail": tail(body, shown, pts, args.side)}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--flibustier", required=True)
    ap.add_argument("--data", required=True)
    ap.add_argument("--server", required=True)
    ap.add_argument("--side", choices=("my", "he"), required=True)
    ap.add_argument("--moves", help="only these moves, e.g. 0,14")
    ap.add_argument("--work", default="trace-work")
    ap.add_argument("out")
    args = ap.parse_args()
    os.makedirs(args.work, exist_ok=True)
    moves = range(len(BANKS)) if not args.moves else \
        [int(v) for v in args.moves.split(",")]
    out = {}
    for m in moves:
        for k in range(BANKS[m]):
            out["%d.%d" % (m, k)] = trace(args, m, k, args.work)
    with open(args.out, "w") as fp:
        json.dump(out, fp, indent=1)


if __name__ == "__main__":
    main()
