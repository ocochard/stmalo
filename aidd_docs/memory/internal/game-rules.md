# Game rules

Behaviour of the original game, described in prose, from analysis of
`PIRATESG.EXE` (GOG build; virtual addresses refer to its flat image, see
[asset-formats](asset-formats.md)). This is the specification the
engine's simulation (`src/sim/`) is written from; the code is not a
translation of the original routines.

Notation: `rand(n)` is a uniform integer in `0..n-1`. Town fields are
unsigned 16-bit values and wrap like them; the names are those of
`struct town` (`src/assets/towns.h`).

## Time

The calendar has twelve months of 30 days (VA `0x1dd0e`):

- **Day of the year:** VA `0x46052`, 0..359.
- **Month:** VA `0x46054`, the day of the year divided by 30.
- **Year:** VA `0x46056`.
- **Advancing:** when time advances by some days, one **world update**
  runs for each month boundary crossed. During that update, the month
  variable holds the number of the month just begun. That number is 1-12,
  and slightly more when a single advance runs past the year end. After
  the updates:
  - if the day of the year reached 360, 360 is subtracted once and the
    year advances;
  - the month variable is recomputed from the day of the year.
- **Engine choice:** the engine advances long periods in steps of at
  most 30 days. Each step then crosses at most one boundary, as in
  normal play.

## World update (monthly)

Runs, in this order (VA `0xceb8`):

1. Town growth.
2. Conquest.
3. Diplomacy.
4. Random town event.
5. The villains' moves (VA `0xcba7`, see "Villains").
6. The treasure fleet's and the silver train's moves (see "The treasure
   fleet and the silver train").

Before them, the player's side (VA `0xceb8`):
- the months since the plunder was divided rise by 1, by 2 with more
  than 32767 gold aboard (see "The crew's mood");
- the fortune rises by half the land count (VA `0x45efe += 0x45f02 /
  2`; each grant is 50 acres);
- the reputation loses an eighth, rounded down (`r -= r >> 3`, a signed
  byte);
- each nation's standing is kept within -100..100.
The date is redrawn when the update runs on screen (VA `0xcfff`).

### Town growth (VA `0xc714`)

Each town, independently, grows with probability 1/4. When it does:

1. Wealth rises by `rand(255 - wealth) >> 5`.
2. Draw `r = rand(wealth)`, using the updated wealth.
3. Citizens rise by `((month * 24 + r) >> 8) & 0xFF`, where `month` is
   the month number of the update (1-12). This adds 0-2 citizens, more
   often late in the year and in wealthy towns.
4. Income `s` is the low 16 bits of `r * citizens`, shifted right by 9.
   Spanish towns get half (`s >> 1`).
5. Gold rises by `s`.
6. Garrison drift: let `d = (s + citizens) / 2 - soldiers`.
   - If `d >= 1` and soldiers are at most 254, soldiers rise by one.
   - Otherwise, if `d < 0` and there are soldiers, they fall by one.

### Conquest (VA `0xc81b`)

1. Pick an attacker `a = rand(towns)` and a target `b = rand(towns)`. The
   target is re-drawn while it is the town whose governor gave the
   mission in progress (VA `0x471ac`, while `0x471aa` is not 0).
2. The target changes hands when all of these hold:
   - the two nations are at war;
   - the target's soldiers, doubled, are fewer than the attacker's;
   - the Manhattan distance between the towns is below 150 map cells;
   - the target is not the game's current town (VA `0x46058`: the
     player's town, or at sea the nearest).
3. The target then takes the attacker's nation and gets a new governor
   (see below), and the news "<nation> captures <town>" is logged.

### Diplomacy (VA `0xc938`)

1. Draw `r = rand(range)`, `range` the era block's byte `0xF8` (VA
   `0x4707e`, copied with the block by VA `0x1361c`; block = era code,
   less 1 past 1, so one block per era): 150 in 1560, 175 in 1600, 140
   in 1620, 120 in 1640, 105 in 1660, 145 in 1680. Nothing
   happens unless `r < 28`.
2. Nations `a = r & 3` and `b = (r >> 2) & 3`. Nothing happens when they
   are the same.
3. The step is +1 when bit 4 of `r` is set, -1 otherwise.
4. Nothing happens if the relation already equals the step, so a step
   never pushes past war (-1) or alliance (+1).
5. Otherwise the relation moves by the step, symmetrically for the
   pair. News is logged, except when the move returns to peace through
   a -1 step (from alliance to peace):
   - "at war" for a new relation of -1;
   - "make peace" for 0;
   - "allies" for +1.
6. On a move to alliance, nation `a` also "offers pirate amnesty", and
   the player's standing with `a` is raised to at least -3.

The initial relations come from the start scenario (below).

### Random town event (VA `0xca7f`)

1. Draw `t = rand(64)`. There is an event only if `t` is a town index,
   so the chance shrinks with fewer towns.
2. Draw the event `e = rand(5)`. "New governor" is re-drawn while a
   mission is in progress and the town is its giver's.
3. The town's flag bit `0x80` moves to bit `0x20`: `0x20` is set if
   `0x80` was set and cleared otherwise, then `0x80` is cleared.
4. Effects:

| e | News | Effect |
|---|---|---|
| 0 | Indians attack | soldiers halved |
| 1 | New gold mine | wealth +64 (255 if it would exceed 254); gold +12 (255 if it would exceed 254) |
| 2 | Malaria strikes | soldiers and citizens halved |
| 3 | New governor | new governor |
| 4 | Pirates plunder | gold set to 0; wealth loses a quarter |

### New governor (VA `0xca51`)

- Field 9 (governor standing, presumably) is reset to 0.
- Unless bit `0x40` of field 11 is set, field 11 becomes `rand(16)`.

## The log and the news

- **Entries (VA `0x47242`, 8 bytes):** the date (`year x 360 + day`),
  the code (CSTN 20) and a value. A line reads "Jan 16:  …" (CSTN 20.0,
  "%s %s:  %s", with the month word of CSTN 6 and `date % 30 + 1`, VA
  `0x1e144`).
- **Sentences (VA `0x1e144`, by code through the table at VA
  `0x346d8`):** CSTN 20.<code>; a town taken for a nation (22) reads the
  town, then the nation's name (CSTN 81) (VA `0x1e407`).
- **The start (VA `0x132f8`, `0x13679`):** a career begins on day
  `rand(150) + 65` of the era's year (March to early August), and the
  log already holds six entries, one per pair of nations, spread from
  New Year to that day: a sixth of the days so far apart, the first at
  half a step. A pair at war or allied logs that (codes 16, 18); a pair
  at peace logs a random town event (codes 11-15, town `rand(towns)`).
- **What others tell (VA `0x1e040`, `0x1e548`):** the bartender (12
  items) and a hailed ship (6) report only the world's news, codes
  10-19, up to today; a town the player took reads as a capture (22 as
  19), one plundered as "Pirates plunder" (3 as 15). They print the
  latest items oldest first.

## Start scenarios

There are 30 records of 90 bytes at VA `0x35b90` (file `0x3a337`):
- 24 career starts, one per era and nation, in the order English,
  French, Dutch, Spanish within each era. The Dutch 1560 record is
  empty.
- Then the 6 famous expeditions (Hawkins, Drake, Heyn, L'Ollonais,
  Morgan, De Pointis).

Fields, as the scenario loader (VA `0x1386c`) uses them:

| Offset | Content |
|---|---|
| 0x00 | name, NUL-terminated (at most 20 bytes) |
| 0x14 | crew (word) |
| 0x16, 0x17 | guns, nation |
| 0x18 | gold in tens (word) |
| 0x1a, 0x1b, 0x1c | food, goods, sugar (tons) |
| 0x1e | food eaten into, 256ths (VA `0x45fa2`) |
| 0x1f | the hold, in tens of tons |
| 0x20, 0x21 | ships, then 8 ship types |
| 0x2a | 5 signed standings (Spain, England, France, Holland, pirates; the pirates' then set to -1), VA `0x45e7a`: the Seahawk 10 with England, Morgan 34, De Pointis 40 with France, -6 with Spain for the expeditions |
| 0x2f | 5 ranks (CSTN 78), VA `0x45e7f`: the Seahawk 2 (Captain), the Costa Garda 1, Heyn 5, Morgan 5, De Pointis 6 |
| 0x34 | 4x4 table of signed bytes: initial relations, indexed `[a * 4 + b]` with the town nation order (Spain, England, France, Holland) |
| 0x44 | 4 start positions (x, y words, map cells), one drawn |
| 0x54 | an expedition's era code (word, VA `0x15bc6`; 0 for careers) |
| 0x56 | an expedition's start date (word): years after the era's first x 360 + the day of the year, stored as the day (VA `0x46052`) and rolled over by the calendar: Hawkins 1569, Drake 1572, Heyn 1628, L'Ollonais 1666, Morgan 1670, De Pointis 1697; the career's start year is `date / 360` (VA `0x13679`); 0 for careers, which start on day 65 + rand(150) (VA `0x132f8`) |
| 0x58 | an expedition's months since the plunder was divided |
| 0x1d, 0x29, 0x55, 0x57, 0x59 | 0 or high bytes of the words above |

The loader also sets the fortune to 200 (VA `0x45efe`).

## Sailing (main loop at VA `0x231b8`)

### Starting a voyage

A start scenario (VA `0x1386c`) sets the player block at VA `0x45f92`:

| Offset in block | Set from scenario | Content |
|---|---|---|
| +0 | `+0x44` | start position x (tiles); one of 4 (x, y) pairs, picked with `rand(4)` |
| +2 | `+0x46` | start position y (tiles) |
| +4 | `+0x14` | crew |
| +6 | `+0x20` | number of ships |
| +0x0a | `+0x16` | guns (judging by the values) |
| +0x08 | `+0x17` | nation |
| +0x1e | `+0x21` | 8 bytes of ship types: 0 pinnace … 7 galleon (CSTN group 8 order) |
| +0x28 | `+0x18` | gold, x 10 |

The town the player is in is the nearest entry of the era's arrival
cells (VA `0x1adfc`, from the scenario loader at VA `0x1386c`). That
list holds (x, y, town) triplets, ending in `0xFFFF`.

### Ship state

- **Position:** world pixels (16 per map cell), x in 65..10175 and y in
  65..7615, each with a 1/8-pixel remainder.
- **Bouncing off the world edges:** the heading is set to face back in,
  east (`0x40`) at the west edge, west at the east edge, south at the
  top and north at the bottom.
- **Heading:** one byte, 256 per turn: `0x00` north, `0x40` east,
  `0x80` south, `0xC0` west.
- **Pace:** each pass (tick) of the loop waits for the game clock to
  advance by 5, or by 1 in the fast mode: 12.2 or 60.9 passes a second.
  The T key toggles the fast mode (VA `0x1b6ef`); the voyage's end (VA
  `0x284b6`) turns it off. The engine does so on reaching a town.
- **Drawing between passes (engine choice, `--smooth`):** the original
  draws the fleet and the clouds where the last pass left them. With
  `--smooth`, the engine draws them on their way from the pass before, one 60 Hz frame at a time
  (the heading too), so they lag the rules by one pass. A move of more
  than 16 pixels in one pass (a cloud coming back at the edge, the
  party landing or boarding) is drawn as a jump. The land battle's
  units glide the same way between its passes (0.6 seconds), a move of
  more than 64 pixels drawn as a jump. Sea battles move every frame
  already: there `--smooth` draws the ships at their exact place
  (the rules keep 1/256 pixels) and turns the drawn heading to a new
  one by an eighth of the way a frame, more than a quarter turn at
  once. Fencers slide to their new place over a fight tick (9 to 12
  frames) instead of at once, a move of more than 48 pixels drawn as a
  jump; their poses change at the tick, as in the original. On a canvas
  larger than 640x480, original sprites between two pixels, and the
  original clouds' shadows, are drawn at their place on the canvas, as
  true-colour art is. The rules are not affected.
- **Ticks:** a tick counter; 64 ticks make a day (engine: a pack's
  `data/calendar` may change that, and the wind's period stretches
  with it; the sightings keep theirs in ticks, asset-packs.md). When a voyage ends,
  the game adds `ticks / 64 + 1` days (VA `0x28789`).
- **The cabin:** Space opens it (key table VA `0x33d90`, Space handler
  VA `0x1b62d`; on foot the table at VA `0x33c74`, handler VA
  `0x1b2e6`). Setting sail sets the input mode `0x4c856` to 3 (VA
  `0x2890b`), so Space sets the flag `0x49b3c` and the loop leaves with
  exit code 0 when word `0x44b26` is 0 (VA `0x24460`), which silences
  the music and opens the cabin (VA `0x28760`). With the mode at 0,
  Space acts as Enter (flag `0x49b3e`). On foot the same exit applies
  (the pass's exit code is 0 unless the walk sets 7, VA `0x235aa`).
  Escape is in neither table: the keyboard reader (VA `0x26844`, BIOS
  keys) passes ASCII through, 0x1b included, and maps the extended keys
  (table VA `0x386dc`, among them arrows to 1-4, Home/PgUp/End/PgDn to
  0x10-0x13), so the original ignores Escape here; the engine accepts it
  as well.

### Each tick

1. **Steering:** a turn input of -1, 0 or +1 changes the heading by
   16 x input, except on ticks where bit 2 of the counter is set, so a
   press can wait 4 ticks.
   - **In the engine:** a press opens that 8-tick window at once: it
     turns on the press and the next 3 ticks while held, then not for
     4; a press while the window is closed is kept and turns when the
     next opens. The ship turns no faster than in the original, and a
     tap released between two passes still steers on the next (an
     engine choice).
2. **Target speed:**
   `(speed_table[class][((heading + 8 - wind) >> 4) & 15] + bonus) x 3`.
   - `class` is the largest ship type in the fleet (`& 7`).
   - `bonus = (5 - d) >> 1`, where `d = clamp(difficulty - navigator,
     0, 4)` and `navigator` is 1 with the navigation ability. The game
     numbers the abilities from 1 in the order of CSTN group 83:
     fencing 1, navigation 3 and gunnery 4 are each tested where they
     belong.
   - **Storms (VA `0x4d292`, the distance to the nearest cloud, see
     "Clouds"):** the multiplier is 6 instead of 3 within 100 pixels of a
     cloud; within `d x 64` the ship is caught: every 16 ticks its
     heading turns 16 toward the wind, and the target speed is halved.
     The distance is measured after the clouds move, so it applies to
     the next tick.
3. **Current speed:** moves 2 toward the target, then is clamped to
   4..12.
4. **Movement:**
   - `dx = sine[heading] x speed >> 8` and
     `dy = sine[heading + 0x40] x speed >> 8`, in 1/8 pixels.
   - `sine` is a 128-entry half-circle table (0 to 255 and back), and
     the second half of the circle is the first one negated. Its
     entries are `round(255 x sin(pi x i / 128))`, so the engine
     computes it.
   - x increases by `dx`, y decreases by `dy`.
5. **Wind:** every 128 ticks, with `w = wind >> 4` and `r = rand(8)`,
   `wind += 16 x ((r - w + 10) >> 1)`, mod 256. It drifts around
   `0xC0` to `0xE0`: blowing toward the west, as the trade winds do.
   The wind starts at `0xC0` on a new game.
6. **Terrain:** the colour of the map pixel under the ship gives a class
   through the 256-entry table at VA `0x4113c`:
   - 3: deep water (40 colours);
   - 1: shallows (16 colours);
   - 2: land (200 colours).

   Odd classes are water; land is where a landing party walks.

### Landing parties (VA `0x231b8`, `0x315d`, `0x24808`)

- **Going ashore:** when the ship's next step is on land outside any
  town's arrival cells (see "Arriving"), the player is
  asked with the language's "Disembark (y/n)" plaque, `misc.crs` frame 8
  at (268, 218) (VA `0x315d`; frame 4 is "Quit (Y/N)", 5 "Game
  Paused"), answered with the keys at VA `0x31610` ("YN", the same in
  every language, upper-cased: Y yes, N no, any other key waits). The
  engine also takes the first letter of the language's "Yes" (CSTR
  274: O in French, J in German) unless it is N, for this prompt and
  "Quit (Y/N)" (an engine addition). No puts the
  ship back where it was and turns it half round. Yes leaves the ship
  where it stood and puts the party on that land.
- **On foot:** the party walks with the input, eight directions (bits 1
  west, 2 east, 4 north, 8 south), half a pixel a pass on each axis
  moved. Time and the wind go on; there are no encounters. In the
  engine a tap released between two passes still walks on the next (an
  engine choice).
- **Sprite:** `sailmisc.spr`, four frames a direction from the table at
  VA `0x36850` (north and standing 0, north-east 4, east 8, south-east
  12, south 16, south-west 20, west 24, north-west 28), stepping every
  four passes. The ship stays drawn where it waits.
- **Back to the water:** within 11 pixels of where the party landed it
  boards again, the ship heading the party's way (table at VA
  `0x367ac`); anywhere else the water stops it. That way can face the
  shore from where the ship waited, and every step onto land asks to
  disembark again; the engine then turns the ship (an engine choice):
  away from the landing point, else 16 at a time either side, to the
  first heading with no land on any of the 16 pixels ahead.
- **Towns on foot:** reached as by sea, on the same arrival cells,
  including inland towns. Going ashore lets a party walk into the town
  the ship just left (after a failed attack, say; an engine choice). The
  arrival offers "March into town" (CSTN 29.5) on `arr_land.pcx`; the
  fort may fire but sinks no ship. In town the ship is not in port: no
  harbour picture, no shipwright. Leaving puts the party on the town's
  own cell (VA `0x19b9e`, branch at `0x19c11`).
- **Attacking on foot:** a land battle first (see below) when the crew
  is at most `soldiers x 15`, the town has forts and more than 9
  soldiers; otherwise straight to the assault. No fort battle on foot.

### The crew's mood (VA `0x1da9b`, `0x1dd0e`, `0x17efb`)

- **Months (VA `0x45e79`, a signed byte):** since the plunder was
  divided. Each month (VA `0xceb8`) adds 1, or 2 with more than 32767
  gold aboard. Careers start at 0, the famous expeditions at their
  scenario's byte 0x58 (3, 0, 1, 3, 0, 3). A new expedition sets -16
  while the refit's days pass, then 0 (VA `0x184a8`, `0x1ef87`); prison
  does the same (VA `0x1f803`).
- **The mood (VA `0x1da9b`, player block +0x18):**
  `(int)(sqrt((float)(gold / 10) / crew) x 2.0) - months + 4 -
  difficulty`, recomputed after every month's update and at the start
  (VA `0x1d8b2`). A capture adds 1, 2 when the ship's role is above 5
  (VA `0xdb7b`), until the next month.
