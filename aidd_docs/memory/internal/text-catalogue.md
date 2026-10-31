# Text catalogue

What each entry of the game's two string tables is for, so that writers
of an open pack can write new text for it without the original. The
format is in [asset-packs.md](asset-packs.md), section "Open text"; this
file only says what to write where.

Licensing: the open pack's text must be new writing. This catalogue
describes each entry's role in its own words and never quotes the
original; keep it that way when you update it.

## How to read it

- **Index:** the key in `text/<lang>/cstr.json` (`"strings"`) or
  `text/<lang>/cstn.json` (`"groups"`).
- **Where shown:** the screen, and the engine's name for the entry
  (`S_*`, `W_*` in `src/ui/screens.c`) when it has one.
- **`%s` arguments:** what the engine fills in, in order. An entry
  keeps this order; it may leave out trailing arguments, and `%1$s` to
  `%9$s` take any argument by its number.
- **Markup:** `\n` line break, `~` at a line start a menu option (the
  count and order are fixed where noted: the code gives each option its
  meaning), `` ` `` at a paragraph start a drop capital, `^` an
  apostrophe. Text boxes have a fixed width and wrap; very long text
  runs off the screen.
- **Not used by the engine:** nothing reads the entry. A pack may leave
  it out (it is then empty); the role is given for completeness.
  `tools/artgen/coverage.py` reads these markers (and "empty in the
  original") and the word counts, to report only what the engine reads:
  keep them in step with the code.
- Amounts written in the text (prices, debts) must match the rules: the
  notes give the constant to keep in step with.

## Strings: `cstr`

| Index | Where shown | What it says | `%s` arguments | Markup and constraints |
|---|---|---|---|---|
| 0 | story script (code 0) | Not used by the engine: byte 0 ends a story script, so this entry is never shown; empty in the original. | none | none |
| 1 | career story page (screen_story) | Opening page of one career background: an ambitious merchant captain seeks royal backing. | up to 4 available: treasure-fleet name (46), start town, year, player name; none used in the original | starts with a backtick drop capital; must fit the lower text box (x 40, y 340, width 580) |
| 2 | career story page (screen_story) | Pre-question page shared by several backgrounds: a patron offers to back the voyage. | same 4 optional story arguments; none used in the original | backtick drop capital; fits the lower text box |
| 3 | career story page (screen_story) | Page after the knowledge check is passed: the voyage sets out and reaches the Caribbean; also the second page of every famous expedition. | same 4 optional story arguments | backtick drop capital; fits the lower text box |
| 4 | career story page, failure branch | Not used by the engine: the failure branch after a failed knowledge check (support withdrawn, crew mutinies); the engine always passes the check. | none | none |
| 5 | career story page, failure branch | Not used by the engine: failure branch ending (player set adrift after losing the fight). | none | none |
| 6 | story script (code 6) | Not used by the engine: byte 6 is a story control code, never shown; a single space in the original. | none | none |
| 7 | career story page (screen_story) | Opening page of a background in which a rumour of lost treasure sparks the career. | same 4 optional story arguments | backtick drop capital; fits the lower text box |
| 8 | career story page (screen_story) | Pre-question page: the player looks for backers who test his knowledge of the region. | same 4 optional story arguments | backtick drop capital; fits the lower text box |
| 9 | career story page (screen_story) | Page after the check is passed: a colonial venture goes sour and leads into a duel with a rebel leader. | same 4 optional story arguments | backtick drop capital; must lead into the sword fight that follows |
| 10 | career story page (screen_story) | Page after the story duel is won: the player sets off on his career. | same 4 optional story arguments | backtick drop capital; fits the lower text box |
| 11 | career story page, failure branch | Not used by the engine: failure branch (backers withdraw, player joins another ship in a lower rank). | none | none |
| 12 | career story page, failure branch | Not used by the engine: failure branch (a duel against a cruel captain). | none | none |
| 13 | career story page (screen_story) | Opening page of a background of poor emigrants shipwrecked on the way to the New World. | same 4 optional story arguments | backtick drop capital; fits the lower text box |
| 14 | career story page (screen_story) | Pre-question page: forced labour on a plantation and a chance meeting with smugglers. | same 4 optional story arguments | backtick drop capital; fits the lower text box |
| 15 | career story page (screen_story) | Page after the check is passed: the player is made leader and leads a raid on a ship (sword fight follows). | same 4 optional story arguments | backtick drop capital; must lead into the sword fight |
| 16 | career story page, failure branch | Not used by the engine: failure branch (the player is taken along as a mere labourer). | none | none |
| 17 | career story page (screen_story) | Page after the story fight is won: the player reaches a friendly port and receives a privateering licence. | same 4 optional story arguments | backtick drop capital; fits the lower text box |
| 18 | career story page (screen_story) | Opening page of a background of indentured service to pay for the passage. | same 4 optional story arguments | backtick drop capital; fits the lower text box |
| 19 | career story page (screen_story) | Pre-question page: the player seeks to escape and approaches some sailors. | same 4 optional story arguments | backtick drop capital; fits the lower text box |
| 20 | career story page (screen_story) | Page after the check is passed: the sailors turn out to be buccaneers and the player duels for command. | same 4 optional story arguments | backtick drop capital; must lead into the sword fight |
| 21 | career story page (screen_story) | Opening page of a background where the player is arrested for a petty crime of need. | same 4 optional story arguments | backtick drop capital; fits the lower text box |
| 22 | career story page (screen_story) | Second page: the player is sentenced and shipped to the colonies. | same 4 optional story arguments | backtick drop capital; fits the lower text box |
| 23 | career story page (screen_story) | Page after the story fight is won: the ship is fitted out and a crew recruited in a friendly port. | same 4 optional story arguments | backtick drop capital; fits the lower text box |
| 24 | career story page (screen_story) | Opening page of a background where a colonial governor grants a coast-guard commission. | same 4 optional story arguments (a town could be passed as the 2nd) | backtick drop capital; fits the lower text box |
| 25 | career story page (screen_story) | Pre-question page: the governor continues and checks the player's knowledge. | same 4 optional story arguments | starts with a quote then backtick drop capital; fits the lower text box |
| 26 | career story page (screen_story) | Page after the check is passed: a first sea action in which the player boards an enemy ship (sword fight follows). | same 4 optional story arguments | backtick drop capital; must lead into the sword fight |
| 27 | career story page (screen_story) | Page after the story fight is won: the captured ship is refitted and crewed. | same 4 optional story arguments | backtick drop capital; fits the lower text box |
| 28 | career story page, failure branch | Not used by the engine: failure branch (a hopeless first battle). | none | none |
| 29 | career story page, failure branch | Not used by the engine: failure branch ending (the player starts out disgraced). | none | none |
| 30 | none | Not used by the engine and referenced by no story script: an unused opening page for a persecuted-captain background. | none | none |
| 31 | career story page (screen_story) | Opening page of a background, word for word the same as index 1 in the original. | same 4 optional story arguments | backtick drop capital; fits the lower text box |
| 32 | career story page (screen_story) | Opening page of a background where the family's lands were seized and the player searches for lost relatives. | same 4 optional story arguments | backtick drop capital; fits the lower text box |
| 33 | career story page (screen_story) | Pre-question page: sailors short of money invite the player to join them. | same 4 optional story arguments | backtick drop capital; fits the lower text box |
| 34 | career story page (screen_story) | Opening page of a background as a renowned merchant captain hired by a trading company. | same 4 optional story arguments | backtick drop capital; fits the lower text box |
| 35 | career story page (screen_story) | Page after the check is passed: a hostile ship boards the player's and its leader draws steel (sword fight follows). | same 4 optional story arguments | backtick drop capital; must lead into the sword fight |
| 36 | career story page (screen_story) | Opening page of a background of a youth from a family of sailors who turns to privateering. | same 4 optional story arguments | backtick drop capital; fits the lower text box |
| 37 | career story page (screen_story) | Pre-question page: a captain recruiting crew in a tavern. | same 4 optional story arguments | backtick drop capital; fits the lower text box |
| 38 | career story page (screen_story) | Opening page of a background raised among buccaneers after fleeing bondage. | same 4 optional story arguments | backtick drop capital; fits the lower text box |
| 39 | career story page (screen_story) | Opening page of a background as a ship owner hurt by restrictive trade laws. | same 4 optional story arguments | backtick drop capital; fits the lower text box |
| 40 | career story page (screen_story) | First page of every famous expedition: the player is put in charge of the expedition; index 3 follows. | same 4 optional story arguments | backtick drop capital; fits the lower text box |
| 41 | story script (code `)`) | Not used by the engine: this byte is the story's wait code, never shown; empty in the original. | none | none |
| 42 | story script (code `*`) | Not used by the engine: this byte is the story's knowledge-check code, never shown; empty in the original. | none | none |
| 43 | career story page (screen_story) | Page after the check is passed: a rival aboard challenges the player (sword fight follows). | same 4 optional story arguments | backtick drop capital; must lead into the sword fight |
| 44 | career story page (screen_story) | Page after landfall: a hostile ship boards the player's (sword fight follows). | same 4 optional story arguments | backtick drop capital; must lead into the sword fight |
| 45 | governor and hostage news (screen_governor, screen_hostage) | Name of the overland silver convoy, inserted into the "where is it" sentence (CSTN 27.0). | none | a short noun phrase, no line break |
| 46 | S_TREASURE_FLEET: story pages, governor and hostage news | Name of the treasure fleet, inserted into the "where is it" sentence and offered as the story's first argument. | none | a short noun phrase, no line break |
| 47 | none | Not used by the engine: a status word for travelling in disguise in the original. | none | none |
| 48 | S_A_FEW: screen_master | Vague distance word used when another nation's nearest town is close, inserted into S_CLOSEST. | none | short phrase, no line break |
| 49 | S_MANY: screen_master | Vague distance word used when that town is far, inserted into S_CLOSEST. | none | short phrase, no line break |
| 50 | story script (code `2`) | Not used by the engine: story control byte, never shown; a single space in the original. | none | none |
| 51 | story script (code `3`) | Not used by the engine: story control byte, never shown; a single space in the original. | none | none |
| 52 | story script (code `4`) | Not used by the engine: story control byte, never shown; a single space in the original. | none | none |
| 53 | story script (code `5`) | Not used by the engine: story control byte, never shown; a single space in the original. | none | none |
| 54 | story script (code `6`) | Not used by the engine: story control byte, never shown; a single space in the original. | none | none |
| 55 | story script (code `7`) | Not used by the engine: story control byte, never shown; a single space in the original. | none | none |
| 56 | none | Not used by the engine; a single space in the original (placeholder). | none | none |
| 57 | none | Not used by the engine; a single space in the original (placeholder). | none | none |
| 58 | none | Not used by the engine; a single space in the original (placeholder). | none | none |
| 59 | none | Not used by the engine; a single space in the original (placeholder). | none | none |
| 60 | none | Not used by the engine; a single space in the original (placeholder). | none | none |
| 61 | none | Not used by the engine: a character-set sample (the alphabet) in the original. | none | none |
| 62 | S_SELECT_EXPEDITION: expedition choice screen | Title asking the player to choose one of the famous expeditions. | none | one line, drawn centred in the title font |
| 63 | none | Not used by the engine; a single space in the original (placeholder). | none | none |
| 64 | S_SELECT_ERA: era choice screen | Title asking the player to choose a starting period. | none | one line, drawn centred at the top |
| 65 | S_NAME_PROMPT: name entry screen | Label asking the captain's name; the typed name is shown below it. | none | one short line, centred under the portrait |
| 66 | S_DIFFICULTY_PROMPT: difficulty screen | Label asking the player to pick a difficulty level. | none | one short line, centred |
| 67 | S_ABILITY_PROMPT: special ability screen | Prompt asking which special skill the captain has. | none | fits a 280-pixel-wide box |
| 68 | S_MASTER: screen_master, first page | The sailing master's welcome in the start town, suggesting the governor and a tavern. | start town name, its nation adjective (CSTN 71) | a speech: opens with a quote (the original has a drop capital and no opening quote, but a lead-in after a quote draws no capital); must end with one throwaway character (a line break) that the engine replaces with the quote closing the opening one (“”, „“, « », »«, else ") |
| 69 | S_CLOSEST: screen_master, second page | One line per other nation giving its nearest town, rough distance and direction; repeated and concatenated. | nation adjective (CSTN 71), town name, distance word (48 or 49), compass direction (CSTN 75) | each copy opens with a quote and ends with a line break, as a speech running over several paragraphs; the engine turns the very last character of the page into the quote closing the first copy's opening one |
| 70 | S_NATION_PROMPT: nationality screen | Prompt asking the player to choose a nationality. | none | fits a 280-pixel-wide box |
| 71 | none | Not used by the engine: a "continue" hint; the engine draws a picture prompt instead. | none | none |
| 72 | screen_ending, last page | Asks whether the old captain stays retired or goes back to sea. | none | backtick drop capital; exactly two `~` options: first stays retired, second returns to sea |
| 73 | S_HOF_QUESTION: screen_retire | Asks whether to record the career in the Hall of Fame. | none | backtick drop capital; exactly two `~` options: first records, second does not |
| 74 | screen_ending, first page | Heading page announcing the hero's later life, with the player's name. | player name | backtick drop capital; fits a 240-pixel-wide box |
| 75 | screen_ending, titles page | Sentence summing up the titles held and the resulting social standing. | lead-in word (CSTN 38.0 or 38.1), list of titles (empty if none), membership adjective (CSTN 38) | the second argument may be empty |
| 76 | screen_ending, life page | Sentence giving the age at retirement; further sentences are appended after a space. | age | none |
| 77 | screen_ending, life page | Sentence on how many relatives were rescued; only when at least one was. | number word (CSTN 79), plural suffix (CSTN 79.0, empty for one) | appended after a space |
| 78 | screen_ending, life page | Sentence on how often the captain was wounded; only when wounded. | count word (CSTN 80) | appended after a space |
| 79 | none | Not used by the engine: a score line out of 100 in the original. | none | none |
| 80 | none | Not used by the engine: a play-again question with two options in the original. | none | none |
| 81 | none | Not used by the engine: a farewell line on quitting in the original. | none | none |
| 82 | screen_ending, verdict page | The final verdict naming what the player became and where. | rank title by rating (CSTN 43), home town, closing phrase (CSTN 44 + rating, one of three) | backtick drop capital |
| 83 | none | Not used by the engine: a special ending where a king makes the player his adviser. | none | none |
| 84 | none | Not used by the engine: a special ending where a king grants the player a noble title. | none | none |
| 85 | S_CHOOSE_WEAPON: weapon choice before a sword fight | Prompt asking the player to choose a sword. | none | one short line, fits 160 pixels |
| 86 | S_SURE: screen_bank | Confirmation question built around an action phrase. | action phrase (95 or 96) | exactly two `~` options: first is yes, second is no |
| 87 | log book, Party Status (`S_HIDES`) | Name of the special cargo in the earliest period. | none | single word, no line break |
| 88 | log book, Party Status (`S_HIDES` + 1) | Name of the special cargo in the middle periods. | none | single word, no line break |
| 89 | log book, Party Status (`S_HIDES` + 2) | Name of the special cargo in the later periods. | none | single word, no line break |
| 90 | S_STANDING_LINE: log book, Personal Status | One line per nation with the player's rank or standing; four copies are concatenated. | nation adjective (CSTN 71), rank (CSTN 78) or standing word (CSTN 21) | should end with a line break |
| 91 | S_MARRIED: log book, Personal Status | Marker shown when the player is married (the single marker is index 285). | none | short, no line break |
| 92 | none | Not used by the engine: a heading for a list of ships in the original. | none | none |
| 93 | S_LOG_DATE (defined, not read) | Not used by the engine: a month-and-year date format; the engine builds the date itself. | none | none |
| 94 | S_NO_FUNDS: governor, map seller, traveller | Tells the player he cannot afford it. | none | shown alone with a continue prompt |
| 95 | S_DIVIDE: screen_bank | Action phrase for sharing out the plunder, inserted into S_SURE. | none | lowercase verb phrase, no punctuation |
| 96 | S_END_GAME: screen_bank | Action phrase for ending the career, inserted into S_SURE. | none | lowercase verb phrase, no punctuation |
| 97 | S_ADVANCE: screen_bank, plan menu | Third option offering to move up one difficulty level; only below the top level. | next difficulty title (CSTN 35) | must be a single `~` option on its own line; appended after S_PLAN's options |
| 98 | none | Not used by the engine: a one-line summary with five fields in the original. | none | none |
| 99 | none | Not used by the engine: a list joiner ("X and ") in the original. | none | none |
| 100 | none | Not used by the engine; empty in the original. | none | none |
| 101 | none | Not used by the engine; empty in the original. | none | none |
| 102 | none | Not used by the engine: a suffix forming a "-hunter" style title in the original. | none | none |
| 103 | none | Not used by the engine; a single space in the original (placeholder). | none | none |
| 104 | S_PARTY: log book, Party Status | The party status sheet, one labelled value per line. | leader name, crew count, guns, gold, food days, crew mood (CSTN 72), cargo tons, special cargo name (87-89), its tons | line breaks between fields; fits x 160 to 560 |
| 105 | S_PERSONAL: log book, Personal Status | The personal status sheet: standings, marital status, age, health, wealth, land, reputation. | four standing lines (from 90), marital marker (91 or 285), age, health word (CSTN 36), gold, acres, reputation word (CSTN 34), difficulty title (CSTN 35) | line breaks between fields; fits x 140 to 560 |
| 106 | none | Not used by the engine; a single space in the original (placeholder). | none | none |
| 107 | villain news (screen_vnews), returned by sim_villain_whereabouts | Says a villain left this town recently for another town (index 108 is used when he is still there). | villain name, destination town | shown alone with a continue prompt |
| 108 | villain news screen (`screen_vnews`, from `sim_villain_whereabouts`) | Answer when the asked-about villain is still in the town being discussed. | villain's name; town name | none |
| 109 | `S_DESERTED` (roll call on leaving town) | Reports how many crewmen ran off while in port. | number of deserters | drop capital |
| 110 | `S_ESCAPED` (second page of the roll call) | Reports that a held prisoner fled by jumping into the sea. | the prisoner captain's name | none |
| 111 | not used by the engine | Not used by the engine; in the original a merchant refused to deal with a pirate (the engine shows entry 256 instead). | none | none |
| 112 | `S_TOO_FEW` (fleet panel, abandon mode) | Prompt asking which ship to give up because the crew cannot man them all. | none | drawn as one centred line in a 415-pixel strip under the panel; keep it to one short line |
| 113 | trading board, jettison mode (literal 113) | Tells the player the hold is overfull and cargo must be dumped before continuing. | none | drop capital; shown at the top of the board up to 320 pixels wide |
| 114 | `S_ARRIVED` (arrival screen) | Arrival description of a town: its kind, name, defences, ruling nation, and any convoy present. | town-or-city word (CSTN 29); town name; forts phrase (may be empty); nation adjective; convoy line (may be empty) | drop capital; `\n` separates the parts; the third and fifth `%s` must start their own lines as they can be empty |
| 115 | `S_ARRIVE_MENU` (arrival screen) | Asks what to do at the town; first option is the filled label, then attack, sneak in, leave. | label of the first option (enter harbour or march in, CSTN 29) | exactly four `~` options in this order; first line is the question |
| 116 | `S_FORT_FIRES` (arrival screen) | The town's fort shoots at the approaching fleet. | none | entry 117 may be appended after a space |
| 117 | `S_SHIP_SUNK` (arrival screen) | Adds that the fort's fire sank one of the player's ships. | none | appended after entry 116 with a space; keep it a sentence fragment that follows on |
| 118 | `S_SAIL_HO` (sea encounter, first sighting) | Lookout reports a distant sail; asks whether to approach or keep sailing. | whose waters line (entry 119 filled) or empty | drop capital after the opening quote mark; exactly two `~` options (approach, continue) |
| 119 | `S_WATERS` (inside entry 118) | Navigator's remark about which nation's waters the fleet is in (easy levels only). | nation adjective or the "unknown" word (CSTN 71) | inserted into entry 118's `%s`; no options |
| 120 | `S_LOOKS_LIKE` (sea encounter, identified) | Names the type of the sighted ship and asks whether to close in or leave. | ship type with article (CSTN 11) | drop capital; exactly two `~` options (approach, leave) |
| 121 | `S_NAMED_CHASE` (sea encounter, hostile or hunted ship) | Alarm that a known captain is pursuing; asks to fight, hail, or flee. | repute word (CSTN 34); pirate or pirate-hunter word (CSTN 105); captain's name | drop capital after the quote mark; exactly three `~` options (fight, hail for news, flee) |
| 122 | `S_COLOURS` (sea encounter, flag seen) | Reports which flag the ship flies; asks to fight, hail, or flee. | nation colours word (CSTN 106) | drop capital; exactly three `~` options (fight, hail for news, flee) |
| 123 | not used by the engine | Not used by the engine; in the original it summed up men, guns and wind before picking a ship to command in battle (the engine uses entry 286). | none | none |
| 124 | `S_FOOD_LEFT` (crew screen) | Warning that provisions run low. | days of food left | none |
| 125 | `S_GOSSIP` (tavern rumours when no news exists) | The tavern host listens to the player's tale of a recent deed. | deed phrase (CSTN 33, may itself be filled with town/nation) | drop capital |
| 126 | `S_OVERTAKEN` (sea encounter result) | The other ship catches up, so battle cannot be avoided. | none | none |
| 127 | last ship lost screen (literal 127) | Too few men remain to sail even the last ship, so it is left behind. | none | none |
| 128 | not used by the engine | Not used by the engine; originally a prompt to choose ships to leave behind. | none | none |
| 129 | not used by the engine | Not used by the engine; originally the navigator saying more cargo must be left. | none | none |
| 130 | `S_NO_OFFICERS` (fleet panel, replace mode) | Prompt asking which ship to give up for lack of officers (fleet at its ship limit). | none | drawn as one centred line in a 415-pixel strip; keep it one short line |
| 131 | `S_VILLAIN_MET` (villain screen) | The player spots the villain fleeing a building and gives chase into a sword fight. | villain's nationality noun (CSTN 97); villain's name | drop capital |
| 132 | `S_VILLAIN_BEATEN` (villain screen) | The beaten villain reveals where the lost relative is held and hands over a map piece. | relative word (CSTN 37) | drop capital after the quote mark |
| 133 | `S_PIRATE_BROUGHT` (governor, mission report) | Governor thanks the player for capturing the wanted pirate. | the pirate's name | drop capital after the quote mark |
| 134 | `S_PROMOTION` (governor) | Governor grants the player a new title as a reward. | nation adjective; new rank title (CSTN 78) | drop capital |
| 135 | `S_LAND` (governor, after a promotion) | Governor adds a gift of land. | number of acres | drop capital after the quote mark |
| 136 | `S_RANSOM` (governor) | Governor relays a ransom offer for a prisoner; take it or wait. | prisoner's name; gold amount | drop capital; exactly two `~` options (accept, wait) |
| 137 | `S_VILLAIN_NEWS` (governor) | Governor has news of the lost relative and names the villain who knows where they are. | relative word; villain's nationality noun; villain's name; relative word again | drop capital after the quote mark; four `%s`, the first and last get the same word |
| 138 | `S_VILLAIN_SEEN` (governor) | Governor adds the town where that villain was last seen. | town name | drop capital after the quote mark |
| 139 | `S_MUTINY` (crew screen) | The crew rises against an unprofitable captain; a duel with the ringleader follows. | none | drop capital; drawn wide at the bottom (570 pixels) |
| 140 | `S_RESTLESS` (crew screen) | Navigator warns the crew will revolt without plunder soon. | none | drop capital |
| 141 | `S_DAUGHTER` (governor's family) | Introduces the governor's female relative, who has a suitor but likes the player; talk or propose. | daughter-or-niece word (CSTN 89); suitor's name | drop capital; exactly two `~` options (talk, propose) |
| 142 | governor's family, refusal (from `sim_daughter_propose`) | She rejects a proposal from a player of low standing. | difficulty-level adjective (CSTN 35) | drop capital after the quote mark |
| 143 | governor's family, gentle refusal (from `sim_daughter_propose`) | She declines for now, hinting the player must gain renown first. | none | drop capital after the quote mark |
| 144 | governor's family, acceptance (literal 144) | She accepts but warns the suitor will demand a duel; fight or withdraw. | rank title (CSTN 78); player's name | drop capital after the quote mark; exactly two `~` options (fight, withdraw) |
| 145 | `S_WEDDING` (governor) | The wedding takes place. | none | drop capital |
| 146 | `S_DAUGHTER_WON` (governor's family) | Charmed by conversation, she agrees to be the player's informant. | none | drop capital |
| 147 | `S_MARQUE` (governor offer) | Governor offers a privateering licence for a price. | nation adjective with article (CSTN 104); price in gold | drop capital; exactly two `~` options (yes, no) |
| 148 | `S_PARDON` (governor offer) | Governor offers forgiveness of past offences against the crown for a price. | nation adjective (CSTN 71); price in gold | drop capital after the quote mark; exactly two `~` options (yes, no) |
| 149 | `S_GOV_CHAT` (governor greeting) | Generic small talk, used when the nation has no allies or enemies to discuss. | none | drop capital |
| 150 | not used by the engine | Not used by the engine; originally a tavern visit where nothing occurs. | none | none |
| 151 | `S_RECRUITS` (tavern recruiting) | Volunteers recognise the player and offer to sign on. | repute word (CSTN 34); rank title (CSTN 78); player's name; number of men | drop capital after the quote mark; exactly two `~` options (hire, refuse) |
| 152 | `S_TRAVELLER` (tavern traveller) | A stranger offers news of a town for money. | town name; price in gold | drop capital; exactly two `~` options (yes, no) |
| 153 | `S_MAP_SELLER` (tavern map seller) | A shady seller offers a piece of a treasure map. | none | drop capital; exactly two `~` options (buy, refuse); the price written in the text must match the engine's fixed `MAP_PRICE` |
| 154 | villain news question (`screen_vnews`, literal 154) | A local source knows who has passed through the town; asks which villain to ask about. | source phrase including its verb (CSTN 16); town name; list of villain names | drop capital; the third `%s` must directly follow a `~` (names are joined with a line break and `~`, so text after it attaches to the last name only); one final `~` option for "nobody" |
| 155 | `S_SHARES` (division of plunder) | Each sailor's share and how the crew feels about it. | gold per man; crew mood word (CSTN 72) | drop capital |
| 156 | `S_PORTIONS` (division of plunder) | The captain's portions and total take. | difficulty-level word (CSTN 35); number of portions; gold total | drop capital |
| 157 | `S_PLAN` (after the division) | The crew goes ashore to spend; asks whether to sail again or retire. | none | drop capital; two `~` options (continue, retire); below the top level the engine appends entry 97 as a third option line |
| 158 | `S_REFIT` (after choosing to continue) | The ship is overhauled and resupplied, and new men join. | none | drop capital |
| 159 | `S_EXPEDITION_OVER` (division screen and prison end pages) | Closing text when a fixed-length expedition ends. | none | drop capital |
| 160 | `S_TOO_OLD` (division screen and prison end pages) | Closing text when age and wounds force the player to retire. | none | drop capital |
| 161 | `S_ASHORE` (prison, marooned) | With no ship left the player washes up on an island and is cared for. | none | drop capital |
| 162 | `S_PICKED_UP` (prison, marooned) | Old crewmates find the castaway and take them back as captain. | captain-grade word by difficulty (CSTN 73) | drop capital |
| 163 | `S_RECOGNIZED` (arrival, sneaking in) | A guard recognises the sneaking player; a duel follows. | none | drop capital; narrow box (200 pixels) |
| 164 | `S_PRIZE` (after a capture) | Describes the captured ship and the free hold space; keep her or plunder and scuttle. | always empty string; tonnage; ship type (CSTN 8); gun count; free tons in hold | drop capital; five `%s` (the first is always empty); exactly two `~` options (keep, sink) |
| 165 | `S_HOSTAGE_TALK` (prisoner screen) | A captured noble offers information; ask about the fleet, the train, or hold for ransom. | prisoner's name | drop capital after the quote mark; exactly three `~` options in that order |
| 166 | volunteers after a capture (literal 166) | Sailors of the beaten ship want to join; accept or refuse. | number of sailors | drop capital; exactly two `~` options (accept, refuse) |
| 167 | `S_YOU_SUNK` (battle end) | The player's ship goes down. | none | drop capital |
| 168 | `S_FOE_SUNK` (battle end) | The enemy ship explodes and is lost. | none | drop capital |
| 169 | `S_SUNSET` (battle end, ship) | Night falls and ends a ship battle. | none | drop capital; narrow box (240 pixels) |
| 170 | `S_STRUCK` (battle end) | The enemy surrenders. | none | drop capital |
| 171 | `S_BOARDING` (boarding, before the sword fight) | The ships collide and the opposing captain comes forward. | captain's name, or a generic enemy-captain word (CSTN 30) | none |
| 172 | not used by the engine | Not used by the engine; originally the pursuer seizing a slow ship of the fleeing player. | none | none |
| 173 | `S_BEACHED` (battle end) | The ship ran aground; the men refuse a land march. | none | drop capital |
| 174 | `S_DISGRACE` (after a lost or fled duel) | The player's honour and reputation suffer. | none | drop capital |
| 175 | `S_IMPRISONED` (prison) | The attack collapses; the wounded player is jailed. | town name, or the word for the ship (CSTN 77) | drop capital |
| 176 | `S_MONTHS_PASS` + 0 (prison) | Time passes in captivity; the player is still in good health. | none | drop capital |
| 177 | `S_MONTHS_PASS` + 1 (prison) | Time passes; the player's health begins to decline. | none | drop capital |
| 178 | `S_MONTHS_PASS` + 2 (prison) | Time passes; the player is often ill and thinks of retiring. | none | drop capital |
| 179 | `S_MONTHS_PASS` + 3 (prison) | Time passes; the player is old and worn, one last voyage at most. | none | drop capital |
| 180 | `S_STARVING` (crew screen, at sea) | Starving men desert by boat with part of the plunder. | none | drop capital |
| 181 | `S_STARVING` + 1 (crew screen, ashore) | Starving men desert on foot with part of the plunder. | none | drop capital |
| 182 | `S_WRECKED` (crew screen) | A ship strikes a reef and sinks. | none | drop capital |
| 183 | `S_CHEST` (digging) | Digging turns up a pirate chest of coins. | amount of gold | drop capital |
| 184 | `S_REUNION` (digging, relative's map) | The lost relative is found and has a story to tell. | relative word (CSTN 37); same word again | drop capital |
| 185 | `S_LOST_FOUND` (digging, lost treasure map) | The great lost treasure is found and loaded. | people/civilisation word (CSTN 93) | drop capital; any sum written must match `LOST_GOLD` in src/sim/treasure.c |
| 186 | `S_FLEET_TAKEN` (town sack) | The player captures the treasure fleet in port. | none | drop capital |
| 187 | `S_TRAIN_TAKEN` (town sack) | The player captures the silver train in town. | none | drop capital |
| 188 | `S_LOST_TALE` (digging, second page after the reunion) | The relative tells the legend of a sunken treasure and hands over a map piece. | people/civilisation word (CSTN 93) | drop capital after the quote mark |
| 189 | `S_NO_TREASURE` (digging) | Days of digging find nothing. | none | drop capital |
| 190 | `S_TRADED_BACK` (prison) | Former crewmates exchange the prisoner and take them back as captain. | captain-grade word by difficulty (CSTN 73) | drop capital |
| 191 | `S_ATTACK_FAILED` (land battle) | The land attack on the town has failed. | town name | drop capital |
| 192 | `S_SCALE_WALLS` (assault on a town with forts) | The men storm the fort; its commander duels the player. | none | drop capital; narrow box (230 pixels) |
| 193 | `S_TOWN_AT_MERCY` (town sack) | The town is taken and looting begins. | town name | drop capital |
| 194 | `S_INSTALL` (town sack) | The governor has fled; choose the nation of the new governor. | none | drop capital; exactly four `~` options in nation order Spain, England, France, Netherlands |
| 195 | `S_GOLD_HIDDEN` (town sack) | The town was warned and hid part of its gold. | fraction word (CSTN 15) | drop capital |
| 196 | `S_FORT_DRIVES_OFF` (battle end vs a fort) | The fort's fire forces the player back to sea. | none | drop capital |
| 197 | `S_YOU_ESCAPED` (battle end) | The player gets away out of range. | none | drop capital |
| 198 | `S_FOE_ESCAPED` (battle end) | The enemy ship gets away out of range. | none | drop capital |
| 199 | `S_MISSION_FAILED` (governor, mission report) | Governor voices disappointment at a failed mission. | none | drop capital after the quote mark |
| 200 | `S_MISSION_ASK` (governor) | Governor has an urgent task; accept or decline. | none | drop capital after the quote mark; exactly two `~` options (accept, decline) |
| 201 | `S_MISSION_TASK` + letter (governor) | Task: carry a letter to a relative of the governor in another town. | destination town | drop capital after the quote mark |
| 202 | `S_MISSION_TASK` + papers (governor) | Task: deliver secret papers to the crown's agent in another town. | destination town | drop capital after the quote mark |
| 203 | `S_MISSION_TASK` + pirate (governor) | Task: hunt down a named pirate near a town. | pirate's name; town name | drop capital after the quote mark |
| 204 | `S_MISSION_TASK` + son (governor) | Task: take an enemy town to free the governor's son held there. | destination town | drop capital after the quote mark |
| 205 | `S_MISSION_DONE` + letter (contact screen) | The letter is delivered and a reward promised. | the giving governor's town; pirate name (unused) | drop capital |
| 206 | `S_MISSION_DONE` + papers (contact screen) | The secret papers are delivered discreetly. | the giving governor's town; pirate name (unused) | drop capital |
| 207 | `S_MISSION_DONE` + pirate (contact screen, after the battle) | The wanted pirate is taken; the governor will be pleased. | the giving governor's town; the pirate's name | drop capital |
| 208 | `S_SON_FREED` / `S_MISSION_DONE` + son (town sack, contact screen) | The player releases the governor's son from prison. | the giving governor's town (a second argument, the pirate name, is passed but unused) | drop capital |
| 209 | `S_MISSION_THANKS` (governor, mission report) | Governor thanks the player for completing the mission. | none | drop capital after the quote mark |
| 210 | `S_ANXIOUS` (crew screen) | Navigator says the long voyage makes the men eager to divide the plunder. | none | drop capital |
| 211 | `S_ENTER_TOWN` (assault on a town without forts) | Armed townsfolk meet the landing party; their leader duels the player. | none | drop capital; narrow box (230 pixels) |
| 212 | not used by the engine | Not used by the engine; empty in the original. | none | none |
| 213 | not used by the engine | Not used by the engine; empty in the original. | none | none |
| 214 | not used by the engine | Not used by the engine; empty in the original. | none | none |
| 215 | not used by the engine | Not used by the engine; empty in the original. | none | none |
| 216 | none | Not used by the engine; empty in the original (unused slot). | none | none |
| 217 | none | Not used by the engine; empty in the original (unused slot). | none | none |
| 218 | none | Not used by the engine; empty in the original (unused slot). | none | none |
| 219 | none | Not used by the engine; empty in the original (unused slot). | none | none |
| 220 | none | Not used by the engine; empty in the original (unused slot). | none | none |
| 221 | none | Not used by the engine; empty in the original (unused slot). | none | none |
| 222 | none | Not used by the engine; empty in the original (unused slot). | none | none |
| 223 | none | Not used by the engine; empty in the original (unused slot). | none | none |
| 224 | none | Not used by the engine; empty in the original (unused slot). | none | none |
| 225 | none | Not used by the engine; empty in the original (unused slot). | none | none |
| 226 | none | Not used by the engine; empty in the original (unused slot). | none | none |
| 227 | none | Not used by the engine; empty in the original (unused slot). | none | none |
| 228 | none | Not used by the engine; empty in the original (unused slot). | none | none |
| 229 | none | Not used by the engine; empty in the original (unused slot). | none | none |
| 230 | none | Not used by the engine; empty in the original (unused slot). | none | none |
| 231 | none | Not used by the engine; empty in the original (unused slot). | none | none |
| 232 | none | Not used by the engine; empty in the original (unused slot). | none | none |
| 233 | none | Not used by the engine; empty in the original (unused slot). | none | none |
| 234 | none | Not used by the engine; empty in the original (unused slot). | none | none |
| 235 | none | Not used by the engine; empty in the original (unused slot). | none | none |
| 236 | none | Not used by the engine; empty in the original (unused slot). | none | none |
| 237 | none | Not used by the engine; empty in the original (unused slot). | none | none |
| 238 | none | Not used by the engine; empty in the original (unused slot). | none | none |
| 239 | none | Not used by the engine; empty in the original (unused slot). | none | none |
| 240 | none | Not used by the engine; empty in the original (unused slot). | none | none |
| 241 | none | Not used by the engine; empty in the original (unused slot). | none | none |
| 242 | none | Not used by the engine; empty in the original (unused slot). | none | none |
| 243 | none | Not used by the engine; empty in the original (unused slot). | none | none |
| 244 | none | Not used by the engine; empty in the original (unused slot). | none | none |
| 245 | none | Not used by the engine; empty in the original (unused slot). | none | none |
| 246 | none | Not used by the engine; empty in the original (unused slot). | none | none |
| 247 | none | Not used by the engine; empty in the original (unused slot). | none | none |
| 248 | none | Not used by the engine; empty in the original (unused slot). | none | none |
| 249 | none | Not used by the engine; empty in the original (unused slot). | none | none |
| 250 | none | Not used by the engine; empty in the original (unused slot). | none | none |
| 251 | none | Not used by the engine; empty in the original (unused slot). | none | none |
| 252 | none | Not used by the engine; empty in the original (unused slot). | none | none |
| 253 | none | Not used by the engine; empty in the original (unused slot). | none | none |
| 254 | none | Not used by the engine; empty in the original (unused slot). | none | none |
| 255 | none | Not used by the engine; in the original a single blank placeholder entry. | none | none |
| 256 | `S_NO_PIRATES` (merchant greeting stage 2, shipwright on entry) | A trader refuses all business because the player is considered a pirate. | none | Shared by the merchant and the shipwright; shown alone with a continue prompt; drop capital usual. |
| 257 | merchant greeting (`sim_merchant_greeting`, `GREET_POOR` + 0) | Merchant's cool welcome to a player with a very light purse (2000 gold or less). | none | First of four consecutive greetings 257-260 chosen by purse size; shown with a continue prompt. |
| 258 | merchant greeting (`GREET_POOR` + 1) | Merchant's reluctant, formal welcome to a modestly funded player (up to 5000 gold). | none | Second of the 257-260 purse ladder. |
| 259 | merchant greeting (`GREET_POOR` + 2) | Merchant's eager welcome to a well-funded player (up to 20000 gold). | none | Third of the 257-260 purse ladder. |
| 260 | merchant greeting (`GREET_POOR` + 3) | Merchant's effusive welcome to a very rich player (over 20000 gold). | none | Last of the 257-260 purse ladder; text box spans x 160-480, keep it to a short paragraph. |
| 261 | `S_ERA_TEXT` + 0, era selection screen | Short assessment of the first (earliest) historical period: balance of powers and difficulty. | none | One of six consecutive era blurbs 261-266, in era order; drawn in the small font under the era name with only about 70 px per era, so two short lines at most. |
| 262 | `S_ERA_TEXT` + 1, era selection screen | Short assessment of the second period: powers and difficulty. | none | Same constraints as 261. |
| 263 | `S_ERA_TEXT` + 2, era selection screen | Short assessment of the third period: powers and difficulty. | none | Same constraints as 261. |
| 264 | `S_ERA_TEXT` + 3, era selection screen | Short assessment of the fourth period: powers and difficulty. | none | Same constraints as 261. |
| 265 | `S_ERA_TEXT` + 4, era selection screen | Short assessment of the fifth period: powers and difficulty. | none | Same constraints as 261. |
| 266 | `S_ERA_TEXT` + 5, era selection screen | Short assessment of the sixth (latest) period: powers and difficulty. | none | Same constraints as 261. |
| 267 | `S_EXPEDITION_TEXT` + 0, famous expedition screen | Describes the first famous expedition: who the player plays, the starting force and its difficulty. | none | One of six consecutive blurbs 267-272 matching the `W_EXPEDITION` options; only the highlighted one is shown, box at (80, 300) 480 px wide. |
| 268 | `S_EXPEDITION_TEXT` + 1, famous expedition screen | Describes the second famous expedition and its difficulty. | none | Same constraints as 267. |
| 269 | `S_EXPEDITION_TEXT` + 2, famous expedition screen | Describes the third famous expedition and its difficulty. | none | Same constraints as 267. |
| 270 | `S_EXPEDITION_TEXT` + 3, famous expedition screen | Describes the fourth famous expedition and its difficulty. | none | Same constraints as 267. |
| 271 | `S_EXPEDITION_TEXT` + 4, famous expedition screen | Describes the fifth famous expedition and its difficulty. | none | Same constraints as 267. |
| 272 | `S_EXPEDITION_TEXT` + 5, famous expedition screen | Describes the sixth famous expedition and its difficulty. | none | Same constraints as 267. |
| 273 | `S_SATISFIED`, character confirmation screen | Asks whether the player is happy with the choices made for the new captain. | none | Plain prompt, no options in the string; box at (180, 316) 280 px wide. |
| 274 | `S_YES`, character confirmation screen | Affirmative answer button. | none | Single short word drawn alone in a 100 px slot. |
| 275 | `S_NAY`, character confirmation screen | Negative answer button (goes back to edit). | none | Single short word drawn alone in a 100 px slot. |
| 276 | none | Not used by the engine; in the original asked the player to name the flagship. | none | none |
| 277 | `S_WRIGHT_MENU`, shipwright main menu | Shipwright's greeting followed by his menu of services. | none | Exactly four `~` options in this order: leave, repair, sell a ship, sell cannon (the code maps choices 0-3). |
| 278 | none | Not used by the engine; in the original the shipwright said the player had nothing worth trading. | none | none |
| 279 | `S_ONLY_SHIP`, shipwright note | Shipwright declines to buy the player's last remaining ship. | none | Shown alone with a continue prompt, box 440 px wide. |
| 280 | none | Not used by the engine; in the original a short stamp confirming a ship sale. | none | none |
| 281 | none | Not used by the engine; in the original a short stamp confirming a repair. | none | none |
| 282 | `S_NOTHING_WRONG`, shipwright note | Shipwright finds no damage to repair on any ship. | none | Shown alone with a continue prompt, box 440 px wide. |
| 283 | `S_CANT_AFFORD`, shipwright note | Player lacks the gold to pay for the chosen repair. | none | Shown alone with a continue prompt, box 440 px wide. |
| 284 | `S_ONLY_CANNON`, shipwright note | Shipwright refuses to buy cannon when the player has fewer than two. | none | Shown alone with a continue prompt, box 440 px wide. |
| 285 | `S_SINGLE`, ship's log personal status | Word or short phrase for an unmarried captain's family status. | none | Substituted as the second `%s` argument of the personal status text (CSTR 105); keep it short, no markup. |
| 286 | `S_COMMAND`, fleet panel | Prompt asking which ship the player will take as flagship. | none | Single line centred in a 415 px wide slot; no markup. |
| 287 | none | Not used by the engine; in the original a progress banner while saving. | none | none |
| 288 | none | Not used by the engine; in the original a disk read/write error message. | none | none |
| 289 | none | Not used by the engine; in the original said no saved games were found. | none | none |
| 290 | `S_SAVE_GAME`, save slot screen | Title of the screen for saving a game. | none | Single centred title line in the title font; no markup. |
| 291 | merchant greeting (`GREET_FAMILY` + 0), father-in-law's shop | Father-in-law's disdainful welcome to a poor son-in-law. | none | First of four consecutive family greetings 291-294 by purse size, same thresholds as 257-260; shown with a continue prompt. |
| 292 | merchant greeting (`GREET_FAMILY` + 1) | Father-in-law's impatient welcome to a modestly funded son-in-law. | none | Second of the 291-294 ladder. |
| 293 | merchant greeting (`GREET_FAMILY` + 2) | Father-in-law's warm, approving welcome to a well-funded son-in-law. | none | Third of the 291-294 ladder. |
| 294 | merchant greeting (`GREET_FAMILY` + 3) | Father-in-law's delighted welcome to a very rich son-in-law. | none | Last of the 291-294 ladder; box spans x 160-480, keep it to a short paragraph. |
| 295 | merchant's daughter screen (`g->merdaugh` = 295) | After trading in the wife's home town, the player dines with his wife. | none | Shown alone with a continue prompt, box at (320, 150) 300 px wide. |
| 296 | merchant's daughter screen (`g->merdaugh` = 296) | The merchant introduces his daughter and the player picks how to respond. | none | Exactly two `~` options in this order: just chat (ends the scene), propose marriage. |
| 297 | merchant's daughter screen (from `sim_merchant_propose`) | The daughter turns the proposal down because the player is not yet rich enough. | none | Shown alone with a continue prompt, box at (320, 150) 300 px wide. |
| 298 | merchant's daughter screen (from `sim_merchant_propose`) | The daughter accepts in principle but explains her father's debt, and the player decides. | none | Exactly two `~` options: settle the father's debt, or decline; drawn higher at (320, 50), so it may be long; the code charges 500 gold, so any amount named should match. |
| 299 | merchant's daughter screen (from `sim_merchant_pay_debt`) | The wedding takes place and the merchant promises the player better prices. | none | Shown on the wedding picture at (360, 150) 260 px wide with a continue prompt. |
| 300 | merchant's daughter screen (from `sim_merchant_pay_debt`) | The merchant scorns the offer because the player cannot cover the debt. | none | Shown on the merchant picture at (160, 40) 320 px wide with a continue prompt. |
| 301 | `S_DRINK`, barmaid screen | Barmaid offers the player a drink. | none | Exactly two `~` options: accept (first), decline (second). |
| 302 | `S_LOOKING_FOR`, barmaid screen | Barmaid asks what the player wants, with three possible answers. | none | Exactly three `~` options in order: nothing, a drink, a drink and company (index matters to `sim_barmaid`). |
| 303 | `S_EVENING`, barmaid screen | The player spends a pleasant but uneventful night out with the barmaid, nothing more. | none | Shown alone with a continue prompt, box at (40, 150) 280 px wide. |
| 304 | `S_HEART`, barmaid screen | The barmaid becomes fond of the player and offers to collect news on later visits. | none | Shown alone with a continue prompt. |
| 305 | `S_NOTHING_TO_SAY`, barmaid screen | The barmaid informant has no news this time. | none | Shown alone with a continue prompt. |
| 306 | `S_INFORMANT`, barmaid screen | The barmaid informant greets the player and is about to share what she learned. | none | Shown alone with a continue prompt; leads into 307 or 308. |
| 307 | `S_GOSSIP_ON`, barmaid screen | The informant says she has fresh news about a town; that town's information screen follows. | a town name | Exactly one `%s`; shown with a continue prompt. |
| 308 | `S_MAP_PIECE`, barmaid screen | The informant hands over a piece of a treasure map; the map is shown next. | none | Shown alone with a continue prompt. |
| 309 | `S_NO_RECRUITS`, tavern recruiting | No sailors in the tavern are willing to sign on. | none | Shown alone with a continue prompt, box at (60, 300) 560 px wide. |
| 310 | none | Not used by the engine; in the original a copy-protection question asking to identify a pirate by his flag, with six name options. | none | none (the quiz is deliberately not implemented) |
| 311 | none | Not used by the engine; in the original asked how to react to a pursuing pirate, with three options. | none | none |
| 312 | `S_LOG_MENU`, ship's log menu | Menu of the captain's log sections. | none | Exactly six `~` options in order: ships met, the party, the player, the fleet, prisoners held, leave the menu; no other text. |
| 313 | `S_CANNON_PRICE`, cannon selling board | States the fixed price the shipwright pays per cannon. | none | One short line at (150, 20) 470 px wide; the code uses 50 gold each, so any number named should match. |
| 314 | none | Not used by the engine; in the original a developer debug readout of map coordinates. | none | none |
| 315 | `S_UNANSWERED`, encounter result | The player's hail is ignored and a fight begins. | none | Shown under the encounter text at x 200, 340 px wide. |
| 316 | none | Not used by the engine; in the original a developer debug readout of free memory. | none | none |
| 317 | `S_FORT_SUNSET`, sea battle end against a fort | Nightfall ends a battle against a town's fort. | none | Plain message, no options. |
| 318 | `S_LANG_MENU`, the cabin's language menu (key L) | Prompt to choose the interface language. | none | `~` options in order: English, French, German, then Spanish (the open pack's); the engine offers as many as there are, up to four. |
| 319 | `S_SHORT_FUNDS`, shipwright note | The shipwright runs out of money to buy everything the player offered. | none | Shown alone with a continue prompt, box 440 px wide. |
| 320 | `S_LOAD_GAME`, save slot screen | Title of the screen for loading a saved game. | none | Single centred title line in the title font; no markup. |
| 321 | none | Not used by the engine; empty in the original. | none | none |

## Word lists: `cstn`

Each group is a list; the engine picks a word by its position, so the
order and the count matter.

| Group | Words | Where shown | What the words are, and what picks one | `%s` arguments |
|---|---|---|---|---|
| 0 | 1 | not used by the engine | a menu heading line for the treasure maps (with `~` and a line break) in the original | none |
| 1 | 1 | not used by the engine | a blank placeholder (one space) | none |
| 2 | 16 | captain and villain names: ship's log, hostages, encounters, governor, tavern (`captain_name()`, `villain_name()`) | 16 surnames for Spanish captains; captains use words 0-7 (id >> 3 & 7), villains any of the 16; the group is 2 + nation | none |
| 3 | 16 | as group 2 | 16 surnames for English captains (2 + nation) | none |
| 4 | 16 | as group 2 | 16 surnames for French captains (2 + nation) | none |
| 5 | 16 | as group 2 | 16 surnames for Dutch captains (2 + nation) | none |
| 6 | 12 | ship's log and news lines, before the day (W_MONTH_SHORT) | the 12 abbreviated month names, January first; indexed by (date % 360) / 30 | none |
| 7 | 12 | the date on the sailing screen and log book (`date_text()`, W_MONTH) | the 12 full month names, January first; indexed by day_of_year / 30 | none |
| 8 | 11 | battle panel, prize text, shipwright, fleet panel, log and gossip (W_SHIP_TYPE) | 0-8 the nine ship types in the engine's type order (`type` in src/sim/sim.h: 0 pinnace .. 8 fast galleon), 9 the fort (FORT_TYPE), 10 a suffix appended to a damaged ship in the shipwright's list; capitalised as a name | none |
| 9 | 4 | city information and the Caribbean map's town field (W_WEALTH) | four town wealth levels, poorest first; indexed by (town wealth / 64) & 3 | none |
| 10 | 12 | plunder board, after the gold amount (W_PLUNDERED) | only word 6 is used: the suffix after the number of gold pieces looted; the others are trade-board labels of the original (price, cargo kinds, units), word 7 empty | none |
| 11 | 9 | encounter, the ship-type line (CSTR 120) (W_A_SHIP) | the nine ship types with an indefinite article, lower case, same order as group 8; indexed by the encounter's ship type | none |
| 12 | 3 | battle screen, wind box (W_WIND) | three wind strengths, weakest first; indexed by the battle's wind_strength (0-2) | none |
| 13 | 16 | sailing screen status line, inside group 30 word 9 (W_COMPASS) | the 16 compass points clockwise starting from south; indexed by wind >> 4 | none |
| 14 | 5 | fleet panel, under each ship (W_DAMAGE) | five damage levels: 0 blank (sound), then worse to sinking; indexed by `panel_level()` of the ship's damage | none |
| 15 | 3 | sacking a town, "gold hidden" text (CSTR 195) (W_HIDDEN) | three quantity words, smallest first, for how much of the town's gold was hidden; indexed by sack.hidden (0-2) | none |
| 16 | 4 | villain news text (CSTR 154, arg 0) | four sources of the rumour (townsfolk, the bartender, a traveller, crewmen), each with its verb; indexed by vnews_source (0-3) | none |
| 17 | 5 | not used by the engine | five labels for dividing or leaving parts of the crew and party in the original | none |
| 18 | 4 | ship's news (encounter hail) and the bartender's rumours (W_NEWS) | 0 empty; 1 the bartender's opening line (drop capital) before the news; 2 the ship's-news title; 3 the "no entries" line | none |
| 19 | 4 | not used by the engine | four cabin menu items (search, save, divide, join) in the original; the engine uses group 86 | none |
| 20 | 37 | ship's log book page, ship's news and bartender's rumours (`news_text()`, `news_body()`, W_LOG) | 0 the dated line format; 1-36 one sentence per news code (`enum news_code` in src/sim/sim.h); see "Group 20" below | see "Group 20" |
| 21 | 4 | log book, personal status, per nation (W_STANDING) | how a nation sees a player who holds no rank there: 0 privateer licence (standing >= 2), 1 no title (>= 0), 2 wary (>= -3), 3 hostile | none |
| 22 | 4 | log book, hostages page (W_HOSTAGE) | 1 a captured pirate-hunter line, 2 a captured pirate line (also the pirate a mission hunts); 0 and 3 are section headings, unused | words 1, 2: the captain's name |
| 23 | 5 | city information screen (`city_text()`, W_CITY_INFO) | 0 the full description; 1 a town with no information; 2 the forts phrase; 3 the gold phrase; 4 information outdated (town flag 0x20) | 0: town name, nation adjective (71), wealth (9), forts phrase (2), soldiers, citizens, gold phrase (3); 1, 4: town name; 2: number word (79), plural suffix (79.0 or empty); 3: gold in thousands |
| 24 | 3 | not used by the engine | title, prompt and exit label of the original's city-information picker | none |
| 25 | 9 | governor's greeting (`governor_greeting()`, `nation_list()`, W_GOV) | pieces of the political-situation greeting: 0 salutation, 1 allies lead-in, 2 enemies lead-in, 3-5 lists of one, two or three countries, 6 closing order to fight the enemies (when at war and standing >= 2), 7 the "and" put before the enemies when there are allies, 8 plain closing | 0: rank (78), player name; 2: word 7 or empty; 3-5: one to three country names (81) |
| 26 | 3 | governor, the hostage/informant stage (GOV_INFORM, W_INFORM) | 0 after-dinner sentence about who gives the news; 1 an informant; 2 the wife; 1 or 2 chosen by the governor's GOV_WIFE flag | 0: word 1 or 2 |
| 27 | 3 | governor, convoy information (fleet or train) | 0 "the convoy is in a place" sentence; 1 fallback place for the treasure fleet, 2 fallback place for the silver train, when no town is known | 0: convoy name (CSTR 46 or 45), town name or word 1/2 |
| 28 | 7 | shipwright, sell and repair lists (`wright_list()`, W_WRIGHT) | 0 sell title, 1 repair title, 2 the decline option, 3 a ship line with price, 4 a ship line with no damage; 5, 6 (done confirmations) unused | 3: ship type (8), damaged suffix (8.10 or empty), price; 4: ship type (8) |
| 29 | 10 | arrival at a town (W_ARRIVAL), into CSTR S_ARRIVED and S_ARRIVE_MENU | 0 one fort (harbour), 1 several forts (harbour), 2 treasure fleet present, 3 silver train present, 4 sail-in option, 5 march-in option (party ashore), 6 inland-village noun, 7 seaside-town noun, 8 one fort (inland), 9 several forts (inland) | 1, 9: number word (79) |
| 30 | 13 | sailing status line, battle panel, sword fight (W_SAILING) | 0 reloading, 1 loaded, 2 battle sails, 3 full sails, 4 raising sail, 5 lowering sail, 7 "the enemy captain", 8 the wind on the sailing status line, 10 guns label, 11 men label, 12 speed label; 6 (no damage) and 9 unused | 8: compass point the wind blows from (13, an abbreviation such as SW) |
| 31 | 5 | treasure map screen title and footer (W_MAP_TITLE) | indexed by map kind (`enum { MAP_PIRATE, MAP_RELATIVE, MAP_LOST }` in sim.h): 0 pirate map, 1 map to a lost relative, 2 map to a lost treasure; 3 missing piece (unused); 4 "somewhere near" footer | 1: relative (37); 4: town name |
| 32 | 10 | land battle side panel (W_LAND) | 0-4 terrain names indexed by the ground type under a unit (4 empty); 5 men/muskets column header (unused); 6 the player's party label; 7 the defenders label; 8 terrain label; 9 morale label | none |
| 33 | 27 | bartender's gossip when no public news, inside CSTR 125 (`gossip_text()`, W_GOSSIP) | one phrase per news code of the player's latest deed; 9-21 are empty (skipped codes); see "Group 33" below | see "Group 33" |
| 34 | 6 | log book personal status, tavern recruiting, chasing captain (W_REPUTATION) | six reputation levels, lowest first; indexed by reputation / 4 + 1 clamped 0-5; a chasing captain uses 1 (named) or (captain & 7) / 3 | none |
| 35 | 5 | difficulty screen, career summary, log book, bank, governor (W_DIFFICULTY) | the four difficulty levels, easiest first, indexed by difficulty 0-3; word 4 repeats word 0 and is never reached | none |
| 36 | 4 | log book personal status (W_HEALTH) | four health levels, best first; indexed by `sim_health()` | none |
| 37 | 4 | lost-relative quest: map title, dig, villain texts, log (W_RELATIVE) | the four kinds of lost relative (sister, father, niece, uncle) in sim.relative order; indexed by relative & 3 | none |
| 38 | 12 | end of career, titles page (`ending_titles()`, into CSTR 75) | 0 no title earned, 1 titles earned lead-in, 2 the first title, 3 each further title, 4-11 eight standings in society, lowest first, indexed by end.member | 2, 3: nation with article (68), rank (78) |
| 39 | 9 | end of career, wealth page | 0 fortune sentence, 1 land clause, 2-8 seven wealth levels, poorest first, indexed by end.wealth | 0: fortune, land clause (word 1 or empty), wealth word; 1: acres |
| 40 | 9 | end of career, reputation page | 0 reputation sentence, 1 the expedition-share sentence (famous expedition), 2-8 seven reputation levels, lowest first, indexed by end.repute | 0: repute word; 1: gold, repute word |
| 41 | 6 | end of career, life page (`ending_life()`) | 0 never married, 1 the bride sentence, 2-5 who the bride is, indexed by married + 1 (married 1-3 => 2-4; word 5 unreached) | 1: bride word |
| 42 | 7 | end of career, life page | 0 the mood sentence, 1-6 six moods from bitter to joyful, indexed by end.mood | 0: mood word |
| 43 | 24 | end of career, verdict (into CSTR 82, arg 0) | 24 retirement occupations, highest first, indexed by the career rating (`hof_rating()`, 0-21); words 22-23 never reached | none |
| 44 | 3 | end of career, verdict (CSTR 82, arg 2) | three alternative closing phrases for rating 0 (group = 44 + rating); one picked at random (end_phrase 0-2) | none |
| 45 | 3 | as group 44 | three phrases for rating 1 | none |
| 46 | 3 | as group 44 | three phrases for rating 2 | none |
| 47 | 3 | as group 44 | three phrases for rating 3 | none |
| 48 | 3 | as group 44 | three phrases for rating 4 | none |
| 49 | 3 | as group 44 | three phrases for rating 5 | none |
| 50 | 3 | as group 44 | three phrases for rating 6 | none |
| 51 | 3 | as group 44 | three phrases for rating 7 | none |
| 52 | 3 | as group 44 | three phrases for rating 8 | none |
| 53 | 3 | as group 44 | three phrases for rating 9 | none |
| 54 | 3 | as group 44 | three phrases for rating 10 | none |
| 55 | 3 | as group 44 | three phrases for rating 11 | none |
| 56 | 3 | as group 44 | three phrases for rating 12 | none |
| 57 | 3 | as group 44 | three phrases for rating 13 | none |
| 58 | 3 | as group 44 | three phrases for rating 14 | none |
| 59 | 3 | as group 44 | three phrases for rating 15 | none |
| 60 | 3 | as group 44 | three phrases for rating 16 | none |
| 61 | 3 | as group 44 | three phrases for rating 17 | none |
| 62 | 3 | as group 44 | three phrases for rating 18 | none |
| 63 | 3 | as group 44 | three phrases for rating 19 | none |
| 64 | 3 | as group 44 | three phrases for rating 20 | none |
| 65 | 3 | as group 44 | three phrases for rating 21 (the worst reached) | none |
| 66 | 3 | as group 44, never reached | three phrases for rating 22 (occupation 43.22); the rating stops at 21 | none |
| 67 | 3 | as group 44, never reached | three phrases for rating 23 (occupation 43.23) | none |
| 68 | 5 | log and gossip (titles, ships), end-of-career titles | nation adjective with its indefinite article, lower case: Spain, England, France, Holland, then pirate; indexed by nation (from news a / 64 or a / 16, % 5) | none |
| 69 | 6 | land battle panel, morale value (W_MORALE) | six morale levels, lowest first; indexed by morale >> 4 clamped to 5 | none |
| 70 | 3 | sword fight, weapon choice (W_WEAPON) | the three weapons in the choice screen's order (light, long, heavy), indexed by the selection | none |
| 71 | 6 | many: identity screen, waters, master's directions, city info, log book, sword fight, governor, land battle (W_NATION) | nation adjective, capitalised as used alone: Spain, England, France, Holland, then 4 pirate, 5 unknown; indexed by nation | none |
| 72 | 4 | log book party page, bank shares (W_MOOD) | four crew moods, worst first; indexed by mood / 4 + 1 clamped 0-3, or plunder.mood | none |
| 73 | 5 | prison: picked up / traded back (W_CAPTAIN) | the four difficulty levels as a noun with indefinite article, by difficulty; word 4 repeats word 0, unreached | none |
| 74 | 6 (+1) | sword fight, the opponent's label (W_TITLE) | six fighter roles (guard, suitor, mutineer, enemy captain, captain, first mate), indexed by the fight's title; in crew fights joined with a nation (71); an optional seventh word, a pack's own, is the label's pattern | 6: nation (71), title (74), reorderable with `%1$s` `%2$s` |
| 75 | 9 | ship's master's directions (CSTR 69, arg 3) (W_DIRECTION) | 0 north, 1 south, 2 east, 3 west, 4 north-west, 5 north-east, 6 south-west, 7 south-east, 8 same place (placeholder); computed from the town offset | none |
| 76 | 12 | not used by the engine | the 12 month names in quotes, January first (original's date entry) | none |
| 77 | 6 | prison text (W_THE_SHIP) | only word 4 is used: "the ship" as the place of captivity when there is no town; the others are the original's sword-fight header pieces | word 0: unused |
| 78 | 10 | everywhere a rank shows: names, governor, log, tavern, end (W_RANK) | ten ranks, lowest first, starting with a plain courtesy title; indexed by sim.rank[nation] (0-9); captains use (id & 7) + 2 | none |
| 79 | 6 | city info, arrival forts, end-of-career relatives (W_NUMBER) | 0 the plural suffix, 1-5 the number words one to five | none |
| 80 | 3 | end of career, injuries (CSTR 78) | how often wounded: once, twice, many times; indexed by injuries - 1 clamped to 2 | none |
| 81 | 4 | log (war, peace, alliance, capture, amnesty), governor's lists (W_NATION_NAME, W_COUNTRY) | the four country names: Spain, England, France, Holland; indexed by nation & 3 | none |
| 82 | 4 | identity screen, flag hover label (W_COLORS) | the four nations' flags, in the flag-slot order England, France, Holland, Spain (`flag_nation[]`); indexed by the hovered slot | none |
| 83 | 5 | identity screen, career summary (W_ABILITY) | the five special abilities in career.ability order: fencing, charm, navigation, gunnery, medicine | none |
| 84 | 8 | town screen, spot labels on the scroll (W_TOWN_SPOT) | 0-6 the town's spots (governor, merchant, bank, tavern, shipwright, city information, leave), 7 the captain's cabin; indexed by town_spot | none |
| 85 | 1 | not used by the engine | a blank placeholder (one space) | none |
| 86 | 11 | captain's cabin labels (W_CABIN); word 9 also the welcome screen's quit option | the cabin's 11 spots in `enum { CABIN_CONTINUE .. CABIN_PRACTICE }` order: continue, maps, cave, log, sea chart, search, divide, join, save, exit, fencing practice | none |
| 87 | 5 | fleet panel button (W_PANEL_BUTTON) | 0 blank button, 1 the "select" button; 2-4 (repair, sell, buy) unused | none |
| 88 | 1 | not used by the engine | a blank placeholder (one space) | none |
| 89 | 2 | governor's daughter scene (CSTR 141) (W_DAUGHTER) | 0 daughter, 1 niece; indexed by daughter.niece | none |
| 90 | 10 | tavern, spot labels on the scroll (W_TAVERN_SPOT) | the ten tavern spots: 0 map seller, 1 information seller, 2 sailors to recruit, 3 bartender, 4 barmaid, 5 stranger, 6 exit, 7-9 more sailors to recruit; indexed by tavern_spot | none |
| 91 | 20 | not used by the engine | 20 full captain names (famous pirates and staff-style names) of the original | none |
| 92 | 12 | various single words (W_NAMES, W_UNKNOWN, W_POSSESSIVE, W_CABIN_TITLE) | 0 the possessive suffix after the player's name (land battle), 4 the default player name, 5 the "unknown" marker on the Caribbean map, 7 the cabin's title, 9 the "none" line (no hostages); 1-3 sample names, 6, 8, 10, 11 unused | none |
| 93 | 4 | lost-treasure dig texts (W_PEOPLE) | four lost civilisations of the Americas, indexed by the find's value & 3 | none |
| 94 | 6 | Caribbean map, the selected town's fields (W_TOWN_FIELD) | five field labels: forts, wealth status, nation, soldiers, citizens (k - 1); 5 the done button, unused | none |
| 95 | 7 | log book titles (W_LOG_TITLE) | 0-5 the page titles in log_page - 1 order (ship's log, party, personal, ship status, hostages, exit); 6 the log book's own title | none |
| 96 | 50 | tavern scroll when nothing is pointed (W_TAVERN_NAME) | 50 tavern names, one per town in the world's town order; indexed by player_town | none |
| 97 | 4 | villain texts (CSTR 131, 137) (W_VILLAIN_NATION) | the nationality noun for a man of each nation, Spain, England, France, Holland; indexed by the villain's nation | none |
| 98 | 6 | era choice, career summary (W_ERA) | the six era titles with their start year, earliest first; indexed by era 0-5 | none |
| 99 | 6 | famous expedition menu (W_EXPEDITION) | the six famous expeditions with their year, in menu order | none |
| 100 | 10 | welcome screen (W_WELCOME) | 0-5 six heading lines; 6-9 the four menu options (new career, load, famous expedition, quit); word 9 is replaced by 86.9 + "?" and unused | none |
| 101 | 4 | difficulty screen, hover text (W_DIFF_TEXT) | a short description of each difficulty level, easiest first; indexed by the hovered level | none |
| 102 | 5 | not used by the engine | five noble titles, lowest first | none |
| 103 | 3 | not used by the engine | loading-screen words (loading, a title, please wait) | none |
| 104 | 5 | governor's privateer-licence offer (CSTR 147) (W_NATION_A) | nation adjective with indefinite article, same shape as group 68: Spain, England, France, Holland, pirate; indexed by the town's nation | none |
| 105 | 2 | encounter, a hostile named captain (CSTR 121, arg 1) | 0 pirate, 1 pirate hunter; 0 when the ship's nation is pirate | none |
| 106 | 6 | encounter, the flag line (CSTR 122) (W_COLOURS) | nation adjective for a flag: Spain, England, France, Holland, 4 pirate, 5 unknown; indexed by the encounter's nation | none |
| 107 | 0 | not used by the engine | empty in the original | none |

### Group 20: log sentences by news code

| word index | when used | `%s` arguments |
|---|---|---|
| 0 | every log/news line: date prefix then the sentence | short month (6), day of month, the sentence below |
| 1 | NEWS_VISITED: the player visited a town | town |
| 2 | NEWS_TITLED: the player was given a title | nation with article (68), rank (78) |
| 3 | NEWS_PLUNDERED: the player sacked a town | town |
| 4 | NEWS_CAPTURED: the player captured a ship | nation with article (68), ship type (8) |
| 5 | ship sunk by the player; code never logged | (two: nation with article, ship type) |
| 6 | NEWS_ENGAGED: the player fought a ship | nation with article (68), ship type (8) |
| 7 | NEWS_LOST_TREASURE: a lost civilisation's treasure found | none |
| 8 | NEWS_TREASURE: buried treasure dug up | none |
| 9 | NEWS_VILLAIN: a villain seen in a town | villain name (rank + surname), town |
| 10 | NEWS_AMNESTY: a nation offers pardons to pirates | country (81) |
| 11 | NEWS_INDIANS: natives attack a town | town |
| 12 | NEWS_GOLD_MINE: a new mine at a town | town |
| 13 | NEWS_MALARIA: epidemic in a town | town |
| 14 | NEWS_GOVERNOR: new governor in a town | town |
| 15 | NEWS_PLUNDER: pirates sacked a town (also the public form of code 3) | town |
| 16 | NEWS_WAR: two nations at war | country (81), country (81) |
| 17 | NEWS_PEACE: two nations make peace | country, country |
| 18 | NEWS_ALLIANCE: two nations ally | country, country |
| 19 | NEWS_CAPTURE: a nation takes a town (also the public form of code 22) | country (81), town |
| 20 | treasure fleet in a town; no engine code emits it | (town, via the default case) |
| 21 | silver train in a town; no engine code emits it | (town, via the default case) |
| 22 | NEWS_TAKEN: the player took a town for a nation | town, nation name (81) |
| 23 | NEWS_NOT_TAKEN: the player failed to take a town | town |
| 24 | NEWS_FLEET_TAKEN: treasure fleet captured | none |
| 25 | NEWS_TRAIN_TAKEN: silver train captured | none |
| 26 | NEWS_RESCUED: a lost relative rescued | relative (37) |
| 27 | empty, no news code | none |
| 28 | NEWS_LETTER: accepted a letter-delivery mission | town |
| 29 | NEWS_PAPERS: accepted a papers-delivery mission | town |
| 30 | NEWS_PIRATE_OFF: a wanted pirate near a town | captain name (rank + surname), town |
| 31 | NEWS_RESCUE: accepted a rescue mission | town |
| 32 | NEWS_DELIVERED_LETTER: letter delivered | town |
| 33 | NEWS_DELIVERED_PAPERS: papers delivered | town |
| 34 | NEWS_PIRATE_TAKEN: the wanted pirate captured | captain name, then the town of the mission (the original's sentence uses only the name) |
| 35 | NEWS_SON_FREED: a prisoner freed from a town | town |
| 36 | NEWS_DISPLEASED: a governor displeased by a failed mission | none |

Codes 10-19 are world news, also told by passing ships (group 18.2) and the bartender (18.1).

### Group 33: bartender gossip by news code

| word index | when used | `%s` arguments |
|---|---|---|
| 0 | nothing notable done yet (or the latest deed was visiting this very town) | none |
| 1 | latest deed: visited a town | town |
| 2 | latest deed: given a title | nation with article (68), rank (78) |
| 3 | latest deed: sacked a town | town |
| 4 | latest deed: captured a ship | nation with article (68), ship type (8) |
| 5 | latest deed: sank a ship | nation with article (68), ship type (8) |
| 6 | latest deed: fought a ship | nation with article (68), ship type (8) |
| 7 | latest deed: found a lost treasure | none |
| 8 | latest deed: dug up buried treasure | none |
| 9-21 | empty: these codes (world news) are skipped when looking for the deed | none |
| 22 | latest deed: took a town for a nation | town, nation adjective (71) |
| 23 | latest deed: failed to take a town | town |
| 24 | latest deed: captured the treasure fleet | none |
| 25 | latest deed: captured the silver train | none |
| 26 | latest deed: rescued a relative | relative (37) |

The phrase is put into CSTR 125 (the bartender's remark); codes above 26 are skipped.

Groups: 96 referenced by the engine (66 and 67 only through 44 + rating, which never reaches them), 12 unused (0, 1, 17, 19, 24, 76, 85, 88, 91, 102, 103, 107; 107 empty).

## Notes on the original

Found while writing this catalogue. The engine follows the original in
each case; they explain entries that are never shown.

- Groups 66 and 67, and words 22-23 of group 43, are never shown: the
  verdict reads group 44 + rating, and the rating is 0-21 (`hof_rating()`,
  "Retirement and the Hall of Fame" in
  game-rules.md).
- Group 20, words 20 and 21 (a convoy arriving in a town) are never
  logged (game-rules.md, "The treasure fleet and the silver train").
- String 298 names a debt; the original, and `sim_merchant_pay_debt`,
  take 500 gold. Write the amount the rules take.
