/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026, Olivier Cochard-Labbé
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assets/pic.h"
#include "assets/fonts.h"
#include "assets/scenario.h"
#include "assets/sheet.h"
#include "assets/towns.h"
#include "assets/world.h"
#include "game.h"
#include "screen.h"

#define FADE_STEP	32	/* brightness per tick: 8 ticks per fade */
#define QUIT_FRAME	4	/* misc.crs: "Quit (Y/N)" */
#define PAUSE_FRAME	5	/* misc.crs: "Game Paused" */

const char *
game_str(const struct game *g, size_t i)
{
	return (i < g->cstr.count ? g->cstr.str[i] : "");
}

const char *
game_word(const struct game *g, size_t group, size_t i)
{
	if (group >= g->cstn.count || i >= g->cstn.groups[group].count)
		return ("");
	return (g->cstn.groups[group].str[i]);
}

static int
load_text(struct game *g)
{
	struct blob b;
	char id[32], why[128];
	int rc;

	snprintf(id, sizeof(id), "text/%s/cstr", g->lang);
	if (packs_load(g->packs, id, &b) != 0) {
		snprintf(g->err, sizeof(g->err), "missing %s", id);
		return (-1);
	}
	rc = strtab_from_blob(&b, &g->cstr, why, sizeof(why));
	blob_free(&b);
	if (rc == 0) {
		snprintf(id, sizeof(id), "text/%s/cstn", g->lang);
		if (packs_load(g->packs, id, &b) != 0) {
			snprintf(g->err, sizeof(g->err), "missing %s", id);
			return (-1);
		}
		rc = strgroups_from_blob(&b, &g->cstn, why, sizeof(why));
		blob_free(&b);
	}
	if (rc != 0)
		snprintf(g->err, sizeof(g->err), "%s: %s", id, why);
	return (rc);
}

static int
load_fonts(struct game *g)
{
	char id[32];
	int i;

	for (i = 0; i < GAME_FONTS; i++) {
		snprintf(id, sizeof(id), "font/pirate%d", i + 1);
		if (font_load(g->packs, id, g->fb.out_h, &g->font[i]) != 0)
			return (-1);
	}
	return (font_load(g->packs, "font/caps", g->fb.out_h, &g->caps));
}

/*
 * The mouse pointer, a sword, is a raw picture in the executable: u16
 * width and height, then the pixels, colour 0 transparent.  Its palette
 * entries (0x1a-0x1f) are the same greys in the game's pictures.  A pack
 * may replace it with ui/pointer.
 */
static void
load_cursor(struct game *g)
{
	struct sprite_bank bank;
	const uint8_t *p;
	int w, h, i;

	/* A pack's pointer: the first frame of ui/pointer, a sprite sheet
	 * drawn as the original's, its tip at the top left. */
	if (bank_load(g->packs, "ui/pointer", BANK_RUNS, &bank) == 0) {
		if (bank.count > 0) {
			g->cursor = bank.frames[0];
			memset(&bank.frames[0], 0, sizeof(bank.frames[0]));
		}
		bank_free(&bank);
		if (g->cursor.pixels != NULL)
			return;
	}
	if ((w = g->ui.cursor_w) == 0)
		return;
	h = g->ui.cursor_h;
	p = g->ui.cursor;
	if ((g->cursor.pixels = malloc((size_t)(w * h))) == NULL ||
	    (g->cursor.mask = malloc((size_t)(w * h))) == NULL) {
		free(g->cursor.pixels);
		g->cursor.pixels = NULL;
		return;
	}
	memcpy(g->cursor.pixels, p, (size_t)(w * h));
	for (i = 0; i < w * h; i++)
		g->cursor.mask[i] = p[i] != 0;
	g->cursor.w = w;
	g->cursor.h = h;
}

const char *const game_langs[GAME_LANGS] = { "eng", "fre", "ger", "spa" };