- **It sets** the reload time in sea battles, the crew's fighting factor
  when storming a fort, the bank's mood word (`clamp(mood / 4 + 1, 0,
  3)`, VA `0x1db2c`, CSTN 72), and desertion.
- **At sea, when months pass (VA `0x1dd0e`):** not in town nor ashore,
  and with more than one man:
  - Below -3, mutiny: on `mutiny.pcx`, "Your men confront you on the
    quarterdeck..." (CSTR 139, at (50, 350) up to x 620). A third of
    the crew leave with their share of the gold (VA `0x1f044`:
    `gold -= n x gold / crew`), then a duel on the deck with their
    leader (VA `0x161ba`: scene 3, title 2 "Mutineer", nation 5, extra
    1). The duel's own rules apply (a loss disgraces); then the
    reputation drops by 4 whatever the outcome.
  - Below 0: on `smaster.pcx`, the sailing master warns, "The men be
    growin' restless... they'll mutiny!" (CSTR 140), or after 12 months
    "...anxious to be dividin' up the plunder" (CSTR 210), at (340, 150)
    up to x 620.
  - The engine stops sailing (`SAIL_CREW`) to show it.
- **Roll call on leaving town (VA `0x17efb`):** `n = (3 - mood) x crew /
  15` (16-bit), at most `crew - 1`; when `n > 1` they leave with their
  share of the gold: "Roll call reveals that %s crew members have
  deserted" (CSTR 109, at (340, 150) up to x 620) on the town's arrival
  picture. Then a random one of the 8 prisoner slots (VA `0x4720a`):
  when held and the ship is in port, he jumps overboard and escapes
  (CSTR 110, same place).

### Prison (VA `0x16647`, `0x1f803`)

- **Ways in:**
  - Beaten storming a town (the assault's sword fight, or a land battle
    that leaves fewer than 2 men, VA `0x18d35`): prison in the town.
  - A boarding party beaten (VA `0x16647` from the sword fight): prison
    "in the ship" (CSTN 77.4), after the flagship is lost.
  - The last ship sunk in a sea battle (VA `0x1f2a1`, `from_fort` 0):
    marooned.
  - The last ship sunk by a fort's salvo on arrival, or in a fort
    battle (the battle passes its fort flag as `from_fort`, VA `0xe6b3`
    and `0xe811`): prison in the town.
- **Pages:** on `prison.pcx` (picture 10) at (300, 150) up to x 560,
  "You are imprisoned in %s" (CSTR 175, the town or "the ship"); after
  a fort's salvo (VA `0x1f2a1`) that page and the next ones at (320,
  150) up to x 620 (VA `0x1f320`, `0x1f33c`); marooned, on
  `maroon.pcx` (picture 11) at (100, 320) up to x 620,
  "With your last ship gone, you are washed ashore..." (CSTR 161).
- **Injury:** a defeat (not the ship losses) adds 1 to VA `0x45e88`.
- **The career may end (VA `0x1f1d7`):** a famous expedition (CSTR 159,
  at (100, 40)), or health 3 (CSTR 160, at (200, 200)), on a cleared
  page; then the end of the career (see "Retirement").
- **Health (VA `0x1db64`):** `(years since the start (VA `0x45e85`) +
  injuries) / (3, 4 with medicine)`, at most 3.
- **Placement (VA `0x1c056`, `0x1be71`):** prison in a town puts the
  ship on its water (fields 26-28; the engine leaves as from the town,
  which then counts as just left). Inland towns (no water cell) start
  from the town's cell nudged toward the coast (ids 0x2d: x +50, 0x33:
  x +20, 0x2b: y -20, 7: y +16, 0x1e: y -20, 0x31: y -10), then the
  nearest shore. Marooned: the nearest shore from where the ship was.
  "In the ship": no move.
  - **Nearest shore (VA `0x1be71`):** for r = 1, 2... with n = 2r + 1:
    n cells of row y - r from x + r leftward, n of row y + r from x - r
    rightward, then n - 2 of row y - r + 1 from x - r rightward and n - 2
    of the same row from x + r rightward. The first cell whose map tile
    reads 0x11, 0x22 or 0x33 in the table at VA `0x33b18` (0x80 and 0
    elsewhere) wins: 0x11 steps x + 1 heading 0x40, 0x22 y - 1 heading
    0x80, 0x33 x - 1 heading 0xC0.
  - **Engine fix:** 0x22 marks shores with the sea to the south (the
    step north lands on land for 80% of them, the step south on sea for
    93%), so the original's step (`dec` at VA `0x1c010`) leaves the new
    ship on land for good, a soft lock. The engine steps y + 1 instead.
- **The stay (VA `0x1f803`):** months set to -16, then `reputation / 2 +
  7` spells of 27 days (none when that is not positive); a fresh start
  from the fortune (VA `0x1ef87` with no gold: the fortune loses a
  tenth, the gold is a tenth of the rest); the mission and the 8
  prisoners are cleared.
- **After:** "The months pass quickly..." to "Each month seems an
  eternity..." (CSTR 176-179 by health; at (40, 320) up to x 620 when
  marooned), the difficulty eases by one (not below 0), then "...take
  you back as %s captain" (CSTR 190, or 162 when marooned, with CSTN 73
  for the difficulty).

### The treasure fleet and the silver train (VA `0x13679`, `0xcde9`)

- **Routes:** the era's town block starts with them (VA `0x4605e`,
  `0x4606e`): 15 stops each, town indices (the fleet's from Havana, the
  train's often from Tampico or Nombre de Dios).
- **Where each is (VA `0x45e71`, `0x45e70`):** a town, or 0xFF away (in
  Spain, in Peru); its stop on the route at VA `0x45e8d`, `0x45e8e`.
- **`era` here is the era code (VA `0x45e8c`):** the era menu's choice
  plus 1 past the first, so 1560 is 0 and 1600-1680 are 2-6 (the year
  is `1560 + code x 20`, VA `0xc46e`; code 1 is unused). Odd codes are
  1620 and 1660: the fleet restarts in October (month 9 from 0), in
  September in those two, as the 1987 manual's calendar has it.
- **The start (VA `0x13679`):** stop `(era & 1) x 2 + day / 15 - 18`
  for the fleet, `- 6` for the train (+24 when negative), by the day of
  the year the career starts; the town at that stop, or away past 14.
- **Each month (VA `0xcde9`, after the town events):** from its stop,
  or from the start (-1) in month `9 - (era & 1)` for the fleet, `3 -
  (era & 1)` for the train: the stop moves `rand(2) + 1` on; past 14 the
  convoy is away; a stop at a town not Spanish moves the base one on and
  draws again. Where it stops, the town's wealth (field 8) rises by 10
  (255 at most) and, for the fleet, its gold (field 7) is quartered.
- **In a town:** the arrival says so, an attack meets 10 more soldiers
  each, and off the fleet's town the treasure galleon may be met.
- **The sack (VA `0x22764`):** after "at your mercy" and the son, each
  convoy there is taken: `fleet.pcx`, "You have captured the royal
  Spanish treasure fleet!..." (CSTR 186), or `silvertr.pcx`, "You have
  captured the silver train..." (CSTR 187), at (100, 300) up to x 620;
  the plunder grows by the treasure galleon's cargo byte x 20 (counted
  in tens) for each; log codes 24, 25; the convoy is away until its
  next start (stop 16).
- Log codes 20 and 21 ("Treasure fleet in %s.") are never written.

### The governor's daughter (VA `0x128c1`)

- **Town field 11** (the governor): bit 0 a niece rather than a
  daughter (`niece.pcx`, picture 0x2c, text at (10, 150) up to x 300;
  else `daughter.pcx`, 0x2b, at (350, 100) up to x 620); 0x80 the
  player's informant, 0x40 his wife (0x20 is read with them, not
  identified).
- **Met** instead of the offer (standing 2 or more), unless married:
  interest `(fortune / 499 + rank x 2 + difficulty) / (niece + 1)`; below
  1 she is not met. It is then halved, doubled with the charm ability.
  Her suitor is a captain id `(town & 3) x 8 + nation x 32 + niece x 2 +
  1`. "The governor introduces you to his young %s. She is presently
  being courted by %s..." (CSTR 141, CSTN 89): conversation or proposal.
- **Conversation:** when `rand(interest) > 2`, "The governor's daughter
  is overwhelmed by your charm and wit..." (CSTR 146): she becomes an
  informant (0x80) and the romances (VA `0x45e87`) count one more;
  otherwise nothing is said.
- **Proposal:** interest below 4 is refused: "Well! What cheek!..." (CSTR
  142, with the difficulty's name, CSTN 35) for 0-1, "...my suitor is a
  wealthy and very distinguished man..." (143) for 2-3. Otherwise "You
  have captured my heart, %s %s..." (144, the player's rank and name):
  fight the duel or retract. The duel is in the alley (scene 0), against
  the suitor, labelled with his name. Won: `wedding.pcx`, "Amid
  merriment and celebration..." (CSTR 145 at (360, 150) up to x 620);
  married (VA `0x45e86` = niece + 2), the town's flag 0x40 and word 9
  + 2. Lost: the duels' disgrace.
- **Later visits** (0xE0 set): "After dinner, you visit your %s..."
  (CSTN 26.0, "informant" or "wife"), then where the treasure fleet and
  the silver train are (VA `0x1e953`, `0x1ea24`), as a hostage tells.
- The merchant's daughter: see "Merchant".

### Named captains (VA `0xd1fc`, `0xdb7b`, `0x127da`)

- **Who:** a ship that chases the player (role above 5, the player's
  standing with her nation below 0) has a named captain; the mission's
  pirate is the hunted one. Both are named at the encounter's third
  question (VA `0xd60d`-`0xd71d`), so a ship left earlier names nobody.
- **The id:** `surname x 8 + rand(8) x 2 + nation x 64`. The surname is
  `rand(5)`, moved on while used lately (a 5-bit mask at VA `0x32e06`,
  cleared when all five are used, 0x1F); the nation is the ship's, or
  for pirates the nearest town's; an id of 0 is drawn again. The mask
  starts at 15 in the program's data, surnames 0-3 used, so a game's
  first captain gets surname 4. `rand(8) x 2` reaches bit 3, so it can
  carry into the surname field, which then reads up to 5. Named as captains are (VA
  `0x19fbb`): rank CSTN 78 entry `(id & 7) + 2`, surname from CSTN 2 +
  `(id >> 6 & 3)` entry `(id >> 3) & 7`.
- **Battle:** the level rises by `(id & 7) / 3` (1 for the hunted
  pirate).
- **The hail (CSTR 121):** "Arrgh! It's the %s %s, %s!": his repute
  (CSTN 34 by the same bonus), "pirate" or "pirate-hunter" (CSTN 105),
  his name.
- **Captured (VA `0xdb7b`):** a Spaniard or a pirate (nation & 3 = 0)
  first speaks on `hostage.pcx` at (340, 150) up to x 620: "I have
  information which may be of value to you," %s says (CSTR 165): the
  treasure fleet (VA `0x1e953`), the silver train (VA `0x1ea24`), or
  hold him for ransom. News is "The %s is in %s." (CSTN 27.0) with
  "Treasure Fleet" or "Silver Train" (CSTR 46, 45) and the town it is
  in, or "Spain" / "Peru" (CSTN 27.1, 27.2) when away (VA `0x45e71`,
  `0x45e70` at 0xFF). News lets him go. Others are held at once.
- **Held:** the first free of 8 slots (VA `0x4720a`) takes `id |
  (nation & 4) << 6` (0x100 marks pirates; Hostage Status calls them
  "(pirate)", others "(hunter)").
- **Ransom (VA `0x127da`), at each governor's visit after the
  promotion:** 4 tries, each a random slot; a held one draws `(rank +
  1) x 50 x (rand(3) + 1) x 10`: "The family of %s will pay a ransom of
  %s gold pieces..." (CSTR 136, Take the gold / Wait for a better
  offer). Taken, he is freed.
- **Lost:** prison clears the slots.

### Villains (VA `0x12178`, `0x18913`)

- **Four slots (VA `0x471da`, 12 bytes each):** +0 the nation of the
  towns he moves through, plus 1 (always 1: Spain; 0 a free slot), +1 6
  (written when he is named, VA `0x12178`, and never read), +2 surname (0-15), +3 rank (0-7), +4 nationality for
  his name (0-3), +5 the town he hides in, +6..+9 the towns before,
  latest first ("none" at first).
- **Each month (VA `0xcba7`):** among the towns of his nation (all but
  the last in the table) with town field 10 at 0, and within `rand(
  difficulty + 5)^2` cells (`|dx| + |dy|`, a draw per town) of his own,
  the best by `4 x field 9 - (gold - soldiers) / 8 + the distance from
  the nation's alarm town - 255`. Moving, his town shifts into his trail,
  field 10 is cleared there and set where he goes (a land battle meets
  `field 10 x 128` more men). Beaten, he leaves field 10 set.
- **The alarm town (VA `0x45eae`, per nation):** on sailing out of a
  town (VA `0x28789`) with the heat above 15, that town.
- **News (VA `0x1eba6`):** where a villain's town or trail holds a
  town: "The %s quite familiar with recent comings and goings of
  prominent persons of %s. Do you wish to inquire about..." (CSTR 154,
  CSTN 16 citizens, bartender, traveller, crewmen; the villains one a
  line, joined by "\r~"; "No one?"), then "%s is now in %s." (108) or
  "%s left for %s a few months ago." (107, the next town of his trail),
  logged (code 9), and the question again, unless only one villain was
  listed (VA `0x1ef56`). The entry that matched (his town or a trail
  step) is rewritten as the town plus 0x80 (VA `0x1ee67`); the matching
  reads the town through `& 0x3f`, so he stays listed, and no reader of
  the mark was traced. Told by the citizens after a
  sack (`town.pcx`, (20, 50) up to x 250), the bartender after his
  rumours (`bartenda.pcx`, (320, 150) up to 560), the traveller after
  his town's information (`infoman.pcx`, (20, 150) up to 280), and a
  crew captured near a town (`crew.pcx`, (40, 350) up to 560; see
  "Near a town").
- **Named (VA `0x12178`):** at a promotion, unless the fourth slot is
  taken; the first free slot. His town is drawn `rand(towns)` again
  until a Spanish one; then surname `rand(16)`, rank `rand(8)`, nation
  `rand(4)`. On `evilone.pcx` at (340, 150) up to x 620: "I have
  recently had news of your long-lost %s... The evil %s %s knows the
  whereabouts of your %s!" (CSTR 137: the relative, CSTN 97 the
  nationality, the name), then "He was last seen in the city of %s"
  (CSTR 138). Log code 9, "%s in %s."
- **His name (VA `0x1a040`):** the rank (CSTN 78) and the surname from
  CSTN group 2 + nation (Montalban, Delgado...).
- **Met (VA `0x18913`):** in his town, at the tavern (the mystery man),
  the governor's mansion and after a sack. On `evilone.pcx`: "You spot
  the evil %s %s sneaking out a back entrance" (CSTR 131). He moves to
  `rand(towns)`, then the duel (VA `0x161ba`: scene 0, the alley, one
  man each at factor 9, labelled with his name).
  - Beaten: his slot is cleared, "I will tell you what I know. Your %s
    is held captive on a remote plantation" (CSTR 132) and a piece of
    the relative's map (VA `0x1f08a`, map 1).
  - A loss is the duels' disgrace (reputation -1).
- **A relative rescued** clears all four slots.

### Missions (VA `0x12e88`, `0x1f8f7`)

- **State (VA `0x471aa`):** the type (1 letter, 2 papers, 3 pirate, 4
  the son; negative once fulfilled), the giving town, the destination,
  the giver's nation, the hunted pirate's id.
- **At the giver's governor**, with a mission under way:
  - Fulfilled: "I am greatly pleased..." (CSTR 209), or for the pirate
    "So you have brought me that despicable pirate, %s" (CSTR 133). The
    standing with the giver's nation rises by 1, and the town's word 9
    drops by the type.
  - Not fulfilled: "I am disappointed..." (CSTR 199); standing -1 with
    the town's nation, its word 9 -1, log code 36.
  - Either way the mission ends. Elsewhere nothing is said.
- **An offer**, with none under way:
  - Draw: type `rand(5) + 1`, then up to 10 towns `rand(towns)` other
    than this one. Papers and the son need an enemy town (relation
    below 0); the letter a town not at war; the pirate a town of the
    same nation without flag 0x40. After 10 failures the type is drawn
    again.
  - Offered when type 5 was not drawn, `standing >> 2 >= type`, and
    the town's word 9 is not negative. "I have an urgent mission for
    you... Accept / Decline" (CSTR 200).
  - Accepted: the task (CSTR 201 letter, 202 papers, 204 son, with the
    destination; CSTR 203 the pirate, with his name and the
    destination). Logs 28, 29, 31; for the pirate log 30, his id
    `(nation << 2 | rand(4)) << 3 | 0x8000`, named as captains are (VA
    `0x19fbb`: rank CSTN 78 `(id & 7) + 2`, surname CSTN 2 + `(id >> 6 &
    3)` entry `(id >> 3) & 7`).
- **Fulfilling (VA `0x1f8f7`):** CSTR 204 + type at (200, 200) up to x
  620, the giving town and the pirate's name; logs 32 to 35 (34 "Captured
  the pirate %s"). The original first clears the screen (VA `0x262d3`);
  the engine writes on `textback.pcx`.
  - Letter, papers: the mystery man in the destination's tavern.
  - The son: the sack of the destination (after "at your mercy").
  - The pirate: off the destination (the encounter's nearest town), four
    times in five the ship met is the mission nation's first pirate
    record; a pirate ship there is him (VA `0xd1fc`), cannot be avoided
    once seen, and capturing it fulfils the mission (VA `0xdb7b`). He
    hails and fights as a named captain (see "Named captains").
- Prison clears the mission (VA `0x1f803`).

### Treasure maps (VA `0x1f08a`, `0x29ece`, `0xa801`)

- **Three maps (VA `0x47186`, 12 bytes each):** the pirate's (0), the
  lost relative's (1), the lost treasure's (2). Each holds the pieces
  (+0: bits 1, 2, 4, 8 for the top-left, top-right, bottom-left and
  bottom-right quarters, 0x10 once shown), the spot (+2, +4, in map
  cells), the lost treasure's people (+6, map 2 only) and the town near
  the spot (+8). A new game clears the pieces.
- **A piece (VA `0x1f08a`):**
  - Map 2 first takes the next lost relative's number as its people
    (VA `0x45e78`: 0 sister, 1 father, 2 niece, 3 uncle; CSTN 37).
  - With no piece yet, the spot is drawn: one of the 256 cells at VA
    `0x34228`, again while the nearest town (VA `0x1adfc`, by
    `|dx| + |dy|`) is the one the player is in or any map's town (an
    empty map keeps its last town, 0 at first). The original draws for
    ever if none qualifies; the engine takes the 2048th draw.
  - Unless the map is whole, a missing quarter is added: `8 >> rand(4)`,
    drawn again while held.
  - On the easiest level the map is whole at once. The map is then
    shown (VA `0x29ece`).
  - Givers: the map seller and the barmaid (map 0), a beaten villain
    (map 1, VA `0x18913`), and a rescued relative (map 2).
- **The page (VA `0x29ece`), in `tmap.imb`'s palette (`tmap16.imb` in
  16 colours):**
  - `wood.pcx`, then each held quarter of `mapframe.pcx` over it,
    rectangles (0, 0)-(318, 251), (319, 0)-(639, 251), (0, 252)-(318,
    479), (319, 252)-(639, 479) (VA `0x41438`).
  - Torn edges along the missing quarters, `treasmap.spr` frames 5-16,
    one set per combination of quarters (frame, x, y):

    | Held | Edges |
    |---|---|
    | 1 | 5 (319, 30), 6 (28, 252), 13 (318, 252) |
    | 2 | 7 (306, 34), 8 (318, 252), 14 (307, 252) |
    | 3 | 6 (28, 252), 8 (318, 252) |
    | 4 | 9 (319, 252), 10 (28, 238), 15 (319, 244) |
    | 5 | 5 (319, 30), 9 (319, 252) |
    | 6 | 9 (319, 252), 10 (28, 238), 7 (306, 34), 8 (318, 252) |
    | 7 | 9 (319, 252), 8 (318, 252) |
    | 8 | 11 (309, 252), 12 (319, 240), 16 (311, 241) |
    | 9 | 5 (319, 30), 6 (28, 252), 11 (309, 252), 12 (319, 240) |
    | 10 | 11 (309, 252), 7 (306, 34) |
    | 11 | 11 (309, 252), 6 (28, 252) |
    | 12 | 10 (28, 238), 12 (319, 240) |
    | 13 | 5 (319, 30), 12 (319, 240) |
    | 14 | 7 (306, 34), 10 (28, 238) |

  - The world in `tmap.imb`'s parchment tiles, 40 x 30 cells from the
    spot less (20, 15), shows through colour 6 (2 in 16 colours).
  - The mark at the centre: map 1 draws frame 0 at (312, 230) when the
    relative is 1 (frame 0 looks female, though 1 is the father), else
    frame 1 at (313, 225); the others the cross, frame 2 at (312, 229).
  - The title, centred at the top in font 0 and ink entry 1: CSTN 31.0
    "Pirate treasure map", 31.1 "Map to lost %s" (the relative), 31.2
    "Map to lost treasure". Then CSTN 31.4 "Somewhere near %s" (the
    spot's town), centred at y 460 in font 1. A click or Enter ends it.
- **Searching (cabin spot 6, VA `0xa801`):** also S on foot, without
  the cabin (key table VA `0x33c74`, handler VA `0x1b3df`: the walk
  leaves with exit code 7, VA `0x2849c`). 3 days pass (VA `0x1e018`).
  Every map with pieces whose spot is within a cell of the party (`|dx|
  < 2` and `|dy| < 2`, in cells) is dug up, whole or not, and cleared;
  texts in ink entry 1:
  - Map 0: `find-t.pcx`, "Your digging uncovers a chest of solid oak!
    ... %s gleaming pieces" (CSTR 183 at (100, 300) up to x 560), gold
    `(rand(9) + 5) x 1000`; log code 8.
  - Map 1: the relative's picture (`reunionf.pcx` for 0 and 2,
    `reunionm.pcx` for 1 and 3), CSTR 184 ("Your %s is rescued!") at
    (340, 150), (320, 150) for the men, up to x 620; then CSTR 188, the
    tale of the lost treasure of the people CSTN 93 (Incan, Aztec,
    Olmec, Mayan) numbered as the relative; then map 2's piece (and its
    page); log code 26 with the relative, who becomes the next one. The
    four villains (VA `0x471da`) are cleared: every slot freed, no towns
    behind them.
  - Map 2: the people's picture (`inca`, `aztec`, `olmec`, `maya.pcx`),
    "You have found the lost %s treasure!" (CSTR 185 at (100, 320) up
    to x 560), 100000 gold; log code 7.
  - Nothing: `search.pcx`, "After three days of fruitless digging..."
    (CSTR 189 at (100, 300) up to x 560).
  - Then the march goes on.

