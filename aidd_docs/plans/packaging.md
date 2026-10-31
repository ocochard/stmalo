# Packaging: a playable game on Windows, FreeBSD, macOS and Linux

Goal: one download per platform that plays the whole game from the open
pack, with no original data, and still reads a player's own GOG
`DATA.DAT` when given.

## Where it stands (2026-10-30)

- The engine builds on all four systems (README); Windows through the
  llvm-mingw cross toolchain, its tests run under Wine.
- The open game data is complete (open-game-data.md, G1-G4): the game
  and `flibustier-sim` run from the open pack alone (`--pack`, no
  `--data`), and the career bot plays full twelve-year careers on it.
- The open pack (`~/flibustier-openpack`, 227 MB: art 155, music 58,
  data 5, provenance 8) is a draft: its pictures are 8-step renders.
- No install rules, no release builds, no CI. The game finds its data
  only through `--data`, `--pack` or `FLIBUSTIER_DATA`; saves default to
  `~/.local/share/flibustier/saves` on every system.

## Blockers outside the code

1. ~~The art's licence~~: CC0 1.0 (maintainer, 2026-10-31).
2. **The music** belongs to its author (`music/CREDITS.txt`): written
   permission to redistribute it in the packages, or packages without
   it (P1 decides what plays then).
3. **Final renders**: the draft pictures are to be re-rendered at full
   quality before a 1.0 (the art session's work, not this plan's).

## Phases

### P1. Gaps of the open pack alone

Done (2026-10-31):

- The 7 spoken sound effects (`sound/hail`, `sail-ho`, `harbour`,
  `land-battle`, `last-man`, `hit-foe`, `hit-you`) recorded by the
  maintainer (CC0, 2026-10-31), cleaned with ffmpeg (trim, EQ, gate,
  loudness; each provenance file gives the edit). `voices.toml` marks
  them `recorded`, so `tools/sfxgen/voicegen.py` (Dia2) never renders
  over them; it stays for any voice added later.
- Text entry 318, the cabin's language menu, in the three languages.
- `tools/artgen/coverage.py` counts the original assets the pack
  replaces in another form as done (`tiles15/sbtl`, `lbtl` by the
  Tiled battle screens, `pic/map1560` by `map/1560`, `pic/baseflag`,
  whose palette trick true-colour flags do not need).