const char *
game_lang_asset(const struct game *g, const char *const ids[GAME_LANGS])
{
	int i;

	for (i = 1; i < GAME_LANGS; i++)
		if (strcmp(g->lang, game_langs[i]) == 0)
			return (packs_has(g->packs, ids[i]) ? ids[i] : ids[0]);
	return (ids[0]);
}

/* The language's permanent sprites (VA 0x1dd6: misc.crs, miscf.crs,
 * miscg.crs; ui/miscs a pack's Spanish), which hold the "Click to
 * continue" plaque. */
static void
load_misc(struct game *g)
{
	static const char *const ids[GAME_LANGS] = { "ui/misc", "ui/miscf",
	    "ui/miscg", "ui/miscs" };

	bank_load(g->packs, game_lang_asset(g, ids), BANK_RUNS, &g->misc);
}

int
game_set_lang(struct game *g, const char *lang)
{
	struct strtab cstr = g->cstr;
	struct strgroups cstn = g->cstn;
	char was[sizeof(g->lang)];

	memcpy(was, g->lang, sizeof(was));
	snprintf(g->lang, sizeof(g->lang), "%s", lang);
	memset(&g->cstr, 0, sizeof(g->cstr));
	memset(&g->cstn, 0, sizeof(g->cstn));
	if (load_text(g) != 0) {
		strtab_free(&g->cstr);
		strgroups_free(&g->cstn);
		g->cstr = cstr;
		g->cstn = cstn;
		memcpy(g->lang, was, sizeof(was));
		return (-1);
	}
	strtab_free(&cstr);
	strgroups_free(&cstn);
	bank_free(&g->misc);
	load_misc(g);
	return (0);
}

/* The career slots that have a start scenario (a name). */
static void
load_careers(struct game *g)
{
	struct scenario scn;
	char err[256];
	int i;

	for (i = 0; i < SCENARIO_CAREERS; i++)
		g->careers[i] = scenario_load(g->packs, g->layout, g->exe,
		    g->exe_len, i, &scn, err, sizeof(err)) == 0 &&
		    scn.name[0] != '\0';
}

int
game_init(struct game *g, struct packs *packs, const char *lang,
    const char *save_dir)
{
	struct blob b;

	memset(g, 0, sizeof(*g));
	g->packs = packs;
	rng_seed(&g->fx_rng, 1, 0x46580000ULL);
	if (fb_init(&g->fb, packs_art_height(packs)) != 0) {
		snprintf(g->err, sizeof(g->err), "cannot set up a %d pixel "
		    "high screen", packs_art_height(packs));
		return (-1);
	}
	if (save_dir != NULL)
		snprintf(g->save_dir, sizeof(g->save_dir), "%s", save_dir);
	snprintf(g->lang, sizeof(g->lang), "%s", lang);
	if (load_text(g) != 0)
		return (-1);
	if (load_fonts(g) != 0) {
		snprintf(g->err, sizeof(g->err), "cannot load the fonts");
		return (-1);
	}
	/* The executable is optional: open game data can stand in. */
	if (exe_open(packs, &b, &g->layout) != 0) {
		snprintf(g->err, sizeof(g->err), "unknown PIRATESG.EXE build");
		return (-1);
	}
	g->exe = b.data;	/* kept for the town and scenario tables */
	g->exe_len = b.len;
	if (ui_tables_load(&g->ui, packs, g->layout, g->exe, g->exe_len,
	    g->err, sizeof(g->err)) != 0)
		return (-1);
	load_cursor(g);
	load_careers(g);
	load_misc(g);
	/* The effects are optional: without them the game is silent. */
	if (packs_load(packs, "driver/digital", &b) == 0) {
		if (digital_decode(b.data, b.len, &g->sfx) != 0)
			digital_free(&g->sfx);
		blob_free(&b);
	}
	g->sfx_last = -1;
	g->music_request = -1;
	g->career.expedition = -1;
	g->hof_new = -1;
	g->brightness = 0;
	g->screen = &screen_intro;
	g->fading = 1;
	g->screen->enter(g);
	return (0);
}