### Land battles (VA `0x5a80`, `0x72ac`, `0x5d40`)

- **The field:** the town's LBTL screen (town field 14, the same index
  as its sea battle screen), 15 x 15 cells of 32 pixels; `lbat.pcx`'s
  panel on the right. Terrain per tile (table at VA `0x3235c`, grouped
  by VA `0x7197`): code above 7 water, 5-7 clear, 4 swamp, below 4
  cover (CSTN 32: clear, cover, swamp, water).
- **Units (16 bytes at VA `0x45f10`):** position, men, flags (type in
  the low bits: 1 party, 2 soldiers, 3 the fort, 4 horsemen; a unit to
  join `<< 4`; 0x80 exposed), muskets, morale (0..79), goal.
  - The party, units 1-3: half the crew, then the rest, halved again
    above 100; around town fields 22, 23 (`x 32`), the second a cell
    up and left, the third down and right; muskets `rand(6) + 10`.
  - The town, units 7 down to 4: `field 10 x 128 + soldiers x 10 + 5`
    men in halves (the last takes the rest, as does any half below
    50). With forts the first is the fort, at the town's cell (fields
    15, 16) and half strength; without, unit 4 is horsemen without
    muskets. The others take the clear cells nearest the town. Muskets
    `(difficulty + 1) x 2 + rand(6)`.
