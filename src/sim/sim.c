/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026, Olivier Cochard-Labbé
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "internal.h"

#define DAYS_PER_MONTH		30
#define DAYS_PER_YEAR		360
#define CONQUEST_RANGE		150	/* Manhattan distance, map cells */
#define DIPLOMACY_EVENTS	28	/* draws below this change relations */
#define EVENT_DRAW		64	/* town event draw; >= towns: none */
#define GOVERNOR_FIXED		0x40	/* field 11: governor never replaced */
#define FLAG_KNOWN		0x80	/* the player has current information */
#define FLAG_OBSOLETE		0x20	/* an event made it out of date */
#define STANDING_FLOOR		(-3)	/* after an amnesty */
#define TOWN_DAYS		3	/* a pass through the town's menu */
#define WIND_TICKS		128	/* the wind may shift this often, */
					/* in ticks of a 64-tick day */
#define WIND_START		0xC0	/* blowing west: the trade winds */
#define SPEED_MIN		4
#define SPEED_MAX		12
#define EDGE_MIN		0x41	/* playable world, in pixels */
#define EDGE_MAX_X		0x27bf
#define EDGE_MAX_Y		0x1dbf
#define START_CELLS		4	/* the start town is within */
#define SIGHTING_TICKS		32	/* a sail may appear this often, */
					/* in ticks, whatever the day */
#define HEAT_IGNORED		16	/* added when an encounter is left */
#define FAR_FROM_TOWNS		12	/* cells: ships from any nation */

enum town_event {
	EV_INDIANS,
	EV_GOLD_MINE,
	EV_MALARIA,
	EV_GOVERNOR,
	EV_PLUNDER,
	EV_COUNT
};

#define log_news	sim_log_news

void
sim_log_news(struct sim *s, int code, int a, int b)
{
	struct sim_news *n = &s->news[s->nnews % SIM_NEWS_MAX];

	n->day = s->day;
	n->date = s->year * DAYS_PER_YEAR + s->day_of_year;
	n->code = code;
	n->a = a;
	n->b = b;
	s->nnews++;
}

size_t
sim_news_kept(const struct sim *s)
{
	return (s->nnews < SIM_NEWS_MAX ? s->nnews : SIM_NEWS_MAX);
}

const struct sim_news *
sim_news_at(const struct sim *s, size_t i)
{
	size_t first;

	if (i >= sim_news_kept(s))
		return (NULL);
	first = s->nnews - sim_news_kept(s);
	return (&s->news[(first + i) % SIM_NEWS_MAX]);
}

/* The town the fleet starts in: the nearest by the era's arrival cells
 * (VA 0x1386c), when its own cell is within "reach" cells; else -1. */
static int
start_town(const struct sim *s, const struct era_towns *era, int reach)
{
	int cx = s->fleet.x / WORLD_TILE, cy = s->fleet.y / WORLD_TILE, t;

	t = town_cells_nearest(era->arrival, era->narrival, cx, cy);
	if (t < 0 || (size_t)t >= s->ntowns ||
	    abs(s->towns[t].x - cx) > reach || abs(s->towns[t].y - cy) > reach)
		return (-1);
	return (t);
}

/*
 * Out of a town onto its water (VA 0x19b9e): the cell of fields 26 and
 * 27, heading field 28 x 32, which points away from the town.
 */
void
sim_harbour_mouth(struct sim *s, int town)
{
	const struct sim_town *t = &s->towns[town];

	if (t->sea_x == 0)
		return;
	s->fleet.x = t->sea_x * WORLD_TILE + 8;
	s->fleet.y = t->sea_y * WORLD_TILE + 8;
	s->fleet.heading = (uint8_t)((t->sea_heading & 7) * 32);
	s->fleet.speed = 0;
}

/*
 * The career starts on day rand(150) + 65 of the era's year (VA
 * 0x132f8), and its log already holds the news of the year so far (VA
 * 0x13679): for each pair of nations, at war or allied, that; at peace,
 * a random town event (codes 11-15).  Six entries, a sixth of the time
 * since New Year apart, the first half a step in.
 */
void
sim_restore_towns(struct sim *s, const struct era_towns *era)
{
	struct sim_town *st;
	const struct town *t;
	size_t i;

	/* The convoys' routes are the era's. */
	for (i = 0; i < CONVOY_STOPS; i++) {
		s->fleet_route[i] = era->fleet_route[i];
		s->train_route[i] = era->train_route[i];
	}
	for (i = 0; i < s->ntowns && i < era->count; i++) {
		st = &s->towns[i];
		t = &era->towns[i];
		if (st->barmaid == TOWN_UNSET)
			st->barmaid = (uint16_t)t->barmaid;
		if (st->garrison == TOWN_UNSET)
			st->garrison = (uint16_t)t->garrison;
		if (st->land_x == TOWN_UNSET)
			st->land_x = (uint16_t)t->land_x;
		if (st->land_y == TOWN_UNSET)
			st->land_y = (uint16_t)t->land_y;
		if (st->party_x == TOWN_UNSET)
			st->party_x = (uint16_t)t->party_x;
		if (st->party_y == TOWN_UNSET)
			st->party_y = (uint16_t)t->party_y;
	}
}

static void
start_log(struct sim *s)
{
	int step, date, i, j, r;
	struct sim_news *n;

	s->day_of_year = rng_below(&s->rng, 150) + 65;
	s->month = s->day_of_year / DAYS_PER_MONTH;
	step = s->day_of_year / 6;
	date = step / 2;
	for (i = 0; i < 3; i++)
		for (j = i + 1; j < 4; j++) {
			n = &s->news[s->nnews % SIM_NEWS_MAX];
			r = s->relations[i][j];
			if (r == 0) {
				n->code = NEWS_INDIANS + rng_below(&s->rng, 5);
				n->a = rng_below(&s->rng, (int)s->ntowns);
				n->b = 0;
			} else {
				n->code = NEWS_PEACE + r;	/* war, alliance */
				n->a = i;
				n->b = j;
			}
			n->date = s->year * DAYS_PER_YEAR + date;
			n->day = date - s->day_of_year;
			s->nnews++;
			date += step;
		}
}

static void	convoys_start(struct sim *, const struct era_towns *);