void
game_free(struct game *g)
{
	int i, j;

	strtab_free(&g->cstr);
	strgroups_free(&g->cstn);
	digital_free(&g->sfx);
	for (i = 0; i < GAME_FONTS; i++)
		font_free(&g->font[i]);
	font_free(&g->caps);
	image_free(&g->bg);
	bank_free(&g->portrait);
	bank_free(&g->ship);
	bank_free(&g->btl_ship[0][0]);
	bank_free(&g->btl_ship[0][1]);
	bank_free(&g->btl_ship[1][0]);
	bank_free(&g->btl_ship[1][1]);
	bank_free(&g->btl_damage[0]);
	bank_free(&g->btl_damage[1]);
	bank_free(&g->btl_misc);
	tileset_free(&g->btl_tiles);
	tileset_free(&g->map_art);
	free(g->cursor.pixels);
	free(g->cursor.mask);
	free(g->cursor.rgba);
	bank_free(&g->intro_spr[0]);
	bank_free(&g->intro_spr[1]);
	free(g->intro_title);
	free(g->intro_shown);
	image_free(&g->credits);
	credits_free(&g->cred_text);
	bank_free(&g->town_spr);
	bank_free(&g->shop_spr[0]);
	bank_free(&g->shop_spr[1]);
	bank_free(&g->shop_spr[2]);
	bank_free(&g->tavern_spr);
	bank_free(&g->gov_spr);
	bank_free(&g->misc);
	bank_free(&g->cabin_spr);
	bank_free(&g->cloud_spr);
	bank_free(&g->party_spr);
	for (i = 0; i < 5; i++)
		bank_free(&g->land_spr[i]);
	for (i = 0; i < 8; i++)
		bank_free(&g->panel_spr[i]);
	for (i = 0; i < 8; i++)
		bank_free(&g->cave[i]);
	for (i = 0; i < 5; i++)
		bank_free(&g->id_spr[i]);
	for (i = 0; i < 2; i++)
		for (j = 0; j < 6; j++)
			group_free(&g->sw_parts[i][j]);
	bank_free(&g->sw_misc);
	image_free(&g->sw_weapons);
	free(g->world);
	free(g->exe);
	fb_free(&g->fb);
}

void
town_slug(const char *name, char *buf, size_t len)
{
	size_t n = 0;
	int dash = 0;
	char c;

	if (len == 0)
		return;
	for (; *name != '\0'; name++) {
		c = *name;
		if (c >= 'A' && c <= 'Z')
			c = (char)(c - 'A' + 'a');
		if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
			if (dash && n > 0 && n + 1 < len)
				buf[n++] = '-';
			dash = 0;
			if (n + 1 < len)
				buf[n++] = c;
		} else
			dash = 1;
	}
	buf[n] = '\0';
}

const char *
game_town_asset(struct game *g, const char *base, char *buf, size_t len)
{
	static const char *const nations[] = { "spain", "england", "france",
	    "holland" };
	const struct sim_town *t;
	char slug[64];

	if (!g->started || g->sim.player_town < 0 ||
	    (size_t)g->sim.player_town >= g->sim.ntowns)
		return (base);
	t = &g->sim.towns[g->sim.player_town];
	town_slug(t->name, slug, sizeof(slug));
	if (slug[0] != '\0' &&
	    (size_t)snprintf(buf, len, "%s/%s", base, slug) < len &&
	    packs_has(g->packs, buf))
		return (buf);
	if (t->nation < sizeof(nations) / sizeof(nations[0]) &&
	    (size_t)snprintf(buf, len, "%s/%s", base, nations[t->nation]) <
	    len && packs_has(g->packs, buf))
		return (buf);
	return (base);
}

void
game_switch(struct game *g, const struct screen *s)
{
	g->screen = s;
	g->ticks = 0;
	g->selected = 0;
	memset(&g->options, 0, sizeof(g->options));
	s->enter(g);
}

void
game_goto(struct game *g, const struct screen *s)
{
	if (g->fading == -1)
		return;		/* already leaving */
	g->next = s;
	g->fading = -1;
}