- **A pass, every 0.6 s (VA `0x8ac`, 600 ms of DOS time):**
  - Every 4 passes the men are counted: no defenders, won; fewer than
    2 attackers, lost. Every 64 passes the town is "ahead" when its men
    outnumber half the exposed attackers; after 1000 passes without a
    fight the attack fails.
  - The button (Space, Enter, a click), pressed anew, selects the next
    unit of the party; held, every unit without a goal follows the
    arrows. The selected unit follows them anyway. In the engine an
    arrow tapped and released between two passes still counts on the
    next, as the button does (an engine choice).
  - The selected unit within 32 pixels of the fort wins (VA `0x7885`).
  - Each unit: exposed unless in cover; speed `(9 / (k + 2)) x 2`
    with `k = terrain + 1`, kept to its low 2 bits for the town's units
    (slots 4-7): in water the attackers crawl (2) while the town's men
    run faster than on clear ground (8 against 6), +4 for horsemen on clear
    ground, half again with morale above 16; with a goal it walks to it
    (and joins its unit there), otherwise it follows the arrows at half
    speed. Off the field it is gone. Every 8 passes, staggered, its
    morale rises by 3 and the exposure clears.
  - Every 16 passes, staggered, each unit faces the nearest foe it can
    see (a foe in cover only within 31 pixels; distance: the longer
    side plus half the shorter). Within 32 pixels they fight (VA
    `0x664c`): `men x morale / 80` (x 4 for horsemen) x 32, halved for
    a target in cover, doubled in water, quartered against the fort
    (which ends the battle, won). With `r = rand((v + 16) / 31)`, the
    target loses `r / 2 + 1` men, then `r x 128 / men left` morale
    (none left: 0), clamped to 0..255; at 0 morale an eighth of its men
    flee.
  - The town's units then choose: the fort rallies the others; any other
    advances on the foe, holds, or falls back to the fort (and joins it)
    by comparing strengths (VA `0x5d40`, `0x6931`). A threshold `d`
    starts at 1, 2 for horsemen; it is -1 when the foe has fewer than
    half its men (horsemen count their men double); then, except for
    horsemen, 2 when its firepower, `(men x muskets / 16) x morale /
    65`, halved when the foe stands in cover, is below the foe's `men x
    muskets / 16`; and 99 when the town is not "ahead". With the foe
    `distance / 16` cells away: farther than `d`, it advances on him;
    exactly `d`, it holds; nearer, it falls back. This is the only use
    of the muskets besides the panel (VA `0x6c9a`): nobody fires from
    afar, and moving or standing still does not change a fight (the 1987
    game's rule that muskets fire only when standing still is not in
    Pirates! Gold). A unit below 16 morale near a foe backs away.
- **Afterwards:** the crew is the party's men (at least 1), the town's
  soldiers a tenth of its men; heat rises by `passes / 99`. Won: the
  assault follows. Lost: "Your men are scattered and exhausted. The
  attack on %s has failed." (CSTR 191 at (100, 200) up to x 620, log
  23); a lone captain (crew 1) goes to prison without it (VA `0x5b61`,
  `0x18d35`).
- **Then** the crew check (see "Manning the fleet").
- **The cabin:** Save game only in town, Search only on foot outside a
  town (the spots' flags are set from the arrival mode, VA `0xa11c`).
  Dividing and joining parties are unreachable in the original.

### Food (VA `0x231b8`, `0x2849c`)

- **Eating:** at sea and on foot, every 64 passes, two before each
  day's (`(pass + 2) & 63 == 0`), with ships and crew: `food x 256 +
  fraction` (the fraction at VA `0x45fa2`, a career's from its start
  record's byte `0x1e`) loses one per man, floored at 0. A ton feeds 256
  men a day; the days left are `(food x 256 + fraction) / crew`, as the
  Party Status shows.
- **Warning:** under 21 days, when the days are a multiple of 4, on
  `smaster.pcx` at (340, 150) up to x 620: "Captain, we have only %s
  days food left!" (CSTR 124).
- **Starving:** at 0, "The crew is starving! A surly group steals a
  small boat and deserts, taking a portion of the plunder." (CSTR 180;
  on foot CSTR 181, without the boat); a third of the crew leave with
  their share of the gold (VA `0x1f044`), and food becomes 5 tons.

### Reefs (VA `0x231b8`, `0x2849c`)

- At sea, not on foot, on a shallows pixel, when the fleet's largest
  class is above 1 (bigger than a sloop) and a wait of 100 passes has
  run out: the wait restarts, and the last ship bigger than a sloop
  grinds onto a reef, on `smaster.pcx` at (340, 150) up to x 620: "With
  a jarring crash, one of your ships grinds onto a shallow reef and
  rapidly fills with water!" (CSTR 182). She is lost with her share of
  the cargo and gold (VA `0x1f2a1`), and the crew is cut to the rest's
  `(class + 2) x 32`. When she is the last ship, the player is
  marooned, as after a sea battle (see "Losing a ship" and "Prison"),
  once the master's word is heard.
- Sails and speed play no part; small ships are safe only because
  they never count.
- The original sizes the fleet once a voyage (VA `0x24523`), reading
  every other slot up to twice the count (a quirk); the engine looks at
  every ship.

### Manning the fleet (VA `0x1f65f`)

Run at each start of sailing (VA `0x28789`), after leaving a town (VA
`0x17efb`) and after a lost land battle:
- While the crew is below 8 men a ship: with several ships, "Your crew
  is too small to man your ships! Which will you abandon?" (CSTR 112),
  chosen on the fleet panel; the ship is left with her cargo kept (VA
  `0x1f2a1` with keep). With one: on `plunder.pcx` at (100, 40) up to x
  620, "You do not have enough crewmen to man your last ship, so
  regretfully you abandon it." (CSTR 127), then the last ship's loss:
  marooned, or imprisoned in the town when reached by sea.
- Then, while the hold overflows (`capacity x 10 - guns / 2 - food -
  goods - sugar < 0`), the trade board in its overboard mode (VA
  `0x2264f` mode 1) with "You have more cargo than will fit in your
  hold! You must throw some overboard!" (CSTR 113): goods sold for
  nothing, nothing to take, no leaving until it fits.

### Arriving (VA `0x231b8`, `0x29dd9`)

- **Arrival cells:** each era has a list of map cells, each naming the
  town reached there (VA `0x13555` copies it to VA `0x4ca58`; layout in
  asset-formats.md). A town has 2 to 13 cells, mostly its own and some
  next to it, at most 3 cells away; the harbour mouth (fields 26, 27) is never
  one, so a fleet leaving a town does not re-enter it.
- **Nearest town (VA `0x1adfc`):** the town of the cell nearest a map
  cell by `|dx| + |dy|`, the first in the list on a tie. The sailing
  loop's nearest town (sightings, the town an encounter or a battle
  belongs to), the start town and a treasure spot's town all come from
  it; with no world loaded the engine measures to the towns' own cells.
- **Test:** every pass, after the move and before the terrain test, the
  cell under the ship or the party (`x >> 4`, `y >> 4`) is looked up;
  the first matching entry wins (VA `0x29dd9`). A match stops the
  voyage with code 4, the arrival (VA `0x24510`), even when the step is
  onto land: the town's shore takes the ship into port instead of
  asking to disembark.
- **On foot after leaving a town:** when the loop starts with the party
  ashore (VA `0x3684e`), no town counts until the party stands on no
  arrival cell; leaving a town on foot puts it on the town's own cell.
- **At sea, no such wait:** the loop also keeps the start position (VA
  `0x367fc`, `0x367fe`) to skip an arrival on that cell, but stores it
  in pixels (`x 16`, VA `0x2329d`) and compares it with a cell, so it
  never matches.
- **Engine:** the ship stays on the water when the step was onto the
  town's land, and, with land within a ship's length ahead, turns away
  from the town to the first heading with no land on any of the 16
  pixels ahead, so sailing
  on from the arrival menu, a failed attack or a sack does not run it
  aground at once. The town just visited counts again once the fleet
  stands on none of the arrival cells, at sea as on foot. Without
  arrival cells (a town pack lacking them), a town is reached on its
  own cell and the eight around it. These are engine choices.

### Leaving a town (VA `0x19b9e`)

The fleet is put on the water outside the town, at town fields 26 and
27, with its bows away from the town: heading `field 28 x 32`. The same
holds at the start of a career; the scenario's start cell itself is
inside the town, on land.

### Clouds (VA `0x24523`, `0x231b8`, `0x297c4`)

- **How many:** `(|month - 5| + 1) / 2` (VA `0x28789`): none in June,
  three in December and January.
- **Look:** a random `cloud.cur` frame each, from (0, 1, 2, 0), chosen
  at the start of a voyage.
- **Motion:** every sailing pass, cloud i moves with the wind by
  `sine x (4i + 6) x 2`, then an eighth: about 1, 2 and 3 pixels.
- **Respawn:** a cloud more than 100 pixels left of the view, past its
  right edge, 40 above it or below it comes back at the right edge
  (view x + 640) at a random height 0..439. Clouds start off the view,
  so they enter from the right.
- **Shown:** only a cloud its move found in that range (VA `0x4c792`);
  one put back at the edge waits for the next pass. Without this, a
  cloud drifting a pixel east past the edge flickers there at a new
  height every pass.
- **Drawing:** first the shadow, 30 pixels left and 30 down, which
  replaces each covered pixel through the 256-byte table at VA
  `0x3400c`; then the cloud.
- **Storms:** each pass after the move, the distance from the fleet to
  the nearest of these clouds, `max(|dx|, |dy|) + min / 2` in pixels
  (9999 with none, 999 when sailing starts), drives the speed (see "Each
  tick"). The clouds are simulation state: their random heights and
  looks use the game's random numbers, and the view they come back
  around is the sailing view (VA `0x231b8`), kept by the simulation.

### Engine choices (not traced)

Where the original's behaviour is not traced yet, the engine does this:
- **Land:** land always stops the ship, but on a town's arrival cell
  (see "Arriving").
- **Time:** days pass steadily, one every 64 ticks, rather than as
  `ticks / 64 + 1` at the end of a voyage, so stepping a voyage in small
  pieces gives the same calendar.

### Speed table

- **Location:** per era, a 0x200-byte block at VA `0x34d68` is copied
  to VA `0x46f86` (VA `0x1361c`). Its first 128 bytes are the speed
  table: 8 ship classes, each with 16 headings relative to the wind.
- **Rows by class:** row 0 reads 4, 5, 6, 7, 7, 6, 4, 3, 3, 3, 4, 6, 7,
  7, 6, 5. Running before the wind gives 4, a beam reach 7, and into
  the wind 3. Rows 3-7 have -1 into the wind.
- **Scale:** with the x3 multiplier and the 4..12 clamp, most entries
  give full speed. The table matters mostly near the wind.
- **The whole block (copied to VA `0x46f86`), by what reads it:**

  | Offset | Content |
  |---|---|
  | 0x00-0x7F | the speed table (above; read through VA `0x4d35c` while sailing, VA `0x44a78` in sea battles) |
  | 0x80-0x9A | the enemy's decision per comparison total (see "Enemy decisions", VA `0x44a78` + 0x80) |
  | 0xF8, 0xF9 | the diplomacy range and the Spanish shipwright's tolerance |
  | 0x100-0x18B | the ships met: 4 nations x 5 records of 7 bytes (type, nation, crew, guns, role, the loot's gold and goods factors; VA `0x47086`) |
  | 0x18C-0x192 | the treasure galleon's record (VA `0x47112`; its byte 5 also sizes a convoy's sack) |
  | 0x193-0x199 | room for the fort's record, built when a fort battle starts (VA `0x18b3f`) |
  | 0x9B-0xF7, 0xFA-0xFF, 0x19A-0x1FF | never read |

## Encounters at sea

### Sighting (sailing loop, VA `0x231b8`)

1. **When:** every 32 ticks (when `(ticks + 4) & 31 == 0`), the game
   finds the nearest town and its distance `d` in map cells (`|dx| +
   |dy|`, at least 1, VA `0x4c0a0`). The town is the one of the nearest
   arrival cell (VA `0x24629` asks VA `0x1adfc`; see "Arriving"); `d`
   is measured to that town's own cell (fields 1, 2).
2. **Lingering counter:** a counter `h` resets to 0 when the nearest
   town changes (VA `0x23df7`). In the sailing loop it is the word at VA
   `0x4d288`; VA `0x4c09e` is its saved copy (loaded at VA `0x28890`,
   stored at VA `0x28958`).
3. **Only at sea:** after that reset, a sail is looked for only while
   the fleet is at sea (word `0x4d28a` is 0, VA `0x23e00`).
4. **Chance of a sail:** with `r` random:

| `d` | A sail is sighted when |
|---|---|
| below 8 | `(r & 7) + (h >> 2) < 2`, after `h += 8 - d` |
| 8 to 15 | `r & 15 < 2` (1 in 8) |
| 16 to 31 | `r & 31 < 4` (1 in 8) |
| 32 or more (capped at 63) | `r & 63 < 4` (1 in 16) |

5. **Lingering raises the counter:** the game reaches it through
   `h += 8 - d`, so near towns sightings thin out the longer the fleet
   stays.
6. **Ignoring raises it too:** an encounter that ends without a fight
   (the encounter function at VA `0xd1fc` returns 0: sailing on at any
   question, or a hail answered with news) adds 16 to `h` (VA `0x284ef`,
   in the dispatcher at VA `0x2849c`).

### Encounter (VA `0xd1fc`)

1. **"Sail ho!"**
   - The lookout reports a sail (CSTR 118). Below difficulty 2, it adds
     whose waters these are (CSTR 119): the nation word (CSTN 71) of
     the nearest town, or entry 5, "unknown", when that town is more
     than 12 cells away (VA `0xd25f`-`0xd279`).
   - Choices: investigate or continue the voyage (the menu at VA
     `0xd2e6`). Continuing ends the encounter (VA `0xd309`) before any
     ship is picked.
   - Layout: no box; the text and choices are written at (100, 300) up
     to x 540. The later questions are at (200, 256), below the ship's
     portrait or flag, on a screen cleared to colour 0 (black in those
     pictures, VA `0x247b`).
2. **Which ship (VA `0xd310`-`0xd47f`, once investigated):**
   - The ship comes from the per-era block (offset `0x100`, VA
     `0x47086`): 4 nations x 5 records of 7 bytes.
   - Normally it is record `k` of the nearest town's nation, with
     `k = min(rand(crew / 50 + h / 6 - town.f9) / 3, 4)`, or `k = 0`
     when that range is below 1.
   - With chance 4/15, or when the nearest town is more than 12 cells
     away, `r = rand(20)` gives record `r / 4` of nation `r % 4`
     instead.
   - Off the treasure fleet's town, when `rand(24)` is above the heat
     (VA `0x4c09e`), the ship is the era's treasure galleon instead: the
     record after the 20 (era block offset `0x100 + 140`, VA `0x47112`),
     a Spanish galleon (type 7) with a rich cargo byte (240 in 1560 down
     to 100 in 1680).
   - Before that: off the destination of a pirate hunt, when `rand(5)`
     is not 0, the first pirate record (byte 1 = 4) of the mission
     nation's five, if any. These are the only special cases.
3. **Ship record:**

| Byte | Content |
|---|---|
| 0 | ship type (`& 7`): 0 pinnace … 7 galleon; type 5 shows as 8 (fast galleon) while the era code (VA `0x45e8c`) is below 4: 1560 to 1620 |
| 1 | nation, 4 = pirate |
| 2 | crew (judging by the values) |
| 3 | guns (judging by the values) |
| 4 | role: 3 to 9; pirates are 7, the big warships 8 or 9 |
| 5, 6 | not decoded |

4. **"She looks like %s, captain"** (CSTR 120, with the article and
   type name from CSTN group 11). Choices: investigate or sail away. A
   chasing ship (below) or the hunted pirate forces investigate (VA
   `0xd560`).
5. **Her colours, the third question:**
   - **The battle's wind strength** is drawn first, for every ship that
     gets this far: `w = rand(3)` (VA `0xd581`, the word at VA
     `0x44ad2`); the same value sets the flag's frame delay, the word at
     VA `0x32e02`, to `2w + 1` (VA `0xd58f`-`0xd593`).
   - **Chasing ships:** a ship whose role is above 5, of any nation with
     which the player's standing is negative, chases the player. Pirates
     (nation 4) start at standing -1, so a pirate of role above 5 always
     does.
   - **Named:** a chasing ship or the hunted pirate is named only now
     (VA `0xd60d`-`0xd71d`; see "Named captains") and hails with CSTR
     121, "Arrgh! It's the %s %s, %s!", with three parameters: his
     repute, CSTN 34 entry `(id & 7) / 3` (1 for the hunted pirate),
     CSTN 105 entry 0 ("pirate") for a pirate or 1 ("pirate-hunter")
     otherwise, and his name.
   - **Any other ship:** "She's flying %s colors" (CSTR 122) with her
     nation's word from CSTN 106.
   - Choices: close for battle, hail for news, or sail away.
6. **Outcomes:**
   - **Overtaken:** when the ship has a captain (the word at VA
     `0x449d2` is not 0, VA `0xd98b`), any answer but closing for
     battle, hailing included, is first met by `rand(3) <` difficulty:
     she overtakes the player (CSTR 126) and there is a battle anyway.
     That branch (VA `0xd999`-`0xda3f`) leaves the town's field 9 alone.
   - **Closing for battle** (VA `0xdb56`) starts a sea battle (phase 6).
   - **Hailing:** the game stores today, `year x 360 + day`, at VA
     `0x45f0e` (VA `0xda71`). A nation with standing below -3 does not
     answer (CSTR 315) and attacks (VA `0xdb31`); otherwise the player
     gets the ship's news.
   - **Near a town:** closing for battle and the unanswered hail, and
     only they, lower the nearest town's field 9 (`[town + 0x12]`) by 1
     when it is less than 7 cells away (VA `0x4c0a0`).
7. **The ship's news (VA `0x1e78d` mode 2):** `pic/shipnews`, the title
   CSTN 18.2, then the 6 latest items others tell (the filter at VA
   `0x1e040`: code 22, a town the player took, reads as 19, a capture,
   and 3, a town he plundered, as 15; only codes 10-19 are kept, dated
   up to the day stored at the hail) at (20, 100) up to x 300, newest
   first. When the filter leaves none, "(no entries)" (CSTN 18.3).
8. **Pictures:**
   - Ship portraits (`S_*.PCX`) hold 5 views of 200x200 at (1, 1), (202,
     1), (403, 1), (1, 202), (202, 202) (the view tables at VA `0x32da8`
     and `0x32db4`), shown in turn while the question waits (VA
     `0xfb24`) in the window whose rectangle is at VA `0x32d90`, at
     (220, 10); each view lasts 11 ticks of the 60.9 Hz clock (the delay
     word at VA `0x32e00` is 10, and the view changes once the clock is
     past the due tick); the ship rocks.
   - The flag uses the same place (its window rectangle at VA
     `0x32dc0`), 8 frames (tables at VA `0x32dd8` and `0x32de8`). Each
     frame (VA `0xfc0a`) is due `2w + 1` ticks on (VA `0x32e02`), one
     less when `rand(4)` is 0, one more when it is 3 (VA
     `0xfc49`-`0xfc7d`), and changes once the clock is past that, so a
     frame lasts `2w + 2` ticks, give or take 1. The first change comes
     at once (flag VA `0x32df8`).
   - **Which flag (VA `0xd1fc`):** a named captain flies a Jolly Roger,
     `FJOLLYnn.PCX` with nn the word at VA `0x32f90` + `(id >> 6 & 3) x
     32 + (id >> 3 & 7) x 2` (pictures 0x5d-0x70 from the name table at
     VA `0x31dd0`); anyone else the nation's flag (pictures 0x43-0x46,
     `F-SPAN`, `F-ENGL`, `F-FRENCH`, `F-DUTCH`). The surname field of an
     id reaches 5 (see "Named captains"), so some captains share a flag.
     An unnamed pirate would get picture 0x47, `null.pcx`, which the
     game does not have.
   - **Over the sky (VA `0x108cf`):** flags hold 8 frames of 200x200 in a
     4 x 2 sheet, drawn in colours 8-15 with 0 see-through (green, red
     in `F-FRENCH`). Each frame is laid over `BASEFLAG.PCX` (picture 3,
     the spyglass's sky and mast, colours 0 and 64 up): a see-through
     pixel takes the base's, the others move up 24; the base's palette
     takes the flag's colours 8-15 at 32-39 (the palette at VA
     `0x45ac8`). In 16 colours the flag keeps its colours.
   - **The first named pirate's quiz (VA `0x10747`, flag `0x449be`):** the
     first time a named captain is met, the original shows a random flag
     and asks which pirate it belongs to; a wrong answer sets the
     difficulty to 4 (VA `0x45e6f`), the guns to 0 (player block +0x0a),
     the gold to 8 (+0x28) and the injuries to 8 (VA `0x45e88`). That is
     the game's copy protection. "He's chasing us" (CSTR 311) follows
     only that quiz (VA `0xd8a7`). **Project decision: it stays
     disabled**; the engine never asks and never applies the penalties,
     so every named captain hails with CSTR 121.

### Engine choices for encounters

- **The flag's jitter** comes from the front end's own random numbers,
  not the rules', so that a cosmetic draw never changes a game.
- **An unnamed pirate's flag:** the engine shows `FJOLLY01` where the
  original would load the missing `null.pcx`.
- **Ships set by hand:** an encounter with no nearest town (a test, a
  state written by hand) keeps the ship it holds when investigated.

## Sea battles (VA `0xe245`; loop at VA `0x7db8`)

- **Sprite banks (names at VA `0x32d64`):** the player's ship (two
  banks, 3 and 4), the enemy's (5, 6), `sbtlmisc.cur` (7: sails' gauges,
  shots, smoke, the cross), the damage pictures. A sprite id is bank and
  frame (VA `0x29a63`): below 0x100 bank 3, 0x100 bank 4, 0x200 3 then 4
  past frame 7, 0x300 5 then 6, 0x400 bank 7, 0x500 the damage bank,
  0x600 bank 5.
- **The cross off the bow (VA `0xf903`):** every pass object 0, the
  player's ship, is type 0x30 and draws sprite 0x414 (`sbtlmisc.cur`
  frame 20, 15 x 15, colours 225-238: gold to orange in the battle
  palettes) at the ship plus the offset for `heading / 16` (table at VA
  `0x40fbc`: a circle of about 50 pixels around (32, 23)) plus (3, 2).
  It marks the player's ship and where her bow points; the guns fire
  abeam, not at it. The engine centres the circle on the ship.
- **Objects drawn (VA `0xf6eb`-`0xf888`):** eight slots each pass, in
  order: 0 the player's ship, 1 the enemy's, 2-3 the player's shot
  (shadow, then balls), 4-5 the enemy's, 6 the burst of a hit on the
  player, 7 on the enemy. Their sprite ids are written each pass (VA
  `0x7e50`); positions are the sprites' corners on the field.
- **`sbtlmisc.cur` frames:** 0-3 the balls' shadows and 4-7 the balls,
  42 x 8, three balls each, the spread widening from the first to the
  last, never rotated; 8-12 a splash, 24-28 a burst (42 x 12); 20 the
  cross; 32-47 the wind compass (45 x 24), frame 32 pointing up and the
  next ones a sixteenth clockwise each.
- **A shot drawn (VA `0x83ef`-`0x8545`, the enemy's `0x9080`-`0x91d0`):**
  - At the shot, the flight `T` ticks (VA `0x9f9e`) sets a period `P =
    T >> 4` and a step 0 (VA `0x8395`). Each pass of the flight, the
    step picks a height `h` (16 signed words at VA `0x32544`, 0 or
    below, rising to the top mid-flight and back) and a spread frame `f`
    (16 words at VA `0x32564`, 0-3, widening), both indexed by the step
    kept within 15. Then the step moves on every `P` passes; with `P`
    0 it stays 0 (the countdown wraps first).
  - In flight: the balls (frame `4 + f`) with their corner at (shot x -
    21, shot y - 6), their shadow (frame `f`) `|h|` below that (VA
    `0x844c`-`0x8479`, `0x90d7`-`0x9104`).
  - When the flight ends the hit test runs, and a landing count
    (word at VA `0x44a02`, the enemy's `0x44a40`; negative during the
    flight) is set to 1. Each pass after, it goes up by 1 and shows
    frame `count >> 3`, then clears on reaching 39 (VA `0x84dc`-`0x8545`,
    `0x9167`-`0x91d0`): frame 0 for 6 passes, then 8 each for frames 1
    to 4, 38 passes in all.
  - A miss: the balls go; the shadow's slot shows splash frame `8 + k`
    at (shot x - 21, shot y - 6 - h). A hit (flag at VA `0x44a2c`, the
    enemy's `0x44a2e`): both shot slots go, and the burst frame `24 +
    k` has its corner 21, 6 before the centre of the ship hit (VA
    `0x8494`-`0x84d6`, `0x911f`-`0x9161`).
  - A side fires only once its count is 0 (VA `0x8284`, `cmp [0x44a02],
    0`; the enemy's likewise): neither in flight nor for the 38 passes
    of the splash or burst, whatever the reload.
  - The fire pass itself neither moves nor draws the shot (it jumps
    past the flight code to VA `0x8729`), and each later pass draws it
    where it was before moving it. The engine moves it on the fire tick
    already and draws it where it is after the tick: its shots land a
    tick early and are drawn two moves ahead (engine choice, kept from
    before; about 4 pixels).
- **The wind indicator (VA `0xee2b`, at setup `0x978a`, `0x9c36`, and
  for panel event 4 when the wind shifts):** the strength word, CSTN
  group 12 entry `strength & 3` ("light", "medium", "strong"), left
  aligned at (524, 234), and the compass, frame `32 + wind / 16` with
  its corner at (584, 229), pointing where the wind blows.
- **The speed (VA `0xf602`, the enemy's `0xf65c`):** each tick, after
  a ship's speed is computed (see "Each tick"), `speed >> 2` is written
  left aligned at (584, 398) for the player and (584, 54) for the enemy,
  beside the arrow and ship that `sbat.pcx` draws on those rows; a fort
  shows none (the call is skipped while the fort flag, VA `0x44a1c`,
  is set).
- **Palette (VA `0x939b`-`0x93ba`):** every 8 passes the battle turns
  the sailing sea's palette ranges once each (the rotations of VA
  `0x2ae26`, `0x2b009`, `0x2aed6`); the battle tiles use 208-215 and
  64-79.

Helper names below are those `tools/ghidra/RegParams.java` gives:
`div16(a, b)` is `a / b`, or 0 when `a < b` or `b == 0`; `sine_mul`
and `cosine_mul` are the sailing sine products.

**Level.** Where this section says "difficulty", the game uses a level
(VA `0x1b087`): `clamp(difficulty + extra - gunner, 0, 4)`, where
`gunner` is 1 with the gunnery ability. `extra` is 0 for ordinary ships;
named captains set it to 1 or `(id & 7) / 3` (VA `0xd1fc`). The sword
fight computes its level the same way with fencing.

### Setup (VA `0x9832`)

- **Flagship:** the ship that fights is the first of the fleet. With
  several ships, the player first picks which one leads (VA `0xf246`).
  Its type byte holds the class (`& 7`) and the hull damage it already
  has (`& 0xF8`).
- **Player crew aboard:** `min(crew + 8 - 8 x ships, (class + 2) x 32)`.
  The rest stays with the fleet. There is no floor: with no men, none
  are aboard and none ashore.
- **Player guns:** `min(guns, crew aboard / 4, (class + 2) x 4)`.
- **Gunnery `g`:** `clamp(mood / 2 + 4, 0, 16)`, from the crew's mood
  (player block +0x18, VA `0x45faa`, see "The crew's mood"). Reloading
  takes `|144 - 8 x g|` ticks.
- **Enemy:** from the encounter record: guns = byte 3, crew = byte 2 x
  4, class = byte 0, role = byte 4.
- **Turn rate, both ships:** `t = 3 - (class + 2) / 4`. Small ships turn
  and gain speed in wind differently from big ones.
- **Positions:** the ships' corners start mirrored about (240, 240) (their
  centres 40 right and 30 down): each axis
  offset by `rand(16) + 32` pixels, sign random.
- **Headings:** the player faces the enemy's side (east or west, 45°
  toward it when not level); the enemy heads `0x60` to the left of the
  player's heading, give or take 16: `heading - 0x60 + (rand & 31) -
  16` (VA `0x9bbd`).
- **Battle state:** the wind keeps its direction from sailing, with
  the strength `w = rand(3)` drawn at the encounter's third question
  (VA `0xd581`; the fort battle draws its own, VA `0x18e21`).
  Both ships start with battle sails.
- **Enemy reload:** `|144 - 8 x role|` ticks (VA `0x9046`).
- **Class 5 shown as 8:** when the byte at VA `0x45e8c` is below 4, a
  class 5 enemy takes the class 8 pictures, box and panel type (VA
  `0xd1db`, `0xe43f`, `0x1058a`); that byte is not identified.
- **Which ship (VA `0xf246`):** with more than one ship, the player
  picks the ship to command on the fleet panel, under "Which ship will
  you command?" (CSTR 286). The code fills CSTR 123 ("We have %s men
  and %s cannon ready for battle. Winds are %s…") first, but copies
  CSTR 286 over it. The chosen ship swaps places with the flagship,
  then fights with `crew - 8 x (ships - 1)` men.

### The fleet panel (VA `0xffae`)

- `shipsel.pcx`, the ships in two rows of four (layout table at VA
  `0x32e68`: picture corners, rectangles, label centres). Each ship is
  its battle picture (`*btl.cur`) at its damage stage `min(damage / 16,
  3)`, with its type (CSTN 8) and damage (CSTN 14) centred below.
- Clicking a ship marks it, its labels in ink entry 3; again unmarks
  it; no more than the allowed number can be marked. The button at
  (437, 429)-(639, 479), labelled from CSTN 87 (SELECT, REPAIR, SELL,
  BUY) centred at (510, 436), ends the choice once enough are marked.
  Two keys step through the ships and the button; Enter acts.
- The prompt is centred across x 5..420 at y 400; with prices, across
  x 5..320 at y 420 (VA `0x103f0`). The shipwright's modes also show
  each ship's price and the total (not used by the engine's shipwright
  yet).

### Each tick

- **Steering:** the player's heading changes by 16 x turn input on
  ticks that are multiples of 8 (VA `0x7e97`). The keys are read every
  tick and the turn request cleared each time (VA `0x1b58a`; Left and
  Right set it to -16 or +16 for that tick only, VA `0x1b6b3`,
  `0x1b6ca`), so a single press turns only when it falls on such a
  tick; holding the key (the keyboard's repeat) turns 16 every 8 ticks.
  - **In the engine:** a press turns at once, then every 8 ticks while
    held; a press during those 8 ticks is kept and turns when they end.
    The ship turns no faster than in the original, and no press is
    lost (an engine choice).
- **Sails:** either sail key, Up or Down, starts a change that flips
  them (VA `0x7f1d`, `0x7eff`).
  - **In the engine:** Up raises them and Down lowers them; the other
    key does nothing (an engine choice).
  - Changing sails takes 160 ticks, while the panel says "Raising sail"
    or "Lowering sail" (CSTN 30.4, 30.5, VA `0xef06` event 10) and
    reloading waits. Then it shows "Full sails" or "Battle sails" (30.3,
    30.2), and full sails multiply the speed by 1.5.
- **Damage word (VA `0xebd7`, event 7):** when a side's damage stage
  (`damage / 16`, at most 4) changes, its status line shows the stage's
  word (CSTN 14). The stages at the battle's start show none (VA
  `0x2a603`). The word stays until the next status event writes over
  it: for the player firing, reloaded, or a sail change starting or
  ending (events 1, 2, 10, 11); for the enemy its sails changing
  (event 12).
- **Speed:**
  - Start from `(speed_table[class][sector] x 4 - damage / 4)`, with
    `sector = ((heading + 8 - wind) >> 4) & 15`.
  - Full sails multiply it by 1.5.
  - The speed is then `div16(that x 8, 8 - (3 - t) x w)`, clamped to
    40. In stronger wind, big ships go faster.
  - For the enemy, a table value equal to the difficulty counts one
    less (as traced; purpose unknown).
- **Movement:** a position in 1/256 pixels gains
  `sine_mul(heading, speed) x 4` on x and loses
  `cosine_mul(heading, speed) x 4` on y.
- **Firing:**
  - The player fires on demand when reloaded and with guns.
  - The enemy fires when the player is within 6 of abeam (90° off its
    bow) and it is reloaded.
  - Neither fires while its last shot flies or splashes (see "A shot
    drawn").
  - A shot flies abeam, toward the target's side, at
    `sine_mul(h, 0x80) x 4` per tick, for `d / 2` ticks with
    `d = max(|dx|, |dy|) + min(|dx|, |dy|) / 4`, at least 16 (VA
    `0x9f9e`).
  - The bearing (VA `0x87bf`, toward where the player will be next
    tick) and `d` run between the two sprites' corners, but a shot
    starts from the firer's centre (VA `0x835a`, `0x8fee`): 40, 30 from
    a ship's corner, 16, 16 from a fort's. Between ships it lands on
    target; a fort's shot lands 24, 14 short of the player's centre,
    so small ships (half box 20 x 10) are hit only when the spread
    brings it back.
- **Hits:** when the shot lands, it hits if it is inside the target's
  box.
  - Half sizes by type: 20 x 10 (0-2), 30 x 20 (3-5), 40 x 30 (6-8).
  - The player's shots see the enemy's box widened by 3, 2, 1, 1 or 1
    quarter sizes for difficulty 0 to 4.
- **Damage of a hit by guns `G` on class `c`:**
  - **Hull:** `div16(G x 4, c + 2)`, halved when the target has
    battle sails, then at least 1 (VA `0x85d9`-`0x85f8`). Shots from the
    player gain half again at difficulty 0.
  - **Any hit** starts the sunset count again (VA `0x8641`, `0x9294`);
    a hit on the enemy sets its target heading to the wind until its
    next decision (VA `0x864a`).
  - **Crew lost:** `((rand & 63) + 4 x div16(crew x G, c + 3)) >> 6`. The
    player keeps at least 1 and at most 999.
  - **Guns lost:** `((rand & 63) + div16(guns x 4 x G, c + 3)) >> 6`.
- **Damage levels:** `damage / 16` (0-4), shown with CSTN group 14.
  Level 4 sinks the ship: the player's (CSTR 167) or the enemy's
  (CSTR 168, she blows up).
- **Wind (VA `0x8806`):** every 512 ticks it moves by -1, 0, +1 or +2
  sixteenths, one chance in four each; only when it moved does the
  strength change, by -1, 0, 0 or +1, kept within 0..2 (event 4).

### Enemy decisions (every 64 ticks)

1. Compare the two ships, with `eff = crew x role >> 3`:

| Test | Adds |
|---|---|
| enemy damage < player damage / 2 + 1 | 9 |
| enemy damage < player damage x 2 | 9 |
| enemy guns > player guns / 2 + 1 | 3 |
| enemy guns > player guns x 2 | 3 |
| enemy eff > player crew / 2 + 1 | 1 |
| enemy eff > player crew x 2 | 1 |

2. Look the total (0-26) up in the era block at offset `0x80`:
   - 0: close in, full sails;
   - 1: take a broadside position, battle sails;
   - 2: flee, full sails.

   A total of 0 (hopeless) with the ships near, their boxes at twice
   their half sizes touching (VA `0x9d53`, not the boarding test): the
   enemy strikes its colours (outcome 9, CSTR 170).
3. Target heading:
   - for 0 and 2, the bearing to the player plus `decision x 0x40`,
     clamped to within 0x50 of downwind;
   - for 1, the bearing plus 0x40, turned half round when that is more
     than 0x40 from the current heading or 0x5F from the wind.

   The enemy then turns toward the target by `t` every 8 ticks.

### The battle field and the coast

- **Screens:** the field is one of 108 prebuilt screens of 15 x 15
  battle tiles (`SBTL.BMP`, tiles from `battle.imb`), 480 x 480 pixels
  (VA `0x2a9e8`).
- **Which screen (VA `0x2a658`, at the encounter):** the set comes from
  where the fleet is. On the shallows it is set 4 (open sea).
  Otherwise the first land found 1 to 19 pixels away, looking north,
  east, south and west in turn at each distance (table at VA
  `0x413f8`), gives set 0, 1, 2 or 3; none gives set 4. A piece is
  picked, `rand(4)`.
- **Sets (table at VA `0x33fd0`, 5 screens each):** 0 coast to the
  north (64-67), 1 east (88-91), 2 south (72-75), 3 west (80-83), 4
  open sea (104-107); the fifth screen of each is open sea (104). Set
  5 is the town's own screen for a fort battle.
- **Positions:** a ship's position is the corner of its sprite; its
  centre is 40 pixels right and 30 down. Ship battles start about
  (240, 240).
- **Scrolling (VA `0x95ac`):** when the player's corner (or else the
  enemy's) leaves 40..400 on an axis, everything shifts 120 pixels
  back. Along a coast (sets 0 and 2 along x, 1 and 3 along y) the
  screen becomes piece `(scrolled + first piece) & 3`; scrolled off the
  coast's axis, open sea. Set 4 stays open sea.
- **Terrain:** the class of the battle tile's colour under a ship's
  centre (table at VA `0x4113c`, the one sailing uses).
- **The player's ship (VA `0x8185`; the shallows roll at `0x81ef`),
  after it moves:**
  - Shallows: once a cooldown has run out, a roll `rand(16) >= class`
    (table at VA `0x327ce`, 0..7) costs 16 damage and starts a 20-tick
    cooldown. The cooldown only counts down on the shallows. A pinnace
    always takes the damage; a galleon on 9 rolls in 16.
  - Land: the ship is beached (outcome 13). If the enemy is alongside
    (the boarding test, VA `0x9c84`), it is a boarding instead (outcome
    5).
- **The enemy (VA `0x8cd6`):** the same shallows rule, with its own
  cooldown. Land does not stop it.
- **Beached:** the enemy's portrait (VA `0xe9e6`) and CSTR 173 at (100,
  300) up to x 560. Nothing is lost.

### The fort battle (VA `0x18b3f`, `FUN_e245(1, …)`)

Attacking a town with forts from the sea starts with a battle against
the fort.
- **The fort as an encounter (VA `0x18d67`):** type 8, the town's
  nation, crew byte `(soldiers + 1) / 2` (so its crew is 4 times that,
  VA `0x999b`), guns `(forts & 7) x 12`, role 0 (reload 144 ticks).
  When any attack on a town begins (VA `0x18b3f`, `0x18b9c`, by sea or
  on foot) the treasure fleet and the silver train each add
  10 soldiers to its garrison, at most 255, for good.
- **The field:** the town's own screen (town field 14). The fort's
  sprite corner is at `field 17, 18 x 32` (centre +16; its box reaches
  32 either way, not quartered as a ship's is);
  the player's at `field 19, 20 x 32 + 16`, heading field 21 (VA
  `0xd16c`). With several ships, the player picks one first.
- **Differences from a ship battle:**
  - The player's ship moves at half speed (VA `0x808f`, `0x80cb`).
  - The field scrolls only beyond 5..470 (VA `0x9a14`), outside the
    10..430 that ends the battle: it never scrolls.
  - The fort neither decides nor moves (VA `0x8914`). Before each
    shot it turns to `bearing + 0x3C + rand(8)`, so it fires whenever
    reloaded, its shot within 4/256 of the bearing. It aims from its
    corner (see "Firing"), so it misses small ships more often.
  - The player's hits count in a fort damage counter, with class 8
    in the damage formula and no bonus. The panel shows `fortbtl.cur`
    stage `damage / 20` (at most 3). The fort is never sunk.
  - The fort is drawn in its screen, not as a sprite. The panel calls
    it "Fort" (CSTN 8.9), with no speed and no status line (VA
    `0xea3e`, `0xebd7`).
  - Leaving 10..430 (the player's corner) ends the battle: "An
    accurate salvo… drives you back" (CSTR 196). Sunset says CSTR 317.
  - Running onto land with the boxes touching (the fort's box not
    quartered) is the landing: the assault follows (outcome 5).
- **Afterwards (VA `0x18e3d`, `0x18e46`):** only the landing goes on. The
  garrison becomes `(soldiers + 1) / 2 + fort crew / 10`, and the men
  who stayed with the fleet do not fight.

### The assault (VA `0x18b3f`)

After a landing by the fort, or straight away at a town without forts:
- **Introduction:** on `landpir.pcx`, "Your men scale the walls of the
  fort…" (CSTR 192), or "As you and your men enter the town…" (CSTR
  211), at (20, 150) up to x 250.
- **The fight:** a crew sword fight, the weapon chosen on the scene's
  picture: the fort (scenes 1, 2, `fort.pcx`, head `HeHead3`), or the
  alley without forts (scene 0).
  - The player: the crew but for the men left with the fleet in the
    fort battle, factor `rand(3) + 3 + mood / 2` (the crew's mood, see
    "The crew's mood"; the engine keeps it at least 1).
  - The garrison: `soldiers x 10 + 5` men, factor `forts + 5`, title 4,
    the town's nation.
- **Won:** `soldiers = (men left + 9) / 10`, the fallen leave the crew,
  and the sack follows.
- **Repelled:** the fallen leave the crew; reputation -1 and an attack
  of -1 on the town's nation.
- **Defeated:** the same, then prison (see "Prison").
- **The family members held there** are told by the villains' news
  (see "Villains").

### The sack (VA `0x22764`)

1. The town loses 20 wealth; its information is current again.
2. On its arrival picture, "The once-proud town of %s is at your
   mercy…" (CSTR 193, at (340, 150) up to x 620).
3. With heat 12 or more, "%s of their gold has been hidden away" (CSTR
   195 with CSTN 15: some, much, most for `min((heat - 12) / 15, 2)`).
   The formula below already accounts for it.
4. Loot: three goods (food, goods, sugar) of `rand(wealth)` each, a
   fourth roll unused, at price 0 on the trading board.
5. Gold: `town gold x 1500 / (heat + 15)`; the town loses a fiftieth of
   that, and another 20 wealth. The player receives ten times that
   figure: "%s gold pieces plundered!" (CSTN 10.6).
6. An attack of -1 on the town's nation; past 99 gold, another of -2
   and reputation +2. The town's standing falls by 3.
7. A crew below `citizens x 10 + rand(citizens x 10)` only plunders
   (log 3, "Plundered %s"). A bigger one may install a governor (CSTR
   194, on `governor.pcx`): the town's nation, standing 2, governor
   `rand(16)`; for a new nation, the player's standing with it +3 and
   an attack of -2 on the old one (log 22, "Captured %s for %s").
8. Cannon: `forts x 6`, offered on the board's fourth row.

Back to sea afterwards.

### End of battle

Each ending has its own picture (VA `0xe67b`-`0xe985`), with the text in
palette entry 2 and no box:
- **Either ship sunk:** `sinking.pcx`, text at (100, 300) up to x 560.
  An enemy sunk costs the current town (VA `0x46f82`; at sea the
  nearest) 10 wealth, to 0 at 10 or less, at any distance (VA `0xe706`).
- **Escape:** `outrun.pcx`, the same place; the slow ship's loss (CSTR
  172) is boxed, at (100, 300) up to x 620 (VA `0xe7e6`).
- **Sunset:** `night.pcx`, text at (40, 150) up to x 280 (CSTR 317
  against a fort).
- **Colours struck, beached (CSTR 173):** written over the battle at
  (100, 300).

| Outcome | When | What follows |
|---|---|---|
| boarding | quarter-size boxes touch | CSTR 171, then a sword fight |
| escape | the ships are more than 240 pixels apart on either axis | CSTR 198 when the enemy was fleeing (decision 2, VA `0x44a20`), otherwise CSTR 197; CSTR 196 against a fort (VA `0xe82a`). The encounter ends (see "The slow ship") |
| sunset | more than 4500 ticks without a hit (VA `0x7e38`) | CSTR 169 |
| surrender | as above | CSTR 170, then capture (CSTR 164: keep her or plunder and sink her) |
| player sunk | damage level 4 | CSTR 167; the fleet loses the ship and its cargo share (see losing a ship); with no ship left, marooned (CSTR 161) |
| enemy sunk | damage level 4 | CSTR 168 |

Afterwards the flagship's crew and guns, and its damage (in the type
byte), return to the fleet.

### Engine choices until more is traced

- **Boarding:** the sword fight decides it (see below). A win counts as
  a surrender: the same capture (VA `0xdb7b`) and log code 4 as colours
  struck (VA `0xe74d`, `0xe95a`).
- **Lost boarding:** the enemy takes the flagship, as if it sank; then
  prison (see "Prison"). The original probably differs: `0xe245` changes
  no ship when the fight is lost (VA `0xe775`: heat +16, reputation -2);
  whether prison (VA `0x1f803`) removes one is not traced.
- **The prize's picture (VA `0xdc12`, `0xdcc2`):** `plunder.pcx`, the
  question at (40, 50) up to x 620 in ink entry 0.
- **Near a town (VA `0x4c0a0`):** the distance `|dx| + |dy|` in cells
  to the nearest town, at least 1, measured when `(ticks + 4) & 31` is
  0 at sea (VA `0x23e0e`) and copied there at the voyage's end (VA
  `0x2896d`). Closing for battle or an unanswered hail within 7 cells
  lowers that town's field 9 by 1, being overtaken does not (see
  "Encounter"); after any sea battle within 12 cells the heat
  rises by `24 - 2 x distance` (VA `0xe245`); a capture within `class +
  5` cells takes 10 from its wealth (to 0 below 11) and its crew tells
  of the villains there (VA `0xdb7b`), after the prize's slot and
  before the captain held.
- **Kept (VA `0xdb7b`):** a new slot; with 8 ships already, "You lack
  enough officers to run your ships! Which will you abandon?" (CSTR
  130) on the fleet panel, and the prize takes the chosen ship's slot,
  her hold given up. The slot's ship is `class | damage`, the hold grows
  by the prize's room, and the crew rises to at least `slot x 8`.
- **Prize text (VA `0xdb7b`):** the prize is `class x 20 + 20` tons, its
  guns are rounded up to even, and the hold space is
  `sum(2 x class + 2) x 10` over the fleet, minus cargo.
- **Plunder (VA `0xdb7b`), kept or sunk, after the captain's part:**
  - gold `rand(((current town's wealth + 64) / 16) x record byte 5) x
    10`, the current town being the player's or at sea the nearest;
  - on the loot board (`plunder.pcx`, mode 2, with "%s gold pieces
    plundered!"): food = record byte 2 (her crew), goods and sugar each
    `(r x r >> 15) x byte 6 >> 16` with `r = rand(32767)`, cannon = her
    guns rounded up to even; cut a unit at a time, the good by the
    running total mod 3 (food, goods, sugar), while `food + goods +
    sugar + cannon / 2` exceeds her tonnage. A pass on an empty good
    still counts, so a little more may remain;
  - the gold is added; then `n = rand((reputation + 5) x byte 2 / 10)`:
    with `n > 1` and room (`crew + n <= sum((class + 2) x 32)`), on
    `crew.pcx` "Captain, %s sailors wish to join our crew. ~Take
    them~Leave them" (CSTR 166) at (100, 280) up to x 540.
  - A kept prize brings no guns by herself: they are on the board.
- **The board's rows (VA `0x20229`):** the town merchant shows food,
  goods and sugar (layout records 0-22); the loot board (VA `0x2264f`:
  mode 1 overboard, 2 capture, 3 sack, 4 dividing or joining the
  squadron) adds cannon (records 23-28), which weigh
  half a ton each.
- **Marooned:** with no ship left, the game shows CSTR 161 and 162
  (VA `0x1f2a1`). The player then continues with a pinnace and at least
  8 men. The original keeps the last ship's entry and runs routines not
  traced yet: VA `0x1be71` looks for land around the fleet, VA `0x1f803`
  resets the crew, and a value used as the difficulty drops by one.
- **The slow ship (VA `0xe78f`, outcome 6 in the table at VA
  `0x33030`):** at each escape the original reads the byte at VA
  `0x45f10`; unless it is -1 the ship in that slot is lost, "As you flee
  the battle, the undamaged enemy ship overtakes a slow sailing %s from
  your fleet and captures it!" (CSTR 172, CSTN 8 by her class), dropped
  by VA `0x1f2a1`. No sea battle code writes that byte: only the new
  game's start (the starting town's code, VA `0x1451e`) and the land
  battle's unit records (cleared at VA `0x1b0e5`, unit 0 unused) share
  it. The engine does not reproduce a loss driven by that leftover
  value.
- The engine keeps hull damage beside each ship rather than packed in
  the type byte.

## Sword fights (VA `0x16205`; frame loop at VA `0x2747c`)

One routine runs every sword fight. A caller fills a block of four
words and four bytes:

| Field | Meaning |
|---|---|
| word 0, 1 | player's men and strength factor |
| word 2, 3 | opponent's men and strength factor |
| byte 8 | opponent's title, CSTN group 74 (0 guard … 3 enemy captain, 4 captain, 5 first mate) |
| byte 9 | opponent's nation, CSTN group 71; -1 a named captain |
| byte 10 | level `extra` (see the sea battle level) |
| byte 11 | kind: 0 crew fight, 1 duel, -1 the career story's fight, -2 practice |

The opponent's label (VA `0x16375`-`0x164c8`): the title (CSTN 74),
led by the nation word (CSTN 71, then a space) when byte 9 is a nation
other than 5. In French a captain (title 4) with a nation reads title,
space, nation ("Capitaine espagnol", VA `0x16447`); in German the
nation's space is left out (VA `0x1639a`), which the engine does not
copy. A pack's text may give its own pattern (CSTN 74.6, see
asset-packs.md).

The routine returns 1 when the player wins, 0 when the player is driven
back, -1 when the player loses (VA `0x16989`: `2 - result`). Both men
counts come back reduced, for every kind (VA `0x16974`).

### Callers

- **Boarding (VA `0xf10a`, outcome code 5 of the sea battle):**
  - Before the fight, CSTR 171 is shown, boxed, at (274, 306) up to x
    560 (VA `0xf170`); its `%s` is a named captain's name, else CSTN
    30.7.
  - Player: the crew aboard, factor `7 - player damage / 16`.
  - Opponent: the enemy crew (record byte 2 x 4), factor
    `record byte 4 - enemy damage / 16`.
  - The opponent is title 4 ("Captain") of the record's nation, with
    `extra` 0, for an ordinary ship. A named captain (VA `0xf1df`,
    word `0x449d2`) fights as title = his id, nation -1 (labelled with
    his name), `extra` = 1 when his id has bit 0x80, else `(id & 7) /
    3`.
  - A win leads to the capture (VA `0xdb7b`, CSTR 164). Being driven
    back ends the battle with no capture: the reputation (VA `0x45e76`)
    drops by 2 and the heat (VA `0x4c09e`) rises by 16 (VA `0xe775`). A
    loss does the same once its prison is over (the fight's routine, VA
    `0x16205`, returns 0 after VA `0x16647`).
  - A loss (VA `0x16647`): CSTR 175, "Your men flee in panic…
    imprisoned in %s", then CSTR 190, an injury (VA `0x45e88`) and
    music 43. Not traced further.
  - After a win or a repulse, the fleet loses the men who fell (VA
    `0x1661d`, kind 0 only).
- **Duels (VA `0x161ba`, `FUN_161ba(scene, title, nation, extra)`):**
  one man each, factors 9 and 9.
  - Scenes (table at VA `0x3361c`): 0 alley, 1 and 2 fort, 3 and 4 the
    ship's deck. The opponent's head is `HeHead1` on the deck,
    `HeHead2` in the alley and `HeHead3` at the fort (VA `0x15f84`).
  - The weapon is chosen on the scene's picture.
  - The opponent is labelled with his title alone; crew fights add the
    nation (never nation 5). Nation -1 gives a named captain's name (VA
    `0x19fbb`), -2 a villain's (bytes VA `0x4722c`-`0x4722e`, caller VA
    `0x18a49`). The label is centred at (160, 336), the player's name
    at (480, 336), in font 1. In French the title comes first (title 4),
    in German nation and title are joined without a space (VA
    `0x16439`, the language at `0x33ef0`); the engine keeps the English
    order.
  - A loss or being driven back (VA `0x165b8`): the captain's portrait
    in its frame (VA `0x159df`), "You have disgraced yourself in
    personal combat!" (CSTR 174, at (100, 350) up to x 560), and the
    reputation becomes -1.
- **Practice (VA `0xbdae`):** 200 men each, factors 32, against a
  first mate. No consequences. Afterwards a fencing practice value
  (VA `0x45e90`, 0..30) rises by 5.
- **The assault (VA `0x18f4b`):** see "The assault".
- **The career story's fight (kind -1, block filled at VA `0x14630`,
  called from VA `0x13a2b`, `0x13ba4`):** title 3, nation 5, no weapon
  choice (the rapier, VA `0x16568`), the flags not moved (VA
  `0x16944`). Two variants: 10 men of factor 16 against 200 of factor
  32 (which also sets difficulty 4), or 200 of 32 against 20 of 16; the
  engine has only the second.

### Setup

- **Level:** `clamp(difficulty + extra - fencer, 0, 4)`, where `fencer`
  is 1 with the fencing ability.
- **Weapons (CSTN group 70):** 0 rapier, 1 longsword, 2 cutlass.
  - The player chooses first on `img/wpnsel` (CSTR 85), with the
    cutlass preselected (VA `0x16e5d`). Only the story's fight (kind
    -1) skips the choice.
  - The choice (VA `0x16e56`): the blades left to right are the rapier,
    the cutlass and the longsword (rectangles at VA `0x33628`).
    Pointing at a blade writes its name in PIRATE1, centred at x 426,
    464 or 506 and y 178, in ink entry 1; pointing elsewhere clears it.
    Clicking a blade takes it. Two keys step along the blades without
    wrapping; Enter or Escape take the one named last.
  - The opponent fights with the rapier in duels and practice. In crew
    fights it picks the longsword or cutlass, `rand(2) + 1`.
- **Strength bar:** `B = 31 x Pm x Pf / (Pm x Pf + Om x Of) + 8`, in
  integer arithmetic, for the player; `O = 47 - B` for the opponent.
  `B / 8` places the two flags on the screen (table at VA `0x338a0`).
- **Positions:** in crew fights (kind 0, VA `0x28018`) the player at x
  340, the opponent at x 220; in other fights 320 and 240. The distance
  is `player x - opponent x`.
- **Moves:** the opponent starts on guard with one of 8 scripts picked
  at random. The player starts with no move, so the first input counts
  at once.

### Moves

Each fighter plays one move at a time, one frame per tick. The tables
below are indexed by `move x 6 + frame`, from VA `0x338ac` (frames per
move) and `0x38a1a` to `0x38b99`.

| Move | Frames | What it does |
|---|---|---|
| 0, 1, 2 | 6 | guard: 0 middle and high, 1 low and middle, 2 high and middle |
| 3, 4, 5 | 6 | the same guards while stepping back 14 on frame 0 |
| 6, 7, 8 | 6 | slash: middle, low, high; strikes on frame 3 (high: 4), steps in 4 on frame 5 |
| 9, 10, 11 | 4 | lunge: middle, low, high; strikes on frame 2, steps in 4 on frame 3 |
| 12 | 6 | advance 14 on frame 5 |
| 13 | 6 | back off 14 on frame 0 |
| 14 | 10 | falls; the fight ends after frame 8 |

- **Lines** are bits: 1 low, 2 middle, 4 high. A guard frame holds the
  bits it covers, and a strike frame holds the bit it attacks.
- **Steps:** stepping back always applies; stepping in only while the
  distance is 10 or more (VA `0x27a2c`, `0x27cb6`).

### The player's input

- **Directions:** the input is a direction on a 3 x 3 pad, with the
  opponent to the left.
  - None, down and up pick guards 0, 1 and 2.
  - Away from the opponent (alone, with down, with up) picks 3, 4 and 5.
  - Toward the opponent picks an attack in the line given by up or
    down.
- **Lunge or slash:** with the keyboard, an attack is a lunge unless a
  shift key is held, which makes it a slash. Each key sets one pad
  position (tables VA `0x3879c`, `0x3889c`: the arrows, Home, PgUp,
  End, PgDn and the keypad's 5), so the last key pressed replaces the
  one before.
- **Mouse (VA `0x248d1`, `0x27638`):** the pointer anywhere on the
  screen, by its side of the pad's centre: x up to 308 toward the
  opponent, from 331 away, y up to 313 up, from 334 down. The left
  button lunges, the right button alone slashes; the input lasts while
  a button is held, and letting go gives guard 0.
- **New moves:** a new move starts only when the current one ends.
  Guards and attacks (moves 0-2 and 6-11) take the input once (table
  VA `0x38a0a`): a held mouse button does not repeat them, a held key
  does, through the keyboard's own repeat.
- **At a distance:** beyond 59, attacks become advances (12) and the
  steps back become move 13.
- **When the opponent falls,** the player stands on guard 1.

### Each tick

1. **The player's frame.**
   - On a strike frame, if the opponent's guard does not cover the
     line, the strike reaches when
     `distance <= reach_level[level] + reach_weapon[weapon]`. The reach
     tables (VA `0x389b8`, `0x389bd`) shrink with the level and from
     rapier to cutlass.
   - It hits when `threshold[level] <= (rand & 255) + practice` (VA
     `0x389e4`).
   - A hit:
     - leaves the player guarding every line (guard 7) for the rest of
       the tick, so the opponent's strike then is covered (VA
       `0x278dd`);
     - pushes the opponent back 4;
     - moves the strength bar toward the player `2 x weapon + 1`
       times, each time by 2 steps (1 for a lunge);
     - counts the hit by line.
   - If the opponent was already beaten (below), he falls (move 14).
   - A covered strike at a distance under 36 is a clash with no
     effect.
2. **The player steps.** Reaching x 442 drives the player back, which
   ends the fight with 0.
3. **The opponent's frame.** It works the same way, with these
   differences:
   - reach by weapon only (VA `0x389c1`);
   - it hits when `threshold[level] <= rand & 255` (VA `0x389e9`);
   - a hit pushes the player back 4 and leaves him guarding every
     line until his next frame, the player's next strike included (VA
     `0x27b7d`);
   - it moves the bar `weapon + 1` times, 2 steps each (1 for a lunge);
   - a covered strike under 35 clashes.
4. **The opponent steps.** At x 60 or less he is driven off, and the
   player wins.
5. **The crew melee (crew fights, every 16 ticks):**
   - Each side rolls `rand(|((men + 8) >> 2) x bar|)`. The product is
     taken as a signed 16-bit value, and `rand(0)` is 0.
   - If the opponent's roll is lower, he loses `((Om + Pm) >> 6) + 1`
     men, kept within 1..9999, and the bar moves one step toward the
     player.
   - Otherwise the player loses men the same way and the bar moves one
     step toward the opponent.
6. **End of the fight:** a fighter whose fall (move 14) passes frame 8
   loses.

- **Bar steps:** a step toward the player raises `B` and lowers `O`,
  only while `O > 0`. When `O` falls below 1, or the opponent is down to
  1 man, the opponent is beaten. A hit that leaves him beaten fells him
  at once (move 14); one beaten by the melee falls at the next hit. The
  same holds the other way for the player.
- **Speed:** a tick lasts 12, 11, 10 or 9 units of the game clock
  (60.9 Hz, see the game clock) by `level & 3` (VA `0x389c4`), so level
  4 plays at level 0's pace. Engine: level 4 takes level 3's (maintainer,
  2026-10-30), so the hardest level is not the slowest.
- **Debug mode:** the command-line argument `RDM` (compared exactly
  with the string at VA `0x31fe8`, VA `0x3bd`) sets the debug flag (VA
  `0x45e6c`). Then, while sailing (key table VA `0x33d90`, read by VA
  `0x1b58a`, handlers VA `0x1b700`-`0x1b74e`; on foot the table at VA
  `0x33c74`, read by VA `0x1b210`, handlers VA `0x1b3ed`-`0x1b43b`):
  N stops sightings and E brings them back (byte VA `0x33ee8`, read at
  VA `0x23ef3`); R makes the player near invulnerable and F ends it
  (byte VA `0x33ee9`: the enemy never fires, VA `0x8ecd`; a hit is not
  doubled, so the player's own hits move the bar `w + 1` times, the
  player's bar never drops (VA `0x27f9c`) and the crew loses no men in
  sword fights, VA `0x2747c`); a
  right click on the cabin's map moves the ship there. Both bytes are 0
  in the program, so normal play has them off.
  - **In the engine:** `--debug` turns the mode on, with the same keys
    (on foot too) and the map's right click; the ship moves when the map
    is left, to the cell `pixel / 0.75` of the last right click, with 10
    more food, as the original does (VA `0xb6d3`), but only at sea (not
    in town nor ashore, an engine choice).

### Engine choices for sword fights

- **Lunges:** whether a strike moves the bar by 1 or 2 steps follows the
  striking move. The original follows the latest input, which differs
  only when a new key arrives in the middle of a move.
- **Men:** the panels show the men only for kinds 0 and up (crew fights
  and duels; not the story's fight nor practice), as numbers. The original draws them
  as figures for 500, 50 and 1 men (VA `0x169b7`).
- **Keys combine:** the engine adds the held keys together (down then
  left lunges low), where the original keeps the last key's pad
  position; the keypad's 5 is not mapped.
- **Not reproduced:** the mouse input lasting only while a button is
  held (the engine takes a click); the flags redrawn when a bar reaches
  a multiple of 8 (result 4, VA `0x27f6e`, `0x16d94`); the other tick
  table at VA `0x389ce` when the word at `0x4c0e6` is 1; a scripted
  fight when `0x4c0e4` is set (VA `0x28274`, purpose not traced); an
  instant win when `0x4c0e2` is set from the command line (VA
  `0x274f5`).

### The opponent's choices (VA `0x280ef`, when a move ends)

- **Far (distance 20 or more):** a random pick from the 8-entry table
  at VA `0x38c5a`, mostly advances, some guards and backing off.
- **Close:** with `rand & 15` above the level's caution (VA `0x38c72`),
  the next step of the current 8-step script. Otherwise guard 1.
- **Scripts:** at VA `0x38c87`, 8 scripts of 8 steps per bank. Bank 0
  serves level 0, bank 1 level 1, bank 2 the higher levels.
  - A step below 0x80 is a move.
  - 0x90 parries the player's current line.
  - 0x91 parries while stepping back.
  - 0x92 slashes, and any other step from 0x80 lunges, in the line
    `line + 1 + rand(2)`, made 0 when it reaches 3 (VA `0x28457`): one
    of the two others against lines 0 and 1, always line 0 against
    line 2. The lines come from the table at VA `0x38d47`.
- **New script:** when a script wraps around, a new one is picked at
  random:
  - at level 1, after more than 2 player hits;
  - at level 2, after any player hit;
  - at level 3, after any hit.
  - Levels 0 and 4 keep their script.

## Towns

### Arrival (VA `0x18fa9`)

- **Screen:** `arr_sea.pcx`, or `arr_land.pcx` for an inland town or a
  land arrival. CSTR 114 at (330, 40) up to x 620: "You have arrived at
  the lovely <CSTN 29.7 seaside town / 29.6 inland village> of <name>.
  <forts> The <nation> flag flies over the town.<extra>".
  - Forts: none, "One fort guards the harbor entrance." (29.0), or
    "<n> forts guard…" (29.1 with CSTN 79); 29.8 and 29.9 inland.
  - Extra: "The treasure fleet is in the harbor!" (29.2) or "The silver
    train is in town!" (29.3, which wins when both are there) in the
    towns at VA `0x45e71` and `0x45e70`.
- **Choices (CSTR 115, at (330, 260)):** sail into the harbour (29.4;
  "March into town", 29.5, on land), attack the town (VA `0x18b3f`, not
  traced yet), sneak in, or leave.
- **When the menu appears:** the visit flags keep bits 1 and 2; in
  another town than the last, they are cleared with the shipwright's
  and merchant's gold. The tavern's people are all back.
- **Sailing in (VA `0x19415`):**
  `x = (standing[town nation] + town word 9 - crew / 100) x
  rand(32767) x 2 / 65536`, in double precision.
  - Above -0.5, or when the town has no forts: in.
  - Otherwise "the fort opens fire!" (CSTR 116) and the fleet stays
    out. At -1 or below, and arriving by sea (VA `0x47aca` = 2, set on
    arrival when the player has a ship), "One of your ships is sunk!"
    (CSTR 117): a random ship is lost with its share of the cargo (see
    losing a ship). With the last ship, the prison path: CSTR 175 and
    190.
- **Sneaking in (VA `0x195f8`):** visit flags 2 and 8 are set. The
  player is in when `town word 9 x reputation > -rand(50)`. Otherwise,
  on `landpir.pcx`, a guard recognizes the player (CSTR 163, at (40,
  150) up to x 240), then a duel in the alley (scene 0), with the
  guard's title (0), the town's nation and one level more (extra 1).
  Whatever the duel's result, the player stays out.
- **Leaving:** back to sea.

### Losing a ship (VA `0x1f2a1`)

`FUN_1f2a1(ship, keep, from_fort)`:
- With other ships, the ship is removed and the fleet's capacity
  shrinks by its room. Unless `keep`, with `r = room / capacity before`
  in single precision:
  - the gold loses `trunc(gold x r)`;
  - food (with its eaten fraction, `food x 256 + fraction`), goods,
    sugar and guns each lose `trunc(x x r + 0.5)`.
  - The crew is untouched.
- The last ship: marooned after a sea battle, or imprisoned in the town
  when `from_fort` (see "Prison"); the ship stays in the fleet's first
  slot, and the fresh start that follows keeps her type.
- Callers (seven sites):
  - VA `0xe6bc`, the flagship sunk in battle: `(0, 0, fort)`, the
    battle's fort flag (its first argument, set by the town's attack
    at VA `0x18e35`, 0 at sea at VA `0x28503`);
  - VA `0xe81d`, the flagship taken: `([0x45f10], 0, fort)`;
  - VA `0x195d1`, the fort's salvo on arrival (keep 0, from_fort 1);
  - VA `0x1f6fc`, manning the fleet with one ship;
  - VA `0x1f748`, manning abandons ship `[0x49b0c]` (keep 1);
  - VA `0x21d37`, a ship sold (keep 1);
  - VA `0x286d0`, the reefs.

Every way in marks the town's information as current: flag 0x80 set,
0x20 cleared. A random town event (see the world update) clears 0x80
and, when it was set, sets 0x20 ("information obsolete").

### Visit flags (VA `0x45f0a`)

| Bit | Meaning |
|---|---|
| 1 | the governor has been seen this stay, or the town has fewer than 6 citizens (set by the menu, VA `0x18739`) |
| 2 | set when sneaking in and by the traveller's draw, kept on a return; nothing reads it |
| 8 | sneaked in: the bank is closed |

On entering a different town the flags are cleared; on re-entering the
same town only bits 1 and 2 are kept. They are also cleared:
- at the start of every battle, at sea or against a fort (VA `0xe25a`,
  both through VA `0xe245`): after a battle the governor receives again
  in the same town;
- by a fresh start (VA `0x1f036`);
- by the calendar at each month crossed (VA `0x1df19`, in VA
  `0x1dd0e`) unless VA `0x47aca` has bit 2 (set on arriving with a
  ship). The engine does not model VA `0x47aca` and leaves this out.

Bit 8 alone is cleared when the governor's daughter is won over (VA
`0x12ce5`, `and [0x45f0a], 0xf7`): the bank opens again.

### Town screen (VA `0x1869b`, picture VA `0x18547`)

- **Picture:** `town.pcx`.
  - `town.spr` frame 0 (the harbour, the ship and the shipwright's
    shed) is drawn at (0, 258) when the ship is in port.
  - Frame 1 (the mansion's gate shut) is drawn at (582, 216) when the
    governor is unavailable.
- **Scroll:** the town's name, PIRATE1, centred at (325, 38).
- **Buildings:** seven rectangles (table at VA `0x339f4`: x0, y0, x1,
  y1, enabled):
  - 0 governor: open while the town has 6 or more citizens and he has
    not been seen this stay;
  - 1 merchant;
  - 2 bank: closed after sneaking in;
  - 3 tavern;
  - 4 shipwright: closed when the ship is not in port;
  - 5 city information (the scroll);
  - 6 leave town.
- **Choice 7:** the captain's cabin, from the Space key.
- **Labels:** pointing at a building shows its label (CSTN group 84) on
  the scroll; pointing elsewhere restores the name.
- **Keyboard:** the arrows step through the open buildings, wrapping
  around. Enter or Escape goes into the selected one.
- **Time in town (VA `0x18771`):** each pass through the menu loop
  calls VA `0x1dd0e` with `si` days, which advances the day counter (VA
  `0x46052`) and runs the month-boundary updates as at sea (the crew
  does not speak in port: VA `0x47aca` is not 0). `si` is 3 after every
  pass (VA `0x1878d`), so getting in costs 3 days, and so does each
  return from the governor, the merchant, the bank, the tavern or the
  shipwright. The city information and the cabin set `si` to 0 (VA
  `0x188b2`, `0x18901`): they cost nothing. Leaving ends the loop before
  the next pass. The first pass is free when bit 2 of VA `0x45e84` is
  set (a saved game just loaded), and the bit is then cleared (VA
  `0x186bb`-`0x186c3`).

### The captain's cabin (VA `0x9ffc`)

- **Picture:** `cabin.pcx`. `cabin.spr` frame 0 is drawn at (325, 229)
  while none of the three slots at VA `0x47186` (treasure maps, it
  seems) is filled.
- **Title:** "Captain's Cabin" (CSTN 92.7), PIRATE1, centred at (320,
  38), in ink 0. Pointing at a spot shows its label there (CSTN 86).
- **Spots (table at VA `0x327e0`, x0, y0, x1, y1, on):**

  | Spot | Label | Engine |
  |---|---|---|
  | 1 | Continue travels | back to the town or the sea |
  | 2 | Treasure maps | the maps held, one after another; on while one is |
  | 3 | Treasure cave | the cave, back to the cabin |
  | 4 | Logbook | the log book (below) |
  | 5 | Map of the Caribbean | below |
  | 6 | Search | digging, on foot only (see "Treasure maps") |
  | 7-8 | Divide party, Join party | always off, as in the original (below) |
  | 9 | Save game | the save slots |
  | 10 | Exit game | "Quit (Y/N)" (see "Retirement") |
  | 11 | Practice fencing | below |

- **The log book (VA `0xc46e`):** on `shiplog.pcx` (picture 0x14), in
  ink entry 0: the date (CSTR 93, month and year) centred at (320, 20)
  in PIRATE2, "LOG BOOK" (CSTN 95.6) at (320, 50) in PIRATE4, and the
  menu "Ship's Log, Party Status, Personal Status, Ship Status, Hostage
  Status, Exit" (CSTR 312) at (250, 100) up to x 500. Each page shows
  its title (CSTN 95) centred at (320, 40) in PIRATE4, then waits:
  - **Ship's Log (VA `0x1fc0f`):** the log's entries with codes 1-36
    (VA `0x1fa07`), oldest first, 10 a leaf, each a dated line (VA
    `0x1e144`), at (140, 100) up to x 520. With none, the leaf is
    skipped (VA `0x1fcd9`-`0x1fce0`, before VA `0x1fa91`): the page
    shows its title alone. The picture's arrows (200, 420)-(240, 450) and (380,
    420)-(420, 450) turn the leaves when there are more; the EXIT
    bookmark (0, 360)-(100, 479) leaves (table at VA `0x34638`).
  - **Party Status (VA `0xa2ca`):** CSTR 104 at (160, 100) up to x 560:
    the leader, crew, cannon, gold, food in days (`(food x 256 +
    fraction) / crew`), the crew's mood word (CSTN 72, `clamp(mood / 4 +
    1, 0, 3)`), goods, and the third good under its era's name (CSTR
    87 Hides in 1560, 88 Tobacco in 1600 and 1620, 89 Sugar after; era
    codes below 2, below 4, VA `0x1d8b2`).
  - **Personal Status (VA `0xa5a7`):** CSTR 105 at (140, 100) up to x
    560: for England, France, Holland and Spain, "   %s %s" (CSTR 90)
    with the nation and the rank, or without one CSTN 21 by standing (2
    or more "Letter of Marque", 0-1 "(no title)", -1 to -3 "wary",
    below "hostile"); "(married)" or "(single)" (CSTR 91, 285; VA
    `0x45e86`); age `years + 25`; health
    (CSTN 36); the fortune; acres (x 50); the reputation word (CSTN 34,
    `clamp(reputation / 4 + 1, 0, 5)`) and the difficulty (CSTN 35).
  - **Ship Status:** the fleet panel only to look (VA `0xffae` mode 0:
    no prompt, a blank button).
  - **Hostage Status (VA `0xa450`):** each of the 8 prisoners (VA
    `0x4720a`) as "%s (hunter)" or "%s (pirate)" (CSTN 22.1, 22.2, by
    bit 0x100), the pirate brought in for a mission (CSTN 22.2), or
    "NONE" (CSTN 92.9), at (240, 100) up to x 560.
- **Map of the Caribbean (VA `0xb464`):** on `citymap.pcx`:
  - A dot per town, `misc.crs` frame 1 at the town's fields 29, 30 (its
    map position, about its cell x 0.75) less 3.
  - The player's mark (frame 3): his town's dot in town, else his cell
    x 0.75. The selected town's mark (frame 2) and the player's blink
    in turn every 5 timer ticks.
  - The right panel lists 20 town names, centred at x 554 from y 40
    every 20; the arrows at (480, 0)-(639, 29) and (480, 450)-(639,
    479) scroll it by one, within the towns.
  - The selected town (kept between visits, the first at the start):
    its name and "Forts:", "Status:" (CSTN 9 by wealth / 64), "Nation:",
    "Soldiers:" (x 10), "Citizens:" (x 10) from CSTN 94, centred at
    (84, 384), (243, 384), (404, 384), (84, 440), (243, 440), (404,
    440); each value reads "?" (CSTN 92.5) until the town's
    information is known (flag 0x80).
  - Clicks: DONE (348, 315)-(479, 359) leaves (the engine starts it
    at y 326, below the dots of the coast towns at y 312-319); on the map, the town
    within 2 pixels; in the list, its row. Escape and Space leave; the
    arrow keys move a bar over the list rows (VA `0xb3ef`), which the
    engine replaces by selecting the previous or next town.
  - In the debug mode (VA `0x45e6c`, see "Sword fights"), a right click
    moves the ship there.
- **Divide and join (dead in Pirates! Gold):** each time the cabin
  opens (VA `0x9ffc`) the flags of spots 7 and 8 (VA `0x32824`,
  `0x3282e`) are set to 0 before the picker, and nothing else sets
  them; their handlers are reached only through those spots. What the
  handlers would do:
  - Divide (VA `0xac0a`): ships picked on the fleet panel ("Select the
    ships you want to leave behind", CSTR 128) leave as a squadron in one of three 48-byte records at
    VA `0x45fc2` (cell, men, ship count and types, hold), taking 8 men
    per ship plus any men the remaining ships cannot hold, a share of
    the gold in proportion to the men, `men x 7 / 256 + 1` food, and
    cargo moved on the trade board (mode 4), again while the main
    holds overflow (CSTR 129).
  - Join (VA `0xaee9`): the squadron's men, gold and ships come back
    (beyond 8 ships the panel asks which to abandon, CSTR 130), and its
    cargo on the trade board; with the hold over full the board asks
    to throw goods overboard (mode 1, CSTR 113).
  - The engine leaves both off.
- **Practice fencing (VA `0xbdae`):** a sword fight on the deck (scene
  3) against the first mate (title 5): 200 men of factor 32 a side, the
  weapon chosen first. Afterwards the practice value (VA `0x45e90`)
  rises by 5, to at most 30.
- **Keys (table at VA `0x32a60`):** Right and Left step through the
  enabled spots, wrapping around; Enter takes the selected one; Escape
  and Space both continue the travels (VA `0xb3a5`).
- **Language (key `L`, VA `0xb3cd`: choice 99, VA `0xa2a7`, `0xc6bb`):**
  the screen fades, then `shiplog.pcx` (picture 20) shows CSTR 318 at
  (250, 100) up to x 500, a menu of English, French and German, the
  values of the language word at VA `0x33ef0`. Another language than
  the current one is stored and its texts (`cstr`/`cstn`, VA `0xba2`)
  and `misc.crs` plaques (VA `0xcb0`) are loaded; then the cabin again.
  The engine also goes back unchanged on Escape, and keeps the old
  language when the new one's texts are missing.

### City information (VA `0x19c62`, text VA `0x196d0`)

- **Picture:** `arr_sea.pcx`, or `arr_land.pcx` for an inland town or
  when the player came on foot (VA `0x47aca` bit 0; VA `0x19c8a`-
  `0x19ca7`).
- **Text:** at (400, 150), up to x 620.
  - With current information (flag 0x80), CSTN 23.0 is filled with:
    - the name and nation;
    - the wealth word (CSTN 9, `wealth / 64`);
    - the forts (CSTN 23.2 with the number word of CSTN 79, plural
      unless 1);
    - soldiers x 10 and citizens x 100;
    - the gold, when there is any (CSTN 23.3, in thousands).
  - Otherwise CSTN 23.1 ("no info"), or 23.4 when the information is
    obsolete.

### Shipwright (VA `0x218c2`)

- **Refusals, before anything else:**
  - "We do not trade with pirates!" (CSTR 256) in a Spanish town when
    `rank with Spain x 25 + the era's tolerance` (VA `0x45e7f`, and
    `0x4707f`, the era block's byte `0xF9`: 105 in 1560, 150 in 1600,
    135, 110, 115, 145) is below the town's wealth (word 8). The same
    test guards the merchant (VA `0x21ed3`).
  - "You have nothing of worth" (CSTR 278) when the arrival mode (VA
    `0x47aca`) is 3, probably a party on foot.
- **His gold (once per stay, VA `0x45f08`):**
  - `x = trunc((|6 - month| + 6) x wealth / 6)`, with the month 0-11;
  - `gold = ((citizens + 10) x x / 150) x 600`, at most 50,000.
  - It is kept for the stay, and reset in another town.
- **Menu (CSTR 277):** parting company, repairing a ship, selling a
  ship, selling some cannon, at (40, 300) up to x 580. His notes (the
  refusals, CSTR 279, 282-284, 319) are at (100, 300) up to x 540 (VA
  `0x21932` and the like).
- **Prices (VA `0xffae`):** with class `c` and damage level `d =
  min(damage / 16, 3)`. The class comes from VA `0xd1db` (called at VA
  `0x100d0`): `type & 7`, a galleon (5) counting as the fast galleon
  (8) while the era code (VA `0x45e8c`) is below 4, 1560 to 1620. The
  engine keeps the fast galleons it meets as type 8 and counts them so.
  - a repair costs `50 x (c + 1) x d`;
  - a ship sells for `50 x (c + 1) x (10 - d)`.
- **Repairing:**
  - "I see nothing wrong" (CSTR 282) when no ship is damaged.
  - The chosen ships' total must be below the player's gold, or he
    cannot afford it (CSTR 283).
  - The gold goes to the shipwright, and the ships' damage is cleared.
- **Selling:**
  - Not the only ship (CSTR 279), and at most all but one.
  - Ships go in fleet order while the shipwright's gold covers the
    price; otherwise he lacks the funds (CSTR 319).
  - A sold ship keeps its cargo in the fleet (VA `0x1f2a1` with keep 1).
- **Cannon (VA `0x22c76`):** "not the only one" (CSTR 284) under 2
  guns. Otherwise the merchant's trading board, lower on the
  shipwright's picture, with "I pay 50 gold pieces per cannon" (CSTR
  313) above it:
  - One row: the shipwright's stock, from 0 each time the board opens
    (VA `0x22e12`), and the player's guns, at 50 gold either way. The merchant's steps apply
    (VA `0x20ad4` sell, `0x20bcd` buy).
  - Selling needs the shipwright's gold to cover the price.
  - Buying back needs the player's gold, his stock, and room in the
    hold. As for the merchant, only a hold exactly full refuses (the
    test is `space != 0`), so an overfull hold still takes guns.
- **Engine choices:**
  - Ranks come from governors' promotions; standing rises only through
    captures and Letters of Marque so far.
  - The original picks several ships at once on a fleet panel; the
    engine takes one ship per choice and shows the list again.

### Bank (VA `0x180c8`)

Dividing the plunder ends an expedition. The date is on the small
sign, the texts at (40, 280) up to x 620.

1. **"Are you sure you want to divide the plunder?"** (CSTR 86 with 95).
   No leaves.
2. **The crew's share (CSTR 155):**
   - `share = trunc(gold / crew x 0.8 + 0.5)`.
   - The crew's mood (CSTN 72, angry to happy) is
     `min(trunc(sqrt(share) / 23), 3)`.
   - The reputation becomes `(trunc(sqrt(share)) + reputation + 1) / 2`.
3. **The captain's portions (CSTR 156):** `2 x difficulty + 2`
   portions, worth `(difficulty + 1) x gold / 20`, added to the
   captain's own fortune (VA `0x45efe`).
4. **The treasure cave (VA `0xbdfe`):** eight piles (`tcave0`-`7`),
   each drawn once the fortune passes its mark (table at VA `0x32928`:
   x, y and the mark; 500, 1000, then by thousands to 7000).
5. **The career may end here (VA `0x1f1d7`):** a famous expedition is
   over (CSTR 159 at (100, 40) up to x 540), or at health 3 (see
   "Prison") "Your crew scatters to the winds. Advancing age and old
   battle wounds combine to sap your strength..." (CSTR 160 at (200,
   200) up to x 620), on a cleared screen; both go to the ending
   without the lure back to the sea. Health is read nowhere else but
   the Personal Status, prison and the ending: age does not slow the
   sword (VA `0x1db64` and its callers).