- `music/prize` (the capture's tune): a whistled tune the maintainer
  recorded (CC0, 2026-10-31). The open pack now covers every asset ID.

### P2. The engine finds its own data

Each package holds the program and the open pack side by side (a zip
or a self-extracting archive, a macOS application bundle), so the
program looks for its pack beside itself (maintainer, 2026-10-30).

- **Done:** with no `--pack`, the game adds the `openpack` directory
  beside its program (`<dir>/openpack/pack.ini`), or in a macOS
  bundle `Contents/Resources/openpack`. The program's directory comes
  from the system (`src/data/progdir.c`: Windows, macOS, FreeBSD,
  Linux), else `argv[0]`. `--data` and `FLIBUSTIER_DATA` still add a
  player's own data under it; `--pack` replaces it.
- **Saves:** still `--saves`, `FLIBUSTIER_SAVES`, else
  `~/.local/share/flibustier/saves`; on Windows and macOS that path is
  to move to `%APPDATA%\Flibustier` and
  `~/Library/Application Support/Flibustier` with the packages (P3).

### P3. Install rules

Done (2026-10-31); README "Installing with the open pack":

- The pack's licence: CC0 1.0 (maintainer, 2026-10-31) for all of it
  but the remade music (its author's) and the fonts (OFL), in the
  pack's `LICENSE` and `pack.ini`.
- CMake `install()` in the `game` component: `flibustier`,
  `flibustier-sim`, `LICENSE`, `README.md`, `MANUAL.md`, and the pack
  named by `FLIBUSTIER_OPENPACK` without its provenance and coverage
  report. A system layout (`bin`, `share/flibustier/openpack`,
  `share/doc/flibustier`; the game looks there from `bin`, P2) or,
  with `FLIBUSTIER_PORTABLE` (Windows' default), all in one directory.
- The remade music is installed by default (maintainer, 2026-10-31);
  `FLIBUSTIER_OPENPACK_MUSIC=OFF` leaves out its files, its fragment
  and its credits block: a credits block may name the
  asset it credits (`needs`), and goes when no pack gives it. The
  recorded tunes (`pack.d/recordings.ini`, CC0) stay.
- `FLIBUSTIER_BUNDLED_DEPS` builds raylib and FreeType in statically
  (the Windows build already links everything but the system DLLs).
- Checked: Release installs on FreeBSD (system layout, no music;
  portable, with music) and Windows (portable, under Wine) start with
  no option and reach the credits from the installed pack; the FreeBSD
  and Windows screens are identical. The package is 167 MB without
  the music, 223 MB with it.

### P4. One package per platform

| System | Package | Built on | Notes |
|---|---|---|---|
| Windows | zip (and later an installer) | FreeBSD, llvm-mingw cross build (as now) | the pack beside the `.exe`; checked under Wine, then on a real Windows |
| macOS | `Flibustier.app` in a `.dmg` | a Mac (or CI macOS runner) | universal binary (arm64 + x86_64); unsigned at first: Gatekeeper asks once; signing and notarisation need a paid Apple developer account (maintainer) |
| Linux | AppImage, and a `.tar.gz` | CI Linux runner, oldest glibc in use | Flatpak later (Flathub) |
| FreeBSD | a port (`games/flibustier`, pack as `games/flibustier-openpack`) | this host | the pack as its own port: it changes less often and is large |

CPack makes the zip, tarball and dmg from the P3 rules; the AppImage
and the FreeBSD port have their own tools.

**Windows zip, done (2026-10-31):** `cpack` in the cross build
directory makes `flibustier-<version>-openpack-<pack tag or
commit>-windows-x86_64.zip` (223 MB with the music), one folder holding
`flibustier.exe`, `flibustier-sim.exe`, `openpack/` and the documents;
elsewhere it makes the same as a tarball. The game is a windowed program
on Windows (no console window on a double click) that reattaches to the
console it is started from (`src/console.c`). Checked under Wine: the
unzipped game starts with no option from another directory, prints its
messages, honours redirected output, and draws the same credits as the
FreeBSD install. Left: a run on a real Windows, and the program's icon.

**macOS dmg, done (2026-10-31):** on a Mac, `cpack` in a universal
Release build (`-DFLIBUSTIER_BUNDLED_DEPS=ON
-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0`)
makes `flibustier-<version>-openpack-<pack tag or commit>-darwin-universal.dmg`
(221 MB with the music): `Flibustier.app` (bundle id
`org.flibustier.Flibustier`, `cmake/Info.plist.in`, the executable is
`Contents/MacOS/Flibustier`), the pack in `Contents/Resources/openpack`,
the documents beside it, an `Applications` link. `flibustier-sim` stays
out. The install ends with an ad hoc `codesign --force --deep -s -`:
without it the bundle is "not signed at all" (only the linker's
signature on the binary), and `codesign -v --deep --strict` fails.
Checked: `lipo -info` gives x86_64 and arm64; `otool -L` lists only
`/usr/lib` and `/System` (OpenGL, Cocoa, IOKit, CoreFoundation, ...);
`codesign -v` valid on the mounted dmg and on a copy in `/tmp`; the copy
run from `/` with no option draws the credits; a double click opens a
frontmost window and loads CoreAudio. Left: hearing the sound, a run on
Intel, Gatekeeper's first-run prompt on a downloaded (quarantined) copy,
the icon, saves in `~/Library/Application Support/Flibustier`.

**FreeBSD ports, written (2026-10-31), not committed:**
`games/flibustier` (the engine from GitHub at a commit, system raylib
and FreeType, depends on the pack) and `games/flibustier-openpack` (the
pack, `MUSIC` option on by default) in the maintainer's ports tree.
GitHub's archives of the pack hold Git LFS pointers, not the files, so
the pack port fetches release tarballs made by
`tools/openpack_dist.py` (reproducible; the remade music in a tarball
of its own) from the pack's `openpack-<version>` GitHub release. While
the music's author has not given permission, its licence allows no
mirroring, so the official package builders skip the port with the
music on. Checked: both ports stage with no plist issues, with the
music and without; the staged game starts on the staged pack.
Left: the `openpack-0.1` release upload, a poudriere run.

### P5. Releases

- Versions: the engine and the pack tagged apart (`v0.x`,
  `openpack-0.x`); a package names both.
- CI (GitHub Actions): build and test on Linux, macOS and Windows (the
  mingw cross build), make the packages on a tag, attach them to the
  GitHub release. The pack's art comes from Git LFS: CI must fetch it
  (`git lfs pull`), within the free LFS bandwidth or from a release
  asset instead.
- Each package is checked by starting the game headless from the
  package (`flibustier --script ...`) before it is attached.

## Order

P1 and P2 first (they make the open game whole and self-starting,
whatever the licences); P3-P5 once blockers 1 and 2 are settled, since
nothing can be published before.

## Decisions for the maintainer

1. ~~The art's licence~~: CC0.
2. The music: permission from its author, or packages without it.
3. macOS signing and notarisation: pay for an Apple developer account,
   or ship unsigned.
4. Where releases live: GitHub releases, plus the FreeBSD port.