int
game_background(struct game *g, const char *pic)
{
	int rc;

	image_free(&g->bg);
	/* Dark ink and a red highlight suit parchment; screens may change
	 * them after this. */
	g->ink[0] = 55;
	g->ink[1] = 30;
	g->ink[2] = 10;
	g->highlight[0] = 170;
	g->highlight[1] = 20;
	g->highlight[2] = 10;
	if ((rc = pic_load(g->packs, pic, &g->bg)) == 0)
		memcpy(g->fb.pal, g->bg.palette, sizeof(g->fb.pal));
	return (rc);
}

int
game_portrait(struct game *g, const char *spr)
{
	bank_free(&g->portrait);
	if (spr == NULL)
		return (0);
	return (bank_load(g->packs, spr, BANK_RUNS, &g->portrait));
}

void
game_event(struct game *g, const struct ui_event *ev)
{
	if (ev->type != EV_MOVE)
		g->idle = 0;
	if (ev->type == EV_MOVE || ev->type == EV_CLICK) {
		g->pointer_x = ev->x;
		g->pointer_y = ev->y;
		g->pointer_seen = 1;
	}
	if (g->fading == -1 || g->quit)
		return;		/* input is ignored while leaving a screen */
	/*
	 * The hot keys (VA 0x2d60), but not during a fade or while a name
	 * is typed: P pauses until P again (VA 0x2e91); Q asks "Quit
	 * (Y/N)" and yes leaves the program (VA 0x2db9).  Each waits for
	 * its own answer only.  V steps the sound level, stopping the
	 * music unless it comes back to 0 (VA 0x2f38).
	 */
	if (ev->type == EV_CHAR && g->fading == 0 &&
	    g->screen != &screen_name) {
		if (g->paused == 'Q' && game_yes(g, ev)) {
			g->paused = 0;
			game_music(g, MUS_SILENCE);
			game_goto(g, NULL);
			return;
		}
		if (g->paused == 'Q' && (ev->ch == 'n' || ev->ch == 'N')) {
			g->paused = 0;
			return;
		}
		if ((ev->ch == 'p' || ev->ch == 'P') && g->paused != 'Q') {
			g->paused = g->paused ? 0 : 'P';
			return;
		}
		if ((ev->ch == 'q' || ev->ch == 'Q') && !g->paused) {
			g->paused = 'Q';
			return;
		}
		if ((ev->ch == 'v' || ev->ch == 'V') && !g->paused) {
			g->sound_level = (g->sound_level + 1) % 3;
			if (g->sound_level != 0)
				game_music(g, MUS_STOP);
			return;
		}
	}
	/* Escape stays, as at the cabin's prompt. */
	if (g->paused == 'Q' && ev->type == EV_KEY &&
	    ev->key == UI_KEY_ESCAPE) {
		g->paused = 0;
		return;
	}
	/* Releases go through, so no key stays held after the pause. */
	if (g->paused && ev->type != EV_KEYUP)
		return;
	g->screen->event(g, ev);
}

void
game_tick(struct game *g)
{
	if (g->paused)
		return;
	if (g->fading < 0) {
		g->brightness -= FADE_STEP;
		if (g->brightness <= 0) {
			g->brightness = 0;
			g->screen = g->next;
			g->next = NULL;
			g->ticks = 0;
			g->selected = 0;
			memset(&g->options, 0, sizeof(g->options));
			g->fading = 1;
			if (g->frozen) {
				memcpy(g->fb.pal, g->next_pal, sizeof(g->fb.pal));
				g->frozen = 0;
			}
			if (g->screen == NULL) {
				g->quit = 1;
				return;
			}
			g->screen->enter(g);
		}
	} else if (g->fading > 0) {
		g->brightness += FADE_STEP;
		if (g->brightness >= 256) {
			g->brightness = 256;
			g->fading = 0;
		}
	}
	if (g->screen != NULL && g->screen->tick != NULL)
		g->screen->tick(g);
	g->ticks++;
	g->idle++;
}

