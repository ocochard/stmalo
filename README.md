# flibustier

An open source engine re-implementation of **Pirates! Gold**, MicroProse's
1993 remake of Sid Meier's *Pirates!*, built on [raylib](https://www.raylib.com/).

The goal is to play the original game on modern systems (FreeBSD, Linux,
macOS, Windows) with faithful gameplay, while allowing fixes and quality of
life improvements the original never had.

## Status

Ready to play, from the intro to retirement and the Hall of Fame: a new
career or a famous expedition in any of the six eras, sailing the
Caribbean, towns (governor, tavern, merchant, shipwright, bank), sea
battles, boarding and duels, attacks on towns by sea and on foot, land
battles and sacks, treasure maps, the lost family and the villains, the
governors' missions and daughters, ransoms, prison, the crew's mood and
food, reefs, and the world's monthly life (diplomacy, conquests, town
events, the treasure fleet and the silver train).

Music and sound play: the game's OPL3 arrangement and FM effects,
emulated, with its digitised effects (cannon, swords, harbour,
sightings). The rules follow the original as traced so far; where a
detail could not be traced,
`aidd_docs/memory/internal/game-rules.md` says so. Please report anything that plays differently
from the original.

## Game data

This project ships **no** original game assets. Graphics, music, text and
other data of Pirates! Gold remain the property of their copyright holders.
To play, you will need a legally obtained copy of the original game; the
engine will load its data files at runtime.