6. **"Do you... Plan another expedition? Retire from privateering?"**
   (CSTR 157). Below Swashbuckler (difficulty 3), "Advance to <level>
   level?" (CSTR 97) is a third choice that raises the difficulty.
   - Retire asks "Are you sure you want to end the game?" (CSTR 86
     with 96). Yes goes to the retirement.
   - Another expedition (VA `0x184a8`, `0x1ef87`): the refit (CSTR
     158) advances the calendar four times by 26 days (VA `0x1dd0e`,
     the world's monthly step for each month crossed). Then the
     flagship alone, repaired, with a tenth of the gold; when the gold
     is 0, a tenth of the fortune less a tenth. The fleet gets that
     ship's capacity, `type x 2 + 6` guns, 15 food, no goods, and a new
     crew of `reputation + 16`, of which only exactly 0 becomes 1 (VA
     `0x1f017`-`0x1f029` tests the word unsigned); the engine makes any
     count below 1 one man (an engine fix).

### Cargo

- **Holdings:** the player block holds food (`+0x0c`), sugar (`+0x0e`),
  a food fraction (`+0x10`), the hold capacity in tens of tons
  (`+0x12`) and goods (`+0x2c`), besides the guns (`+0x0a`).
- **Start values:** a start scenario sets food, goods and sugar from
  bytes 0x1a-0x1c, the fraction from 0x1e and the capacity from 0x1f.
- **Capacity:** a captured ship adds `2 x class + 2`; a lost or sold
  ship removes its share.
- **Space in the hold:**
  `capacity x 10 - guns / 2 - food - goods - sugar` tons.

### Merchant (VA `0x21e56`)

- **Greeting:** written on `merchant.pcx` at (160, 40) up to x 480, in
  ink 0, without a box.

- **Refusal:** as at the shipwright (CSTR 256), except for the
  merchant's family (town word 12 `& 0xe0`, set by marrying his
  daughter).