/*
 * The original hides the pointer for the whole of sailing (VA 0x22c76)
 * and for the sea and land battles' fighting (VA 0xe57e-0xe62a,
 * 0x5aa3-0x5c1d), which the keys steer; their results show it again.
 * The engine also keeps it off the intro pictures.
 */
static int
pointer_hidden(const struct game *g)
{
	return (g->paused || g->screen == &screen_intro ||
	    g->screen == &screen_sail ||
	    (g->screen == &screen_battle && g->btl_stage == 0) ||
	    (g->screen == &screen_land && g->land_stage == 0));
}

void
game_draw(struct game *g)
{
	if (g->screen == NULL)
		return;
	/*
	 * A screen fades out on the last frame it drew: what left it
	 * (an answer, a cleared event, a stage moved on) may already have
	 * changed what it would draw now.  Its palette is kept apart for
	 * the next screen.
	 */
	if (g->fading == -1) {
		if (!g->frozen) {
			memcpy(g->next_pal, g->fb.pal, sizeof(g->next_pal));
			g->frozen = 1;
		}
		memcpy(g->fb.pal, g->shown_pal, sizeof(g->fb.pal));
		return;
	}
	if (g->bg.pixels != NULL)
		fb_image(&g->fb, &g->bg, 0, 0, 0);
	else
		fb_clear(&g->fb, 0);
	g->screen->draw(g);
	if (g->paused && g->misc.count > PAUSE_FRAME)
		fb_frame(&g->fb, &g->misc.frames[g->paused == 'Q' ?
		    QUIT_FRAME : PAUSE_FRAME], 268, 218);
	/* The pointer, its tip at the top left, once the mouse is used. */
	if (g->pointer_seen && g->cursor.pixels != NULL && !pointer_hidden(g))
		fb_frame(&g->fb, &g->cursor, g->pointer_x, g->pointer_y);
	memcpy(g->shown_pal, g->fb.pal, sizeof(g->shown_pal));
}

int
game_attach_world(struct game *g)
{
	int era;

	for (era = 0; era < EXE_ERAS; era++)
		if (town_era_years[era] == g->sim.era_year)
			break;
	if (era == EXE_ERAS)
		return (-1);
	if (g->world == NULL && (g->world = calloc(1, sizeof(*g->world))) == NULL)
		return (-1);
	if (g->world->era != era || g->world->sine[64] == 0) {
		if (world_load(g->world, g->packs, g->layout, g->exe,
		    g->exe_len, era, g->err, sizeof(g->err)) != 0) {
			free(g->world);
			g->world = NULL;
			return (-1);
		}
	}
	g->sim.world = g->world;
	/* An older save lacks some town data: the town table has it. */
	{
		struct era_towns towns;

		if (towns_for_era(g->packs, g->layout, g->exe, g->exe_len,
		    era, &towns, g->err, sizeof(g->err)) != 0)
			return (-1);
		sim_restore_towns(&g->sim, &towns);
	}
	return (0);
}

/* Start the simulation from the player's choices. */
int
game_start_career(struct game *g)
{
	struct era_towns era;
	struct scenario scn;
	int index;

	index = g->career.expedition >= 0 ?
	    SCENARIO_CAREERS + g->career.expedition :
	    scenario_career(g->career.era, g->career.nation == NATION_SPAIN ?
	    SCN_SPAIN : (enum scenario_nation)(g->career.nation - 1));
	if (scenario_load(g->packs, g->layout, g->exe, g->exe_len, index,
	    &scn, g->err, sizeof(g->err)) != 0 ||
	    towns_for_era(g->packs, g->layout, g->exe, g->exe_len,
	    g->career.era, &era, g->err, sizeof(g->err)) != 0)
		return (-1);
	snprintf(g->career.title, sizeof(g->career.title), "%s", scn.name);
	/* A fixed seed for now; it will come from the clock or a setting. */
	sim_new_game(&g->sim, &era, &scn, 1);
	snprintf(g->sim.player.name, sizeof(g->sim.player.name), "%s",
	    g->career.name);
	g->sim.player.nation = g->career.nation;
	g->sim.player.difficulty = g->career.difficulty;
	g->sim.player.ability = g->career.ability;
	g->sim.player.expedition = g->career.expedition;
	g->started = 1;
	g->news_seen = g->sim.nnews;
	return (game_attach_world(g));
}

