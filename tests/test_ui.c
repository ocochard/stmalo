/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026, Olivier Cochard-Labbé
 *
 * Game shell tests: drive the start screens with scripted input, without
 * a window.  Needs the game data (FLIBUSTIER_DATA); skipped otherwise.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include "assets/pic.h"
#include "assets/towns.h"
#include "assets/world.h"
#include "data/fsys.h"
#include "data/pack.h"
#include "sim/hof.h"
#include "sim/json.h"
#include "sim/proto.h"
#include "sim/save.h"
#include "sim/internal.h"
#include "ui/game.h"
#include "ui/screen.h"
#include "ui/script.h"

#define SKIP	77

static int failures;
static struct packs *packs;
static char saves_tmp[] = "/tmp/flibustier-ui-XXXXXX";

#define CHECK(cond) do {						\
	if (!(cond)) {							\
		fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__,	\
		    __LINE__, #cond);					\
		failures++;						\
	}								\
} while (0)

/*
 * Run a script in a fresh game; the caller frees it.  Saved games are
 * disabled unless "saves" is set, which removes "Continue a saved game"
 * from the welcome menu.
 */
static struct game *
play_raw(const char *saves, const char *lang, const char *script)
{
	static struct game g;
	static char full[2048];

	/* A leading "enter" means "skip to the menu": one key ends the
	 * intro, one jumps to the last credits block, one ends them. */
	if (strncmp(script, "enter", 5) == 0 && strlen(script) + 16 <
	    sizeof(full)) {
		snprintf(full, sizeof(full), "enter enter %s", script);
		script = full;
	}
	memset(&g, 0, sizeof(g));
	if (game_init(&g, packs, lang, saves) != 0) {
		fprintf(stderr, "game_init: %s\n", g.err);
		failures++;
		return (NULL);
	}
	/* Sail in the fast mode (T), a pass a frame, so that the scripts
	 * count passes; test_pace checks the normal pace. */
	g.sail_fast = 1;
	if (game_script(&g, script) != 0) {
		fprintf(stderr, "script: %s\n", g.err);
		failures++;
	}
	return (&g);
}

/* The town the career left from: the nearest, just out of it. */
static struct sim_town *
home_port(struct game *g)
{
	int dist;

	return (&g->sim.towns[sim_nearest_town(&g->sim, &dist)]);
}

/*
 * Put the fleet on a water cell where town "t" is reached (its arrival
 * cells): the next step sails in.  Its harbour mouth is not one.
 */
static void
into_town(struct game *g, const struct sim_town *t)
{
	const struct world *w = g->sim.world;
	int town = (int)(t - g->sim.towns), x, y, r, best = -1;
	size_t i;

	/* The water pixel nearest a cell's centre, so a step any way stays
	 * on the cell. */
	for (i = 0; w != NULL && i < w->narrival; i++) {
		if (w->arrival[i].town != town)
			continue;
		for (y = 0; y < 16; y++)
			for (x = 0; x < 16; x++) {
				r = abs(x - 8) > abs(y - 8) ? abs(x - 8) :
				    abs(y - 8);
				if ((best < 0 || r < best) && world_terrain(w,
				    w->arrival[i].x * 16 + x,
				    w->arrival[i].y * 16 + y) != TERRAIN_LAND) {
					best = r;
					g->sim.fleet.x = w->arrival[i].x * 16 + x;
					g->sim.fleet.y = w->arrival[i].y * 16 + y;
				}
			}
	}
	g->sim.fleet.leaving = -1;
	CHECK(best >= 0);
}

/*
 * Play a script, with no sail sighted once the fleet leaves port (its
 * "click:60,300"): sightings draw on the same random numbers as
 * everything else, so a career's first days vary.  The encounter tests
 * turn them back on.
 */
static struct game *
play_in(const char *saves, const char *lang, const char *script)
{
	static char head[2048];
	const char *leave = strstr(script, "click:60,300");
	struct game *g;

	if (leave == NULL || (size_t)(leave - script) >= sizeof(head))
		return (play_raw(saves, lang, script));
	memcpy(head, script, (size_t)(leave - script));
	head[leave - script] = '\0';
	if ((g = play_raw(saves, lang, head)) == NULL)
		return (NULL);
	g->sim.encounters = 0;
	if (game_script(g, leave) != 0) {
		fprintf(stderr, "script: %s\n", g->err);
		failures++;
	}
	return (g);
}

static struct game *
play(const char *lang, const char *script)
{
	return (play_in(NULL, lang, script));
}

static void
test_career(void)
{
	struct game *g;

	/* Intro, welcome, era 1660 (5th), the English flag, ability 3
	 * (gunnery), difficulty 2, name, summary. */
	g = play("eng", "enter enter down down down down enter down enter "
	    "right right right right enter right enter type:Blackbeard enter enter");
	if (g == NULL)
		return;
	CHECK(g->started);
	CHECK(g->career.era == 4 && g->career.nation == NATION_ENGLAND);
	CHECK(strcmp(g->career.name, "Blackbeard") == 0);
	CHECK(g->career.difficulty == 1 && g->career.ability == 3);
	CHECK(strcmp(g->career.title, "English Buccaneer") == 0);
	CHECK(g->sim.era_year == 1660 && g->sim.ntowns == 43);
	/* England and Spain start at war in this scenario. */
	CHECK(g->sim.relations[NATION_SPAIN][NATION_ENGLAND] == -1);
	game_free(g);
}

static void
test_menus(void)
{
	struct game *g;

	/* "Continue a saved game" is disabled: one down skips to the famous
	 * expeditions; the first expedition is in 1560. */
	g = play("eng", "enter down enter enter wait:40 right enter enter "
	    "type:Hawkins enter enter");
	if (g != NULL) {
		CHECK(g->started && g->career.expedition == 0);
		CHECK(strcmp(g->career.title, "John Hawkins") == 0);
		CHECK(g->sim.era_year == 1560);
		/* After the story, no sailing master: it starts at sea. */
		CHECK(game_script(g, "esc") == 0 &&
		    g->screen == &screen_sail && g->sim.fleet.leaving < 0);
		game_free(g);
	}
	/* No Dutch career in 1560: down from the French flag skips past
	 * the Dutch one to the Spanish. */
	g = play("eng", "enter enter enter down down down");
	if (g != NULL) {
		CHECK(g->id_key == 3);
		game_free(g);
	}
	/* With the mouse: a flag, an ability icon, then a level (pointing
	 * at it changes the portrait). */
	g = play("eng", "enter enter down down down down enter wait:40 "
	    "click:60,60");
	if (g != NULL) {
		CHECK(g->screen == &screen_ability &&
		    g->career.nation == NATION_ENGLAND);
		CHECK(game_script(g, "click:310,430") == 0 &&
		    g->screen == &screen_difficulty &&
		    g->career.ability == ABILITY_NAVIGATION);
		CHECK(game_script(g, "move:350,355") == 0 && g->id_key == 2);
		CHECK(game_script(g, "click:350,355") == 0 &&
		    g->screen == &screen_name && g->career.difficulty == 2);
		game_free(g);
	}
	/* Each period is a full-width row: a click on 1660's description
	 * picks it. */
	g = play("eng", "enter enter wait:40 click:20,375 wait:40");
	if (g != NULL) {
		CHECK(g->career.era == 4 && g->screen == &screen_nation);
		game_free(g);
	}
	/* "Are ye satisfied?": Nay starts over from the flags. */
	g = play("eng", "enter enter down down down down enter down enter "
	    "right enter enter enter right");
	if (g != NULL) {
		CHECK(g->screen == &screen_confirm && g->id_key == 1);
		CHECK(game_script(g, "enter") == 0 && g->screen == &screen_nation);
		CHECK(game_script(g, "down down enter right enter enter enter "
		    "click:210,380 wait:40") == 0);
		CHECK(g->started && g->career.nation == NATION_FRANCE);
		game_free(g);
	}
	/* Escape goes back, the name needs a letter, Backspace erases. */
	g = play("eng", "enter enter esc");
	if (g != NULL) {
		CHECK(!g->started && g->career.era == 0);
		game_free(g);
	}
	g = play("eng", "enter enter enter left enter right enter enter "
	    "type:Ab bs enter");
	if (g != NULL) {
		CHECK(strcmp(g->career.name, "A") == 0);
		game_free(g);
	}
	/* P and Q are letters of the name, not hot keys. */
	g = play("eng", "enter enter enter left enter right enter enter "
	    "type:PipQ");
	if (g != NULL) {
		CHECK(strcmp(g->career.name, "PipQ") == 0 && !g->paused);
		/* Elsewhere Q's yes leaves the program. */
		CHECK(game_script(g, "enter type:Q") == 0 &&
		    g->paused == 'Q');
		CHECK(game_script(g, "type:y wait:40") == 0 && g->quit);
		game_free(g);
	}
	/* The name starts as "Incognito": Enter keeps it, Backspace does
	 * nothing until a letter replaces it; 11 letters at most. */
	g = play("eng", "enter enter enter left enter right enter enter bs");
	if (g != NULL) {
		CHECK(strcmp(g->career.name, "Incognito") == 0);
		CHECK(game_script(g, "type:ABCDEFGHIJKLM") == 0 &&
		    strcmp(g->career.name, "ABCDEFGHIJK") == 0);
		game_free(g);
	}
	g = play("eng", "enter enter enter left enter right enter enter enter "
	    "enter");
	if (g != NULL) {
		CHECK(g->started && strcmp(g->sim.player.name, "Incognito") == 0);
		game_free(g);
	}
	/* Return to DOS. */
	g = play("eng", "enter down down enter");
	if (g != NULL) {
		CHECK(g->quit);
		game_free(g);
	}
	/* Pointer: clicking the second era picks 1600. */
	g = play("eng", "enter enter");
	if (g != NULL && g->options.noptions == 6) {
		const struct text_option *o = &g->options.option[1];
		char step[64];

		snprintf(step, sizeof(step), "click:%d,%d", o->x + 2,
		    o->y + o->h / 2);
		CHECK(game_script(g, step) == 0);
		CHECK(g->career.era == 1);
		game_free(g);
	} else if (g != NULL) {
		CHECK(!"era screen shows six options");
		game_free(g);
	}
}

static void
test_languages(void)
{
	struct game *g;

	g = play("fre", "enter enter down down down down enter left enter "
	    "right enter enter type:Surcouf enter enter");
	if (g != NULL) {
		CHECK(g->started && g->sim.era_year == 1660);
		game_free(g);
	}
	g = play("ger", "enter");
	if (g != NULL) {
		/* A bad script step is reported, not ignored. */
		CHECK(game_script(g, "enter dance") == -1);
		CHECK(strstr(g->err, "dance") != NULL);
		game_free(g);
	}
}

/* Save in slot 3 after two months, then load it in a new game. */
static void
test_saves(void)
{
	struct game *g;
	struct jbuf before = { 0 }, after = { 0 };
	char path[600], desc[128], err[256];

	/* At sea the cabin cannot save; back in town, in slot 4. */
	g = play_in(saves_tmp, "eng", "enter enter down down down down enter "
	    "down enter right enter enter type:Kidd enter enter esc enter enter click:60,300");
	if (g == NULL)
		return;
	g->sim.encounters = 0;
	CHECK(game_script(g, "wait:100 esc") == 0 &&
	    g->music_request == MUS_SILENCE);	/* the cabin cuts the music */
	CHECK(game_script(g, "wait:40 click:560,170") == 0 &&
	    g->screen == &screen_cabin);	/* Save game is off at sea */
	CHECK(g->started && g->sim.day >= 1 && g->sim.fleet.ticks >= 100);
	g->sim.player_town = g->sim.fleet.leaving >= 0 ?
	    g->sim.fleet.leaving : 0;		/* as if back in town */
	CHECK(game_script(g, "click:560,170 down down down enter") == 0);
	CHECK(strncmp(g->msg, "Kidd, English Buccaneer, ", 25) == 0);
	sim_to_json(&g->sim, &before);
	game_free(g);
	save_slot_path(saves_tmp, 3, path, sizeof(path));
	CHECK(save_read(path, NULL, desc, sizeof(desc), err, sizeof(err)) == 0);
	CHECK(strstr(desc, "Kidd") != NULL);

	/* Welcome: "Continue a saved game", then slot 4 (index 3); empty
	 * slots are skipped, so "down" from slot 1 lands on it. */
	g = play_in(saves_tmp, "eng", "enter down enter down enter");
	if (g == NULL)
		return;
	CHECK(g->started);
	sim_to_json(&g->sim, &after);
	CHECK(before.s != NULL && after.s != NULL &&
	    strcmp(before.s, after.s) == 0);
	CHECK(strcmp(g->sim.player.name, "Kidd") == 0);
	game_free(g);
	jb_free(&before);
	jb_free(&after);

	/* A file that is not a save is refused. */
	save_slot_path(saves_tmp, 5, path, sizeof(path));
	{
		FILE *fp = fopen(path, "w");

		if (fp != NULL) {
			fputs("{\"format\":\"other\"}", fp);
			fclose(fp);
		}
	}
	CHECK(save_read(path, NULL, desc, sizeof(desc), err, sizeof(err)) == -1);
	remove(path);
	save_slot_path(saves_tmp, 3, path, sizeof(path));
	remove(path);
}

/* Exit game quits without a record; retiring records the career. */
static void
test_hall_of_fame(void)
{
	struct game *g;
	struct hof h;
	struct career_end end;
	char path[600], err[256];
	int k;
	static const char *career = "enter enter down down down down enter "
	    "down enter right enter enter type:Anne enter enter esc enter enter click:60,300 wait:40 esc ";

	hof_path(saves_tmp, path, sizeof(path));
	remove(path);
	g = play_in(saves_tmp, "eng", career);	/* to the cabin */
	if (g == NULL)
		return;
	/* The cabin's Exit game: "Quit (Y/N)"; N stays, Y quits. */
	CHECK(game_script(g, "click:400,180") == 0 && g->quit_prompt);
	CHECK(game_script(g, "type:n") == 0 && !g->quit_prompt &&
	    g->screen == &screen_cabin);
	CHECK(game_script(g, "click:400,180 type:o") == 0 && g->quit_prompt);
	CHECK(game_script(g, "type:y wait:40") == 0 &&
	    !g->started && g->screen == &screen_welcome);
	game_free(g);
	/* In French, O (Oui) quits as well. */
	g = play_in(saves_tmp, "fre", career);
	if (g == NULL)
		return;
	CHECK(game_script(g, "click:400,180") == 0 && g->quit_prompt);
	CHECK(game_script(g, "type:o wait:40") == 0 &&
	    !g->started && g->screen == &screen_welcome);
	game_free(g);
	CHECK(hof_load(path, &h, err, sizeof(err)) != 0 || h.count == 0);

	/* Retiring: the further adventures, the summary, the verdict,
	 * "Remain in retirement", then the Hall of Fame. */
	g = play_in(saves_tmp, "eng", career);
	if (g == NULL)
		return;
	g->sim.fortune = 20000;
	g->sim.rank[NATION_ENGLAND] = 3;
	g->end_forced = 0;
	game_goto(g, &screen_ending);
	CHECK(game_script(g, "wait:40") == 0 && g->screen == &screen_ending);
	for (k = 0; k < 6; k++)
		CHECK(game_script(g, "enter") == 0);
	CHECK(g->end_page == 6);			/* the lure of the sea */
	CHECK(game_script(g, "enter wait:40") == 0 &&
	    g->screen == &screen_retire);
	sim_career_end(&g->sim, &end);
	CHECK(end.points == 20000 / 10 + 3 * 500 +
	    (long)g->sim.reputation * 200);
	CHECK(game_script(g, "enter") == 0 && g->hof_new == 0);
	game_free(g);
	CHECK(hof_load(path, &h, err, sizeof(err)) == 0 && h.count == 1);
	CHECK(strcmp(h.entry[0].name, "Anne") == 0 &&
	    h.entry[0].score == end.score &&
	    h.entry[0].rating == hof_rating(end.score));
	remove(path);
}

