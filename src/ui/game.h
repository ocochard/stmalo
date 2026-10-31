/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026, Olivier Cochard-Labbé
 */

#ifndef FLIBUSTIER_GAME_H
#define FLIBUSTIER_GAME_H

#include <stddef.h>
#include <stdint.h>

#include "assets/credits.h"
#include "assets/digital.h"
#include "assets/font.h"
#include "assets/text.h"
#include "assets/uitables.h"
#include "data/exe.h"
#include "data/pack.h"
#include "assets/image.h"
#include "assets/sprite.h"
#include "assets/tileset.h"
#include "assets/world.h"
#include "gfx/sea.h"
#include "gfx/fb.h"
#include "gfx/text.h"
#include "sim/sim.h"

/*
 * The game shell: screens, input and drawing, independent of raylib.
 * A front end feeds events and ticks, and shows game.fb; tests and bots do
 * the same without a window.
 */

enum ui_key {
	UI_KEY_NONE,
	UI_KEY_UP,
	UI_KEY_DOWN,
	UI_KEY_LEFT,
	UI_KEY_RIGHT,
	UI_KEY_ENTER,
	UI_KEY_ESCAPE,
	UI_KEY_BACKSPACE,
	UI_KEY_SHIFT,		/* either shift key */
	UI_KEY_UP_LEFT,		/* numeric keypad diagonals: 7, 9, 1, 3 */
	UI_KEY_UP_RIGHT,
	UI_KEY_DOWN_LEFT,
	UI_KEY_DOWN_RIGHT
};

enum ui_event_type {
	EV_KEY,		/* key pressed */
	EV_KEYUP,	/* key released */
	EV_CHAR,	/* ch: a typed character, Unicode */
	EV_MOVE,	/* x, y: pointer position on the 640x480 screen */
	EV_CLICK,	/* x, y */
	EV_RCLICK	/* x, y: the right button */
};

struct ui_event {
	enum ui_event_type	type;
	int			key;
	int			ch;
	int			x, y;
};

#define GAME_FONTS	4
#define NAME_MAX_LEN	20

struct screen;

/* The player's choices on the way to a new career. */
struct career {
	int	era;		/* 0..5 */
	int	nation;		/* town nation order: Spain, England... */
	int	difficulty;	/* 0..3 */
	int	ability;	/* 0..4 */
	int	expedition;	/* -1, or 0..5 for a famous expedition */
	char	name[NAME_MAX_LEN * 2 + 1];	/* UTF-8 */
	char	title[24];	/* start scenario: "English Buccaneer" */
};

struct game {
	struct packs		*packs;
	char			 lang[4];	/* eng, fre, ger, or a pack's */
	struct strtab		 cstr;
	struct strgroups	 cstn;
	struct font		 font[GAME_FONTS];	/* PIRATE1..4 */
	struct font		 caps;
	uint8_t			*exe;
	size_t			 exe_len;
	const struct exe_layout	*layout;
	struct ui_tables	 ui;		/* the screens' tables */
	/* Which career starts exist, by scenario_career() index: the
	 * original has no Dutch career in 1560, a pack may. */
	uint8_t			 careers[SCENARIO_CAREERS];

	struct fb		 fb;
	int			 brightness;	/* 0..256, for fades */
	const struct screen	*screen;
	const struct screen	*next;		/* during a fade out */
	int			 fading;	/* -1 out, +1 in, 0 none */
	int			 frozen;	/* fading out on the last frame drawn */
	uint8_t			 shown_pal[256 * 3];	/* that frame's palette */
	uint8_t			 next_pal[256 * 3];	/* set since, for the next */
	int			 selected;	/* option under the cursor */
	struct text_layout	 options;	/* menu options of the last draw */
	unsigned		 ticks;		/* on the current screen */
	struct image		 bg;		/* current background */
	uint8_t			 ink[3];	/* text colour */
	uint8_t			 highlight[3];	/* selected option colour */
	struct sprite_bank	 portrait;	/* current portrait, if any */
	struct frame		 cursor;	/* the sword pointer */
	int			 pointer_x, pointer_y;
	int			 pointer_seen;	/* a pointer event arrived */
	int			 quit;

