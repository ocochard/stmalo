# Open game data

Scope (2026-10-29) for running the game with no `PIRATESG.EXE`: every
table the engine reads from the executable gets an open format that a
pack can give, as roadmap decision 1 planned ("Game data ... has its own
documented open text format"). It follows
[open-terrain.md](open-terrain.md), which did the same for the maps,
towns, treasure spots and battle sets ("Beyond terrain" there).

## Where it stands

- The engine refuses to start without the executable: `game_init()`
  (`src/ui/game.c`), `flibustier-sim` (`src/sim_main.c`), the viewer
  (`src/view/view.c`) and `--export-world` (`src/assets/worldfiles.c`)
  all load `program/main` and identify it by SHA-256 first.
  `flibustier --pack <open pack>` alone stops at "missing or unknown
  PIRATESG.EXE".
- Already open, with the executable as fallback: the era maps and
  battle screens (Tiled), `data/towns/<year>` (with the arrival cells),
  `data/treasure`, `data/battle-sets`, the mouse pointer (`ui/pointer`),
  the credits (`text/<lang>/credits`), the text (`text/<lang>/*`), and
  the terrain classes of a replacement tile sheet (open-terrain T2).
- Even then, `world_load()` reads the executable's land and colour
  classes, battle sets, treasure spots and coast before any pack
  replaces them, so the executable is needed anyway.

## The tables left

From `struct exe_layout` (`src/data/exe.h`), by who reads them and the
open home proposed for each.

| Table | Read by | Open home |
|---|---|---|
| era block: `speed`, `encounter`, `treasure_ship`, `battle_ai`, diplomacy range, shipwright tolerance | `world_load()` | `data/era/<year>` |
| `half_w`, `half_h`, `hit_steps`, `shot_rise`, `shot_frame`, `bow_mark` | `world_load()` | `data/sea-battle` |
| `sword_moves`, `sword` (`struct sword_tables`) | `sword_load()` | `data/sword` |
| `scenario` (30 start scenarios, and `raw[0x58]`, the months, read by `sim.c`) | `scenario_load()` | `data/scenarios` |
| `story` (career stories, byte scripts) | `story_script()`, screens.c | `data/stories` |
| `jolly` (a named captain's Jolly Roger) | `encounter_flag()`, screens.c | `data/captain-flags` |
| `board`, `tavern_spots`, `town_spots`, `cave`, `cabin`, `weapon_spots`, `fleet_panel` | screens.c | `data/layout/<screen>` |
| `coast` | `world_load()` (prison escape) | the map tile sheet (open-terrain "Remaining map data") |
| `colour_class`, `land_class`, `tile_class` | `world_load()`, view.c | the tile sheets (open-terrain T2, done); only the read order changes |
| `sine` | `world_load()` | derived by the engine (G1) |
| `shade` (a colour's shadow, the clouds) | `clouds_draw()` | derived from the palette by the engine (G3) |
| `cursor`, `credits` | game.c, screens.c | done: `ui/pointer`, `text/<lang>/credits` |
| `town_va`, `arrival_va` | towns.c | done: `data/towns/<year>` |
| `treasure`, `battle_sets` | `world_load()` | done: `data/treasure`, `data/battle-sets` |

`struct world` already holds every table `world_load()` reads, so the
sim needs no new structure: only where the values come from changes.
The UI tables are the opposite case: screens.c calls `exe_at()` at the
point of use and decodes the bytes there, so they need a structure first.

## Rules shared by every format

The same as the open world data (asset-packs.md):

- JSON, read with `sim/json.c`, one ID per group above, named fields
  that say what a value means (the names in `world.h` and
  game-rules.md), never offsets or raw byte blocks.
- A pack that gives an ID gives it whole and valid: a broken or
  incomplete file is an error naming the file and the field, never a
  silent fallback to the original.
- An ID no pack gives comes from the executable, as today.
- Coordinates in layouts are in the 640x480 logical screen, as the
  engine already uses them.
- Values in the open pack are written anew, never copied from the
  original (roadmap, "Licensing"). The repository holds the formats,
  the loaders and the tests, never the original's numbers.

## Plan

### G1: one source for the tables (refactor, no behaviour change)

Status (2026-10-29): done.

- `src/assets/uitables.c` decodes the screens' tables once, at game
  start, into `struct ui_tables` (`g->ui`): the seven screen layouts,
  the credits' blocks and ink, the story scripts, the captain flags,
  the shade table and the pointer. No `exe_at()` is left in `src/ui/`.
  The story scripts stay bytes for now; G2 gives them steps.
- `struct scenario` has a named `months` field instead of `raw[]`.
- `sine` is computed, `round(255 x sin(pi x i / 128))`, which gives the
  original's 128 bytes exactly (`test_original` compares them; the
  nearest value is 0.012 from a rounding boundary, far beyond libm
  error). No pack gives it.
- Check: `test_original`, `test_ui` and the replays pass; 14 scripted
  screenshots are byte-identical to the build before.
- Moved to G3, where they can be tested: the `world_load()` read order,
  and the shade table for a pack palette.

### G2: formats, loaders and export

Status (2026-10-29): `data/sword`, `data/era/<year>` and
`data/sea-battle`, `data/scenarios`, `data/layout/<screen>`,
`data/captain-flags` and `data/stories` done: G2 is complete (asset-packs.md, "Open game data");
`test_original` round-trips them and refuses a broken file, and the
ten-year replay with the export over the original is identical. The
encounter records are arrays of 7 in a documented order rather than
named fields, so one field list serves every table.

For each ID in the table above:

- The format, documented in asset-packs.md ("Open game data", next to
  "Open world data").
- The loader, filling the structures of G1.
- The export: `flibustier --export-world DIR` writes the world tables
  in these formats with the world, and their lines in its `pack.ini`
  (one command instead of a separate `--export-data`). Exported from
  the original data, it is for local checks and as a starting point
  only: never distribute it.
- Verification, as for T3: `test_original` exports the original's
  tables, loads them back, and compares the structures; the ten-year
  replay through `flibustier-sim --pack <export>` gives byte-identical
  output.

Format notes:

- **`data/stories`:** done, a list of steps per scenario:
  `{"text": <CSTR index>}`, `"page"`, `"wait"`, `"fight"`; a story ends
  after its last step, so there is no `"ask"` or `"end"`. The engine
  converts the original's byte scripts to the same steps: the
  copy-protection question's `)` stays a plain wait, as before; its `*`
  and its fail branch are dropped.
- **`data/scenarios`:** done, with the ship types as numbers in the
  CSTN 8 order, as the other tables' codes. Still from the engine: the
  expeditions' eras (`expedition_era`, screens.c); a pack cannot move
  an expedition to another era yet.
- **`data/layout/<screen>`:** done; the original's `on` words stay
  numbers, unread by the engine. The fleet panel's raw words became
  named arrays (`struct ui_fleet_panel`), and the captain flags keep
  only the 4 x 8 entries the game reads.

Size: medium to large; it can land one ID at a time, each with its test.

### G3: start without the executable

Status (2026-10-29): done.

- `exe_open()` (`src/data/exe.c`) gives the executable when the packs
  have one, and nothing otherwise; an unknown build is still refused.
  The game, `flibustier-sim`, the viewer and the export all start
  without it, and `flibustier-sim` takes `--pack` with no `--data`.
- `world_load()`, `towns_for_era()`, `scenario_load()` and
  `ui_tables_load()` take each table from the packs, or else the
  executable; with neither, the error names the ID.
- The coast moved to the map tile sheet (`[coast]`), with terrain and
  ground already there.
- `shade`: not derived. No simple rule reproduces the original's table
  (the best, the nearest colour to 40 % brightness, matches 162 of
  256), and only the original's indexed cloud sprites use it; without
  the executable they draw no shadow. A pack's clouds are sheets.
- The credits picture's blocks stay the original's; without them, a
  pack gives text credits or the credits are skipped.
- Check: `test_original` exports the world, loads every era, the towns,
  scenarios and screen tables from the export alone with no executable,
  and finds them equal to the original's (but for the tiles' palette
  indices, reduced without the original tiles); a missing table is
  named. The ten-year replay through `flibustier-sim --pack <export>`,
  with no `--data`, is byte-identical to the original's. The game runs
  headless from the open pack and the export alone.
- Found: `data/towns` lists the arrival cells town by town, so a
  converted era keeps the original's cells but not always their order
  (34 of 223 in 1560 move). The nearest-town search keeps the first of
  equally near cells, so a tie could pick another town than the
  original. Fixing it needs a list-wide order in `data/towns`.
- Not done: `tools/career-bot/seamap.c` still requires the executable.

### G4: the content (open pack repository)

Write the values anew for every ID, in the open pack, with a provenance
record each, as for the art. Values are free to differ from the
original's (decision 1).

- **Clean room.** The author of a table never reads the original's
  values: not the executable, not an `--export-world` export, not the
  numbers game-rules.md quotes. They work from a values-free brief per
  table: what each field does, its units and range, and the design
  intent. The brief is checked for numbers before it is handed over.
- **Tooling:** `tools/opendata/` (Python), one generator per table from
  a design file with its rationale, writing the pack's `data/` files,
  their `pack.d/data.ini` fragment and a provenance record each. It
  never reads the original data. The engine checks the output: the pack
  must load with no `--data` (G3's errors name what is missing).
- **Order**, cheapest and least coupled first:
  1. `data/captain-flags`, `data/stories` (on the open text's
     entries), `data/layout/<screen>` (measured on the pack's own
     pictures).
  2. `data/sword`, `data/sea-battle` (hit boxes from the pack's ship
     sheets), `data/era/<year>` (speeds from a sail polar, encounters,
     the enemy's decisions).
  3. The world (open-terrain T4): the era maps from Natural Earth
     coastlines on the pack's tile set, with terrain, ground and
     coast; the battle screens; towns per era from real places;
     treasure spots; battle sets.
  4. `data/scenarios`, last: their start cells need the map.
- **The day:** a ship sails at most 6 map cells per 64 ticks, so on a
  faithful map of the Caribbean voyages take two to three times their
  historical length, whatever the map's projection (a fixed 640x480
  frame cannot bring the far ends closer; a cartogram that empties the
  sea makes coastal routes longer). The day's length became open data
  (`data/calendar`, maintainer 2026-10-29); the open pack sets it from
  the route targets. The wind's shifts stretch with the day. The
  sightings stretched too (2026-10-30: at a fixed tick period a longer
  day had made each year at sea three times as busy, the career bot
  taking 2.5 times the prizes), but at 192 ticks a day a voyage of 250
  cells met about 2 sails where the original's rate gives about 9, and
  play was quiet. They count ticks again (maintainer, 2026-10-31): the
  sea keeps the original's density of ships per distance, and the
  balance of a year is tuned on the pack's side, by the day's length
  (a shorter day gives back the original's pace a year) and by the
  encounter records (what each sail is worth), against the career
  bot's figures on the original data.
- **The open world, merged as one** (the map, the towns, the calendar,
  the scenarios, the battle screens, the treasure spots and battle sets
  and the open-shallows tiles had to reach the pack together: each
  alone, over the original's data, would not match the rest). Merged
  into the pack (2026-10-30), with the open-shallows tiles, the tile
  masks and every table regenerated, and the open text's expedition
  menu dating Hawkins 1568 as his scenario. Left: `pic/citymap`, the
  Map of the Caribbean, redrawn from the open map (asked of the art
  session).
- **Balance:** the career bot (`tools/career-bot`) plays full careers
  on the open data; its `ANOMALY` lines and journals guide tuning.
- Size: large, and mostly design and play-testing, not code.

## Not in this plan, still blocking the no-original milestone

- The sound drivers: the music plays from `driver/psound` bytecode
  where a tune has no recording, and the FM effects need the driver or
  a `sound/` recording each (7 to do). The voices of `driver/digital`
  have no open replacement.
- The open world content (open-terrain T4) and the art left to do
  (open pack `COVERAGE.md`).
- The open pack's licence: still undecided, and the music belongs to
  its author.

## Decisions for the maintainer

1. ~~Should the open values match the original's balance?~~ No: they
   are free to differ (maintainer, 2026-10-29), here and in
   open-terrain decision 2.
2. ~~Format of the tables?~~ Proposed: JSON, one ID per group, as the
   open world data. Open to change before G2.