- **Greeting by the player's gold:** up to 2000, 5000, 20000, then
  more: CSTR 257-260, or 291-294 for the family.
- **Prices and stock,** rolled at each visit, with
  `W = (|6 - month| + 6) x wealth / 6` kept in floating point:
  - **Merchant's gold:** `trunc(W x (citizens + 10) / 150) x 600`, at
    most 50,000, kept for the stay.
  - **For goods i = 0, 1, 2 (food, goods, sugar):**
    - stock `= b + rand(b)`, with `b = trunc(W x (citizens + 10) /
      150)`;
    - base `= trunc(W / 50 + 3 - i / 2)`;
    - halved plus 1 when town flag bit `3 - i` is set (a local
      product);
    - sugar's doubled when bit 4 is set;
    - price `= base x 10`.
    - **Quirk:** the code then reuses W's variable, so each later good
      uses the previous good's price as its W, for both stock and
      price. The engine does the same.
  - **Then:** food and goods x 1.5, sugar x 1.5 then x 2 (truncating).
    For the family, each price x 0.75, at least 1.
- **Trading:**
  - One price both ways, one unit at a time.
  - **Buying** needs merchant stock, the price in gold and space in the
    hold: only a hold exactly full refuses (VA `0x20c2c`, `cmp
    [0x49b0e], 0; je`), so an overfull one still buys.
  - **Selling** needs a unit and the merchant's gold.
  - The gold changes hands; the merchant's stock rises and falls.