void
sim_new_game(struct sim *s, const struct era_towns *era,
    const struct scenario *scn, uint64_t seed)
{
	const struct town *t;
	size_t i;

	memset(s, 0, sizeof(*s));
	rng_seed(&s->rng, seed, 0x5354414c4f554fULL);
	s->era_year = era->year;
	s->year = era->year;
	s->villain_duel = -1;
	s->storm = 9999;		/* no cloud near */
	s->ransom_slot = -1;
	s->player_town = SIM_NO_TOWN;
	s->visit_town = SIM_NO_TOWN;
	s->wright_gold = -1;
	s->merchant_gold = -1;
	s->player.expedition = -1;
	snprintf(s->player.title, sizeof(s->player.title), "%s", scn->name);
	memcpy(s->relations, scn->relations, sizeof(s->relations));
	s->fleet.nships = scn->nships > 0 ? scn->nships : 1;
	memcpy(s->fleet.ships, scn->ships, sizeof(s->fleet.ships));
	s->fleet.crew = scn->crew;
	s->fleet.guns = scn->guns;
	s->fleet.gold = scn->gold;
	s->fleet.food = scn->food;
	s->fleet.food_frac = scn->food_frac;
	s->fleet.goods = scn->goods;
	s->fleet.sugar = scn->sugar;
	s->fleet.capacity = scn->capacity;
	{
		int k = rng_below(&s->rng, SCENARIO_STARTS);

		s->fleet.x = scn->start[k][0] * WORLD_TILE;
		s->fleet.y = scn->start[k][1] * WORLD_TILE;
	}
	s->fleet.leaving = -1;
	s->wind = WIND_START;
	/* The start's standings and ranks; pirates always hostile; a
	 * fortune of 200 (VA 0x1386c). */
	memcpy(s->standing, scn->standing, sizeof(s->standing));
	memcpy(s->rank, scn->rank, sizeof(s->rank));
	s->standing[NATION_PIRATE] = -1;
	s->fortune = 200;
	/* The surnames 0-3 start as used, so the first captain named is
	 * surname 4 (VA 0x32e06 holds 15 in the program's data). */
	s->surnames = 0x0F;
	s->encounters = 1;
	s->heat_town = -1;
	s->ntowns = era->count;
	for (i = 0; i < era->count; i++) {
		t = &era->towns[i];
		memcpy(s->towns[i].name, t->name, sizeof(s->towns[i].name));
		s->towns[i].flags = (uint16_t)t->flags;
		s->towns[i].x = (uint16_t)t->x;
		s->towns[i].y = (uint16_t)t->y;
		s->towns[i].nation = (uint16_t)t->nation;
		s->towns[i].forts = (uint16_t)t->forts;
		s->towns[i].soldiers = (uint16_t)t->soldiers;
		s->towns[i].citizens = (uint16_t)t->citizens;
		s->towns[i].gold = (uint16_t)t->gold;
		s->towns[i].wealth = (uint16_t)t->wealth;
		s->towns[i].standing = (uint16_t)t->standing;
		s->towns[i].governor = (uint16_t)t->governor;
		s->towns[i].family = (uint16_t)t->family;
		s->towns[i].id = (uint16_t)t->id;
		s->towns[i].fort_x = (uint16_t)t->fort_x;
		s->towns[i].fort_y = (uint16_t)t->fort_y;
		s->towns[i].ship_x = (uint16_t)t->ship_x;
		s->towns[i].ship_y = (uint16_t)t->ship_y;
		s->towns[i].ship_heading = (uint16_t)t->ship_heading;
		s->towns[i].sea_x = (uint16_t)t->sea_x;
		s->towns[i].sea_y = (uint16_t)t->sea_y;
		s->towns[i].sea_heading = (uint16_t)t->sea_heading;
		s->towns[i].barmaid = TOWN_UNSET;
		s->towns[i].garrison = TOWN_UNSET;
		s->towns[i].land_x = TOWN_UNSET;
		s->towns[i].land_y = TOWN_UNSET;
		s->towns[i].party_x = TOWN_UNSET;
		s->towns[i].party_y = TOWN_UNSET;
	}
	sim_restore_towns(s, era);
	/* The start position is in a town; the voyage starts on the water
	 * outside its harbour. */
	s->fleet.leaving = start_town(s, era, START_CELLS);
	if (s->fleet.leaving >= 0)
		sim_harbour_mouth(s, s->fleet.leaving);
	start_log(s);
	/* A famous expedition starts on its own date (VA 0x15bc6): the
	 * years past the era's first, and the day (VA 0x13679). */
	if (scn->date > 0) {
		s->year = s->era_year + scn->date / DAYS_PER_YEAR;
		s->day_of_year = scn->date % DAYS_PER_YEAR;
		s->month = s->day_of_year / DAYS_PER_MONTH;
	}
	convoys_start(s, era);
	/* The expeditions start months at sea. */
	s->months = scn->months;
	s->start_year = s->year;
	s->fleet.mood = sim_crew_mood(s);
}

static void
new_governor(struct sim *s, struct sim_town *t)
{
	t->standing = 0;
	if (!(t->governor & GOVERNOR_FIXED))
		t->governor = (uint16_t)rng_below(&s->rng, 16);
}

static void
town_growth(struct sim *s)
{
	struct sim_town *t;
	size_t i;
	int r, income, drift;

	for (i = 0; i < s->ntowns; i++) {
		t = &s->towns[i];
		if (rng_below(&s->rng, 4) != 0)
			continue;
		t->wealth = (uint16_t)(t->wealth +
		    (rng_below(&s->rng, 255 - t->wealth) >> 5));
		r = rng_below(&s->rng, t->wealth);
		t->citizens = (uint16_t)(t->citizens +
		    (((uint32_t)s->month * 24 + (uint32_t)r) >> 8 & 0xFF));
		income = (uint16_t)((uint32_t)r * t->citizens) >> 9;
		if (t->nation == NATION_SPAIN)
			income >>= 1;
		t->gold = (uint16_t)(t->gold + income);
		drift = (int16_t)((income + t->citizens) / 2 - t->soldiers);
		if (drift >= 1 && t->soldiers <= 254)
			t->soldiers++;
		else if (drift < 0 && t->soldiers > 0)
			t->soldiers--;
	}
}

/* Draw a town, avoiding the player's town when there is a choice. */
static int
draw_target(struct sim *s)
{
	int b;

	/* Never the town whose governor gave the mission (VA 0x471ac). */
	do
		b = rng_below(&s->rng, (int)s->ntowns);
	while (s->mission.type != 0 && b == s->mission.giver &&
	    s->ntowns > 1);
	return (b);
}

/* The game's current town (VA 0x46058): the player's, or at sea the
 * nearest. */
static int
current_town(const struct sim *s)
{
	int dist;

	return (s->player_town >= 0 ? s->player_town :
	    sim_nearest_town(s, &dist));
}

static void
conquest(struct sim *s)
{
	struct sim_town *a, *b;
	int ia, ib, dist;

	if (s->ntowns == 0)
		return;
	ia = rng_below(&s->rng, (int)s->ntowns);
	ib = draw_target(s);
	a = &s->towns[ia];
	b = &s->towns[ib];
	if (s->relations[a->nation & 3][b->nation & 3] != -1 ||
	    (uint32_t)b->soldiers * 2 >= a->soldiers)
		return;
	dist = abs((int)a->x - (int)b->x) + abs((int)a->y - (int)b->y);
	if (dist >= CONQUEST_RANGE || ib == current_town(s))
		return;
	b->nation = a->nation;
	new_governor(s, b);
	log_news(s, NEWS_CAPTURE, a->nation, ib);
}