	struct career		 career;
	struct sim		 sim;
	int			 started;	/* sim holds a game */
	size_t			 news_seen;	/* news shown so far */
	struct world		*world;		/* the career's era, for sailing */
	struct sprite_bank	 ship;		/* sailing ship, 16 headings */
	struct sea		 sea;
	struct tileset		 map_art;	/* tiles/map, for its art */
	int			 turn;		/* steering input: -1, 0, +1 */
	int			 turn_tap;	/* a sailing press not yet
						 * seen by a pass */
	int			 enc_result;	/* -1 while the dialogs go on */
	struct sprite_bank	 btl_ship[2][2];	/* [you, foe][A, B] */
	struct sprite_bank	 btl_damage[2];	/* damage pictures */
	struct sprite_bank	 btl_misc;	/* sbtlmisc.cur */
	struct tileset		 btl_tiles;	/* 32 x 32 battle tiles */
	int			 btl_fire, btl_sails;	/* pending inputs */
	int			 btl_heading[2];	/* --smooth: as drawn */
	int			 btl_hurt[2];	/* damage word shown, or -1 */
	int			 btl_hurt_seen[2];	/* damage stage last seen */
	int			 btl_stage;	/* 0 fight, 1 outcome, 2 prize */
	int			 btl_final;	/* outcome after sim_battle_finish */
	struct sprite_group	 sw_parts[2][6];	/* [you, foe] fighter layers */
	char			 sw_part_id[2][6][32];	/* the ID each holds */
	struct sprite_bank	 sw_misc;	/* flags */
	struct image		 sw_weapons;	/* weapon choice picture */
	int			 sw_stage;	/* 0 intro, 1 weapon, 2 fight,
						 * 3 outcome */
	int			 sw_held;	/* arrow and shift keys held */
	int			 sw_flag_y[2];	/* drawn flag heights */
	int			 sw_wait;	/* game ticks to the next fight tick */
	int			 sw_period;	/* game ticks of this fight tick */
	int			 sw_from[2];	/* --smooth: x at the last one */
	int			 intro_step;	/* 0 logo .. 4 title */
	int			 intro_hold;	/* ticks shown at full brightness */
	int			 intro_queued;	/* a key pressed during a fade */
	int			 intro_next;	/* advance on the next enter */
	struct sprite_bank	 intro_spr[2];
	uint8_t			*intro_title;	/* title layer, w x h, 0 clear */
	uint8_t			*intro_shown;	/* dissolve cells revealed */
	uint32_t		 intro_iter;	/* title screen loop passes */
	uint32_t		 intro_cell;	/* dissolve generator state */
	uint32_t		 intro_left;	/* dissolve cells to visit */
	int			 intro_timer[4];	/* palette cycle counters */
	int			 intro_cycles;	/* steps of the 0xd3 cycle */
	int			 intro_pause;
	struct image		 credits;	/* the credits' text picture */
	struct credits		 cred_text;	/* or a pack's credits text */
	int			 cred_count;	/* blocks to show */
	int			 cred_block;	/* 0 .. cred_count - 1 */
	int			 cred_ticks;	/* ticks into the block */
	int			 cred_step;	/* ink step, 0 .. 19 */
	struct sprite_bank	 town_spr;	/* harbour and closed gate */
	int			 town_spot;	/* building under the cursor, -1 */
	int			 town_key;	/* keyboard selection, -1 */
	int			 town_free;	/* the next pass costs no time */
	int			 wright_stage;	/* 0 menu, 1 repair, 2 sell, 3 note */
	int			 wright_note;	/* CSTR shown at stage 3 */
	int			 shop_stage;	/* 0 greeting, 1 trading, 2 refused */
	struct sprite_bank	 shop_spr[3];	/* board pieces, board, exit */
	int			 shop_buying;	/* the Sell/Buy sign */
	struct sprite_bank	 tavern_spr;	/* the people who may be there */
	int			 tavern_key;	/* keyboard selection */
	struct sprite_bank	 id_spr[5];	/* small flags, small skills,
						 * portrait, big flag, big skill */
	int			 id_key;	/* keyboard selection, or -1 */
	int			 name_default;	/* the name is still the default */
	size_t			 story_pos;	/* next step of the story */
	int			 story_text;	/* CSTR on the page, or -1 */
	int			 story_waiting;	/* a key turns the page */
	int			 sw_story;	/* the sword fight is the story's */
	struct sprite_bank	 misc;		/* ui/misc: the "continue" plaque */
	unsigned		 idle;		/* ticks since the last input */
	int			 master_page;	/* the sailing master: 0 or 1 */
	int			 bank_stage;
	int			 arrive_stage;	/* the arrival menu, then its result */
	int			 arrive_result;	/* enum arrival */
	int			 sw_guard;	/* the sword fight is the guard's duel */
	int			 sail_fast;	/* T: a sailing pass every frame */
	int			 sw_practice;	/* the sword fight is practice */
	int			 sw_assault;	/* the sword fight storms a town */
	int			 assault_town;
	int			 assault_aboard;	/* men left with the fleet */
	int			 sack_stage;
	struct sack		 sack;
	int			 looting;	/* the merchant board is the loot */
	int			 info_town;	/* city information: -1 the current */
	const struct screen	*info_back;	/* where it returns */
	int			 talk;		/* a tavern talk's stage */
	int			 talk_town;
	/* The treasure maps shown one after another, then "tmap_back". */
	int			 tmap_maps[SIM_MAPS];
	int			 tmap_n, tmap_i;
	const struct screen	*tmap_back;
	/* What the dig turned up, page by page. */
	struct sim_find		 dig[SIM_MAPS];
	int			 dig_n, dig_i, dig_page;
	int			 tavern_spot;	/* pointed at, or -1 */
	int			 btl_chosen;	/* the ship to command is chosen */
	int			 ashore_prompt;	/* "go ashore?" waits for Y or N */
	int			 march_held;	/* bits 1 W, 2 E, 4 N, 8 S */
	int			 march_tap;	/* on foot and in land battles,
						 * presses not yet seen by a
						 * pass */
	struct sprite_bank	 party_spr;	/* sailmisc.spr */
	struct sprite_bank	 land_spr[5];	/* lbtlmisc, urlbtl, hsilbtl,
						 * hsclbtl, fortbtl */
	int			 land_button;	/* Space, Enter or a click */
	int			 land_wait;
	int			 land_from[LAND_UNITS][2];	/* --smooth: the
						 * units at the last pass */
	int			 land_stage;	/* 0 fighting, 1 the failure */
	int			 panel_cursor;	/* the fleet panel: 0-7 a ship, 8 done */
	unsigned		 panel_marked;	/* ships selected, bit by bit */
	struct sprite_bank	 panel_spr[8];
	int			 sw_named;	/* the chosen weapon's name shows */
	int			 cabin_spot;	/* pointed at, or -1 */
	int			 cabin_key;	/* selected with the keys */
	const struct screen	*cave_back;	/* where the cave returns */
	struct sprite_bank	 cabin_spr;
	struct sprite_bank	 cloud_spr;	/* cloud.cur */
	unsigned		 sail_wait;	/* frames to the next pass */
	unsigned		 sail_passes;	/* passes of the sailing loop */
	struct sail_pose {		/* at the last pass, to draw the */
		int32_t	 x8, y8;	/* moves between passes: the fleet */
		int	 heading, ashore;	/* in 1/8 pixels, */
		int	 cloud_x[3], cloud_y[3];	/* the clouds */
	}			 sail_from;
	struct plunder		 plunder;
	struct sprite_bank	 cave[8];	/* the treasure piles */
	struct sprite_bank	 gov_spr;	/* the proclamation */
	int			 gov_stage;	/* greeting, promotion, land, offer,
						 * short of gold */
	int			 gov_rank, gov_acres;
	int			 gov_resume;	/* back from the villain's duel */
	int			 gov_report;	/* the mission's end, as reported */
	int			 gov_villain;	/* the villain named, or -1 */
	/* The villain met (VA 0x18913): his page, the duel, then back. */
	int			 villain, villain_stage, sw_villain;
	const struct screen	*villain_back;
	const struct screen	*disgrace_back;
	int			 sw_mutiny;	/* the fight is the mutineers' */
	/* The Map of the Caribbean: towns' dots, the selected town (kept
	 * between visits), the list's first row, the player's mark. */
	int			 cm_x[TOWN_MAX], cm_y[TOWN_MAX];
	int			 cm_sel, cm_scroll, cm_px, cm_py;
	/* The log book: its page (0 the menu) and the log's leaf. */
	int			 log_page, log_leaf;
	int			 panel_view;	/* the fleet panel only shows */
	int			 gov_tries;	/* ransom tries this visit */
	struct daughter		 daughter;	/* the governor's, met */
	int			 gov_said;	/* her answer: a CSTR */
	int			 sw_suitor;	/* the duel with her suitor */
	int			 merdaugh;	/* her page: a CSTR, 0 none */
	/* The end of a career: its figures, the page, whether it was
	 * forced (no "Return to pirating"), the verdict's phrase. */
	struct career_end	 end;
	int			 end_page, end_forced, end_phrase;
	int			 quit_prompt;	/* the cabin's "Quit (Y/N)" */
	/* Held by a hot key: 'P' "Game Paused", 'Q' "Quit (Y/N)", 0 not. */
	int			 paused;
	/* V: 0 music and FM effects, 1 FM effects only, 2 neither
	 * (VA 0x4c0ec). */
	int			 sound_level;
	int			 panel_abandon;	/* the panel picks a ship to leave */
	int			 panel_replace;	/* ... to make room for a prize */
	/* After a sea battle, the steps still to show. */
	int			 btl_news, btl_landed, btl_aboard, btl_hunted;
	int			 btl_loot;	/* a capture's board to show */
	/* The digitised effects, and those asked for since the front end
	 * last played them (bit n: effect n); the last one, for tests. */
	struct digital		 sfx;
	unsigned		 sfx_packed;	/* bit n: a pack's recording of
					 * effect n, with or without DIGITAL */
	unsigned		 sfx_pending;
	int			 sfx_last;
	/* The tune asked for (MUS_*), -1 none, until the front end takes
	 * it. */
	int			 music_request;
	/* A recording that screen would rather have for that tune, such as
	 * "music/shipwright" for the trade tune, or NULL. */
	const char		*music_alt;
	/* The FM effect asked for this pass (FM_*), 0 none: one a pass, the
	 * last asked wins, as the original's request word (VA 0x49a7e). */
	int			 fm_request;
	int			 flag_composed;	/* the flag is over the sky */
	int			 flag_frame;	/* the waving flag's frame */
	unsigned		 flag_next;	/* the tick it changes after */
	/* Cosmetic randomness, apart from the rules' deterministic draws
	 * so that it never changes a game (the flag's jitter). */
	struct rng		 fx_rng;
	/* The original's debug mode (its "RDM" argument; --debug): keys
	 * while sailing, and a right click on the map moves the ship. */
	int			 debug;
	/* --smooth: the fleet and the clouds drawn moving between sailing
	 * passes (an engine choice). */
	int			 smooth;
	int			 cm_move, cm_mx, cm_my;	/* the map's right click */
	int			 prize_loot;	/* the board holds her cargo */
	int			 jettison;	/* the board throws cargo away */
	/* News of villains (VA 0x1eba6): who tells, of which town, the
	 * villains named, the answer shown, where it goes next. */
	int			 vnews_source, vnews_town, vnews_n;
	int			 vnews_list[SIM_VILLAINS];
	int			 vnews_said, vnews_who, vnews_where;
	const struct screen	*vnews_back;
	int			 hostage_said;	/* the hostage's news, or -1 */
	int			 escape_page;	/* the roll call's second page */
	int			 deserted_town;	/* the town just left */
	char			 save_dir[512];	/* empty: saving disabled */
	int			 saving;	/* slot screen: save, not load */
	char			 slot_desc[10][96];	/* "" when empty */
	char			 msg[128];	/* one-line feedback */
	int			 hof_new;	/* new Hall of Fame row, or -1 */
	char			 err[256];
};