- **Closing:** if the player's gold moved by more than 20, the town's
  wealth becomes `wealth x 19 / 20 + 12`, at most 255.
- **After the business** (not when looting a sack), on `merdaugh.pcx`
  (picture 0x2e) at (320, 150) up to x 620:
  - The family: "That evening, you enjoy a delicious meal with your
    lovely wife" (CSTR 295).
  - Otherwise, single, one time in ten (`rand(10) = 0`): "...The
    merchant introduces you to his daughter..." (296). Conversation
    ends it. A proposal: a fortune below 400 (200 with charm) brings
    "...my father will not let me wed anyone who is not wealthy..."
    (297); otherwise his debt (298, at (320, 50)): pay it or withdraw.
    Paying needs 500 gold (the text says 5,000): short of it, on
    `merchant.pcx` at (160, 40) up to x 480, "The merchant sneers at
    your offer of help..." (300); paid, on `wedding.pcx` at (360, 150),
    "Amid great merriment, you are married... I'll give it to you
    wholesale." (299): married (VA `0x45e86` = 1), town word 12 | 0x40.
- **The board (VA `0x20229`):**
  - Layout records at VA `0x348c4`: frame, highlighted frame, x, y.
  - The merchant's column is on the left and the player's (the skull)
    on the right.
  - Digits are glyphs of `buysell.spr`: big 0-19, small 20-52.
  - Pieces are per language (`buysell`, `buyboard`, `buysell1`, with
    `f` and `g` variants; VA `0x20138`, the names' table at VA
    `0x3487c`). The loot board (VA `0x2264f`) loads the second set,
    `take-dis`, `takboard`, `takeds1` (`takedisf`... `takedisg`...),
    whose sign reads Discard and Take, and sets layout record 0, the
    name tag, to frame 88 + mode: Throw overboard, Captured ship,
    Plundered town, and frame 92 for dividing the squadron.
  - **Its areas (VA `0x20515`, table at VA `0x34b04`):**
    - The slider's arrows, (226, 62)-(248, 90) and (386, 62)-(410, 90),
      and the two name tags, (0, 0)-(83, 70) and (557, 0)-(639, 70):
      left sells to the merchant (on the loot board, leaves it), right
      buys (takes), a unit at once and again every 100 loop passes
      while the button is held.
    - A row's arrows, (169, 107 + 50k)-(190, 147 + 50k) and (444, …)-
      (465, …): they select the row and the way (VA `0x21122`), and
      trade nothing.
    - The sign (234, 100)-(314, 126) only shows the way; a click on it
      does nothing.
    - "Take all" (321, 100)-(401, 126) (VA `0x20cd9`): while the hold
      has room, a unit of the selected good, or else of the first good
      left (food, goods, sugar, cannon), bought while the selected
      good's price is below the gold. The original then loops for ever
      when the player cannot pay; the engine stops.
    - The exit (252, 433)-(391, 479).
  - **The slider (VA `0x20ff3`, frame 0x4d):** at `x0 + trunc(held x
    (x1 - x0) / (stock + held))` in single floats, for the selected row:
    the merchant's stock and the player's holding (the words at VA
    `0x33f74 + 4 x row`, +2). The track runs from x 249 to 374 at y 72
    (VA `0x34adc`), for the shipwright's guns (row 4) at y 318 (VA
    `0x34aec`); with no units it rests at x0 (the original divides by
    zero).

### Reputation (VA `0x45e76`)

A signed byte, 0 at the start:
- +1 for each capture (VA `0xdb7b`);
- -2 for a failed boarding, beaten back or beaten (VA `0xe775`);
- set to -1 after a disgrace in a duel (CSTR 174);
- -1 for an assault on a town that fails (VA `0x18f57`);
- +2 for a sack that yields 100 gold or more (VA `0x22af1`);
- -4 for a mutiny (VA `0x1de82`);
- each month, `r -= r >> 3` (VA `0xcf0c`);
- the bank moves it halfway to the crew's share (VA `0x181fb`, see
  "Bank").

The prison reads it for its spells, `rep / 2 + 7` (VA `0x1f834`).

It names the player in the tavern (CSTN 34[`clamp(rep / 4 + 1, 0,
5)`]: cowardly ... infamous) and brings recruits.

### Tavern (VA `0x11867`)

- **Picture:** `tavern.pcx`. The tavern's name (CSTN 96, by town) is on
  the scroll at (320, 38).
- **People:** ten spots (table at VA `0x330f0`: x0, y0, x1, y1,
  present):
  - 0 the map seller and 1 the information seller (`tavern.spr` frames
    0 and 1 at (0, 255) and (142, 259));
  - 2, 7, 8 and 9 seamen to recruit;
  - 3 the bartender;
  - 4 the barmaid;
  - 5 a stranger (frame 2 at (349, 199));
  - 6 the door.
- **Presence:** all are back on the next arrival in town (VA
  `0x18fa9`). Spoken to:
  - the map seller and the traveller leave for the stay whatever the
    answer, a refusal or no gold included (spots cleared after their
    calls, VA `0x1191b`, `0x11935`);
  - the four seamen spots all leave, with no recruits or a refusal too
    (VA `0x10fdf`-`0x10ff3`);
  - the bartender leaves only when he had no public news (VA `0x1185b`);
  - the barmaid leaves after her report;
  - the mystery man's presence is checked again after the meeting (VA
    `0x119de` calls VA `0x10ffe`): he stays while a villain still hides
    here or a contact is due.

  On each entry (VA `0x11867`):
  - The traveller (VA `0x110c7`) draws a price `(rand(10) + 5) x 10`
    and a random town, again while that town's information is current
    and the price is not 50. He stays away when the price is 50 or the
    town is the one the player is in. Drawing sets visit flag 2.
  - The map seller (VA `0x112b1`) is there one time in five (the draw,
    `rand(5) == 0`, kept at VA `0x44ae2`), and never once the pirate's
    map is whole and shown (its slot reads `0x1f`).
  - The mystery man (VA `0x10ffe`) is there while a villain hides in
    the town or the mission's contact lives there (see "Villains" and
    "Missions"). Clicking him meets the villain (VA `0x18913`), else
    hands over the letter or papers (VA `0x1f8f7`), by the reason VA
    `0x10ffe` left at VA `0x44ae4`.
- **Labels:** pointing at someone writes their name on the scroll
  (CSTN 90: Map Seller, Traveller, Unemployed Sailors, Bartender,
  Barmaid, Mystery Man, Exit).
- **The map seller (VA `0x112ef`), on `mapman.pcx`:** "…part of an old
  treasure map for 500 gold pieces" (CSTR 153, at (340, 150) up to x
  620), but the code takes 50. Without the gold: CSTR 94. Paying gives
  a piece of the pirate's map and shows it (see "Treasure maps").
- **The traveller (VA `0x1116f`), on `infoman.pcx`:** "A traveller from
  %s offers to sell you information for %s gold pieces" (CSTR 152, at
  (20, 150) up to x 280). Paying makes that town's information current,
  clears the "obsolete" mark of the town the player is in, and shows
  the town's information.
- **The barmaid (VA `0x114d4`), on `barmaid.pcx`, texts at (40, 150) up
  to x 320:** her fondness is town field 13.
  - Below 11: at 0, five times in six, "Will ye be wantin' a drink?"
    (CSTR 301); yes costs 1 gold. Otherwise CSTR 302: nothing, a drink
    (1 gold), or a drink and company: with 5 gold, fondness +1 and "a
    pleasant evening" (CSTR 303), or at 10 "You've won the barmaid's
    heart!" (CSTR 304).
  - Above 10 she reports: nothing (CSTR 305) when neither the traveller
    nor the map seller is there; otherwise CSTR 306, then the
    traveller's town (CSTR 307, the town's information) or a map piece
    (CSTR 308). Either way she leaves for the stay.
- **Recruits (VA `0x10e2a`), counted on entering:**
  - `n = (rep + 7 - difficulty) x (citizens + rand(citizens)) / 9`;
  - with fewer than `4m` men aboard, at least `m = (4 - difficulty) x
    6`;
  - at most `(ships x 2 + capacity) x 16 - crew`; a negative count
    becomes 1 (VA `0x10ef4`, `jns`).
- **Inks:** the tavern sets ink entry 1 (palette index 2) and highlight
  entry 2 (index 6) for its pages.
- **Recruiting (VA `0x10f04`), on `crew.pcx`:**
  - Written on the picture at (60, 300) up to x 620, the choice up to x
    560, without a box.
  - CSTR 151 offers the men, naming the player by reputation, rank with
    the town's nation (`rank[0x4605c]`, VA `0x10f2f`) and name.
  - Signing them up adds them to the crew; all four seamen spots then
    leave.
  - With no recruits: CSTR 309.
- **The bartender (VA `0x1183e`):**
  - On `bartenda.pcx`, in ink entry 0, "The Bartender tells you that:"
    (CSTN 18.1), then up to 12 recent public news items, at (222, 40)
    up to x 610 (VA `0x1e78d`, mode 1).
  - With news, that is all, and he stays. With no public news (VA
    `0x1184c`, the count at VA `0x49b3a`), he listens to the player's
    last exploit, gives what he knows of villains here (VA `0x1eba6`,
    see "Villains"), and leaves for the stay (spot 3 cleared, VA
    `0x1185b`). The exploit (CSTR 125 with CSTN 33) is at (320, 150) up
    to x 560 (VA `0x10b10`): from
    the newest log entry back (a visit at the head passed over), the
    first of codes 1-8 or 22-26; none, or a visit to this very town,
    gives "your promising plans" (33.0). Its words: the town (1, 3, 23),
    CSTN 68 by `a / 64` and the rank (2), CSTN 68 by `a / 16` and the
    ship class as written (4-6: the `tolower` at VA `0x10ce5` is
    called on its first letter and the result dropped, as at VA
    `0x4ed8`, `0xdc0a`, `0xe7db` and `0x1e2a8`), the town and CSTN 71
    (22), the relative (26).
- **Personal log entries:** a visit (code 1, the town) on every arrival
  in a town (VA `0x17e78`, called by the main loop at VA `0x1da14`),
  except the arrival a saved game resumes: loading sets bit 1 of VA
  `0x45e84` (VA `0x134f2`), which skips that arrival's log and the town
  menu's first 3 days, and the menu then clears the bit (VA `0x186c3`).
  The engine resumes a save in the cabin, which logs nothing either. A
  title (2, `nation x 64
  + rank`, VA `0x12549`); after a sea battle not against a fort, 4 for
  a capture and 6 otherwise, `nation x 16 + class` (VA `0xe986`). Code
  5, a sinking, is never logged.

### Standing and ranks

- **Layout:** the player's standing per nation is a signed byte array at
  VA `0x45e7a` (index 4: pirates). The rank per nation (CSTN 78:
  Mr. ... Count) follows at VA `0x45e7f`.
- **Attacking a nation N by d (VA `0x1eaf5`):**
  - N's standing changes by d;
  - every other nation's by `d x relation(N, k)` (+1 alliance, -1 war);
  - but such changes cannot lift a standing above 1, nor drop one that
    is above 1 below 2.
- **The callers:** capturing a ship of a nation (not pirates), d = -1
  (VA `0xdb7b`); a repelled or beaten assault, -1 (VA `0x18b3f`); a
  sack, -1, and -2 more when it yields 100 gold or more (VA `0x22764`);
  a governor installed for another nation, -2 against the old one,
  while the new one's standing rises by 3 (VA `0x22764`).

### Governor (VA `0x13269`)

He receives the player once a stay, in towns of 6 citizens or more. In
order:

1. **The news (VA `0x11d64`):** "My dear <rank> <name>," with the
   nations his own is allied with and at war with (CSTN 25, names from
   CSTN 81). It ends with a charge to fight them when at war and the
   standing is 2 or more. With neither allies nor enemies: CSTR 149.
   The music is 18 when at least one enemy is named and the standing
   is 2 or more, else 17 (VA `0x1208e`-`0x120b9`).
2. A villain hiding in the town (VA `0x18913`, see "Villains"), then
   the mission (VA `0x12e88`, see "Missions"). VA `0x12432` rewards
   prisoners with bit 0x8000, which only the mission's pirate has and
   who is never held: it never fires.