Assets are organized in layered packs: any original asset can be replaced
by a pack using modern formats (PNG, OGG, ...), for example remixed music
(`--pack DIR`; see `aidd_docs/memory/internal/asset-packs.md`).
`tools/music_pack.py` turns the "Pirates! Gold (HQ Remake)" album by
[Pieces of 8-bit](https://soundcloud.com/piecesof8bit) into such music,
and `tools/sfxgen/` renders new sound effects from text prompts with
Stable Audio Open.
The long-term goal is a complete set of assets released under an open
license, so the game can run without any original data.

Do not open issues or pull requests containing original game files.

## Building

Requirements:

- A C99 compiler (clang or gcc)
- CMake 3.16 or later
- [raylib](https://github.com/raysan5/raylib) 5.5 or later (optional: if
  not installed, CMake downloads and builds raylib 5.5)
- [FreeType](https://freetype.org/) 2 (optional: if not installed, CMake
  downloads and builds FreeType 2.14.3)

On FreeBSD: `pkg install cmake raylib freetype2`

On Ubuntu/Debian: `apt install build-essential cmake git libxrandr-dev
libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev libasound2-dev`
(the X11, GL and ALSA headers are needed when CMake builds raylib itself;
tested on Ubuntu 26.04).

On macOS: install the Xcode command line tools (`xcode-select --install`)
and CMake (`brew install cmake`). raylib and FreeType are optional
(`brew install raylib freetype`); otherwise the first configure downloads
and builds them, which needs network access and takes a minute or two.

```sh
cmake -B build
cmake --build build
./build/flibustier
```

Run the tests with `ctest --test-dir build`. Tests that need the original
game data are skipped unless `FLIBUSTIER_DATA` points to the GOG `DATA.DAT`
(or its install directory, or an extracted copy of the CD):

```sh
FLIBUSTIER_DATA=~/games/pirates_gold ctest --test-dir build
```

### Windows

The Windows build is cross-compiled with clang and MinGW-w64, using an
[llvm-mingw](https://github.com/mstorsjo/llvm-mingw/releases) release
(the `ucrt-ubuntu` tarball) as the sysroot. The result is a set of
self-contained `.exe` files with no DLL to ship.

On Linux or macOS, llvm-mingw's own clang is used:

```sh
cmake -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake \
    -DMINGW_ROOT=$HOME/opt/llvm-mingw
cmake --build build-win
```

On FreeBSD, the host clang is used with llvm-mingw's headers and
libraries, so pick an llvm-mingw release built on the same LLVM major
version (libc++ refuses an older clang): with `pkg install llvm19`, that is
llvm-mingw 20250114 (LLVM 19.1.7).

```sh
cmake -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake \
    -DMINGW_ROOT=$HOME/opt/llvm-mingw-20250114-ucrt-ubuntu-20.04-x86_64 \
    -DMINGW_CLANG_SUFFIX=19
cmake --build build-win
```

With [Wine](https://www.winehq.org/) installed, `ctest --test-dir
build-win` runs the tests through it. On Windows, saves go to
`%APPDATA%\flibustier\saves`.

### Installing with the open pack

A package carries the program and the open pack together and starts
with no option. `FLIBUSTIER_OPENPACK` names the pack checkout to
install, `FLIBUSTIER_BUNDLED_DEPS` builds raylib and FreeType in
statically, and the `game` component leaves out the headers and
libraries those two install of their own:

```sh
cmake -B build-rel -DCMAKE_BUILD_TYPE=Release -DFLIBUSTIER_BUNDLED_DEPS=ON \
    -DFLIBUSTIER_OPENPACK=$HOME/flibustier-openpack
cmake --build build-rel
cmake --install build-rel --component game --prefix /usr/local
```

This puts the programs in `bin` and the pack in
`share/flibustier/openpack`, and the documents in `share/doc`.
`-DFLIBUSTIER_PORTABLE=ON` (the default for Windows) puts everything in
the prefix itself instead, as a zip holds it. The pack's remade music,
which belongs to its author, is installed too;
`-DFLIBUSTIER_OPENPACK_MUSIC=OFF` leaves it out, with its credit. The
pack's provenance records stay in its repository.

## Running

```sh
./build/flibustier --data ~/games/pirates_gold [--lang eng|fre|ger, or en|fr|de; es|spa with the open pack]
./build/flibustier --pack ~/flibustier-openpack     # the open game, no original data
```

With no `--lang`, the game speaks the system's language (`LC_ALL`,
`LC_MESSAGES` or `LANG`, else the Windows display language or the macOS
preferred language) when it has text for it, and English otherwise.

With no `--pack`, the game also looks for an `openpack` directory beside
its program, in a macOS application bundle's `Contents/Resources`, or
in an install's `share/flibustier` (see "Installing with the open
pack").

The mouse works everywhere; the keys are the original's, listed screen
by screen in [MANUAL.md](MANUAL.md) (P pauses, Q quits, V steps the
sound level).

`--crt` (or F2 while playing) gives the window a CRT look, like
dosbox-staging's default shader. `--smooth` draws the ship, the
landing party and the clouds gliding between the sailing loop's moves
(twelve a second), and the land battle's units between theirs (every 0.6
seconds), instead of stepping, a move behind the rules; in sea battles,
the ships move by less than a pixel and turn over a few frames; in
fencing, the fighters slide to their new places. On a window larger than
640x480, original sprites move by less than one of their pixels too. `--debug` turns on the original's debug
mode (its `RDM` argument; its keys are in the manual). Careers end by retiring at the bank
(after dividing the plunder) or in prison; the cabin's "Exit game" only
quits. Saves (one JSON file per slot) and the Hall of Fame (a
tab-separated text file) go to `~/.local/share/flibustier/saves`, or
wherever `--saves DIR` says.
`--script "enter down enter ..." out.png` plays the same screens without
a window and saves the last one, for tests and bots.

## Asset viewer

A viewer for the original assets:

```sh
./build/flibustier --data ~/games/pirates_gold --viewer
```

Up/Down picks an asset, Left/Right steps through its frames, PgUp/PgDn
pages (in the sailing world views, they scroll vertically), Tab changes
the palette of sprites, and typing filters the list. Besides the
original files, `world/<year>` shows the sailing world of an era with
its animated sea, and `towns/<year>` the era's towns on a terrain map.

Sounds play when selected (Enter replays): `audio/digital/N` are the
digitised effects, `audio/fm/N` the FM driver's tunes and effects
rendered by the music engine, and any `sound/` or `music/` recording a
pack provides is listed and played too. Filter on `audio/` to find them.

`--list` prints every asset ID. `--export ID out.png` renders one asset
without opening a window. Use `--frame N` to pick the frame and
`--tick N` the animation step. `--pack DIR` adds an asset pack on top of
the original data.

## Headless simulation

`flibustier-sim` runs the game rules without graphics, driven by JSON
commands on stdin, for bots, AI agents and tests:

```sh
echo '{"cmd":"new_game","era":1660,"seed":7}
{"cmd":"advance","days":360}
{"cmd":"state"}' | ./build/flibustier-sim --data ~/games/pirates_gold
```

The protocol is described in
`aidd_docs/memory/internal/sim-protocol.md`.
`tools/career-bot/` holds a bot that plays a whole career through it and
writes a journal, as a long regression test.

## Methodology and legal

flibustier is an interoperable re-implementation. It does not contain the
original game; it reads the files of a copy the player owns.

How the original is studied:

- **File formats** are worked out by examining the game's data files and
  checking each decoder against every file of its kind.
- **Behaviour that only exists as code** (game rules, data tables inside
  the executable, palette animation) is worked out by disassembling and
  decompiling our own copy of the game's executable with
  [Ghidra](https://ghidra-sre.org/). This is done for interoperability:
  to load the original data and reproduce the game's behaviour.

How findings become code:

- Every finding is written down as a specification in plain prose:
  file formats, data table layouts and game rules, with evidence (see
  `aidd_docs/memory/internal/`).
- The engine is written from those specifications. It is not a
  translation of the original program, and it does not reuse its code.
- Numbers that are game data (town tables, start scenarios) are not
  copied into the source. The engine reads them at runtime from the
  player's executable, which it recognises by checksum.

What the repository never contains:

- original graphics, music, sound or text;
- original program code, whether disassembled, decompiled or
  transcribed;
- the output of the analysis tools. `tools/ghidra/` writes it outside
  the repository, and it must stay there.

This is how we read the rules on interoperability, not legal advice. If
you hold rights in the original game and have a concern, please open an
issue.

## Contributing

Contributions are welcome. Please follow the method above: document
behaviour as a specification, then implement from it, and never submit
original assets or decompiled code.

## License

The engine source code is released under the BSD 2-Clause License. See
[LICENSE](LICENSE).

"Pirates!" and "Pirates! Gold" are trademarks of their respective owners.
This project is not affiliated with or endorsed by them.