static void
diplomacy(struct sim *s)
{
	int r, a, b, step, rel;

	/* The era's range (VA 0x4707e), unless overridden. */
	r = rng_below(&s->rng, s->diplomacy_range > 0 ? s->diplomacy_range :
	    s->world != NULL && s->world->diplomacy_range > 0 ?
	    s->world->diplomacy_range : SIM_DIPLOMACY_RANGE);
	if (r >= DIPLOMACY_EVENTS)
		return;
	a = r & 3;
	b = r >> 2 & 3;
	step = (r & 16) ? 1 : -1;
	if (a == b || s->relations[a][b] == step)
		return;
	rel = s->relations[a][b] + step;
	s->relations[a][b] = s->relations[b][a] = (int8_t)rel;
	if (rel == 0 && step == -1)
		return;		/* an alliance lapsing into peace is silent */
	log_news(s, NEWS_PEACE + rel, a, b);
	if (rel > 0) {
		log_news(s, NEWS_AMNESTY, a, 0);
		if (s->standing[a] < STANDING_FLOOR)
			s->standing[a] = STANDING_FLOOR;
	}
}

static void
town_event(struct sim *s)
{
	struct sim_town *t;
	int it, e;

	it = rng_below(&s->rng, EVENT_DRAW);
	if (it >= (int)s->ntowns)
		return;
	t = &s->towns[it];
	do
		e = rng_below(&s->rng, EV_COUNT);
	while (e == EV_GOVERNOR && s->mission.type != 0 &&
	    it == s->mission.giver);
	log_news(s, NEWS_INDIANS + e, it, 0);
	if (t->flags & FLAG_KNOWN)
		t->flags |= FLAG_OBSOLETE;
	else
		t->flags &= (uint16_t)~FLAG_OBSOLETE;
	t->flags &= (uint16_t)~FLAG_KNOWN;
	switch (e) {
	case EV_INDIANS:
		t->soldiers >>= 1;
		break;
	case EV_GOLD_MINE:
		t->wealth = t->wealth + 64 > 254 ? 255 : t->wealth + 64;
		t->gold = t->gold + 12 > 254 ? 255 : t->gold + 12;
		break;
	case EV_MALARIA:
		t->soldiers >>= 1;
		t->citizens >>= 1;
		break;
	case EV_GOVERNOR:
		new_governor(s, t);
		break;
	case EV_PLUNDER:
		t->gold = 0;
		t->wealth = (uint16_t)(t->wealth - (t->wealth >> 2));
		break;
	}
}

/*
 * The game's era code (VA 0x45e8c), by the era's year: the era menu's
 * choice, plus 1 past the first, so 1560 is 0 and 1600 to 1680 are 2 to
 * 6 (the year is 1560 + code x 20, VA 0xc46e).
 */
static int
era_code(const struct sim *s)
{
	int e;

	for (e = 0; e < EXE_ERAS; e++)
		if (town_era_years[e] == s->era_year)
			return (e == 0 ? 0 : e + 1);
	return (0);
}

/* A convoy's month (VA 0xcde9): 1 or 2 stops on, past the towns not
 * Spanish. */
static void
convoy_move(struct sim *s, int *step, int *town, const int *route, int from,
    int fleet)
{
	struct sim_town *t;
	int st, k;

	for (;;) {
		st = from + 1 + rng_below(&s->rng, 2);
		*step = st;
		if (st >= CONVOY_STOPS) {
			k = -1;
			break;
		}
		k = route[st];
		if (k < 0 || (size_t)k >= s->ntowns) {
			k = -1;
			break;
		}
		if (s->towns[k].nation == NATION_SPAIN)
			break;
		from++;
	}
	if (k >= 0) {
		t = &s->towns[k];
		t->wealth = t->wealth < 245 ? (uint16_t)(t->wealth + 10) : 255;
		if (fleet)
			t->gold = (uint16_t)(t->gold / 4);
	}
	*town = k;
}

static void
convoys_move(struct sim *s)
{
	int odd = era_code(s) & 1;

	convoy_move(s, &s->fleet_step, &s->fleet_town, s->fleet_route,
	    s->month == 9 - odd ? -1 : s->fleet_step, 1);
	convoy_move(s, &s->train_step, &s->train_town, s->train_route,
	    s->month == 3 - odd ? -1 : s->train_step, 0);
}

/* Where the convoys are at the start (VA 0x13679): by the date. */
static void
convoys_start(struct sim *s, const struct era_towns *era)
{
	int odd = era_code(s) & 1, k;

	for (k = 0; k < CONVOY_STOPS; k++) {
		s->fleet_route[k] = era->fleet_route[k];
		s->train_route[k] = era->train_route[k];
	}
	s->fleet_step = odd * 2 + s->day_of_year / 15 - 18;
	if (s->fleet_step < 0)
		s->fleet_step += 24;
	s->fleet_town = s->fleet_step < CONVOY_STOPS ?
	    s->fleet_route[s->fleet_step] : -1;
	s->train_step = odd * 2 + s->day_of_year / 15 - 6;
	if (s->train_step < 0)
		s->train_step += 24;
	s->train_town = s->train_step < CONVOY_STOPS ?
	    s->train_route[s->train_step] : -1;
}

int
sim_convoys_at(const struct sim *s, int town)
{
	if (town < 0)
		return (0);
	return ((s->fleet_town == town ? CONVOY_FLEET : 0) |
	    (s->train_town == town ? CONVOY_TRAIN : 0));
}

void
sim_attack_begin(struct sim *s, int town)
{
	struct sim_town *t;
	int n;

	if (town < 0 || (size_t)town >= s->ntowns)
		return;
	t = &s->towns[town];
	n = t->soldiers;
	if (s->train_town == town)
		n += 10;
	if (s->fleet_town == town)
		n += 10;
	t->soldiers = (uint16_t)(n > 254 ? 255 : n);
}

/*
 * The player's side of the month (VA 0xceb8, before the world's): the
 * land yields half its count to the fortune, the reputation fades by an
 * eighth (rounded down), and the standings stay within -100..100.
 */
static void
month_housekeeping(struct sim *s)
{
	int n, r = s->reputation;

	s->fortune += s->acres / 2;
	s->reputation = r - (r >= 0 ? r / 8 : -((-r + 7) / 8));
	for (n = 0; n < 4; n++)
		if (s->standing[n] > 100)
			s->standing[n] = 100;
		else if (s->standing[n] < -100)
			s->standing[n] = -100;
}

void
sim_world_update(struct sim *s)
{
	month_housekeeping(s);
	town_growth(s);
	conquest(s);
	diplomacy(s);
	town_event(s);
	sim_villains_move(s);			/* VA 0xcba7 */
	convoys_move(s);
}

/*
 * One step of at most a month, as the game handles time: each month
 * boundary crossed runs a world update with the new month number (which
 * reaches 12 at the year end), then the day of the year wraps once.
 * News is dated at the boundary that produced it.
 */