/* Sailing: the ship leaves port, time passes, steering turns it. */
static void
test_sailing(void)
{
	struct game *g;
	int y0, h0;

	g = play_in(saves_tmp, "eng", "enter enter down down down down "
	    "enter down enter right enter enter type:Low enter enter esc "
	    "enter enter click:60,300 wait:40 esc");	/* at Port Royale */
	if (g == NULL)
		return;
	CHECK(g->started && g->world != NULL && g->sim.world == g->world);
	/* The fleet starts outside the harbour, bows away from the town
	 * (town field 28); half a turn to starboard, then on that way. */
	CHECK(world_terrain(g->world, g->sim.fleet.x, g->sim.fleet.y) !=
	    TERRAIN_LAND);
	g->sim.encounters = 0;	/* encounters have their own test */
	CHECK(game_script(g, "enter") == 0);	/* set sail */
	h0 = g->sim.fleet.heading;
	CHECK(game_script(g, "hold:right:16") == 0);
	CHECK(g->sim.fleet.heading == ((h0 + 0x80) & 0xFF));
	y0 = g->sim.fleet.y;
	CHECK(game_script(g, "wait:64") == 0);
	CHECK(g->sim.fleet.ticks >= 64 && g->sim.day >= 1);
	/* Southward headings (0x40..0xC0) sail down the map. */
	CHECK(g->sim.fleet.heading > 0x40 && g->sim.fleet.heading < 0xC0 ?
	    g->sim.fleet.y > y0 : g->sim.fleet.y < y0);
	/* Between two passes the ship is drawn moving on, the rules
	 * still. */
	{
		uint8_t *was = malloc(FB_W * FB_H);
		uint32_t ticks;

		CHECK(was != NULL);
		g->sail_fast = 0;
		g->smooth = 1;
		game_tick(g);
		while (g->sail_wait != 0)
			game_tick(g);
		ticks = g->sim.fleet.ticks;
		game_draw(g);
		if (was != NULL)
			memcpy(was, g->fb.px, FB_W * FB_H);
		game_tick(g);
		game_tick(g);
		game_tick(g);
		game_tick(g);
		game_draw(g);
		CHECK(g->sim.fleet.ticks == ticks);
		CHECK(was != NULL && memcmp(was, g->fb.px, FB_W * FB_H) != 0);
		/* Without --smooth, the original's steps. */
		g->smooth = 0;
		while (g->sail_wait != 0)
			game_tick(g);
		game_draw(g);
		if (was != NULL)
			memcpy(was, g->fb.px, FB_W * FB_H);
		game_tick(g);
		game_tick(g);
		game_draw(g);
		CHECK(was != NULL && memcmp(was, g->fb.px, FB_W * FB_H) == 0);
		free(was);
	}
	/* P pauses (VA 0x2e91): the rules stand still under the "Game
	 * Paused" plaque, other keys are dropped, and P goes on. */
	{
		uint8_t *was = malloc(FB_W * FB_H);
		uint32_t ticks = g->sim.fleet.ticks;
		int h = g->sim.fleet.heading;

		CHECK(was != NULL);
		CHECK(game_script(g, "type:p wait:64 hold:right:16 esc") == 0 &&
		    g->paused && g->screen == &screen_sail);
		CHECK(g->sim.fleet.ticks == ticks &&
		    g->sim.fleet.heading == h && g->turn == 0);
		game_draw(g);
		if (was != NULL)
			memcpy(was, g->fb.px, FB_W * FB_H);
		CHECK(game_script(g, "type:P") == 0 && !g->paused);
		game_draw(g);
		CHECK(was != NULL && memcmp(was, g->fb.px, FB_W * FB_H) != 0);
		CHECK(game_script(g, "wait:64") == 0 &&
		    g->sim.fleet.ticks > ticks);
		free(was);
	}
	/* Q asks "Quit (Y/N)" (VA 0x2db9), holding the game like P; P is
	 * dropped there, and N or Escape go on (not to the cabin). */
	{
		uint32_t ticks = g->sim.fleet.ticks;

		CHECK(game_script(g, "type:q wait:64 type:p") == 0 &&
		    g->paused == 'Q' && g->sim.fleet.ticks == ticks);
		CHECK(game_script(g, "type:n") == 0 && !g->paused);
		CHECK(game_script(g, "type:Q esc") == 0 && !g->paused &&
		    g->screen != &screen_cabin);
	}
	/* V steps the sound level (VA 0x2f38): 1 stops the music and
	 * keeps tunes off, 2 the FM effects too; the digitised ones play
	 * on; back to 0, tunes play again. */
	CHECK(g->sound_level == 0);
	CHECK(game_script(g, "type:v") == 0 && g->sound_level == 1 &&
	    g->music_request == MUS_STOP);
	game_music(g, MUS_TITLE);
	game_fm(g, FM_SWING);
	CHECK(g->music_request == MUS_STOP && g->fm_request == FM_SWING);
	CHECK(game_script(g, "type:V") == 0 && g->sound_level == 2);
	g->fm_request = 0;
	g->sfx_pending = 0;
	game_fm(g, FM_SWING);
	g->sfx_packed |= 1u << SFX_HAIL;
	game_sfx(g, SFX_HAIL);
	CHECK(g->fm_request == 0 && g->sfx_pending == (1u << SFX_HAIL));
	CHECK(game_script(g, "type:v") == 0 && g->sound_level == 0);
	game_music(g, MUS_TITLE);
	CHECK(g->music_request == MUS_TITLE);
	CHECK(game_script(g, "esc") == 0);	/* back to the menu */
	game_free(g);
}

/*
 * The cabin's L (VA 0xc6bb): the language menu, Escape back unchanged;
 * French reloads the texts and the plaques, English brings them back.
 */
static void
test_language(void)
{
	struct game *g;
	char eng[64];

	g = play_in(saves_tmp, "eng", "enter enter down down down down "
	    "enter down enter right enter enter type:Ann enter enter esc "
	    "enter enter click:60,300 wait:40 esc ");	/* to the cabin */
	if (g == NULL)
		return;
	CHECK(g->screen == &screen_cabin && g->misc.count > 4);
	snprintf(eng, sizeof(eng), "%s", game_str(g, 274));	/* "Yes" */
	CHECK(game_script(g, "type:l wait:40") == 0 &&
	    g->screen == &screen_language);
	CHECK(game_script(g, "esc wait:40") == 0 &&
	    g->screen == &screen_cabin && strcmp(g->lang, "eng") == 0);
	CHECK(game_script(g, "type:L wait:40 down enter wait:40") == 0 &&
	    g->screen == &screen_cabin && strcmp(g->lang, "fre") == 0);
	CHECK(strcmp(game_str(g, 274), eng) != 0 && g->misc.count > 4);
	CHECK(game_script(g, "type:l wait:40 enter wait:40") == 0 &&
	    strcmp(g->lang, "eng") == 0 &&
	    strcmp(game_str(g, 274), eng) == 0);
	/* The original's menu has three options: a fourth Down wraps. */
	CHECK(game_script(g, "type:l wait:40 down down down enter "
	    "wait:40") == 0 && strcmp(g->lang, "eng") == 0);
	/* A language no pack has: the old one is kept. */
	CHECK(game_set_lang(g, "xyz") == -1 &&
	    strcmp(g->lang, "eng") == 0 &&
	    strcmp(game_str(g, 274), eng) == 0);
	game_free(g);
}

/* The tunes and sounds a pack can replace. */
static void
test_tune_ids(void)
{
	CHECK(strcmp(game_tune_id(MUS_TITLE), "music/title") == 0);
	CHECK(strcmp(game_tune_id(MUS_HARBOUR), "music/harbour") == 0);
	CHECK(game_tune_id(MUS_STOP) == NULL && game_tune_id(26) == NULL);
	/* The sounds: the digitised effects and the FM ones. */
	CHECK(strcmp(game_sfx_id(SFX_BROADSIDE), "sound/broadside") == 0);
	CHECK(strcmp(game_sfx_id(SFX_LAST_MAN), "sound/last-man") == 0);
	CHECK(game_sfx_id(DIGITAL_EFFECTS) == NULL);
	CHECK(strcmp(game_fm_id(FM_SWING), "sound/swing") == 0);
	CHECK(strcmp(game_fm_id(FM_ROW), "sound/row") == 0);
	CHECK(game_fm_id(FM_BROADSIDE) == NULL);	/* the digitised one */
	/* A pack's recording makes an effect playable without DIGITAL. */
	{
		struct game g;

		memset(&g, 0, sizeof(g));
		CHECK(!game_has_sfx(&g, SFX_HAIL));
		g.sfx_packed = 1u << SFX_HAIL;
		CHECK(game_has_sfx(&g, SFX_HAIL) && !game_has_sfx(&g, SFX_PARRY));
		game_sfx(&g, SFX_HAIL);
		CHECK(g.sfx_pending == (1u << SFX_HAIL));
	}
}

/* The original's debug mode (--debug): the keys at sea, and a right
 * click on the cabin's map moving the ship. */
static void
test_debug_mode(void)
{
	struct game *g;
	int food;

	g = play("eng", "enter enter down down down down enter down enter "
	    "right enter enter type:Lo enter enter esc enter enter click:60,300 "
	    "wait:40");
	if (g == NULL)
		return;
	CHECK(g->screen == &screen_sail);
	/* Off by default: the keys do nothing. */
	g->sim.encounters = 0;
	CHECK(game_script(g, "type:er") == 0 && g->sim.encounters == 0 &&
	    !g->sim.invulnerable);
	g->debug = 1;
	CHECK(game_script(g, "type:r") == 0 && g->sim.invulnerable);
	CHECK(game_script(g, "type:F") == 0 && !g->sim.invulnerable);
	/* E brings sightings back, N stops them again before a sail can
	 * be seen (one step: no time passes between the keys). */
	CHECK(game_script(g, "type:En") == 0 && g->sim.encounters == 0 &&
	    g->screen == &screen_sail);
	g->sim.encounters = 0;
	CHECK(game_script(g, "type:E") == 0 && g->sim.encounters == 1);
	g->sim.encounters = 0;
	memset(&g->sim.encounter, 0, sizeof(g->sim.encounter));
	game_goto(g, &screen_sail);
	/* The map: right click at (300, 200), then DONE: cell 400, 266,
	 * 10 more food. */
	food = g->sim.fleet.food;
	CHECK(game_script(g, "wait:40 esc wait:40 click:200,400 wait:40") ==
	    0 && g->screen == &screen_citymap);
	CHECK(game_script(g, "rclick:300,200 click:400,335 wait:40") == 0 &&
	    g->screen == &screen_cabin);
	CHECK(g->sim.fleet.x == 400 * 16 && g->sim.fleet.y == 266 * 16 &&
	    g->sim.fleet.food == food + 10);
	game_free(g);
}

/* An encounter on the real map: sighted, identified, hailed. */
static void
test_encounter_screens(void)
{
	struct game *g;
	int i;

	g = play("eng", "enter enter down down down down enter down enter "
	    "right enter enter type:Lo enter enter esc enter enter click:60,300");
	if (g == NULL)
		return;
	/* Sail until a sail is sighted. */
	g->sim.encounters = 1;
	for (i = 0; i < 200 && g->sim.encounter.stage == ENC_NONE; i++)
		CHECK(game_script(g, "wait:32") == 0);
	CHECK(g->sim.encounter.stage == ENC_SIGHTED);
	CHECK(game_script(g, "wait:40") == 0 &&
	    g->sfx_last == SFX_SAIL_HO &&	/* "Sail ho!" */
	    g->music_request == MUS_SAIL_HO);
	CHECK(game_script(g, "enter") == 0);	/* investigate */
	CHECK(g->sim.encounter.stage == ENC_IDENTIFIED);
	if (!g->sim.encounter.hostile) {
		/* Peaceful: investigate again: her colours over the
		 * spyglass's sky, no see-through green left; then hail. */
		CHECK(game_script(g, "enter wait:4") == 0 &&
		    g->sim.encounter.stage == ENC_COLOURS && g->flag_composed);
		{
			int x, y, green = 0;
			uint8_t key = fb_nearest(&g->fb, 0, 255, 0);

			for (y = 10; y < 210; y++)
				for (x = 220; x < 420; x++)
					green += g->fb.px[y * FB_W + x] == key &&
					    g->fb.pal[key * 3 + 1] == 255 &&
					    g->fb.pal[key * 3] == 0;
			CHECK(green == 0);
		}
		CHECK(game_script(g, "down enter") == 0);
		CHECK(g->sim.encounter.stage == ENC_NONE &&
		    g->enc_result == RES_NEWS);
	} else {
		/* A hunter: close for battle. */
		CHECK(game_script(g, "enter enter") == 0);
		CHECK(g->sim.encounter.stage == ENC_NONE &&
		    g->sim.battle.active && g->screen == &screen_battle);
		game_free(g);
		return;
	}
	CHECK(game_script(g, "enter wait:10") == 0);	/* back to sea */
	game_free(g);
}

/* The encounter screen at the current stage, drawn into "out". */
static void
encounter_page(struct game *g, uint8_t *out)
{
	game_switch(g, &screen_encounter);
	game_draw(g);
	memcpy(out, g->fb.px, sizeof(g->fb.px));
}

/* The hail's news page with the log as it is, drawn into "out". */
static void
news_page(struct game *g, uint8_t *out)
{
	memset(&g->sim.encounter, 0, sizeof(g->sim.encounter));
	g->sim.encounter.stage = ENC_COLOURS;
	g->sim.encounter.type = 1;
	g->sim.encounter.nation = NATION_ENGLAND;
	g->sim.encounter.town = -1;
	g->sim.standing[NATION_ENGLAND] = 0;
	game_switch(g, &screen_encounter);
	CHECK(game_script(g, "down enter") == 0 && g->enc_result == RES_NEWS);
	game_draw(g);
	memcpy(out, g->fb.px, sizeof(g->fb.px));
}

/* What the encounter's pages say and how her flag waves. */
static void
test_encounter_pages(void)
{
	static uint8_t a[FB_W * FB_H], b[FB_W * FB_H];
	struct sim_encounter *e;
	struct game *g;
	int i, w, y, last, len, bad, changes, same;

	g = play("eng", "enter enter down down down down enter down enter "
	    "right enter enter type:Lo enter enter esc enter enter click:60,300");
	if (g == NULL)
		return;
	e = &g->sim.encounter;
	CHECK(strcmp(game_word(g, 71, 5), "unknown") == 0);
	/* "Sail ho!" more than 12 cells from the nearest town: unknown
	 * waters; nearer, the town's nation. */
	g->sim.player.difficulty = 0;
	memset(e, 0, sizeof(*e));
	e->stage = ENC_SIGHTED;
	e->town = 0;
	e->dist = 12;
	encounter_page(g, a);
	e->dist = 5;
	encounter_page(g, b);
	CHECK(memcmp(a, b, sizeof(a)) == 0);
	e->dist = 13;
	encounter_page(g, b);
	CHECK(memcmp(a, b, sizeof(a)) != 0);
	encounter_page(g, a);
	e->dist = 40;
	encounter_page(g, b);
	CHECK(memcmp(a, b, sizeof(a)) == 0);

	/* The hunted pirate hails with his name (CSTR 121) even when he
	 * does not chase; another pirate shows her colours (CSTR 122). */
	memset(e, 0, sizeof(*e));
	e->stage = ENC_COLOURS;
	e->type = 1;
	e->nation = NATION_PIRATE;
	e->role = 4;
	e->town = 0;
	e->captain = 2 << 3 | 1 << 6;
	e->named = 0;
	encounter_page(g, a);
	e->named = 1;
	encounter_page(g, b);
	CHECK(memcmp(a, b, sizeof(a)) != 0);

	/* The flag: a frame lasts 2w + 2 ticks, give or take 1. */
	for (w = 0; w < 3; w++) {
		e->named = 0;
		e->captain = 0;
		e->nation = NATION_ENGLAND;
		e->wind_strength = w;
		encounter_page(g, a);
		last = 0;
		bad = changes = 0;
		for (i = 1; i < 400; i++) {
			game_tick(g);
			game_draw(g);
			same = 1;
			for (y = 10; y < 210 && same; y++)
				same = memcmp(a + y * FB_W + 220,
				    g->fb.px + y * FB_W + 220, 200) == 0;
			if (same)
				continue;
			memcpy(a, g->fb.px, sizeof(a));
			len = i - last;
			last = i;
			if (changes++ > 0)	/* the first follows the fade */
				bad += len < 2 * w + 1 || len > 2 * w + 3;
		}
		CHECK(changes > 400 / (2 * w + 3) - 2 && bad == 0);
	}

	/* The hail's page: "(no entries)" when nothing is told, even with
	 * the player's own deeds in the log; the items up to x 300. */
	g->sim.nnews = 0;
	news_page(g, a);
	g->sim.nnews = 1;
	g->sim.news[0].code = NEWS_PLUNDERED - 1;
	g->sim.news[0].date = g->sim.year * 360;
	news_page(g, b);
	CHECK(memcmp(a, b, sizeof(a)) == 0);
	for (i = 0; i < 6; i++) {	/* lines that reach x 300 */
		g->sim.news[i].code = NEWS_PEACE;
		g->sim.news[i].a = NATION_ENGLAND;
		g->sim.news[i].b = i % 2 ? NATION_HOLLAND : NATION_ENGLAND;
		g->sim.news[i].date = g->sim.year * 360 + i;
	}
	g->sim.nnews = 6;
	news_page(g, b);
	for (y = 100, bad = 0; y < 440; y++)
		bad += memcmp(a + y * FB_W + 300, b + y * FB_W + 300, 20) != 0;
	CHECK(bad == 0);
	for (y = 100, same = 1; y < 440 && same; y++)
		same = memcmp(a + y * FB_W + 20, b + y * FB_W + 20, 280) == 0;
	CHECK(!same);
	game_free(g);
}