3. **Promotion (VA `0x12549`):**
   - With `r` the next rank, when `standing >= r^2 + 2` and `r <= 9`:
     the rank rises, and the standing drops to `r^2 + 2`.
   - CSTR 134 is shown with the proclamation (`gov.spr` frames at
     (140, 35) and (113, 262)).
   - When the surplus plus the town's word 9 is positive, that many
     times 50 acres are granted (CSTR 135). VA `0x45f02` counts the
     grants in 50-acre units.
   - After a promotion, while a relative is still lost and not in a
     famous expedition (VA `0x45e84` bit 0, set when one is chosen, VA
     `0x15bc6`, cleared by a new career's era menu, VA `0x15a3c`), a new
     villain is named (VA `0x12178`, see "Villains").
4. **Ransoms (VA `0x127da`, see "Named captains").**
5. **His offer, by standing s:**
   - With `s > -4`, the price is `(4 - s^3) x 200` gold.
   - From s = 2 the price is negative and his daughter is met instead
     (VA `0x128c1`, see "The governor's daughter").
   - Otherwise (VA `0x12d6d`): a Letter of Marque for `s >= 0` (CSTR
     147), a pardon for `s < 0` (CSTR 148).
   - Paying raises the standing by 2, to at most 2, and resets the
     town's word 9. Without the gold: CSTR 94.

## Intro (VA `0x3325`)

1. `micro.pcx`: fade in, 2 s (20 x 100 ms), fade out.
2. On black, keeping that palette, each sprite faded in and out at its
   top-left position:
   - `and.spr` at (229, 227), 1 s;
   - `mpslabs.spr` at (195, 100), 2 s;
   - `present.spr` at (134, 231), 1 s.
3. `moon03.pcx`, faded in:
   - **Palette cycling:** 2000 loop passes (VA `0x36e4`). Each step
     rotates a range of palette entries up by one: 0xee-0xfe every 301
     passes, 100-163 every 51, 0xc2-0xce every 101 (each counter fires
     when it drops below 0, then reloads 300, 50 or 100, VA `0x370a`).
     The fades take 0x40 steps for the logo and the sprites, 0x80 for
     `moon03.pcx`; their speed is the CPU's.
   - **Title (VA `0x379f`):** `title.spr` and `title1.spr`, stacked at
     x 272, dissolve in by 2 x 2 cells of a 184 x 137 grid (the area
     is 136 cells high, but row 136 is visited too, VA `0x38a3`). The
     cells are visited in the order `x = (5x + 1) mod 32768` from a
     random start; values past the grid are skipped. One cell is
     revealed per pass, and the same cycling goes on, its counters
     started afresh (VA `0x3855`).
   - **Then:** 0xd3-0xec rotates every 301 passes, 26 times, then
     pauses for 10000 passes; the other three cycles go on.
   - **End:** after 45000 passes, at the end of a 26-step cycle or
     during the pause that follows (VA `0x39f5`), the picture fades out.
4. **Skipping (VA `0x2246`):** only Enter, Escape or the left button
   (released) count; other keys are read and ignored. During the logo,
   "and" or "MPS Labs" the intro ends at once, without a fade out (VA
   `0x35f4`); during "Present" only its wait is cut, and the moon
   follows (VA `0x34fb`); during the 2000 passes before the title, the
   title starts (VA `0x3781`); during the title, its fade out follows.
   The fades read no keys: a key pressed meanwhile waits in the event
   queue for the next wait.

Then the credits (VA `0x1fe91`):
- **Picture:** `credpic.pcx`, with the language's credits picture
  (`credtext`, `credtxtf`, `credtxtg`) as the source.
- **Blocks:** six rectangles of the credits picture (table at VA
  `0x34668`), shown one at a time. Each is centred at (508, 240), with x
  rounded down to a multiple of 8, `(504 - w / 2 + 4) & ~7`, and y `240
  - (y2 - y1) / 2` (VA `0x1ffaf`, `0x1ffce`).
- **Fades:** the text is palette entry 0x20. Its colour steps through
  20 entries (table at VA `0x34698`) from the parchment colour to black,
  40 ms each. It then holds 1.6 s and steps back, and the screen is
  left without a block for 0.4 s (VA `0x2010f`, `0x20114`).
- **Keys:** the intro's (Enter, Escape, the left button). One during
  blocks 0-4 jumps to block 5; one during block 5 ends the credits.
  The ink steps read no keys.
- **Open credits:** a pack's `text/<lang>/credits` replaces the blocks
  with text drawn on the picture, with the same timing and keys (see
  "Open credits" in asset-packs.md).

The welcome menu (VA `0x1443e`) is full screen on `textback.pcx`
(its last option, "Return to DOS?", reads as the cabin's "Exit game"
(CSTN 86.9) with a question mark: an engine choice, translated in every
language):
- the 10 lines of CSTN group 100 are centred at x 320, at heights 70,
  120, 145, 170, 195, 250, 300, 330, 360 and 390;
- the 6 heading lines use PIRATE4, and the 4 options PIRATE1. The game
  loads its fonts in the order PIRATE1, PIRATE2, CAPS, PIRATE3,
  PIRATE4;
- each option's mouse area is a full-width row 30 pixels high;
- every line is in ink entry 0 (word VA `0x3421c`, VA `0x14460`).

A word at VA `0x4c0e2`, set from the command line, skips the intro (VA
`0x3027`), the credits (VA `0x1fe98`) and the menu loop (VA `0x144fb`).

The title screen's passes wait 1/10 of a 10 ms tick, which is 0. So
their speed is the CPU's. The engine runs 167 passes per 1/60 s frame,
about 0.1 ms each, and starts the dissolve at cell 0.

## Screens in general

- **Inks:** text colours are palette entries of the current picture,
  from the table at VA `0x3421c`: 0, 2, 6 and 31.
  - The story uses entry 2, an orange in `frame.pcx`.
  - The engine reads that entry from the picture where it has been
    traced, and uses fixed colours elsewhere.
- **"Click to continue" (VA `0x1a3c4`):** after a message, the game
  waits for a key.
  - After 420 vertical retraces without one (about 7 s), it draws the
    plaque of `misc.crs` frame 7 (`miscf`, `miscg` by language) at
    (268, 449), then keeps waiting.
  - The engine draws the same plaque after 420 idle ticks wherever it
    waits for a key.
- **Hot keys (table VA `0x321e8`, VA `0x2d60`):** every wait for input
  (20 call sites: menus, the identity screens, sailing, on foot, the
  map...) first checks for Q, P and V, either case; the keyboard poll
  (VA `0x275a2`) hands Q and P to the same handlers. Q and P do nothing
  while the screen is faded out (word VA `0x4c858`, set by the fade out
  at VA `0x1d3a5`, cleared by the fade in at VA `0x1d476`).
  - **P, pause (VA `0x2e91`):** the music driver pauses (call 6), the
    pointer hides, and the plaque of `misc.crs` frame 5 ("Game
    Paused") is drawn at (268, 218), where "Quit (Y/N)" goes. The game
    then waits for P or p only; other keys are dropped. The plaque's
    background is put back, the pointer shown, the music resumed (call
    7), and the game goes on where it was.
  - **Q (VA `0x2db9`):** the same, with the plaque of frame 4 ("Quit
    (Y/N)") and the keys at VA `0x31610`: Y blacks the screen and
    leaves the program (VA `0x737`); N goes on; other keys are dropped.
  - **V, the sound level (VA `0x2f38`):** steps the word at VA
    `0x4c0ec` through 0, 1, 2 and back to 0; on 1 and 2 it stops the
    music (command 1, VA `0x1b8ee`). Only VA `0x1b8cb` and `0x1b8a2`
    read the word:
    - 0: music and FM effects;
    - 1: FM effects only, no tune starts (VA `0x1b8cb`);
    - 2: neither (VA `0x1b8a2`).
    - Back to 0, nothing restarts the music: the next tune asked for
      plays. Nothing reads the word for the digitised effects (VA
      `0x7d82`), which play on. The keyboard poll does not take V: it
      works where the game waits for input, not in the fights.
  - A string "Game Paused. (Press space key to resume.)" (VA `0x33c26`)
    has a pointer at VA `0x33c50` that no code reads: the plaque
    replaced it.
  - The engine takes P, Q and V anywhere but while a screen fades or
    a name is typed (where they are letters), V in the fights too. The
    level is not saved: each run starts at 0. Key releases still reach
    the screen, so a key let go during the pause does not stay held.
    Q's prompt takes the language's yes like the other prompts, and
    Escape as N; yes fades out and closes the program.
- **The mouse pointer:** drawn by the game itself (VA `0x250cb`, from
  the mouse handler at VA `0x248f1` unless the hide flag at VA
  `0x49b5e` is set). Most screens hide it only while they redraw (VA
  `0x18d8`, `0x18ec`). It stays hidden:
  - for the whole of sailing, on foot too (VA `0x22c76`, which holds
    the loop at VA `0x231b8`);
  - during a sea battle's fighting (VA `0xe57e`-`0xe62a`) and a land
    battle's (VA `0x5aa3`-`0x5c1d`); their results show it again.
  - The sword fight shows it (VA `0x1692f`): the mouse steers there.

## New career: the time period (VA `0x15a3c`)

On `textback.pcx`:
- the title (CSTR 64) is centred at y 20 in PIRATE2;
- each period's name (CSTN 98) is centred in PIRATE1 at y 60, 130, 200,
  270, 340 and 410;
- its description (CSTR 261-266) sits under it in PIRATE3, across x
  40-620, at y 83 + 70 per period;
- each period is a full-width row 70 pixels high for the mouse (VA
  `0x333d8`).

## New career: the identity screen (VA `0x1482e`)

After the era (or a famous expedition), one picture, `idselect.pcx`,
fills in step by step. The captain's portrait (`bigman0.spr`) sits in
the oval at (237, 66).

1. **Nationality (VA `0x14aa4`, CSTR 70):**
   - Four small flags (`lilflgs.spr`, frames in the order English,
     French, Dutch, Spanish) at (0, 0), (51, 155), (481, 1) and
     (441, 151) (table at VA `0x33488`). The flags available per era are
     at VA `0x3357c`; no Dutch in 1560. The engine offers a flag when
     its career's start scenario has a name, so a pack may add one.
   - Pointing at a flag names it on the scroll (CSTN 82, centred at
     (320, 368)); a click chooses it, and the keys step through them.
   - The chosen nation's big flag (`big?flg.spr`) then stays at (0, 0),
     with its name at (160, 250).
2. **Special ability (VA `0x14e6e`, CSTR 67):**
   - Five icons (`lilskls.spr`) along the bottom (table at VA
     `0x334b0`). Slot i is ability i (CSTN 83 order); the icon frames
     shown are 0, 3, 1, 2, 4.
   - The chosen ability's picture (`bigfen.spr` ...) then stays at
     (440, 0), with its name at (520, 250).
   - A famous expedition starts here, with its own flag.
3. **Difficulty (VA `0x151c6`, CSTR 66):**
   - The four levels (CSTN 35) in a row at x 80, 190, 300, 410, y 345.
   - Pointing at one selects it: it is highlighted, described below
     (CSTN 101, centred at (320, 380)), and the portrait becomes
     `bigman0`-`3` for that level. A click or Enter confirms.
4. **Name (VA `0x155fa`, editor VA `0x157f4`, CSTR 65):**
   - The field starts as "Incognito" (CSTN 92.4); Enter or a click
     accepts it.
   - The first letter typed replaces it; Backspace only erases what was
     typed.
   - At most 11 characters.
5. **Confirmation (VA `0x15498`):** "Are ye satisfied with your choices,
   Captain?" (CSTR 273), with "Yes" (CSTR 274) at (208, 368) and "Nay"
   (275) at (390, 368) (click areas at VA `0x33470`). "Nay" starts the
   identity over from the nationality, keeping the era.

## New career: the story (VA `0x139e2`)

- **Setup (VA `0x1451e`):** before the story, the game lowers every
  nation's standing by 16, marks the towns whose information the player
  knows (by difficulty) and picks the missing family member.
- **Picture:** `frame.pcx` with the captain's portrait for the
  difficulty at (236, 58). The text is at (40, 340), up to x 620.
- **Scripts:** one byte script per career, at VA `0x35968`, separated by
  -1, in the order of the start scenarios (era x 4 + nation, English,
  French, Dutch, Spanish; then the expeditions).
- **Bytes:**
  - A CSTR number shows that text. Its `%s` are the treasure (CSTR 46,
    or 45 the silver train), a town, the year and the player's name.
  - -2 waits for a key and turns the page.
  - ')' waits for a key.
  - '*' prepares the fight, and 'a' fights it (VA `0x16205`).
  - 6 asks a question; 0 ends.
  - '2'-'7' show nothing.
  - The engine converts each script to steps (asset-packs.md,
    `data/stories`), which a pack may give instead.
- **The copy-protection question:**
  - ')' and '*' were the original's question.
  - **Pass:** the standings get their 16 back, and the fight is 200 men
    of factor 32 against 20 of 16.
  - **Fail:** difficulty 4, no guns, 8 gold, and a hard fight, followed
    by the script's other branch after its first 0.
  - In this build the comparison overwrites the expected answer with
    the given one (VA `0x14695`), so it always passes. The engine keeps
    the pass branch: the -16 and the +16 cancel, so the standings stay
    the scenario's, and it applies neither.
- **Engine choices:**
  - The career starts when the player confirms, before the story.
  - The story's fight uses the boarding fight's screen, with no weapon
    choice and nothing at stake.
  - Escape skips the story.

## New career: the sailing master (VA `0x13e3f`)

After the story, unless a famous expedition (VA `0x13348`, VA `0x45e84`
bit 0), which starts at sea in no town, on `smaster.pcx`, with the text
at (340, 100) up to x 620, two pages:

1. **CSTR 68:** "Here in <town>, you'll probably be wantin' to see the
   <nation> governor…".
2. **For each other nation:** its nearest town by `|dx| + |dy|` on the
   map, as CSTR 69, "The closest <nation> city is <town>, which lies
   <days> days to the <direction>."
   - `<days>` is "a few" (CSTR 48) under 31, otherwise "many" (CSTR 49).
   - The direction (CSTN 75) comes from the signs of dx and dy, after
     zeroing an axis that is less than half the other.

Each page's last character, a line break, is replaced by a closing
quote. The career then begins in the start town.

## Retirement and the Hall of Fame (VA `0x45d0`)

A career ends by retiring at the bank (dividing the plunder, then
retiring), or in prison (a famous expedition, or health 3). The cabin's
"Exit game" does not retire: it shows the "Quit (Y/N)" plaque
(`misc.crs` frame 4 at (268, 218), VA `0x31df`), answered with the
keys of "Disembark" (see "Landing parties"), and leaves the game.

1. **Points (VA `0x5039`):** `fortune / 10 + land x 50 + (the four
   ranks added) x 500 + reputation x 200 + married x 1000 + relatives
   found x 1000 - injuries x 1000` (married: 1 a merchant's daughter, 2
   a governor's, 3 his niece).
2. **Score:** `(int)(points / 330 x (25 - years) / 20)`, the years since
   the career began.
3. **Rating `r`, 0 best:** at least 1: `max(19 - score / 5, 1) - 1`;
   otherwise `min(20 - score / 10, 22) - 1`. `r / 4`, at most 3, picks
   the picture: `sunset4.pcx` (0) down to `sunset1.pcx` (3).
4. **Pages, on that picture:** "The Further Adventures of the Pirate
   %s..." (CSTR 74, at (20, 200) up to x 260), then at (20, 100) up to x
   260 (VA `0x4a1a`):
   - "You achieved the title of a Spanish Captain and an English
     Major, making you %s member of your community" (CSTR 75; CSTN 38.1,
     38.2 and 38.3 for the titles with CSTN 68 and 78, or 38.0 "You
     failed to achieve a rank"; CSTN 38 entry `min(ranks x 5 / 2 + 4,
     11)`).
   - "You accumulated the sum of %s gold pieces %sduring your career,
     making you %s" (CSTN 39.0: the fortune, "and %s acres of land "
     (39.1, x 50), entry `min((int)sqrt((land x 100 + fortune) / 500),
     6) + 2`).
   - "Your exploits on the high seas leave you with a reputation as %s
     captain" (CSTN 40.0, entry `clamp(reputation / 4 + 1, 0, 6) + 2`).
   - "You retired from privateering at the age of %s" (CSTR 76, years +
     25), the bride (CSTN 41.0, or 41.1 with 41.(married + 1)), "You
     succeeded in rescuing %s member%s of your family" (CSTR 77, CSTN
     79 as written, the `tolower` at VA `0x4ed8` dropped, "s" past
     one), "You were wounded %s" (CSTR 78, CSTN
     80: once, twice, numerous times), "In the course of your travels,
     you become %s" (CSTN 42, entry `clamp((int)(3 x h), 0, 5) + 1` with
     `h = max((relatives / 8 + married / 8 + 1 - injuries / 6) x (25 -
     years) / 20, -1)`, 2 for an expedition).
   - A famous expedition instead tells its share (CSTN 40.1, VA
     `0x4938`): the fortune, and CSTN 40 entry `min(trunc(fortune x 0.7
     / 40), 6) + 2` (in doubles).
5. **Verdict (VA `0x51c2`):** "Your pirating days are over. Your
   seafaring adventures have made you a %s in %s, where %s" (CSTR 82):
   CSTN 43 entry `r` (King's advisor... Beggar), the place (left over by
   the caller; the engine names the town retired in), and one of three
   of CSTN `44 + r` (`rand(3)`).
6. Unless the career was cut short or is an expedition: "The lure of
   the sea is strong... Remain in retirement / Return to pirating?" (CSTR
   72); returning goes on with the game.
7. "Do you wish to have your exploits recorded in the Pirate Hall of
   Fame?" (CSTR 73); "Yes" records the score and rating (the original's
   own file, VA `0x529a`, is not reproduced: the engine keeps
   `halloffame.txt`).
8. "Do you wish to play another game?" (CSTR 80), "Thank you and
   goodbye." (CSTR 81): the engine returns to the welcome menu.

## Sound effects (VA `0x7d82`)

The digitised effects of `DIGITAL.PIR`, by where the game plays them
(the FM effects below stand in for the ones marked when the driver is
missing):

| Effect | When |
|---|---|
| 0 | after a named captain's hail (CSTR 121) is answered (VA `0xd1fc`) |
| 1 | a broadside, the player's or the enemy's (sound request `0x1d`, VA `0x7db8`, `0x9551`) |
| 2 | a land battle begins (end of VA `0x169b7`) |
| 3 | the sword fight: the opponent's last man falls to the player's blow (`0x44b88 == 1`) |
| 4 | sailing into a harbour: the arrival on `arr_sea.pcx` (VA `0x18fa9`) |
| 5 | a sail sighted, before the question (VA `0xd1fc`) |
| 6 | the sword fight: blades meet (VA `0x2747c`) |
| 7 | the sword fight: the player's blow lands |
| 8 | the sword fight: the opponent's blow lands |

The sword fight's effect replaces its FM sound for that pass.

### FM effects (VA `0x1b8a2`)

Most of the game's sounds are FM effects: driver commands sent to the
music driver (request word VA `0x49a7e`, the argument unused by its
command entry, VA `cs:0x8f5`), played on its effect voices 6-8 over the
tune. They play with or without the digitised driver, except where a
digitised effect replaces them. Unless the sound is off (`0x4c0ec` 2 or
more).

- **Sea battle (VA `0x7db8`, sent by VA `0x9551`):** one request a tick,
  cleared at its start; the last written wins, in this order: the
  player's ship on the shallows (0x27, also running onto land), the
  player's broadside (0x1d), the player's shot landing (0x1e on the
  ship, 0x1f in the water), the hulls meeting (0x20, boarding), the
  enemy on the shallows (0x27, not a fort), the enemy's broadside
  (0x1d), the enemy's shot landing (0x1e, 0x1f). A broadside plays the
  digitised effect 1 instead. Sails, wind, reloading and scrolling are
  silent.
- **Sword fight (VA `0x2747c`):** one request a pass, cleared at its
  start, none on the first pass; the player's frame, then the
  opponent's, then the crew's every 16 passes: the player's strike 0x21,
  a miss 0x23, his blow landing 0x22 (digitised 7, or 3 when the
  opponent's last man falls), blades meeting 0x21 (digitised 6); the
  opponent's strike 0x22, a miss too, his blow landing 0x22 (digitised
  8); the opponent's crew losing men 0x24, the player's 0x25. A
  digitised effect set earlier in the pass plays instead of the FM
  sound, even when the crew's sound came last.
- **Land battle (VA `0x5a80`):** each fight at once: 0x25 for the
  player's units, 0x24 for the town's.
- **Trade boards (VA `0x20229`, `0x22c76`):** each unit that changes
  hands 0x29 (the merchant's, the loot's and the shipwright's guns); the
  merchant's board 0x2a when it opens (the first row, selling) and when
  the row or the way changes. Up and Down stop at the first and last
  rows.
- **Sailing:** striking a reef 0x20, before the master's word (VA
  `0x2849c`). Opening the cabin from the sea silences the music at once
  (command 0, VA `cs:0x1ad`: every channel keyed off, every operator at
  its quietest), not with the fade. Request 0x28 (exit code 6, VA
  `0x24507`) is never reached.
  - A player's blow that lands without felling the opponent's last man
    then goes through the clash test (VA `0x279c0`): within 36 pixels
    (the reach runs to 65: VA `0x389b8` 15, 10, 5, 0, 0 by level, plus
    `0x389bd` 50, 40, 30, 20 by weapon) it sounds as blades meeting,
    0x21 and digitised 6, the hit kept.

## Music

The engine plays the OPL3 driver `PSOUND.PIR` (Sound Blaster Pro 2 /
Pro Audio Spectrum class), the best of the game's FM arrangements and
in stereo; the Roland MT-32 one would need Roland's ROMs. Its scripts
run 60.9 times a second on an emulated YMF262 (ymfm, vendored); see "The
FM music engine" in asset-formats.md.

The game starts driver command n through VA `0x1b8cb` (unless the flag at
VA `0x4c0ec` mutes it) and stops the music through VA `0x1b8ee`
(command 1). A tune already playing goes on when asked again (the
title's handler checks, VA `0x82b`).

| Command | Where (VA) |
|---|---|
| 9 | the intro (`0x3022`), the welcome menu (`0x132f8`) |
| 10, 16, 22 | the lost treasure, a pirate's treasure, a relative found (`0xa801`) |
| 11 | dinner with the merchant's wife or daughter (`0x21e56`) |
| 12, 13 | meeting the governor's niece, or his daughter (town field 11 bit 0, `0x128c1`) |
| 14 | the bank (`0x180c8`) |
| 15, 23 | the barmaid while she talks, then the tavern again; 23 also the tavern and its bartender (`0x114d4`, `0x11867`) |
| 17, 18 | the governor's news; 18 with standing 2 or more with his nation (`0x11d64`) |
| 19 | a promotion (`0x12549`) |
| 20 | the merchant, the shipwright (`0x21e56`, `0x218c2`) |
| 21 | the loot of a capture or a sack (`0xdb7b`, `0x22764`) |
| 24 | a wedding (`0x128c1`, `0x21e56`) |
| 25 | a sail sighted (`0xd1fc`) |
| 26 | a villain's news (`0x18913`; empty in this driver) |
| 27 | the verdict at a career's end (`0x51c2`) |
| 28 | "Shall we keep her?" (`0xdb7b`) |
| 43 | prison, marooned (`0x16647`, `0x1f2a1`) |
| 44 | arriving in a town (`0x18fa9`) |
| stop | setting sail (`0x28789`), sword fights (`0x16205`), a sea battle's end (`0xe245`), leaving some screens |

- **Stopping** (command 1, VA `0x72d`; also the first step of every
  tune) marks the playing voices fading, their start the silent stream
  `ds:0xadd6`; each tick (VA `0x784`) a fading voice's volume falls a
  step every 4 ticks until -40, then it plays that stream and ends. A
  new tune takes free voices, else fading ones (VA `0x541`).
- **Percussion instruments** (byte `+0x38`: 28, 30, 34, 35, used by the
  stingers 29, 30, 32, 36, 37, 39) do not key a pitch: they switch the
  timer to divisor `0xf88` (VA `0x484`); each fast tick (VA `0x401`) gives
  such a voice the frequency `(random & mask) + base` (words `+0x3a`,
  `+0x3c`) with key-on, every fifth steps the music (VA `0x383`) and moves
  the base by the step (`+0x3e`) while the countdown lasts, then leaves
  it at the base. With none left the normal timer returns (the music
  runs 1.4% slower meanwhile).
- Commands 0-8 are controls (stop variants, a voices-playing query,
  pause), not effects.

## Open questions

None for the world update: the treasure fleet and silver train (VA
`0xcde9`) and the villains (VA `0xcba7`) are specified above.