static void
advance_step(struct sim *s, int days)
{
	int32_t start_day = s->day;
	int start_doy = s->day_of_year, m, months = 0;

	s->day_of_year += days;
	for (m = start_doy / DAYS_PER_MONTH + 1;
	    m <= s->day_of_year / DAYS_PER_MONTH; m++) {
		s->month = m;
		s->day = start_day + (m * DAYS_PER_MONTH - start_doy);
		/* A month longer since the plunder was divided, two with
		 * a fortune aboard (VA 0xceb8). */
		s->months = (int8_t)(s->months + (s->fleet.gold > 0x7FFF ?
		    2 : 1));
		sim_world_update(s);
		s->fleet.mood = sim_crew_mood(s);
		months++;
	}
	/* At sea (not in town nor ashore), the crew speaks (VA 0x1dd0e). */
	if (months > 0 && s->player_town < 0 && !s->fleet.ashore &&
	    s->fleet.crew != 1 && s->crew_event == CREW_NONE) {
		if (s->fleet.mood < -3) {
			s->crew_event = CREW_MUTINY;
			sim_crew_leave(s, s->fleet.crew / 3);
		} else if (s->fleet.mood < 0)
			s->crew_event = s->months < 12 ? CREW_RESTLESS :
			    CREW_ANXIOUS;
	}
	s->day = start_day + days;
	if (s->day_of_year >= DAYS_PER_YEAR) {
		s->day_of_year -= DAYS_PER_YEAR;
		s->year++;
	}
	s->month = s->day_of_year / DAYS_PER_MONTH;
}

void
sim_advance(struct sim *s, int32_t days)
{
	int step;

	while (days > 0) {
		step = days > DAYS_PER_MONTH ? DAYS_PER_MONTH : (int)days;
		advance_step(s, step);
		days -= step;
	}
}

static int	sighting(struct sim *s);

/* Signed sine of a heading, from the game's half-circle table. */
static int
sine(const struct world *w, int heading)
{
	heading &= 0xFF;
	return (heading < 0x80 ? w->sine[heading] : -w->sine[heading - 0x80]);
}

/* Division by 256 rounding down, also for negative values. */
static int
div256(int v)
{
	return (v >= 0 ? v / 256 : -((-v + 255) / 256));
}

int
sim_sine_mul(const struct world *w, int heading, int v)
{
	return (div256(sine(w, heading) * v));
}

int
sim_div16(int a, int b)
{
	if (a == 0 || b == 0 || a < b)
		return (0);
	return (a / b);
}

/*
 * The game's bearing: the angle of the smaller axis over the larger one
 * from a polynomial approximation of the arc tangent, in 1/65536 turn,
 * placed in its octant, then scaled to 256 per turn.
 */
int
sim_bearing(int dx, int dy)
{
	int p1 = dx, p2 = -dy;	/* east and north components */
	int a, b, swap, ratio, t;
	unsigned ang;

	if (p1 == 0)
		return (p2 < 1 ? 0x80 : 0);
	if (p2 == 0)
		return (p1 < 1 ? 0xC0 : 0x40);
	a = abs(p1);
	b = abs(p2);
	swap = b < a;		/* the ratio is then north over east */
	ratio = swap ? (b << 14) / a : (a << 14) / b;
	t = abs(ratio - 0x1333);
	ang = (unsigned)(((0x2800 - (t * 0xb00 >> 14)) * ratio) >> 14) &
	    0xFFFF;
	if (p1 < 1) {
		if (p2 < 1)
			ang = swap ? 0xC000 - ang : ang + 0x8000;
		else
			ang = swap ? ang + 0xC000 : 0x10000 - ang;
	} else {
		if (p2 < 1)
			ang = swap ? ang + 0x4000 : 0x8000 - ang;
		else
			ang = swap ? 0x4000 - ang : ang;
	}
	return ((int)((ang & 0xFFFF) >> 8));
}

static int
ship_class(const struct sim_fleet *f)
{
	int i, c = 0;

	for (i = 0; i < f->nships && i < SCENARIO_SHIPS; i++)
		if ((f->ships[i] & 7) > c)
			c = f->ships[i] & 7;
	return (c);
}

/* Speed the ship heads for on its course, before the 4..12 clamp. */
static int
target_speed(const struct sim *s)
{
	int sector, d, bonus;

	sector = ((s->fleet.heading + 8 - s->wind) >> 4) & 15;
	d = s->player.difficulty - (s->player.ability == ABILITY_NAVIGATION);
	d = d < 0 ? 0 : d > 4 ? 4 : d;
	bonus = (5 - d) >> 1;
	return ((s->world->speed[ship_class(&s->fleet)][sector] + bonus) * 3);
}

/* How near a cloud catches the ship: the navigator's difficulty x 64. */
static int
storm_reach(const struct sim *s)
{
	int d = s->player.difficulty - (s->player.ability == ABILITY_NAVIGATION);

	return ((d < 0 ? 0 : d > 4 ? 4 : d) * 64);
}

static void
shift_wind(struct sim *s)
{
	int w = s->wind >> 4, r = rng_below(&s->rng, 8);

	/* An arithmetic shift of a small signed value: floor(x / 2). */
	s->wind = (uint8_t)(s->wind + 16 * ((r - w + 10 >= 0) ?
	    (r - w + 10) / 2 : -((-(r - w + 10) + 1) / 2)));
}

static int	shore_ahead(const struct sim *s);
static void	clear_heading(struct sim *s, int x, int y);

/*
 * Town whose arrival cell holds world pixel (x, y) (VA 0x29dd9), or -1.
 * The town just left (or any, as the game ignores them all then) counts
 * once the fleet stands on no arrival cell.
 */
static int
town_at(struct sim *s, int x, int y)
{
	int t;

	t = world_arrival(s->world, x / WORLD_TILE, y / WORLD_TILE);
	if (t < 0 || (size_t)t >= s->ntowns) {
		s->fleet.leaving = -1;
		return (-1);
	}
	return (s->fleet.leaving >= 0 ? -1 : t);
}

/*
 * In port by sea.  The ship waits on the water; facing the shore, it
 * turns away from the town to open water, so sailing on from the
 * arrival does not run it aground at once (an engine choice).
 */
static enum sail_stop
arrive(struct sim *s, int t, int *town)
{
	s->fleet.speed = 0;
	if (shore_ahead(s))
		clear_heading(s, s->towns[t].x * WORLD_TILE + 8,
		    s->towns[t].y * WORLD_TILE + 8);
	s->fleet.leaving = t;
	*town = t;
	return (SAIL_TOWN);
}

/* Ticks a day (data/calendar), the original's when no world says. */
static int
day_ticks(const struct sim *s)
{
	return (s->world != NULL && s->world->ticks_per_day > 0 ?
	    s->world->ticks_per_day : WORLD_TICKS_PER_DAY);
}

/*
 * A period of "ticks" in a day of the original's length, in this world's
 * ticks: a longer day stretches the voyages, not how often things happen
 * in a day.
 */
static int
per_day(const struct sim *s, int ticks)
{
	int t = ticks * day_ticks(s) / WORLD_TICKS_PER_DAY;

	return (t > 0 ? t : 1);
}

/*
 * The crew eats (VA 0x231b8), two passes before each day's: food x 256
 * + fraction loses a 256th a man, floored at 0; the days left are that
 * over the crew (at most 32767).  Under 21 days, on a multiple of 4,
 * the master warns (CSTR 124); at 0 the crew starves (VA 0x2849c): a
 * third desert with their share of the gold, food becomes 5 (CSTR 180,
 * 181 on foot).  Returns 1 when that crew event is raised.
 */
