/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026, Olivier Cochard-Labbé
 *
 * Simulation and protocol tests on a synthetic era; no game data needed.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sim/hof.h"
#include "sim/json.h"
#include "sim/proto.h"
#include "sim/rng.h"
#include "sim/sim.h"
#include "sim/internal.h"

static int failures;

#define CHECK(cond) do {						\
	if (!(cond)) {							\
		fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__,	\
		    __LINE__, #cond);					\
		failures++;						\
	}								\
} while (0)

/* Four towns: two Spanish, one English next to them, one far French. */
static void
make_era(struct era_towns *era, struct scenario *scn)
{
	static const struct {
		const char	*name;
		int		 x, y, nation, soldiers;
	} t[] = {
		{ "Alpha", 100, 100, NATION_SPAIN, 40 },
		{ "Beta", 120, 110, NATION_SPAIN, 10 },
		{ "Gamma", 110, 130, NATION_ENGLAND, 60 },
		{ "Delta", 600, 400, NATION_FRANCE, 20 },
	};
	size_t i;

	memset(era, 0, sizeof(*era));
	memset(scn, 0, sizeof(*scn));
	era->year = 1660;
	era->count = sizeof(t) / sizeof(t[0]);
	for (i = 0; i < era->count; i++) {
		snprintf(era->towns[i].name, sizeof(era->towns[i].name), "%s",
		    t[i].name);
		era->towns[i].x = t[i].x;
		era->towns[i].y = t[i].y;
		era->towns[i].nation = t[i].nation;
		era->towns[i].soldiers = t[i].soldiers;
		era->towns[i].citizens = 30;
		era->towns[i].gold = 20;
		era->towns[i].wealth = 120;
	}
	snprintf(scn->name, sizeof(scn->name), "Test Captain");
	scn->relations[NATION_SPAIN][NATION_ENGLAND] = -1;
	scn->relations[NATION_ENGLAND][NATION_SPAIN] = -1;
	/* Delta is at war with England too, but out of range. */
	scn->relations[NATION_FRANCE][NATION_ENGLAND] = -1;
	scn->relations[NATION_ENGLAND][NATION_FRANCE] = -1;
}

static int
loader(void *arg, int era, int scenario, struct era_towns *towns,
    struct scenario *scn)
{
	(void)arg;
	(void)era;
	(void)scenario;
	make_era(towns, scn);
	return (0);
}

static void
test_rng(void)
{
	struct rng a, b;
	int i, v, seen[7] = { 0 };

	rng_seed(&a, 42, 1);
	rng_seed(&b, 42, 1);
	for (i = 0; i < 1000; i++)
		CHECK(rng_next(&a) == rng_next(&b));
	rng_seed(&b, 43, 1);
	CHECK(rng_next(&a) != rng_next(&b));
	for (i = 0; i < 7000; i++) {
		v = rng_below(&a, 7);
		CHECK(v >= 0 && v < 7);
		if (v >= 0 && v < 7)
			seen[v]++;
	}
	for (i = 0; i < 7; i++)
		CHECK(seen[i] > 800 && seen[i] < 1200);
	CHECK(rng_below(&a, 0) == 0 && rng_below(&a, -5) == 0);
}

static void
test_json(void)
{
	struct json *j;
	char err[128];

	j = json_parse(" {\"a\": [1, -2, {\"b\": \"x\\u00e9\\n\"}], "
	    "\"t\": true, \"n\": null} ", err, sizeof(err));
	CHECK(j != NULL);
	if (j != NULL) {
		CHECK(json_get(j, "a")->count == 3);
		CHECK(json_get(j, "a")->items[1]->num == -2);
		CHECK(strcmp(json_get_str(json_get(j, "a")->items[2], "b"),
		    "x\xc3\xa9\n") == 0);
		CHECK(json_get_int(j, "missing", 7) == 7);
		json_free(j);
	}
	CHECK(json_parse("{\"a\":1,}", err, sizeof(err)) == NULL);
	CHECK(json_parse("[1.5]", err, sizeof(err)) == NULL);
	CHECK(json_parse("\"open", err, sizeof(err)) == NULL);
	CHECK(json_parse("{} x", err, sizeof(err)) == NULL);
	CHECK(json_parse("99999999999999999999", err, sizeof(err)) == NULL);
}

/* News by absolute index (0 = first ever logged); must still be kept. */
static const struct sim_news *
news_abs(const struct sim *s, size_t k)
{
	return (sim_news_at(s, k - (s->nnews - sim_news_kept(s))));
}

/* Reset the conquest test conditions. */
static void
pin_war(struct sim *s)
{
	s->relations[NATION_SPAIN][NATION_ENGLAND] = -1;
	s->relations[NATION_ENGLAND][NATION_SPAIN] = -1;
	s->relations[NATION_FRANCE][NATION_ENGLAND] = -1;
	s->relations[NATION_ENGLAND][NATION_FRANCE] = -1;
	s->towns[1].soldiers = 0;
	s->towns[2].soldiers = 60;
	s->towns[3].soldiers = 0;
}

static void
test_rules(void)
{
	struct era_towns era;
	struct scenario scn;
	struct sim s;
	const struct sim_news *n;
	size_t k, before;
	int m, a, b, captured = 0, wrong = 0;

	make_era(&era, &scn);

	/* The start: the scenario's standings and ranks, pirates hostile,
	 * a fortune of 200; a dated one (an expedition) starts on its
	 * date. */
	scn.standing[NATION_ENGLAND] = 10;
	scn.standing[NATION_PIRATE] = 5;
	scn.rank[NATION_ENGLAND] = 2;
	scn.date = 2 * 360 + 95;
	sim_new_game(&s, &era, &scn, 7);
	CHECK(s.standing[NATION_ENGLAND] == 10 && s.rank[NATION_ENGLAND] == 2 &&
	    s.standing[NATION_PIRATE] == -1 && s.fortune == 200);
	CHECK(s.year == era.year + 2 && s.start_year == s.year &&
	    s.day_of_year == 95 && s.month == 3);
	memset(scn.standing, 0, sizeof(scn.standing));
	memset(scn.rank, 0, sizeof(scn.rank));
	scn.date = 0;

	/*
	 * Conquest, with the conditions pinned before every update so that
	 * diplomacy and growth cannot drift them: England (Gamma, strong) is
	 * at war with Spain (Beta, empty, in range) and France (Delta, weak,
	 * out of range).
	 */
	sim_new_game(&s, &era, &scn, 7);
	s.diplomacy_range = 0xFFFF;
	for (m = 0; m < 400; m++) {
		pin_war(&s);
		before = s.nnews;
		sim_world_update(&s);
		for (k = before; k < s.nnews; k++) {
			n = news_abs(&s, k);
			if (n->code != NEWS_CAPTURE)
				continue;
			captured += n->b == 1;
			wrong += n->b == 3;	/* Delta is out of range */
		}
		s.towns[1].nation = NATION_SPAIN;	/* re-arm the target */
	}
	CHECK(captured > 0);
	CHECK(wrong == 0);
	for (a = 0; a < 4; a++)
		for (b = 0; b < 4; b++)
			CHECK(s.relations[a][b] == s.relations[b][a]);

	/* Same setup while Beta's governor has given a mission: never
	 * captured, never given a new governor (VA 0x471ac). */
	sim_new_game(&s, &era, &scn, 11);
	s.diplomacy_range = 0xFFFF;
	s.player_town = 2;
	s.mission.type = MISSION_LETTER;
	s.mission.giver = 1;
	captured = wrong = 0;
	for (m = 0; m < 2000; m++) {
		pin_war(&s);
		before = s.nnews;
		sim_world_update(&s);
		for (k = before; k < s.nnews; k++) {
			n = news_abs(&s, k);
			if ((n->code == NEWS_CAPTURE && n->b == 1) ||
			    (n->code == NEWS_GOVERNOR && n->a == 1))
				wrong++;
			captured += n->code == NEWS_GOVERNOR;
		}
	}
	CHECK(wrong == 0);
	CHECK(captured > 0);	/* governors do change elsewhere */
	CHECK(s.towns[1].nation == NATION_SPAIN);
	/* The town the player is in is never captured. */
	sim_new_game(&s, &era, &scn, 12);
	s.diplomacy_range = 0xFFFF;
	s.player_town = 1;
	for (m = 0, wrong = 0; m < 2000; m++) {
		pin_war(&s);
		before = s.nnews;
		sim_world_update(&s);
		for (k = before; k < s.nnews; k++)
			wrong += news_abs(&s, k)->code == NEWS_CAPTURE &&
			    news_abs(&s, k)->b == 1;
	}
	CHECK(wrong == 0);

	/* Diplomacy keeps relations symmetric and within -1..1. */
	sim_new_game(&s, &era, &scn, 13);
	s.diplomacy_range = 28;
	for (m = 0; m < 500; m++) {
		sim_world_update(&s);
		for (a = 0; a < 4; a++)
			for (b = 0; b < 4; b++) {
				CHECK(s.relations[a][b] == s.relations[b][a]);
				CHECK(s.relations[a][b] >= -1 &&
				    s.relations[a][b] <= 1);
			}
	}

	/* With a range of 28 every draw changes relations or is a no-op. */
	sim_new_game(&s, &era, &scn, 3);
	s.diplomacy_range = 28;
	for (m = 0; m < 200; m++)
		sim_world_update(&s);
	CHECK(s.nnews > 50);

	/* Time: an update per 30-day month, the year turns every 360 days. */
	sim_new_game(&s, &era, &scn, 5);
	/* The career starts mid-year with six entries in the log (VA
	 * 0x132f8, 0x13679): spread from New Year, war or alliance by the
	 * pair's relation, a town event at peace. */
	CHECK(s.day_of_year >= 65 && s.day_of_year < 215 && s.nnews == 6);
	for (k = 0; k < 6; k++) {
		const struct sim_news *e = news_abs(&s, k);

		CHECK(e->day < 0 && e->date >= 1660 * 360 &&
		    e->date < 1660 * 360 + s.day_of_year);
		CHECK(k == 0 || e->date > news_abs(&s, k - 1)->date);
		CHECK(e->code == NEWS_WAR || e->code == NEWS_ALLIANCE ||
		    (e->code >= NEWS_INDIANS && e->code <= NEWS_PLUNDER));
	}
	CHECK(news_abs(&s, 0)->date - 1660 * 360 == s.day_of_year / 12);
	s.day_of_year = s.month = 0;	/* the calendar from New Year */
	before = s.nnews;
	s.diplomacy_range = 28;	/* every draw is a diplomatic event */
	sim_advance(&s, 29);
	CHECK(s.month == 0 && s.day == 29 && s.year == 1660);
	sim_advance(&s, 1);
	CHECK(s.month == 1 && s.day == 30 && s.day_of_year == 30);
	sim_advance(&s, 95);
	CHECK(s.month == 4 && s.day == 125 && s.day_of_year == 125);
	sim_advance(&s, 360);
	CHECK(s.year == 1661 && s.day_of_year == 125 && s.day == 485);
	sim_advance(&s, 0);
	sim_advance(&s, -10);
	CHECK(s.day == 485);
	/* News is dated at its month boundary, not at the end. */
	for (k = before; k < s.nnews; k++)
		CHECK(news_abs(&s, k)->day % 30 == 0);
	CHECK(s.nnews > before);
}

/* Run a command script in a fresh session; returns all output lines. */
static char *
run(const char *const *cmds, size_t n)
{
	struct session ss;
	struct jbuf all = { 0 }, out;
	size_t i;

	memset(&ss, 0, sizeof(ss));
	ss.load = loader;
	for (i = 0; i < n; i++) {
		memset(&out, 0, sizeof(out));
		proto_handle(&ss, cmds[i], &out);
		jb_raw(&all, out.s != NULL ? out.s : "");
		jb_raw(&all, "\n");
		jb_free(&out);
	}
	return (all.s);
}

static void
test_protocol(void)
{
	static const char *const script[] = {
		"{\"cmd\":\"new_game\",\"era\":1660,\"seed\":99}",
		"{\"cmd\":\"set\",\"field\":\"diplomacy_range\",\"value\":40}",
		"{\"cmd\":\"advance\",\"days\":3650}",
		"{\"cmd\":\"state\"}",
	};
	static const char *const other_seed[] = {
		"{\"cmd\":\"new_game\",\"era\":1660,\"seed\":100}",
		"{\"cmd\":\"set\",\"field\":\"diplomacy_range\",\"value\":40}",
		"{\"cmd\":\"advance\",\"days\":3650}",
		"{\"cmd\":\"state\"}",
	};
	static const char *const errors[] = {
		"{\"cmd\":\"advance\",\"days\":30}",
		"not json",
		"{\"cmd\":\"new_game\",\"era\":1234}",
		"{\"cmd\":\"new_game\",\"era\":1660,\"nation\":\"venice\"}",
		"{\"cmd\":\"new_game\",\"era\":1660}",
		"{\"cmd\":\"fly\"}",
		"{\"cmd\":\"set\",\"town\":\"Nowhere\",\"field\":\"gold\","
		    "\"value\":1}",
		"{\"cmd\":\"set\",\"town\":\"Alpha\",\"field\":\"gold\","
		    "\"value\":70000}",
		"{\"cmd\":\"advance\",\"days\":0}",
		"{\"cmd\":\"load\",\"state\":{\"version\":9}}",
		"{\"cmd\":\"quit\"}",
	};
	char *a, *b, *c, *line, *err;
	size_t nlines;

	/* Same script, same output; another seed, another history. */
	a = run(script, 4);
	b = run(script, 4);
	c = run(other_seed, 4);
	CHECK(a != NULL && b != NULL && c != NULL);
	if (a != NULL && b != NULL && c != NULL) {
		CHECK(strcmp(a, b) == 0);
		CHECK(strcmp(a, c) != 0);
		CHECK(strstr(a, "\"scenario\":\"Test Captain\"") != NULL);
		CHECK(strstr(a, "\"day\":3650") != NULL);
	}
	free(b);
	free(c);

	err = run(errors, sizeof(errors) / sizeof(errors[0]));
	CHECK(err != NULL);
	for (nlines = 0, line = err; line != NULL && *line != '\0'; nlines++) {
		/* Every line but the last two (valid game start, quit) fails. */
		if (nlines != 4 && nlines != 10)
			CHECK(strncmp(line, "{\"ok\":false", 11) == 0);
		line = strchr(line, '\n');
		if (line != NULL)
			line++;
	}
	CHECK(nlines == sizeof(errors) / sizeof(errors[0]));
	free(err);
	free(a);
}

/* A saved state reloads to the same state and replays identically. */
static void
test_snapshot(void)
{
	struct session s1, s2;
	struct jbuf st1 = { 0 }, st2 = { 0 }, cmd = { 0 }, out = { 0 };
	struct jbuf r1 = { 0 }, r2 = { 0 };

	memset(&s1, 0, sizeof(s1));
	s1.load = loader;
	proto_handle(&s1, "{\"cmd\":\"new_game\",\"era\":1600,\"seed\":5}",
	    &out);
	jb_free(&out);
	sim_advance(&s1.sim, 30 * 80);	/* fill and wrap the news ring */
	CHECK(s1.sim.nnews > 0);
	/* every kind of news survives a round trip (the last events kept) */
	{
		static const int codes[] = { NEWS_VISITED, NEWS_TITLED,
		    NEWS_PLUNDERED, NEWS_CAPTURED, NEWS_ENGAGED,
		    NEWS_LOST_TREASURE, NEWS_TREASURE, NEWS_VILLAIN,
		    NEWS_AMNESTY, NEWS_INDIANS, NEWS_GOLD_MINE, NEWS_MALARIA,
		    NEWS_GOVERNOR, NEWS_PLUNDER, NEWS_WAR, NEWS_PEACE,
		    NEWS_ALLIANCE, NEWS_CAPTURE, NEWS_TAKEN, NEWS_NOT_TAKEN,
		    NEWS_FLEET_TAKEN, NEWS_TRAIN_TAKEN, NEWS_RESCUED,
		    NEWS_LETTER, NEWS_PAPERS, NEWS_PIRATE_OFF, NEWS_RESCUE,
		    NEWS_DELIVERED_LETTER, NEWS_DELIVERED_PAPERS,
		    NEWS_PIRATE_TAKEN, NEWS_SON_FREED, NEWS_DISPLEASED };
		size_t k;

		for (k = 0; k < sizeof(codes) / sizeof(codes[0]); k++)
			sim_log_news(&s1.sim, codes[k], 0, 0);
	}
	sim_to_json(&s1.sim, &st1);

	jb_raw(&cmd, "{\"cmd\":\"load\",\"state\":");
	jb_raw(&cmd, st1.s);
	jb_raw(&cmd, "}");
	memset(&s2, 0, sizeof(s2));
	s2.load = loader;
	proto_handle(&s2, cmd.s, &out);
	CHECK(out.s != NULL && strcmp(out.s, "{\"ok\":true}") == 0);
	jb_free(&out);
	sim_to_json(&s2.sim, &st2);
	CHECK(st1.s != NULL && st2.s != NULL && strcmp(st1.s, st2.s) == 0);

	proto_handle(&s1, "{\"cmd\":\"advance\",\"days\":900}", &r1);
	proto_handle(&s2, "{\"cmd\":\"advance\",\"days\":900}", &r2);
	CHECK(r1.s != NULL && r2.s != NULL && strcmp(r1.s, r2.s) == 0);

	jb_free(&st1);
	jb_free(&st2);
	jb_free(&cmd);
	jb_free(&r1);
	jb_free(&r2);
}

static void
test_hof(void)
{
	static const char *path = "/tmp/flibustier-test-hof.txt";
	struct hof h, back;
	struct hof_entry e;
	char err[128];
	FILE *fp;
	int i;

	/* The original's rating scale: 0 is best, 21 worst. */
	CHECK(hof_rating(100) == 0 && hof_rating(90) == 0);
	CHECK(hof_rating(5) == 17 && hof_rating(1) == 18);
	CHECK(hof_rating(0) == 19 && hof_rating(-100) == 21);

	memset(&h, 0, sizeof(h));
	memset(&e, 0, sizeof(e));
	for (i = 0; i < 12; i++) {
		e.score = (i * 7) % 12;	/* 0, 7, 2, 9, 4, 11, 6, 1, 8, 3... */
		snprintf(e.name, sizeof(e.name), "Captain\t%d", i);
		snprintf(e.title, sizeof(e.title), "Title %d", i);
		hof_add(&h, &e);
	}
	CHECK(h.count == HOF_ENTRIES);
	for (i = 1; i < (int)h.count; i++)
		CHECK(h.entry[i - 1].score >= h.entry[i].score);
	CHECK(h.entry[0].score == 11 && h.entry[9].score == 2);
	CHECK(strchr(h.entry[0].name, '\t') == NULL);	/* tabs removed */
	e.score = 2;	/* ties the last entry: does not displace it */
	CHECK(hof_add(&h, &e) == -1);
	e.score = 5;
	CHECK(hof_add(&h, &e) == 7 && h.entry[9].score == 3);

	CHECK(hof_save(path, &h, err, sizeof(err)) == 0);
	CHECK(hof_load(path, &back, err, sizeof(err)) == 0);
	CHECK(memcmp(&h, &back, sizeof(h)) == 0);

	/* Hand edited out of order: sorted on load. */
	if ((fp = fopen(path, "w")) != NULL) {
		fputs("# flibustier hall of fame 1\n3\t17\tA\tX\n9\t0\tB\tY\n",
		    fp);
		fclose(fp);
	}
	CHECK(hof_load(path, &back, err, sizeof(err)) == 0);
	CHECK(back.count == 2 && strcmp(back.entry[0].name, "B") == 0);
	if ((fp = fopen(path, "w")) != NULL) {
		fputs("12 no tabs here\n", fp);
		fclose(fp);
	}
	CHECK(hof_load(path, &back, err, sizeof(err)) == -1);
	remove(path);
	/* No file yet: an empty Hall of Fame. */
	CHECK(hof_load(path, &back, err, sizeof(err)) == 0 && back.count == 0);
}

/*
 * Synthetic sword fight tables, simple enough to reason about: every
 * move has 6 frames (the fall 10); guards hold one line each; slashes
 * strike on frame 3, lunges on frame 2; the opponent always hits and
 * the player always does (thresholds 0), and a cautious opponent (15)
 * only stands on guard 1.  Tests adjust them as needed.
 */
static void
make_sword(struct sword_tables *t)
{
	int m, f;

	memset(t, 0, sizeof(*t));
	for (m = 0; m < SWORD_MOVES; m++)
		t->frames[m] = 6;
	t->frames[SWM_FALL] = 10;
	for (m = 0; m < BATTLE_LEVELS; m++) {
		t->reach_level[m] = 10;
		t->caution[m] = 15;
	}
	t->reach_weapon[0] = 40;
	t->reach_weapon[1] = 30;
	t->reach_weapon[2] = 20;
	t->foe_reach[0] = t->foe_reach[1] = t->foe_reach[2] = 30;
	t->player_edge = 450;
	t->foe_edge = 50;
	for (m = 0; m < 12; m++) {
		static const uint8_t lines[3] = { 2, 1, 4 };

		t->far_move[m] = m < 3 ? m : m < 6 ? SWM_RETREAT : SWM_ADVANCE;
		t->once[m] = m < 3 || m >= 6;
		t->line[m] = (uint8_t)(m % 3);
		t->area[m] = m >= 6 ? (uint8_t)(m % 3 == 2 ? 1 : m % 3 + 2) :
		    0;
		for (f = 0; f < SWORD_FRAMES; f++)
			if (m < 6)
				t->guard[m * 6 + f] = lines[m % 3];
		if (m >= 6 && m < 9) {
			t->strike[m * 6 + 3] = lines[m % 3];
			t->step_in[m * 6 + 5] = 4;
		} else if (m >= 9) {
			t->strike[m * 6 + 2] = lines[m % 3];
			t->step_in[m * 6 + 3] = 4;
		}
		if (m >= 3 && m < 6)
			t->step_back[m * 6] = 14;
	}
	t->step_in[SWM_ADVANCE * 6 + 5] = 14;
	t->step_back[SWM_RETREAT * 6] = 14;
	for (f = 0; f < SWORD_FRAMES; f++)
		t->guard[SWM_FALL * 6 + f] = 7;
	memset(t->foe_hit, 255, sizeof(t->foe_hit));	/* rarely */
	memset(t->far_ai, SWM_GUARD, sizeof(t->far_ai));
	memset(t->scripts, 0x90, sizeof(t->scripts));	/* parry */
}

/*
 * A synthetic world: all sea, except a land block of cells around
 * (110..129, 40..49).  Triangle "sine" and a flat speed table, enough to
 * check the rules' mechanics.
 */
static struct world test_world;

static void
make_world(struct world *w)
{
	int x, y, i;

	memset(w, 0, sizeof(*w));
	w->era = 4;
	memset(w->map_terrain, TERRAIN_SEA, sizeof(w->map_terrain));
	memset(w->battle_terrain, TERRAIN_SEA, sizeof(w->battle_terrain));
	/* Every land tile reads as a shore with the sea to its west. */
	memset(w->coast, 0x80, sizeof(w->coast));
	w->coast[1] = 0x33;
	/* Treasure spots: three cells east of each town in turn. */
	for (i = 0; i < TREASURE_SPOTS; i++) {
		static const int16_t town[4][2] = { { 100, 100 },
		    { 120, 110 }, { 110, 130 }, { 600, 400 } };

		w->treasure[i][0] = (int16_t)(town[i % 4][0] + 3);
		w->treasure[i][1] = town[i % 4][1];
	}
	memset(w->map_terrain + 1 * WORLD_TILE * WORLD_TILE, TERRAIN_LAND,
	    WORLD_TILE * WORLD_TILE);	/* tile 1: land */
	for (y = 40; y < 50; y++)
		for (x = 110; x < 130; x++)
			w->map[y * WORLD_CELLS_W + x] = 1;
	/* Battle tiles: 1 land as on the map, 2 shallows, on the row under
	 * the land (tile 2 looks like sea when sailing). */
	memset(w->battle_terrain + 1 * BATTLE_TILE * BATTLE_TILE,
	    TERRAIN_LAND, BATTLE_TILE * BATTLE_TILE);
	memset(w->battle_terrain + 2 * BATTLE_TILE * BATTLE_TILE,
	    TERRAIN_SHALLOWS, BATTLE_TILE * BATTLE_TILE);
	/* Battle screen 1: land on rows 0-4, shallows on row 5; its set
	 * 0 (coast to the north) offers it. */
	for (y = 0; y < BATTLE_SCREEN; y++)
		for (x = 0; x < BATTLE_SCREEN; x++)
			w->battle_screens[1][y * BATTLE_SCREEN + x] =
			    y < 5 ? 1 : y == 5 ? 2 : 0;
	for (x = 0; x < 4; x++)
		w->battle_sets[0][x] = 1;
	for (i = 0; i < 128; i++)
		w->sine[i] = (uint8_t)(i < 64 ? i * 4 : i == 64 ? 255 :
		    (128 - i) * 4);
	for (y = 0; y < SHIP_CLASSES; y++)
		for (x = 0; x < WIND_SECTORS; x++)
			w->speed[y][x] = x == 8 ? -1 : 7;
	/* Per nation: merchantmen, then a pirate sloop as record 1. */
	for (y = 0; y < ENCOUNTER_NATIONS; y++)
		for (x = 0; x < ENCOUNTER_SHIPS; x++) {
			uint8_t *r = w->encounter[y][x];

			r[0] = x == 1 ? 1 : 4;
			r[1] = x == 1 ? NATION_PIRATE : (uint8_t)y;
			r[2] = 20;
			r[3] = 8;
			r[4] = x == 1 ? 7 : 4;
		}
	/* Battles: always broadside; hit boxes as the game's. */
	memset(w->battle_ai, 1, sizeof(w->battle_ai));
	for (i = 0; i < BATTLE_TYPES; i++) {
		w->half_w[i] = (int16_t)(20 + 10 * (i / 3 < 3 ? i / 3 : 3));
		w->half_h[i] = (int16_t)(10 + 10 * (i / 3 < 3 ? i / 3 : 3));
	}
	w->half_w[9] = 40;
	w->half_h[9] = 10;
	w->hit_steps[0] = 3;
	w->hit_steps[1] = 2;
	w->hit_steps[2] = w->hit_steps[3] = w->hit_steps[4] = 1;
	make_sword(&w->sword);
}