/*
 * Returns 0, or -1 with a message in g->err.  "save_dir" may be NULL or
 * empty to disable saved games.
 */
int	game_init(struct game *g, struct packs *packs, const char *lang,
	    const char *save_dir);
void	game_free(struct game *g);

/*
 * The asset to use for "base" (an ID such as "pic/tavern") in the town
 * the player is in, so packs can give each town its own:
 * "<base>/<town>" when a pack has it, else "<base>/<nation>" for the
 * town's nation (spain, england, france, holland), else "base" itself.
 * <town> is the town's name in lowercase, each run of other characters
 * than a-z and 0-9 a "-": "port-royale".  Outside a town, "base".  The
 * result is "buf" or "base".
 */
const char *game_town_asset(struct game *g, const char *base, char *buf,
	    size_t len);

/* The ID part for a town name: "Port Royale" is "port-royale". */
void	town_slug(const char *name, char *buf, size_t len);
/* Go to a screen at once, without a fade (steps of one picture). */
void	game_switch(struct game *g, const struct screen *s);

void	game_event(struct game *g, const struct ui_event *ev);
void	game_tick(struct game *g);	/* 60 per second */
void	game_draw(struct game *g);	/* into g->fb */

/* Strings, "" when missing. */
const char	*game_str(const struct game *g, size_t i);
const char	*game_word(const struct game *g, size_t group, size_t i);
/*
 * The languages with their own pictures, in the order of every
 * per-language table: the original's three, then Spanish, which only a
 * pack gives.
 */