static int
eat(struct sim *s)
{
	struct sim_fleet *f = &s->fleet;
	int32_t v;
	int days;

	if ((f->ticks + 2) % day_ticks(s) != 0 || f->crew <= 0 ||
	    f->nships <= 0)
		return (0);
	v = (int32_t)f->food * 256 + f->food_frac - f->crew;
	if (v < 0)
		v = 0;
	f->food = (int)(v >> 8);
	f->food_frac = (int)(v & 0xFF);
	days = v / f->crew > 0x7FFF ? 0x7FFF : (int)(v / f->crew);
	if (days >= 21 || (days & 3) != 0 || s->crew_event != CREW_NONE)
		return (0);
	if (days == 0) {
		f->food = 5;
		sim_crew_leave(s, f->crew / 3);
		s->crew_event = CREW_STARVING;
	} else {
		s->food_days = days;
		s->crew_event = CREW_HUNGRY;
	}
	return (1);
}

/*
 * Reefs (VA 0x231b8, 0x2849c): on the shallows, with a ship bigger
 * than a sloop in the fleet and the 100-pass wait over, the last such
 * ship grinds onto a reef and is lost with her share of the cargo
 * (CSTR 182); the crew is cut to what the rest can carry.  Losing the
 * last ship maroons the player.  The original sizes the fleet once a
 * voyage from every other slot; the engine looks at every ship.
 */
static int
reef(struct sim *s, int terrain)
{
	struct sim_fleet *f = &s->fleet;
	int k, victim = -1, room;

	if (f->reef_wait > 0)
		f->reef_wait--;
	if (terrain != TERRAIN_SHALLOWS || f->reef_wait > 0 ||
	    s->crew_event != CREW_NONE)
		return (0);
	for (k = 0; k < f->nships; k++)
		if ((f->ships[k] & 7) > 1)
			victim = k;
	if (victim < 0)
		return (0);
	f->reef_wait = 100;
	/* The last ship: marooned, as after a sea battle (VA 0x1f2a1),
	 * once the master's word is heard. */
	if (f->nships == 1) {
		sim_imprison(s, JAIL_MAROONED, -1);
		s->crew_event = CREW_WRECKED;
		return (1);
	}
	sim_drop_ship(s, victim, 0);
	for (k = 0, room = 0; k < f->nships; k++)
		room += ((f->ships[k] & 0xF) + 2) * 32;
	if (f->crew > room)
		f->crew = room;
	s->crew_event = CREW_WRECKED;
	return (1);
}

/*
 * The view (VA 0x231b8): tile aligned, it jumps when the fleet nears an
 * edge, looking ahead the way it heads.
 */
#define VIEW_W		640
#define VIEW_H		480
#define VIEW_EDGE	50
#define VIEW_RIGHT	570
#define VIEW_BOTTOM	432
#define VIEW_AHEAD_X	192
#define VIEW_AHEAD_Y	128
#define VIEW_MAX_X	(600 * WORLD_TILE)
#define VIEW_MAX_Y	(450 * WORLD_TILE)
#define STORM_NONE	9999
#define STORM_NEAR	100	/* nearer, the wind blows twice as hard */

static int
clampi(int v, int lo, int hi)
{
	return (v < lo ? lo : v > hi ? hi : v);
}

/* sine x v / 256, the sign put back after (VA 0x297c4's drift). */
static int
drift(const struct sim *s, int h, int v)
{
	int a = s->world->sine[h & 0x7F] * v >> 8;

	return ((h & 0x80) ? -a : a);
}

static int
lookahead(int angle, int amount)
{
	int a = (int)(signed char)(uint8_t)angle;

	if (a > 0x60 || a < -0x60 || (a > -0x20 && a < 0x20))
		return (0);
	return (a > 0 ? amount : -amount);
}

static void
follow(struct sim *s, int force)
{
	const struct sim_fleet *f = &s->fleet;
	int sx = f->x - s->view_x, sy = f->y - s->view_y;

	if (force || sx > VIEW_RIGHT || sx < VIEW_EDGE) {
		s->view_x = (f->x - VIEW_W / 2 + lookahead(f->heading,
		    VIEW_AHEAD_X)) / WORLD_TILE * WORLD_TILE;
		s->view_x = clampi(s->view_x, 0, VIEW_MAX_X);
	}
	if (force || sy > VIEW_BOTTOM || sy < VIEW_EDGE) {
		s->view_y = (f->y - VIEW_H / 2 + lookahead(f->heading - 0x40,
		    VIEW_AHEAD_Y)) / WORLD_TILE * WORLD_TILE;
		s->view_y = clampi(s->view_y, 0, VIEW_MAX_Y);
	}
}

int
sim_clouds(const struct sim *s)
{
	int m = s->day_of_year / 30 - 5;

	return (((m < 0 ? -m : m) + 1) / 2);
}

void
sim_voyage_start(struct sim *s)
{
	static const int frames[4] = { 0, 1, 2, 0 };
	int i;

	follow(s, 1);
	for (i = 0; i < 3; i++) {
		s->clouds[i].frame = frames[rng_below(&s->rng, 4)];
		s->clouds[i].x = s->clouds[i].y = 0;	/* off the view */
		s->clouds[i].shown = 0;
	}
	s->storm = 999;
}

/*
 * The clouds' move (VA 0x231b8): cloud i drifts with the wind by (4i +
 * 6) x 2 eighths; out of -100..640 across or -40..480 down from the view
 * it comes back at the right edge at a random height.  The storm is the
 * nearest one's distance, max(|dx|, |dy|) + min / 2.
 */
static void
clouds_move(struct sim *s)
{
	struct sim_cloud *c;
	int i, n = sim_clouds(s), sx, sy, dx, dy;

	s->storm = STORM_NONE;
	for (i = 0; i < n && i < 3; i++) {
		c = &s->clouds[i];
		c->x += drift(s, s->wind, (4 * i + 6) * 2) >> 3;
		c->y -= drift(s, s->wind + 0x40, (4 * i + 6) * 2) >> 3;
		sx = c->x - s->view_x;
		sy = c->y - s->view_y;
		c->shown = !(sx < -100 || sx > VIEW_W || sy < -40 ||
		    sy > VIEW_H);
		if (!c->shown) {
			c->x = s->view_x + VIEW_W;
			c->y = s->view_y + rng_below(&s->rng, 440);
		}
		dx = abs(s->fleet.x - c->x);
		dy = abs(s->fleet.y - c->y);
		dx = dx > dy ? dx + dy / 2 : dy + dx / 2;
		if (dx < s->storm)
			s->storm = dx;
	}
}