static int
world_cb(void *arg, int era, struct world *w)
{
	(void)arg;
	make_world(w);
	w->era = era;
	return (0);
}

/* A game on the synthetic world, fleet at cell (cx, cy) heading "h". */
/* The test world's arrival cells: each town's and the eight around. */
static void
test_arrivals(struct world *w, const struct sim *s)
{
	size_t i;
	int dx, dy;

	w->narrival = 0;
	for (i = 0; i < s->ntowns; i++)
		for (dy = -1; dy <= 1; dy++)
			for (dx = -1; dx <= 1; dx++) {
				w->arrival[w->narrival].x =
				    (int16_t)(s->towns[i].x + dx);
				w->arrival[w->narrival].y =
				    (int16_t)(s->towns[i].y + dy);
				w->arrival[w->narrival].town = (int16_t)i;
				w->narrival++;
			}
}

static void
sea_game(struct sim *s, int cx, int cy, int heading)
{
	struct era_towns era;
	struct scenario scn;

	make_era(&era, &scn);
	scn.nships = 1;
	sim_new_game(s, &era, &scn, 21);
	s->day_of_year = s->month = 0;	/* mechanics from New Year */
	s->world = &test_world;
	test_arrivals(&test_world, s);
	s->fleet.x = cx * WORLD_TILE + 8;
	s->fleet.y = cy * WORLD_TILE + 8;
	s->fleet.heading = (uint8_t)heading;
	s->fleet.leaving = -1;
	s->encounters = 0;	/* mechanics only; encounters have tests */
}

static void
test_sailing(void)
{
	struct sim s;
	int town, x0, y0, i, n;
	uint8_t w0;

	make_world(&test_world);

	/* North: y falls, x stays; speed ramps up and settles at 12. */
	sea_game(&s, 120, 100, 0x00);
	x0 = s.fleet.x;
	y0 = s.fleet.y;
	CHECK(sim_sail(&s, 0, 10, &town) == SAIL_TICKS);
	CHECK(s.fleet.x == x0 && s.fleet.y < y0 && s.fleet.speed == 12);
	/* East: x grows. */
	sea_game(&s, 120, 100, 0x40);
	x0 = s.fleet.x;
	CHECK(sim_sail(&s, 0, 10, &town) == SAIL_TICKS && s.fleet.x > x0);
	/* Heading east into a wind blowing west is sector 8, the slowest:
	 * (-1 + 2) x 3 = 3, clamped up to 4. */
	sea_game(&s, 120, 100, 0x40);
	s.wind = 0xC0;
	CHECK(sim_sail(&s, 0, 10, &town) == SAIL_TICKS && s.fleet.speed == 4);

	/* A day is the world's ticks_per_day (data/calendar): 64 by
	 * default, longer when a pack says. */
	sea_game(&s, 300, 300, 0x00);
	s.encounters = 0;
	CHECK(sim_sail(&s, 0, 192, &town) == SAIL_TICKS &&
	    s.day_of_year == 3);
	test_world.ticks_per_day = 192;
	sea_game(&s, 300, 300, 0x00);
	CHECK(sim_sail(&s, 0, 192, &town) == SAIL_TICKS &&
	    s.day_of_year == 1);
	/* Sightings count ticks whatever the day: a 192-tick day checks
	 * 6 times, a 1 in 16 chance each far from towns. */
	s.encounters = 1;
	for (i = 0, n = 0; i < 100 * 192; i++) {
		if (i % 192 == 0)
			s.fleet.x = s.fleet.y = 300 * WORLD_TILE;
		if (sim_sail(&s, 0, 1, &town) == SAIL_ENCOUNTER) {
			n++;
			s.encounter.stage = ENC_NONE;
		}
	}
	CHECK(n > 20);		/* about 600 / 16 = 37; 12 stretched */
	test_world.ticks_per_day = 0;

	/* Turning: 4 ticks of every 8 from the press, 8 of 16 held, at
	 * any phase of the tick counter. */
	sea_game(&s, 300, 300, 0x00);
	s.fleet.ticks = 3;
	CHECK(sim_sail(&s, 1, 1, &town) == SAIL_TICKS);
	CHECK(s.fleet.heading == 0x10);			/* at once */
	CHECK(sim_sail(&s, 1, 15, &town) == SAIL_TICKS);
	CHECK(s.fleet.heading == 0x80);
	/* A tap while the window is closed is kept, not lost. */
	sea_game(&s, 300, 300, 0x00);
	sim_sail(&s, -1, 1, &town);
	sim_sail(&s, 0, 4, &town);
	sim_sail(&s, -1, 1, &town);			/* closed: kept */
	sim_sail(&s, 0, 2, &town);
	CHECK(s.fleet.heading == 0xF0);
	sim_sail(&s, 0, 1, &town);			/* window opens */
	CHECK(s.fleet.heading == 0xE0);
	sim_sail(&s, 0, 16, &town);
	CHECK(s.fleet.heading == 0xE0);			/* nothing owed */

	/* Land stops the ship on the water, before the coast. */
	sea_game(&s, 120, 60, 0x00);
	CHECK(sim_sail(&s, 0, 2000, &town) == SAIL_LAND);
	CHECK(world_terrain(&test_world, s.fleet.x, s.fleet.y) == TERRAIN_SEA);
	CHECK(s.fleet.y / WORLD_TILE == 50 && s.fleet.speed == 0);

	/* World edge: the ship turns back in. */
	sea_game(&s, 4, 300, 0xC0);
	CHECK(sim_sail(&s, 0, 200, &town) == SAIL_TICKS);
	CHECK(s.fleet.heading == 0x40 && s.fleet.x >= 0x41);

	/* Time: a day per 64 ticks; the wind may only shift every 128. */
	sea_game(&s, 300, 300, 0x00);
	s.fleet.ticks = 0;
	w0 = s.wind;
	sim_sail(&s, 0, 127, &town);
	CHECK(s.day == 1 && s.wind == w0);
	sim_sail(&s, 0, 64 * 30, &town);
	CHECK(s.day == 31 && s.month == 1);

	/* Arrival at a town; not at the one just left. Gamma is at cell
	 * (110, 130): start beside it, leaving it, sail away and back. */
	sea_game(&s, 110, 133, 0x00);
	s.fleet.leaving = 2;
	CHECK(sim_sail(&s, 0, 10, &town) == SAIL_TICKS);	/* in harbour */
	sea_game(&s, 110, 140, 0x00);
	CHECK(sim_sail(&s, 0, 2000, &town) == SAIL_TOWN && town == 2);
	CHECK(s.fleet.leaving == 2 && s.fleet.speed == 0);
}

/* A saved fleet reloads and sails on identically. */
static void
test_sail_snapshot(void)
{
	struct session s1, s2;
	struct jbuf st = { 0 }, cmd = { 0 }, out = { 0 }, r1 = { 0 },
	    r2 = { 0 };

	memset(&s1, 0, sizeof(s1));
	s1.load = loader;
	s1.load_world = world_cb;
	proto_handle(&s1, "{\"cmd\":\"new_game\",\"era\":1660}", &out);
	jb_free(&out);
	s1.sim.fleet.x = 300 * WORLD_TILE;	/* open sea */
	s1.sim.fleet.y = 300 * WORLD_TILE;
	s1.sim.encounters = 0;
	proto_handle(&s1, "{\"cmd\":\"sail\",\"turn\":1,\"ticks\":300}",
	    &out);
	CHECK(out.s != NULL && strstr(out.s, "\"stop\":\"ticks\"") != NULL);
	jb_free(&out);
	sim_to_json(&s1.sim, &st);
	jb_raw(&cmd, "{\"cmd\":\"load\",\"state\":");
	jb_raw(&cmd, st.s);
	jb_raw(&cmd, "}");
	memset(&s2, 0, sizeof(s2));
	s2.load = loader;
	s2.load_world = world_cb;
	proto_handle(&s2, cmd.s, &out);
	CHECK(out.s != NULL && strcmp(out.s, "{\"ok\":true}") == 0);
	jb_free(&out);
	proto_handle(&s1, "{\"cmd\":\"sail\",\"turn\":-1,\"ticks\":500}",
	    &r1);
	proto_handle(&s2, "{\"cmd\":\"sail\",\"turn\":-1,\"ticks\":500}",
	    &r2);
	CHECK(r1.s != NULL && r2.s != NULL && strcmp(r1.s, r2.s) == 0);
	/* Without a world, sailing is refused. */
	s2.load_world = NULL;
	session_free(&s2);
	proto_handle(&s2, "{\"cmd\":\"sail\"}", &out);
	CHECK(out.s != NULL && strstr(out.s, "\"ok\":false") != NULL);
	jb_free(&out);
	session_free(&s1);
	jb_free(&st);
	jb_free(&cmd);
	jb_free(&r1);
	jb_free(&r2);
}

/* Encounters: sightings, the dialogs, and their outcomes. */
static void
test_encounters(void)
{
	struct sim s;
	int town, i, sighted = 0, far = 0, hostile = 0;
	enum encounter_result r;

	/* Far from towns a sail appears in 1 of 16 checks; near them, the
	 * lingering counter makes them rarer and rarer. */
	sea_game(&s, 300, 300, 0x00);	/* the towns are 150+ cells away */
	s.encounters = 1;
	for (i = 0; i < 400; i++) {
		s.encounter.stage = ENC_NONE;
		s.fleet.x = 300 * WORLD_TILE;
		s.fleet.y = 300 * WORLD_TILE;
		if (sim_sail(&s, 0, 32, &town) == SAIL_ENCOUNTER) {
			far++;
			sim_encounter_act(&s, ACT_INVESTIGATE);
			hostile += s.encounter.hostile;
		}
	}
	CHECK(far > 10 && far < 50);	/* about 400 / 16 = 25 */
	CHECK(hostile > 0);	/* pirates are among the records */

	/* Near a town (2 cells from Beta), a first check adds 6 to the
	 * counter: (r & 7) + 1 < 2, 1 in 8.  Lingering shuts sightings off.
	 * Beta's arrival cells are left out: the ship lingers, not arrives. */
	sea_game(&s, 120, 112, 0x00);
	test_world.narrival = 0;
	s.encounters = 1;
	for (i = 0; i < 400; i++) {
		s.encounter.stage = ENC_NONE;
		s.heat = 0;
		s.fleet.x = 120 * WORLD_TILE + 8;
		s.fleet.y = 112 * WORLD_TILE + 8;
		if (sim_sail(&s, 0, 32, &town) == SAIL_ENCOUNTER)
			sighted++;
	}
	CHECK(sighted > 25 && sighted < 80);	/* about 400 / 8 = 50 */
	for (i = 0, sighted = 0; i < 400; i++) {
		s.encounter.stage = ENC_NONE;
		s.fleet.x = 120 * WORLD_TILE + 8;
		s.fleet.y = 112 * WORLD_TILE + 8;
		if (i > 2 && sim_sail(&s, 0, 32, &town) == SAIL_ENCOUNTER)
			sighted++;
		else if (i <= 2)
			sim_sail(&s, 0, 32, &town);
	}
	CHECK(sighted == 0 && s.heat > 100);

	/* While an encounter is open, sailing waits for an answer.  A ship
	 * set by hand, with no town, is kept when investigated. */
	sea_game(&s, 300, 300, 0x00);
	s.encounter.stage = ENC_SIGHTED;
	s.encounter.nation = 1;
	s.encounter.town = -1;
	CHECK(sim_sail(&s, 0, 10, &town) == SAIL_ENCOUNTER);
	CHECK(s.fleet.ticks == 0);

	/* A peaceful ship can be left at any step. */
	s.encounter.hostile = 0;
	CHECK(sim_encounter_act(&s, ACT_IGNORE) == RES_ESCAPED);
	CHECK(s.encounter.stage == ENC_NONE && s.heat == 16);
	s.encounter.stage = ENC_SIGHTED;
	CHECK(sim_encounter_act(&s, ACT_INVESTIGATE) == RES_CONTINUE);
	CHECK(s.encounter.stage == ENC_IDENTIFIED);
	CHECK(sim_encounter_act(&s, ACT_INVESTIGATE) == RES_CONTINUE);
	CHECK(s.encounter.stage == ENC_COLOURS);
	CHECK(sim_encounter_act(&s, ACT_HAIL) == RES_NEWS);
	/* A disliked nation does not answer. */
	s.encounter.stage = ENC_COLOURS;
	s.standing[1] = -4;
	CHECK(sim_encounter_act(&s, ACT_HAIL) == RES_UNANSWERED);
	CHECK(s.battle.active);		/* she opens fire */
	s.battle.active = 0;
	s.encounter.stage = ENC_COLOURS;
	CHECK(sim_encounter_act(&s, ACT_BATTLE) == RES_BATTLE);
	CHECK(s.battle.active && s.encounter.stage == ENC_NONE);
	CHECK(sim_sail(&s, 0, 10, &town) == SAIL_ENCOUNTER);	/* fight! */
	s.battle.active = 0;

	/* A hunter cannot be avoided once seen, and is named at her
	 * colours; at the hardest level a captain always catches a player
	 * who does not close for battle. */
	s.encounter.stage = ENC_IDENTIFIED;
	s.encounter.hostile = 1;
	s.encounter.captain = 0;
	CHECK(sim_encounter_act(&s, ACT_IGNORE) == RES_CONTINUE);
	CHECK(s.encounter.captain != 0);
	s.player.difficulty = 3;
	for (i = 0, hostile = 0; i < 300; i++) {
		s.encounter.stage = ENC_COLOURS;
		r = sim_encounter_act(&s, ACT_IGNORE);
		hostile += r == RES_OVERTAKEN;
		s.battle.active = 0;
	}
	CHECK(hostile == 300);	/* rand(3) < 3 always */
	s.player.difficulty = 0;
	for (i = 0, hostile = 0; i < 300; i++) {
		s.encounter.stage = ENC_COLOURS;
		hostile += sim_encounter_act(&s, ACT_IGNORE) == RES_OVERTAKEN;
	}
	CHECK(hostile == 0);	/* rand(3) < 0 never */
}

static void	battle_game(struct sim *);

/* Find a sail far from the towns; 0 if none comes. */
static int
far_sail(struct sim *s)
{
	int i, town;

	for (i = 0; i < 400; i++) {
		s->encounter.stage = ENC_NONE;
		s->fleet.x = 300 * WORLD_TILE;
		s->fleet.y = 300 * WORLD_TILE;
		if (sim_sail(s, 0, 32, &town) == SAIL_ENCOUNTER)
			return (1);
	}
	return (0);
}

/* What each question reveals, and what each answer costs. */
static void
test_encounter_steps(void)
{
	struct sim s, t;
	struct rng before;
	struct jbuf st = { 0 };
	struct json *j;
	char err[128];
	int i, k, mask, dist, town, standing, w, found = 0;

	/* "Sail ho!" knows only the nearest town: sailing on draws
	 * nothing and names nobody. */
	sea_game(&s, 300, 300, 0x00);
	s.encounters = 1;
	CHECK(far_sail(&s));
	CHECK(s.encounter.stage == ENC_SIGHTED && s.encounter.crew == 0 &&
	    s.encounter.captain == 0 && s.encounter.town >= 0 &&
	    s.encounter.dist > 12);
	before = s.rng;
	mask = s.surnames;
	CHECK(sim_encounter_act(&s, ACT_IGNORE) == RES_ESCAPED);
	CHECK(memcmp(&before, &s.rng, sizeof(before)) == 0 &&
	    s.surnames == mask);

	/* Saved at "Sail ho!", loaded, investigated: the ship is picked
	 * then; a hostile one is named only at her colours. */
	for (i = 0; i < 40 && !found; i++) {
		CHECK(far_sail(&s));
		sim_to_json(&s, &st);
		j = json_parse(st.s, err, sizeof(err));
		CHECK(j != NULL && sim_from_json(&t, j, err, sizeof(err)) == 0);
		json_free(j);
		jb_free(&st);
		t.world = &test_world;
		CHECK(t.encounter.stage == ENC_SIGHTED &&
		    t.encounter.town == s.encounter.town &&
		    t.encounter.dist == s.encounter.dist);
		CHECK(sim_encounter_act(&t, ACT_INVESTIGATE) == RES_CONTINUE);
		CHECK(t.encounter.stage == ENC_IDENTIFIED &&
		    t.encounter.crew > 0 && t.encounter.captain == 0 &&
		    t.encounter.dist == s.encounter.dist);
		if (!t.encounter.hostile) {
			sim_encounter_act(&s, ACT_IGNORE);
			continue;
		}
		found = 1;
		mask = t.surnames;
		CHECK(sim_encounter_act(&t, ACT_INVESTIGATE) == RES_CONTINUE);
		CHECK(t.encounter.stage == ENC_COLOURS &&
		    t.encounter.captain != 0 && t.surnames != mask);
		CHECK(t.encounter.wind_strength >= 0 &&
		    t.encounter.wind_strength <= 2);
	}
	CHECK(found);

	/* The battle's wind strength is the one drawn at her colours. */
	for (w = 0; w < 3; w++) {
		battle_game(&s);
		s.battle.active = 0;
		s.encounter.wind_strength = w;
		sim_battle_start(&s);
		CHECK(s.battle.wind_strength == w);
	}

	/* A captain, hostile or not, overtakes a player who sails away or
	 * hails, at the hardest level always; a ship without one never. */
	sea_game(&s, 300, 300, 0x00);
	s.player.difficulty = 3;
	s.encounter.nation = 1;
	s.encounter.town = -1;
	for (k = 0; k < 2; k++) {
		s.encounter.stage = ENC_COLOURS;
		s.encounter.hostile = 0;
		s.encounter.captain = 3 << 3 | 1 << 6;
		CHECK(sim_encounter_act(&s, k ? ACT_HAIL : ACT_IGNORE) ==
		    RES_OVERTAKEN);
		s.battle.active = 0;
		s.encounter.stage = ENC_COLOURS;
		s.encounter.hostile = 1;
		s.encounter.captain = 0;
		CHECK(sim_encounter_act(&s, k ? ACT_HAIL : ACT_IGNORE) ==
		    (k ? RES_NEWS : RES_ESCAPED));
	}

	/* Within 7 cells of a town: closing for battle and an unanswered
	 * hail lower its field 9, being overtaken does not. */
	sea_game(&s, 120, 112, 0x00);	/* 2 cells from Beta */
	town = sim_nearest_town(&s, &dist);
	CHECK(town >= 0 && dist < 7);
	s.player.difficulty = 3;
	s.encounter.nation = 1;
	s.encounter.town = -1;
	standing = (int16_t)s.towns[town].standing;
	s.encounter.stage = ENC_COLOURS;
	s.encounter.captain = 3 << 3 | 1 << 6;
	CHECK(sim_encounter_act(&s, ACT_IGNORE) == RES_OVERTAKEN);
	CHECK((int16_t)s.towns[town].standing == standing);
	s.battle.active = 0;
	s.encounter.stage = ENC_COLOURS;
	s.encounter.captain = 0;
	CHECK(sim_encounter_act(&s, ACT_BATTLE) == RES_BATTLE);
	CHECK((int16_t)s.towns[town].standing == standing - 1);
	s.battle.active = 0;
	s.encounter.stage = ENC_COLOURS;
	s.standing[1] = -4;
	CHECK(sim_encounter_act(&s, ACT_HAIL) == RES_UNANSWERED);
	CHECK((int16_t)s.towns[town].standing == standing - 2);
}

/* A battle against a merchantman (type 4, 8 guns, 80 men). */
static void
battle_game(struct sim *s)
{
	sea_game(s, 300, 300, 0x00);
	s->fleet.ships[0] = 1;		/* a sloop */
	s->fleet.crew = 40;
	s->fleet.guns = 12;
	memset(&s->encounter, 0, sizeof(s->encounter));
	s->encounter.type = 4;
	s->encounter.nation = 1;
	s->encounter.guns = 8;
	s->encounter.crew = 20;
	s->encounter.role = 4;
	s->encounter.gold = 30;
	sim_battle_start(s);
}

