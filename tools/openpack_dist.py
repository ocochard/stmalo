#!/usr/bin/env python3
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026, Olivier Cochard-Labbe
#
"""Write the open pack's release tarballs, for packagers (the FreeBSD
port) who cannot fetch Git LFS files.

Two tarballs, both unpacking into flibustier-openpack-VERSION/:
flibustier-openpack-VERSION.tar.gz holds what playing needs (the
pack's files but its provenance and coverage report), and
flibustier-openpack-music-VERSION.tar.gz the remade music
(pack.d/music.ini, its tunes and their credits), which belongs to its
author and so ships apart.  The archives are reproducible: sorted, owned
by root, dated by the pack's commit.
"""
import argparse
import gzip
import io
import os
import subprocess
import sys
import tarfile

ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
ap.add_argument("--pack", default=os.path.expanduser("~/flibustier-openpack"),
                help="the pack checkout, its LFS files fetched")
ap.add_argument("--out", default=".", help="where to write the tarballs")
ap.add_argument("version", help="the release's version, e.g. 0.1")
args = ap.parse_args()
PACK = args.pack
SKIP = ("provenance/", "COVERAGE.md", ".gitattributes", ".gitignore")


def git(*a):
    return subprocess.run(["git", "-C", PACK] + list(a), check=True,
                          capture_output=True, text=True).stdout


def music_files():
    out = {"pack.d/music.ini", "music/CREDITS.txt"}
    with open(os.path.join(PACK, "pack.d", "music.ini")) as f:
        for line in f:
            if "=" in line and line.strip().startswith("music/"):
                out.add(line.split("=", 1)[1].strip())
    return out


def write(name, files, mtime):
    top = "flibustier-openpack-%s/" % args.version
    path = os.path.join(args.out, name)
    raw = io.BytesIO()
    with tarfile.open(fileobj=raw, mode="w", format=tarfile.PAX_FORMAT) as t:
        for f in sorted(files):
            with open(os.path.join(PACK, f), "rb") as fp:
                data = fp.read()
            if data.startswith(b"version https://git-lfs"):
                sys.exit("%s is an LFS pointer: git lfs pull first" % f)
            info = tarfile.TarInfo(top + f)
            info.size = len(data)
            info.mtime = mtime
            info.mode = 0o644
            info.uname = info.gname = "root"
            t.addfile(info, io.BytesIO(data))
    with open(path, "wb") as fp, gzip.GzipFile(
            filename="", mode="wb", fileobj=fp, mtime=mtime) as gz:
        gz.write(raw.getvalue())
    print("%s: %d files, %d MB" % (path, len(files),
                                   os.path.getsize(path) >> 20))


if git("status", "--porcelain", "--untracked-files=no").strip():
    sys.exit("%s has uncommitted changes" % PACK)
mtime = int(git("log", "-1", "--format=%ct").strip())
files = [f for f in git("ls-files").splitlines() if not f.startswith(SKIP)]
music = music_files()
write("flibustier-openpack-%s.tar.gz" % args.version,
      [f for f in files if f not in music], mtime)
write("flibustier-openpack-music-%s.tar.gz" % args.version,
      [f for f in files if f in music], mtime)