enum sail_stop
sim_sail(struct sim *s, int turn, int ticks, int *town)
{
	struct sim_fleet *f = &s->fleet;
	int32_t x8, y8, nx, ny;
	int target, step, t;

	*town = -1;
	if (s->world == NULL)
		return (SAIL_TICKS);
	if (s->encounter.stage != ENC_NONE || s->battle.active)
		return (SAIL_ENCOUNTER);	/* answer or fight it first */
	turn = turn < 0 ? -1 : turn > 0 ? 1 : 0;
	while (ticks-- > 0) {
		f->ticks++;
		/*
		 * Steering.  The original turns by 16 on the 4 ticks of
		 * every 8 where bit 2 of the counter is clear, so a press
		 * could wait 4 ticks.  The engine opens that 8-tick window
		 * at the press: 4 ticks turning, 4 not.  A press while it
		 * is closed is kept and turns when the next one opens.  The
		 * ship turns no faster than in the original.
		 */
		if (turn == 0)
			f->turn_held = 0;
		else if (turn != f->turn_held)
			f->turn_held = f->turn_owed = turn;
		if (f->turn_wait == 0 && (f->turn_owed != 0 ||
		    f->turn_held != 0))
			f->turn_wait = 8;
		if (f->turn_wait > 4 && (f->turn_owed != 0 ||
		    f->turn_held != 0)) {
			f->heading = (uint8_t)(f->heading + 16 *
			    (f->turn_owed != 0 ? f->turn_owed : f->turn_held));
			f->turn_owed = 0;
		}
		if (f->turn_wait > 0)
			f->turn_wait--;
		target = target_speed(s);
		/* Near a cloud the wind blows twice as hard; caught in it
		 * (within difficulty x 64), the ship is turned toward the
		 * wind every 16 ticks at half speed (VA 0x231b8). */
		if (s->storm < STORM_NEAR)
			target *= 2;
		if (s->storm < storm_reach(s)) {
			if ((f->ticks & 15) == 0 && f->heading != s->wind)
				f->heading = (uint8_t)(f->heading + 16 *
				    ((int8_t)(s->wind - f->heading) < 0 ?
				    -1 : 1));
			target >>= 1;
		}
		step = target > f->speed ? 2 : target < f->speed ? -2 : 0;
		f->speed += step;
		f->speed = f->speed < SPEED_MIN ? SPEED_MIN :
		    f->speed > SPEED_MAX ? SPEED_MAX : f->speed;
		x8 = f->x * 8 + f->fx + div256(sine(s->world, f->heading) *
		    f->speed);
		y8 = f->y * 8 + f->fy - div256(sine(s->world, f->heading +
		    0x40) * f->speed);
		nx = x8 >> 3;
		ny = y8 >> 3;
		/* Bounce back into the world at its edges. */
		if (nx > EDGE_MAX_X || nx < EDGE_MIN || ny > EDGE_MAX_Y ||
		    ny < EDGE_MIN) {
			f->heading = nx > EDGE_MAX_X ? 0xC0 : nx < EDGE_MIN ?
			    0x40 : ny > EDGE_MAX_Y ? 0x00 : 0x80;
			x8 = (nx > EDGE_MAX_X ? EDGE_MAX_X : nx < EDGE_MIN ?
			    EDGE_MIN : nx) * 8;
			y8 = (ny > EDGE_MAX_Y ? EDGE_MAX_Y : ny < EDGE_MIN ?
			    EDGE_MIN : ny) * 8;
			nx = x8 >> 3;
			ny = y8 >> 3;
		}
		/* Time passes whether or not the ship can move. */
		if (f->ticks % per_day(s, WIND_TICKS) == 0)
			shift_wind(s);
		if (eat(s))
			return (SAIL_CREW);
		if (f->ticks % day_ticks(s) == 0) {
			sim_advance(s, 1);
			if (s->crew_event != CREW_NONE)
				return (SAIL_CREW);
		}
		/* A town's arrival cell is looked up before the shore (VA
		 * 0x231b8): its land takes the ship into port.  Ships never
		 * sail onto other land; the player may go ashore. */
		t = town_at(s, nx, ny);
		if (world_terrain(s->world, nx, ny) == TERRAIN_LAND) {
			f->speed = 0;
			if (t >= 0)
				return (arrive(s, t, town));
			f->land_x = nx;
			f->land_y = ny;
			return (SAIL_LAND);
		}
		f->x = nx;
		f->y = ny;
		f->fx = (uint8_t)(x8 & 7);
		f->fy = (uint8_t)(y8 & 7);
		follow(s, 0);
		clouds_move(s);
		if (reef(s, world_terrain(s->world, nx, ny)))
			return (SAIL_CREW);
		if (t >= 0)
			return (arrive(s, t, town));
		/* Sails are met along the way: their checks count ticks,
		 * not days, so a longer day does not empty the sea. */
		if (s->encounters &&
		    (f->ticks + 4) % SIGHTING_TICKS == 0 &&
		    sighting(s)) {
			f->speed = 0;
			return (SAIL_ENCOUNTER);
		}
	}
	return (SAIL_TICKS);
}

#define BOARD_REACH	11	/* pixels from the landing point */

void
sim_go_ashore(struct sim *s)
{
	struct sim_fleet *f = &s->fleet;

	if (f->ashore || s->world == NULL ||
	    world_terrain(s->world, f->land_x, f->land_y) != TERRAIN_LAND)
		return;
	f->ashore = 1;
	f->ship_x = f->x;
	f->ship_y = f->y;
	f->x = f->land_x;
	f->y = f->land_y;
	f->fx = f->fy = 4;
	f->speed = 0;
	/* A party landing next to the town the ship just left (a failed
	 * attack, say) may walk into it (an engine choice). */
	f->leaving = -1;
}

void
sim_turn_back(struct sim *s)
{
	s->fleet.heading = (uint8_t)(s->fleet.heading + 0x80);
}

/* Land within a ship's length ahead (16 pixels, each checked: a spit a
 * few pixels thick would slip between coarser steps). */
static int
shore_ahead(const struct sim *s)
{
	const struct sim_fleet *f = &s->fleet;
	int d;

	for (d = 1; d <= 16; d++)
		if (world_terrain(s->world, f->x + sim_sine_mul(s->world,
		    f->heading, d), f->y - sim_sine_mul(s->world,
		    f->heading + 0x40, d)) == TERRAIN_LAND)
			return (1);
	return (0);
}

/* The heading nearest "away from pixel (x, y)" with open water ahead,
 * turning 16 at a time either side. */
static void
clear_heading(struct sim *s, int x, int y)
{
	struct sim_fleet *f = &s->fleet;
	int away = (sim_bearing(f->x - x, f->y - y) + 8) & 0xF0, k;

	for (k = 0; k <= 128; k += 16) {
		f->heading = (uint8_t)(away + k);
		if (!shore_ahead(s))
			return;
		f->heading = (uint8_t)(away - k);
		if (!shore_ahead(s))
			return;
	}
	f->heading = (uint8_t)away;
}

/* The ship's heading on boarding, by the party's direction (VA 0x367ac;
 * bits 1 west, 2 east, 4 north, 8 south). */
static uint8_t
boarding_heading(int dx, int dy)
{
	static const uint8_t h[16] = { 0, 192, 64, 0, 0, 224, 32, 0, 128,
	    160, 96, 0, 0, 0, 0, 0 };

	return (h[(dx < 0 ? 1 : dx > 0 ? 2 : 0) |
	    (dy < 0 ? 4 : dy > 0 ? 8 : 0)]);
}