/* Sea battles: setup, gunnery, damage, outcomes and their effects. */
static void
test_battles(void)
{
	struct sim s, s2;
	struct sim_battle *bt = &s.battle;
	int i, ev, hits, gx, gy;
	long gold;

	make_world(&test_world);

	/* Setup: guns limited by crew/4 and by the class ((1 + 2) * 4). */
	battle_game(&s);
	CHECK(bt->active && bt->outcome == BAT_NONE);
	CHECK(bt->you.crew == 40 && bt->you.guns == 10);
	CHECK(bt->guns_ashore == 2 && bt->crew_ashore == 0);
	CHECK(bt->foe.crew == 80 && bt->foe.guns == 8 && bt->foe.type == 4);
	/* No men: none aboard and none ashore, never fewer (VA 0x9832). */
	sea_game(&s, 300, 300, 0x00);
	s.fleet.crew = 0;
	sim_battle_start(&s);
	CHECK(bt->you.crew == 0 && bt->crew_ashore == 0);
	battle_game(&s);
	CHECK(bt->you.turn_rate == 3 && bt->foe.turn_rate == 2);
	gx = abs(bt->you.x - bt->foe.x) / 256;
	gy = abs(bt->you.y - bt->foe.y) / 256;
	CHECK(gx >= 64 && gx <= 94 && gy >= 64 && gy <= 94);
	CHECK(!bt->you.full_sails && !bt->foe.full_sails);
	/* The enemy 0x60 to the player's left, give or take 16. */
	CHECK(abs((signed char)(uint8_t)(bt->foe.heading - bt->you.heading +
	    0x60)) <= 16);

	/* Firing: one shot, then a reload of |144 - 8 x (mood / 2 + 4)|
	 * ticks: 112 with a neutral crew. */
	s.fleet.mood = 0;
	ev = sim_battle_tick(&s, 0, 1, 0);
	CHECK((ev & BEV_FIRED) && bt->you.reload == 112);
	CHECK(bt->you.shot.ticks > 0);
	CHECK((sim_battle_tick(&s, 0, 1, 0) & BEV_FIRED) == 0);

	/* The debug mode's invulnerability: the enemy holds his fire. */
	battle_game(&s);
	s.invulnerable = 1;
	for (i = 0, ev = 0; i < 2000 && bt->outcome == BAT_NONE; i++)
		ev |= sim_battle_tick(&s, 0, 0, 0);
	CHECK(!(ev & BEV_FOE_FIRED));
	s.invulnerable = 0;

	/* Sails take 160 ticks to change; the guns wait meanwhile. */
	battle_game(&s);
	CHECK(sim_battle_tick(&s, 0, 0, 1) == 0 && bt->you.sail_timer == 160);
	for (i = 0, ev = 0; i < 160 && bt->outcome == BAT_NONE; i++)
		ev |= sim_battle_tick(&s, 0, 0, 0);
	CHECK((ev & BEV_SAILS) && bt->you.full_sails);
	/* Up again keeps them full; Down lowers them. */
	CHECK(sim_battle_tick(&s, 0, 0, 1) == 0 && bt->you.sail_timer == 0);
	CHECK(sim_battle_tick(&s, 0, 0, -1) == 0 && bt->you.sail_timer == 160);
	for (i = 0, ev = 0; i < 160 && bt->outcome == BAT_NONE; i++)
		ev |= sim_battle_tick(&s, 0, 0, 0);
	CHECK((ev & BEV_SAILS) && !bt->you.full_sails);
	CHECK(sim_battle_tick(&s, 0, 0, -1) == 0 && bt->you.sail_timer == 0);

	/* Steering: 16 at the press, then 16 per 8 ticks held, never
	 * faster. */
	battle_game(&s);
	gx = bt->you.heading;
	sim_battle_tick(&s, 1, 0, 0);
	CHECK(bt->you.heading == ((gx + 16) & 0xFF));	/* at once */
	for (i = 0; i < 7; i++)
		sim_battle_tick(&s, 1, 0, 0);
	CHECK(bt->you.heading == ((gx + 16) & 0xFF));
	sim_battle_tick(&s, 1, 0, 0);
	CHECK(bt->you.heading == ((gx + 32) & 0xFF));	/* held: 8 later */
	/* A tap during the wait is kept and served when it ends; taps do
	 * not turn faster than holding. */
	battle_game(&s);
	gx = bt->you.heading;
	sim_battle_tick(&s, -1, 0, 0);			/* turns */
	sim_battle_tick(&s, 0, 0, 0);
	sim_battle_tick(&s, -1, 0, 0);			/* kept */
	for (i = 0; i < 4; i++)
		sim_battle_tick(&s, 0, 0, 0);
	CHECK(bt->you.heading == ((gx - 16) & 0xFF));
	sim_battle_tick(&s, 0, 0, 0);
	sim_battle_tick(&s, 0, 0, 0);
	CHECK(bt->you.heading == ((gx - 32) & 0xFF));	/* 8 after the first */
	for (i = 0; i < 8; i++)
		sim_battle_tick(&s, 0, 0, 0);
	CHECK(bt->you.heading == ((gx - 32) & 0xFF));	/* nothing owed */

	/* A shot at point blank hits: the hull takes guns x 4 / (class + 2),
	 * halved under battle sails, and half again at difficulty 0. */
	battle_game(&s);
	bt->foe.x = bt->you.x + 60 * 256;	/* abeam, to starboard */
	bt->foe.y = bt->you.y;
	bt->you.heading = 0;
	bt->foe.heading = 0;
	bt->foe.target = 0;
	bt->foe.guns = 0;			/* she cannot answer */
	sim_battle_tick(&s, 0, 1, 0);
	CHECK(bt->sound == FM_BROADSIDE);	/* the tick's sound */
	for (i = 0, hits = 0; i < 40 && bt->outcome == BAT_NONE; i++)
		if (sim_battle_tick(&s, 0, 0, 0) & BEV_HIT_FOE) {
			hits++;
			CHECK(bt->sound == FM_SHOT_HIT);
		}
	CHECK(hits == 1);
	CHECK(s.player.difficulty == 0 && bt->foe.damage == 4);	/* 6, 3 */
	CHECK(bt->foe.crew < 80);
	/* At difficulty 1 the bonus is gone, unless gunnery brings the
	 * level back to 0. */
	for (i = 0; i < 2; i++) {
		int j;

		battle_game(&s);
		s.player.difficulty = 1;
		s.player.ability = i == 0 ? ABILITY_FENCING : ABILITY_GUNNERY;
		bt->foe.x = bt->you.x + 60 * 256;
		bt->foe.y = bt->you.y;
		bt->you.heading = bt->foe.heading = bt->foe.target = 0;
		bt->foe.guns = 0;
		sim_battle_tick(&s, 0, 1, 0);
		for (j = 0; j < 40 && bt->outcome == BAT_NONE; j++)
			sim_battle_tick(&s, 0, 0, 0);
		CHECK(bt->foe.damage == (i == 0 ? 3 : 4));
	}

	/* A shot's arc steps every flight / 16 ticks, up to 15; once it
	 * lands the guns wait while its splash or burst shows, 38 ticks,
	 * whatever the reload. */
	battle_game(&s);
	bt->foe.x = bt->you.x + 120 * 256;
	bt->foe.y = bt->you.y;
	bt->you.heading = bt->foe.heading = bt->foe.target = 0;
	bt->foe.guns = 0;
	CHECK(sim_battle_tick(&s, 0, 1, 0) & BEV_FIRED);
	gx = bt->you.shot.ticks + 1;		/* the flight, one flown */
	CHECK(gx == 60 && bt->you.shot.period == gx >> 4);
	CHECK(bt->you.shot.landing == 0 && bt->you.shot.hit == 0);
	for (i = 1, ev = 1; bt->you.shot.ticks > 0 && bt->outcome == BAT_NONE;
	    i++) {
		ev &= bt->you.shot.step == (i / 3 < 15 ? i / 3 : 15);
		bt->you.reload = 0;
		ev &= !(sim_battle_tick(&s, 0, 1, 0) & BEV_FIRED);
	}
	CHECK(ev && i == gx && bt->you.shot.step == 15);
	CHECK(bt->you.shot.landing == 1);
	for (i = 0, ev = 0; i < 38 && bt->outcome == BAT_NONE; i++) {
		bt->you.reload = 0;
		ev |= sim_battle_tick(&s, 0, 1, 0) & BEV_FIRED;
		CHECK(bt->you.shot.landing == i + 2);
	}
	CHECK(!ev && i == 38);
	bt->you.reload = 0;
	CHECK(sim_battle_tick(&s, 0, 1, 0) & BEV_FIRED);
	CHECK(bt->you.shot.landing == 0 && bt->you.shot.step == 0);
	/* Without firing, the count clears after its last tick. */
	bt->you.shot.ticks = 0;
	bt->you.shot.landing = 39;
	sim_battle_tick(&s, 0, 0, 0);
	CHECK(bt->you.shot.landing == 0);
	/* A shot of fewer than 16 ticks keeps the first step. */
	bt->you.shot.ticks = 10;
	bt->you.shot.period = bt->you.shot.wait = bt->you.shot.step = 0;
	sim_battle_tick(&s, 0, 0, 0);
	CHECK(bt->you.shot.step == 0 && bt->you.shot.ticks == 9);
	battle_game(&s);
	bt->foe.x = bt->you.x + 60 * 256;
	bt->foe.y = bt->you.y;
	bt->you.heading = bt->foe.heading = bt->foe.target = 0;
	bt->foe.guns = 0;
	bt->foe.damage = 4;

	/* Sinking at damage 64; the player keeps her prize money only from
	 * surrenders. */
	bt->foe.damage = 63;
	bt->foe.full_sails = 1;
	sim_battle_tick(&s, 0, 1, 0);
	for (i = 0; i < 300 && bt->outcome == BAT_NONE; i++)
		sim_battle_tick(&s, 0, 1, 0);
	CHECK(bt->outcome == BAT_ENEMY_SUNK);
	gold = s.fleet.gold;
	{
		int d, near = sim_nearest_town(&s, &d), w;

		CHECK(near >= 0);
		s.towns[near].wealth = 50;
		CHECK(sim_battle_finish(&s, 1) == BAT_ENEMY_SUNK);
		/* A ship sunk costs the nearest town 10 wealth, however
		 * far. */
		w = s.towns[near].wealth;
		CHECK(w == 40);
	}
	CHECK(!bt->active && s.fleet.nships == 1 && s.fleet.gold == gold);
	CHECK(s.fleet.guns == bt->you.guns + 2);

	/* Escape: more than 240 pixels apart on an axis. */
	battle_game(&s);
	bt->foe.x = bt->you.x + 239 * 256;
	bt->foe.y = bt->you.y;
	bt->you.heading = 0xC0;		/* west, away from her */
	bt->you.full_sails = 1;
	for (i = 0; i < 400 && bt->outcome == BAT_NONE; i++)
		sim_battle_tick(&s, 0, 0, 0);
	CHECK(bt->outcome == BAT_ESCAPED);

	/* Boarding: the ships touch; the sword fight decides.  Nothing is
	 * applied before it is fought. */
	battle_game(&s);
	bt->foe.x = bt->you.x;
	bt->foe.y = bt->you.y;
	sim_battle_tick(&s, 0, 0, 0);
	CHECK(bt->outcome == BAT_BOARDING);
	CHECK(sim_battle_finish(&s, 1) == BAT_NONE && bt->active);
	sim_board(&s, 2);
	CHECK(s.sword.active && s.sword.kind == SWORD_CREW);
	CHECK(s.sword.you.men == 40 && s.sword.foe.men == 80);
	CHECK(s.sword.you.bar == 31 * (40 * 7) / (40 * 7 + 80 * 4) + 8);
	CHECK(sim_battle_finish(&s, 1) == BAT_NONE);	/* still fighting */
	s.sword.result = SWORD_WON;
	s.sword.you.men = 30;			/* 10 fell */
	gold = s.fleet.gold;
	CHECK(sim_battle_finish(&s, 1) == BAT_SURRENDER);
	CHECK(s.fleet.nships == 2 && s.fleet.ships[1] == 4);
	CHECK(s.fleet.gold == gold + bt->loot_gold &&
	    bt->loot_gold % 10 == 0 && s.fleet.crew == 30);
	CHECK(!s.sword.active && !bt->active);
	/* A named captain fights under his name, (id & 7) / 3 levels
	 * harder, or one with bit 0x80 (VA 0xf1df). */
	battle_game(&s);
	bt->outcome = BAT_BOARDING;
	s.encounter.captain = 0;
	sim_board(&s, 0);
	{
		int level;

		CHECK(s.sword.title == 4 && s.sword.nation == s.encounter.nation);
		battle_game(&s);
		bt->outcome = BAT_BOARDING;
		s.encounter.captain = 0x16;	/* id & 7 = 6: 2 levels */
		sim_board(&s, 0);
		level = s.player.difficulty + 2 -
		    (s.player.ability == ABILITY_FENCING);
		CHECK(s.sword.title == 0x16 && s.sword.nation == -1 &&
		    s.sword.level == (level < 0 ? 0 : level > 4 ? 4 : level));
		s.encounter.captain = 0;
	}
	/* Driven back: no prize, the losses stay. */
	battle_game(&s);
	bt->outcome = BAT_BOARDING;
	sim_board(&s, 0);
	s.sword.result = SWORD_REPELLED;
	s.sword.you.men = 25;
	{
		int heat = s.heat, rep = s.reputation;

		CHECK(sim_battle_finish(&s, 1) == BAT_REPELLED);
		/* The heat rises by 16 (no town near), the reputation
		 * drops by 2 (VA 0xe775). */
		CHECK(s.heat == heat + 16 && s.reputation == rep - 2);
	}
	CHECK(s.fleet.nships == 1 && s.fleet.crew == 25);
	/* Lost: prison aboard (an injury), then a fresh start in the
	 * same type of ship. */
	battle_game(&s);
	bt->outcome = BAT_BOARDING;
	sim_board(&s, 0);
	s.sword.result = SWORD_LOST;
	int ship0 = s.fleet.ships[0] & 7;
	CHECK(sim_battle_finish(&s, 1) == BAT_DEFEATED);
	CHECK(s.fleet.nships == 1 && s.fleet.ships[0] == ship0 &&
	    s.jail.fresh && s.jail.cause == JAIL_SHIP && s.injuries == 1);

	/* Surrender: an outmatched enemy alongside strikes her colours at
	 * the next decision. */
	battle_game(&s);
	bt->foe.x = bt->you.x + 50 * 256;
	bt->foe.y = bt->you.y + 30 * 256;
	bt->foe.damage = 40;
	bt->foe.guns = 2;
	bt->foe.crew = 10;
	bt->foe.speed = 0;
	for (i = 0; i < 64 && bt->outcome == BAT_NONE; i++) {
		bt->foe.x = bt->you.x + 50 * 256;
		bt->foe.y = bt->you.y + 30 * 256;
		sim_battle_tick(&s, 0, 0, 0);
	}
	CHECK(bt->outcome == BAT_SURRENDER && bt->tick == 64);
	CHECK(sim_battle_finish(&s, 0) == BAT_SURRENDER);
	CHECK(s.fleet.nships == 1);	/* plundered, not kept */

	/* Exact limits, with both ships sailing north side by side at the
	 * same speed: boarding when the quarter boxes touch (5 + 7 = 12
	 * pixels across), escape beyond 240 pixels. */
	for (i = 0; i < 4; i++) {
		static const int gap[4] = { 12, 13, 240, 241 };
		static const int want[4] = { BAT_BOARDING, BAT_NONE, BAT_NONE,
		    BAT_ESCAPED };

		battle_game(&s);
		bt->wind_strength = 0;
		bt->you.heading = bt->foe.heading = bt->foe.target = 0;
		bt->foe.x = bt->you.x + gap[i] * 256;
		bt->foe.y = bt->you.y;
		sim_battle_tick(&s, 0, 0, 0);
		CHECK(bt->you.speed == bt->foe.speed);
		CHECK(bt->outcome == want[i]);
	}

	/* Sunset after 4500 ticks without a hit; a hit starts them
	 * again and turns the enemy to the wind. */
	battle_game(&s);
	bt->quiet = 4499;
	bt->foe.x = bt->you.x + 100 * 256;
	sim_battle_tick(&s, 0, 0, 0);
	CHECK(bt->outcome == BAT_NONE && bt->quiet == 4500);
	bt->foe.full_sails = 1;
	bt->you.shot.ticks = 1;
	bt->you.shot.x = bt->foe.x;
	bt->you.shot.y = bt->foe.y;
	bt->foe.target = (uint8_t)(bt->wind + 0x40);
	sim_battle_tick(&s, 0, 0, 0);
	CHECK(bt->outcome == BAT_NONE && bt->quiet == 0 &&
	    bt->foe.damage > 0 && bt->foe.target == bt->wind);
	bt->quiet = 4500;
	sim_battle_tick(&s, 0, 0, 0);
	CHECK(bt->outcome == BAT_SUNSET);

	/* Deterministic: the same start and inputs give the same battle. */
	battle_game(&s);
	battle_game(&s2);
	for (i = 0; i < 2000; i++) {
		sim_battle_tick(&s, i % 3 - 1, i % 50 == 0, i == 700);
		sim_battle_tick(&s2, i % 3 - 1, i % 50 == 0, i == 700);
	}
	CHECK(memcmp(&s.battle, &s2.battle, sizeof(s.battle)) == 0);
	CHECK(bt->tick > 0);
}

/* A battle through the protocol: snapshots mid-fight replay the same. */
static void
test_battle_protocol(void)
{
	struct session s1, s2;
	struct jbuf st = { 0 }, cmd = { 0 }, out = { 0 }, r1 = { 0 },
	    r2 = { 0 };

	memset(&s1, 0, sizeof(s1));
	s1.load = loader;
	s1.load_world = world_cb;
	proto_handle(&s1, "{\"cmd\":\"new_game\",\"era\":1660}", &out);
	jb_free(&out);
	proto_handle(&s1, "{\"cmd\":\"battle\"}", &out);
	CHECK(out.s != NULL && strstr(out.s, "no battle") != NULL);
	jb_free(&out);
	s1.sim.encounter.stage = ENC_COLOURS;
	s1.sim.encounter.type = 4;
	s1.sim.encounter.guns = 8;
	s1.sim.encounter.crew = 20;
	s1.sim.encounter.role = 4;
	proto_handle(&s1, "{\"cmd\":\"encounter\",\"action\":\"battle\"}",
	    &out);
	CHECK(out.s != NULL && strstr(out.s, "\"battle\"") != NULL);
	jb_free(&out);
	proto_handle(&s1, "{\"cmd\":\"finish\"}", &out);
	CHECK(out.s != NULL && strstr(out.s, "no finished battle") != NULL);
	jb_free(&out);
	proto_handle(&s1, "{\"cmd\":\"battle\",\"ticks\":100,\"fire\":1,"
	    "\"turn\":1}", &out);
	CHECK(out.s != NULL && strstr(out.s, "\"tick\":") != NULL &&
	    strstr(out.s, "\"events\":") != NULL);
	jb_free(&out);
	proto_handle(&s1, "{\"cmd\":\"battle\",\"turn\":2}", &out);
	CHECK(out.s != NULL && strstr(out.s, "\"ok\":false") != NULL);
	jb_free(&out);

	sim_to_json(&s1.sim, &st);
	jb_raw(&cmd, "{\"cmd\":\"load\",\"state\":");
	jb_raw(&cmd, st.s);
	jb_raw(&cmd, "}");
	memset(&s2, 0, sizeof(s2));
	s2.load = loader;
	s2.load_world = world_cb;
	proto_handle(&s2, cmd.s, &out);
	CHECK(out.s != NULL && strcmp(out.s, "{\"ok\":true}") == 0);
	jb_free(&out);
	CHECK(memcmp(&s1.sim.battle, &s2.sim.battle,
	    sizeof(s1.sim.battle)) == 0);
	proto_handle(&s1, "{\"cmd\":\"battle\",\"ticks\":3000,\"fire\":1}",
	    &r1);
	proto_handle(&s2, "{\"cmd\":\"battle\",\"ticks\":3000,\"fire\":1}",
	    &r2);
	CHECK(r1.s != NULL && r2.s != NULL && strcmp(r1.s, r2.s) == 0);
	if (s1.sim.battle.outcome != BAT_NONE) {
		proto_handle(&s1, "{\"cmd\":\"finish\",\"keep\":1}", &out);
		CHECK(out.s != NULL && strstr(out.s, "\"outcome\":") != NULL);
		CHECK(!s1.sim.battle.active);
		jb_free(&out);
	}
	session_free(&s1);
	session_free(&s2);
	jb_free(&st);
	jb_free(&cmd);
	jb_free(&r1);
	jb_free(&r2);
}

/* A crew fight at "dist" pixels: 40 men (factor 7) against 80 (4). */
static void
sword_game(struct sim *s, int dist)
{
	struct sword_setup in;

	sea_game(s, 300, 300, 0);
	memset(&in, 0, sizeof(in));
	in.kind = SWORD_CREW;
	in.you_men = 40;
	in.you_factor = 7;
	in.foe_men = 80;
	in.foe_factor = 4;
	in.title = 4;
	in.nation = 1;
	in.weapon = 2;			/* cutlass */
	sim_sword_start(s, &in);
	s->sword.foe.x = s->sword.you.x - dist;
}

/* Sword fights: setup, moves, hits, the bar, the melee and the end. */
static void
test_sword(void)
{
	struct sim s, s2;
	struct sim_sword *sw = &s.sword;
	int i, k, ev, x, snd, eff;

	make_world(&test_world);

	/* Setup: the bar from men x factor, start positions, level. */
	sword_game(&s, 120);
	CHECK(sw->active && sw->result == SWORD_NONE);
	CHECK(sw->you.bar == 31 * 280 / 600 + 8 && sw->foe.bar == 47 - 22);
	CHECK(sw->you.x == 340 && sw->foe.x == 220);
	CHECK(sw->foe.weapon == 1 || sw->foe.weapon == 2);
	CHECK(sw->you.move == -1 && sw->level == 0);
	{
		struct sword_setup in;

		memset(&in, 0, sizeof(in));
		in.you_men = in.foe_men = in.you_factor = in.foe_factor = 1;
		s.player.difficulty = 2;
		s.player.ability = ABILITY_FENCING;
		sim_sword_start(&s, &in);
		CHECK(sw->level == 1);
		in.extra = 9;
		sim_sword_start(&s, &in);
		CHECK(sw->level == 4);		/* clamped */
		CHECK(sw->foe.weapon == 1 || sw->foe.weapon == 2);
		in.kind = SWORD_DUEL;
		sim_sword_start(&s, &in);
		CHECK(sw->foe.weapon == 0 && sw->you.x == 320 &&
		    sw->foe.x == 240);
	}

	/* The opponent starts on the middle guard.  A high slash hits on
	 * frame 3: the cutlass moves the bar 5 x 2 steps, pushes back 4, and
	 * wounds the head. */
	sword_game(&s, 25);
	sim_sword_input(&s, SWM_SLASH + 2);
	x = sw->foe.x;
	for (i = 0, ev = 0, snd = 0, eff = -1; i < 4; i++)
		if ((ev |= sim_sword_tick(&s)) & SEV_HIT_FOE && snd == 0) {
			snd = sw->sound;	/* the blow's sound, digitised 7 */
			eff = sw->effect;
		}
	/* Within 36 the original goes on to the clash test: the blow
	 * sounds as blades meeting (VA 0x279c0). */
	CHECK(snd == FM_SWING && eff == 6);
	CHECK((ev & SEV_HIT_FOE) && sw->you.move == SWM_SLASH + 2);
	CHECK(sw->you.bar == 32 && sw->foe.bar == 15);
	CHECK(sw->foe.x == x - 4 && sw->foe.wounds[0] == 1);
	CHECK(sw->input == SWM_GUARD);		/* taken once */
	/* From further, with a longer reach, the blow's own sound. */
	{
		int reach = test_world.sword.reach_weapon[2];

		test_world.sword.reach_weapon[2] = 40;
		sword_game(&s, 40);
		sim_sword_input(&s, SWM_SLASH + 2);
		for (i = 0, ev = 0, snd = 0, eff = -1; i < 4; i++)
			if ((ev |= sim_sword_tick(&s)) & SEV_HIT_FOE &&
			    snd == 0) {
				snd = sw->sound;
				eff = sw->effect;
			}
		CHECK(snd == FM_BLOW && eff == 7);
		test_world.sword.reach_weapon[2] = (uint8_t)reach;
	}
	/* The middle line is guarded: a clash, no hit. */
	sword_game(&s, 25);
	sim_sword_input(&s, SWM_SLASH);
	for (i = 0, ev = 0, snd = 0, eff = -1; i < 4; i++)
		if ((ev |= sim_sword_tick(&s)) & SEV_CLASH && snd == 0) {
			snd = sw->sound;	/* blades meet, digitised 6 */
			eff = sw->effect;
		}
	CHECK(snd == FM_SWING && eff == 6);
	CHECK((ev & SEV_CLASH) && !(ev & SEV_HIT_FOE));
	/* Out of reach (10 + 20): a miss. */
	sword_game(&s, 31);
	sim_sword_input(&s, SWM_SLASH + 2);
	for (i = 0, ev = 0, snd = 0; i < 4; i++) {
		ev |= sim_sword_tick(&s);
		if (sw->sound == FM_MISS)
			snd = FM_MISS;		/* FM only: nothing digitised */
	}
	CHECK(snd == FM_MISS);
	CHECK(!(ev & SEV_HIT_FOE) && sw->foe.bar == 25);
	/* The debug mode's invulnerability (VA 0x27f9c, 0x27d50): the
	 * same melee that costs the player men leaves them standing. */
	for (k = 0; k < 2; k++) {
		int bar;

		sword_game(&s, 25);
		s.invulnerable = k;
		bar = sw->you.bar;
		for (i = 0, ev = 0; i < 400 && sw->result == SWORD_NONE; i++)
			ev |= sim_sword_tick(&s);
		if (k == 0)
			CHECK((ev & SEV_YOU_MEN) && sw->you.men < 40);
		else
			CHECK(!(ev & SEV_YOU_MEN) && sw->you.men == 40 &&
			    sw->you.bar >= bar);
	}
	s.invulnerable = 0;
	/* A lunge strikes on frame 2 and moves the bar one step a pass. */
	sword_game(&s, 25);
	sim_sword_input(&s, SWM_LUNGE + 1);
	sim_sword_tick(&s);
	sim_sword_tick(&s);
	CHECK(sim_sword_tick(&s) & SEV_HIT_FOE);	/* frame 2: tick 3 */
	CHECK(sw->foe.bar == 20 && sw->foe.wounds[2] == 1);

	/* At a distance an attack becomes an advance of 14. */
	sword_game(&s, 100);
	sim_sword_input(&s, SWM_SLASH);
	x = sw->you.x;
	for (i = 0; i < 6; i++)
		sim_sword_tick(&s);
	CHECK(sw->you.move == SWM_ADVANCE && sw->you.x == x - 14);
	/* The limit is 59: an attack there stays an attack. */
	sword_game(&s, 59);
	sim_sword_input(&s, SWM_SLASH);
	sim_sword_tick(&s);
	CHECK(sw->you.move == SWM_SLASH);
	sword_game(&s, 60);
	sim_sword_input(&s, SWM_SLASH);
	sim_sword_tick(&s);
	CHECK(sw->you.move == SWM_ADVANCE);
	/* A lopsided bar: 31 x 100 / 101 + 8. */
	{
		struct sword_setup in;

		memset(&in, 0, sizeof(in));
		in.you_men = 100;
		in.foe_men = in.you_factor = in.foe_factor = 1;
		sim_sword_start(&s, &in);
		CHECK(sw->you.bar == 38 && sw->foe.bar == 9);
	}
	/* Up close, a cautious opponent stands on guard 1. */
	sword_game(&s, 15);
	for (i = 0; i < 6; i++)
		sim_sword_tick(&s);
	CHECK(sw->foe.move == SWM_GUARD + 1);
	/* Stepping back repeats while the input stays. */
	sword_game(&s, 100);
	sim_sword_input(&s, SWM_BACK);
	x = sw->you.x;
	for (i = 0; i < 12; i++)
		sim_sword_tick(&s);
	CHECK(sw->you.x == x + 28 && sw->input == SWM_BACK);
	/* Close in, stepping back still works; only stepping in waits for
	 * 10 apart (VA 0x27a2c). */
	sword_game(&s, 5);
	sim_sword_input(&s, SWM_BACK);
	x = sw->you.x;
	sim_sword_tick(&s);
	CHECK(sw->you.x == x + 14);

	/* A hit leaves the hitter guarding every line (VA 0x278dd): the
	 * opponent's strike that tick is covered. */
	sword_game(&s, 25);
	sim_sword_input(&s, SWM_SLASH + 2);
	for (i = 0, ev = 0; i < 4 && !(ev & SEV_HIT_FOE); i++)
		ev = sim_sword_tick(&s);
	CHECK((ev & SEV_HIT_FOE) && sw->you.guard == 7);

	/* The hit that empties the opponent's bar fells him; he lies
	 * after frame 8 and the player wins. */
	sword_game(&s, 25);
	sw->foe.bar = 3;
	sim_sword_input(&s, SWM_SLASH + 2);
	for (i = 0; i < 4; i++)
		sim_sword_tick(&s);
	CHECK(sw->beaten == 1 && sw->foe.move == SWM_FALL);
	for (i = 0; i < 20 && sw->result == SWORD_NONE; i++)
		sim_sword_tick(&s);
	CHECK(sw->result == SWORD_WON && sw->foe.frame == 9);

	/* Edges: the opponent driven to x 50 loses; the player at 450 is
	 * driven back. */
	sword_game(&s, 100);
	sw->foe.x = 64;			/* backs off 14 on tick 6: 50 */
	sw->you.x = 164;
	memset(test_world.sword.far_ai, SWM_RETREAT, 8);
	for (i = 0; i < 6; i++)
		sim_sword_tick(&s);
	CHECK(sw->foe.x == 50 && sw->result == SWORD_WON);
	memset(test_world.sword.far_ai, SWM_GUARD, 8);
	sword_game(&s, 100);
	sw->you.x = 436;		/* steps back 14 on tick 1: 450 */
	sw->foe.x = 300;
	sim_sword_input(&s, SWM_BACK);
	sim_sword_tick(&s);
	CHECK(sw->you.x == 450 && sw->result == SWORD_REPELLED);

	/* The crew melee runs on the 15th tick (counter 16): someone loses
	 * ((40 + 80) >> 6) + 1 = 2 men and the bar moves one step. */
	sword_game(&s, 100);
	for (i = 0, ev = 0; i < 14; i++)
		ev |= sim_sword_tick(&s);
	CHECK(!(ev & (SEV_FOE_MEN | SEV_YOU_MEN)));
	ev = sim_sword_tick(&s);
	CHECK((ev & SEV_FOE_MEN) ? sw->foe.men == 78 && sw->you.bar == 23 :
	    (ev & SEV_YOU_MEN) && sw->you.men == 38 && sw->you.bar == 21);
	/* None in duels. */
	sword_game(&s, 100);
	sw->kind = SWORD_DUEL;
	for (i = 0, ev = 0; i < 40; i++)
		ev |= sim_sword_tick(&s);
	CHECK(!(ev & (SEV_FOE_MEN | SEV_YOU_MEN)));

	/* The opponent's script: up close and not cautious, 0x90 parries
	 * the player's line (here low, after a low slash). */
	memset(test_world.sword.caution, 0, sizeof(test_world.sword.caution));
	sword_game(&s, 15);
	sim_sword_input(&s, SWM_SLASH + 1);
	for (i = 0; i < 6; i++)
		sim_sword_tick(&s);
	CHECK(sw->foe.move == SWM_GUARD + 1);
	/* 0x92 slashes in one of the other lines: the player now stands
	 * on guard 0 (middle), so low or high. */
	memset(test_world.sword.scripts, 0x92, sizeof(test_world.sword.scripts));
	for (i = 0; i < 6; i++)
		sim_sword_tick(&s);
	CHECK(sw->you.move == SWM_GUARD);
	CHECK(sw->foe.move == SWM_SLASH + 1 || sw->foe.move == SWM_SLASH + 2);
	{
		int seen[3] = { 0, 0, 0 };

		for (x = 0; x < 40; x++) {
			sword_game(&s, 15);
			s.rng.state += (uint64_t)x * 7919;
			for (i = 0; i < 12; i++)
				sim_sword_tick(&s);
			if (sw->foe.move >= SWM_SLASH &&
			    sw->foe.move < SWM_SLASH + 3)
				seen[sw->foe.move - SWM_SLASH]++;
		}
		CHECK(seen[0] == 0 && seen[1] > 0 && seen[2] > 0);
	}
	memset(test_world.sword.scripts, 0x90, sizeof(test_world.sword.scripts));
	memset(test_world.sword.caution, 15, sizeof(test_world.sword.caution));

	/* Deterministic. */
	sword_game(&s, 40);
	sword_game(&s2, 40);
	for (i = 0; i < 300; i++) {
		if (i % 7 == 0) {
			sim_sword_input(&s, i % 12);
			sim_sword_input(&s2, i % 12);
		}
		sim_sword_tick(&s);
		sim_sword_tick(&s2);
	}
	CHECK(memcmp(&s.sword, &s2.sword, sizeof(s.sword)) == 0);
}