/* A sea battle: fight, see the outcome, take the prize, sail on. */
/* After a capture: leave her cargo board, answer her sailors. */
static void
after_capture(struct game *g)
{
	int i;

	for (i = 0; i < 4; i++)
		if (g->screen == &screen_merchant)
			CHECK(game_script(g, "click:320,450 wait:40") == 0);
		else if (g->screen == &screen_volunteers)
			CHECK(game_script(g, "down enter wait:40") == 0);
}

/* The battle panel's speed box at row y, drawn into "out" (60 x 23). */
static void
speed_box(struct game *g, int y, uint8_t *out)
{
	int r;

	game_draw(g);
	for (r = 0; r < 23; r++)
		memcpy(out + r * 60, &g->fb.px[(y - 5 + r) * FB_W + 580], 60);
}

/* The battle panel's status line at row y, drawn into "out" (140 x 14). */
static void
status_line(struct game *g, int y, uint8_t *out)
{
	int r;

	game_draw(g);
	for (r = 0; r < 14; r++)
		memcpy(out + r * 140, &g->fb.px[(y + r) * FB_W + 492], 140);
}

static void
test_battle_screen(void)
{
	struct game *g;
	int i;

	g = play("eng", "enter enter down down down down enter down enter "
	    "right enter enter type:Lo enter enter esc enter enter click:60,300");
	if (g == NULL)
		return;
	g->sim.encounters = 1;
	for (i = 0; i < 200 && g->sim.encounter.stage == ENC_NONE; i++)
		CHECK(game_script(g, "wait:32") == 0);
	CHECK(game_script(g, "enter enter") == 0);	/* investigate twice */
	if (g->sim.encounter.stage == ENC_COLOURS)
		CHECK(game_script(g, "enter") == 0);	/* attack */
	CHECK(g->screen == &screen_battle && g->sim.battle.active);
	/* --smooth: the drawn heading turns to a new one over frames. */
	{
		int h = g->sim.battle.you.heading;

		g->btl_heading[0] = (h - 16) & 255;
		CHECK(game_script(g, "wait:1") == 0 &&
		    g->sim.battle.you.heading == h &&
		    g->btl_heading[0] == ((h - 14) & 255));
		CHECK(game_script(g, "wait:20") == 0 &&
		    g->btl_heading[0] == g->sim.battle.you.heading);
	}
	/* The speed / 4 beside the panel's arrows (VA 0xf602, 0xf65c),
	 * over the picture; a fort shows none. */
	{
		struct sim_battle saved = g->sim.battle;
		uint8_t a[60 * 23], b[60 * 23], bg[60 * 23];
		int r;

		for (r = 0; r < 23; r++)
			memcpy(bg + r * 60, &g->bg.pixels[(393 + r) * g->bg.w +
			    580], 60);
		g->sim.battle.you.speed = 13;
		speed_box(g, 398, a);
		CHECK(memcmp(a, bg, sizeof(a)) != 0);
		g->sim.battle.you.speed = 15;
		speed_box(g, 398, b);
		CHECK(memcmp(a, b, sizeof(a)) == 0);
		g->sim.battle.you.speed = 16;
		speed_box(g, 398, b);
		CHECK(memcmp(a, b, sizeof(a)) != 0);
		g->sim.battle.foe.speed = 13;
		speed_box(g, 54, a);
		g->sim.battle.foe.speed = 16;
		speed_box(g, 54, b);
		CHECK(memcmp(a, b, sizeof(a)) != 0);
		g->sim.battle.fort = 1;
		speed_box(g, 54, a);
		g->sim.battle.foe.speed = 13;
		speed_box(g, 54, b);
		CHECK(memcmp(a, b, sizeof(a)) == 0);
		g->sim.battle = saved;
	}
	/* A new damage stage puts its word on the side's status line until
	 * the next status event (VA 0xebd7, 0xef06); a fort has no line. */
	{
		struct sim_battle saved = g->sim.battle;
		uint8_t a[140 * 14], b[140 * 14];

		CHECK(g->btl_hurt[0] == -1 && g->btl_hurt[1] == -1);
		status_line(g, 116, a);
		g->sim.battle.foe.damage = 16;
		g->sim.battle.you.damage = 32;
		CHECK(game_script(g, "wait:1") == 0 &&
		    g->btl_hurt[1] == 1 && g->btl_hurt[0] == 2);
		status_line(g, 116, b);
		CHECK(memcmp(a, b, sizeof(a)) != 0);
		CHECK(game_script(g, g->sim.battle.you.full_sails ? "down" :
		    "up") == 0 && game_script(g, "wait:1") == 0 &&
		    g->btl_hurt[0] == -1 && g->btl_hurt[1] == 1);
		g->sim.battle.fort = 1;
		CHECK(game_script(g, "wait:1") == 0 && g->btl_hurt[1] == -1);
		g->sim.battle = saved;
		g->btl_hurt[0] = g->btl_hurt[1] = -1;
		g->btl_hurt_seen[0] = saved.you.damage / 16;
		g->btl_hurt_seen[1] = saved.foe.damage / 16;
	}
	/* An escape: outrun.pcx and its text in the ink at (100, 300)
	 * (CSTR 197, 198). */
	{
		struct sim_battle saved = g->sim.battle;
		int x, y, inked = 0, before = 0;
		uint8_t ink;

		g->sim.battle.outcome = BAT_ESCAPED;
		g->sim.battle.decision = 2;
		CHECK(game_script(g, "wait:2") == 0 && g->btl_stage == 1);
		CHECK(g->bg.w == 640 && g->bg.h == 480);
		ink = fb_nearest(&g->fb, g->ink[0], g->ink[1], g->ink[2]);
		for (y = 300; y < 330; y++)
			for (x = 100; x < 560; x++) {
				inked += g->fb.px[y * FB_W + x] == ink;
				before += g->bg.pixels[y * 640 + x] == ink;
			}
		CHECK(inked > before + 100);
		g->sim.battle = saved;
		game_switch(g, &screen_battle);
		CHECK(game_script(g, "wait:2") == 0 && g->btl_stage == 0);
	}
	/* With two ships, the one to command comes first. */
	{
		int n = g->sim.fleet.nships;

		g->sim.fleet.nships = 2;
		g->sim.fleet.ships[1] = 5;
		g->sim.battle.tick = 0;
		game_goto(g, &screen_battle);
		/* The battle never shows before the choice. */
		for (i = 0; i < 80; i++) {
			game_tick(g);
			game_draw(g);
			CHECK(g->screen != &screen_battle || g->fading != 1);
		}
		CHECK(g->screen == &screen_fleet_panel);
		/* SELECT does nothing until a ship is marked; the second
		 * slot, then SELECT. */
		CHECK(game_script(g, "click:540,450") == 0 &&
		    g->screen == &screen_fleet_panel);
		CHECK(game_script(g, "click:240,120") == 0 &&
		    g->panel_marked == 2);
		CHECK(game_script(g, "click:540,450 wait:40") == 0 &&
		    g->screen == &screen_battle && g->btl_stage == 0 &&
		    g->sim.fleet.ships[0] == 5 && g->sim.battle.you.type == 5);
		g->sim.fleet.nships = n;	/* the frigate fights on */
	}
	/* Raising sail takes 160 ticks, then full sails. */
	CHECK(game_script(g, "up") == 0 && g->sim.battle.you.sail_timer > 0 &&
	    !g->sim.battle.you.full_sails);
	CHECK(game_script(g, "wait:200") == 0);	/* full sails */
	CHECK(g->sim.battle.you.full_sails || g->sim.battle.outcome != 0);
	/* Steer toward her and fire until it is decided. */
	g->sfx_last = -1;
	for (i = 0; i < 300 && g->btl_stage == 0; i++)
		CHECK(game_script(g, "enter wait:16") == 0);
	CHECK(g->btl_stage == 1 && g->sim.battle.outcome != BAT_NONE);
	CHECK(g->sfx_last >= 0);		/* broadsides, a boarding */
	CHECK(game_script(g, "enter") == 0);
	if (g->btl_stage == 2) {		/* a prize: keep her */
		CHECK(game_script(g, "enter wait:40") == 0);
		after_capture(g);
	}
	if (g->screen == &screen_prison)	/* sunk, the last ship */
		CHECK(g->sim.jail.cause == JAIL_MAROONED);
	else
		CHECK(g->screen == &screen_sail);
	CHECK(!g->sim.battle.active && g->sim.fleet.nships >= 1);
	game_free(g);
}

/* Press up to three keys in order, release them; the pending move. */
static int
sword_keys(struct game *g, int k1, int k2, int k3)
{
	const int keys[3] = { k1, k2, k3 };
	struct ui_event ev;
	int i, got;

	memset(&ev, 0, sizeof(ev));
	g->sim.sword.input = -1;
	for (i = 0; i < 3 && keys[i] != 0; i++) {
		ev.type = EV_KEY;
		ev.key = keys[i];
		game_event(g, &ev);
	}
	got = g->sim.sword.input;
	for (i = 0; i < 3 && keys[i] != 0; i++) {
		ev.type = EV_KEYUP;
		ev.key = keys[i];
		game_event(g, &ev);
	}
	CHECK(g->sw_held == 0);
	return (got);
}

/* Boarding: the picture, the weapon, the keys, the fight to its end. */
static void
test_sword_screen(void)
{
	struct game *g;
	int i;

	g = play("eng", "enter enter down down down down enter down enter "
	    "right enter enter type:Lo enter enter esc enter enter click:60,300");
	if (g == NULL)
		return;
	g->sim.encounters = 1;
	for (i = 0; i < 200 && g->sim.encounter.stage == ENC_NONE; i++)
		CHECK(game_script(g, "wait:32") == 0);
	CHECK(game_script(g, "enter enter") == 0);
	if (g->sim.encounter.stage == ENC_COLOURS)
		CHECK(game_script(g, "enter") == 0);
	CHECK(g->screen == &screen_battle);
	/* Bring the ships together: boarding. */
	g->sim.battle.foe.x = g->sim.battle.you.x;
	g->sim.battle.foe.y = g->sim.battle.you.y;
	CHECK(game_script(g, "wait:30") == 0);
	CHECK(g->screen == &screen_sword && g->sw_stage == 0);
	CHECK(game_script(g, "enter") == 0);
	CHECK(g->sw_stage == 1 && g->selected == 2);	/* cutlass first */
	CHECK(game_script(g, "up up enter") == 0);	/* the rapier */
	CHECK(g->sw_stage == 2 && g->sim.sword.active &&
	    g->sim.sword.you.weapon == 0);
	CHECK(g->sw_parts[0][0].count == 16 && g->sw_parts[1][1].count == 16);
	/* Keys: toward the opponent lunges, with Shift slashes; up and
	 * down pick the line; away steps back. */
	CHECK(sword_keys(g, UI_KEY_LEFT, 0, 0) == SWM_LUNGE);
	CHECK(sword_keys(g, UI_KEY_UP, UI_KEY_LEFT, 0) == SWM_LUNGE + 2);
	CHECK(sword_keys(g, UI_KEY_SHIFT, UI_KEY_LEFT, 0) == SWM_SLASH);
	CHECK(sword_keys(g, UI_KEY_SHIFT, UI_KEY_DOWN, UI_KEY_LEFT) ==
	    SWM_SLASH + 1);
	CHECK(sword_keys(g, UI_KEY_RIGHT, 0, 0) == SWM_BACK);
	CHECK(sword_keys(g, UI_KEY_UP, 0, 0) == SWM_GUARD + 2);
	/* The keypad's corners are diagonals: 7 lunges high, Shift+1
	 * slashes low, 3 steps back low. */
	CHECK(sword_keys(g, UI_KEY_UP_LEFT, 0, 0) == SWM_LUNGE + 2);
	CHECK(sword_keys(g, UI_KEY_SHIFT, UI_KEY_DOWN_LEFT, 0) ==
	    SWM_SLASH + 1);
	CHECK(sword_keys(g, UI_KEY_DOWN_RIGHT, 0, 0) == SWM_BACK + 1);
	/* The mouse, anywhere: left of the pad lunges, the right button
	 * slashes, above it high, right of it steps back. */
	CHECK(game_script(g, "rclick:100,100") == 0 &&
	    g->sim.sword.input == SWM_SLASH + 2);
	CHECK(game_script(g, "click:600,400") == 0 &&
	    g->sim.sword.input == SWM_BACK + 1);
	CHECK(game_script(g, "click:320,323") == 0 &&
	    g->sim.sword.input == SWM_GUARD);
	/* Fight until it is decided. */
	for (i = 0; i < 600 && g->sim.sword.result == SWORD_NONE; i++)
		CHECK(game_script(g, i % 2 ? "up left wait:10" :
		    "down left wait:10") == 0);
	CHECK(g->sim.sword.result != SWORD_NONE);
	CHECK(game_script(g, "wait:30") == 0);
	if (g->sim.sword.result == SWORD_WON) {
		CHECK(g->screen == &screen_battle && g->btl_stage == 2);
		CHECK(game_script(g, "enter wait:30") == 0);	/* keep her */
		CHECK(g->sim.fleet.nships == 2);
		after_capture(g);
	} else if (g->sim.sword.result == SWORD_LOST) {
		/* Prison, "in the ship": the defeat, the months, the
		 * way back. */
		CHECK(g->screen == &screen_prison &&
		    g->sim.jail.cause == JAIL_SHIP);
		CHECK(game_script(g, "enter enter enter wait:30") == 0);
	}
	CHECK(g->screen == &screen_sail && !g->sim.battle.active &&
	    !g->sim.sword.active);
	game_free(g);
}