enum sail_stop
sim_march(struct sim *s, int dx, int dy, int ticks, int *town)
{
	struct sim_fleet *f = &s->fleet;
	int32_t x8, y8, nx, ny;
	int t;

	*town = -1;
	if (s->world == NULL || !f->ashore)
		return (SAIL_TICKS);
	dx = dx < 0 ? -1 : dx > 0 ? 1 : 0;
	dy = dy < 0 ? -1 : dy > 0 ? 1 : 0;
	while (ticks-- > 0) {
		f->ticks++;
		if (f->ticks % per_day(s, WIND_TICKS) == 0)
			shift_wind(s);
		if (eat(s))
			return (SAIL_CREW);
		if (f->ticks % day_ticks(s) == 0)
			sim_advance(s, 1);
		follow(s, 0);
		clouds_move(s);		/* standing still too */
		if (dx == 0 && dy == 0)
			continue;
		x8 = f->x * 8 + f->fx + dx * 4;
		y8 = f->y * 8 + f->fy + dy * 4;
		nx = x8 >> 3;
		ny = y8 >> 3;
		if (nx > EDGE_MAX_X || nx < EDGE_MIN || ny > EDGE_MAX_Y ||
		    ny < EDGE_MIN)
			continue;
		/* The town first, as at sea (its cells may be water). */
		if ((t = town_at(s, nx, ny)) >= 0) {
			if (world_terrain(s->world, nx, ny) == TERRAIN_LAND) {
				f->x = nx;
				f->y = ny;
				f->fx = (uint8_t)(x8 & 7);
				f->fy = (uint8_t)(y8 & 7);
			}
			*town = t;
			f->leaving = t;
			return (SAIL_TOWN);
		}
		if (world_terrain(s->world, nx, ny) != TERRAIN_LAND) {
			/* Water: aboard again near the landing point. */
			if (abs(nx - f->land_x) < BOARD_REACH &&
			    abs(ny - f->land_y) < BOARD_REACH) {
				f->ashore = 0;
				f->x = f->ship_x;
				f->y = f->ship_y;
				f->fx = f->fy = 4;
				f->heading = boarding_heading(dx, dy);
				f->speed = 0;
				/* The party's way can face the shore again,
				 * and each step onto it would ask once more
				 * whether to disembark: the engine then
				 * heads away from where the party landed. */
				if (shore_ahead(s))
					clear_heading(s, f->land_x,
					    f->land_y);
				return (SAIL_BOARDED);
			}
			continue;
		}
		f->x = nx;
		f->y = ny;
		f->fx = (uint8_t)(x8 & 7);
		f->fy = (uint8_t)(y8 & 7);
	}
	return (SAIL_TICKS);
}

/*
 * The nearest town by the era's arrival cells (VA 0x24629 asks VA
 * 0x1adfc), and the distance to its own cell, |dx| + |dy|, at least 1.
 * Without a world, by the towns' cells.
 */
int
sim_nearest_town(const struct sim *s, int *dist)
{
	int cx = s->fleet.x / WORLD_TILE, cy = s->fleet.y / WORLD_TILE;
	int best = -1, d;
	size_t i;

	*dist = 0;
	if (s->world != NULL &&
	    (best = world_nearest_town(s->world, cx, cy)) >= 0 &&
	    (size_t)best < s->ntowns) {
		*dist = abs(s->towns[best].x - cx) + abs(s->towns[best].y - cy);
		if (*dist < 1)
			*dist = 1;
		return (best);
	}
	best = -1;
	for (i = 0; i < s->ntowns; i++) {
		d = abs(s->towns[i].x - cx) + abs(s->towns[i].y - cy);
		if (best < 0 || d < *dist) {
			best = (int)i;
			*dist = d;
		}
	}
	if (*dist < 1)
		*dist = 1;
	return (best);
}

/* Pick the ship met, from the era's records, once "Sail ho!" is
 * answered with investigate (VA 0xd310; sailing on picks none). */
static void
pick_ship(struct sim *s)
{
	struct sim_encounter *e = &s->encounter;
	const uint8_t *rec;
	int town = e->town, dist = e->dist;
	int nation = s->towns[town].nation & 3, k = 0, range, r;

	range = s->fleet.crew / 50 + s->heat / 6 -
	    (int16_t)s->towns[town].standing;
	if (range >= 1) {
		k = rng_below(&s->rng, range) / 3;
		if (k > ENCOUNTER_SHIPS - 1)
			k = ENCOUNTER_SHIPS - 1;
	}
	if (rng_below(&s->rng, 15) < 4 || dist > FAR_FROM_TOWNS) {
		r = rng_below(&s->rng, ENCOUNTER_NATIONS * ENCOUNTER_SHIPS);
		k = r / 4;
		nation = r % 4;
	}
	rec = s->world->encounter[nation][k];
	/* Off the town a governor sent the player to hunt a pirate, four
	 * times in five the first pirate of the mission nation's records
	 * (VA 0xd1fc). */
	if (s->mission.type == MISSION_PIRATE && town >= 0 &&
	    town == s->mission.dest && rng_below(&s->rng, 5) != 0) {
		int p;

		for (p = 0; p < ENCOUNTER_SHIPS; p++)
			if (s->world->encounter[s->mission.nation & 3][p][1] ==
			    NATION_PIRATE) {
				rec = s->world->encounter[s->mission.nation &
				    3][p];
				break;
			}
	}
	/* Off the treasure fleet's town: its galleon, unless the region
	 * is alerted (VA 0xd1fc). */
	if (town >= 0 && town == s->fleet_town &&
	    rng_below(&s->rng, 24) > s->heat)
		rec = s->world->treasure_ship;
	memset(e, 0, sizeof(*e));
	e->stage = ENC_IDENTIFIED;
	e->town = town;
	e->dist = dist;
	e->type = rec[0] & 7;
	/* Shown as a fast galleon while the era code is below 4, 1560
	 * to 1620 (VA 0xd1db). */
	if (e->type == 5 && s->world->era < 3)
		e->type = 8;
	e->nation = rec[1] <= NATION_PIRATE ? rec[1] : NATION_PIRATE;
	e->crew = rec[2];
	e->guns = rec[3];
	e->role = rec[4];
	e->cargo = rec[5];
	e->gold = rec[6];
	e->hostile = e->role > 5 && s->standing[e->nation] < 0;
	/* Off the town a governor sent the player to: the pirate he
	 * hunts (VA 0xd1fc). */
	e->named = s->mission.type == MISSION_PIRATE &&
	    s->mission.dest == town && e->nation == NATION_PIRATE;
}

/*
 * Her colours, the third question (VA 0xd581-0xd71d): the battle's wind
 * strength is drawn for every ship asked about; then a hostile warship or
 * pirate gets a named captain, the hunted pirate the mission's.
 */
static void
show_colours(struct sim *s)
{
	struct sim_encounter *e = &s->encounter;
	int town = e->town;

	e->stage = ENC_COLOURS;
	e->wind_strength = rng_below(&s->rng, 3);
	if (e->named)
		e->captain = s->mission.pirate;
	else if (e->hostile)
		e->captain = sim_name_captain(s, e->nation == NATION_PIRATE &&
		    town >= 0 ? s->towns[town].nation & 3 : e->nation & 3);
}