#define GAME_LANGS	4
extern const char *const game_langs[GAME_LANGS];
/* "ids[i]" for the language, when the packs give it, else English's. */
const char	*game_lang_asset(const struct game *g,
		    const char *const ids[GAME_LANGS]);
/* Switch to another language's texts and plaques: 0, or -1 with
 * g->err set and the old language kept. */
int	game_set_lang(struct game *g, const char *lang);
/* A Y/N prompt's yes, in the language. */
int	game_yes(const struct game *g, const struct ui_event *ev);

/* The digitised effects by where the original plays them (VA 0x7d82). */
enum {
	SFX_HAIL,		/* a named captain's "Arrgh!" (VA 0xd1fc) */
	SFX_BROADSIDE,		/* either ship fires (VA 0x7db8) */
	SFX_LAND_BATTLE,	/* a land battle begins (VA 0x169b7) */
	SFX_LAST_MAN,		/* the opponent's last man falls to a blow */
	SFX_HARBOUR,		/* sailing into a harbour (VA 0x18fa9) */
	SFX_SAIL_HO,		/* a sail sighted (VA 0xd1fc) */
	SFX_PARRY,		/* blades meet (VA 0x2747c) */
	SFX_HIT_FOE,		/* the player's blow lands */
	SFX_HIT_YOU		/* the opponent's blow lands */
};