/* The sword pointer: drawn where the mouse is, once it moved. */
static void
test_pointer(void)
{
	struct game *g;
	int x, y, drawn = 0;

	g = play("eng", "enter wait:60");
	if (g == NULL)
		return;
	CHECK(g->cursor.w == 24 && g->cursor.h == 27);
	CHECK(!g->pointer_seen);
	CHECK(game_script(g, "move:300,250 wait:1") == 0);
	CHECK(g->pointer_seen && g->pointer_x == 300 && g->pointer_y == 250);
	game_draw(g);
	for (y = 0; y < g->cursor.h; y++)
		for (x = 0; x < g->cursor.w; x++)
			if (g->cursor.mask[y * g->cursor.w + x])
				drawn += g->fb.px[(250 + y) * FB_W + 300 + x] ==
				    g->cursor.pixels[y * g->cursor.w + x];
	CHECK(drawn > 50);
	/* The menu's rows span the screen, as in the original. */
	CHECK(g->screen == &screen_welcome);
	CHECK(game_script(g, "move:20,365 wait:1") == 0 && g->selected == 2);
	CHECK(game_script(g, "move:620,395 wait:1") == 0 && g->selected == 3);
	game_free(g);
}

/* The intro's five steps, then the menu; a key skips it. */
static void
test_intro(void)
{
	struct game *g;
	int i, seen = 0, last = -1, dissolved = 0;

	g = play("eng", "");
	if (g == NULL)
		return;
	for (i = 0; i < 2000 && g->screen == &screen_intro; i++) {
		if (g->intro_step != last) {
			CHECK(g->intro_step == last + 1);
			last = g->intro_step;
			seen++;
		}
		if (g->intro_shown != NULL && g->intro_left == 0)
			dissolved = 1;
		CHECK(game_script(g, "wait:1") == 0);
	}
	CHECK(seen == 5 && dissolved);
	CHECK(game_script(g, "wait:40") == 0);
	CHECK(g->screen == &screen_credits && g->intro_title == NULL);
	/* The credits: six blocks, each about 3.2 s, then the menu. */
	for (i = 0, last = -1, seen = 0; i < 2000 &&
	    g->screen == &screen_credits; i++) {
		if (g->cred_block != last && g->cred_block < 6) {
			CHECK(g->cred_block == last + 1);
			last = g->cred_block;
			seen++;
		}
		CHECK(game_script(g, "wait:1") == 0);
	}
	CHECK(seen == 6 && i > 6 * 180);
	CHECK(game_script(g, "wait:40") == 0);
	CHECK(g->screen == &screen_welcome && g->credits.pixels == NULL);
	game_free(g);

	/* Keys: the intro ends; the credits jump to the last block, then
	 * end. */
	g = play("eng", "wait:200 enter wait:40");
	if (g == NULL)
		return;
	CHECK(g->screen == &screen_credits && g->cred_block == 0);
	CHECK(game_script(g, "wait:20 enter wait:5") == 0);
	CHECK(g->cred_block == 5);
	CHECK(game_script(g, "enter wait:40") == 0);
	CHECK(g->screen == &screen_welcome);
	game_free(g);

	/* Only Enter, Escape or a click skip (VA 0x2246).  "Present" and
	 * the lead before the title only skip ahead (VA 0x34fb, 0x3781). */
	g = play("eng", "wait:200 down type:x wait:40");
	if (g == NULL)
		return;
	CHECK(g->screen == &screen_intro);
	for (i = 0; i < 2000 && (g->intro_step < 3 || g->fading != 0); i++)
		CHECK(game_script(g, "wait:1") == 0);
	CHECK(g->intro_step == 3);
	CHECK(game_script(g, "enter") == 0);
	for (i = 0; i < 100 && (g->intro_step != 4 || g->fading != 0); i++)
		CHECK(game_script(g, "wait:1") == 0);
	CHECK(g->screen == &screen_intro && g->intro_step == 4);
	CHECK(g->intro_iter < 2000);
	CHECK(game_script(g, "enter wait:1") == 0);
	CHECK(g->screen == &screen_intro && g->intro_iter >= 2000);
	CHECK(game_script(g, "enter wait:40") == 0);
	CHECK(g->screen == &screen_credits);
	game_free(g);
}

/* A town: the buildings, their labels, City Information, leaving. */
static void
test_town(void)
{
	struct game *g;
	const struct sim_town *t;
	int i;

	g = play_in(saves_tmp, "eng", "enter enter down down down down "
	    "enter down enter right enter enter type:Lo enter enter esc "
	    "enter enter click:60,300 wait:30");
	if (g == NULL)
		return;
	CHECK(g->screen == &screen_sail);
	g->sim.encounters = 0;
	t = home_port(g);
	into_town(g, t);
	CHECK(game_script(g, "wait:40") == 0);
	/* The arrival menu; sailing into the home port. */
	CHECK(g->screen == &screen_arrive && g->sim.player_town == -1);
	CHECK(g->sfx_last == SFX_HARBOUR &&	/* into the harbour */
	    g->music_request == MUS_HARBOUR);
	CHECK(game_script(g, "enter wait:40") == 0);
	CHECK(g->screen == &screen_town && g->sim.player_town >= 0);
	t = &g->sim.towns[g->sim.player_town];
	CHECK((t->flags & 0x80) && !(t->flags & 0x20));
	/* Pointing at the bank labels it; off the buildings, the name. */
	CHECK(game_script(g, "move:540,320 wait:1") == 0 &&
	    g->town_spot == 2);
	CHECK(game_script(g, "move:300,150 wait:1") == 0 &&
	    g->town_spot == -1);
	/* The keyboard skips a closed governor. */
	g->sim.visit_flags |= VISIT_GOVERNOR;
	g->town_key = 6;
	CHECK(game_script(g, "right") == 0 && g->town_key == 1);
	CHECK(game_script(g, "left") == 0 && g->town_key == 6);
	g->sim.visit_flags = 0;
	/* City Information, then back. */
	CHECK(game_script(g, "click:300,40 wait:40") == 0);
	CHECK(g->screen == &screen_cityinfo);
	CHECK(game_script(g, "enter wait:40") == 0 &&
	    g->screen == &screen_town);
	/* Leave: back at sea, out of the town. */
	CHECK(game_script(g, "click:60,300 wait:40") == 0);
	CHECK(g->screen == &screen_sail && g->sim.player_town == -1);
	for (i = 0; i < 20; i++)
		CHECK(game_script(g, "wait:1") == 0);
	CHECK(g->screen == &screen_sail);	/* not straight back in */
	game_free(g);
}

/* The shipwright: repair one ship, sell another, the refusals. */
static void
test_shipwright(void)
{
	struct game *g;
	const struct sim_town *t;
	long gold;

	g = play("eng", "enter enter down down down down enter down enter "
	    "right enter enter type:Lo enter enter esc enter enter click:60,300 wait:30");
	if (g == NULL)
		return;
	g->sim.encounters = 0;
	t = home_port(g);
	CHECK(t->nation != NATION_SPAIN);
	into_town(g, t);
	g->sim.fleet.nships = 2;
	g->sim.fleet.ships[1] = 4;
	g->sim.fleet.damage[1] = 0;
	g->sim.fleet.damage[0] = 40;	/* the sloop: level 2 */
	CHECK(game_script(g, "wait:40 enter wait:40 click:60,400 wait:40") == 0);
	CHECK(g->screen == &screen_shipwright && g->wright_stage == 0);
	/* Repair: the list, the sloop, paid. */
	gold = g->sim.fleet.gold;
	CHECK(game_script(g, "down enter") == 0 && g->wright_stage == 1);
	CHECK(game_script(g, "enter") == 0);
	CHECK(g->sim.fleet.damage[0] == 0 && g->sim.fleet.gold == gold - 200);
	CHECK(g->wright_stage == 0);	/* nothing left to repair */
	/* Repairing again: "nothing wrong". */
	CHECK(game_script(g, "down enter") == 0 && g->wright_stage == 3);
	CHECK(game_script(g, "enter") == 0 && g->wright_stage == 0);
	/* Sell the merchantman: 50 x 5 x 10 gold. */
	gold = g->sim.fleet.gold;
	CHECK(game_script(g, "down down enter") == 0 && g->wright_stage == 2);
	CHECK(game_script(g, "down enter") == 0);
	CHECK(g->sim.fleet.nships == 1 && g->sim.fleet.gold == gold + 2500);
	/* The only ship is not for sale. */
	CHECK(game_script(g, "down down enter") == 0 && g->wright_stage == 3);
	CHECK(game_script(g, "enter") == 0);
	/* Parting company: back in town. */
	CHECK(game_script(g, "enter wait:40") == 0 &&
	    g->screen == &screen_town);
	/* Refused: the note stays up while the town fades in, no menu. */
	game_goto(g, &screen_shipwright);
	CHECK(game_script(g, "wait:40") == 0);
	g->wright_stage = 3;
	g->wright_note = 256;		/* "We do not trade with pirates!" */
	CHECK(game_script(g, "enter wait:1") == 0 && g->wright_stage == 3);
	CHECK(game_script(g, "wait:40") == 0 && g->screen == &screen_town);
	game_free(g);
}

/* The merchant: the greeting, the board, buying and selling, leaving. */
static void
test_merchant(void)
{
	struct game *g;
	const struct sim_town *t;
	long gold;
	int price, food;

	g = play("eng", "enter enter down down down down enter down enter "
	    "right enter enter type:Lo enter enter esc enter enter click:60,300 wait:30");
	if (g == NULL)
		return;
	g->sim.encounters = 0;
	t = home_port(g);
	into_town(g, t);
	CHECK(game_script(g, "wait:40 enter wait:40 click:400,320 wait:40") == 0);
	CHECK(g->screen == &screen_merchant && g->shop_stage == 0);
	CHECK(g->sim.market.open && g->shop_spr[0].count == 93);
	g->fm_request = 0;
	CHECK(game_script(g, "enter") == 0 && g->shop_stage == 1);
	CHECK(g->fm_request == FM_ROW);		/* the board opens */
	/* Right buys a unit of the selected good; Left sells it back. */
	gold = g->sim.fleet.gold;
	food = g->sim.fleet.food;
	price = g->sim.market.price[0];
	CHECK(game_script(g, "right right") == 0 && g->fm_request == FM_TRADE);
	CHECK(g->sim.fleet.food == food + 2 &&
	    g->sim.fleet.gold == gold - 2 * price && g->shop_buying);
	CHECK(game_script(g, "left") == 0 && g->sim.fleet.food == food + 1 &&
	    !g->shop_buying);
	/* A row's arrow selects goods without trading; the player's flag
	 * buys, the merchant's tag sells (none held). */
	CHECK(game_script(g, "click:455,175") == 0 && g->selected == 1 &&
	    g->shop_buying && g->sim.fleet.goods == 0);
	CHECK(game_script(g, "click:40,30") == 0 && g->sim.fleet.goods == 0);
	CHECK(game_script(g, "click:600,30") == 0 && g->sim.fleet.goods == 1);
	/* Take all: the hold filled with goods, while the gold lasts. */
	CHECK(game_script(g, "click:360,112") == 0 &&
	    (sim_hold_space(&g->sim) == 0 || g->sim.market.stock[1] == 0 ||
	    g->sim.fleet.gold <= g->sim.market.price[1]));
	/* O.K.: back in town, the shop closed (his daughter may come in
	 * first, one time in ten: talking ends it). */
	CHECK(game_script(g, "click:320,440 wait:40") == 0);
	if (g->screen == &screen_merdaugh)
		CHECK(game_script(g, "enter wait:40") == 0);
	CHECK(g->screen == &screen_town && !g->sim.market.open);
	/* His daughter: a proposal, the debt paid, married. */
	g->sim.married = 0;
	g->sim.fortune = 1000;
	g->sim.fleet.gold = 1000;
	g->sim.towns[g->sim.player_town].family = 0;
	g->merdaugh = 296;
	game_goto(g, &screen_merdaugh);
	CHECK(game_script(g, "wait:40 down enter") == 0 &&
	    g->merdaugh == 298);
	CHECK(game_script(g, "enter") == 0 && g->merdaugh == 299 &&
	    g->sim.married == 1 && g->sim.fleet.gold == 500);
	CHECK(game_script(g, "enter wait:40") == 0 &&
	    g->screen == &screen_town);
	game_free(g);
}

/* The tavern: recruiting, the bartender, the door. */
static void
test_tavern(void)
{
	struct game *g;
	const struct sim_town *t;
	int crew, men;

	g = play("eng", "enter enter down down down down enter down enter "
	    "right enter enter type:Lo enter enter esc enter enter click:60,300 wait:30");
	if (g == NULL)
		return;
	g->sim.encounters = 0;
	t = home_port(g);
	into_town(g, t);
	CHECK(game_script(g, "wait:40 enter wait:40 click:250,350 wait:40") == 0);
	CHECK(g->screen == &screen_tavern && g->tavern_spr.count == 3);
	CHECK(g->sim.recruits > 0);
	/* The seamen: sign them up; they are gone afterwards. */
	crew = g->sim.fleet.crew;
	men = g->sim.recruits;
	CHECK(game_script(g, "click:280,400 wait:40") == 0 &&
	    g->screen == &screen_recruit);
	CHECK(game_script(g, "enter wait:40") == 0);
	CHECK(g->screen == &screen_tavern && g->sim.fleet.crew == crew + men);
	CHECK(game_script(g, "click:280,400 wait:5") == 0 &&
	    g->screen == &screen_tavern);	/* nobody there now */
	/* The bartender's rumours, then back: with news, he stays. */
	sim_log_news(&g->sim, NEWS_WAR, NATION_SPAIN, NATION_FRANCE);
	CHECK(game_script(g, "click:110,250 wait:40") == 0 &&
	    g->screen == &screen_rumours);
	CHECK(game_script(g, "enter wait:40") == 0 &&
	    g->screen == &screen_tavern &&
	    (g->sim.tavern_spots & TAVERN_BAR));
	/* A villain passed through: with news, not a word of him. */
	g->sim.villains[2].active = 1;
	g->sim.villains[2].town = 3;
	g->sim.villains[2].trail[0] = g->sim.player_town;
	g->sim.villains[2].trail[1] = g->sim.villains[2].trail[2] =
	    g->sim.villains[2].trail[3] = -1;
	CHECK(game_script(g, "click:110,250 wait:40 enter wait:40") == 0 &&
	    g->screen == &screen_tavern);
	/* Without news, the gossip, then where the villain went; the only
	 * one, he is not asked about again; the bartender then leaves. */
	g->sim.nnews = 0;
	CHECK(game_script(g, "click:110,250 wait:40 enter wait:40") == 0 &&
	    g->screen == &screen_vnews && g->vnews_n == 1);
	CHECK(game_script(g, "enter") == 0 && g->vnews_said == 107 &&
	    g->vnews_where == 3);
	CHECK(game_script(g, "enter wait:40") == 0 &&
	    g->screen == &screen_tavern &&
	    !(g->sim.tavern_spots & TAVERN_BAR));
	/* Two villains: the question again after each, "No one?" ends
	 * it. */
	g->sim.tavern_spots |= TAVERN_BAR;
	g->sim.villains[1] = g->sim.villains[2];
	CHECK(game_script(g, "click:110,250 wait:40 enter wait:40") == 0 &&
	    g->screen == &screen_vnews && g->vnews_n == 2);
	CHECK(game_script(g, "enter") == 0 && g->vnews_said == 107);
	CHECK(game_script(g, "enter") == 0 && g->vnews_said == 0 &&
	    g->screen == &screen_vnews);
	CHECK(game_script(g, "down down enter wait:40") == 0 &&
	    g->screen == &screen_tavern);
	g->sim.villains[1].active = 0;
	/* A crew captured near the town tells the same, on crew.pcx,
	 * then the steps after the battle go on to the sea. */
	g->btl_news = g->sim.player_town;
	g->btl_landed = g->btl_hunted = 0;
	game_goto(g, &screen_battle_end);
	CHECK(game_script(g, "wait:40") == 0 &&
	    g->screen == &screen_vnews && g->vnews_source == 3);
	CHECK(game_script(g, "down enter wait:80") == 0 &&
	    g->screen == &screen_sail);
	g->sim.villains[2].active = 0;
	game_goto(g, &screen_tavern);
	CHECK(game_script(g, "wait:40") == 0 && g->screen == &screen_tavern);
	/* The keyboard skips the seamen now; the door leads out. */
	g->sim.tavern_spots |= TAVERN_BAR;
	g->tavern_key = 1;
	CHECK(game_script(g, "right") == 0 && g->tavern_key == 3);
	CHECK(game_script(g, "click:610,300 wait:40") == 0 &&
	    g->screen == &screen_town);
	game_free(g);
}