/* Every 32 ticks: maybe a sail on the horizon (game-rules.md). */
static int
sighting(struct sim *s)
{
	int dist, town, r, chance;

	if ((town = sim_nearest_town(s, &dist)) < 0)
		return (0);
	if (town != s->heat_town) {
		s->heat = 0;
		s->heat_town = town;
	}
	if (dist > 63)
		dist = 63;
	r = rng_below(&s->rng, 0x10000);
	if (dist < 8) {
		s->heat += 8 - dist;
		chance = (r & 7) + (s->heat >> 2) < 2;
	} else if (dist < 16)
		chance = (r & 15) < 2;
	else if (dist < 32)
		chance = (r & 31) < 4;
	else
		chance = (r & 63) < 4;
	if (!chance)
		return (0);
	/* Only the nearest town is known until she is investigated. */
	memset(&s->encounter, 0, sizeof(s->encounter));
	s->encounter.stage = ENC_SIGHTED;
	s->encounter.town = town;
	s->encounter.dist = dist;
	return (1);
}

static enum encounter_result
end_encounter(struct sim *s, enum encounter_result r)
{
	if (r == RES_ESCAPED || r == RES_NEWS)
		s->heat += HEAT_IGNORED;
	s->encounter.stage = ENC_NONE;
	return (r);
}

/* The dialogs end in a sea battle against the encounter's ship. */
static enum encounter_result
to_battle(struct sim *s, enum encounter_result r)
{
	int dist, town = sim_nearest_town(s, &dist);

	/* Closing for battle (VA 0xdb56) or an unanswered hail (VA
	 * 0xdb31) within 7 cells of a town lowers its field 9; being
	 * overtaken does not (VA 0xd999-0xda3f). */
	if (r != RES_OVERTAKEN && town >= 0 && dist < 7)
		s->towns[town].standing =
		    (uint16_t)((int16_t)s->towns[town].standing - 1);
	sim_battle_start(s);
	return (end_encounter(s, r));
}

enum encounter_result
sim_encounter_act(struct sim *s, enum encounter_action action)
{
	struct sim_encounter *e = &s->encounter;

	if (s->battle.active)
		return (RES_BATTLE);	/* fight it out first */
	switch (e->stage) {
	case ENC_SIGHTED:
		if (action == ACT_IGNORE)
			return (end_encounter(s, RES_ESCAPED));
		/* A ship set by hand (no town known) is kept as it is. */
		if (s->world != NULL && e->town >= 0 &&
		    (size_t)e->town < s->ntowns)
			pick_ship(s);
		else
			e->stage = ENC_IDENTIFIED;
		return (RES_CONTINUE);
	case ENC_IDENTIFIED:
		/* A hunter, or the pirate hunted, cannot be avoided once
		 * she is seen. */
		if (action == ACT_IGNORE && !e->hostile && !e->named)
			return (end_encounter(s, RES_ESCAPED));
		show_colours(s);
		return (RES_CONTINUE);
	case ENC_COLOURS:
		/* A ship with a captain may overtake a player who does not
		 * close for battle, hailing or not (VA 0xd98b). */
		if (action != ACT_BATTLE && e->captain != 0 &&
		    rng_below(&s->rng, 3) < s->player.difficulty)
			return (to_battle(s, RES_OVERTAKEN));
		if (action == ACT_BATTLE)
			return (to_battle(s, RES_BATTLE));
		if (action == ACT_HAIL && s->standing[e->nation] < -3)
			return (to_battle(s, RES_UNANSWERED));
		if (action == ACT_HAIL)
			return (end_encounter(s, RES_NEWS));
		return (end_encounter(s, RES_ESCAPED));
	default:
		return (RES_ESCAPED);
	}
}

/*
 * In town (VA 0x18fa9 and the ways in): the visit flags survive only a
 * return to the same town, and then only bits 0 and 1; the town's
 * information becomes current.
 */
void
sim_arrive(struct sim *s, int town)
{
	if (town < 0 || (size_t)town >= s->ntowns)
		return;
	if (town != s->visit_town) {
		s->visit_flags = 0;
		s->wright_gold = -1;
		s->wright_cannon = 0;
		s->merchant_gold = -1;
	} else
		s->visit_flags &= VISIT_GOVERNOR | VISIT_KEPT;
	s->visit_town = town;
}

void
sim_enter_town(struct sim *s, int town)
{
	struct sim_town *t;

	if (town < 0 || (size_t)town >= s->ntowns)
		return;
	sim_arrive(s, town);
	/* Every arrival is logged (VA 0x17e78, from the main loop at VA
	 * 0x1da14).  The original skips it only for the arrival a saved
	 * game resumes (VA 0x45e84 bit 1, cleared by the town menu at VA
	 * 0x186c3), which does not come through here. */
	log_news(s, NEWS_VISITED, town, 0);
	s->player_town = town;
	s->tavern_spots = TAVERN_ALL;		/* VA 0x18fa9 */
	t = &s->towns[town];
	t->flags = (uint16_t)((t->flags | FLAG_KNOWN) & ~FLAG_OBSOLETE);
}

/*
 * Time in town (VA 0x18771): each pass through the menu advances the
 * calendar by three days, the month's updates with it, as at sea (VA
 * 0x1dd0e).  The crew does not speak in port.
 */
void
sim_town_pass(struct sim *s)
{
	sim_advance(s, TOWN_DAYS);
}

void
sim_leave_town(struct sim *s)
{
	int n;

	/* Roll call (VA 0x17efb): (3 - mood) x crew / 15 men desert,
	 * when more than one. */
	s->deserted = 0;
	n = (int16_t)((3 - s->fleet.mood) * s->fleet.crew) / 15;
	if (n >= s->fleet.crew)
		n = s->fleet.crew - 1;
	if (n > 1) {
		sim_crew_leave(s, n);
		s->deserted = n;
	}
	/* A town where the heat ran high raises its nation's alarm (VA
	 * 0x28789). */
	if (s->heat > 15 && s->player_town >= 0 &&
	    (size_t)s->player_town < s->ntowns &&
	    s->towns[s->player_town].nation < 4)
		s->alert[s->towns[s->player_town].nation] = s->player_town;
	/* One prisoner may jump overboard when the ship is in port. */
	s->escaped = 0;
	n = rng_below(&s->rng, SIM_PRISONERS);
	if (s->prisoners[n] != 0 && !s->fleet.ashore) {
		s->escaped = s->prisoners[n] & 0xFF;
		s->prisoners[n] = 0;
	}
	if (s->player_town >= 0 && (size_t)s->player_town < s->ntowns) {
		if (s->fleet.ashore) {
			/* On foot: out on the town's own cell (VA 0x19c11). */
			s->fleet.x = s->towns[s->player_town].x * WORLD_TILE + 8;
			s->fleet.y = s->towns[s->player_town].y * WORLD_TILE + 8;
		} else
			sim_harbour_mouth(s, s->player_town);
		s->fleet.leaving = s->player_town;
	}
	s->player_town = SIM_NO_TOWN;
}