/* The music driver's commands, by where the original plays them (VA
 * 0x1b8cb calls; MUS_STOP is VA 0x1b8ee). */
enum {
	MUS_SILENCE = 0,	/* at once, no fade (opening the cabin) */
	MUS_STOP = 1,
	MUS_TITLE = 9,		/* the intro and the welcome menu */
	MUS_LOST_TREASURE = 10,	/* the lost treasure dug up */
	MUS_FAMILY = 11,	/* dinner with the merchant's family */
	MUS_NIECE = 12,		/* meeting the governor's niece */
	MUS_DAUGHTER = 13,	/* ... or his daughter */
	MUS_BANK = 14,
	MUS_BARMAID = 15,	/* while the barmaid talks */
	MUS_TREASURE = 16,	/* a pirate's treasure dug up */
	MUS_NEWS = 17,		/* the governor's political news */
	MUS_NEWS_TITLED = 18,	/* the same, titled by his nation */
	MUS_PROMOTION = 19,
	MUS_TRADE = 20,		/* the merchant, the shipwright */
	MUS_PLUNDER = 21,	/* a capture's or a sack's loot */
	MUS_RESCUE = 22,	/* a relative rescued */
	MUS_TAVERN = 23,
	MUS_WEDDING = 24,
	MUS_SAIL_HO = 25,	/* a sail sighted */
	MUS_VERDICT = 27,	/* the end of a career */
	MUS_PRIZE = 28,		/* "Shall we keep her?" */
	MUS_PRISON = 43,	/* prison, or marooned */
	MUS_HARBOUR = 44	/* arriving in a town */
};

/* The asset ID a pack replaces tune "cmd" (MUS_*) with, such as
 * "music/title", or NULL for commands that are not tunes. */
const char	*game_tune_id(int cmd);

/* Ask for driver command "cmd" (MUS_*). */
void	game_music(struct game *g, int cmd);
/* The same, with the recording "alt" preferred when a pack has one. */
void	game_music_as(struct game *g, int cmd, const char *alt);

/* Ask for FM effect "cmd" (FM_*), played over the music. */
void	game_fm(struct game *g, int cmd);

/* Ask for digitised effect n (SFX_*) to play. */
void	game_sfx(struct game *g, int n);
/* Whether effect n can play: DIGITAL.PIR's, or a pack's recording. */
int	game_has_sfx(const struct game *g, int n);
/* The asset IDs a pack replaces the sounds with: digitised effect n
 * (SFX_*, "sound/broadside"), or FM effect "cmd" (FM_*, "sound/swing");
 * NULL for none. */
const char	*game_sfx_id(int n);
const char	*game_fm_id(int cmd);

#endif