/* Boarding through the protocol, and a snapshot taken mid-fight. */
static void
test_sword_protocol(void)
{
	struct session s1, s2;
	struct jbuf st = { 0 }, cmd = { 0 }, out = { 0 }, r1 = { 0 },
	    r2 = { 0 };

	memset(&s1, 0, sizeof(s1));
	s1.load = loader;
	s1.load_world = world_cb;
	proto_handle(&s1, "{\"cmd\":\"new_game\",\"era\":1660}", &out);
	jb_free(&out);
	proto_handle(&s1, "{\"cmd\":\"board\"}", &out);
	CHECK(out.s != NULL && strstr(out.s, "no boarding") != NULL);
	jb_free(&out);
	s1.sim.encounter.type = 4;
	s1.sim.encounter.guns = 8;
	s1.sim.encounter.crew = 20;
	s1.sim.encounter.role = 4;
	sim_battle_start(&s1.sim);
	s1.sim.battle.outcome = BAT_BOARDING;
	proto_handle(&s1, "{\"cmd\":\"finish\"}", &out);
	CHECK(out.s != NULL && strstr(out.s, "board and fight") != NULL);
	jb_free(&out);
	proto_handle(&s1, "{\"cmd\":\"board\",\"weapon\":3}", &out);
	CHECK(out.s != NULL && strstr(out.s, "\"ok\":false") != NULL);
	jb_free(&out);
	proto_handle(&s1, "{\"cmd\":\"board\",\"weapon\":0}", &out);
	CHECK(out.s != NULL && strstr(out.s, "\"sword\":{\"active\":1") !=
	    NULL);
	jb_free(&out);
	proto_handle(&s1, "{\"cmd\":\"sword\",\"ticks\":30,\"input\":6}",
	    &out);
	CHECK(out.s != NULL && strstr(out.s, "\"events\":") != NULL);
	jb_free(&out);

	sim_to_json(&s1.sim, &st);
	jb_raw(&cmd, "{\"cmd\":\"load\",\"state\":");
	jb_raw(&cmd, st.s);
	jb_raw(&cmd, "}");
	memset(&s2, 0, sizeof(s2));
	s2.load = loader;
	s2.load_world = world_cb;
	proto_handle(&s2, cmd.s, &out);
	CHECK(out.s != NULL && strcmp(out.s, "{\"ok\":true}") == 0);
	jb_free(&out);
	CHECK(memcmp(&s1.sim.sword, &s2.sim.sword,
	    sizeof(s1.sim.sword)) == 0);
	proto_handle(&s1, "{\"cmd\":\"sword\",\"ticks\":3000,\"input\":9}",
	    &r1);
	proto_handle(&s2, "{\"cmd\":\"sword\",\"ticks\":3000,\"input\":9}",
	    &r2);
	CHECK(r1.s != NULL && r2.s != NULL && strcmp(r1.s, r2.s) == 0);
	if (s1.sim.sword.result != SWORD_NONE) {
		proto_handle(&s1, "{\"cmd\":\"finish\",\"keep\":1}", &out);
		CHECK(out.s != NULL && strstr(out.s, "\"outcome\":") != NULL);
		CHECK(!s1.sim.battle.active && !s1.sim.sword.active);
		jb_free(&out);
	}
	session_free(&s1);
	session_free(&s2);
	jb_free(&st);
	jb_free(&cmd);
	jb_free(&r1);
	jb_free(&r2);
}

/* Town visits: the visit flags and what the player knows. */
static void
test_towns(void)
{
	struct sim s;
	struct session ss;
	struct jbuf out = { 0 };

	sea_game(&s, 300, 300, 0);
	s.towns[1].flags = 0x20;		/* obsolete information */
	sim_enter_town(&s, 1);
	CHECK(s.player_town == 1 && s.visit_town == 1);
	CHECK((s.towns[1].flags & 0x80) && !(s.towns[1].flags & 0x20));
	s.visit_flags = VISIT_GOVERNOR | VISIT_KEPT | VISIT_SNEAKED;
	sim_leave_town(&s);
	CHECK(s.player_town == -1);
	sim_enter_town(&s, 1);		/* the same town: bits 0-1 kept */
	CHECK(s.visit_flags == (VISIT_GOVERNOR | VISIT_KEPT));
	sim_leave_town(&s);
	sim_enter_town(&s, 0);		/* another: all cleared */
	CHECK(s.visit_flags == 0 && s.player_town == 0);
	sim_enter_town(&s, 99);		/* no such town: ignored */
	CHECK(s.player_town == 0);

	memset(&ss, 0, sizeof(ss));
	ss.load = loader;
	ss.load_world = world_cb;
	proto_handle(&ss, "{\"cmd\":\"new_game\",\"era\":1660}", &out);
	jb_free(&out);
	proto_handle(&ss, "{\"cmd\":\"town\",\"action\":\"leave\"}", &out);
	CHECK(out.s != NULL && strstr(out.s, "not in a town") != NULL);
	jb_free(&out);
	ss.sim.fleet.leaving = 2;
	proto_handle(&ss, "{\"cmd\":\"town\",\"action\":\"enter\"}", &out);
	CHECK(out.s != NULL && strstr(out.s, "\"player_town\":2") != NULL);
	jb_free(&out);
	/* A governor only after a sack that allows a conquest. */
	proto_handle(&ss, "{\"cmd\":\"assault\",\"action\":\"governor\","
	    "\"nation\":0}", &out);
	CHECK(out.s != NULL && strstr(out.s, "no sack here") != NULL);
	jb_free(&out);
	ss.sim.fleet.crew = 30000;
	proto_handle(&ss, "{\"cmd\":\"assault\",\"action\":\"sack\"}", &out);
	CHECK(out.s != NULL && strstr(out.s, "\"conquest\":1") != NULL);
	jb_free(&out);
	proto_handle(&ss, "{\"cmd\":\"assault\",\"action\":\"governor\","
	    "\"nation\":0}", &out);
	CHECK(out.s != NULL && strstr(out.s, "\"ok\":true") != NULL &&
	    ss.sim.towns[2].nation == 0);
	jb_free(&out);
	proto_handle(&ss, "{\"cmd\":\"assault\",\"action\":\"governor\","
	    "\"nation\":1}", &out);
	CHECK(out.s != NULL && strstr(out.s, "no sack here") != NULL);
	jb_free(&out);
	/* The reckoning: a fresh captain, 25, of no rank. */
	proto_handle(&ss, "{\"cmd\":\"career_end\"}", &out);
	CHECK(out.s != NULL && strstr(out.s, "\"ok\":true") != NULL &&
	    strstr(out.s, "\"age\":25") != NULL &&
	    strstr(out.s, "\"ranks\":0") != NULL);
	jb_free(&out);
	/* The map seller: a piece for 50 gold, only while he is there. */
	ss.sim.tavern_spots = 0;
	proto_handle(&ss, "{\"cmd\":\"tavern\",\"action\":\"buy_map\"}",
	    &out);
	CHECK(out.s != NULL && strstr(out.s, "no map seller") != NULL);
	jb_free(&out);
	ss.sim.tavern_spots = TAVERN_MAPS;
	ss.sim.fleet.gold = 60;
	proto_handle(&ss, "{\"cmd\":\"tavern\",\"action\":\"buy_map\"}",
	    &out);
	CHECK(out.s != NULL && strstr(out.s, "\"ok\":true") != NULL &&
	    ss.sim.fleet.gold == 10 && (ss.sim.maps[MAP_PIRATE].pieces & 0xF));
	jb_free(&out);
	proto_handle(&ss, "{\"cmd\":\"town\",\"action\":\"leave\"}", &out);
	CHECK(out.s != NULL && strstr(out.s, "\"player_town\":-1") != NULL);
	jb_free(&out);
	session_free(&ss);
}

/* The shipwright: his gold, the refusal, prices, repairs and sales. */
static void
test_shipwright(void)
{
	struct sim s;
	struct sim_town *t;
	long gold;

	sea_game(&s, 300, 300, 0);
	t = &s.towns[1];
	t->nation = NATION_ENGLAND;
	t->wealth = 120;
	t->citizens = 20;
	s.month = 0;				/* January: |6 - 0| = 6 */
	sim_enter_town(&s, 1);
	CHECK(!sim_shipwright_refuses(&s));
	/* ((6 + 6) x 120 / 6) = 240; (30 x 240 / 150) x 600 = 28800. */
	CHECK(sim_shipwright_gold(&s) == 28800);
	s.month = 6;				/* set once per stay */
	CHECK(sim_shipwright_gold(&s) == 28800);
	sim_leave_town(&s);
	sim_enter_town(&s, 0);
	sim_leave_town(&s);
	sim_enter_town(&s, 1);			/* a new stay: July */
	CHECK(sim_shipwright_gold(&s) == ((30 * (6 * 120 / 6)) / 150) * 600);
	t->wealth = 255;
	t->citizens = 200;
	sim_enter_town(&s, 0);
	sim_enter_town(&s, 1);
	CHECK(sim_shipwright_gold(&s) == 50000);	/* the cap */
	/* Spanish towns refuse while the rank falls short of the wealth. */
	t->nation = NATION_SPAIN;
	CHECK(sim_shipwright_refuses(&s));
	t->wealth = 0;
	CHECK(!sim_shipwright_refuses(&s));
	t->nation = NATION_ENGLAND;

	/* Prices: 50 x (class + 1) x level, and x (10 - level). */
	s.fleet.nships = 3;
	s.fleet.ships[0] = 1;			/* sloop, undamaged */
	s.fleet.damage[0] = 0;
	s.fleet.ships[1] = 4;			/* merchantman, level 2 */
	s.fleet.damage[1] = 40;
	s.fleet.ships[2] = 0;			/* pinnace, level 3 (capped) */
	s.fleet.damage[2] = 63;
	CHECK(sim_repair_cost(&s, 0) == 0 && sim_ship_price(&s, 0) == 1000);
	CHECK(sim_repair_cost(&s, 1) == 500 && sim_ship_price(&s, 1) == 2000);
	CHECK(sim_repair_cost(&s, 2) == 150 && sim_ship_price(&s, 2) == 350);

	/* Repairs: nothing to do, too dear (strictly less than the gold),
	 * then paid to the shipwright. */
	CHECK(sim_repair(&s, 1u) == REPAIR_NOTHING);
	s.fleet.gold = 650;
	CHECK(sim_repair(&s, 6u) == REPAIR_TOO_DEAR && s.fleet.damage[1] == 40);
	s.fleet.gold = 651;
	gold = sim_shipwright_gold(&s);
	CHECK(sim_repair(&s, 6u) == REPAIR_DONE);
	CHECK(s.fleet.gold == 1 && s.fleet.damage[1] == 0 &&
	    s.fleet.damage[2] == 0 && sim_shipwright_gold(&s) == gold + 650);

	/* Sales: in fleet order, one ship stays, his gold may run out. */
	CHECK(sim_sell_ships(&s, 7u) == SELL_DONE);
	CHECK(s.fleet.nships == 1 && s.fleet.ships[0] == 0);	/* kept */
	CHECK(s.fleet.gold == 1 + 1000 + 2500);
	CHECK(sim_sell_ships(&s, 1u) == SELL_ONLY_SHIP);
	s.fleet.nships = 2;
	s.fleet.ships[1] = 8;
	s.fleet.damage[1] = 0;
	s.wright_gold = 4499;			/* a fast galleon: 4500 */
	CHECK(sim_sell_ships(&s, 2u) == SELL_SHORT && s.fleet.nships == 2);
	s.wright_gold = 4500;
	CHECK(sim_sell_ships(&s, 2u) == SELL_DONE && s.wright_gold == 0);
	/* Levels stop at 3, as the game's damage nibble is read. */
	s.fleet.damage[0] = 200;
	CHECK(sim_repair_cost(&s, 0) == 50 * 1 * 3);
}

/* The merchant: prices, stock, trading limits, the town's wealth. */
static void
test_merchant(void)
{
	struct sim s;
	struct sim_town *t;
	struct sim_market *m = &s.market;

	sea_game(&s, 300, 300, 0);
	t = &s.towns[1];
	t->nation = NATION_ENGLAND;
	t->wealth = 120;
	t->citizens = 20;
	t->flags = 0;
	t->family = 0;
	s.month = 0;
	sim_enter_town(&s, 1);
	CHECK(!sim_merchant_refuses(&s));
	sim_merchant_open(&s);
	CHECK(m->open);
	/* W = 12 x 120 / 6 = 240: gold 48 x 600; food 7 -> 70, then each
	 * good starts from the last price: goods 4 -> 40, sugar 2 -> 20;
	 * then x 1.5, and sugar x 2. */
	CHECK(s.merchant_gold == 28800);
	CHECK(m->price[0] == 105 && m->price[1] == 60 && m->price[2] == 60);
	CHECK(m->stock[0] >= 48 && m->stock[0] < 96);
	CHECK(m->stock[1] >= 14 && m->stock[1] < 28);
	CHECK(m->stock[2] >= 8 && m->stock[2] < 16);
	/* A local product is cheap (bit 3: food): 7 / 2 + 1 = 4. */
	t->flags = 0x08;
	sim_merchant_open(&s);
	CHECK(m->price[0] == 60);
	/* Wanted sugar (bit 4) doubles it again. */
	t->flags = 0x10;
	sim_merchant_open(&s);
	CHECK(m->price[2] == 120);
	/* Family pays three quarters. */
	t->flags = 0;
	t->family = 0x40;
	sim_merchant_open(&s);
	CHECK(m->price[0] == 78 && m->price[1] == 45);
	CHECK(sim_merchant_greeting(&s) == 291 + (s.fleet.gold > 2000));
	t->family = 0;
	/* Spanish towns: the family is welcome, others are not. */
	t->nation = NATION_SPAIN;
	CHECK(sim_merchant_refuses(&s));
	t->family = 0x40;
	CHECK(!sim_merchant_refuses(&s));
	t->family = 0;
	t->nation = NATION_ENGLAND;

	/* Trading: one price both ways, limited by stock, gold, hold space
	 * and the merchant's gold. */
	sim_merchant_open(&s);
	s.fleet.gold = 1000;
	s.fleet.capacity = 4;
	s.fleet.guns = 8;
	s.fleet.food = s.fleet.goods = s.fleet.sugar = 0;
	CHECK(sim_hold_space(&s) == 36);
	CHECK(sim_trade(&s, 1, 5) == 5);
	CHECK(s.fleet.goods == 5 && s.fleet.gold == 700 &&
	    s.merchant_gold == 28800 + 300);
	CHECK(sim_trade(&s, 1, 100) == 11);	/* 700 / 60: gold runs out */
	CHECK(s.fleet.gold == 40);
	s.fleet.gold = 100000;
	CHECK(sim_trade(&s, 0, 1000) == 20);	/* the hold is full */
	CHECK(sim_hold_space(&s) == 0);
	CHECK(sim_trade(&s, 1, -3) == 3 && s.fleet.goods == 13);
	s.merchant_gold = 100;
	CHECK(sim_trade(&s, 1, -5) == 1);	/* he runs out of gold */
	m->stock[2] = 0;
	s.fleet.food = 0;
	CHECK(sim_trade(&s, 2, 1) == 0);	/* no sugar to buy */
	CHECK(sim_trade(&s, 0, -1) == 0);	/* no food to sell */
	/* Closing: more than 20 gold moved, wealth x 19 / 20 + 12. */
	sim_merchant_close(&s);
	CHECK(!m->open && t->wealth == 120 * 19 / 20 + 12);
	sim_merchant_open(&s);
	sim_merchant_close(&s);			/* nothing traded */
	CHECK(t->wealth == 126);
	sim_merchant_open(&s);
	s.fleet.gold += 20;			/* 20 is not enough */
	sim_merchant_close(&s);
	CHECK(t->wealth == 126);
	/* Greetings by purse: up to 2000, 5000, 20000, then more. */
	s.fleet.gold = 2000;
	CHECK(sim_merchant_greeting(&s) == 257);
	s.fleet.gold = 2001;
	CHECK(sim_merchant_greeting(&s) == 258);
	s.fleet.gold = 5001;
	CHECK(sim_merchant_greeting(&s) == 259);
	s.fleet.gold = 5000;
	CHECK(sim_merchant_greeting(&s) == 258);
	s.fleet.gold = 20000;
	CHECK(sim_merchant_greeting(&s) == 259);
	s.fleet.gold = 20001;
	CHECK(sim_merchant_greeting(&s) == 260);
	t->wealth = 250;
	sim_merchant_open(&s);
	s.fleet.gold += 21;
	sim_merchant_close(&s);
	CHECK(t->wealth == 249);		/* 237 + 12 */
}

/* The tavern: who is there, the recruits, the reputation. */
static void
test_tavern(void)
{
	struct sim s;
	struct sim_town *t;
	int i, lo = 1000, hi = 0;

	sea_game(&s, 300, 300, 0);
	t = &s.towns[1];
	sim_enter_town(&s, 1);
	CHECK(s.tavern_spots == TAVERN_ALL);
	s.player.difficulty = 1;
	s.fleet.nships = 1;
	s.fleet.capacity = 4;		/* room for (2 + 4) x 16 = 96 men */
	/* No citizens: a small crew still gets (4 - 1) x 6 = 18 men. */
	t->citizens = 0;
	s.fleet.crew = 10;
	sim_tavern_open(&s);
	CHECK(s.recruits == 18);
	s.fleet.crew = 71;		/* under 72: still 18 */
	sim_tavern_open(&s);
	CHECK(s.recruits == 18);
	s.fleet.crew = 80;		/* 18 would pass 96: 16 */
	sim_tavern_open(&s);
	CHECK(s.recruits == 0);		/* not small: 0 */
	s.fleet.crew = 100;		/* over the limit: at least one */
	sim_tavern_open(&s);
	CHECK(s.recruits == 1);
	/* With people: (0 + 7 - 1) x (20 + rand(20)) / 9 = 13..25. */
	t->citizens = 20;
	s.fleet.crew = 72;
	for (i = 0; i < 200; i++) {
		sim_tavern_open(&s);
		lo = s.recruits < lo ? s.recruits : lo;
		hi = s.recruits > hi ? s.recruits : hi;
	}
	CHECK(lo == 13 && hi == 24);	/* 6 x 39 / 9 = 26 caps at 24 */
	s.reputation = 3;		/* a better name, more men */
	s.fleet.crew = 10;
	sim_tavern_open(&s);
	CHECK(s.recruits >= 9 * 20 / 9);
	/* Signing them on; then they are gone until the next arrival. */
	i = s.recruits;
	sim_recruit(&s, 1);
	CHECK(s.fleet.crew == 10 + i && !(s.tavern_spots & TAVERN_CREW));
	sim_tavern_open(&s);
	CHECK(s.recruits == 0);
	sim_leave_town(&s);
	sim_enter_town(&s, 1);
	CHECK(s.tavern_spots == TAVERN_ALL);

	/* Reputation: a capture +1, a failed boarding -2. */
	battle_game(&s);
	s.reputation = 0;
	s.battle.outcome = BAT_SURRENDER;
	sim_battle_finish(&s, 0);
	CHECK(s.reputation == 1);
	battle_game(&s);
	s.battle.outcome = BAT_BOARDING;
	sim_board(&s, 0);
	s.sword.result = SWORD_REPELLED;
	sim_battle_finish(&s, 0);
	CHECK(s.reputation == -2);	/* a fresh game: 0 - 2 */
}

