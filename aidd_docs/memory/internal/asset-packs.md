# Asset packs and IDs

Implemented in `src/data/pack.c`. The design rationale is decision 1 of
the [roadmap](../../plans/roadmap.md).

## IDs

- Game code loads assets by logical ID, never by file name.
- An ID is lowercase words separated by `/`: `[a-z0-9_-]` plus `/`, with
  no empty component. This also rules out `..`.
- The kind prefix tells the engine which decoder to use for original data.
  For native files, the decoder is chosen from the file extension.

| Prefix | Content | Original file |
|---|---|---|
| `pic/<name>` | full-screen picture | `PCX/<NAME>.PCX` |
| `patch/<name>` | era patch of the overview map | `PCX/<NAME>.RSD` |
| `spr/<name>` | run-coded sprite bank | `SPR/<NAME>.SPR` |
| `cur/<name>` | raw sprite bank | `SPR/<NAME>.CUR` |
| `grp/<name>` | fencing animation groups | `SPR/<NAME>.GRP` |
| `img/<name>` | standalone picture | `<NAME>.IMG`, then `SPR/<NAME>.IMG` |
| `tiles/<name>` | 16x16 or 32x32 tileset | `<NAME>.IMB` |
| `tiles15/<name>` | 15x15 battle tiles | `<NAME>.BMP` |
| `font/<name>` | 1-bit font | `<NAME>.CHR` |
| `ui/<name>` | UI sprite bank | `<NAME>.CRS` |
| `ui/pointer` | the mouse pointer, frame 0, its tip at the top left (packs only; the original's is a picture in the executable) | none |
| `driver/<name>` | sound driver (music, effects) | `<NAME>.PIR` |
| `text/<lang>/<name>` | string table (packs: JSON, see "Open text") | `<NAME>.<LANG>`, e.g. `CSTR.ENG` |
| `program/main` | original executable (data tables) | `PIRATESG.EXE` |
| `music/<tune>` | a recording that replaces a tune (packs only) | none |
| `sound/<name>` | a recording that replaces a sound effect (packs only) | none |

## Replacement sounds (`sound/`)

A pack may replace any of the game's sound effects with a recording, in
any format raylib loads (WAV, OGG, MP3, FLAC, QOA), loaded once at the
start. See "Sound effects" and "FM effects" in game-rules.md for when
each plays.

- **The digitised effects** (`DIGITAL.PIR`): `sound/hail` (a pirate's
  "Arrgh!"), `sound/broadside`, `sound/land-battle` (a voice: "En
  garde!"), `sound/last-man` (the beaten opponent's voice, surrendering),
  `sound/harbour` (a voice), `sound/sail-ho` (a voice: "Sail ho!"),
  `sound/parry`,
  `sound/hit-foe` and `sound/hit-you` (cries of the man hit). A recording plays even without the original driver,
  and wins over the driver's where the game prefers a digitised effect
  to an FM one.
- **The FM effects** (sent to the music driver): `sound/shot-hit`,
  `sound/shot-miss`, `sound/crash` (hulls meeting, a reef),
  `sound/swing`, `sound/blow`, `sound/miss`, `sound/their-men`,
  `sound/our-men`, `sound/scrape`, `sound/trade` (a unit traded),
  `sound/row` (the trade board's row). A recording plays instead of the
  driver's sound.
- An effect without a recording plays as before.
- **The open set:** `tools/sfxgen/sfxgen.py` renders those that are not
  voices (the model cannot speak; the voices stay the original's) with
  Stable Audio Open 1.0 on a ComfyUI server, from the text prompts of
  `tools/sfxgen/sounds.toml` alone (never an original sound as input),
  trims and normalises them with ffmpeg, and writes them to the open
  pack with a provenance record each and the `pack.d/sound.ini`
  fragment. The model needs a Hugging Face account that accepted
  Stability AI's licence; its text encoder is `t5_base.safetensors`.

## Replacement music (`music/`)

A pack may replace any of the game's tunes with a recording, in any
format raylib streams (OGG, MP3, WAV, FLAC, QOA, XM, MOD), detected by
its file extension. The IDs, by driver command (see "Music" in
game-rules.md):

| ID | When | ID | When |
|---|---|---|---|
| `music/title` | intro, welcome menu (9) | `music/trade` | merchant, shipwright (20) |
| `music/lost-treasure` | lost treasure found (10) | `music/plunder` | a capture's or sack's loot (21) |
| `music/family` | dinner with the merchant's family (11) | `music/rescue` | a relative found (22) |
| `music/niece` | the governor's niece (12) | `music/tavern` | the tavern (23) |
| `music/daughter` | the governor's daughter (13) | `music/wedding` | a wedding (24) |
| `music/bank` | the bank (14) | `music/sail-ho` | a sail sighted (25) |
| `music/barmaid` | the barmaid talking (15) | `music/verdict` | a career's end (27) |
| `music/treasure` | a pirate's treasure found (16) | `music/prize` | "Shall we keep her?" (28) |
| `music/news` | the governor's news (17) | `music/prison` | prison, marooned (43) |
| `music/news-titled` | the same, titled by his nation (18) | `music/harbour` | arriving in a town (44) |
| `music/promotion` | a promotion (19) | | |

- **Variants:** `music/<tune>-2` to `music/<tune>-9` are other
  recordings of the same tune; each time the tune starts, one of those
  present (with `music/<tune>`) is picked at random.
- **The shipwright's own:** the game plays the trade tune for the
  merchant and the shipwright alike; a pack may give the shipwright
  `music/shipwright` (with its variants), preferred there over
  `music/trade`.
- The recording plays once, as the driver's tunes do, in place of the FM
  arrangement; asked again while it plays, it goes on.
- A stop fades it out over 3 seconds (the driver's fade); opening the
  cabin at sea cuts it at once.
- The FM sound effects (battles, swords, trade boards) still play over
  it from the driver.
- A tune without a recording plays from the FM driver as usual; a file
  raylib cannot read is reported on stderr and the FM tune plays.

## Pack stack

- The last pack added has the highest priority. The first pack that
  provides an ID wins, so a pack can override a single asset.
- `packs_list()` returns the union of all IDs, sorted and unique.

## Original data pack

`packs_add_original()` accepts any of:

- the GOG `DATA.DAT` ISO 9660 image, read in place;
- the GOG install directory that contains it;
- a directory holding the extracted CD contents, or `PIRATESG.CD` itself.

File names are matched case-insensitively.

## Directory packs: `pack.ini`

```ini
# Comments start with '#' or ';'.
[pack]
name = Pirates! Gold remixed music
license = CC-BY-4.0
art_height = 1080

[assets]
music/title = music/01-title.ogg
pic/town = art/town.png
```

- File paths are relative to the pack directory and may not contain `..`.
- **Fragments:** after `pack.ini`, every `pack.d/*.ini` is read in name
  order, with the same syntax, so that each tool keeps its own list (the
  art generator writes `pack.ini`, `tools/music_pack.py` writes
  `pack.d/music.ini`). The art generator merges into `pack.ini` rather
  than rewriting it: entries already there keep their place, its own
  assets are set or added, and an entry goes only when its file is gone,
  so runs with different specs do not drop each other's entries. A fragment's `[pack]` keys are ignored: the
  pack's name and licence are `pack.ini`'s. The first entry for an ID
  wins, `pack.ini`'s first.
- Unknown `[pack]` keys are ignored, for forward compatibility.
- `art_height`: the screen height the pack's art is drawn for, 240 to
  4320 (default 480, the original). A full-screen picture for 1080p is
  1440x1080; other pictures cover their 640x480 size times
  `art_height / 480`, rounded.
  - The engine renders at the largest `art_height` of all packs, rounded
    down to a multiple of 3 for a whole 4:3 width.
- Planned `[pack]` keys:
  - `format`: version of the manifest format.

## Replacement pictures (`pic/` as PNG)

Loaded by `pic_load()` (`src/assets/pic.c`):

- The true colours are kept at the art's resolution.
- Palette indices are derived too, for code that works on indices
  (colour keys, palette effects, sprites drawn in the picture's palette):
  - the original PCX's palette and pixels when it is installed with the
    same size;
  - otherwise the PNG reduced to the original's palette, or to a fixed
    palette (6x6x6 cube plus 40 greys) without original data.
- Limits:
  - Palette cycling and ink fades do not change true colours.
  - Screens that take their text ink (entry 2) or highlight (entry 6)
    from the picture's palette get an engine orange and pale yellow
    instead when the picture has the fixed palette, whose entries there
    are dark blues.
  - The fleet panel on a replacement picture marks a ship in red, not
    entry 31, and writes its prompt in light parchment, not entry 0:
    the open pack's light plaques and dark wood suit neither.
  - `fb_image_part` draws indices only, so the credits text blocks stay
    in original resolution.
  - The treasure map is composed on indices, so it drops the true
    colours of `pic/wood`.

## Replacement sprites: sprite sheets

Any bank ID (`spr/`, `ui/`, `cur/`, `tiles15/`, `grp/`) may map to a
`.sheet` file, loaded by `bank_load()` and `group_load()`
(`src/assets/sheet.c`):

```ini
# sprites/tavern.sheet
[sheet]
image = tavern.png

[frames]
# frame = x y w h, in art pixels of the image
0 = 0 0 300 200
1 = 300 0 120 80
```

- `image` is relative to the sheet's directory, in the same pack, and
  may not contain `..`.
- Frames are numbered from 0 with no gaps, in any order. Fencing groups
  (`grp/`) number them `bank.frame`, for example `3.0`.
- A frame covers its art size times `480 / art_height` in 640x480
  pixels, rounded, so game code places it as the original frame.
- Frames are drawn on the canvas by their alpha.
- Palette indices and a mask are derived for code that reads them:
  - the original frame's, when it is installed with the same size;
  - otherwise the art reduced to the fixed palette, opaque where alpha is
    at least half, never index 0 when opaque (raw frames use 0 for
    transparency).
- Ship banks may have more headings than the original, frame 0 north
  and clockwise: `spr/ship0`..`spr/ship2` (16 frames in the original)
  any number, and the battle pairs (`cur/ship*a` and `b`, 8 + 8) any
  number, the same in both banks of a pair, the first half of the turn
  in `a`. The bow mark of sea battles keeps the original's 16 places.
- Limits: the title is composed on indices (intro), and so are sprites
  drawn into the treasure map, so those show at original resolution.
  `img/` has no replacement format yet.

## Replacement tiles: tile sheets

Any `tiles/` ID may map to a `.tiles` file, loaded by `tileset_load()`
(`src/assets/tiles.c`):

```ini
# tiles/map.tiles
[tiles]
image = map.png
# tile side in art pixels: 36 for the 16 pixel map tiles at 1080p
tile = 36
# optional: tile count (default 256)
count = 256
# optional: animation frames (default 1), each a full set of tiles
# below the previous one, and sailing passes per frame (default 4)
frames = 8
period = 4
# optional: terrain, the same grid at 640x480 resolution in three
# colours: sea #0000FF, shallows #00FFFF, land #00FF00
terrain = map-terrain.png

# optional, battle tiles: land battle ground per tile, clear, cover,
# swamp or water; unlisted tiles are clear
[ground]
12 = cover

# optional, map tiles: a shore tile's side toward the sea, east, south
# or west, where the prison escape puts a new ship (game-rules.md,
# "Nearest shore"); unlisted tiles are no shore
[coast]
40 = south
```

- Tiles fill the image in rows, left to right, top to bottom; the image
  width sets the columns.
- A tile covers its art size times `480 / art_height` in 640x480 pixels,
  rounded. The game needs 16 pixel map tiles (`tiles/map`) and 32 pixel
  battle tiles (`tiles/battle`), 256 of each.
- Terrain: the world keeps a class per tile pixel. With `terrain`, the
  sheet gives it; without, it comes from the tiles' palette indices
  through the executable's colour classes, which only describe the
  original's indices. A sheet for an open pack therefore needs
  `terrain`, the battle tiles a `[ground]` section, and the map tiles a
  `[coast]` section (without one, and without the original, the escape
  finds no shore and leaves the ship where it was).
- Palette indices and a palette still come with the tiles, for code that
  reads them: the original tileset's when it is installed with the same
  tiles, otherwise the first frame reduced to the fixed palette.
- Animation stands in for the original's palette shimmer of the sea:
  frame `pass / period % frames`, counted in sailing passes (12.2 a
  second). Battles draw frame 0.
- Limits:
  - The treasure map's tiles are composed on indices (original
    resolution).
  - The shimmer pause while the ship touches land is not reproduced.

## Replacement fonts

Any `font/` ID may map to a `.font` file, loaded by `font_load()`
(`src/assets/fonts.c`) through FreeType:

```ini
# fonts/pirate1.font
[font]
file = PirataOne-Regular.ttf
# line height in 640x480 pixels, as the original font's
height = 25
# optional: glyph size (default: height) and baseline below the line top
# (default: the font's ascender), in 640x480 pixels
size = 22.5
baseline = 19
```

- `file` (TrueType or OpenType) is relative to the descriptor, in the
  same pack.
- Original line heights: `pirate1` 25, `pirate2` 15, `pirate3` 14,
  `pirate4` 30, `caps` 34.
- `font/caps` holds the drop capitals, looked up as A-Z then the
  accented capitals (`caps_char()`); the other fonts hold characters
  0x21-0xFF, mapped from Mac Roman to Unicode.
- Text layout stays in 640x480 pixels but moves the pen in 1/16 pixel,
  so the font's own spacing survives; the original fonts advance by
  whole pixels as before.
- Glyphs are rasterised once for the canvas and blended by coverage in
  the text colour. The 640x480 screen gets 1-bit glyphs (coverage at
  least half), so code reading it still works.
- Text set on a full-screen picture gets a soft halo first (light under
  dark ink, dark under light ink), so it reads over a pack's photograph.
  It shows only where the picture already lies on the halo's side of
  the ink, so a red option on black keeps its black. Glyphs without a
  canvas rasterisation (the original's 1-bit fonts) get none.
- No kerning yet.
- The open pack's fonts (IM Fell English for `pirate1`-`pirate3`, Pirata
  One for `pirate4` and `caps`, both SIL OFL) are fetched by
  `tools/font_pack.py`, which also writes their descriptors and
  `pack.d/fonts.ini`. Their sizes and baselines match the original's
  x-height and baseline in each line.

## Open world data

The world's layout and towns, when a pack gives them; otherwise the
original files and executable.

| ID | Format | Content |
|---|---|---|
| `map/<year>` | Tiled map, 640 x 480 cells | the era's map (1560 ... 1680), one tile number per cell |
| `screens/sea` | Tiled map, 15 x 15 cells, 108 tile layers | sea battle screens, in layer order |
| `screens/land` | Tiled map, 15 x 15 cells, 108 tile layers | land battle screens, by town id (the original has 61, the rest empty) |
| `data/towns/<year>` | JSON (`towns.h`) | the era's towns: position, nation, wealth, garrison, harbour, fort and battle cells, dot on the Map of the Caribbean, `arrival` (optional: 1 to 16 `[x, y]` map cells where the town is reached, also the cells the nearest-town search measures to; absent, its cell and the eight around); `treasure_fleet` and `silver_train`, each up to 15 stops as town indices (-1 unused), the convoys' routes (absent: no convoys) |
| `data/treasure` | JSON (`world.h`) | 256 treasure spots, world cells |
| `data/battle-sets` | JSON (`world.h`) | sea battle screens offered by a coast north, east, south, west, or in open sea |

- Tiled maps (`src/assets/tiled.c`): orthogonal, fixed size, one
  tileset, tile layers stored as CSV ("Tile Layer Format: CSV"), every
  cell an unflipped tile 0-255. Object and image layers are ignored,
  group layers refused. Files end in `.tmj` (or `.json`).
- A pack that gives one of these must give it whole and valid: a broken
  file is an error, never a silent fallback to the original.
- `flibustier --export-world DIR` writes the installed world (the original's
  unless a pack replaces it) in these formats, with Tiled tilesets
  `map.tsj` and `battle.tsj` (images `map.png`, `battle.png` to supply)
  and a `pack.ini`. Exported from the original data, it is for local
  checks and as a starting point only: never distribute it.

## Open game data

The tables the rules read, when a pack gives them; otherwise the
original executable's. Plan: [open-game-data.md](../../plans/open-game-data.md).
Implemented with one list of fields per ID (`src/assets/datafields.c`)
that both the loader and the export use.

| ID | Format | Content |
|---|---|---|
| `data/era/<year>` | JSON (`struct world`, `world.h`) | the era's ship speeds, ships met at sea, enemy decisions, diplomacy and shipwright values |
| `data/sea-battle` | JSON (`struct world`) | sea battle hit boxes, shot arcs, the bow cross |
| `data/sword` | JSON (`struct sword_tables`, `world.h`) | the sword fight tables |
| `data/calendar` | JSON (`struct world`) | the length of a day at sea and on foot |
| `data/scenarios` | JSON (`struct scenario`, `scenario.h`) | the 30 start scenarios |
| `data/layout/<screen>` | JSON (`struct ui_tables`, `uitables.h`) | a screen's hot spots and piece positions |
| `data/captain-flags` | JSON (`struct ui_tables`) | the named captains' Jolly Rogers |
| `data/stories` | JSON (`struct ui_story_step`, `uitables.h`) | the career stories, as steps |

- **Shape:** a JSON object whose keys are exactly the fields listed
  below, each an integer or nested arrays of fixed sizes, outer index
  first. A missing field, an unknown one, a wrong size or a value out of
  range is an error naming the ID and the field; it never falls back to
  the original.
- **Values** are the open pack's own; they need not match the
  original's (maintainer, 2026-10-29).
- **Export:** `flibustier --export-world DIR` writes these IDs too, with
  the world (see "Open world data") and the tile sets as sheets
  (`map.tiles`, `battle.tiles`, with their pictures, terrain masks,
  ground and coast): for local checks only, never distributed. That
  export runs the rules with no original data at all.
- **Without the original:** the executable is optional. When the packs
  give no `program/main`, every table above must come from a pack, and
  a missing one is an error naming its ID ("no data/sword: give the
  original data or a pack that has it"). Only the original's own extras
  are then absent: the credits picture's blocks (a pack gives text
  credits), the indexed cloud sprites' shade table (a pack's clouds are
  sheets with their own shadow) and the original mouse pointer (a pack
  gives `ui/pointer`).

### `data/era/<year>`

One per era (1560 ... 1680). See "Speed table", "Encounters at sea" and
"Enemy decisions" in game-rules.md.

| Field | Size | Content |
|---|---|---|
| `speed` | 8 x 16, i8 | per ship class, the speed at each of 16 headings relative to the wind, from running before it |
| `encounter` | 4 x 5 x 7, u8 | ships met at sea: per nation, 5 records of type, nation, crew, guns, role, the loot's gold factor and goods factor |
| `treasure_ship` | 7, u8 | the treasure galleon's record, as an encounter's; its gold factor also sizes a convoy's sack |
| `battle_ai` | 27, u8 | the enemy's decision per comparison total, read `% 3` |
| `diplomacy_range` | u8 | a diplomatic event comes when `rand(range) < 28` |
| `wright_tolerance` | u8 | a Spanish shipwright serves up to `rank x 25 + tolerance` in town wealth |

### `data/sea-battle`

See "Sea battles" in game-rules.md. Types are the 9 ship types, then
the fort.

| Field | Size | Content |
|---|---|---|
| `half_w`, `half_h` | 10, i16 | the hit box's half width and half height, per type |
| `hit_steps` | 5, i16 | the hit box widening, by level |
| `shot_rise` | 16, i16 | a shot's height above its shadow (0 or less), by step of its arc |
| `shot_frame` | 16, i16 | the shot's spread frame 0-3, by step |
| `bow_mark` | 32, i16 | the bow cross's offset around the ship: x, y pairs by heading / 16 |

### `data/sword`

See "Sword fights" in game-rules.md for how each table is used. Moves
are 0-15, levels 0-4 (the difficulty), weapons 0-2 (rapier, longsword,
cutlass).

| Field | Size | Content |
|---|---|---|
| `flag_y` | 6, i16 | the flags' height on screen, by bar position / 8 |
| `frames` | 16, i16 | frames per move |
| `reach_level` | 5, u8 | the player's reach, by level |
| `reach_weapon` | 3, u8 | the player's reach, by weapon |
| `foe_reach` | 3, u8 | the opponent's reach, by weapon |
| `delay` | 4, i16 | the frame time, in timer units, by level (level 4 takes level 3's) |
| `player_edge`, `foe_edge` | i16 | the x at or past which the player is driven back, and at or below which the opponent is driven off |
| `hit`, `foe_hit` | 5, u8 | hit thresholds by level, against `rand & 255` |
| `far_move` | 12, u8 | the move an input becomes at a distance |
| `once` | 16, u8 | non-zero: the move takes its input once |
| `guard` | 16 x 6, u8 | per move and frame: the lines covered (bits: 1 low, 2 middle, 4 high) |
| `strike` | 16 x 6, u8 | per move and frame: the line attacked, 0 none |
| `step_in` | 16 x 6, u8 | per move and frame: the step toward the opponent |
| `step_back` | 16 x 6, u8 | per move and frame: the step away |
| `far_ai` | 8, u8 | the opponent's moves at a distance |
| `caution` | 5, u8 | by level: up close, the opponent follows its script when `rand(16) > caution`, otherwise guards |
| `area` | 16, u8 | the body area a strike hits |
| `scripts` | 3 x 8 x 8, u8 | the opponent's move scripts up close: per bank (by level), 8 scripts of 8 moves; 0x80 and above is a counter to the player's move |
| `line` | 16, u8 | the line of each move |

### `data/calendar`

`{"ticks_per_day": n}`, 16 to 1024: the sailing and marching ticks a
day takes. Each day the calendar moves on and the crew eats. The
wind's shifts and the sightings of sails keep their pace in days: their
periods (128 and 32 ticks of a 64-tick day) stretch with the day, so a
longer day makes voyages take fewer days without making a day at sea
any busier. Battles and moves within a day count ticks. Not in the original's
executable: without the ID the engine keeps the original's 64, with or
without the original data. A pack whose map puts its ports many cells
apart lengthens the day so that voyages take fewer days (a ship sails at
most 6 map cells per 64 ticks).

### `data/scenarios`

`{"scenarios": [...]}`, exactly 30 objects, in the original's order
(game-rules.md, "Start scenarios"): the 24 careers by era (1560 ...
1680) and, within an era, English, French, Dutch, Spanish; then the 6
famous expeditions. A career with an empty name does not exist: the
game hides it and `new_game` refuses it (the original's Dutch 1560
career); the open pack gives all 24.

Nations in `nation`, `relations` and the first four of `standing` and
`rank` are in the town nation order: 0 Spain, 1 England, 2 France, 3
Holland.

| Field | Size | Content |
|---|---|---|
| `name` | text, up to 20 bytes | the career's or expedition's title |
| `nation` | int, 0-3 | the player's nation |
| `relations` | 4 x 4, i8, -1 to 1 | the nations' initial relations, `[a][b]` |
| `standing` | 5, i8 | the player's standing per nation, then the pirates |
| `rank` | 5, u8 | the player's rank (CSTN 78), in the order of `standing` |
| `crew`, `guns` | int | the crew and the guns |
| `gold` | int | gold pieces |
| `food`, `goods`, `sugar` | int | cargo, in tons |
| `food_frac` | int | the food already eaten into, in 256ths of a ton |
| `capacity` | int | the hold, in tens of tons |
| `nships` | int, 0-8 | the ships, then their types in `ships` |
| `ships` | 8, u8 | ship types (CSTN 8 order, 0 pinnace); past `nships`, ignored |
| `start` | 4 x 2, u16 | 4 start positions `[x, y]` in map cells, one drawn |
| `date` | int | an expedition's start: years after the era's first x 360 + the day of the year; 0 for a career |
| `months` | i8 | an expedition's months already at sea; 0 for a career |

The expeditions' eras are the engine's (`expedition_era` in
`src/ui/screens.c`), not part of this file.

### `data/layout/<screen>`

Positions in the 640x480 logical screen. A rectangle is `[x0, y0, x1,
y1]`, inclusive; where a fifth number follows, it is the original's
"on" word. The cabin reads it: a spot with 0 is off, except save,
search and treasure maps, which the rules switch (divide and join
party are 0 in the original's). The fleet panel's is not read. A pack that redraws a screen's picture with
another composition gives its layout with it; otherwise the original's
spots apply to the new picture.

| ID | Field | Size | Content |
|---|---|---|---|
| `data/layout/cabin` | `spots` | 11 x 5 | the cabin's spots, in the order of game-rules.md ("The captain's cabin"): continue, treasure maps, cave, log book, map, search, divide, join, save, exit, practice |
| `data/layout/town` | `spots` | 8 x 4 | the town's buildings: governor, merchant, bank, tavern, shipwright, city information, leave, then one more the engine does not use |
| `data/layout/tavern` | `spots` | 10 x 4 | the tavern's people: map seller, information seller, seaman, bartender, barmaid, stranger, door, three more seamen |
| `data/layout/weapons` | `spots` | 3 x 4 | the weapon choice's blades, by weapon: 0 rapier, 1 longsword, 2 cutlass |
| `data/layout/board` | `pieces` | 29 x 5 | the trading board's pieces: frame, highlighted frame, x, y (frames of the board's sprite bank), and 1 if the loot board shows it too (it has no prices); record 0 is the name tag, whose frames the engine sets by mode |
| `data/layout/cave` | `piles` | 8 x 3 | the treasure cave's piles (`spr/tcave0` ... 7): x, y, and the fortune past which it shows |
| `data/layout/fleet-panel` | `ship_x`, `ship_y` | 8 each | each ship's picture, top left |
| | `spots` | 9 x 5 | the ships' rectangles, then the button's |
| | `type_x`, `type_y`, `damage_x`, `damage_y` | 8 each | each ship's type and damage labels: x their centre, y their top |

### `data/captain-flags`

`flags`: 4 x 8 u16, the Jolly Roger (`pic/fjolly01` ... `pic/fjolly20`,
1-20) a named captain flies, by his id's bits 6-7 and then 3-5 (see
"Which flag" and "Named captains" in game-rules.md). Another value
shows his nation's flag instead.

The credits picture's blocks and ink steps stay the executable's: a
pack gives its credits as text (`text/<lang>/credits`).

### `data/stories`

`{"stories": [...]}`, exactly 30 lists of steps, one per start
scenario in the order of `data/scenarios`; the list for the unused
Dutch 1560 career may be empty. A story plays its steps in order
(game-rules.md, "New career: the story"):

| Step | Meaning |
|---|---|
| `{"text": n}` | show CSTR entry `n` (0-1023) alone on the page; its `%s` are the treasure (CSTR 46), the start town, the year and the player's name |
| `"page"` | wait for a key, then clear the page |
| `"wait"` | wait for a key, keeping the text |
| `"fight"` | the story's sword fight; the story goes on after it |

At most 64 steps a story. The story ends after its last step. From the
original's scripts, the engine keeps the branch for a right answer to
the copy-protection question: the question's `)` becomes a plain
`"wait"`, its `*` and the branch for a wrong answer have no step.

## Open text

The game's text, when a pack gives it; otherwise the original `CSTR` and
`CSTN` files (see asset-formats.md). Implemented in `src/assets/text.c`
(`strtab_from_blob()`, `strgroups_from_blob()`) and
`src/assets/textfiles.c`.

| ID | Format | Content |
|---|---|---|
| `text/<lang>/cstr` | JSON, `.json` | prose and dialogue: tavern talk, governors, missions, battles, menus, the era and expedition texts |
| `text/<lang>/cstn` | JSON, `.json` | word lists: captain names, months, ship types, nations, compass points, the log book's sentences |

- The decoder is chosen by the file extension: `.json` is this format,
  and the original data pack's files stay binary. Both decode to the
  same `struct strtab` and `struct strgroups` (`src/assets/text.h`), so
  the rest of the engine does not change.
- `text/<lang>/cstr`:

  ```json
  {
    "strings": {
      "62": "Pick your voyage:\n~First choice\n~Second choice",
      "119": "These are %s waters, Captain."
    }
  }
  ```

  `strings` maps an index to its text. Keys are decimal indices with no
  leading zero, 0 to 1023. The engine looks entries up by the original's
  index (named `S_*` in `src/ui/screens.c`). Index 322 and above are
  allowed but unused.
- `text/<lang>/cstn`:

  ```json
  {
    "groups": {
      "6": ["Jan", "Feb", "Mar", "Apr", "May", "Jun",
            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"],
      "71": ["Spanish", "English", "French", "Dutch"]
    }
  }
  ```

  `groups` maps a group index (0 to 1023, decimal) to its words, in
  order. The order carries meaning: group 8 lists the ship types in the
  engine's order, group 20 the log's sentences by news code, and group
  13 the compass points clockwise from south (`W_*` in
  `src/ui/screens.c`).
- **Words of the engine's own.** A pack may add words the original
  does not have, after a group's last one; without them the engine
  behaves as with the original. So far: CSTN 74.6, the pattern of a
  crew fighter's label, filled with the nation (CSTN 71) and the title
  (CSTN 74), for example `%2$s %1$s` for "Mutin espagnol".
- **Whole tables.** A pack that gives `text/<lang>/cstr` replaces the
  whole table. An index it leaves out is an empty string, never the
  original's entry; a group it leaves out has no words, and a short group
  has empty words past its end. So a pack can run without the original
  text.
- **Errors.** A file that is not valid JSON, has a key that is not an
  index in range, or has a value of the wrong type is an error when the
  game loads it, with the file and the reason on stderr. It never falls
  back silently to the original.
- **Text.** UTF-8. The fonts draw the Mac Roman repertoire (ASCII plus
  Western European accents and some symbols). A character outside it is
  drawn as `?`, even with a replacement font.
- **Markup**, as in the original (the text renderer, `src/gfx/text.c`):
  - `%s`: an argument the engine supplies (a name, a town, a sum). The
    engine fills the `%s` in order, so an entry keeps the order of its
    arguments; it may leave out trailing ones. `%1$s` to `%9$s` take
    that argument instead, so a translation can reorder them (an
    engine extension: the original has none). `%%` is not a
    conversion: a literal `%` is written alone.
  - `\n` (or `\r`): a line break. The decoder turns each `\n` into
    `\r`, the original's, since some screens test for it.
  - `~` at the start of a line: a menu option the player can select.
  - `` ` `` at the start of a paragraph: the next letter is drawn as a
    drop capital. Elsewhere it is ignored.
  - `^`: an apostrophe. A plain `'` draws the same, and a pack may use
    either.
  - The original's control bytes (0x01-0x1B) and its `0xCA` placeholder
    in `CSTN` are not used by the engine: the renderer skips control
    bytes. A pack does not need them, and may write them as `\u0001`
    and so on.
- **Languages.** `<lang>` is three lowercase letters. The original has
  `eng`, `fre` and `ger`. A pack may add a language (for example
  `text/spa/cstr` and `text/spa/cstn`), and `--lang` accepts any code
  for which the pack stack gives both tables. With no `--lang`, the
  game takes the system's language (`LC_ALL`, `LC_MESSAGES` or `LANG`,
  else the Windows display language or the macOS preferred language)
  when the stack gives its text, and `eng` otherwise. The system gives
  a two-letter code, which only `en`, `fr` and `de` map to: a pack's
  added language is reached with `--lang` only.
- **Writing it.** The open pack's text must be written anew, never
  copied or paraphrased line by line from the original (see
  "Licensing" in the roadmap); [text-catalogue.md](text-catalogue.md)
  says what each entry is for. `flibustier --export-text DIR` writes the
  installed tables of every language in this format, as
  `<lang>/cstr.json` and `<lang>/cstn.json` with a `pack.ini`; line
  breaks are written `\n`. Exported from the original data, it is for
  checking the format and the indices locally: never distribute it.
- **Coverage.** `tools/artgen/coverage.py` counts the text IDs with the
  other assets, and lists per text ID the pack gives the strings the
  engine reads that the original has (not empty) and the pack lacks,
  and the groups it gives fewer words than the engine reads. What the
  engine reads comes from text-catalogue.md.

## Open credits

The credits after the intro, as text, when a pack gives them; otherwise
the blocks cut from the original's credits picture (see "the credits"
in game-rules.md). Implemented in `src/assets/credits.c` (decoder) and
the credits screen of `src/ui/screens.c`.

| ID | Format | Content |
|---|---|---|
| `text/<lang>/credits` | JSON, `.json` | the credits' blocks, in order of appearance |

```json
{
  "blocks": [
    {
      "heading": "Original game",
      "lines": ["First Author", "Second Author"]
    },
    {
      "heading": "Engine and assets",
      "lines": ["A Name"]
    }
  ]
}
```

- **Blocks:** 1 to 16, shown one at a time, as the original's six. Each
  is a `heading` (a string, may be empty) and `lines` (0 to 12
  strings). A block with neither is an error. An optional `needs`
  names the asset ID the block credits: when no pack gives it, the
  block is left out (the open pack's music credit names
  `music/title-2`, a remade variant the original data has not, so a
  package without the remade music does not credit it).
- **Fonts:** the heading in `font/pirate4`, each line in
  `font/pirate1`, so the pack's fonts apply. Lines are one line each:
  `\n` is not allowed. The text markup of "Open text" otherwise
  applies; the decoder drops a leading `~` and every `` ` ``.
- **Layout,** in 640x480 pixels: every line centred at x 508 (the
  original blocks' centre, right of the ship in `pic/credpic`), the
  block centred at y 240. The heading sits on the first line, a gap of
  10 pixels below it, then the lines at the font's line height. A line
  should fit 256 pixels (x 380 to 636); a wider one is drawn centred
  anyway, with a warning on stderr naming the block and line. A block
  taller than 440 pixels is an error.
- **Fade:** the original's timing (20 steps of 40 ms in, a 1.6 s hold,
  20 steps out, 0.4 s with no block). The text is drawn in `ink` (an
  optional top-level `"#RRGGBB"`, default black) at opacity step / 19
  over the picture, so it fades on any background. The 640x480 index
  screen gets the glyphs (1-bit, palette entry 0x20 set to `ink`) from
  step 10 on, for code and tests that read it; so do the original
  1-bit fonts, which have no coverage to fade.
- **Keys:** as the original's: Enter, Escape or the left button during
  any block but the last jumps to the last; during the last, ends the
  credits.
- **Precedence:** the engine looks for `text/<lang>/credits`, then
  `text/eng/credits`; the first found replaces the picture's blocks
  whatever pack the credits picture comes from. Without either, the
  original's picture blocks show, as now. `pic/credpic` is the
  background in both cases.
- **Errors:** invalid JSON, a missing `blocks`, a value of the wrong
  type or a count out of range is an error when the credits load, with
  the file and the reason on stderr; the credits are then skipped (the
  welcome menu follows), never replaced by the original's.
- **Languages:** each language's file is whole. Names stay as written;
  headings are translated.
- **The open pack's credits,** in this order (one block each, the
  wording the pack's):
  1. the original game's authors, as the original's own credits name
     them (names and roles are attribution, not text copied: the
     sentences around them are written anew);
  2. Olivier Cochard-Labbé, for the engine re-implementation and the
     open assets;
  3. Anthropic, for its Claude for Open Source programme;
  4. the ComfyUI and Blender teams, whose tools make the art;
  5. Pieces of 8-bit, for the remade music.

## Languages

The original ships English, French and German (`eng`, `fre`, `ger`).
A language is a three-letter code; what a new one needs, and what the
engine does today with a code it does not know:

| Part | IDs | A new language |
|---|---|---|
| Prose and words | `text/<lang>/cstr`, `text/<lang>/cstn` | given by the pack; `--lang` accepts any code whose two tables the stack gives ("Open text") |
| Credits | `text/<lang>/credits` | given by the pack, else `text/eng/credits` ("Open credits") |
| Trading boards | `spr/buysell`, `spr/buyboard`, `spr/buysell1`; loot board `spr/take-dis`, `spr/takboard`, `spr/takeds1`; French and German under the original's names (`buysellf`, `takbrdg`...) | no ID: the engine picks them by a fixed fre/ger test (`board_files`, `src/ui/screens.c`), so any other code gets the English pieces |
| Plaques (quit, pause, click to go on, go ashore) | `ui/misc`, `ui/miscf`, `ui/miscg` | no ID: fixed fre/ger test in `src/ui/game.c`, English otherwise |
| Credits picture | `pic/credtext`, `pic/credtxtf`, `pic/credtxtg` | no ID (`credits_picture()`); unused when the pack gives text credits |
| Hall of Fame heading | `pic/fame` | the open pack paints "Hall of Fame" into the picture: English in every language |
| Intro lettering | `spr/and`, `spr/present` | the open pack's sprites read "and" and "present": English in every language |

- **Language menu.** The cabin's menu (key `L`) offers only `eng`,
  `fre` and `ger` (`menu_lang`, `src/ui/screens.c`); another language
  is reached with `--lang` only.
- **Answers.** The Y/N prompts take Y, N and the first letter of the
  language's `S_YES` (CSTR 274) unless it is N, so they follow the new
  text with no change.
- **Titles.** A captain's title is set by the pattern in `W_TITLE`
  entry 6 (`TITLE_FORMAT`) of `cstn`; empty, the engine writes "title
  nation", or "nation title" for the Captain title in French.
- **Characters.** Text fonts hold Mac Roman 0x21-0xFF, which covers
  the Western European letters and `¿ ¡`. Drop capitals (`font/caps`)
  are A-Z and À Ä Ç É Ñ Ö Ü only: a paragraph opening on another
  character gets no drop capital.
- **Open pack art.** `tools/artgen/assets.toml` sets the words of the
  boards and plaques per language (`[asset.board.words.<lang>]`,
  `[asset.uimisc.words.<lang>]`, the IDs in each asset's per-language
  table) for `eng`, `fre` and `ger`; `pic/fame`, `spr/and` and
  `spr/present` take a single English text.

So a pack adding a language today brings its text and credits; its
boards, plaques and the three English pictures show in English, and the
cabin menu does not list it. Giving it its own art needs a language
suffix rule for these IDs in the engine (with English as the fall-back)
and per-language words for every artgen asset that paints them.

## Per-town assets

Town screens look up their assets per town (`game_town_asset()`,
`src/ui/game.c`), so a pack can give any town its own:

1. `<base>/<town>`: the town's name in lowercase, each run of
   characters other than a-z and 0-9 a `-` (`pic/tavern/port-royale`,
   `pic/governor/st-augustine`);
2. `<base>/<nation>`: the town's current nation, `spain`, `england`,
   `france` or `holland` (`pic/governor/england`), so a change of
   owner changes the picture;
3. `<base>`: the shared asset, as in the original.

| Screen | Assets looked up |
|---|---|
| Village | `pic/town`, `spr/town` |
| Tavern | `pic/tavern`, `spr/tavern` |
| Bartender | `pic/bartenda` |
| Barmaid | `pic/barmaid` |
| Governor | `pic/governor`, `spr/gov` |

- The clickable spots of each screen come from the executable and do
  not change per town: a town's picture must keep the shared layout.
- Town names come from the town data, so an open pack's own names give
  its keys.