int
game_has_sfx(const struct game *g, int n)
{
	return (n >= 0 && n < DIGITAL_EFFECTS && (g->sfx.pcm[n] != NULL ||
	    (g->sfx_packed & (1u << n))));
}

const char *
game_sfx_id(int n)
{
	static const char *const ids[DIGITAL_EFFECTS] = {
		"sound/hail", "sound/broadside", "sound/land-battle",
		"sound/last-man", "sound/harbour", "sound/sail-ho",
		"sound/parry", "sound/hit-foe", "sound/hit-you",
	};

	return (n >= 0 && n < DIGITAL_EFFECTS ? ids[n] : NULL);
}

const char *
game_fm_id(int cmd)
{
	switch (cmd) {
	case FM_SHOT_HIT:
		return ("sound/shot-hit");
	case FM_SHOT_MISS:
		return ("sound/shot-miss");
	case FM_CRASH:
		return ("sound/crash");
	case FM_SWING:
		return ("sound/swing");
	case FM_BLOW:
		return ("sound/blow");
	case FM_MISS:
		return ("sound/miss");
	case FM_THEIR_MEN:
		return ("sound/their-men");
	case FM_OUR_MEN:
		return ("sound/our-men");
	case FM_SCRAPE:
		return ("sound/scrape");
	case FM_TRADE:
		return ("sound/trade");
	case FM_ROW:
		return ("sound/row");
	default:
		return (NULL);
	}
}

void
game_sfx(struct game *g, int n)
{
	if (!game_has_sfx(g, n))
		return;
	g->sfx_pending |= 1u << n;
	g->sfx_last = n;
}

const char *
game_tune_id(int cmd)
{
	static const struct {
		int		 cmd;
		const char	*id;
	} tunes[] = {
		{ MUS_TITLE, "music/title" },
		{ MUS_LOST_TREASURE, "music/lost-treasure" },
		{ MUS_FAMILY, "music/family" },
		{ MUS_NIECE, "music/niece" },
		{ MUS_DAUGHTER, "music/daughter" },
		{ MUS_BANK, "music/bank" },
		{ MUS_BARMAID, "music/barmaid" },
		{ MUS_TREASURE, "music/treasure" },
		{ MUS_NEWS, "music/news" },
		{ MUS_NEWS_TITLED, "music/news-titled" },
		{ MUS_PROMOTION, "music/promotion" },
		{ MUS_TRADE, "music/trade" },
		{ MUS_PLUNDER, "music/plunder" },
		{ MUS_RESCUE, "music/rescue" },
		{ MUS_TAVERN, "music/tavern" },
		{ MUS_WEDDING, "music/wedding" },
		{ MUS_SAIL_HO, "music/sail-ho" },
		{ MUS_VERDICT, "music/verdict" },
		{ MUS_PRIZE, "music/prize" },
		{ MUS_PRISON, "music/prison" },
		{ MUS_HARBOUR, "music/harbour" },
	};
	size_t i;

	for (i = 0; i < sizeof(tunes) / sizeof(tunes[0]); i++)
		if (tunes[i].cmd == cmd)
			return (tunes[i].id);
	return (NULL);
}

void
game_fm(struct game *g, int cmd)
{
	if (g->sound_level >= 2)
		return;		/* VA 0x1b8a2 */
	g->fm_request = cmd;
}

void
game_music(struct game *g, int cmd)
{
	game_music_as(g, cmd, NULL);
}

void
game_music_as(struct game *g, int cmd, const char *alt)
{
	if (cmd > MUS_STOP && g->sound_level != 0)
		return;		/* VA 0x1b8cb; stopping goes through */
	g->music_request = cmd;
	g->music_alt = alt;
}