/* The governor: standing, promotions, the Letter of Marque. */
static void
test_governor(void)
{
	struct sim s;
	struct sim_town *t;
	long price;
	int acres;

	sea_game(&s, 300, 300, 0);
	memset(s.standing, 0, 4);
	memset(s.relations, 0, sizeof(s.relations));
	s.relations[NATION_SPAIN][NATION_ENGLAND] = -1;	/* war */
	s.relations[NATION_SPAIN][NATION_FRANCE] = 1;	/* alliance */
	/* Attacking Spain: Spain -1, its ally France -1, its enemy England
	 * +1, but no higher than 1 through others. */
	sim_attack_nation(&s, NATION_SPAIN, -1);
	CHECK(s.standing[NATION_SPAIN] == -1 && s.standing[NATION_FRANCE] == -1);
	CHECK(s.standing[NATION_ENGLAND] == 1 && s.standing[NATION_HOLLAND] == 0);
	sim_attack_nation(&s, NATION_SPAIN, -1);
	CHECK(s.standing[NATION_ENGLAND] == 1);		/* capped */
	s.standing[NATION_ENGLAND] = 2;			/* above 1: free */
	sim_attack_nation(&s, NATION_SPAIN, -1);
	CHECK(s.standing[NATION_ENGLAND] == 3);
	s.standing[NATION_FRANCE] = 3;			/* not below 2 */
	sim_attack_nation(&s, NATION_SPAIN, -2);
	CHECK(s.standing[NATION_FRANCE] == 2);
	sim_attack_nation(&s, NATION_PIRATE, -1);	/* ignored */

	/* Offers: price (4 - s^3) x 200; at 2 and over, the daughter. */
	t = &s.towns[1];
	t->nation = NATION_ENGLAND;
	sim_enter_town(&s, 1);
	s.standing[NATION_ENGLAND] = 0;
	CHECK(sim_governor_offer(&s, &price) == OFFER_MARQUE && price == 800);
	s.standing[NATION_ENGLAND] = 1;
	CHECK(sim_governor_offer(&s, &price) == OFFER_MARQUE && price == 600);
	s.standing[NATION_ENGLAND] = -2;
	CHECK(sim_governor_offer(&s, &price) == OFFER_PARDON && price == 2400);
	s.standing[NATION_ENGLAND] = -4;
	CHECK(sim_governor_offer(&s, &price) == OFFER_NONE);
	s.standing[NATION_ENGLAND] = 2;
	CHECK(sim_governor_offer(&s, &price) == OFFER_DAUGHTER);
	/* Paying: +2, no higher than 2; the town's favour is spent. */
	s.standing[NATION_ENGLAND] = 1;
	t->standing = 5;
	s.fleet.gold = 599;
	CHECK(!sim_governor_pay(&s));
	s.fleet.gold = 600;
	CHECK(sim_governor_pay(&s) && s.fleet.gold == 0);
	CHECK(s.standing[NATION_ENGLAND] == 2 && t->standing == 0);

	/* Promotion: rank r + 1 needs (r + 1)^2 + 2; the surplus and the
	 * town's favour become acres (x 50). */
	s.rank[NATION_ENGLAND] = 0;
	s.standing[NATION_ENGLAND] = 2;
	CHECK(sim_governor_promote(&s, &acres) == 0);	/* 3 needed */
	s.standing[NATION_ENGLAND] = 5;
	t->standing = 1;
	CHECK(sim_governor_promote(&s, &acres) == 1);
	CHECK(s.rank[NATION_ENGLAND] == 1 && s.standing[NATION_ENGLAND] == 3);
	CHECK(news_abs(&s, s.nnews - 1)->code == NEWS_TITLED &&
	    news_abs(&s, s.nnews - 1)->a == NATION_ENGLAND * 64 + 1);
	CHECK(acres == (2 + 1) * 50 && s.acres == 3);
	s.standing[NATION_ENGLAND] = 6;			/* rank 2: 6 */
	t->standing = 0;
	CHECK(sim_governor_promote(&s, &acres) == 2 && acres == 0);
	/* A town's disfavour (negative) eats the surplus; it never wraps
	 * into a grant of tens of thousands of acres. */
	s.standing[NATION_ENGLAND] = 12;		/* rank 3: 11 */
	t->standing = (uint16_t)(int16_t)-3;
	CHECK(sim_governor_promote(&s, &acres) == 3 && acres == 0 &&
	    s.acres == 3);
	s.rank[NATION_ENGLAND] = 2;
	s.rank[NATION_ENGLAND] = 9;			/* the top */
	s.standing[NATION_ENGLAND] = 120;
	CHECK(sim_governor_promote(&s, &acres) == 0 &&
	    s.rank[NATION_ENGLAND] == 9);
	/* Ranks open Spanish doors: rank x 25 against the wealth. */
	t->nation = NATION_SPAIN;
	t->wealth = 50;
	s.rank[NATION_SPAIN] = 1;
	CHECK(sim_shipwright_refuses(&s));
	s.rank[NATION_SPAIN] = 2;
	CHECK(!sim_shipwright_refuses(&s));
}

/* Sea battles at the coast: the shallows scrape, land beaches. */
static void
test_coast(void)
{
	struct sim s;
	int i, hurt = 0;

	/* Below the land block (cells y 40-49), the coast is to the north:
	 * set 0, whose screen has land on rows 0..159, shallows below. */
	sea_game(&s, 120, 50, 0);
	sim_battle_start(&s);
	CHECK(s.battle.set == 0 && s.battle.screen == 1);
	s.battle.foe.x = 420 * 256;		/* in range, not alongside */
	s.battle.foe.y = 300 * 256;
	/* A pinnace (class 0) in the shallows: hurt at once, then 20 ticks
	 * of grace. */
	s.battle.you.type = 0;
	s.battle.you.damage = 0;
	s.battle.you.x = 240 * 256;
	s.battle.you.y = 176 * 256;
	s.battle.you.heading = 0x40;		/* along the shoal */
	CHECK(sim_battle_tick(&s, 0, 0, 0) & BEV_AGROUND);
	CHECK(s.battle.you.damage == 16 && s.battle.you_scrape == 20);
	for (i = 0; i < 5; i++)
		CHECK(!(sim_battle_tick(&s, 0, 0, 0) & BEV_AGROUND));
	CHECK(s.battle.you.damage == 16 && s.battle.you_scrape == 15);
	/* A galleon (class 7) is hurt on rolls of 7..15 only. */
	s.battle.you.type = 7;
	for (i = 0; i < 400 && s.battle.outcome == BAT_NONE; i++) {
		s.battle.you.y = 176 * 256;
		s.battle.you.x = 240 * 256;
		s.battle.you.damage = 0;
		s.battle.you_scrape = 0;
		hurt += (sim_battle_tick(&s, 0, 0, 0) & BEV_AGROUND) != 0;
	}
	CHECK(hurt > 150 && hurt < 300);	/* 9/16 of 400 */
	/* Land: beached; with the enemy alongside, a boarding. */
	s.battle.outcome = BAT_NONE;
	s.battle.you.x = 240 * 256;
	s.battle.you.y = 100 * 256;
	sim_battle_tick(&s, 0, 0, 0);
	CHECK(s.battle.outcome == BAT_BEACHED);
	s.battle.outcome = BAT_NONE;
	s.battle.you.y = 100 * 256;
	s.battle.foe.x = s.battle.you.x;
	s.battle.foe.y = s.battle.you.y;
	sim_battle_tick(&s, 0, 0, 0);
	CHECK(s.battle.outcome == BAT_BOARDING);
	/* Off the field is open sea. */
	s.battle.outcome = BAT_NONE;
	s.battle.you.damage = s.battle.foe.damage = 0;
	s.battle.you.x = -50 * 256;
	s.battle.you.y = 100 * 256;
	s.battle.foe.x = -50 * 256;
	s.battle.foe.y = 300 * 256;
	sim_battle_tick(&s, 0, 0, 0);
	CHECK(s.battle.outcome == BAT_NONE);
}

/* Battle screens: the set by the coast, scrolling along and off it. */
static void
test_screens(void)
{
	struct sim s;
	int i;

	/* Far from land: open sea, set 4. */
	sea_game(&s, 300, 300, 0);
	for (i = 0; i < 5; i++)
		test_world.battle_sets[4][i] = (uint8_t)(100 + i);
	sim_battle_start(&s);
	CHECK(s.battle.set == 4 && s.battle.screen == 100 + s.battle.piece);
	/* Along the north coast: the next piece when scrolling east, open
	 * sea once scrolled south. */
	sea_game(&s, 120, 50, 0);
	for (i = 0; i < 5; i++)
		test_world.battle_sets[0][i] = (uint8_t)(i < 4 ? 10 + i : 104);
	sim_battle_start(&s);
	CHECK(s.battle.set == 0);
	s.battle.you.x = (420 + 40) * 256;	/* corner past 400 */
	s.battle.you.y = (200 + 30) * 256;
	s.battle.foe.x = (300 + 40) * 256;
	s.battle.foe.y = (200 + 30) * 256;
	s.battle.you.heading = 0x40;
	CHECK(sim_battle_tick(&s, 0, 0, 0) & BEV_SCROLLED);
	CHECK(s.battle.scroll_x == 1 && s.battle.scroll_y == 0);
	CHECK(s.battle.screen == 10 + ((1 + s.battle.piece) & 3));
	CHECK(s.battle.you.x / 256 < 420 + 40 - 100);
	s.battle.you.y = (420 + 30) * 256;
	s.battle.foe.y = s.battle.you.y - 60 * 256;
	sim_battle_tick(&s, 0, 0, 0);
	CHECK(s.battle.scroll_y == 1 && s.battle.screen == 104);
	for (i = 0; i < 5; i++)
		test_world.battle_sets[0][i] = test_world.battle_sets[4][i] = 0;
	for (i = 0; i < 4; i++)
		test_world.battle_sets[0][i] = 1;
}

/* The fort battle: the town's screen, a fort that stays and shoots,
 * hits counted apart, the edge and the landing. */
static void
test_fort_battle(void)
{
	struct sim s;
	struct sim_town *t;
	int i, fx, fy, shots = 0;

	sea_game(&s, 300, 300, 0);
	t = &s.towns[1];
	t->id = 1;			/* screen 1: land on rows 0-4 */
	t->fort_x = 7;
	t->fort_y = 3;
	t->ship_x = 6;
	t->ship_y = 10;
	t->ship_heading = 0;
	t->soldiers = 41;
	t->forts = 2;
	sim_fort_battle_start(&s, 1);
	CHECK(s.battle.fort && s.battle.screen == 1 && s.battle.set == 5);
	CHECK(s.battle.foe.guns == 24 && s.battle.foe.crew == 21 * 4);
	CHECK(s.battle.you.x / 256 == 6 * 32 + 16 + 40 &&
	    s.battle.you.y / 256 == 10 * 32 + 16 + 30);
	CHECK(s.battle.foe.x / 256 == 7 * 32 + 16 &&
	    s.battle.foe.y / 256 == 3 * 32 + 16);
	fx = s.battle.foe.x;
	fy = s.battle.foe.y;
	for (i = 0; i < 400 && s.battle.outcome == BAT_NONE; i++) {
		s.battle.you.x = (6 * 32 + 16 + 40) * 256;
		s.battle.you.y = (10 * 32 + 16 + 30) * 256;
		s.battle.you.damage = 0;
		shots += (sim_battle_tick(&s, 0, 0, 0) & BEV_FOE_FIRED) != 0;
	}
	CHECK(s.battle.outcome == BAT_NONE && shots >= 2);
	CHECK(s.battle.foe.x == fx && s.battle.foe.y == fy);
	/* It holds its fire while its last shot's splash shows. */
	s.battle.foe.shot.ticks = 0;
	s.battle.foe.shot.landing = 38;
	s.battle.foe.reload = 0;
	CHECK(!(sim_battle_tick(&s, 0, 0, 0) & BEV_FOE_FIRED));
	CHECK(s.battle.foe.shot.landing == 39);
	/* Its shot is timed corner to corner (fort 224, 96; ship 208,
	 * 336): d = 240 + 16 / 4, 122 ticks less the one flown at once and
	 * the ship's first pixels north; centre to centre would be 128. */
	s.battle.you.x = (6 * 32 + 16 + 40) * 256;
	s.battle.you.y = (10 * 32 + 16 + 30) * 256;
	s.battle.you.speed = 0;
	s.battle.foe.shot.ticks = 0;
	s.battle.foe.reload = 1;
	CHECK(sim_battle_tick(&s, 0, 0, 0) & BEV_FOE_FIRED);
	CHECK(s.battle.foe.shot.ticks >= 118 && s.battle.foe.shot.ticks <= 121);
	/* The player's shots count against the fort, which never sinks. */
	s.battle.foe.damage = 0;
	s.battle.you.shot.ticks = 1;
	s.battle.you.shot.x = fx;
	s.battle.you.shot.y = fy;
	sim_battle_tick(&s, 0, 0, 0);
	CHECK(s.battle.fort_damage > 0 && s.battle.foe.damage == 0);
	/* Past 400, still in reach: a fort's field does not scroll (its
	 * bounds, 5..470, lie beyond the battle's end, VA 0x9a14). */
	s.battle.you.x = (410 + 40) * 256;
	CHECK(!(sim_battle_tick(&s, 0, 0, 0) & BEV_SCROLLED) &&
	    s.battle.scroll_x == 0 && s.battle.outcome == BAT_NONE);
	/* Past 430 on an axis: out of the fort's reach. */
	s.battle.you.x = (440 + 40) * 256;
	sim_battle_tick(&s, 0, 0, 0);
	CHECK(s.battle.outcome == BAT_ESCAPED);
	/* Onto the land by the fort: the landing; elsewhere, beached. */
	s.battle.outcome = BAT_NONE;
	s.battle.you.x = fx;
	s.battle.you.y = fy + 20 * 256;		/* land row, next to it */
	sim_battle_tick(&s, 0, 0, 0);
	CHECK(s.battle.outcome == BAT_LANDED);
	s.battle.outcome = BAT_NONE;
	s.battle.you.x = 13 * 32 * 256;
	s.battle.you.y = 2 * 32 * 256;
	sim_battle_tick(&s, 0, 0, 0);
	CHECK(s.battle.outcome == BAT_BEACHED);
}

/* Choosing the ship to command swaps it into the flagship's place. */
static void
test_command(void)
{
	struct sim s;

	sea_game(&s, 300, 300, 0);
	s.fleet.nships = 2;
	s.fleet.ships[0] = 1;			/* a sloop */
	s.fleet.ships[1] = 5;			/* a frigate */
	s.fleet.damage[0] = 0;
	s.fleet.damage[1] = 20;
	s.fleet.crew = 100;
	s.fleet.guns = 30;
	sim_battle_start(&s);
	CHECK(s.battle.you.type == 1 && s.battle.you.crew == 92);
	CHECK(s.battle.you.guns == 12);		/* (1 + 2) x 4 */
	sim_battle_command(&s, 1);
	CHECK(s.fleet.ships[0] == 5 && s.fleet.ships[1] == 1 &&
	    s.fleet.damage[0] == 20 && s.fleet.damage[1] == 0);
	CHECK(s.battle.you.type == 5 && s.battle.you.damage == 20 &&
	    s.battle.you.crew == 92 && s.battle.you.guns == 23);
	CHECK(s.battle.crew_ashore == 8 && s.battle.guns_ashore == 7);
	/* Not once the battle runs. */
	sim_battle_tick(&s, 0, 0, 0);
	sim_battle_command(&s, 1);
	CHECK(s.fleet.ships[0] == 5);
}

/* Through the protocol: the fort battle from the sea, the ship to
 * command, and the landing's garrison. */
static void
test_fort_protocol(void)
{
	struct session ss;
	struct jbuf out = { 0 };
	struct sim_town *t;

	memset(&ss, 0, sizeof(ss));
	ss.load = loader;
	ss.load_world = world_cb;
	proto_handle(&ss, "{\"cmd\":\"new_game\",\"era\":1660}", &out);
	jb_free(&out);
	proto_handle(&ss, "{\"cmd\":\"flagship\",\"ship\":0}", &out);
	CHECK(out.s != NULL && strstr(out.s, "\"ok\":false") != NULL);
	jb_free(&out);
	t = &ss.sim.towns[1];
	t->forts = 0;
	ss.sim.fleet.leaving = 1;
	ss.sim.fleet.ashore = 0;
	ss.sim.player_town = -1;
	proto_handle(&ss, "{\"cmd\":\"town\",\"action\":\"fort\"}", &out);
	CHECK(out.s != NULL && strstr(out.s, "\"ok\":false") != NULL);
	jb_free(&out);
	t->forts = 2;
	t->soldiers = 41;
	ss.sim.fleet.nships = 2;
	ss.sim.fleet.ships[0] = 1;
	ss.sim.fleet.ships[1] = 5;
	ss.sim.fleet.damage[0] = 0;
	ss.sim.fleet.damage[1] = 0;
	ss.sim.fleet.crew = 100;
	ss.sim.fleet.guns = 30;
	proto_handle(&ss, "{\"cmd\":\"town\",\"action\":\"fort\"}", &out);
	CHECK(out.s != NULL && strstr(out.s, "\"ok\":true") != NULL);
	CHECK(ss.sim.battle.active && ss.sim.battle.fort);
	jb_free(&out);
	proto_handle(&ss, "{\"cmd\":\"flagship\",\"ship\":2}", &out);
	CHECK(out.s != NULL && strstr(out.s, "\"ok\":false") != NULL);
	jb_free(&out);
	proto_handle(&ss, "{\"cmd\":\"flagship\",\"ship\":1}", &out);
	CHECK(out.s != NULL && strstr(out.s, "\"ok\":true") != NULL);
	CHECK(ss.sim.fleet.ships[0] == 5 && ss.sim.battle.you.type == 5);
	CHECK(ss.sim.battle.you.crew == 92 && ss.sim.battle.crew_ashore == 8);
	jb_free(&out);
	proto_handle(&ss, "{\"cmd\":\"battle\"}", &out);
	jb_free(&out);
	proto_handle(&ss, "{\"cmd\":\"flagship\",\"ship\":1}", &out);
	CHECK(out.s != NULL && strstr(out.s, "\"ok\":false") != NULL);
	jb_free(&out);
	/* Landed: half the garrison and a tenth of the fort's crew stay;
	 * the men left with the fleet are reported. */
	ss.sim.battle.outcome = BAT_LANDED;
	ss.sim.battle.foe.crew = 60;
	proto_handle(&ss, "{\"cmd\":\"finish\"}", &out);
	CHECK(out.s != NULL && strstr(out.s, "\"outcome\":\"landed\"") !=
	    NULL && strstr(out.s, "\"aboard\":8") != NULL);
	CHECK(t->soldiers == 21 + 6);
	jb_free(&out);
	/* The fort, left as the encounter, loads back. */
	{
		struct jbuf st = { 0 }, cmd = { 0 };

		sim_to_json(&ss.sim, &st);
		jb_raw(&cmd, "{\"cmd\":\"load\",\"state\":");
		jb_raw(&cmd, st.s);
		jb_raw(&cmd, "}");
		proto_handle(&ss, cmd.s, &out);
		CHECK(out.s != NULL && strcmp(out.s, "{\"ok\":true}") == 0);
		CHECK(ss.sim.encounter.type == FORT_TYPE);
		jb_free(&out);
		jb_free(&st);
		jb_free(&cmd);
	}
	session_free(&ss);
}

/* The land battle: the forces, moving, the fort reached, the count. */
static void
test_land_battle(void)
{
	struct sim s;
	struct sim_town *t;
	struct sim_land *l = &s.land;
	int i, x, ev = 0;

	memset(test_world.land_ground, GROUND_CLEAR,
	    sizeof(test_world.land_ground));
	sea_game(&s, 300, 300, 0);
	t = &s.towns[1];
	t->id = 0;
	t->forts = 1;
	t->soldiers = 10;
	t->garrison = 0;
	t->land_x = 7;
	t->land_y = 2;
	t->party_x = 7;
	t->party_y = 12;
	s.fleet.crew = 100;
	CHECK(sim_land_battle_needed(&s, 1));
	s.fleet.crew = 151;
	CHECK(!sim_land_battle_needed(&s, 1));
	s.fleet.crew = 100;
	sim_land_start(&s, 1);
	/* 100 men: 50 and 50; 105 defenders: the fort's 26, then 79. */
	CHECK(l->active && l->u[1].men == 50 && l->u[2].men == 50 &&
	    l->u[3].flags == 0);
	CHECK(l->u[1].x == 7 * 32 && l->u[1].y == 12 * 32 &&
	    l->u[2].x == 6 * 32 && l->u[2].y == 11 * 32);
	CHECK(l->u[7].flags == LAND_FORT && l->u[7].men == 26 &&
	    l->u[7].x == 7 * 32 && l->fort == 7);
	CHECK(l->u[6].flags == LAND_SOLDIERS && l->u[6].men == 79 &&
	    l->u[5].flags == 0 && l->u[4].flags == 0);
	CHECK(l->u[1].muskets >= 10 && l->u[1].muskets < 16 &&
	    l->u[1].morale == 79);
	/* Clear ground at full morale: 3 x 2 + 3 = 9, half a step a pass:
	 * 4 pixels. */
	x = l->u[1].x;
	sim_land_tick(&s, 1, 0, 0);
	CHECK(l->selected == 1 && l->u[1].x == x + 4);
	/* The button selects the next unit. */
	CHECK(sim_land_tick(&s, 0, 0, 1) & LEV_SELECTED);
	CHECK(l->selected == 2);
	sim_land_tick(&s, 0, 0, 0);
	/* Fights happen when close; marching on the fort wins. */
	l->u[2].x = l->u[7].x;
	l->u[2].y = l->u[7].y + 40;
	for (i = 0; i < 200 && l->result == 0; i++)
		ev |= sim_land_tick(&s, 0, -1, 0);
	CHECK(l->result == 2);
	CHECK(sim_land_end(&s) == 1 && !l->active);
	CHECK(s.fleet.crew >= 1 && s.fleet.crew <= 100 &&
	    t->soldiers <= 10);
	/* Without men the attack fails. */
	sim_land_start(&s, 1);
	for (i = 1; i < 4; i++)
		l->u[i].men = 0;
	sim_land_tick(&s, 0, 0, 0);
	CHECK(l->result == 3 && sim_land_end(&s) == 0 && s.jail.fresh &&
	    s.jail.cause == JAIL_TOWN && s.jail.town == 1);
	(void)ev;
}

/* A landing party: ashore, walking, stopped by water, back aboard. */
static void
test_landing(void)
{
	struct sim s;
	int town, i;
	enum sail_stop r = SAIL_TICKS;
	int32_t sx, sy;

	sea_game(&s, 120, 52, 0x00);		/* south of the island */
	for (i = 0; i < 400 && r != SAIL_LAND; i++)
		r = sim_sail(&s, 0, 1, &town);
	CHECK(r == SAIL_LAND && s.fleet.land_y / 16 == 49);
	/* No: the ship turns about. */
	sim_turn_back(&s);
	CHECK(s.fleet.heading == 0x80 && !s.fleet.ashore);
	s.fleet.heading = 0;
	for (i = 0, r = SAIL_TICKS; i < 400 && r != SAIL_LAND; i++)
		r = sim_sail(&s, 0, 1, &town);
	CHECK(r == SAIL_LAND);
	sx = s.fleet.x;
	sy = s.fleet.y;
	sim_go_ashore(&s);
	CHECK(s.fleet.ashore && s.fleet.ship_x == sx && s.fleet.ship_y == sy &&
	    s.fleet.x == s.fleet.land_x && s.fleet.y == s.fleet.land_y);
	/* North, half a pixel a tick, into the island. */
	CHECK(sim_march(&s, 0, -1, 40, &town) == SAIL_TICKS &&
	    s.fleet.y == s.fleet.land_y - 20 && s.fleet.ashore);
	/* East along the island, then south: water far from the landing
	 * point stops the party. */
	CHECK(sim_march(&s, 1, 0, 120, &town) == SAIL_TICKS);
	CHECK(sim_march(&s, 0, 1, 200, &town) == SAIL_TICKS &&
	    s.fleet.ashore && s.fleet.y / 16 == 49);
	/* Back west and south to the landing point: aboard, bows south. */
	CHECK(sim_march(&s, -1, 0, 120, &town) == SAIL_TICKS);
	CHECK(sim_march(&s, 0, 1, 200, &town) == SAIL_BOARDED &&
	    !s.fleet.ashore && s.fleet.x == sx && s.fleet.y == sy &&
	    s.fleet.heading == 128);
	/* A reef just south of the ship: bows south would run onto it and
	 * ask to disembark again, so the ship turns to open water. */
	sim_go_ashore(&s);
	{
		int cx = sx / 16, cy = sy / 16 + 1, k;

		test_world.map[cy * WORLD_CELLS_W + cx] = 1;
		CHECK(sim_march(&s, 0, 1, 200, &town) == SAIL_BOARDED &&
		    s.fleet.heading != 128);
		for (k = 4; k <= 16; k += 4)
			CHECK(world_terrain(&test_world, s.fleet.x +
			    sim_sine_mul(&test_world, s.fleet.heading, k),
			    s.fleet.y - sim_sine_mul(&test_world,
			    s.fleet.heading + 0x40, k)) != TERRAIN_LAND);
		test_world.map[cy * WORLD_CELLS_W + cx] = 0;
	}
	/* Time passes on foot too. */
	sim_go_ashore(&s);
	i = s.day;
	sim_march(&s, 0, 0, 64, &town);
	CHECK(s.day == i + 1);
	/* A town the ship just left (a failed attack) can be walked into
	 * by a party landing beside it. */
	{
		struct sim_town keep = s.towns[1];

		s.fleet.ashore = 0;
		s.fleet.x = sx;
		s.fleet.y = sy;
		s.fleet.land_x = sx;
		s.fleet.land_y = sy - 16;
		s.towns[1].x = (uint16_t)(sx / 16);
		s.towns[1].y = (uint16_t)(sy / 16 - 3);	/* 2 from the landing */
		test_arrivals(&test_world, &s);
		s.fleet.leaving = 1;
		sim_go_ashore(&s);
		CHECK(s.fleet.leaving == -1);
		CHECK(sim_march(&s, 0, -1, 200, &town) == SAIL_TOWN &&
		    town == 1);
		s.towns[1] = keep;
		test_arrivals(&test_world, &s);
	}
}

/* How many quarters of a treasure map are held. */
static int
quarters(unsigned pieces)
{
	return ((pieces & 1) + (pieces >> 1 & 1) + (pieces >> 2 & 1) +
	    (pieces >> 3 & 1));
}