/* The governor: the news, the Letter of Marque, once a stay. */
static void
test_governor(void)
{
	struct game *g;
	const struct sim_town *t;
	long gold;

	g = play("eng", "enter enter down down down down enter down enter "
	    "right enter enter type:Lo enter enter esc enter enter click:60,300 wait:30");
	if (g == NULL)
		return;
	g->sim.encounters = 0;
	/* The English Buccaneer starts at standing 2 with England (the
	 * scenario's): back to 0 for the Letter of Marque. */
	CHECK(g->sim.standing[NATION_ENGLAND] == 2);
	g->sim.standing[NATION_ENGLAND] = 0;
	t = home_port(g);
	into_town(g, t);
	CHECK(game_script(g, "wait:40 enter wait:40 click:580,220 wait:40") == 0);
	CHECK(g->screen == &screen_governor && g->gov_stage == 0);
	CHECK(g->sim.visit_flags & VISIT_GOVERNOR);
	/* Standing 0 with England: a Letter of Marque for 800. */
	gold = g->sim.fleet.gold;
	CHECK(game_script(g, "enter") == 0 && g->gov_stage == 9);
	CHECK(game_script(g, "enter wait:40") == 0);
	CHECK(g->sim.fleet.gold == gold - 800 &&
	    g->sim.standing[NATION_ENGLAND] == 2);
	CHECK(g->screen == &screen_town);
	/* He will not receive the player again this stay. */
	CHECK(game_script(g, "click:580,220 wait:5") == 0 &&
	    g->screen == &screen_town);
	/* Another stay, standing 8 (no promotion at rank 2): offers come
	 * until one is drawn; accepting shows the task. */
	{
		int k, n, offered = 0;

		g->sim.rank[NATION_ENGLAND] = 3;
		for (k = 0; k < 30 && !offered; k++) {
			g->sim.visit_flags &= ~VISIT_GOVERNOR;
			g->sim.standing[NATION_ENGLAND] = 8;
			g->sim.mission.type = MISSION_NONE;
			CHECK(game_script(g, "click:580,220 wait:40 enter") ==
			    0);
			if (g->gov_stage == 3) {
				offered = 1;
				CHECK(game_script(g, "enter") == 0 &&
				    g->gov_stage == 4 &&
				    g->sim.mission.type > 0 &&
				    g->sim.mission.giver ==
				    g->sim.player_town);
			}
			for (n = 0; n < 20 && g->screen == &screen_governor;
			    n++)
				CHECK(game_script(g, "enter wait:40") == 0);
		}
		CHECK(offered);
	}
	/* A captain held: sooner or later a visit brings a ransom. */
	{
		int k, ransomed = 0;
		long before;

		g->sim.prisoners[3] = 1 << 6 | 2 << 3 | 4;
		for (k = 0; k < 30 && !ransomed; k++) {
			g->sim.visit_flags &= ~VISIT_GOVERNOR;
			g->sim.mission.type = MISSION_NONE;
			g->sim.standing[NATION_ENGLAND] = 0;
			int n;

			CHECK(game_script(g, "click:580,220 wait:40") == 0);
			for (n = 0; n < 20 && g->screen == &screen_governor &&
			    g->gov_stage != 12; n++)
				CHECK(game_script(g, "enter wait:2") == 0);
			if (g->screen == &screen_governor &&
			    g->gov_stage == 12) {
				before = g->sim.fleet.gold;
				CHECK(game_script(g, "enter") == 0 &&
				    g->sim.fleet.gold == before +
				    g->sim.ransom_gold &&
				    g->sim.prisoners[3] == 0);
				ransomed = 1;
			}
			for (n = 0; n < 20 && g->screen == &screen_governor;
			    n++)
				CHECK(game_script(g, "enter wait:40") == 0);
		}
		CHECK(ransomed);
	}
	/* Standing 4: his daughter instead of an offer; a proposal, the
	 * suitor beaten in the alley, the wedding. */
	{
		int n;
		int nat = g->sim.towns[g->sim.player_town].nation & 3;

		g->sim.visit_flags &= ~VISIT_GOVERNOR;
		g->sim.mission.type = MISSION_NONE;
		g->sim.standing[nat] = 4;
		g->sim.rank[nat] = 9;		/* no promotion left */
		g->sim.fortune = 4990;
		g->sim.married = 0;
		memset(g->sim.prisoners, 0, sizeof(g->sim.prisoners));
		g->sim.towns[g->sim.player_town].governor = 0;
		CHECK(game_script(g, "click:580,220 wait:40") == 0);
		for (n = 0; n < 20 && g->screen == &screen_governor &&
		    g->gov_stage != 13; n++)
			CHECK(game_script(g, "enter wait:2") == 0);
		CHECK(g->screen == &screen_governor && g->gov_stage == 13 &&
		    g->daughter.charm >= 4 &&
		    g->music_request == (g->daughter.niece ? MUS_NIECE :
		    MUS_DAUGHTER));
		CHECK(game_script(g, "down enter") == 0 &&
		    g->gov_stage == 15);
		CHECK(game_script(g, "enter wait:40") == 0 &&
		    g->screen == &screen_sword && g->sw_stage == 1);
		CHECK(game_script(g, "enter") == 0 && g->sw_stage == 2 &&
		    g->sim.sword.nation == -1);
		g->sim.sword.result = SWORD_WON;
		CHECK(game_script(g, "wait:40") == 0 &&
		    g->screen == &screen_governor && g->gov_stage == 16 &&
		    g->sim.married == 2);
		CHECK(game_script(g, "enter wait:40") == 0 &&
		    g->screen == &screen_town);
	}
	/* A hostage's news, then back to sea. */
	g->sim.hostage = 5 << 3 | 2;
	g->hostage_said = -1;
	g->selected = 0;
	game_goto(g, &screen_hostage);
	CHECK(game_script(g, "wait:40 enter") == 0 && g->hostage_said == 0 &&
	    g->sim.hostage == 0);
	CHECK(game_script(g, "enter wait:40") == 0 &&
	    g->screen == &screen_sail);
	game_free(g);
}

/* The career's story: 1680, English (script 20). */
static void
test_story(void)
{
	struct game *g;
	g = play("eng", "enter enter down down down down down enter wait:40 "
	    "down enter right enter enter enter enter wait:40");
	if (g == NULL)
		return;
	CHECK(g->started && g->screen == &screen_story);
	CHECK(g->story_text == 21 && g->story_waiting);	/* the uncle */
	/* "Click to continue": the plaque, after 420 idle ticks only. */
	CHECK(g->misc.count == 9);
	g->idle = 0;
	CHECK(game_script(g, "wait:410") == 0);
	game_draw(g);
	CHECK(g->fb.px[(449 + 15) * FB_W + 268 + 50] !=
	    g->misc.frames[7].pixels[15 * g->misc.frames[7].w + 50] ||
	    g->misc.frames[7].pixels[15 * g->misc.frames[7].w + 50] == 0);
	CHECK(game_script(g, "wait:20") == 0);
	game_draw(g);
	CHECK(g->fb.px[(449 + 15) * FB_W + 268 + 50] ==
	    g->misc.frames[7].pixels[15 * g->misc.frames[7].w + 50]);
	CHECK(game_script(g, "enter") == 0 && g->story_text == 22);
	CHECK(game_script(g, "enter") == 0 && g->story_text == 14);
	CHECK(game_script(g, "enter") == 0 && g->story_text == 15);
	CHECK(game_script(g, "enter wait:40") == 0);
	CHECK(g->screen == &screen_sword && g->sim.sword.active &&
	    g->sim.sword.kind == SWORD_STORY && g->sw_stage == 2);
	CHECK(g->sim.sword.you.men == 200 && g->sim.sword.foe.men == 20);
	CHECK(game_script(g, "left wait:30") == 0);
	/* --smooth: a fencer who moved slides there over the fight tick;
	 * without, he is already there. */
	{
		uint8_t *was = malloc(FB_W * FB_H);
		uint32_t counter;
		int smooth;

		CHECK(was != NULL);
		for (smooth = 1; smooth >= 0 && was != NULL; smooth--) {
			g->smooth = smooth;
			while (g->sw_wait != g->sw_period)
				game_tick(g);
			counter = g->sim.sword.counter;
			g->sw_from[1] = g->sim.sword.foe.x - 20;
			game_draw(g);
			memcpy(was, g->fb.px, FB_W * FB_H);
			game_tick(g);
			game_tick(g);
			game_tick(g);
			game_draw(g);
			CHECK(g->sim.sword.counter == counter);
			CHECK((memcmp(was, g->fb.px, FB_W * FB_H) != 0) ==
			    smooth);
		}
		free(was);
	}
	/* Win or lose, the story goes on. */
	g->sim.sword.result = SWORD_LOST;
	CHECK(game_script(g, "wait:40") == 0);
	CHECK(g->screen == &screen_story && g->story_text == 23);
	/* The sailing master's two pages, then the start town. */
	CHECK(game_script(g, "enter wait:40") == 0 &&
	    g->screen == &screen_master && g->master_page == 0);
	CHECK(game_script(g, "enter") == 0 && g->master_page == 1);
	CHECK(game_script(g, "enter wait:40") == 0 &&
	    g->screen == &screen_town &&
	    g->sim.player_town == g->sim.fleet.leaving);
	game_free(g);
}

/* On foot against a strong garrison: the land battle, then the assault
 * or the failure. */
static void
test_land_battle(void)
{
	struct game *g;
	struct sim_town *t;
	int town;

	g = play("eng", "enter enter down down down down enter down enter "
	    "right enter enter type:Lo enter enter esc enter enter "
	    "click:60,300 wait:30");
	if (g == NULL)
		return;
	g->sim.encounters = 0;
	town = (int)(home_port(g) - g->sim.towns);
	t = &g->sim.towns[town];
	t->soldiers = 20;
	t->forts = 2;
	g->sim.fleet.crew = 120;
	g->sim.fleet.ashore = 1;
	g->sim.fleet.leaving = town;	/* the town arrived at */
	g->arrive_stage = 0;
	game_goto(g, &screen_arrive);
	CHECK(game_script(g, "wait:40 down enter wait:40") == 0 &&
	    g->screen == &screen_land && g->sim.land.active);
	CHECK(g->sim.land.u[1].men == 60 && g->sim.land.u[2].men == 60);
	/* A pass every 36 frames. */
	CHECK(game_script(g, "wait:36") == 0 && g->sim.land.pass >= 1);
	/* --smooth: a unit that moved glides between passes; without, it
	 * stays put. */
	{
		uint8_t *was = malloc(FB_W * FB_H);
		uint32_t pass;
		int smooth, i;

		CHECK(was != NULL);
		for (smooth = 1; smooth >= 0 && was != NULL; smooth--) {
			g->smooth = smooth;
			while (g->land_wait != 0)
				game_tick(g);
			pass = g->sim.land.pass;
			g->land_from[1][0] = g->sim.land.u[1].x - 20;
			game_draw(g);
			memcpy(was, g->fb.px, FB_W * FB_H);
			for (i = 0; i < 18; i++)	/* half a pass */
				game_tick(g);
			game_draw(g);
			CHECK(g->sim.land.pass == pass);
			CHECK((memcmp(was, g->fb.px, FB_W * FB_H) != 0) ==
			    smooth);
		}
		free(was);
	}
	/* Won: on to the assault. */
	g->sim.land.result = 2;
	CHECK(game_script(g, "wait:80") == 0 && g->screen == &screen_assault);
	/* Lost: the failure, then to sea. */
	g->sim.fleet.crew = 120;
	sim_land_start(&g->sim, town);
	game_goto(g, &screen_land);
	CHECK(game_script(g, "wait:40") == 0);
	g->sim.land.result = 3;
	CHECK(game_script(g, "wait:40") == 0 && g->land_stage == 1);
	CHECK(game_script(g, "enter wait:40") == 0 &&
	    g->screen == &screen_sail);
	/* Lost with no one left: prison at once, without the failure. */
	g->sim.fleet.crew = 120;
	sim_land_start(&g->sim, town);
	game_goto(g, &screen_land);
	CHECK(game_script(g, "wait:40") == 0);
	g->sim.land.u[1].men = g->sim.land.u[2].men = 0;
	g->sim.land.u[3].men = 0;
	g->sim.land.result = 3;
	CHECK(game_script(g, "wait:40") == 0 &&
	    g->screen == &screen_prison && g->land_stage == 0);
	game_free(g);
}

/* Ashore: the question, Y, walking, the shipwright closed in town. */
static void
test_landing(void)
{
	struct game *g;
	int x, y, found = 0, i;
	size_t k;

	g = play("eng", "enter enter down down down down enter down enter "
	    "right enter enter type:Lo enter enter esc enter enter "
	    "click:60,300 wait:30");
	if (g == NULL)
		return;
	g->sim.encounters = 0;
	/* A shore facing north, far from the towns: sea at (x, y), land
	 * just above. */
	for (y = 2000; y < 6000 && !found; y += 16)
		for (x = 2000; x < 9000 && !found; x += 16) {
			if (world_terrain(g->world, x, y) != TERRAIN_SEA ||
			    world_terrain(g->world, x, y - 16) !=
			    TERRAIN_LAND)
				continue;
			found = 1;
			for (k = 0; k < g->sim.ntowns; k++)
				if (abs(g->sim.towns[k].x * 16 - x) < 100 &&
				    abs(g->sim.towns[k].y * 16 - y) < 100)
					found = 0;
			if (found) {
				g->sim.fleet.x = x;
				g->sim.fleet.y = y;
			}
		}
	CHECK(found);
	g->sim.fleet.heading = 0;
	g->sim.fleet.leaving = -1;
	for (i = 0; i < 40 && !g->ashore_prompt; i++)
		CHECK(game_script(g, "wait:4") == 0);
	CHECK(g->ashore_prompt && g->screen == &screen_sail);
	CHECK(game_script(g, "type:y") == 0 && !g->ashore_prompt &&
	    g->sim.fleet.ashore);
	y = g->sim.fleet.y;
	CHECK(game_script(g, "hold:up:20") == 0 && g->sim.fleet.y < y);
	/* The cabin, from Space as in the original: no saving outside a
	 * town; search on foot. */
	{
		struct ui_event ev = { 0 };

		ev.type = EV_CHAR;
		ev.ch = ' ';
		game_event(g, &ev);
	}
	CHECK(game_script(g, "wait:40") == 0 && g->screen == &screen_cabin);
	CHECK(game_script(g, "click:560,170") == 0 &&
	    g->screen == &screen_cabin);
	/* A whole map whose spot is right here: the maps' spot shows it,
	 * the search digs it up and returns to the march. */
	g->sim.maps[MAP_PIRATE].pieces = MAP_WHOLE | MAP_SHOWN;
	g->sim.maps[MAP_PIRATE].x = (int16_t)(g->sim.fleet.x / 16);
	g->sim.maps[MAP_PIRATE].y = (int16_t)(g->sim.fleet.y / 16);
	g->sim.maps[MAP_PIRATE].town = 2;
	CHECK(game_script(g, "click:400,280 wait:40") == 0 &&
	    g->screen == &screen_tmap && g->tmap_n == 1);
	CHECK(game_script(g, "enter wait:40") == 0 &&
	    g->screen == &screen_cabin);
	{
		long gold = g->sim.fleet.gold;

		CHECK(game_script(g, "click:280,200 wait:40") == 0 &&
		    g->screen == &screen_dig && g->dig_n == 1 &&
		    g->sim.fleet.gold >= gold + 5000 &&
		    g->sim.maps[MAP_PIRATE].pieces == 0);
	}
	CHECK(game_script(g, "enter wait:40") == 0 &&
	    g->screen == &screen_sail && g->sim.fleet.ashore);
	/* On foot, S searches without the cabin (VA 0x1b3df). */
	{
		int32_t day = g->sim.day;

		CHECK(game_script(g, "type:s wait:40") == 0 &&
		    g->screen == &screen_dig && g->dig_n == 0 &&
		    g->sim.day != day);
		CHECK(game_script(g, "enter wait:40") == 0 &&
		    g->screen == &screen_sail && g->sim.fleet.ashore);
	}
	/* Nothing left: the maps' spot is closed again. */
	CHECK(game_script(g, "esc wait:40") == 0 && g->screen == &screen_cabin);
	CHECK(game_script(g, "click:400,280") == 0 &&
	    g->screen == &screen_cabin);
	/* A town reached on foot: marching in, no ship in port. */
	g->sim.fleet.leaving = 1;
	g->sim.towns[1].forts = 0;
	g->arrive_stage = 0;
	game_goto(g, &screen_arrive);
	CHECK(game_script(g, "wait:40 enter wait:40") == 0 &&
	    g->screen == &screen_town);
	CHECK(game_script(g, "click:60,400 wait:40") == 0 &&
	    g->screen == &screen_town);	/* the shipwright is closed */
	/* Out on foot: on the town's own cell, still ashore. */
	CHECK(game_script(g, "click:60,300 wait:40") == 0 &&
	    g->screen == &screen_sail && g->sim.fleet.ashore &&
	    g->sim.fleet.x == g->sim.towns[1].x * 16 + 8);
	game_free(g);
}