/* The tavern's people: the traveller, the map seller, the barmaid. */
static void
test_tavern_people(void)
{
	struct sim s;
	int i, seen = 0, town, maps = 0;

	for (i = 0; i < 50; i++) {
		sea_game(&s, 300, 300, 0);
		rng_seed(&s.rng, (uint64_t)i + 1, 1);
		sim_enter_town(&s, 1);
		sim_tavern_open(&s);
		CHECK(!(s.tavern_spots & TAVERN_MYSTERY));
		if (s.tavern_spots & TAVERN_NEWS) {
			CHECK(s.news_price >= 60 && s.news_price <= 140 &&
			    s.news_town != 1 &&
			    !(s.towns[s.news_town].flags & 0x80));
			seen++;
		}
		maps += (s.tavern_spots & TAVERN_MAPS) != 0;
	}
	CHECK(seen > 30 && maps > 2 && maps < 25);
	/* Buying: news of the town, a map piece for 50. */
	sea_game(&s, 300, 300, 0);
	sim_enter_town(&s, 1);
	s.tavern_spots |= TAVERN_NEWS | TAVERN_MAPS;
	s.news_town = 2;
	s.news_price = 80;
	s.fleet.gold = 100;
	s.towns[2].flags &= ~0x80;
	CHECK(sim_buy_news(&s) == 2 && s.fleet.gold == 20 &&
	    (s.towns[2].flags & 0x80) && !(s.tavern_spots & TAVERN_NEWS));
	CHECK(sim_buy_map(&s) == 0 && s.fleet.gold == 20);
	s.fleet.gold = 60;
	s.player.difficulty = 1;
	CHECK(sim_buy_map(&s) == 1 && s.fleet.gold == 10 &&
	    quarters(s.maps[MAP_PIRATE].pieces) == 1 &&
	    (s.maps[MAP_PIRATE].pieces & MAP_SHOWN));
	/* The barmaid: company for 5, her heart at 10, then reports. */
	s.towns[1].barmaid = 9;
	s.fleet.gold = 7;
	CHECK(sim_barmaid(&s, BARMAID_COMPANY) == 1 && s.fleet.gold == 2);
	CHECK(sim_barmaid(&s, BARMAID_COMPANY) == -1);
	CHECK(sim_barmaid(&s, BARMAID_DRINK) == 0 && s.fleet.gold == 1);
	s.towns[1].barmaid = 11;
	CHECK(sim_barmaid_meet(&s) == 2);
	s.tavern_spots |= TAVERN_NEWS | TAVERN_MAID;
	s.news_town = 3;
	s.towns[3].flags &= ~0x80;
	CHECK(sim_barmaid_report(&s, &town) == 1 && town == 3 &&
	    (s.towns[3].flags & 0x80) && !(s.tavern_spots & TAVERN_MAID));
	s.tavern_spots |= TAVERN_MAPS;
	CHECK(sim_barmaid_report(&s, &town) == 2 &&
	    quarters(s.maps[MAP_PIRATE].pieces) == 2);
}

/* The assault on a town, the sack, a new governor. */
static void
test_assault(void)
{
	struct sim s;
	struct sim_town *t;
	struct sack k;
	int n;

	sea_game(&s, 300, 300, 0);
	t = &s.towns[1];
	t->soldiers = 6;
	t->forts = 2;
	t->wealth = 100;
	t->gold = 300;
	t->citizens = 3;
	t->standing = 0;
	n = t->nation;
	s.fleet.crew = 80;
	s.fleet.gold = 0;
	s.heat = 0;
	s.reputation = 0;
	memset(s.standing, 0, sizeof(s.standing));
	sim_assault_start(&s, 1, 10, 2);
	CHECK(s.sword.active && s.sword.kind == SWORD_CREW &&
	    s.sword.you.men == 70 && s.sword.foe.men == 65 &&
	    s.sword.title == 4 && s.sword.scene == 1);
	/* Won with 50 men left against 12: 2 soldiers stay, 20 men lost. */
	s.sword.result = SWORD_WON;
	s.sword.you.men = 50;
	s.sword.foe.men = 12;
	CHECK(sim_assault_end(&s, 1) == 1 && t->soldiers == 2 &&
	    s.fleet.crew == 60);
	/* The sack: 300 x 1500 / 15 = 30000, counted as 300000 gold; the
	 * town keeps 300 - 600 -> 0; wealth 100 - 20 - 20; standing -3;
	 * crew 60 against 30 + rand(30): a conquest. */
	sim_sack(&s, 1, &k);
	CHECK(k.gold == 300000 && s.fleet.gold == 300000 && t->gold == 0);
	CHECK(t->wealth == 60 && (int16_t)t->standing == -3 &&
	    k.cannon == 12 && k.hidden == -1);
	CHECK(k.goods[0] < 80 && k.goods[1] < 80 && k.goods[2] < 80);
	CHECK(s.standing[n] == -3 && s.reputation == 2 && k.conquest);
	/* The loot for nothing: "Take all" starts with the selected good,
	 * then the first left, until the hold is full. */
	{
		const int loot[BOARD_GOODS] = { 3, 5, 100, 0 };

		s.fleet.capacity = 1;		/* 10 tons */
		s.fleet.guns = s.fleet.food = s.fleet.goods = s.fleet.sugar = 0;
		sim_plunder_open(&s, loot);
		CHECK(sim_take_all(&s, 1) == 10 && s.fleet.goods == 5 &&
		    s.fleet.food == 3 && s.fleet.sugar == 2 &&
		    sim_hold_space(&s) == 0);
		sim_plunder_close(&s);
	}
	sim_install_governor(&s, 1, (n + 1) & 3);
	CHECK(t->nation == ((n + 1) & 3) && t->standing == 2 &&
	    s.standing[(n + 1) & 3] >= 3);
	/* Warned towns hide their gold; a small crew only plunders. */
	sea_game(&s, 300, 300, 0);
	t = &s.towns[1];
	t->citizens = 50;
	t->gold = 10;
	s.heat = 40;
	s.fleet.crew = 20;
	sim_sack(&s, 1, &k);
	CHECK(k.hidden == 1 && !k.conquest && k.gold == 10 * 1500 / 55 * 10);
	CHECK(s.news[(s.nnews - 1) % SIM_NEWS_MAX].code == NEWS_PLUNDERED);
	/* Repelled: the fallen, and a blot on the record. */
	sea_game(&s, 300, 300, 0);
	t = &s.towns[1];
	t->forts = 0;
	s.fleet.crew = 50;
	s.reputation = 4;
	sim_assault_start(&s, 1, 0, 2);
	CHECK(s.sword.scene == 0);
	s.sword.result = SWORD_REPELLED;
	s.sword.you.men = 30;
	CHECK(sim_assault_end(&s, 1) == 0 && s.fleet.crew == 30 &&
	    s.reputation == 3);
}

/* Leaving a town: on its water, bows away from it (field 28). */
static void
test_leave_heading(void)
{
	struct sim s;

	sea_game(&s, 300, 300, 0);
	s.towns[1].sea_x = 120;
	s.towns[1].sea_y = 60;
	s.towns[1].sea_heading = 5;
	s.fleet.heading = 0;
	sim_enter_town(&s, 1);
	sim_leave_town(&s);
	CHECK(s.player_town == -1 && s.fleet.leaving == 1);
	CHECK(s.fleet.x == 120 * 16 + 8 && s.fleet.y == 60 * 16 + 8 &&
	    s.fleet.heading == 160 && s.fleet.speed == 0);
}

/* The arrival: the fort's welcome, a lost ship's cargo, sneaking in. */
static void
test_arrival(void)
{
	struct sim s;
	struct rng r;
	double x;
	int i, n, in = 0, fired = 0, sunk = 0, caught = 0;
	enum arrival a;

	/* A ship lost with its share: rooms 10 and 4 of 14. */
	sea_game(&s, 300, 300, 0);
	s.fleet.nships = 2;
	s.fleet.ships[0] = 4;
	s.fleet.ships[1] = 1;
	s.fleet.capacity = 14;
	s.fleet.gold = 1400;
	s.fleet.guns = 14;
	s.fleet.food = 7;
	s.fleet.goods = 20;
	s.fleet.sugar = 0;
	s.fleet.crew = 50;
	sim_drop_ship(&s, 1, 0);
	CHECK(s.fleet.nships == 1 && s.fleet.capacity == 10);
	CHECK(s.fleet.gold == 1000 && s.fleet.guns == 10 &&
	    s.fleet.food == 5 && s.fleet.goods == 14 && s.fleet.crew == 50);
	/* Kept cargo stays. */
	s.fleet.nships = 2;
	s.fleet.capacity = 14;
	sim_drop_ship(&s, 1, 1);
	CHECK(s.fleet.gold == 1000 && s.fleet.goods == 14);
	/* The last ship stays for the caller to maroon the player. */
	int ship0 = s.fleet.ships[0];
	sim_drop_ship(&s, 0, 0);
	CHECK(s.fleet.nships == 1 && s.fleet.ships[0] == ship0 &&
	    s.fleet.gold == 1000);

	/* Sailing in, replayed against the same random numbers. */
	for (i = 0; i < 200; i++) {
		sea_game(&s, 300, 300, 0);
		s.towns[1].forts = 2;
		s.towns[1].standing = (uint16_t)(int16_t)(i % 5 - 3);
		n = s.towns[1].nation;
		s.standing[n] = (int8_t)(i % 3 - 2);
		s.fleet.crew = 40 + i;
		s.fleet.nships = 2;
		s.fleet.ships[1] = 1;
		s.fleet.capacity = (s.fleet.ships[0] & 0xF) * 2 + 2 + 4;
		rng_seed(&s.rng, (uint64_t)i + 1, 1);
		r = s.rng;
		x = (double)(s.standing[n] + (int16_t)s.towns[1].standing) -
		    s.fleet.crew / 100.0;
		x = x * (rng_below(&r, 0x7FFF) * 2) / 65536.0;
		a = sim_sail_in(&s, 1);
		if (x > -0.5) {
			CHECK(a == ARRIVE_ENTER && s.player_town == 1);
			in++;
		} else if (x > -1.0) {
			CHECK(a == ARRIVE_FIRED && s.player_town == -1 &&
			    s.fleet.nships == 2);
			fired++;
		} else {
			CHECK(a == ARRIVE_SUNK && s.player_town == -1 &&
			    s.fleet.nships == 1);
			sunk++;
		}
	}
	CHECK(in > 0 && fired > 0 && sunk > 0);
	/* No forts: always in. */
	sea_game(&s, 300, 300, 0);
	s.towns[1].forts = 0;
	s.standing[s.towns[1].nation] = -100;
	CHECK(sim_sail_in(&s, 1) == ARRIVE_ENTER);

	/* Sneaking in: standing x reputation against -rand(50). */
	for (i = 0; i < 100; i++) {
		sea_game(&s, 300, 300, 0);
		s.towns[1].standing = (uint16_t)(int16_t)-(i % 7);
		s.reputation = i % 9;
		rng_seed(&s.rng, (uint64_t)i + 1, 1);
		r = s.rng;
		n = -(int16_t)s.towns[1].standing * s.reputation;
		a = sim_sneak_in(&s, 1);
		CHECK(s.visit_flags & VISIT_SNEAKED);
		if (-n > -rng_below(&r, 50)) {
			CHECK(a == ARRIVE_ENTER && s.player_town == 1);
		} else {
			CHECK(a == ARRIVE_CAUGHT && s.player_town == -1);
			caught++;
		}
	}
	CHECK(caught > 0 && caught < 100);
	/* The guard's duel: a loss disgraces. */
	sea_game(&s, 300, 300, 0);
	s.reputation = 6;
	sim_guard_duel(&s, 1, 0);
	CHECK(s.sword.active && s.sword.kind == SWORD_DUEL &&
	    s.sword.you.men == 1 && s.sword.foe.weapon == 0 &&
	    s.sword.title == 0);
	s.sword.result = SWORD_WON;
	CHECK(sim_duel_end(&s) == 0 && s.reputation == 6);
	sim_guard_duel(&s, 1, 0);
	s.sword.result = SWORD_REPELLED;
	CHECK(sim_duel_end(&s) == 1 && s.reputation == -1);
}

/* Cannon at the shipwright: 50 gold each, within both purses. */
static void
test_cannon(void)
{
	struct sim s;

	sea_game(&s, 300, 300, 0);
	sim_enter_town(&s, 1);
	s.fleet.guns = 6;
	s.fleet.gold = 0;
	s.fleet.capacity = 4;
	s.fleet.food = s.fleet.goods = s.fleet.sugar = 0;
	s.wright_gold = 120;
	/* Two sold: the shipwright's 120 pays for no third. */
	CHECK(sim_trade_cannon(&s, -3) == 2 && s.fleet.guns == 4 &&
	    s.fleet.gold == 100 && s.wright_gold == 20 &&
	    s.wright_cannon == 2);
	/* Bought back: his stock of 2 is the limit. */
	CHECK(sim_trade_cannon(&s, 5) == 2 && s.fleet.guns == 6 &&
	    s.fleet.gold == 0 && s.wright_cannon == 0);
	/* A hold exactly full refuses; an overfull one does not. */
	s.wright_cannon = 2;
	s.fleet.gold = 500;
	s.fleet.food = 37;		/* 40 - 37 - 6 / 2 = 0 */
	CHECK(sim_trade_cannon(&s, 1) == 0);
	s.fleet.food = 38;		/* -1 */
	CHECK(sim_trade_cannon(&s, 1) == 1 && s.fleet.guns == 7);
	/* None left to sell. */
	s.fleet.guns = 0;
	CHECK(sim_trade_cannon(&s, -1) == 0);
}

/* The bank: dividing the plunder, then another expedition. */
static void
test_bank(void)
{
	struct sim s;
	struct plunder p;
	int32_t day;

	sea_game(&s, 300, 300, 0);
	sim_enter_town(&s, 1);
	s.player.difficulty = 1;
	s.reputation = 0;
	s.fortune = 0;
	s.fleet.gold = 10000;
	s.fleet.crew = 40;
	/* 10000 / 40 x 0.8 + 0.5 = 200; sqrt 14 / 23: angry; the
	 * reputation (14 + 0 + 1) / 2 = 7; the captain 2 x 10000 / 20. */
	sim_divide_plunder(&s, &p);
	CHECK(p.share == 200 && p.mood == 0 && s.reputation == 7);
	CHECK(p.portions == 4 && p.yours == 1000 && s.fortune == 1000);
	/* Happy from sqrt(share) / 23 >= 3, that is a share of 4761. */
	s.fleet.gold = 4761 * 50 / 4;		/* share 4761.25 + 0.5 */
	s.fleet.crew = 10;
	sim_divide_plunder(&s, &p);
	CHECK(p.share == 4761 && p.mood == 3);
	s.fleet.gold = 6600;			/* a share of 528: angry */
	sim_divide_plunder(&s, &p);
	CHECK(p.share == 528 && p.mood == 0);
	s.fleet.crew = 8;
	s.fleet.gold = 5290;			/* 529: sqrt 23, unhappy */
	sim_divide_plunder(&s, &p);
	CHECK(p.share == 529 && p.mood == 1);

	/* Another expedition: the flagship alone, a tenth of the gold, a
	 * new crew of reputation + 16, 104 days of refit. */
	s.fleet.nships = 3;
	s.fleet.ships[0] = 4;
	s.fleet.damage[0] = 40;
	s.fleet.gold = 10000;
	s.fleet.goods = 30;
	s.reputation = 7;
	day = s.day;
	sim_new_expedition(&s, 0);
	CHECK(s.fleet.nships == 1 && s.fleet.ships[0] == 4 &&
	    s.fleet.damage[0] == 0);
	CHECK(s.fleet.gold == 1000 && s.fleet.capacity == 10 &&
	    s.fleet.guns == 14 && s.fleet.food == 15 && s.fleet.goods == 0);
	CHECK(s.fleet.crew == 23 && s.day == day + 104);
	/* Advancing the difficulty; no gold: a tenth of the fortune, less
	 * a tenth. */
	s.fleet.gold = 0;
	s.fortune = 5000;
	sim_new_expedition(&s, 1);
	CHECK(s.player.difficulty == 2 && s.fortune == 4500 &&
	    s.fleet.gold == 450);
	s.player.difficulty = 3;
	sim_new_expedition(&s, 1);
	CHECK(s.player.difficulty == 3);
}

/* Treasure maps: pieces, the spot, and digging. */
static void
test_treasure(void)
{
	struct sim s;
	struct sim_find found[SIM_MAPS];
	const struct sim_map *m = &s.maps[MAP_PIRATE];
	int k, day;
	long gold;

	sea_game(&s, 300, 300, 0);
	s.player_town = 1;
	s.player.difficulty = 1;
	/* Pieces one at a time; the spot is drawn once, away from the
	 * player's town (Beta) and the empty maps' town (Alpha). */
	sim_map_piece(&s, MAP_PIRATE);
	CHECK(quarters(m->pieces) == 1 && (m->pieces & MAP_SHOWN));
	CHECK(m->town == 2 || m->town == 3);
	CHECK(m->x == s.towns[m->town].x + 3 && m->y == s.towns[m->town].y);
	k = m->x;
	for (day = 0; day < 4; day++)
		sim_map_piece(&s, MAP_PIRATE);
	CHECK(m->pieces == (MAP_WHOLE | MAP_SHOWN) && m->x == k);
	/* The easiest level gets the whole map at once. */
	s.player.difficulty = 0;
	sim_map_piece(&s, MAP_RELATIVE);
	CHECK(s.maps[MAP_RELATIVE].pieces == (MAP_WHOLE | MAP_SHOWN));
	CHECK(s.maps[MAP_RELATIVE].town != m->town);
	/* Digging away from any spot: three days, nothing. */
	s.fleet.ashore = 1;
	day = s.day;
	gold = s.fleet.gold;
	CHECK(sim_search(&s, found) == 0 && s.day == day + 3 &&
	    s.fleet.gold == gold);
	/* At the pirate's spot (a cell off counts): 5000-13000 gold. */
	s.fleet.x = (m->x + 1) * WORLD_TILE + 8;
	s.fleet.y = (m->y - 1) * WORLD_TILE + 8;
	CHECK(sim_search(&s, found) == 1 && found[0].map == MAP_PIRATE &&
	    found[0].gold >= 5000 && found[0].gold <= 13000 &&
	    found[0].gold % 1000 == 0 && s.fleet.gold == gold +
	    found[0].gold && m->pieces == 0);
	CHECK(news_abs(&s, s.nnews - 1)->code == NEWS_TREASURE);
	/* The relative rescued tells of a lost treasure. */
	s.fleet.x = s.maps[MAP_RELATIVE].x * WORLD_TILE + 8;
	s.fleet.y = s.maps[MAP_RELATIVE].y * WORLD_TILE + 8;
	s.villains[2].active = 1;		/* a villain on the trail */
	s.villains[2].trail[0] = 5;
	CHECK(sim_search(&s, found) == 1 && found[0].map == MAP_RELATIVE &&
	    found[0].value == 0 && s.relative == 1);
	/* Every villain is let go, his trail forgotten (VA 0xa801). */
	CHECK(!s.villains[2].active && s.villains[2].trail[0] == -1);
	CHECK(s.maps[MAP_LOST].pieces != 0 && s.maps[MAP_LOST].value == 0);
	CHECK(news_abs(&s, s.nnews - 1)->code == NEWS_RESCUED);
	gold = s.fleet.gold;
	s.fleet.x = s.maps[MAP_LOST].x * WORLD_TILE + 8;
	s.fleet.y = s.maps[MAP_LOST].y * WORLD_TILE + 8;
	CHECK(sim_search(&s, found) == 1 && found[0].map == MAP_LOST &&
	    found[0].gold == 100000 && s.fleet.gold == gold + 100000);
	CHECK(news_abs(&s, s.nnews - 1)->code == NEWS_LOST_TREASURE);
	/* The map seller stays away once the map is whole. */
	s.maps[MAP_PIRATE].pieces = MAP_WHOLE | MAP_SHOWN;
	for (k = 0; k < 20; k++) {
		s.tavern_spots = TAVERN_MAPS;
		sim_tavern_open(&s);
		CHECK(!(s.tavern_spots & TAVERN_MAPS));
	}
}

/* The relatives' villains: named after a promotion, fought in their
 * town, beaten for a piece of the relative's map. */
static void
test_villains(void)
{
	struct sim s;
	int k, v;

	sea_game(&s, 300, 300, 0);
	s.player.difficulty = 1;
	v = sim_villain_appear(&s);
	CHECK(v == 0 && s.villains[0].active &&
	    s.towns[s.villains[0].town].nation == NATION_SPAIN);
	CHECK(s.villains[0].surname < 16 && s.villains[0].rank < 8 &&
	    s.villains[0].nation < 4);
	CHECK(news_abs(&s, s.nnews - 1)->code == NEWS_VILLAIN &&
	    news_abs(&s, s.nnews - 1)->a == 0);
	for (k = 1; k < SIM_VILLAINS; k++)
		CHECK(sim_villain_appear(&s) == k);
	CHECK(sim_villain_appear(&s) == -1);	/* four at most */
	/* Met in his town: the mystery man is in the tavern. */
	s.player_town = s.villains[2].town;
	CHECK(sim_villain_at(&s, s.player_town) >= 0);
	v = sim_villain_at(&s, s.player_town);
	s.tavern_spots = TAVERN_MYSTERY;
	sim_tavern_open(&s);
	CHECK(s.tavern_spots & TAVERN_MYSTERY);
	/* Beaten in the duel: a piece of the relative's map. */
	sim_villain_duel(&s, v, 1);
	CHECK(s.sword.active && s.sword.kind == SWORD_DUEL &&
	    s.sword.nation == -2 && s.villain_duel == v);
	s.sword.result = SWORD_WON;
	CHECK(sim_villain_duel_end(&s) == 1 && !s.villains[v].active &&
	    s.villain_duel == -1 && quarters(s.maps[MAP_RELATIVE].pieces) == 1);
	/* A loss disgraces; the villain lives on elsewhere. */
	v = 0;
	sim_villain_duel(&s, v, 1);
	s.sword.result = SWORD_LOST;
	CHECK(sim_villain_duel_end(&s) == 0 && s.villains[v].active &&
	    s.reputation == -1);
	/* Nowhere to hide: no mystery man. */
	for (k = 0; k < SIM_VILLAINS; k++)
		s.villains[k].active = 0;
	s.tavern_spots = TAVERN_MYSTERY;
	sim_tavern_open(&s);
	CHECK(!(s.tavern_spots & TAVERN_MYSTERY));
	/* No more villains once the relatives are all found, nor in a
	 * famous expedition. */
	s.relative = 4;
	CHECK(sim_villain_appear(&s) == -1);
	s.relative = 0;
	s.player.expedition = 2;
	CHECK(sim_villain_appear(&s) == -1);
	s.player.expedition = -1;
	CHECK(sim_villain_appear(&s) >= 0);
}

/* Governors' missions: offered, done, reported. */
static void
test_missions(void)
{
	struct sim s;
	struct sack sack;
	int k, type = 0, dest, st;

	sea_game(&s, 300, 300, 0);
	s.player_town = 0;			/* Alpha, Spanish */
	s.towns[0].standing = 0;
	s.standing[NATION_SPAIN] = 0;
	/* Standing 0 earns no mission. */
	for (k = 0; k < 50; k++)
		CHECK(sim_mission_draw(&s) == 0);
	s.standing[NATION_SPAIN] = 16;
	for (k = 0; k < 200 && type != MISSION_LETTER; k++)
		type = sim_mission_draw(&s);
	CHECK(type == MISSION_LETTER && s.mission_offer.dest != 0 &&
	    s.mission_offer.giver == 0 &&
	    s.relations[NATION_SPAIN][s.towns[s.mission_offer.dest].nation]
	    >= 0);
	dest = s.mission_offer.dest;
	sim_mission_accept(&s);
	CHECK(s.mission.type == MISSION_LETTER && s.mission.dest == dest &&
	    news_abs(&s, s.nnews - 1)->code == NEWS_LETTER);
	/* No second offer while one runs; the contact waits at dest. */
	CHECK(sim_mission_draw(&s) == 0);
	CHECK(sim_mission_contact(&s, dest) && !sim_mission_contact(&s, 0));
	s.player_town = dest;
	s.tavern_spots = TAVERN_MYSTERY;
	sim_tavern_open(&s);
	CHECK(s.tavern_spots & TAVERN_MYSTERY);
	sim_mission_done(&s);
	CHECK(s.mission.type == -MISSION_LETTER &&
	    news_abs(&s, s.nnews - 1)->code == NEWS_DELIVERED_LETTER);
	/* Back at the giver: +1 standing, his town's word 9 less 1. */
	CHECK(sim_mission_report(&s) == 0);	/* not the giver's town */
	s.player_town = 0;
	st = s.standing[NATION_SPAIN];
	CHECK(sim_mission_report(&s) == 1 && s.mission.type == 0 &&
	    s.standing[NATION_SPAIN] == st + 1 &&
	    (int16_t)s.towns[0].standing == -1);
	/* Back without doing it: a failure. */
	s.mission.type = MISSION_PAPERS;
	s.mission.giver = 0;
	s.mission.nation = NATION_SPAIN;
	st = s.standing[NATION_SPAIN];
	CHECK(sim_mission_report(&s) == -1 &&
	    s.standing[NATION_SPAIN] == st - 1 &&
	    (int16_t)s.towns[0].standing == -2 &&
	    news_abs(&s, s.nnews - 1)->code == NEWS_DISPLEASED);
	/* The son is freed in the sack of his prison town. */
	s.mission.type = MISSION_SON;
	s.mission.dest = 2;
	sim_sack(&s, 2, &sack);
	CHECK(sack.son_freed && s.mission.type == -MISSION_SON &&
	    (news_abs(&s, s.nnews - 1)->code == NEWS_SON_FREED ||
	    news_abs(&s, s.nnews - 2)->code == NEWS_SON_FREED));
	/* The pirate hunted, taken at sea. */
	s.mission.type = MISSION_PIRATE;
	s.mission.dest = 3;
	s.mission.pirate = 0x8000 | 5 << 3;
	memset(&s.battle, 0, sizeof(s.battle));
	s.battle.active = 1;
	s.battle.outcome = BAT_SURRENDER;
	s.battle.you.crew = 20;
	s.encounter.nation = NATION_PIRATE;
	s.encounter.named = 1;
	s.encounter.role = 7;
	k = s.fleet.mood;
	CHECK(sim_battle_finish(&s, 0) == BAT_SURRENDER &&
	    s.mission.type == -MISSION_PIRATE);
	CHECK(s.fleet.mood == k + 2);	/* a warship's capture cheers */
	s.player_town = 0;
	s.mission.giver = 0;
	CHECK(sim_mission_report(&s) == 2);
}