/* The tavern's people: names when pointed at, the traveller's news,
 * the map seller, the barmaid. */
static void
test_tavern_people(void)
{
	struct game *g;
	long gold;

	g = play("eng", "enter enter down down down down enter down enter "
	    "right enter enter type:Lo enter enter esc enter enter wait:40 "
	    "click:250,350 wait:40");
	if (g == NULL)
		return;
	CHECK(g->screen == &screen_tavern);
	CHECK(game_script(g, "move:450,250") == 0 && g->tavern_spot == 4);
	/* The traveller, here for sure. */
	g->sim.tavern_spots |= TAVERN_NEWS | TAVERN_MAPS;
	g->sim.news_town = 2;
	g->sim.news_price = 70;
	g->sim.fleet.gold = 1000;
	CHECK(game_script(g, "click:180,290 wait:40") == 0 &&
	    g->screen == &screen_traveller);
	CHECK(game_script(g, "enter wait:40") == 0 &&
	    g->screen == &screen_cityinfo && g->info_town == 2 &&
	    g->sim.fleet.gold == 930);
	CHECK(game_script(g, "enter wait:40") == 0 &&
	    g->screen == &screen_tavern && g->info_town == -1);
	/* The map seller: 50 gold. */
	CHECK(game_script(g, "click:40,350 wait:40") == 0 &&
	    g->screen == &screen_mapman);
	gold = g->sim.fleet.gold;
	CHECK(game_script(g, "enter") == 0 && g->sim.fleet.gold == gold - 50 &&
	    (g->sim.maps[MAP_PIRATE].pieces & MAP_SHOWN));
	CHECK(game_script(g, "enter wait:40") == 0 &&
	    g->screen == &screen_tavern);
	/* The barmaid: some company. */
	g->sim.towns[g->sim.player_town].barmaid = 2;
	CHECK(game_script(g, "click:450,250 wait:40") == 0 &&
	    g->screen == &screen_barmaid && g->talk == 1 &&
	    g->music_request == MUS_BARMAID);
	CHECK(game_script(g, "down down enter") == 0 && g->talk == 2 &&
	    g->sim.towns[g->sim.player_town].barmaid == 3);
	CHECK(game_script(g, "enter wait:40") == 0 &&
	    g->screen == &screen_tavern);
	/* The mystery man: a villain hiding here, fought in the alley;
	 * beaten, he gives a piece of the relative's map. */
	g->sim.villains[1].active = 1;
	g->sim.villains[1].town = g->sim.player_town;
	g->sim.tavern_spots |= TAVERN_MYSTERY;
	/* He stays while the villain is still here (VA 0x119de). */
	CHECK(game_script(g, "click:380,260 wait:40") == 0 &&
	    g->screen == &screen_villain && g->villain == 1 &&
	    (g->sim.tavern_spots & TAVERN_MYSTERY));
	CHECK(game_script(g, "enter wait:40") == 0 &&
	    g->screen == &screen_sword && g->sw_stage == 1);
	CHECK(game_script(g, "enter") == 0 && g->sw_stage == 2 &&
	    g->sim.villain_duel == 1 && g->sim.sword.nation == -2);
	g->sim.sword.result = SWORD_WON;
	CHECK(game_script(g, "wait:40") == 0 &&
	    g->screen == &screen_villain && g->villain_stage == 1 &&
	    !g->sim.villains[1].active &&
	    g->sim.maps[MAP_RELATIVE].pieces != 0);
	CHECK(game_script(g, "enter wait:40") == 0 &&
	    g->screen == &screen_tmap);
	CHECK(game_script(g, "enter wait:40") == 0 &&
	    g->screen == &screen_tavern &&
	    !(g->sim.tavern_spots & TAVERN_MYSTERY));	/* nobody left */
	/* The contact for a letter: delivered. */
	g->sim.mission.type = MISSION_LETTER;
	g->sim.mission.giver = 2;
	g->sim.mission.dest = g->sim.player_town;
	g->sim.tavern_spots |= TAVERN_MYSTERY;
	CHECK(game_script(g, "click:380,260 wait:40") == 0 &&
	    g->screen == &screen_contact &&
	    g->sim.mission.type == -MISSION_LETTER);
	CHECK(game_script(g, "enter wait:40") == 0 &&
	    g->screen == &screen_tavern &&
	    !(g->sim.tavern_spots & TAVERN_MYSTERY));
	/* The map seller and the traveller leave for the stay, turned
	 * down too (VA 0x1191b, 0x11935). */
	g->sim.tavern_spots |= TAVERN_NEWS | TAVERN_MAPS;
	CHECK(game_script(g, "click:40,350 wait:40") == 0 &&
	    g->screen == &screen_mapman);
	CHECK(game_script(g, "down enter wait:40") == 0 &&
	    g->screen == &screen_tavern &&
	    !(g->sim.tavern_spots & TAVERN_MAPS));
	CHECK(game_script(g, "click:180,290 wait:40") == 0 &&
	    g->screen == &screen_traveller);
	CHECK(game_script(g, "down enter wait:40") == 0 &&
	    g->screen == &screen_tavern &&
	    !(g->sim.tavern_spots & TAVERN_NEWS));
	game_free(g);
}

/* The pixels of the screen's ink in a box, less those of the picture. */
static int
inked(struct game *g, int x0, int y0, int x1, int y1)
{
	uint8_t ink = fb_nearest(&g->fb, g->ink[0], g->ink[1], g->ink[2]);
	int x, y, n = 0;

	for (y = y0; y < y1; y++)
		for (x = x0; x < x1; x++)
			n += (g->fb.px[y * FB_W + x] == ink) -
			    (g->bg.pixels[y * g->bg.w + x] == ink);
	return (n);
}

/*
 * Time in town: three days a pass through the menu, none back from the
 * city information or the cabin (VA 0x18771).  And the town's pages
 * checked against the original on the way.
 */
static void
test_town_time(void)
{
	struct game *g;
	struct image want;
	struct ui_event space = { 0 };
	struct sim_town *t;
	int32_t day;
	int a, b, k;

	space.type = EV_CHAR;
	space.ch = ' ';
	g = play("eng", "enter enter down down down down enter down enter "
	    "right enter enter type:Lo enter enter esc enter enter wait:40");
	if (g == NULL)
		return;
	CHECK(g->screen == &screen_town);
	t = &g->sim.towns[g->sim.player_town];
	day = g->sim.day;
	/* City information, then back: no time. */
	CHECK(game_script(g, "click:300,40 wait:40") == 0 &&
	    g->screen == &screen_cityinfo);
	CHECK(game_script(g, "enter wait:40") == 0 &&
	    g->screen == &screen_town && g->sim.day == day);
	/* On foot, the town is seen by land (VA 0x19c8a). */
	if (!(t->flags & TOWN_INLAND)) {
		g->sim.fleet.ashore = 1;
		CHECK(game_script(g, "click:300,40 wait:40") == 0 &&
		    g->screen == &screen_cityinfo);
		CHECK(pic_load(packs, "pic/arr_land", &want) == 0);
		CHECK(g->bg.pixels != NULL && want.pixels != NULL &&
		    g->bg.w == want.w && g->bg.h == want.h &&
		    memcmp(g->bg.pixels, want.pixels,
		    (size_t)want.w * want.h) == 0);
		image_free(&want);
		CHECK(game_script(g, "enter wait:40") == 0);
		g->sim.fleet.ashore = 0;
	}
	/* The cabin: Space opens it, Space continues (VA 0xb3a5); no
	 * time either. */
	game_event(g, &space);
	CHECK(game_script(g, "wait:40") == 0 && g->screen == &screen_cabin);
	game_event(g, &space);
	CHECK(game_script(g, "wait:40") == 0 && g->screen == &screen_town &&
	    g->sim.day == day);
	/* The Ship's Log without entries: its title alone (VA 0x1fce0). */
	g->sim.nnews = 0;
	game_goto(g, &screen_logbook);
	CHECK(game_script(g, "wait:40 enter wait:1") == 0 &&
	    g->log_page == 1);
	CHECK(inked(g, 140, 100, 520, 140) <= 0);
	/* A town taken for a nation names the town, then the nation
	 * (VA 0x1e407). */
	g->sim.nnews = 1;
	g->sim.news[0].code = NEWS_TAKEN;
	g->sim.news[0].date = g->sim.year * 360;
	g->sim.news[0].a = g->sim.player_town;
	g->sim.news[0].b = NATION_SPAIN;
	game_goto(g, &screen_logbook);
	CHECK(game_script(g, "wait:40 enter wait:1") == 0 &&
	    g->log_page == 1);
	a = inked(g, 140, 100, 520, 140);
	g->sim.news[0].b = NATION_HOLLAND;
	CHECK(game_script(g, "wait:1") == 0);
	b = inked(g, 140, 100, 520, 140);
	CHECK(a > 0 && b > 0 && a != b);
	g->sim.nnews = 0;
	game_goto(g, &screen_town);
	CHECK(game_script(g, "wait:40") == 0);
	day = g->sim.day;
	/* Back from a building: three days. */
	CHECK(game_script(g, "click:60,400 wait:40") == 0 &&
	    g->screen == &screen_shipwright);
	/* His notes at (100, 300) up to x 540 (VA 0x21932). */
	g->wright_stage = 3;
	g->wright_note = 282;		/* "I see nothing wrong" */
	CHECK(game_script(g, "wait:1") == 0);
	CHECK(inked(g, 40, 300, 98, 340) <= 0 &&
	    inked(g, 100, 300, 540, 340) > 50);
	CHECK(game_script(g, "enter enter wait:40") == 0 &&
	    g->screen == &screen_town && g->sim.day == day + 3);
	/* The recruits are offered by the rank with the town's nation
	 * (VA 0x10f2f). */
	g->sim.recruits = 10;
	g->sim.rank[t->nation] = 0;
	game_goto(g, &screen_recruit);
	CHECK(game_script(g, "wait:40") == 0);
	a = inked(g, 60, 300, 560, 400);
	g->sim.rank[t->nation] = 6;
	CHECK(game_script(g, "wait:1") == 0);
	b = inked(g, 60, 300, 560, 400);
	CHECK(a != b);
	/* The governor's news: the call to arms only with enemies (VA
	 * 0x1208e). */
	g->sim.standing[t->nation] = 2;
	for (k = 0; k < 4; k++)
		g->sim.relations[t->nation][k] = 0;
	g->sim.visit_flags &= ~VISIT_GOVERNOR;
	game_goto(g, &screen_governor);
	CHECK(game_script(g, "wait:40") == 0 &&
	    g->screen == &screen_governor && g->music_request == MUS_NEWS);
	g->sim.relations[t->nation][NATION_SPAIN] = -1;
	g->sim.visit_flags &= ~VISIT_GOVERNOR;
	game_switch(g, &screen_governor);
	CHECK(g->music_request == MUS_NEWS_TITLED);
	game_free(g);
}

/* Storming a town without forts: the assault, the sack, the loot. */
static void
test_assault(void)
{
	struct game *g;
	struct sim_town *t;
	long gold;
	int town;

	g = play("eng", "enter enter down down down down enter down enter "
	    "right enter enter type:Lo enter enter esc enter enter "
	    "click:60,300 wait:30");
	if (g == NULL)
		return;
	g->sim.encounters = 0;
	town = (int)(home_port(g) - g->sim.towns);
	t = &g->sim.towns[town];
	t->forts = 0;
	t->gold = 200;
	t->citizens = 1;
	g->sim.fleet.crew = 100;
	g->sim.heat = 0;
	into_town(g, t);
	/* The treasure fleet is in port: 10 more men guard the town, and
	 * the sack takes the fleet. */
	g->sim.fleet_town = town;
	g->sim.train_town = -1;
	t->soldiers = 5;
	CHECK(game_script(g, "wait:40") == 0 && g->screen == &screen_arrive);
	CHECK(game_script(g, "down enter wait:40") == 0 &&
	    g->screen == &screen_assault && t->soldiers == 15);
	CHECK(game_script(g, "enter wait:40") == 0 &&
	    g->screen == &screen_sword && g->sw_stage == 1);
	CHECK(game_script(g, "enter wait:2") == 0 && g->sw_stage == 2 &&
	    g->sim.sword.scene == 0 && g->sim.sword.foe.men ==
	    t->soldiers * 10 + 5);
	g->sim.sword.result = SWORD_WON;
	gold = g->sim.fleet.gold;
	CHECK(game_script(g, "wait:40") == 0 && g->screen == &screen_sack &&
	    g->sack.gold == (200L * 1500 / 15 +
	    g->world->treasure_ship[5] * 20) * 10 &&
	    g->sim.fleet.gold == gold + g->sack.gold &&
	    (g->sack.convoys & CONVOY_FLEET) && g->sim.fleet_town == -1);
	/* The crew of 100 can hold a town of one citizen: a governor. */
	CHECK(g->sack.conquest);
	CHECK(game_script(g, "enter") == 0 && g->sack_stage == 5);
	CHECK(game_script(g, "enter") == 0 && g->sack_stage == 2);
	CHECK(game_script(g, "down down down enter wait:40") == 0 &&
	    t->nation == 3 && g->screen == &screen_merchant && g->looting);
	/* The loot for nothing, then away to sea. */
	g->sack.goods[0] = 0;
	CHECK(game_script(g, "down right wait:2") == 0);
	CHECK(game_script(g, "esc wait:40") == 0 &&
	    g->screen == &screen_sail && !g->looting &&
	    g->sim.fleet.gold == gold + g->sack.gold);
	/* With forts: the fort battle first; landing by the fort leads to
	 * the assault, the garrison set from the battle. */
	t->forts = 2;
	t->soldiers = 9;
	into_town(g, t);
	CHECK(game_script(g, "wait:40 down enter wait:40") == 0 &&
	    g->screen == &screen_battle && g->sim.battle.fort);
	g->sim.battle.foe.crew = 31;
	g->sim.battle.outcome = BAT_LANDED;
	CHECK(game_script(g, "wait:40") == 0 && g->screen == &screen_assault &&
	    t->soldiers == 5 + 3 && g->assault_town == town);
	game_free(g);
}

/* The captain's cabin: labels, the logbook, the cave, practice. */
static void
test_cabin(void)
{
	struct game *g;
	uint8_t mark;
	int practice;

	g = play("eng", "enter enter down down down down enter down enter "
	    "right enter enter type:Lo enter enter esc enter enter wait:40");
	if (g == NULL)
		return;
	CHECK(g->screen == &screen_town);
	{
		struct ui_event ev = { 0 };

		ev.type = EV_CHAR;	/* Space: the cabin */
		ev.ch = ' ';
		game_event(g, &ev);
	}
	CHECK(game_script(g, "wait:40") == 0 &&
	    g->screen == &screen_cabin && g->cabin_spot == -1);
	/* Pointing at the logbook names it; the disabled search does not
	 * answer. */
	CHECK(game_script(g, "move:190,200") == 0 && g->cabin_spot == 3);
	CHECK(game_script(g, "move:280,200") == 0 && g->cabin_spot == -1);
	CHECK(game_script(g, "click:190,200 wait:40") == 0 &&
	    g->screen == &screen_logbook && g->log_page == 0);
	/* The log book: the Ship's Log (leaves turned by the arrows, the
	 * bookmark leaves), the status pages, Ship Status on the fleet
	 * panel, Exit. */
	CHECK(game_script(g, "enter") == 0 && g->log_page == 1);
	CHECK(game_script(g, "click:300,200") == 0 && g->log_page == 1);
	CHECK(game_script(g, "click:50,420") == 0 && g->log_page == 0);
	CHECK(game_script(g, "down enter") == 0 && g->log_page == 2);
	CHECK(game_script(g, "enter") == 0 && g->log_page == 0);
	CHECK(game_script(g, "down down enter") == 0 && g->log_page == 3);
	CHECK(game_script(g, "esc") == 0 && g->log_page == 0);
	CHECK(game_script(g, "down down down enter wait:40") == 0 &&
	    g->screen == &screen_fleet_panel && g->panel_view);
	CHECK(game_script(g, "click:300,200 wait:40") == 0 &&
	    g->screen == &screen_logbook && !g->panel_view);
	CHECK(game_script(g, "down down down down enter") == 0 &&
	    g->log_page == 5);
	CHECK(game_script(g, "enter") == 0 && g->log_page == 0);
	CHECK(game_script(g, "esc wait:40") == 0 &&
	    g->screen == &screen_cabin);
	/* The Map of the Caribbean: a town picked in the list, the list
	 * scrolled, DONE. */
	CHECK(game_script(g, "click:200,400 wait:40") == 0 &&
	    g->screen == &screen_citymap && g->cm_scroll == 0);
	CHECK(game_script(g, "click:560,85") == 0 && g->cm_sel == 2);
	CHECK(game_script(g, "click:560,465") == 0 && g->cm_scroll == 1);
	CHECK(game_script(g, "click:560,10") == 0 && g->cm_scroll == 0);
	CHECK(game_script(g, "down") == 0 && g->cm_sel == 3);
	CHECK(game_script(g, "click:400,335 wait:40") == 0 &&
	    g->screen == &screen_cabin);
	CHECK(game_script(g, "click:60,300 wait:40") == 0 &&
	    g->screen == &screen_cave);
	CHECK(game_script(g, "enter wait:40") == 0 &&
	    g->screen == &screen_cabin);
	/* Practice: the weapon on the deck, a fight, then 5 more. */
	practice = g->sim.practice;
	CHECK(game_script(g, "click:110,120 wait:40") == 0 &&
	    g->screen == &screen_sword && g->sw_stage == 1);
	/* The weapons left to right: rapier, cutlass, longsword; the keys
	 * stop at the ends, a click off the blades does nothing. */
	CHECK(g->selected == 2 && g->sw_named);
	CHECK(game_script(g, "left left") == 0 && g->selected == 0);
	CHECK(game_script(g, "right right right") == 0 && g->selected == 1);
	CHECK(game_script(g, "move:600,20") == 0 && !g->sw_named);
	CHECK(game_script(g, "click:600,20") == 0 && g->sw_stage == 1);
	CHECK(game_script(g, "move:418,200") == 0 && g->selected == 0 &&
	    g->sw_named);
	CHECK(game_script(g, "click:418,200") == 0 && g->sw_stage == 2 &&
	    g->sim.sword.you.weapon == 0);
	CHECK(g->sim.sword.kind == SWORD_PRACTICE &&
	    g->sim.sword.you.men == 200 && g->sim.sword.title == 5);
	/* A mark on the player's first frame: loading it again would
	 * erase it. */
	CHECK(g->sw_parts[0][0].count > 0 &&
	    g->sw_parts[0][0].banks[0].count > 0);
	mark = g->sw_parts[0][0].banks[0].frames[0].pixels[0] ^= 0x5a;
	g->sim.sword.result = SWORD_LOST;
	CHECK(game_script(g, "wait:40") == 0 &&
	    g->screen == &screen_cabin && g->sim.practice == practice + 5);
	/* Another practice, the same weapon: the fighters' layers are kept
	 * from the last fight, not loaded again. */
	CHECK(game_script(g, "click:110,120 wait:40 click:418,200") == 0 &&
	    g->sw_stage == 2 &&
	    g->sw_parts[0][0].banks[0].frames[0].pixels[0] == mark);
	g->sim.sword.result = SWORD_LOST;
	CHECK(game_script(g, "wait:40") == 0 && g->screen == &screen_cabin);
	/* Escape continues the travels: back in town. */
	CHECK(game_script(g, "esc wait:40") == 0 &&
	    g->screen == &screen_town);
	game_free(g);
}

/* Sailing passes: one in 5 frames, every frame after T; T again. */
static void
test_pace(void)
{
	struct game *g;
	int32_t t0;

	g = play("eng", "enter enter down down down down enter down enter "
	    "right enter enter type:Lo enter enter esc enter enter "
	    "click:60,300 wait:30");
	if (g == NULL)
		return;
	CHECK(g->screen == &screen_sail);
	g->sim.encounters = 0;
	g->sail_fast = 0;
	t0 = g->sim.fleet.ticks;
	CHECK(game_script(g, "wait:50") == 0 &&
	    g->sim.fleet.ticks - t0 == 10);
	CHECK(game_script(g, "type:t") == 0 && g->sail_fast);
	t0 = g->sim.fleet.ticks;
	CHECK(game_script(g, "wait:50") == 0 &&
	    g->sim.fleet.ticks - t0 == 50);
	CHECK(game_script(g, "type:T") == 0 && !g->sail_fast);
	game_free(g);
}

/* Reaching a hostile port: the fort fires; sneaking in, a guard. */
static void
test_arrival(void)
{
	struct game *g;
	struct sim_town *t;
	int i, fired = 0, caught = 0;

	g = play("eng", "enter enter down down down down enter down enter "
	    "right enter enter type:Lo enter enter esc enter enter "
	    "click:60,300 wait:30");
	if (g == NULL)
		return;
	g->sim.encounters = 0;
	t = home_port(g);
	for (i = 0; i < 20 && (!fired || !caught); i++) {
		into_town(g, t);
		g->sim.player_town = -1;
		t->forts = 2;
		t->standing = (uint16_t)(int16_t)-20;
		g->sim.reputation = 5;
		CHECK(game_script(g, "wait:40") == 0 &&
		    g->screen == &screen_arrive);
		if (!fired) {
			/* Sailing in under the guns. */
			CHECK(game_script(g, "enter") == 0);
			if (g->screen == &screen_town ||
			    g->sim.player_town >= 0) {
				/* A lucky draw let the ship in: out, again. */
				CHECK(game_script(g, "wait:40 click:60,300 "
				    "wait:40") == 0);
				while (g->screen == &screen_deserted)
					CHECK(game_script(g, "enter wait:40") ==
					    0);
				continue;
			}
			CHECK(g->arrive_stage == 1 && g->screen ==
			    &screen_arrive && g->sim.player_town == -1);
			CHECK(game_script(g, "enter wait:40") == 0);
			/* The only ship sunk: prison in the town. */
			if (g->screen == &screen_prison) {
				CHECK(g->sim.jail.cause == JAIL_FORT);
				CHECK(game_script(g, "wait:40 enter enter "
				    "enter wait:40") == 0);
			}
			CHECK(g->screen == &screen_sail);
			fired = 1;
			continue;
		}
		/* Sneaking in: -20 x 5 loses to -rand(50). */
		CHECK(game_script(g, "down down enter wait:40") == 0);
		CHECK(g->screen == &screen_arrive && g->arrive_stage == 2);
		CHECK(game_script(g, "enter wait:40") == 0 &&
		    g->screen == &screen_sword && g->sw_guard);
		CHECK(game_script(g, "enter wait:2") == 0 &&
		    g->sim.sword.active && g->sim.sword.kind == SWORD_DUEL);
		/* Thrown back: a disgrace. */
		g->sim.sword.result = SWORD_REPELLED;
		CHECK(game_script(g, "wait:40") == 0 &&
		    g->screen == &screen_disgrace && g->sim.reputation == -1);
		CHECK(game_script(g, "enter wait:40") == 0 &&
		    g->screen == &screen_sail);
		caught = 1;
	}
	CHECK(fired && caught);
	game_free(g);
}

/* Selling cannon to the shipwright, 50 gold each, and buying back. */
static void
test_cannon(void)
{
	struct game *g;
	long gold;
	int guns;

	g = play("eng", "enter enter down down down down enter down enter "
	    "right enter enter type:Lo enter enter esc enter enter wait:40");
	if (g == NULL)
		return;
	guns = g->sim.fleet.guns;
	gold = g->sim.fleet.gold;
	CHECK(game_script(g, "click:60,400 wait:40") == 0 &&
	    g->screen == &screen_shipwright);
	CHECK(game_script(g, "down down down enter wait:40") == 0 &&
	    g->screen == &screen_cannon);
	CHECK(game_script(g, "left left") == 0 &&
	    g->sim.fleet.guns == guns - 2 && g->sim.wright_cannon == 2 &&
	    g->sim.fleet.gold == gold + 100);
	CHECK(game_script(g, "right") == 0 &&
	    g->sim.fleet.guns == guns - 1 && g->sim.fleet.gold == gold + 50);
	CHECK(game_script(g, "enter wait:40") == 0 &&
	    g->screen == &screen_shipwright);
	game_free(g);
}

/* The bank: divide the plunder, the cave, another expedition. */
static void
test_bank(void)
{
	struct game *g;

	g = play("eng", "enter enter down down down down enter down enter "
	    "right enter enter type:Lo enter enter esc enter enter wait:40");
	if (g == NULL)
		return;
	CHECK(g->screen == &screen_town);
	g->sim.fleet.gold = 60000;
	g->sim.fleet.crew = 50;
	g->sim.fleet.nships = 2;
	CHECK(game_script(g, "click:540,320 wait:40") == 0 &&
	    g->screen == &screen_bank);
	/* 3000 more to the fortune, which starts at 200. */
	CHECK(game_script(g, "enter") == 0 && g->plunder.share == 960 &&
	    g->plunder.mood == 1 && g->sim.fortune == 3200);
	CHECK(game_script(g, "enter enter wait:40") == 0 &&
	    g->screen == &screen_cave);
	CHECK(game_script(g, "enter wait:40") == 0 &&
	    g->screen == &screen_bank && g->bank_stage == 3);
	/* Plan another expedition: back in town with one ship. */
	CHECK(game_script(g, "enter enter wait:40") == 0 &&
	    g->screen == &screen_town);
	CHECK(g->sim.fleet.nships == 1 && g->sim.fleet.gold == 6000);
	/* No: the bank lets the player go. */
	CHECK(game_script(g, "click:540,320 wait:40 down enter wait:40") == 0
	    && g->screen == &screen_town && g->sim.fleet.gold == 6000);
	/* Nine years at sea: health 3, the officers leave, the career is
	 * over before any plan (CSTR 160). */
	g->sim.start_year = g->sim.year - 9;
	g->sim.injuries = 0;
	g->sim.player.ability = ABILITY_FENCING;
	CHECK(game_script(g, "click:540,320 wait:40 enter enter enter "
	    "wait:40 enter wait:40") == 0 && g->screen == &screen_bank &&
	    g->bank_stage == 6);
	CHECK(game_script(g, "enter wait:40") == 0 &&
	    g->screen == &screen_ending && g->end_forced);
	game_free(g);
}

/* The crew: the roll call on leaving town, the master's warning and a
 * mutiny at sea. */
static void
test_crew(void)
{
	struct game *g;
	struct ui_event ev;
	uint8_t *shown;
	size_t n;
	int i, rep;

	g = play("eng", "enter enter down down down down enter down enter "
	    "right enter enter type:Lo enter enter esc enter enter wait:40");
	if (g == NULL)
		return;
	CHECK(g->screen == &screen_town);
	g->sim.encounters = 0;
	/* A sullen crew: (3 + 2) x 30 / 15 = 10 desert on leaving. */
	g->sim.fleet.crew = 30;
	g->sim.fleet.mood = -2;
	CHECK(game_script(g, "click:60,300 wait:40") == 0 &&
	    g->screen == &screen_deserted && g->sim.deserted == 10 &&
	    g->sim.fleet.crew == 20);
	CHECK(game_script(g, "enter wait:40") == 0 &&
	    g->screen == &screen_sail);
	/* Broke for months: the master warns at the month's turn. */
	g->sim.fleet.gold = 0;
	g->sim.months = 4;
	for (i = 0; i < 2000 && g->screen == &screen_sail; i++)
		CHECK(game_script(g, "wait:4") == 0);
	CHECK(g->screen == &screen_crew &&
	    g->sim.crew_event == CREW_RESTLESS);
	/* The warning fades out as it was read, though Enter cleared the
	 * event its text comes from. */
	g->sim.crew_event = CREW_HUNGRY;
	g->sim.food_days = 8;
	CHECK(game_script(g, "wait:20") == 0);
	n = (size_t)g->fb.out_w * g->fb.out_h;
	if ((shown = malloc(n)) != NULL) {
		memcpy(shown, g->fb.idx, n);
		memset(&ev, 0, sizeof(ev));
		ev.type = EV_KEY;
		ev.key = UI_KEY_ENTER;
		game_event(g, &ev);
		for (i = 0; i < 4; i++) {
			game_tick(g);
			game_draw(g);
		}
		CHECK(g->screen == &screen_crew && g->fading == -1 &&
		    g->sim.crew_event == CREW_NONE &&
		    memcmp(shown, g->fb.idx, n) == 0);
		free(shown);
	}
	CHECK(game_script(g, "wait:40") == 0 &&
	    g->screen == &screen_sail && g->sim.crew_event == CREW_NONE);
	/* Worse: mutiny, the duel on the deck, reputation -4. */
	g->sim.months = 10;
	sim_advance(&g->sim, 30);
	CHECK(g->sim.crew_event == CREW_MUTINY);
	game_goto(g, &screen_crew);
	CHECK(game_script(g, "wait:40 enter wait:40") == 0 &&
	    g->screen == &screen_sword && g->sw_stage == 1);
	CHECK(game_script(g, "enter") == 0 && g->sw_stage == 2 &&
	    g->sim.sword.title == 2);
	rep = g->sim.reputation;
	g->sim.sword.result = SWORD_WON;
	CHECK(game_script(g, "wait:40") == 0 && g->screen == &screen_sail &&
	    g->sim.reputation == rep - 4 && g->sim.crew_event == CREW_NONE);
	/* Too few men for two ships: one is left behind, chosen on the
	 * fleet panel. */
	g->sim.fleet.nships = 2;
	g->sim.fleet.ships[1] = 2;
	g->sim.fleet.damage[1] = 0;
	g->sim.fleet.capacity += 6;
	g->sim.fleet.crew = 10;
	game_goto(g, &screen_sail);
	CHECK(game_script(g, "wait:40") == 0 &&
	    g->screen == &screen_fleet_panel && g->panel_abandon);
	CHECK(game_script(g, "click:240,120 click:540,450 wait:40") == 0 &&
	    g->sim.fleet.nships == 1 && !g->panel_abandon &&
	    g->screen == &screen_sail);
	/* An overflowing hold: cargo goes overboard before sailing on. */
	g->sim.fleet.goods = sim_hold_space(&g->sim) + g->sim.fleet.goods + 5;
	game_goto(g, &screen_sail);
	CHECK(game_script(g, "wait:40") == 0 &&
	    g->screen == &screen_merchant && g->jettison &&
	    g->shop_stage == 3);
	CHECK(game_script(g, "enter") == 0 && g->shop_stage == 1);
	CHECK(game_script(g, "click:320,450") == 0 &&
	    g->screen == &screen_merchant);	/* not until it fits */
	CHECK(game_script(g, "click:455,175") == 0 && g->selected == 1);
	CHECK(game_script(g, "click:40,30 click:40,30 click:40,30 "
	    "click:40,30 click:40,30") == 0 && sim_hold_space(&g->sim) >= 0);
	CHECK(game_script(g, "click:320,450 wait:40") == 0 &&
	    g->screen == &screen_sail && !g->jettison);
	/* A prize kept with eight ships: the panel picks the ship that
	 * makes room. */
	g->sim.fleet.nships = 8;
	for (rep = 1; rep < 8; rep++) {
		g->sim.fleet.ships[rep] = 0;
		g->sim.fleet.damage[rep] = 0;
	}
	g->sim.fleet.crew = 100;
	g->sim.prize_pending = 1;
	g->sim.prize_type = 6;
	g->sim.prize_damage = 0;
	g->panel_replace = 1;
	game_goto(g, &screen_fleet_panel);
	CHECK(game_script(g, "wait:40 click:240,120 click:540,450 wait:40") ==
	    0 && !g->sim.prize_pending && g->sim.fleet.ships[1] == 6 &&
	    g->screen == &screen_sail);
	g->sim.fleet.nships = 1;
	/* A capture's plunder: her cargo and guns on the loot board (four
	 * rows), then her sailors. */
	g->sim.battle.loot_gold = 120;
	g->sim.battle.loot[0] = 3;
	g->sim.battle.loot[1] = g->sim.battle.loot[2] = 0;
	g->sim.battle.loot[3] = 4;
	g->sim.battle.volunteers = 9;
	g->sim.fleet.goods = g->sim.fleet.sugar = 0;
	g->btl_loot = 1;
	g->btl_news = -1;
	g->btl_landed = g->btl_hunted = 0;
	rep = g->sim.fleet.guns;
	game_goto(g, &screen_battle_end);
	CHECK(game_script(g, "wait:40") == 0 &&
	    g->screen == &screen_merchant && g->prize_loot &&
	    g->sim.market.plunder && g->sim.market.stock[3] == 4 &&
	    g->music_request == MUS_PLUNDER);
	CHECK(game_script(g, "down down down right right") == 0 &&
	    g->selected == 3 && g->sim.fleet.guns == rep + 2 &&
	    g->sim.market.stock[3] == 2);
	CHECK(game_script(g, "click:320,450 wait:40") == 0 &&
	    g->screen == &screen_volunteers && !g->sim.market.plunder);
	rep = g->sim.fleet.crew;
	CHECK(game_script(g, "enter wait:40") == 0 &&
	    g->sim.fleet.crew == rep + 9 && g->screen == &screen_sail);
	/* Seven men for the last ship: abandoned, marooned. */
	g->sim.fleet.crew = 7;
	g->sim.player.expedition = -1;
	g->sim.start_year = g->sim.year;
	g->sim.injuries = 0;
	game_goto(g, &screen_sail);
	CHECK(game_script(g, "wait:40") == 0 &&
	    g->screen == &screen_lastship);
	CHECK(game_script(g, "enter wait:40") == 0 &&
	    g->screen == &screen_prison &&
	    g->sim.jail.cause == JAIL_MAROONED);
	game_free(g);
}