/* The crew's mood: its formula, the monthly talk at sea, mutiny and
 * desertion. */
static void
test_crew_mood(void)
{
	struct sim s;
	int town, k, stop = SAIL_TICKS;

	sea_game(&s, 300, 300, 0);
	s.player_town = -1;
	s.player.difficulty = 1;
	/* sqrt(10000 / 10 / 25) x 2 = 12, + 4 - 1. */
	s.fleet.gold = 10000;
	s.fleet.crew = 25;
	s.months = 0;
	CHECK(sim_crew_mood(&s) == 15);
	s.months = 3;
	CHECK(sim_crew_mood(&s) == 12);
	/* Leaving men take their share of the gold. */
	s.fleet.gold = 1000;
	s.fleet.crew = 10;
	sim_crew_leave(&s, 3);
	CHECK(s.fleet.gold == 700 && s.fleet.crew == 7);
	/* Broke for 5 months: restless, and sailing stops to say so. */
	s.fleet.gold = 0;
	s.fleet.crew = 30;
	s.fleet.food = 1000;		/* not hungry */
	s.months = 4;
	for (k = 0; k < 40 && stop != SAIL_CREW; k++)
		stop = sim_sail(&s, 0, 64, &town);
	CHECK(stop == SAIL_CREW && s.crew_event == CREW_RESTLESS &&
	    s.months == 5 && s.fleet.mood == -2);
	/* After 12 months the master speaks of dividing the plunder. */
	s.crew_event = CREW_NONE;
	s.months = 11;
	s.player.difficulty = 0;
	s.fleet.gold = 4000;		/* 2 x sqrt(13.3) = 7: mood -1 */
	sim_advance(&s, 30);
	CHECK(s.crew_event == CREW_ANXIOUS && s.fleet.mood == -1);
	/* Below -3: mutiny, a third leave, the leader duels. */
	s.crew_event = CREW_NONE;
	s.player.difficulty = 1;
	s.fleet.gold = 0;
	s.months = 7;
	sim_advance(&s, 30);
	CHECK(s.crew_event == CREW_MUTINY && s.fleet.crew == 20);
	sim_mutiny_duel(&s, 2);
	CHECK(s.sword.active && s.sword.title == 2 && s.sword.nation == 5 &&
	    s.sword.scene == 3);
	s.sword.result = SWORD_LOST;
	s.reputation = 3;
	sim_mutiny_end(&s);
	CHECK(s.reputation == -5 && !s.sword.active);
	/* In town the crew keeps quiet. */
	s.crew_event = CREW_NONE;
	s.player_town = 0;
	sim_advance(&s, 30);
	CHECK(s.crew_event == CREW_NONE);
	/* Leaving town: (3 - mood) x crew / 15 desert, when more than 1. */
	s.fleet.crew = 30;
	s.fleet.gold = 0;
	s.fleet.mood = -2;
	sim_leave_town(&s);
	CHECK(s.deserted == 10 && s.fleet.crew == 20);
	s.player_town = 0;
	s.fleet.crew = 30;
	s.fleet.mood = 2;
	sim_leave_town(&s);
	CHECK(s.deserted == 2 && s.fleet.crew == 28);
	s.player_town = 0;
	s.fleet.mood = 3;
	sim_leave_town(&s);
	CHECK(s.deserted == 0 && s.fleet.crew == 28);
}

/* Prison: the stay, the fresh start, the career's ends, health. */
static void
test_prison(void)
{
	struct sim s;
	int day;

	sea_game(&s, 300, 300, 0);
	s.player.expedition = -1;
	s.player.difficulty = 2;
	s.player.ability = ABILITY_FENCING;
	/* Health: (years + injuries) / 3, / 4 with medicine. */
	s.start_year = s.year - 4;
	s.injuries = 2;
	CHECK(sim_health(&s) == 2);
	s.player.ability = ABILITY_MEDICINE;
	CHECK(sim_health(&s) == 1);
	s.player.ability = ABILITY_FENCING;
	s.start_year = s.year;
	s.injuries = 0;
	/* Beaten in Beta: an injury, rep / 2 + 7 spells of 27 days, a
	 * fresh start from the fortune, the mission lost, one level down. */
	s.reputation = 4;
	s.fortune = 5000;
	s.fleet.gold = 9999;
	s.fleet.nships = 2;
	s.mission.type = MISSION_LETTER;
	s.player_town = 1;
	s.towns[1].sea_x = 125;
	s.towns[1].sea_y = 115;
	day = s.day;
	sim_imprison(&s, JAIL_TOWN, 1);
	CHECK(s.jail.fresh && !s.jail.end && s.jail.spells == 9 &&
	    s.day == day + 9 * 27 && s.injuries == 1);
	CHECK(s.fortune == 4500 && s.fleet.gold == 450 && s.fleet.nships == 1 &&
	    s.fleet.crew == 20 && s.months == 0 && s.mission.type == 0 &&
	    s.player.difficulty == 1 && s.player_town == -1);
	/* Out on the town's water. */
	CHECK(s.fleet.x / WORLD_TILE == s.towns[1].sea_x &&
	    s.fleet.y / WORLD_TILE == s.towns[1].sea_y);
	/* Marooned: by the nearest shore, an injury less. */
	s.fleet.x = 104 * WORLD_TILE + 8;	/* west of the test land */
	s.fleet.y = 45 * WORLD_TILE + 8;
	s.reputation = -20;			/* no time at all */
	day = s.day;
	sim_imprison(&s, JAIL_MAROONED, -1);
	CHECK(s.jail.spells == 0 && s.day == day && s.injuries == 1);
	CHECK(world_terrain(&test_world, s.fleet.x, s.fleet.y) !=
	    TERRAIN_LAND && s.fleet.x / WORLD_TILE == 109 &&
	    s.fleet.heading == 0xC0);
	/* A shore facing south: out on the sea below it, heading south
	 * (the original stepped onto the land). */
	test_world.coast[1] = 0x22;
	s.fleet.x = 120 * WORLD_TILE + 8;
	s.fleet.y = 53 * WORLD_TILE + 8;	/* south of the test land */
	sim_imprison(&s, JAIL_MAROONED, -1);
	CHECK(world_terrain(&test_world, s.fleet.x, s.fleet.y) !=
	    TERRAIN_LAND && s.fleet.y / WORLD_TILE == 50 &&
	    s.fleet.heading == 0x80);
	test_world.coast[1] = 0x33;
	/* A famous expedition ends in prison; so does health 3. */
	s.player.expedition = 2;
	sim_imprison(&s, JAIL_SHIP, -1);
	CHECK(s.jail.end == 1 && s.injuries == 2);
	s.player.expedition = -1;
	s.injuries = 8;
	sim_imprison(&s, JAIL_SHIP, -1);
	CHECK(s.jail.end == 2);
}

/* Named captains: held after a capture, ransomed, escaping. */
static void
test_prisoners(void)
{
	struct sim s;
	int k, id, tries, held = 0, slot;
	long gold;

	sea_game(&s, 300, 300, 0);
	/* A new game starts with surnames 0-3 used: the first captain is
	 * surname 4 (5 when 2 x rand(8) carries into bit 3). */
	CHECK(s.surnames == 0x0F);
	id = sim_name_captain(&s, 2);
	CHECK(((id >> 3) & 7) == 4 || ((id >> 3) & 7) == 5);
	CHECK(s.surnames == 0x1F);
	/* Five surnames before one comes back; nation in bits 6-7. */
	for (k = 0; k < 5; k++) {
		id = sim_name_captain(&s, 2);
		CHECK(id != 0 && (id >> 6) == 2);
	}
	CHECK(s.surnames == 0x1F);
	/* An English captain taken: held at once. */
	memset(&s.battle, 0, sizeof(s.battle));
	s.battle.active = 1;
	s.battle.outcome = BAT_SURRENDER;
	s.battle.you.crew = 20;
	s.encounter.nation = NATION_ENGLAND;
	s.encounter.captain = 1 << 6 | 3 << 3 | 4;
	s.encounter.named = 0;
	CHECK(sim_battle_finish(&s, 0) == BAT_SURRENDER &&
	    s.prisoners[0] == (1 << 6 | 3 << 3 | 4) && s.hostage == 0);
	/* A pirate: a hostage first; held, marked as a pirate. */
	s.battle.active = 1;
	s.battle.outcome = BAT_SURRENDER;
	s.encounter.nation = NATION_PIRATE;
	s.encounter.captain = 2 << 6 | 1 << 3 | 2;
	CHECK(sim_battle_finish(&s, 0) == BAT_SURRENDER &&
	    s.hostage == (2 << 6 | 1 << 3 | 2) && s.prisoners[1] == 0);
	sim_hostage_answer(&s, 2);
	CHECK(s.hostage == 0 && s.prisoners[1] == (0x100 | 2 << 6 | 1 << 3 | 2));
	/* A Spaniard traded for news is let go. */
	s.hostage = 5 << 3;
	sim_hostage_answer(&s, 0);
	CHECK(s.hostage == 0 && s.prisoners[2] == 0);
	/* Ransom: up to 4 tries a visit; (rank + 1) x 500 x 1..3. */
	gold = s.fleet.gold;
	for (k = 0, slot = -1; k < 50 && slot < 0; k++) {
		tries = 0;
		slot = sim_ransom_offer(&s, &tries);
		CHECK(tries <= 4);
	}
	CHECK(slot == 0 || slot == 1);
	id = s.prisoners[slot] & 0xFF;
	CHECK(s.ransom_gold % ((id & 7) + 1) * 500 == 0 &&
	    s.ransom_gold >= ((id & 7) + 1) * 500 &&
	    s.ransom_gold <= ((id & 7) + 1) * 1500);
	sim_ransom_take(&s);
	CHECK(s.prisoners[slot] == 0 && s.fleet.gold == gold + s.ransom_gold);
	/* One jumps overboard at a roll call now and then. */
	s.prisoners[0] = s.prisoners[1] = 0;
	for (k = 0; k < SIM_PRISONERS; k++)
		s.prisoners[k] = 8 + k;
	s.fleet.mood = 20;
	for (k = 0, id = 0; k < 40 && id == 0; k++) {
		s.player_town = 0;
		sim_leave_town(&s);
		id = s.escaped;
	}
	CHECK(id != 0);
	for (k = 0, held = 0; k < SIM_PRISONERS; k++)
		held += s.prisoners[k] == 0;
	CHECK(held == 1);
	/* Prison frees them all. */
	s.player.expedition = -1;
	s.start_year = s.year;
	sim_imprison(&s, JAIL_SHIP, -1);
	for (k = 0; k < SIM_PRISONERS; k++)
		CHECK(s.prisoners[k] == 0);
}

/* The treasure fleet and the silver train. */
static void
test_convoys(void)
{
	struct era_towns era;
	struct scenario scn;
	struct sim s;
	struct sack sack;
	int k, wealth, gold, seen = 0;
	long before;

	make_era(&era, &scn);
	for (k = 0; k < CONVOY_STOPS; k++)
		era.fleet_route[k] = era.train_route[k] = -1;
	/* Alpha and Beta are Spanish, Gamma English: passed by. */
	era.fleet_route[0] = 0;
	era.fleet_route[1] = 2;
	era.fleet_route[2] = 1;
	era.train_route[0] = 1;
	sim_new_game(&s, &era, &scn, 3);
	s.world = &test_world;
	/* The start, by the date: stop doy / 15 - 18 (+24), +2 with an
	 * odd era code (1620: 3, 1660: 5). */
	k = s.day_of_year / 15 - 18 + ((s.era_year == 1620 ||
	    s.era_year == 1660) ? 2 : 0);
	if (k < 0)
		k += 24;
	CHECK(s.fleet_step == k);
	CHECK(s.fleet_town == (k < CONVOY_STOPS ? era.fleet_route[k] : -1));
	/* The fleet's month: back to the start, 1 or 2 stops on, never in
	 * the English town; where it stops wealth +10, gold / 4. */
	s.towns[0].wealth = s.towns[1].wealth = 100;
	s.towns[0].gold = s.towns[1].gold = 400;
	for (k = 0; k < 40; k++) {
		s.fleet_step = -1 + 0;	/* as at the restart */
		s.fleet_town = -1;
		s.day_of_year = 8 * 30 - 1;	/* the next update is month 9 */
		s.month = 8;
		wealth = s.towns[0].wealth + s.towns[1].wealth;
		gold = s.towns[0].gold + s.towns[1].gold;
		sim_advance(&s, 1);
		CHECK(s.fleet_town != 2);
		if (s.fleet_town == 0 || s.fleet_town == 1) {
			seen |= 1 << s.fleet_town;
			CHECK(s.towns[0].wealth + s.towns[1].wealth >= wealth);
			CHECK(s.towns[s.fleet_town].gold <= 100 ||
			    s.towns[0].gold + s.towns[1].gold < gold);
		}
		s.towns[0].wealth = s.towns[1].wealth = 100;
		s.towns[0].gold = s.towns[1].gold = 400;
	}
	CHECK(seen == 3);
	/* An attack on the fleet's town: 10 soldiers more. */
	s.fleet_town = k = 1;
	CHECK(sim_convoys_at(&s, k) == CONVOY_FLEET);
	s.train_town = -1;
	s.towns[k].soldiers = 20;
	sim_attack_begin(&s, k);
	CHECK(s.towns[k].soldiers == 30);
	/* Sacked: the fleet taken, the galleon's cargo x 20 in tens. */
	before = s.fleet.gold;
	s.heat = 0;
	s.towns[k].gold = 0;
	test_world.treasure_ship[5] = 100;
	sim_sack(&s, k, &sack);
	CHECK((sack.convoys & CONVOY_FLEET) && sack.gold == 100 * 20 * 10 &&
	    s.fleet.gold == before + sack.gold && s.fleet_town == -1 &&
	    s.fleet_step == 16);
	CHECK(news_abs(&s, s.nnews - 1)->code == NEWS_FLEET_TAKEN ||
	    news_abs(&s, s.nnews - 2)->code == NEWS_FLEET_TAKEN);
	test_world.treasure_ship[5] = 0;
	/* Past its last stop a convoy is away. */
	s.fleet_step = 13;
	s.day_of_year = 30 - 1;
	s.month = 0;
	for (k = 0; k < 3; k++)
		sim_advance(&s, 30);
	CHECK(s.fleet_town == -1);
}

/* The governor's daughter: met, charmed, wed after a duel. */
static void
test_daughter(void)
{
	struct sim s;
	struct daughter d;
	int k, won = 0;

	sea_game(&s, 300, 300, 0);
	s.player_town = 0;
	s.towns[0].nation = NATION_SPAIN;
	s.towns[0].governor = 0;
	s.player.difficulty = 1;
	s.player.ability = ABILITY_FENCING;
	s.rank[NATION_SPAIN] = 0;
	s.fortune = 0;
	/* (0 + 0 + 1) / 1 = 1, halved: 0, she stays away. */
	CHECK(sim_daughter_meet(&s, &d) == 1 && d.charm == 0);
	/* Rich and titled: (4990 / 499 + 2 x 3 + 1) / 1 = 17, halved 8. */
	s.fortune = 4990;
	s.rank[NATION_SPAIN] = 3;
	CHECK(sim_daughter_meet(&s, &d) == 1 && d.charm == 8 &&
	    d.suitor == 0 * 8 + 0 * 32 + 0 * 2 + 1 && !d.niece);
	s.player.ability = ABILITY_CHARM;
	CHECK(sim_daughter_meet(&s, &d) == 1 && d.charm == 16);
	/* A niece halves the interest first. */
	s.towns[0].governor = GOV_NIECE;
	CHECK(sim_daughter_meet(&s, &d) == 1 && d.niece && d.charm == 8);
	s.towns[0].governor = 0;
	CHECK(sim_daughter_meet(&s, &d) == 1);
	/* Talk: now and then she becomes an informant. */
	for (k = 0; k < 40 && !won; k++)
		won = sim_daughter_talk(&s, &d);
	CHECK(won && (s.towns[0].governor & GOV_INFORMANT) &&
	    s.romances == 1);
	CHECK(sim_daughter_meet(&s, &d) == 2);
	/* A proposal: too little interest is refused. */
	d.charm = 1;
	CHECK(sim_daughter_propose(&d) == 142);
	d.charm = 3;
	CHECK(sim_daughter_propose(&d) == 143);
	d.charm = 8;
	CHECK(sim_daughter_propose(&d) == 144);
	/* The suitor beaten: married, the town pleased. */
	s.towns[0].standing = 0;
	sim_daughter_duel(&s, &d, 1);
	CHECK(s.sword.active && s.sword.nation == -1 &&
	    s.sword.title == d.suitor);
	s.sword.result = SWORD_WON;
	CHECK(sim_daughter_duel_end(&s, &d) == 1 && s.married == 2 &&
	    (s.towns[0].governor & GOV_WIFE) &&
	    (int16_t)s.towns[0].standing == 2);
	/* Married, no other daughter comes. */
	s.player_town = 1;
	s.towns[1].governor = 0;
	CHECK(sim_daughter_meet(&s, &d) == 0);
}

/* The merchant's daughter: met now and then, wed for his debt. */
static void
test_merchant_daughter(void)
{
	struct sim s;
	int k, met = 0;

	sea_game(&s, 300, 300, 0);
	s.player_town = 0;
	s.towns[0].family = 0;
	s.married = 0;
	s.player.ability = ABILITY_FENCING;
	for (k = 0; k < 200; k++)
		met += sim_merchant_after(&s) == 1;
	CHECK(met > 5 && met < 50);		/* one time in ten */
	s.fortune = 399;
	CHECK(sim_merchant_propose(&s) == 297);
	s.player.ability = ABILITY_CHARM;
	CHECK(sim_merchant_propose(&s) == 298);	/* 200 with charm */
	s.fleet.gold = 499;
	CHECK(sim_merchant_pay_debt(&s) == 300 && s.married == 0);
	s.fleet.gold = 600;
	CHECK(sim_merchant_pay_debt(&s) == 299 && s.married == 1 &&
	    s.fleet.gold == 100 && (s.towns[0].family & 0x40));
	/* Family now: dinner every visit; no other daughter elsewhere. */
	CHECK(sim_merchant_after(&s) == 2);
	s.player_town = 1;
	s.towns[1].family = 0;
	for (k = 0; k < 50; k++)
		CHECK(sim_merchant_after(&s) == 0);
}

/* The villains move from town to town, leaving a trail. */
static void
test_villain_moves(void)
{
	struct sim s;
	int k, list[SIM_VILLAINS], n, where, moved = 0;

	sea_game(&s, 300, 300, 0);
	s.player.difficulty = 3;
	/* Alpha and Beta, Spanish, 3 cells apart; Gamma English. */
	s.towns[0].x = 100; s.towns[0].y = 100;
	s.towns[1].x = 102; s.towns[1].y = 101;
	s.towns[0].garrison = s.towns[1].garrison = 0;
	memset(s.villains, 0, sizeof(s.villains));
	s.villains[0].active = 1;
	s.villains[0].town = 0;
	for (k = 0; k < 4; k++)
		s.villains[0].trail[k] = -1;
	/* Beta richer in soldiers than gold scores higher. */
	s.towns[0].gold = 800; s.towns[0].soldiers = 0;
	s.towns[1].gold = 0; s.towns[1].soldiers = 80;
	s.towns[0].standing = s.towns[1].standing = 0;
	for (k = 0; k < 20 && !moved; k++) {
		sim_villains_move(&s);
		moved = s.villains[0].town == 1;
	}
	CHECK(moved && s.villains[0].trail[0] == 0 &&
	    s.towns[1].garrison == 1 && s.towns[0].garrison == 0);
	/* Never to an English town, nor back to a town with a villain. */
	for (k = 0; k < 20; k++) {
		sim_villains_move(&s);
		CHECK(s.villains[0].town == 0 || s.villains[0].town == 1);
	}
	/* News in Alpha: he left for Beta, or he is here. */
	s.villains[0].town = 1;
	s.villains[0].trail[0] = 0;
	n = sim_villain_news(&s, 0, list);
	CHECK(n == 1 && list[0] == 0);
	CHECK(sim_villain_whereabouts(&s, 0, 0, &where) == 107 &&
	    where == 1);
	CHECK(sim_villain_whereabouts(&s, 0, 1, &where) == 108 &&
	    where == 1);
	CHECK(news_abs(&s, s.nnews - 1)->code == NEWS_VILLAIN);
	CHECK(sim_villain_news(&s, 3, list) == 0);
	/* Leaving a town with the heat high raises its nation's alarm. */
	s.player_town = 1;
	s.heat = 20;
	sim_leave_town(&s);
	CHECK(s.alert[NATION_SPAIN] == 1);
}

/* The end of a career: its points, score, rating and words. */
static void
test_career_end(void)
{
	struct sim s;
	struct career_end e;

	sea_game(&s, 300, 300, 0);
	s.player.expedition = -1;
	s.start_year = s.year - 5;
	s.fortune = 50000;
	s.acres = 4;
	memset(s.rank, 0, sizeof(s.rank));
	s.rank[NATION_ENGLAND] = 4;
	s.rank[NATION_FRANCE] = 2;
	s.reputation = 10;
	s.married = 2;
	s.relative = 1;
	s.injuries = 1;
	sim_career_end(&s, &e);
	/* 5000 + 200 + 3000 + 2000 + (2 + 1 - 1) x 1000 */
	CHECK(e.points == 12200 && e.ranks == 6);
	CHECK(e.score == (int)(12200 / 330.0 * 20 / 20.0));	/* 36 */
	CHECK(e.rating == 19 - 36 / 5 - 1);		/* 11 */
	CHECK(e.member == 11 && e.age == 30);
	/* sqrt((400 + 50000) / 500) = 10, at most 6, + 2 */
	CHECK(e.wealth == 8);
	CHECK(e.repute == 10 / 4 + 1 + 2);
	/* (1/8 + 2/8 + 1 - 1/6) x 20/20 x 3 = 3.6: 3, + 1 */
	CHECK(e.mood == 4);
	/* A poor, unknown, hurt captain. */
	s.fortune = 0;
	s.acres = 0;
	memset(s.rank, 0, sizeof(s.rank));
	s.reputation = -5;
	s.married = s.relative = 0;
	s.injuries = 3;
	sim_career_end(&s, &e);
	CHECK(e.points == -1000 - 3000 && e.score < 1 && e.rating >= 19 &&
	    e.member == 4 && e.wealth == 2 && e.repute == 2);
	/* (0 + 1 - 3/6) x 3 = 1.5: 1, + 1 */
	CHECK(e.mood == 2);
	/* A famous expedition tells its share: 200 x 0.7 / 40 = 3.5, kept
	 * at 3; a large one stops at 6. */
	s.player.expedition = 1;
	s.fortune = 200;
	sim_career_end(&s, &e);
	CHECK(e.repute == 3 + 2 && e.mood == 6);
	s.fortune = 5000;
	sim_career_end(&s, &e);
	CHECK(e.repute == 6 + 2);
	s.player.expedition = -1;
}

/* Can the crew sail the fleet? */
static void
test_crew_check(void)
{
	struct sim s;

	sea_game(&s, 300, 300, 0);
	s.player.expedition = -1;
	s.start_year = s.year;
	s.fleet.nships = 3;
	s.fleet.ships[0] = 4;
	s.fleet.ships[1] = 2;
	s.fleet.ships[2] = 0;
	s.fleet.capacity = 20;
	s.fleet.guns = s.fleet.food = s.fleet.goods = s.fleet.sugar = 0;
	s.fleet.crew = 24;
	CHECK(sim_crew_check(&s) == CHECK_OK);
	s.fleet.crew = 23;		/* 8 men a ship */
	CHECK(sim_crew_check(&s) == CHECK_ABANDON);
	s.fleet.goods = 50;
	sim_abandon_ship(&s, 1);	/* her cargo stays aboard */
	CHECK(s.fleet.nships == 2 && s.fleet.goods == 50 &&
	    sim_crew_check(&s) == CHECK_OK);
	/* The hold too small now: throw some overboard. */
	s.fleet.goods = s.fleet.capacity * 10 + 1;
	CHECK(sim_crew_check(&s) == CHECK_HOLD);
	s.fleet.goods = 0;
	/* Seven men for one ship: it is lost, the captain marooned. */
	s.fleet.nships = 1;
	s.fleet.crew = 7;
	CHECK(sim_crew_check(&s) == CHECK_LAST);
	s.player_town = -1;
	sim_abandon_last(&s);
	CHECK(s.jail.fresh && s.jail.cause == JAIL_MAROONED);
}