static int
touch(const char *dir, const char *name, const char *text)
{
	char path[1024];
	FILE *fp;

	snprintf(path, sizeof(path), "%s/%s", dir, name);
	if ((fp = fopen(path, "w")) == NULL)
		return (-1);
	fputs(text, fp);
	return (fclose(fp));
}

/* The ink's pixels on the 640x480 screen, right of the ship. */
static int
credit_pixels(const struct game *g)
{
	int x, y, n = 0;

	for (y = 0; y < FB_H; y++)
		for (x = 380; x < FB_W; x++)
			n += g->fb.px[y * FB_W + x] == 0x20;
	return (n);
}

/*
 * A pack's credits as text: its blocks instead of the picture's, faded,
 * English's for a language without its own; broken ones are skipped.
 */
static void
test_credits_text(void)
{
	char dir[] = "/tmp/flibustier-test-XXXXXX", path[1024];
	struct packs *own, *saved = packs;
	struct game *g;
	int i, last, seen, full = 0;

	if (fsys_mkdtemp(dir) == NULL) {
		CHECK(!"set up");
		return;
	}
	CHECK(touch(dir, "pack.ini", "[assets]\n"
	    "text/eng/credits = credits.json\n"
	    "text/ger/credits = broken.json\n") == 0);
	CHECK(touch(dir, "credits.json", "{\"ink\": \"#102030\", \"blocks\": ["
	    "{\"heading\": \"Thanks\", \"lines\": [\"A\", \"B\"]},"
	    "{\"lines\": [\"C\"]}]}") == 0);
	CHECK(touch(dir, "broken.json", "{\"blocks\": []}") == 0);
	own = packs_new();
	CHECK(packs_add_original(own, getenv("FLIBUSTIER_DATA")) == 0);
	CHECK(packs_add_dir(own, dir) == 0);
	packs = own;

	g = play("eng", "wait:200 enter wait:40");
	if (g != NULL) {
		CHECK(g->screen == &screen_credits && g->cred_count == 2 &&
		    g->cred_text.nblocks == 2 && g->credits.pixels == NULL);
		for (i = 0, last = 0, seen = 1; i < 1000 &&
		    g->screen == &screen_credits; i++) {
			if (g->cred_block != last && g->cred_block < 2) {
				CHECK(g->cred_block == last + 1);
				last = g->cred_block;
				seen++;
			}
			/* Shown from half the steps on, in the ink. */
			if (g->cred_step == 19 && g->cred_block == 0) {
				full = credit_pixels(g);
				CHECK(memcmp(&g->fb.pal[0x20 * 3],
				    "\x10\x20\x30", 3) == 0);
			} else if (g->cred_step < 10)
				CHECK(credit_pixels(g) == 0);
			CHECK(game_script(g, "wait:1") == 0);
		}
		CHECK(seen == 2 && full > 0 && i > 2 * 180 && i < 2 * 200);
		CHECK(game_script(g, "wait:40") == 0);
		CHECK(g->screen == &screen_welcome &&
		    g->cred_text.nblocks == 0);
		game_free(g);
	}

	/* French has none: English's.  The keys: to the last, then out. */
	g = play("fre", "wait:200 enter wait:40");
	if (g != NULL) {
		CHECK(g->screen == &screen_credits && g->cred_count == 2);
		CHECK(game_script(g, "wait:20 enter wait:5") == 0);
		CHECK(g->cred_block == 1);
		CHECK(game_script(g, "enter wait:40") == 0);
		CHECK(g->screen == &screen_welcome);
		game_free(g);
	}

	/* German's is broken: no credits, not the picture's. */
	g = play("ger", "wait:200 enter wait:40");
	if (g != NULL) {
		CHECK(g->screen == &screen_welcome && g->credits.pixels == NULL);
		game_free(g);
	}

	packs = saved;
	packs_free(own);
	snprintf(path, sizeof(path), "%s/pack.ini", dir);
	remove(path);
	snprintf(path, sizeof(path), "%s/credits.json", dir);
	remove(path);
	snprintf(path, sizeof(path), "%s/broken.json", dir);
	remove(path);
	rmdir(dir);
}

/*
 * Per-town assets: the town's own, else its nation's, else the shared
 * one; outside a town, the shared one.
 */
static void
test_town_assets(void)
{
	static const char *const files[] = { "pack.ini", "tavern.png",
	    "governor.png", "tavern.pcx", NULL };
	struct image want;
	struct blob pic;
	FILE *fp;
	char dir[] = "/tmp/flibustier-test-XXXXXX", path[1024], id[96], slug[32];
	struct packs *own, *saved;
	struct game *g;
	struct sim_town *t;
	size_t i;

	town_slug("Port Royale", slug, sizeof(slug));
	CHECK(strcmp(slug, "port-royale") == 0);
	town_slug("St. Augustine", slug, sizeof(slug));
	CHECK(strcmp(slug, "st-augustine") == 0);
	town_slug("  San--Juan ", slug, sizeof(slug));
	CHECK(strcmp(slug, "san-juan") == 0);
	town_slug("Cura\xc3\xa7" "ao", slug, sizeof(slug));	/* UTF-8 */
	CHECK(strcmp(slug, "cura-ao") == 0);
	town_slug("Maracaibo", slug, 5);
	CHECK(strcmp(slug, "mara") == 0);

	g = play("eng", "enter enter down down down down enter down enter "
	    "right enter enter type:Lo enter enter esc enter enter "
	    "click:60,300 wait:30");
	if (g == NULL || fsys_mkdtemp(dir) == NULL) {
		CHECK(!"set up");
		return;
	}
	for (i = 0; i < g->sim.ntowns; i++)
		if (strcmp(g->sim.towns[i].name, "Port Royale") == 0)
			break;
	CHECK(i < g->sim.ntowns);
	if (i == g->sim.ntowns)
		return;
	t = &g->sim.towns[i];
	CHECK(t->nation == NATION_ENGLAND);
	CHECK(touch(dir, "pack.ini", "[assets]\n"
	    "pic/tavern/port-royale = tavern.png\n"
	    "pic/governor/england = governor.png\n") == 0);
	CHECK(touch(dir, "tavern.png", "x") == 0);
	CHECK(touch(dir, "governor.png", "x") == 0);
	/* A stack of its own, so later tests keep the original. */
	own = packs_new();
	CHECK(packs_add_original(own, getenv("FLIBUSTIER_DATA")) == 0);
	CHECK(packs_add_dir(own, dir) == 0);
	saved = g->packs;
	g->packs = own;

	g->sim.player_town = -1;
	CHECK(strcmp(game_town_asset(g, "pic/tavern", id, sizeof(id)),
	    "pic/tavern") == 0);
	g->sim.player_town = (int)i;
	CHECK(strcmp(game_town_asset(g, "pic/tavern", id, sizeof(id)),
	    "pic/tavern/port-royale") == 0);
	CHECK(strcmp(game_town_asset(g, "pic/governor", id, sizeof(id)),
	    "pic/governor/england") == 0);
	CHECK(strcmp(game_town_asset(g, "pic/barmaid", id, sizeof(id)),
	    "pic/barmaid") == 0);
	/* Taken by Spain: no Spanish governor, so the shared one. */
	t->nation = NATION_SPAIN;
	CHECK(strcmp(game_town_asset(g, "pic/governor", id, sizeof(id)),
	    "pic/governor") == 0);
	/* Too small a buffer: the shared one, never a cut ID. */
	t->nation = NATION_ENGLAND;
	CHECK(strcmp(game_town_asset(g, "pic/tavern", id, 12),
	    "pic/tavern") == 0);

	/* In the game: Port Royale's tavern shows its own picture, here
	 * the governor's (copied for the test only). */
	g->packs = saved;
	g->sim.player_town = -1;
	CHECK(packs_load(saved, "pic/governor", &pic) == 0);
	snprintf(path, sizeof(path), "%s/tavern.pcx", dir);
	if ((fp = fopen(path, "wb")) != NULL) {
		fwrite(pic.data, 1, pic.len, fp);
		fclose(fp);
	}
	blob_free(&pic);
	CHECK(touch(dir, "pack.ini", "[assets]\n"
	    "pic/tavern/port-royale = tavern.pcx\n") == 0);
	packs_free(own);
	own = packs_new();
	CHECK(packs_add_original(own, getenv("FLIBUSTIER_DATA")) == 0);
	CHECK(packs_add_dir(own, dir) == 0);
	g->packs = own;
	g->sim.encounters = 0;
	into_town(g, t);
	CHECK(game_script(g, "wait:40 enter wait:40 click:250,350 wait:40") ==
	    0);
	CHECK(g->screen == &screen_tavern && g->sim.player_town == (int)i);
	CHECK(pic_load(own, "pic/governor", &want) == 0);
	CHECK(g->bg.pixels != NULL && want.pixels != NULL &&
	    g->bg.w == want.w && g->bg.h == want.h &&
	    memcmp(g->bg.pixels, want.pixels, (size_t)want.w * want.h) == 0);
	image_free(&want);

	g->packs = saved;
	packs_free(own);
	game_free(g);
	for (i = 0; files[i] != NULL; i++) {
		snprintf(path, sizeof(path), "%s/%s", dir, files[i]);
		remove(path);
	}
	remove(dir);
}

/* Whether drawing the pointer at (300, 200) changes the screen. */
static int
pointer_drawn(struct game *g)
{
	static uint8_t without[FB_W * FB_H];

	g->pointer_x = 300;
	g->pointer_y = 200;
	g->pointer_seen = 0;
	game_draw(g);
	memcpy(without, g->fb.px, sizeof(without));
	g->pointer_seen = 1;
	game_draw(g);
	return (memcmp(without, g->fb.px, sizeof(without)) != 0);
}

/* The pointer is hidden while sailing (VA 0x22c76), shown elsewhere. */
static void
test_pointer_hidden(void)
{
	struct game *g;

	g = play("eng", "enter enter down down down down enter down enter "
	    "right enter enter type:Lo enter enter esc enter enter click:60,300 "
	    "wait:40");
	if (g == NULL)
		return;
	CHECK(g->screen == &screen_sail);
	CHECK(!pointer_drawn(g));
	game_goto(g, &screen_cabin);
	CHECK(game_script(g, "wait:40") == 0 && g->screen == &screen_cabin);
	CHECK(pointer_drawn(g));
}

/* The game's printf-style fill, with the pack's numbered arguments. */
static void
test_fill_args(void)
{
	const char *arg[3] = { "espagnol", "Mutin", NULL };
	char out[64];

	fill_args(out, sizeof(out), "%s %s", arg, 2);
	CHECK(strcmp(out, "espagnol Mutin") == 0);
	fill_args(out, sizeof(out), "%2$s %1$s", arg, 2);
	CHECK(strcmp(out, "Mutin espagnol") == 0);
	/* Past the arguments, or NULL: empty; %% and %d stay as written. */
	fill_args(out, sizeof(out), "[%3$s|%4$s|%s %s %s] 5%% %d", arg, 3);
	CHECK(strcmp(out, "[||espagnol Mutin ] 5%% %d") == 0);
	fill_args(out, 6, "%2$s %1$s", arg, 2);
	CHECK(strcmp(out, "Mutin") == 0);
}

int
main(void)
{
	const char *path;

	if ((path = getenv("FLIBUSTIER_DATA")) == NULL || *path == '\0') {
		printf("FLIBUSTIER_DATA not set, skipping\n");
		return (SKIP);
	}
	if ((packs = packs_new()) == NULL ||
	    packs_add_original(packs, path) != 0)
		return (1);
	if (fsys_mkdtemp(saves_tmp) == NULL)
		return (1);
	test_fill_args();
	test_pointer_hidden();
	test_career();
	test_menus();
	test_languages();
	test_saves();
	test_hall_of_fame();
	test_language();
	test_sailing();
	test_encounter_screens();
	test_encounter_pages();
	test_debug_mode();
	test_tune_ids();
	test_battle_screen();
	test_sword_screen();
	test_pointer();
	test_intro();
	test_credits_text();
	test_town();
	test_town_assets();
	test_shipwright();
	test_merchant();
	test_tavern();
	test_governor();
	test_story();
	test_bank();
	test_cannon();
	test_arrival();
	test_pace();
	test_cabin();
	test_assault();
	test_tavern_people();
	test_town_time();
	test_landing();
	test_crew();
	test_land_battle();
	rmdir(saves_tmp);
	packs_free(packs);
	if (failures > 0) {
		fprintf(stderr, "%d check(s) failed\n", failures);
		return (1);
	}
	printf("all shell tests passed\n");
	return (0);
}