/* A kept prize: a new slot, or with eight ships one replaced. */
static void
test_prize_place(void)
{
	struct sim s;
	int k;

	sea_game(&s, 300, 300, 0);
	s.fleet.nships = 3;
	for (k = 0; k < 3; k++) {
		s.fleet.ships[k] = 1;
		s.fleet.damage[k] = 0;
	}
	s.fleet.capacity = 3 * 4;
	s.fleet.crew = 10;
	s.fleet.nships = 4;			/* as capture() grows it */
	sim_prize_place(&s, 3, 4, 8);
	CHECK(s.fleet.ships[3] == 4 && s.fleet.damage[3] == 8 &&
	    s.fleet.capacity == 12 + 10 && s.fleet.crew == 24);
	/* Eight ships: the prize waits, then takes the chosen slot. */
	s.fleet.nships = 8;
	for (k = 4; k < 8; k++)
		s.fleet.ships[k] = 0;
	s.fleet.capacity = 3 * 4 + 10 + 4 * 2;
	s.prize_pending = 1;
	s.prize_type = 7;
	s.prize_damage = 0;
	sim_prize_place(&s, 5, s.prize_type, s.prize_damage);
	CHECK(!s.prize_pending && s.fleet.nships == 8 &&
	    s.fleet.ships[5] == 7 && s.fleet.capacity == 30 - 2 + 16 &&
	    s.fleet.crew == 40);
}

/* Off the destination of a pirate hunt, the pirate comes four times in
 * five. */
static void
test_hunted_pirate(void)
{
	struct sim s;
	int town, i, k, n, met = 0, named = 0, nat = -1;

	for (n = 0; n < 4 && nat < 0; n++)
		for (k = 0; k < ENCOUNTER_SHIPS; k++)
			if (test_world.encounter[n][k][1] == NATION_PIRATE) {
				nat = n;
				break;
			}
	CHECK(nat >= 0);
	sea_game(&s, 120, 112, 0x00);	/* 2 cells from Beta */
	s.encounters = 1;
	s.mission.type = MISSION_PIRATE;
	s.mission.dest = 1;
	s.mission.nation = nat;
	s.mission.pirate = 0x8000 | 2 << 3;
	for (i = 0; i < 600; i++) {
		s.encounter.stage = ENC_NONE;
		s.heat = 0;
		s.fleet.x = 120 * WORLD_TILE + 8;
		s.fleet.y = 112 * WORLD_TILE + 8;
		if (sim_sail(&s, 0, 32, &town) != SAIL_ENCOUNTER ||
		    s.encounter.town != 1)
			continue;
		met++;
		sim_encounter_act(&s, ACT_INVESTIGATE);
		named += s.encounter.named;
	}
	CHECK(met > 20 && named * 10 > met * 6);	/* about 4 in 5 */
}

/* A sea battle near a town: heat, and after a capture its wealth and
 * the crew's news. */
static void
test_battle_near_town(void)
{
	struct sim s;
	int dist, town, heat;

	sea_game(&s, 120, 112, 0x00);	/* 2 cells from Beta */
	town = sim_nearest_town(&s, &dist);
	CHECK(town == 1 && dist == 2);
	s.towns[town].wealth = 30;
	s.heat = 0;
	memset(&s.battle, 0, sizeof(s.battle));
	s.battle.active = 1;
	s.battle.outcome = BAT_SURRENDER;
	s.battle.you.crew = 20;
	s.battle.foe.type = 1;
	s.encounter.nation = NATION_PIRATE;
	CHECK(sim_battle_finish(&s, 0) == BAT_SURRENDER);
	CHECK(s.heat == 24 - 2 * 2 && s.towns[town].wealth == 20 &&
	    s.battle.news_town == town);
	/* Escaped, not captured: only the heat. */
	heat = s.heat;
	s.battle.active = 1;
	s.battle.outcome = BAT_ESCAPED;
	CHECK(sim_battle_finish(&s, 0) == BAT_ESCAPED);
	CHECK(s.heat == heat + 20 && s.towns[town].wealth == 20 &&
	    s.battle.news_town == -1);
	/* Far from towns, nothing. */
	sea_game(&s, 300, 300, 0x00);
	s.heat = 0;
	memset(&s.battle, 0, sizeof(s.battle));
	s.battle.active = 1;
	s.battle.outcome = BAT_SURRENDER;
	s.battle.you.crew = 20;
	s.encounter.nation = NATION_PIRATE;
	sim_battle_finish(&s, 0);
	CHECK(s.heat == 0 && s.battle.news_town == -1);
}

/* The personal log: visits, captures and engagements at sea. */
static void
test_personal_log(void)
{
	struct sim s;
	size_t n;

	sea_game(&s, 120, 112, 0x00);
	n = s.nnews;
	sim_enter_town(&s, 1);
	CHECK(s.nnews == n + 1 && news_abs(&s, s.nnews - 1)->code ==
	    NEWS_VISITED && news_abs(&s, s.nnews - 1)->a == 1);
	/* Every arrival is logged, sneaking in or after the traveller
	 * too (VA 0x1da14). */
	s.visit_flags |= VISIT_KEPT | VISIT_SNEAKED;
	sim_enter_town(&s, 1);
	CHECK(s.nnews == n + 2);
	s.player_town = -1;
	memset(&s.battle, 0, sizeof(s.battle));
	s.battle.active = 1;
	s.battle.outcome = BAT_SURRENDER;
	s.battle.you.crew = 20;
	s.encounter.nation = 1;
	s.encounter.type = 7;
	sim_battle_finish(&s, 0);
	CHECK(news_abs(&s, s.nnews - 1)->code == NEWS_CAPTURED &&
	    news_abs(&s, s.nnews - 1)->a == 1 * 16 + 7);
	s.battle.active = 1;
	s.battle.outcome = BAT_ESCAPED;
	sim_battle_finish(&s, 0);
	CHECK(news_abs(&s, s.nnews - 1)->code == NEWS_ENGAGED);
	/* Not against a fort. */
	n = s.nnews;
	s.battle.active = 1;
	s.battle.fort = 1;
	s.battle.outcome = BAT_ESCAPED;
	sim_battle_finish(&s, 0);
	CHECK(s.nnews == n);
}

/* Each month: land income, the reputation fading, standings bounded. */
static void
test_month_housekeeping(void)
{
	struct sim s;

	sea_game(&s, 300, 300, 0x00);
	s.acres = 5;
	s.fortune = 1000;
	s.reputation = 20;
	s.standing[1] = 120;
	s.standing[2] = -110;
	s.standing[3] = 50;
	sim_world_update(&s);
	CHECK(s.fortune == 1002 && s.reputation == 18 &&
	    s.standing[1] == 100 && s.standing[2] == -100 &&
	    s.standing[3] == 50);
	s.reputation = -9;		/* an eighth, rounded down */
	sim_world_update(&s);
	CHECK(s.reputation == -7);
	s.reputation = 7;
	sim_world_update(&s);
	CHECK(s.reputation == 7);
}

/* A capture's plunder: gold by the town's wealth and her cargo byte,
 * goods cut to her tonnage, her guns on the board, volunteers. */
static void
test_capture_plunder(void)
{
	struct sim s;
	int k, total, gold_seen = 0, vol_seen = 0, n;
	long gold;

	for (n = 0; n < 200; n++) {
		sea_game(&s, 300, 300, 0x00);
		s.towns[0].wealth = 100;
		memset(&s.battle, 0, sizeof(s.battle));
		s.battle.active = 1;
		s.battle.outcome = BAT_SURRENDER;
		s.battle.you.crew = 40;
		s.battle.foe.type = 1;		/* a sloop: 40 tons */
		s.battle.foe.guns = 7;
		s.encounter.nation = NATION_PIRATE;
		s.encounter.crew = 30;
		s.encounter.cargo = 20;
		s.encounter.gold = 200;
		s.reputation = 20;
		s.fleet.nships = 1;
		s.fleet.ships[0] = 7;		/* room for 288 */
		gold = s.fleet.gold;
		sim_battle_finish(&s, 0);
		CHECK(s.fleet.gold == gold + s.battle.loot_gold &&
		    s.battle.loot_gold % 10 == 0);
		/* the nearest town's (wealth + 64) / 16 = 10, x 20 */
		CHECK(s.battle.loot_gold < 10L * 20 * 10);
		gold_seen += s.battle.loot_gold > 0;
		CHECK(s.battle.loot[3] == 8);	/* 7 guns, rounded up */
		for (k = 0, total = 4; k < 3; k++)
			total += s.battle.loot[k];
		/* Cut toward 40 tons; a pass on an empty good still
		 * counts, as in the original, so a little may remain. */
		CHECK(total < 1 * 20 + 20 + 20);
		CHECK(s.battle.loot[0] <= 30 && s.battle.loot[1] < 100 &&
		    s.battle.loot[2] < 100);
		/* rand((20 + 5) x 30 / 10) = rand(75) */
		CHECK(s.battle.volunteers == 0 ||
		    (s.battle.volunteers > 1 && s.battle.volunteers < 75));
		vol_seen += s.battle.volunteers > 0;
	}
	CHECK(gold_seen > 150 && vol_seen > 150);
	k = s.fleet.crew + s.battle.volunteers;
	sim_take_volunteers(&s);
	CHECK(s.fleet.crew == k && s.battle.volunteers == 0);
}

/* The treasure fleet restarts its route in October, in September in
 * 1620 and 1660 (the era code's parity; the 1987 manual's calendar). */
static void
test_fleet_calendar(void)
{
	static const int sept[EXE_ERAS] = { 0, 0, 1, 0, 1, 0 };
	struct sim s;
	int e;

	for (e = 0; e < EXE_ERAS; e++) {
		sea_game(&s, 300, 300, 0x00);
		s.era_year = town_era_years[e];
		s.fleet_step = 10;
		s.month = 9 - sept[e];		/* 0-based: 9 is October */
		sim_world_update(&s);
		CHECK(s.fleet_step == 0 || s.fleet_step == 1);
		s.fleet_step = 10;
		s.month = 8 + sept[e];		/* the other month */
		sim_world_update(&s);
		CHECK(s.fleet_step >= 11);
	}
}

/* The crew eats a 256th of a ton a man a day; the master warns under
 * 21 days on multiples of 4; at none a third desert, food becomes 5. */
static void
test_food(void)
{
	struct sim s;
	int town, k, stop = SAIL_TICKS;

	sea_game(&s, 300, 300, 0x00);
	s.encounters = 0;
	s.months = 0;
	s.fleet.gold = 30000;		/* a happy crew */
	s.fleet.crew = 10;
	s.fleet.food = 1;		/* 25 days for 10 men */
	s.fleet.food_frac = 0;
	for (k = 0; k < 40 && stop != SAIL_CREW; k++)
		stop = sim_sail(&s, 0, 64, &town);
	CHECK(stop == SAIL_CREW && s.crew_event == CREW_HUNGRY &&
	    s.food_days == 20 && s.fleet.food == 0 &&
	    s.fleet.food_frac == 256 - 5 * 10);	/* 206 / 10 */
	s.crew_event = CREW_NONE;
	for (stop = SAIL_TICKS, k = 0; k < 40; k++) {
		stop = sim_sail(&s, 0, 64, &town);
		if (stop == SAIL_CREW && s.crew_event == CREW_STARVING)
			break;
		s.crew_event = CREW_NONE;	/* the warnings at 16..4 */
	}
	CHECK(s.crew_event == CREW_STARVING && s.fleet.crew == 7 &&
	    s.fleet.food == 5 && s.fleet.gold == 30000 - 3 * 3000);
}

/* Shallows sink the last ship bigger than a sloop, then wait 100
 * passes; a fleet of sloops crosses them safely. */
static void
test_reef(void)
{
	struct sim s;
	int town, x, y, stop;

	memset(test_world.map_terrain + 3 * WORLD_TILE * WORLD_TILE,
	    TERRAIN_SHALLOWS, WORLD_TILE * WORLD_TILE);	/* tile 3 */
	for (y = 280; y < 320; y++)
		for (x = 280; x < 320; x++)
			test_world.map[y * WORLD_CELLS_W + x] = 3;
	sea_game(&s, 300, 300, 0x00);
	s.encounters = 0;
	s.fleet.food = 1000;
	s.fleet.nships = 3;
	s.fleet.ships[0] = 1;
	s.fleet.ships[1] = 4;
	s.fleet.ships[2] = 1;
	s.fleet.capacity = 4 + 10 + 4;
	s.fleet.crew = 300;
	stop = sim_sail(&s, 0, 1, &town);
	CHECK(stop == SAIL_CREW && s.crew_event == CREW_WRECKED &&
	    s.fleet.nships == 2 && s.fleet.ships[1] == 1 &&
	    s.fleet.crew == 2 * 3 * 32 && s.fleet.reef_wait == 100);
	/* The wait survives a save and a load. */
	{
		struct sim t;
		struct jbuf st = { 0 };
		struct json *j;
		char err[128];

		sim_to_json(&s, &st);
		j = json_parse(st.s, err, sizeof(err));
		CHECK(j != NULL && sim_from_json(&t, j, err, sizeof(err)) == 0 &&
		    t.fleet.reef_wait == 100);
		json_free(j);
		jb_free(&st);
	}
	/* Only sloops left: safe. */
	s.crew_event = CREW_NONE;
	s.fleet.reef_wait = 0;
	CHECK(sim_sail(&s, 0, 64, &town) != SAIL_CREW);
	/* A lone ship bigger than a sloop: her loss maroons the player. */
	sea_game(&s, 300, 300, 0x00);
	s.encounters = 0;
	s.fleet.food = 1000;
	s.fleet.nships = 1;
	s.fleet.ships[0] = 4;
	s.fleet.crew = 100;
	stop = sim_sail(&s, 0, 1, &town);
	CHECK(stop == SAIL_CREW && s.crew_event == CREW_WRECKED &&
	    s.jail.fresh && s.jail.cause == JAIL_MAROONED);
	for (y = 280; y < 320; y++)
		for (x = 280; x < 320; x++)
			test_world.map[y * WORLD_CELLS_W + x] = 0;
	memset(test_world.map_terrain + 3 * WORLD_TILE * WORLD_TILE,
	    TERRAIN_SEA, WORLD_TILE * WORLD_TILE);
}

/* A cloud over the ship: caught within difficulty x 64, it turns toward
 * the wind every 16 ticks; without clouds nothing turns it. */
static void
test_storm(void)
{
	struct sim s;
	int town, h0, i;

	for (i = 0; i < 2; i++) {
		sea_game(&s, 300, 300, 0x00);
		s.encounters = 0;
		s.fleet.food = 1000;
		s.player.difficulty = 2;	/* caught within 128 */
		s.day_of_year = 0;		/* January: three clouds */
		s.wind = 0x80;
		s.fleet.heading = 0x00;
		sim_voyage_start(&s);
		CHECK(s.storm == 999 && sim_clouds(&s) == 3);
		h0 = s.fleet.heading;
		if (i == 0) {
			/* The clouds kept on the ship. */
			int t, k;

			for (t = 0; t < 33; t++) {
				for (k = 0; k < 3; k++) {
					s.clouds[k].x = s.fleet.x;
					s.clouds[k].y = s.fleet.y;
				}
				sim_sail(&s, 0, 1, &town);
			}
			CHECK(s.storm < 100);
			CHECK(s.fleet.heading != h0);	/* swung by the wind */
		} else {
			sim_sail(&s, 0, 33, &town);
			CHECK(s.fleet.heading == h0);	/* clouds far off */
		}
	}
}

/* Removes every ',"key":<number>' from a JSON text. */
static void
strip_key(char *text, const char *key)
{
	char pat[32], *p, *e;

	snprintf(pat, sizeof(pat), ",\"%s\":", key);
	while ((p = strstr(text, pat)) != NULL) {
		e = p + strlen(pat);
		while (*e == '-' || (*e >= '0' && *e <= '9'))
			e++;
		memmove(p, e, strlen(e) + 1);
	}
}

/* A version 14 state still loads; the town table fills what it lacks. */
static void
test_old_state(void)
{
	static const char *later[] = { "barmaid", "garrison", "land_x",
	    "land_y", "party_x", "party_y", "date" };
	struct era_towns era;
	struct scenario scn;
	struct sim s, t;
	struct jbuf st = { 0 };
	struct json *j;
	char err[128], *v;
	size_t i;

	make_era(&era, &scn);
	era.towns[0].garrison = 7;
	era.towns[0].party_x = 9;		/* a landing cell */
	sim_new_game(&s, &era, &scn, 3);
	CHECK(s.towns[0].garrison == 7 && s.towns[0].party_x == 9);
	sim_to_json(&s, &st);
	for (i = 0; i < sizeof(later) / sizeof(later[0]); i++)
		strip_key(st.s, later[i]);
	/* The current version has two digits, as 14 and 99. */
	CHECK((v = strstr(st.s, "\"version\":")) != NULL &&
	    v[10] >= '1' && v[10] <= '9' && v[11] >= '0' && v[11] <= '9' &&
	    v[12] == ',');
	if (v != NULL)
		memcpy(v, "\"version\":14", 12);
	j = json_parse(st.s, err, sizeof(err));
	CHECK(j != NULL && sim_from_json(&t, j, err, sizeof(err)) == 0);
	CHECK(t.towns[0].garrison == TOWN_UNSET);
	sim_restore_towns(&t, &era);
	CHECK(t.towns[0].garrison == 7 && t.towns[0].party_x == 9);
	/* Undated news is dated back from today. */
	CHECK(t.nnews == s.nnews && sim_news_at(&t, 0)->date ==
	    sim_news_at(&s, 0)->date);
	/* A newer version than the engine knows is refused. */
	if (v != NULL)
		memcpy(v, "\"version\":99", 12);
	json_free(j);
	j = json_parse(st.s, err, sizeof(err));
	CHECK(j != NULL && sim_from_json(&t, j, err, sizeof(err)) != 0);
	json_free(j);
	jb_free(&st);
}

/*
 * Town rules checked against the original: time in town, the visit
 * flags after a battle or the daughter's favour, a fort sinking the
 * last ship, the shipwright's classes, the overfull hold, the tavern's
 * people.
 */
static void
test_town_rules(void)
{
	struct sim s;
	struct daughter d;
	int32_t day;
	int k, won;

	/* Three days a pass through the menu, the month's update with
	 * them (VA 0x18771, 0x1dd0e). */
	sea_game(&s, 300, 300, 0);
	sim_enter_town(&s, 1);
	s.day_of_year = 28;
	s.month = 0;
	day = s.day;
	sim_town_pass(&s);
	CHECK(s.day == day + 3 && s.day_of_year == 31 && s.month == 1);
	CHECK(s.crew_event == CREW_NONE);	/* no word in port */

	/* Through the protocol: getting in is a pass, and each "back". */
	{
		struct session ss;
		struct jbuf o = { 0 };

		memset(&ss, 0, sizeof(ss));
		ss.load = loader;
		ss.load_world = world_cb;
		proto_handle(&ss, "{\"cmd\":\"new_game\",\"era\":1660}", &o);
		jb_free(&o);
		ss.sim.fleet.leaving = 1;
		ss.sim.player_town = -1;
		day = ss.sim.day;
		proto_handle(&ss, "{\"cmd\":\"town\",\"action\":\"enter\"}",
		    &o);
		CHECK(ss.sim.player_town == 1 && ss.sim.day == day + 3);
		jb_free(&o);
		proto_handle(&ss, "{\"cmd\":\"town\",\"action\":\"back\"}", &o);
		CHECK(o.s != NULL && strstr(o.s, "\"ok\":true") != NULL &&
		    ss.sim.day == day + 6);
		jb_free(&o);
		/* The map seller leaves even unpaid: "bought" 0. */
		ss.sim.tavern_spots |= TAVERN_MAPS;
		ss.sim.fleet.gold = 10;
		proto_handle(&ss, "{\"cmd\":\"tavern\",\"action\":\"buy_map\"}",
		    &o);
		CHECK(o.s != NULL && strstr(o.s, "\"ok\":true") != NULL &&
		    strstr(o.s, "\"bought\":0") != NULL &&
		    !(ss.sim.tavern_spots & TAVERN_MAPS));
		jb_free(&o);
		proto_handle(&ss, "{\"cmd\":\"town\",\"action\":\"leave\"}", &o);
		jb_free(&o);
		proto_handle(&ss, "{\"cmd\":\"town\",\"action\":\"back\"}", &o);
		CHECK(o.s != NULL && strstr(o.s, "\"ok\":false") != NULL);
		jb_free(&o);
		session_free(&ss);
	}

	/* Every battle forgets the stay (VA 0xe25a): the governor again. */
	sea_game(&s, 300, 300, 0);
	s.visit_flags = VISIT_GOVERNOR | VISIT_KEPT | VISIT_SNEAKED;
	sim_battle_start(&s);
	CHECK(s.visit_flags == 0);
	s.towns[1].forts = 2;
	s.visit_flags = VISIT_GOVERNOR;
	sim_fort_battle_start(&s, 1);
	CHECK(s.visit_flags == 0);
	/* The last ship sunk by a fort: the town's prison (VA 0xe6b3). */
	s.fleet.nships = 1;
	s.battle.outcome = BAT_PLAYER_SUNK;
	sim_battle_finish(&s, 0);
	CHECK(s.jail.fresh && s.jail.cause == JAIL_FORT && s.jail.town == 1);
	/* At sea it maroons. */
	sea_game(&s, 300, 300, 0);
	sim_battle_start(&s);
	s.battle.outcome = BAT_PLAYER_SUNK;
	sim_battle_finish(&s, 0);
	CHECK(s.jail.fresh && s.jail.cause == JAIL_MAROONED);

	/* The daughter won over reopens the bank (VA 0x12ce5). */
	for (k = 0, won = 0; k < 20 && !won; k++) {
		sea_game(&s, 300, 300, 0);
		rng_seed(&s.rng, (uint64_t)k + 1, 1);
		s.visit_flags = VISIT_KEPT | VISIT_SNEAKED;
		d.town = 0;
		d.charm = 16;
		d.niece = 0;
		d.suitor = 1;
		won = sim_daughter_talk(&s, &d);
		CHECK(!won || s.visit_flags == VISIT_KEPT);
	}
	CHECK(won);

	/* The shipwright's class (VA 0xd1db): type & 7, a galleon as the
	 * fast galleon (8) while the era code is below 4. */
	sea_game(&s, 300, 300, 0);
	sim_enter_town(&s, 1);
	s.fleet.nships = 2;
	s.fleet.ships[0] = 5;
	s.fleet.ships[1] = 9;
	s.fleet.damage[0] = s.fleet.damage[1] = 16;
	CHECK(sim_repair_cost(&s, 0) == 50 * 6 && sim_ship_price(&s, 1) ==
	    50 * 2 * 9);
	test_world.era = 0;			/* 1560 */
	CHECK(sim_repair_cost(&s, 0) == 50 * 9 && sim_ship_price(&s, 0) ==
	    50 * 9 * 9);
	test_world.era = 3;			/* 1640: code 4 */
	CHECK(sim_repair_cost(&s, 0) == 50 * 6);
	test_world.era = 4;

	/* An overfull hold still buys; only exactly full refuses (VA
	 * 0x20c2c). */
	sim_merchant_open(&s);
	s.fleet.gold = 100000;
	s.fleet.capacity = 4;
	s.fleet.guns = 8;
	s.fleet.goods = s.fleet.sugar = 0;
	s.fleet.food = 37;			/* 40 - 4 - 37 = -1 */
	CHECK(sim_hold_space(&s) == -1 && sim_trade(&s, 1, 1) == 1);
	s.fleet.goods = 1;
	s.fleet.food = 35;			/* 40 - 4 - 35 - 1 */
	CHECK(sim_hold_space(&s) == 0 && sim_trade(&s, 1, 1) == 0);

	/* The traveller leaves even unpaid (VA 0x11935); the mystery man
	 * stays while a villain hides here (VA 0x119de). */
	s.tavern_spots = TAVERN_ALL;
	s.fleet.gold = 0;
	s.news_price = 60;
	s.news_town = 2;
	CHECK(sim_buy_news(&s) == -1 && !(s.tavern_spots & TAVERN_NEWS));
	CHECK(sim_buy_map(&s) == 0 && !(s.tavern_spots & TAVERN_MAPS));
	s.villains[0].active = 1;
	s.villains[0].town = 1;
	sim_tavern_stranger(&s);
	CHECK(s.tavern_spots & TAVERN_MYSTERY);
	s.villains[0].active = 0;
	sim_tavern_stranger(&s);
	CHECK(!(s.tavern_spots & TAVERN_MYSTERY));
}

int
main(void)
{
	test_rng();
	test_json();
	test_rules();
	test_protocol();
	test_snapshot();
	test_hof();
	test_sailing();
	test_sail_snapshot();
	test_encounters();
	test_encounter_steps();
	test_battles();
	test_battle_protocol();
	test_sword();
	test_sword_protocol();
	test_towns();
	test_shipwright();
	test_merchant();
	test_tavern();
	test_governor();
	test_bank();
	test_cannon();
	test_arrival();
	test_command();
	test_coast();
	test_screens();
	test_fort_battle();
	test_fort_protocol();
	test_leave_heading();
	test_assault();
	test_tavern_people();
	test_landing();
	test_land_battle();
	test_old_state();
	test_treasure();
	test_villains();
	test_missions();
	test_crew_mood();
	test_prison();
	test_prisoners();
	test_convoys();
	test_daughter();
	test_merchant_daughter();
	test_villain_moves();
	test_career_end();
	test_crew_check();
	test_prize_place();
	test_hunted_pirate();
	test_battle_near_town();
	test_personal_log();
	test_month_housekeeping();
	test_capture_plunder();
	test_fleet_calendar();
	test_food();
	test_reef();
	test_storm();
	test_town_rules();
	if (failures > 0) {
		fprintf(stderr, "%d check(s) failed\n", failures);
		return (1);
	}
	printf("all simulation tests passed\n");
	return (0);
}
