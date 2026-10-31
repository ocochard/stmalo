/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026, Olivier Cochard-Labbé
 *
 * The start of the game: intro, welcome menu, and the choices that make a
 * new career (era, nation, name, difficulty, special ability), or a
 * famous expedition.  The texts are the game's own (CSTR and CSTN).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assets/credits.h"
#include "assets/img.h"
#include "assets/scenario.h"
#include "assets/pic.h"
#include "assets/sheet.h"
#include "assets/textfiles.h"
#include "assets/tiles.h"
#include "sim/hof.h"
#include "sim/save.h"
#include "screen.h"

/* String table entries used here (CSTR) and word lists (CSTN groups). */
#define S_SELECT_EXPEDITION	62
#define S_SELECT_ERA		64
#define S_NAME_PROMPT		65
#define S_DIFFICULTY_PROMPT	66
#define S_ABILITY_PROMPT	67
#define S_NATION_PROMPT		70
#define S_HOF_QUESTION		73	/* with its own ~Yes ~No options */
#define S_SAVE_GAME		290
#define S_LOAD_GAME		320
#define S_ERA_TEXT		261	/* six era descriptions */
#define S_EXPEDITION_TEXT	267	/* six expedition descriptions */
#define W_MONTH			7	/* January.. */
#define W_LOG			20	/* log sentences, by news code */
#define W_MONTH_SHORT		6	/* Jan, Feb... */
#define W_DIFFICULTY		35
#define W_NATION		71	/* Spanish, English, French, Dutch */
#define NATION_UNKNOWN		5	/* its "unknown" */
#define W_ABILITY		83
#define W_ERA			98
#define W_EXPEDITION		99
#define W_WELCOME		100	/* 6 heading lines, then 4 options */
#define W_NATION_NAME		81	/* Spain, England, France, Holland */
#define W_TRAVEL		86	/* in-game menu */
#define W_COMPASS		13	/* S, SSW... clockwise */
#define W_SAILING		30	/* sailing phrases */
#define SAILING_WINDS		8	/* "%s winds": the compass point */
#define SAILING_KNOTS		12	/* "Knots: " */
#define S_SAIL_HO		118	/* "Sail ho!" ... %s */
#define S_WATERS		119	/* "We're in %s waters" */
#define S_LOOKS_LIKE		120	/* "She looks like %s" */
#define S_COLOURS		122	/* "She's flying %s colors" */
#define S_OVERTAKEN		126
#define S_NAMED_CHASE		121	/* "Arrgh! It's the %s %s, %s!" */
#define S_UNANSWERED		315
#define W_A_SHIP		11	/* "a pinnace"... by ship type */
#define W_COLOURS		106	/* "Spanish"... "pirate" */
#define W_SHIP_TYPE		8	/* "Pinnace".. "Fort" */
#define W_DAMAGE		14	/* damage levels */
#define W_NEWS			18	/* 2 "Ship's News", 3 "(no entries)" */
#define NEWS_TITLE		2
#define NEWS_NONE		3
#define NEWS_SHOWN		6	/* the latest items a hail shows */
#define S_PRIZE			164	/* "we've captured %sa %s ton %s..." */
#define S_YOU_SUNK		167
#define S_ASHORE		161	/* the last ship lost: washed ashore */
#define S_PICKED_UP		162	/* "...back as %s captain" */
#define W_CAPTAIN		73	/* "an apprentice"..., by difficulty */
#define S_FOE_SUNK		168
#define S_SUNSET		169
#define S_STRUCK		170
#define S_YOU_ESCAPED		197
#define S_COMMAND		286
#define S_TOO_FEW		112	/* "Your crew is too small..." */
#define S_NO_OFFICERS		130	/* "You lack enough officers..." */
#define W_THE_SHIP		77	/* 4: "the ship" */
#define W_PLUNDERED		10	/* 6: " gold pieces plundered!" */
#define PLUNDERED_WORD		6
#define S_FORT_SUNSET		317
#define S_FORT_DRIVES_OFF	196
#define S_BEACHED		173
#define S_FOE_ESCAPED		198
#define S_BOARDING		171	/* "...you spot %s approaching" */
#define SAILING_RELOADING	0
#define SAILING_LOADED		1
#define SAILING_BATTLE_SAILS	2
#define SAILING_FULL_SAILS	3
#define SAILING_RAISING		4
#define SAILING_LOWERING	5
#define SAILING_CAPTAIN		7	/* "the enemy captain" */
#define SAILING_GUNS		10	/* "Guns: " */
#define SAILING_MEN		11	/* "Men: " */
#define TRAVEL_CONTINUE		0
#define TRAVEL_SAVE		8
#define TRAVEL_EXIT		9

#define WELCOME_HEADING		6

/* Text fonts by size: PIRATE1 is the largest (25 px). */
#define FONT_TITLE		0
#define FONT_BODY		1
#define FONT_SMALL		2	/* PIRATE3, 14 px */
#define FONT_PIRATE4		3
#define BOW_MARK		20	/* sbtlmisc.cur: the red cross */
#define SHOT_SHADOW		0	/* sbtlmisc.cur: + the spread, 0-3 */
#define SHOT_BALLS		4
#define SHOT_SPLASH		8	/* + landing / 8, 0-4 */
#define SHOT_BURST		24
#define WIND_COMPASS		32	/* + wind / 16, clockwise from up */
#define W_WIND			12	/* "light", "medium", "strong" */
#define W_RANK			78	/* Mr., Ensign, Captain... */
#define W_MOOD			72	/* angry .. happy */
#define W_UNKNOWN		92	/* 5: "?", 9: "NONE" */

static void	villain_name(const struct game *, int, char *, size_t);
static void	captain_name(const struct game *, int, char *, size_t);

/* Era of each famous expedition, from its year. */
static const int expedition_era[6] = { 0, 0, 2, 4, 4, 5 };

/* Portraits for the abilities, in the order of the ability list. */
static const char *ability_portrait[5] = {
	"spr/bigfen", "spr/bigcha", "spr/bignav", "spr/biggun", "spr/bigmed",
};

/* Flags in the oval, in town nation order. */
static const char *nation_flag[4] = {
	"spr/bigsflg", "spr/bigeflg", "spr/bigfflg", "spr/bigdflg",
};

/*
 * Identity screen (pic/idselect).  The game stores the rectangle
 * (237, 66)-(395, 273) next to its portrait table: the oval's opening.
 * Portraits are drawn clipped to it; centring them is a guess, since the
 * drawing code is not traced yet.  The scroll's flat parchment spans
 * x 51-582, y 315-391.
 */
#define OVAL_X0		237
#define OVAL_Y0		66
#define OVAL_X1		395
#define OVAL_Y1		273
#define SCROLL_X	80
#define SCROLL_Y	318
#define SCROLL_W	480
#define SCROLL_COL	230	/* option column width */

static void
plot(void *arg, int x, int y, uint32_t ink)
{
	struct fb *fb = arg;

	if (x >= 0 && y >= 0 && x < FB_W && y < FB_H)
		fb_put(fb, (size_t)y * FB_W + x, (uint8_t)ink);
}

static void
put_glyph(void *arg, const struct font *f, size_t g, int x_sub, int y,
    uint32_t ink)
{
	fb_glyph(arg, f, g, x_sub, y, (uint8_t)ink);
}

static void
style(const struct game *g, int font, int caps, struct text_style *st)
{
	st->font = &g->font[font];
	st->caps = caps ? &g->caps : NULL;
	st->ink = fb_nearest(&g->fb, g->ink[0], g->ink[1], g->ink[2]);
	st->highlight = fb_nearest(&g->fb, g->highlight[0], g->highlight[1],
	    g->highlight[2]);
	st->line_gap = 2;
}

static void
plot_none(void *arg, int x, int y, uint32_t ink)
{
	(void)arg; (void)x; (void)y; (void)ink;
}

static void
put_halo(void *arg, const struct font *f, size_t g, int x_sub, int y,
    uint32_t ink)
{
	fb_glyph_halo(arg, f, g, x_sub, y, (uint8_t)ink);
}

/*
 * Draw text into the framebuffer; options are remembered for input.
 * Over a picture, a halo goes under the whole text first, so that it
 * reads over a pack's photograph.
 */
static int
draw(struct game *g, int font, int x, int y, int w, const char *text,
    int menu)
{
	struct text_target t = { plot, &g->fb, put_glyph };
	struct text_target halo = { plot_none, &g->fb, put_halo };
	struct text_style st;
	struct text_layout lay;

	style(g, font, 1, &st);
	if (g->bg.pixels != NULL)
		text_draw(&halo, &st, x, y, w, text, menu ? g->selected : -1,
		    &lay);
	text_draw(&t, &st, x, y, w, text, menu ? g->selected : -1, &lay);
	if (menu)
		g->options = lay;
	return (lay.height);
}

/*
 * "Click to continue" (VA 0x1a3c4): after about seven seconds without
 * input (420 vertical retraces), the plaque of ui/misc frame 7 appears
 * at (268, 449); a key or a click goes on at any time.
 */
#define CONTINUE_IDLE		420
#define CONTINUE_FRAME		7
#define DISEMBARK_FRAME		8	/* "Disembark", answered Y or N */
#define QUIT_FRAME		4	/* "Quit (Y/N)" */

static void
continue_prompt(struct game *g)
{
	if (g->idle >= CONTINUE_IDLE && g->misc.count > CONTINUE_FRAME)
		fb_frame(&g->fb, &g->misc.frames[CONTINUE_FRAME], 268, 449);
}

static void
draw_centered(struct game *g, int font, int y, const char *text)
{
	int w = text_width(&g->font[font], text);

	draw(g, font, (FB_W - w) / 2, y, FB_W, text, 0);
}

/* Build "~a\r~b..." from a word list, for a menu. */
static void
options(const struct game *g, size_t group, int count, char *buf,
    size_t len)
{
	int i;

	buf[0] = '\0';
	for (i = 0; i < count; i++) {
		if (i > 0)
			strncat(buf, "\r", len - strlen(buf) - 1);
		strncat(buf, "~", len - strlen(buf) - 1);
		strncat(buf, game_word(g, group, (size_t)i),
		    len - strlen(buf) - 1);
	}
}

/*
 * Menu input shared by every choice screen: arrows move, Enter or a click
 * chooses, Escape goes back.  "enabled" may veto options.
 */
static void
menu_event(struct game *g, const struct ui_event *ev, int count,
    int (*enabled)(const struct game *, int),
    void (*choose)(struct game *, int), const struct screen *back)
{
	int i, step;

	switch (ev->type) {
	case EV_KEY:
		if (ev->key == UI_KEY_UP || ev->key == UI_KEY_DOWN) {
			step = ev->key == UI_KEY_UP ? count - 1 : 1;
			for (i = 0; i < count; i++) {
				g->selected = (g->selected + step) % count;
				if (enabled == NULL || enabled(g, g->selected))
					break;
			}
		} else if (ev->key == UI_KEY_ENTER) {
			if (enabled == NULL || enabled(g, g->selected))
				choose(g, g->selected);
		} else if (ev->key == UI_KEY_ESCAPE && back != NULL)
			game_goto(g, back);
		break;
	case EV_MOVE:
	case EV_CLICK:
		for (i = 0; i < g->options.noptions && i < count; i++) {
			const struct text_option *o = &g->options.option[i];

			if (ev->x < o->x || ev->x >= o->x + o->w ||
			    ev->y < o->y || ev->y >= o->y + o->h)
				continue;
			if (enabled != NULL && !enabled(g, i))
				break;
			g->selected = i;
			if (ev->type == EV_CLICK)
				choose(g, i);
			break;
		}
		break;
	default:
		break;
	}
}


/*
 * Intro (VA 0x3325): the MicroProse logo; "and", "MPS Labs" and
 * "Present" on black, each faded in and out; then the moonlit ship
 * (moon03) with its palette cycling, where the "Pirates! Gold" title
 * dissolves in on the right.  Enter, Escape or a click (VA 0x2246)
 * skips: at once from the first three steps, to the moon from
 * "Present", to the title from the lead before it, and out from the
 * title with its fade.
 */
static void	load_bank(struct game *, const char *, int,
		    struct sprite_bank *);
static void	sword_load(struct game *);
static void	fill(char *, size_t, const char *, const char *, const char *);

#define INTRO_STEPS		5
#define TITLE_X			272	/* the title's left edge */
#define TITLE_W			368
#define TITLE_H			274	/* two sprites: 135 + 134 rows */
#define CELLS_W			184	/* the dissolve's 2 x 2 cells */
#define CELLS_H			137
#define CELL_SPACE		32768	/* power of two above 0x61c0 */
#define TITLE_PASSES		45000	/* loop passes before the menu */
#define TITLE_LEAD		2000	/* palette cycling before the title */
#define PASSES_PER_TICK		167	/* engine choice: 0.1 ms a pass */

static const struct {
	const char	*pic;		/* NULL: black, the logo's palette */
	const char	*spr;
	int		 x, y;
	int		 hold;		/* ticks at full brightness */
} intro_steps[INTRO_STEPS] = {
	{ "pic/micro", NULL, 0, 0, 120 },	/* 20 x 100 ms */
	{ NULL, "spr/and", 229, 227, 60 },	/* 20 x 50 ms */
	{ NULL, "spr/mpslabs", 195, 100, 120 },
	{ NULL, "spr/present", 134, 231, 60 },
	{ "pic/moon03", NULL, 0, 0, 0 },	/* ends by its loop */
};

/* Rotate palette entries [start, start + count) up by one. */
static void
cycle(struct game *g, int start, int count)
{
	uint8_t last[3];

	memcpy(last, &g->fb.pal[(start + count - 1) * 3], 3);
	memmove(&g->fb.pal[(start + 1) * 3], &g->fb.pal[start * 3],
	    (size_t)(count - 1) * 3);
	memcpy(&g->fb.pal[start * 3], last, 3);
}

/* Compose both title sprites into one layer. */
static void
intro_title_layer(struct game *g)
{
	int i, x, y, oy = 0;

	free(g->intro_title);
	free(g->intro_shown);
	g->intro_title = calloc(TITLE_W * TITLE_H, 1);
	g->intro_shown = calloc(CELLS_W * CELLS_H, 1);
	if (g->intro_title == NULL || g->intro_shown == NULL)
		return;
	for (i = 0; i < 2; i++) {
		const struct frame *f;

		if (g->intro_spr[i].count == 0)
			continue;
		f = &g->intro_spr[i].frames[0];
		for (y = 0; y < f->h && oy + y < TITLE_H; y++)
			for (x = 0; x < f->w && x < TITLE_W; x++)
				if (f->mask == NULL || f->mask[y * f->w + x])
					g->intro_title[(oy + y) * TITLE_W + x] =
					    f->pixels[y * f->w + x];
		oy += f->h;
	}
}

static void
intro_enter(struct game *g)
{
	int step;

	game_music(g, MUS_TITLE);
	if (g->intro_next) {	/* changed only once the fade is over */
		g->intro_step++;
		g->intro_next = 0;
	}
	step = g->intro_step;
	game_portrait(g, NULL);
	g->intro_hold = 0;
	bank_free(&g->intro_spr[0]);
	bank_free(&g->intro_spr[1]);
	if (intro_steps[step].pic != NULL)
		game_background(g, intro_steps[step].pic);
	else
		image_free(&g->bg);	/* black, keeping the palette */
	if (intro_steps[step].spr != NULL)
		load_bank(g, intro_steps[step].spr, 0, &g->intro_spr[0]);
	if (step == INTRO_STEPS - 1) {
		load_bank(g, "spr/title", 0, &g->intro_spr[0]);
		load_bank(g, "spr/title1", 0, &g->intro_spr[1]);
		intro_title_layer(g);
		g->intro_iter = 0;
		g->intro_cell = 0;	/* the original starts at random */
		g->intro_left = CELL_SPACE;
		g->intro_timer[0] = g->intro_timer[3] = 300;
		g->intro_timer[1] = 50;
		g->intro_timer[2] = 100;
		g->intro_cycles = 26;
		g->intro_pause = 0;
	}
}

static void
intro_free(struct game *g)
{
	free(g->intro_title);
	free(g->intro_shown);
	g->intro_title = g->intro_shown = NULL;
}

static void
intro_leave(struct game *g)
{
	intro_free(g);
	game_goto(g, &screen_credits);
}

/* The keys that skip the intro and the credits (VA 0x2246): Enter,
 * Escape or the left button; the others are ignored. */
static int
intro_skip_key(const struct ui_event *ev)
{
	return (ev->type == EV_CLICK || (ev->type == EV_KEY &&
	    (ev->key == UI_KEY_ENTER || ev->key == UI_KEY_ESCAPE)));
}

static void
intro_skip(struct game *g)
{
	g->intro_queued = 0;
	if (g->intro_step < 3) {
		/* The logo and the first two sprites: cut to black, no
		 * fade out (VA 0x35f4); the credits fade in. */
		intro_free(g);
		g->brightness = 0;
		game_goto(g, &screen_credits);
	} else if (g->intro_step == 3) {
		/* "Present": its wait cut short, then the moon (VA
		 * 0x34fb). */
		g->intro_next = 1;
		game_goto(g, &screen_intro);
	} else if (g->intro_iter < TITLE_LEAD)
		g->intro_iter = TITLE_LEAD;	/* on to the title (0x3781) */
	else
		intro_leave(g);
}

static void
intro_event(struct game *g, const struct ui_event *ev)
{
	if (!intro_skip_key(ev))
		return;
	/* The fades do not read the keys: one pressed meanwhile waits in
	 * the queue for the next step. */
	if (g->fading != 0)
		g->intro_queued = 1;
	else
		intro_skip(g);
}

/* One pass of the title screen's loop (VA 0x36e4, then 0x379f).
 * Returns 1 when the screen is over. */
static int
title_pass(struct game *g)
{
	uint32_t row, col;

	if (g->intro_iter < TITLE_LEAD) {
		if (--g->intro_timer[0] < 0) {
			cycle(g, 0xee, 0x11);
			g->intro_timer[0] = 300;
		}
		if (--g->intro_timer[1] < 0) {
			cycle(g, 100, 0x40);
			g->intro_timer[1] = 50;
		}
		if (--g->intro_timer[2] < 0) {
			cycle(g, 0xc2, 0xd);
			g->intro_timer[2] = 100;
		}
		g->intro_iter++;
		return (0);
	}
	if (g->intro_iter == TITLE_LEAD) {
		/* The title starts its cycles afresh (VA 0x3855). */
		g->intro_timer[0] = 300;
		g->intro_timer[1] = 50;
		g->intro_timer[2] = 100;
	}
	if (g->intro_left != 0) {
		/* Visit the cells in the order of x = 5x + 1, skipping
		 * those below the title. */
		for (;;) {
			g->intro_cell = (g->intro_cell * 5 + 1) %
			    CELL_SPACE;
			row = g->intro_cell / CELLS_W;
			if (row < CELLS_H)
				break;
			if (--g->intro_left == 0)
				break;
		}
		if (g->intro_left != 0) {
			col = g->intro_cell % CELLS_W;
			if (g->intro_shown != NULL)
				g->intro_shown[row * CELLS_W + col] = 1;
			g->intro_left--;
		}
	} else if (g->intro_pause == 0) {
		if (--g->intro_timer[3] < 0) {
			cycle(g, 0xd3, 0x1a);
			g->intro_timer[3] = 300;
			if (--g->intro_cycles == 0) {
				g->intro_cycles = 26;
				g->intro_pause = 10000;
			}
		}
	} else
		g->intro_pause--;
	if (--g->intro_timer[0] < 0) {
		cycle(g, 0xee, 0x11);
		g->intro_timer[0] = 300;
	}
	if (--g->intro_timer[1] < 0) {
		cycle(g, 100, 0x40);
		g->intro_timer[1] = 50;
	}
	if (--g->intro_timer[2] < 0) {
		cycle(g, 0xc2, 0xd);
		g->intro_timer[2] = 100;
	}
	g->intro_iter++;
	return (g->intro_iter - TITLE_LEAD >= TITLE_PASSES &&
	    g->intro_cycles == 26);
}

static void
intro_tick(struct game *g)
{
	int i;

	if (g->fading != 0)
		return;
	if (g->intro_queued) {
		intro_skip(g);
		return;
	}
	if (g->intro_step == INTRO_STEPS - 1) {
		for (i = 0; i < PASSES_PER_TICK; i++)
			if (title_pass(g)) {
				intro_leave(g);
				return;
			}
		return;
	}
	if (++g->intro_hold >= intro_steps[g->intro_step].hold) {
		g->intro_next = 1;
		game_goto(g, &screen_intro);
	}
}

/*
 * A replacement title in true colours, dissolving in by the same cells:
 * each row of cells drawn as its runs of revealed ones.  It takes no
 * part in the palette's cycling.
 */
static void
intro_draw_hd(struct game *g)
{
	const struct frame *f;
	int i, oy = 0, r, c, c0;

	for (i = 0; i < 2; i++) {
		if (g->intro_spr[i].count == 0)
			continue;
		f = &g->intro_spr[i].frames[0];
		for (r = oy / 2; r < CELLS_H && 2 * r < oy + f->h; r++)
			for (c = 0; c < CELLS_W; c++) {
				if (!g->intro_shown[r * CELLS_W + c])
					continue;
				for (c0 = c; c + 1 < CELLS_W &&
				    g->intro_shown[r * CELLS_W + c + 1]; c++)
					;
				fb_frame_rgba_clip(&g->fb, f, TITLE_X, oy,
				    TITLE_X + 2 * c0, 2 * r, TITLE_X + 2 * c + 1,
				    2 * r + 1);
			}
		oy += f->h;
	}
}

static void
intro_draw(struct game *g)
{
	int step = g->intro_step, x, y;
	uint8_t c;

	if (g->intro_spr[0].count > 0 && step < INTRO_STEPS - 1)
		fb_frame(&g->fb, &g->intro_spr[0].frames[0],
		    intro_steps[step].x, intro_steps[step].y);
	if (step != INTRO_STEPS - 1 || g->intro_title == NULL ||
	    g->intro_shown == NULL)
		return;
	if (g->intro_spr[0].count > 0 &&
	    g->intro_spr[0].frames[0].rgba != NULL) {
		intro_draw_hd(g);
		return;
	}
	for (y = 0; y < TITLE_H; y++)
		for (x = 0; x < TITLE_W; x++)
			if (g->intro_shown[(y / 2) * CELLS_W + x / 2] &&
			    (c = g->intro_title[y * TITLE_W + x]) != 0)
				fb_put(&g->fb, y * FB_W + TITLE_X + x, c);
}

const struct screen screen_intro = {
	"intro", intro_enter, intro_event, intro_tick, intro_draw,
};

/*
 * Credits (VA 0x1fe91): on the ship picture, six blocks of the credits
 * picture appear one at a time on the right.  Each fades in as its ink
 * (palette entry 0x20) steps from the parchment colour to black, stays
 * 1.6 s, fades out, and 0.4 s pass before the next (VA 0x20114).
 * Enter, Escape or a click skips to the last block; another ends it.
 */
#define CRED_BLOCKS		UI_CREDIT_BLOCKS
#define CRED_INK		0x20
#define CRED_STEPS		20
#define CRED_STEP_MS		40
#define CRED_HOLD_MS		1600
#define CRED_GAP_MS		400
#define CRED_CX			508	/* the blocks' centre */
#define CRED_CY			240

static const char *
credits_picture(const struct game *g)
{
	if (strcmp(g->lang, "fre") == 0)
		return ("pic/credtxtf");
	if (strcmp(g->lang, "ger") == 0)
		return ("pic/credtxtg");
	return ("pic/credtext");
}

/* Block "i": its rectangle in the credits picture. */
static int
credit_rect(const struct game *g, int i, int r[4])
{
	int k;

	if (i < 0 || i >= UI_CREDIT_BLOCKS)
		return (-1);
	for (k = 0; k < 4; k++)
		r[k] = g->ui.credit_rect[i][k];
	return (0);
}

/* Ink step 0 (parchment) .. 19 (black). */
static void
credit_ink(struct game *g, int step)
{
	if (step >= 0 && step < UI_CREDIT_STEPS)
		memcpy(&g->fb.pal[CRED_INK * 3], g->ui.credit_ink[step], 3);
}

/*
 * A pack's credits as text (asset-packs.md, "Open credits"): each block
 * a heading in PIRATE4 and lines in PIRATE1, centred where the
 * picture's blocks are, faded in and out by opacity on the same steps.
 */
#define CRED_HEAD_FONT		3	/* pirate4 */
#define CRED_LINE_FONT		0	/* pirate1 */
#define CRED_HEAD_GAP		10
#define CRED_MAX_W		256
#define CRED_MAX_H		440

static int
credit_text_height(const struct game *g, const struct credit_block *b)
{
	int h = (int)b->nlines * g->font[CRED_LINE_FONT].height;

	if (b->heading[0] != '\0')
		h += g->font[CRED_HEAD_FONT].height +
		    (b->nlines > 0 ? CRED_HEAD_GAP : 0);
	return (h);
}

static void
credit_text_warn(const char *id, size_t i, const struct font *f,
    const char *s)
{
	if (text_width(f, s) > CRED_MAX_W)
		fprintf(stderr, "%s: block %zu: \"%s\" is wider than %d "
		    "pixels\n", id, i, s, CRED_MAX_W);
}

/*
 * Load the pack's credits text for the language, else English's: 1
 * when loaded, 0 when no pack gives them, -1 when they are broken.
 */
static int
credits_text_load(struct game *g)
{
	const char *lang[2] = { g->lang, "eng" };
	const struct credit_block *b;
	struct blob blob;
	char id[32], why[128];
	size_t i, k;
	int n, rc;

	for (n = 0; n < 2; n++) {
		snprintf(id, sizeof(id), "text/%s/credits", lang[n]);
		if (packs_load(g->packs, id, &blob) == 0)
			break;
	}
	if (n == 2)
		return (0);
	rc = credits_decode(blob.data, blob.len, &g->cred_text, why,
	    sizeof(why));
	blob_free(&blob);
	/* A block crediting an asset the packs do not give goes. */
	for (i = g->cred_text.nblocks; rc == 0 && i-- > 0;)
		if (g->cred_text.blocks[i].needs != NULL &&
		    !packs_provides(g->packs, g->cred_text.blocks[i].needs))
			credits_drop(&g->cred_text, i);
	for (i = 0; rc == 0 && i < g->cred_text.nblocks; i++) {
		b = &g->cred_text.blocks[i];
		if (credit_text_height(g, b) > CRED_MAX_H) {
			snprintf(why, sizeof(why), "block %zu is taller than "
			    "%d pixels", i, CRED_MAX_H);
			credits_free(&g->cred_text);
			rc = -1;
			break;
		}
		credit_text_warn(id, i, &g->font[CRED_HEAD_FONT], b->heading);
		for (k = 0; k < b->nlines; k++)
			credit_text_warn(id, i, &g->font[CRED_LINE_FONT],
			    b->lines[k]);
	}
	if (rc != 0) {
		fprintf(stderr, "%s: %s\n", id, why);
		return (-1);
	}
	return (1);
}

/* Ink step 0 (none) .. 19 (full), as the picture's or the text's. */
static void
credit_step(struct game *g, int step)
{
	g->cred_step = step;
	if (g->cred_text.nblocks > 0)
		memcpy(&g->fb.pal[CRED_INK * 3], g->cred_text.ink, 3);
	else
		credit_ink(g, step);
}

static void
credits_enter(struct game *g)
{
	game_background(g, "pic/credpic");
	image_free(&g->credits);
	credits_free(&g->cred_text);
	switch (credits_text_load(g)) {
	case 1:
		g->cred_count = (int)g->cred_text.nblocks;
		break;
	case 0:
		/* The picture's blocks are the original's: without them,
		 * no credits. */
		pic_load(g->packs, credits_picture(g), &g->credits);
		g->cred_count = g->ui.has_credits ? CRED_BLOCKS : 0;
		break;
	default:
		g->cred_count = 0;	/* broken: skipped */
		break;
	}
	g->cred_block = 0;
	g->cred_ticks = 0;
	credit_step(g, 0);
}

static void
credits_done(struct game *g)
{
	image_free(&g->credits);
	credits_free(&g->cred_text);
	game_goto(g, &screen_welcome);
}

static void
credits_event(struct game *g, const struct ui_event *ev)
{
	if (!intro_skip_key(ev))
		return;
	if (g->cred_block < g->cred_count - 1) {
		g->cred_block = g->cred_count - 1;
		g->cred_ticks = 0;
		credit_step(g, 0);
	} else
		credits_done(g);
}

static void
credits_tick(struct game *g)
{
	int t, fade = CRED_STEPS * CRED_STEP_MS;

	if (g->fading != 0)
		return;
	if (g->cred_count == 0) {
		credits_done(g);
		return;
	}
	t = ++g->cred_ticks * 1000 / 60;
	if (t < fade)
		credit_step(g, t / CRED_STEP_MS);
	else if (t < fade + CRED_HOLD_MS)
		credit_step(g, CRED_STEPS - 1);
	else if (t < 2 * fade + CRED_HOLD_MS)
		credit_step(g, CRED_STEPS - 1 - (t - fade - CRED_HOLD_MS) /
		    CRED_STEP_MS);
	else if (t < 2 * fade + CRED_HOLD_MS + CRED_GAP_MS)
		credit_step(g, 0);	/* the block gone, a pause */
	else if (++g->cred_block < g->cred_count) {
		g->cred_ticks = 0;
		credit_step(g, 0);
	} else
		credits_done(g);
}

/* The text target of a credits block: its glyphs at an opacity. */
struct fade_target {
	struct fb	*fb;
	int		 opacity;	/* 0..255 */
};

static void
plot_fade(void *arg, int x, int y, uint32_t ink)
{
	struct fade_target *ft = arg;

	if (ft->opacity >= 128)
		plot(ft->fb, x, y, ink);
}

static void
glyph_fade(void *arg, const struct font *f, size_t g, int x_sub, int y,
    uint32_t ink)
{
	struct fade_target *ft = arg;

	fb_glyph_fade(ft->fb, f, g, x_sub, y, (uint8_t)ink, ft->opacity);
}

static void
credit_text_line(struct game *g, struct fade_target *ft, int font,
    const char *s, int y)
{
	struct text_target t = { plot_fade, ft, glyph_fade };
	struct text_style st = { NULL, NULL, CRED_INK, CRED_INK, 0 };

	st.font = &g->font[font];
	text_draw(&t, &st, CRED_CX - text_width(st.font, s) / 2, y, FB_W, s,
	    -1, NULL);
}

static void
credits_text_draw(struct game *g)
{
	const struct credit_block *b = &g->cred_text.blocks[g->cred_block];
	struct fade_target ft;
	size_t k;
	int y;

	ft.fb = &g->fb;
	ft.opacity = g->cred_step * 255 / (CRED_STEPS - 1);
	y = CRED_CY - credit_text_height(g, b) / 2;
	if (b->heading[0] != '\0') {
		credit_text_line(g, &ft, CRED_HEAD_FONT, b->heading, y);
		y += g->font[CRED_HEAD_FONT].height + CRED_HEAD_GAP;
	}
	for (k = 0; k < b->nlines; k++) {
		credit_text_line(g, &ft, CRED_LINE_FONT, b->lines[k], y);
		y += g->font[CRED_LINE_FONT].height;
	}
}

static void
credits_draw(struct game *g)
{
	int r[4], w, h, x, y;

	if (g->cred_block >= g->cred_count)
		return;
	if (g->cred_text.nblocks > 0) {
		credits_text_draw(g);
		return;
	}
	if (g->credits.pixels == NULL ||
	    credit_rect(g, g->cred_block, r) != 0)
		return;
	w = r[2] - r[0] + 1;
	h = r[3] - r[1];
	x = (CRED_CX - w / 2) & ~7;
	y = CRED_CY - h / 2;
	fb_image_part(&g->fb, &g->credits, r[0], r[1], w, h + 1, x, y, -1);
}

const struct screen screen_credits = {
	"credits", credits_enter, credits_event, credits_tick, credits_draw,
};

/* Welcome menu. */

enum { WELCOME_NEW, WELCOME_LOAD, WELCOME_FAMOUS, WELCOME_QUIT };

static void
welcome_enter(struct game *g)
{
	game_music(g, MUS_TITLE);	/* kept on if playing */
	game_background(g, "pic/textback");
	game_portrait(g, NULL);
}

static int
welcome_enabled(const struct game *g, int i)
{
	return (i != WELCOME_LOAD || g->save_dir[0] != '\0');
}

static void
welcome_choose(struct game *g, int i)
{
	switch (i) {
	case WELCOME_NEW:
		g->career.expedition = -1;
		game_goto(g, &screen_era);
		break;
	case WELCOME_LOAD:
		g->saving = 0;
		game_goto(g, &screen_slots);
		break;
	case WELCOME_FAMOUS:
		game_goto(g, &screen_expedition);
		break;
	case WELCOME_QUIT:
		game_goto(g, NULL);
		break;
	}
}

static void
welcome_event(struct game *g, const struct ui_event *ev)
{
	menu_event(g, ev, 4, welcome_enabled, welcome_choose, NULL);
}

static void
welcome_draw(struct game *g)
{
	/* VA 0x1443e: every line centred; the heading in PIRATE4, the
	 * options in PIRATE1 with full-width rows for the mouse. */
	static const int line_y[WELCOME_HEADING + 4] = { 70, 120, 145, 170,
	    195, 250, 300, 330, 360, 390 };
	struct text_target t = { plot, &g->fb, put_glyph };
	struct text_style st;
	char opt[256], quit[96];
	const char *text;
	int i, w;

	for (i = 0; i < WELCOME_HEADING + 4; i++) {
		text = game_word(g, W_WELCOME, (size_t)i);
		if (i < WELCOME_HEADING) {
			w = text_width(&g->font[FONT_PIRATE4], text);
			draw(g, FONT_PIRATE4, FB_W / 2 - w / 2, line_y[i],
			    FB_W, text, 0);
			continue;
		}
		/* "Return to DOS?" reads as the cabin's "Exit game"
		 * (CSTN 86.9), translated in every language: there is no
		 * DOS to return to (an engine choice). */
		if (i == WELCOME_HEADING + WELCOME_QUIT) {
			snprintf(quit, sizeof(quit), "%s?",
			    game_word(g, 86, 9));
			text = quit;
		}
		snprintf(opt, sizeof(opt), "~%s", text);
		w = text_width(&g->font[FONT_TITLE], text);
		style(g, FONT_TITLE, 1, &st);
		text_draw(&t, &st, FB_W / 2 - w / 2, line_y[i], FB_W, opt,
		    g->selected == i - WELCOME_HEADING ? 0 : -1, NULL);
		g->options.option[i - WELCOME_HEADING].x = 0;
		g->options.option[i - WELCOME_HEADING].y = line_y[i];
		g->options.option[i - WELCOME_HEADING].w = FB_W;
		g->options.option[i - WELCOME_HEADING].h = 30;
	}
	g->options.noptions = 4;
}

const struct screen screen_welcome = {
	"welcome", welcome_enter, welcome_event, NULL, welcome_draw,
};

/* Era choice, with the era's description. */

static void
parchment_enter(struct game *g)
{
	game_background(g, "pic/textback");
	game_portrait(g, NULL);
}

static void
era_choose(struct game *g, int i)
{
	g->career.era = i;
	game_goto(g, &screen_nation);
}

static void
era_event(struct game *g, const struct ui_event *ev)
{
	menu_event(g, ev, 6, NULL, era_choose, &screen_welcome);
}

/*
 * The era screen (VA 0x15a3c): the title, then each period's name
 * centred (PIRATE1) with its description under it in the small PIRATE3,
 * every 70 pixels; each period is a full-width row for the mouse.
 */
#define ERA_NAME_Y		60
#define ERA_DESC_Y		83
#define ERA_STEP		70

static void
era_draw(struct game *g)
{
	struct text_target t = { plot, &g->fb, put_glyph };
	struct text_style st;
	char opt[160];
	const char *name;
	int i, w;

	w = text_width(&g->font[FONT_BODY], game_str(g, S_SELECT_ERA));
	draw(g, FONT_BODY, FB_W / 2 - w / 2, 20, FB_W,
	    game_str(g, S_SELECT_ERA), 0);
	for (i = 0; i < 6; i++) {
		name = game_word(g, W_ERA, (size_t)i);
		snprintf(opt, sizeof(opt), "~%s", name);
		w = text_width(&g->font[FONT_TITLE], name);
		style(g, FONT_TITLE, 1, &st);
		text_draw(&t, &st, FB_W / 2 - w / 2, ERA_NAME_Y + i * ERA_STEP,
		    FB_W, opt, g->selected == i ? 0 : -1, NULL);
		draw(g, FONT_SMALL, 40, ERA_DESC_Y + i * ERA_STEP, 580,
		    game_str(g, S_ERA_TEXT + (size_t)i), 0);
		g->options.option[i].x = 0;
		g->options.option[i].y = ERA_NAME_Y + i * ERA_STEP;
		g->options.option[i].w = FB_W;
		g->options.option[i].h = ERA_STEP;
	}
	g->options.noptions = 6;
}

const struct screen screen_era = {
	"era", parchment_enter, era_event, NULL, era_draw,
};

/* Famous expedition choice. */

static void
expedition_choose(struct game *g, int i)
{
	struct scenario scn;
	char err[256];

	g->career.expedition = i;
	g->career.era = expedition_era[i];
	/* The expedition's own colours, for the flag. */
	if (scenario_load(g->packs, g->layout, g->exe, g->exe_len,
	    SCENARIO_CAREERS + i, &scn, err, sizeof(err)) == 0)
		g->career.nation = scn.nation;
	game_goto(g, &screen_ability);
}

static void
expedition_event(struct game *g, const struct ui_event *ev)
{
	menu_event(g, ev, 6, NULL, expedition_choose, &screen_welcome);
}

static void
expedition_draw(struct game *g)
{
	char buf[512];

	draw_centered(g, FONT_TITLE, 40, game_str(g, S_SELECT_EXPEDITION));
	options(g, W_EXPEDITION, 6, buf, sizeof(buf));
	draw(g, FONT_BODY, 150, 100, 420, buf, 1);
	draw(g, FONT_BODY, 80, 300, 480,
	    game_str(g, S_EXPEDITION_TEXT + (size_t)g->selected), 0);
}

const struct screen screen_expedition = {
	"expedition", parchment_enter, expedition_event, NULL,
	expedition_draw,
};

/*
 * Identity (VA 0x1482e): one picture that fills in as the player
 * answers.  The captain's portrait sits in the oval; the nationality is
 * chosen by clicking one of four flags, two on each side (VA 0x14aa4),
 * and its big flag then stays at the top left; the special ability is
 * one of five icons along the bottom (VA 0x14e6e), its picture then at
 * the top right; the difficulty is a row of four levels on the scroll,
 * the portrait changing with each (VA 0x151c6); the name comes last.
 * Pointing at a choice names it on the scroll.  The rectangles are the
 * game's (VA 0x33488, 0x334b0, 0x33448).
 */
#define W_COLORS		82	/* "English colors"... */
#define W_DIFF_TEXT		101	/* "An easy level"... */
#define ID_LABEL_Y		368
#define FLAG_SLOTS		4	/* English, French, Dutch, Spanish */
#define SKILL_SLOTS		5

/* Flag slots, in the game's order, as town nations. */
static const int flag_nation[FLAG_SLOTS] = {
	NATION_ENGLAND, NATION_FRANCE, NATION_HOLLAND, NATION_SPAIN,
};
static const struct { int x0, y0, x1, y1; } flag_rect[FLAG_SLOTS] = {
	{ 0, 0, 129, 133 }, { 51, 155, 199, 279 }, { 481, 1, 629, 139 },
	{ 441, 151, 599, 279 },
};
/* Ability slots: the icon frame shown in each. */
static const int skill_frame[SKILL_SLOTS] = { 0, 3, 1, 2, 4 };
static const struct { int x0, y0, x1, y1; } skill_rect[SKILL_SLOTS] = {
	{ 42, 342, 85, 477 }, { 147, 391, 178, 474 }, { 276, 396, 353, 470 },
	{ 421, 396, 527, 456 }, { 540, 345, 620, 460 },
};
static const int level_x[4] = { 80, 190, 300, 410 };
#define LEVEL_Y			345
#define LEVEL_H			20

/* Captain portraits, one per difficulty level. */
static const char *captain_portrait[4] = {
	"spr/bigman0", "spr/bigman1", "spr/bigman2", "spr/bigman3",
};

static void
identity_enter(struct game *g)
{
	game_background(g, "pic/idselect");
	game_portrait(g, NULL);
	g->id_key = -1;
}

static void
id_frame(struct game *g, int bank, int frame, int x, int y)
{
	if ((size_t)frame < g->id_spr[bank].count)
		fb_frame(&g->fb, &g->id_spr[bank].frames[frame], x, y);
}

static void
id_label(struct game *g, int cx, int y, const char *text)
{
	int w = text_width(&g->font[FONT_BODY], text);

	draw(g, FONT_BODY, cx - w / 2, y, FB_W, text, 0);
}

/* The labels under the big pictures, in the game's second, light ink. */
static void
id_caption(struct game *g, int cx, int y, const char *text)
{
	static const uint8_t light[3] = { 240, 220, 170 };
	uint8_t keep[3];

	memcpy(keep, g->ink, 3);
	memcpy(g->ink, light, 3);
	id_label(g, cx, y, text);
	memcpy(g->ink, keep, 3);
}

/* What is decided so far: portrait, big flag and ability picture. */
static void
identity_draw_base(struct game *g, int flag, int skill)
{
	id_frame(g, 2, 0, OVAL_X0, OVAL_Y0);
	if (flag) {
		id_frame(g, 3, 0, 0, 0);
		id_caption(g, 160, 250, game_word(g, W_NATION,
		    (size_t)g->career.nation));
	}
	if (skill) {
		id_frame(g, 4, 0, 440, 0);
		id_caption(g, 520, 250, game_word(g, W_ABILITY,
		    (size_t)g->career.ability));
	}
}

static int
flag_open(const struct game *g, int slot)
{
	/* A career whose scenario slot is empty (the original's Dutch in
	 * 1560) cannot be chosen. */
	int n = flag_nation[slot];

	return (g->careers[scenario_career(g->career.era, n == NATION_SPAIN ?
	    SCN_SPAIN : (enum scenario_nation)(n - 1))] != 0);
}

static int
id_hit_flag(const struct game *g, int x, int y)
{
	int i;

	for (i = 0; i < FLAG_SLOTS; i++)
		if (flag_open(g, i) && x >= flag_rect[i].x0 &&
		    x <= flag_rect[i].x1 && y >= flag_rect[i].y0 &&
		    y <= flag_rect[i].y1)
			return (i);
	return (-1);
}

static void
nation_enter(struct game *g)
{
	identity_enter(g);
	load_bank(g, "spr/lilflgs", 0, &g->id_spr[0]);
	load_bank(g, captain_portrait[0], 0, &g->id_spr[2]);
}

static void
nation_choose(struct game *g, int slot)
{
	g->career.nation = flag_nation[slot];
	load_bank(g, nation_flag[g->career.nation & 3], 0, &g->id_spr[3]);
	game_switch(g, &screen_ability);
}

/* The keyboard steps through the open slots (codes 2 and 4). */
static int
id_step(int key, int n, int d, int (*open)(const struct game *, int),
    const struct game *g)
{
	int i;

	for (i = 0; i < n; i++) {
		key = key < 0 ? (d > 0 ? 0 : n - 1) : (key + d + n) % n;
		if (open == NULL || open(g, key))
			break;
	}
	return (key);
}

static void
nation_event(struct game *g, const struct ui_event *ev)
{
	int i;

	switch (ev->type) {
	case EV_MOVE:
		g->id_key = id_hit_flag(g, ev->x, ev->y);
		break;
	case EV_CLICK:
		if ((i = id_hit_flag(g, ev->x, ev->y)) >= 0)
			nation_choose(g, i);
		break;
	case EV_KEY:
		if (ev->key == UI_KEY_RIGHT || ev->key == UI_KEY_DOWN)
			g->id_key = id_step(g->id_key, FLAG_SLOTS, 1, flag_open,
			    g);
		else if (ev->key == UI_KEY_LEFT || ev->key == UI_KEY_UP)
			g->id_key = id_step(g->id_key, FLAG_SLOTS, -1,
			    flag_open, g);
		else if (ev->key == UI_KEY_ENTER && g->id_key >= 0)
			nation_choose(g, g->id_key);
		else if (ev->key == UI_KEY_ESCAPE)
			game_goto(g, &screen_era);
		break;
	default:
		break;
	}
}

static void
nation_draw(struct game *g)
{
	int i;

	identity_draw_base(g, 0, 0);
	for (i = 0; i < FLAG_SLOTS; i++)
		if (flag_open(g, i))
			id_frame(g, 0, i, flag_rect[i].x0, flag_rect[i].y0);
	draw(g, FONT_BODY, 180, 316, 280, game_str(g, S_NATION_PROMPT), 0);
	if (g->id_key >= 0)
		id_label(g, 320, ID_LABEL_Y, game_word(g, W_COLORS,
		    (size_t)g->id_key));
}

const struct screen screen_nation = {
	"nation", nation_enter, nation_event, NULL, nation_draw,
};

/* Special ability: five icons along the bottom. */

static int
id_hit_skill(int x, int y)
{
	int i;

	for (i = 0; i < SKILL_SLOTS; i++)
		if (x >= skill_rect[i].x0 && x <= skill_rect[i].x1 &&
		    y >= skill_rect[i].y0 && y <= skill_rect[i].y1)
			return (i);
	return (-1);
}

static void
ability_enter(struct game *g)
{
	g->id_key = -1;
	load_bank(g, "spr/lilskls", 0, &g->id_spr[1]);
	if (g->id_spr[2].count == 0)
		load_bank(g, captain_portrait[0], 0, &g->id_spr[2]);
	if (g->career.expedition >= 0 || g->id_spr[3].count == 0) {
		identity_enter(g);
		load_bank(g, nation_flag[g->career.nation & 3], 0,
		    &g->id_spr[3]);
	}
}

static void
ability_choose(struct game *g, int i)
{
	g->career.ability = i;
	load_bank(g, ability_portrait[i], 0, &g->id_spr[4]);
	game_switch(g, &screen_difficulty);
}

static void
ability_event(struct game *g, const struct ui_event *ev)
{
	int i;

	switch (ev->type) {
	case EV_MOVE:
		g->id_key = id_hit_skill(ev->x, ev->y);
		break;
	case EV_CLICK:
		if ((i = id_hit_skill(ev->x, ev->y)) >= 0)
			ability_choose(g, i);
		break;
	case EV_KEY:
		if (ev->key == UI_KEY_RIGHT || ev->key == UI_KEY_DOWN)
			g->id_key = id_step(g->id_key, SKILL_SLOTS, 1, NULL, g);
		else if (ev->key == UI_KEY_LEFT || ev->key == UI_KEY_UP)
			g->id_key = id_step(g->id_key, SKILL_SLOTS, -1, NULL,
			    g);
		else if (ev->key == UI_KEY_ENTER && g->id_key >= 0)
			ability_choose(g, g->id_key);
		else if (ev->key == UI_KEY_ESCAPE)
			game_goto(g, g->career.expedition >= 0 ?
			    &screen_expedition : &screen_nation);
		break;
	default:
		break;
	}
}

static void
ability_draw(struct game *g)
{
	int i;

	identity_draw_base(g, 1, 0);
	draw(g, FONT_BODY, 180, 316, 280, game_str(g, S_ABILITY_PROMPT), 0);
	for (i = 0; i < SKILL_SLOTS; i++)
		id_frame(g, 1, skill_frame[i], skill_rect[i].x0,
		    skill_rect[i].y0);
	if (g->id_key >= 0)
		id_label(g, 320, ID_LABEL_Y, game_word(g, W_ABILITY,
		    (size_t)g->id_key));
}

const struct screen screen_ability = {
	"ability", ability_enter, ability_event, NULL, ability_draw,
};

/* Difficulty: four levels in a row; the portrait follows. */

static void
difficulty_show(struct game *g, int i)
{
	g->id_key = i;
	load_bank(g, captain_portrait[i & 3], 0, &g->id_spr[2]);
}

static int
id_hit_level(int x, int y)
{
	static const int x1[4] = { 199, 295, 423, 479 };
	int i;

	for (i = 0; i < 4; i++)
		if (x >= level_x[i] && x <= x1[i] && y >= LEVEL_Y &&
		    y <= LEVEL_Y + LEVEL_H)
			return (i);
	return (-1);
}

static void
difficulty_enter(struct game *g)
{
	difficulty_show(g, 0);
}

static void
difficulty_choose(struct game *g, int i)
{
	g->career.difficulty = i;
	game_switch(g, &screen_name);
}

static void
difficulty_event(struct game *g, const struct ui_event *ev)
{
	int i;

	switch (ev->type) {
	case EV_MOVE:
		if ((i = id_hit_level(ev->x, ev->y)) >= 0 && i != g->id_key)
			difficulty_show(g, i);
		break;
	case EV_CLICK:
		if ((i = id_hit_level(ev->x, ev->y)) >= 0)
			difficulty_choose(g, i);
		break;
	case EV_KEY:
		if (ev->key == UI_KEY_RIGHT || ev->key == UI_KEY_DOWN)
			difficulty_show(g, (g->id_key + 1) & 3);
		else if (ev->key == UI_KEY_LEFT || ev->key == UI_KEY_UP)
			difficulty_show(g, (g->id_key + 3) & 3);
		else if (ev->key == UI_KEY_ENTER)
			difficulty_choose(g, g->id_key);
		else if (ev->key == UI_KEY_ESCAPE)
			game_switch(g, &screen_ability);
		break;
	default:
		break;
	}
}

static void
difficulty_draw(struct game *g)
{
	uint8_t keep[3];
	int i;

	identity_draw_base(g, 1, 1);
	id_label(g, 320, 316, game_str(g, S_DIFFICULTY_PROMPT));
	for (i = 0; i < 4; i++) {
		memcpy(keep, g->ink, 3);
		if (i == g->id_key)
			memcpy(g->ink, g->highlight, 3);
		draw(g, FONT_BODY, level_x[i], LEVEL_Y, 140,
		    game_word(g, W_DIFFICULTY, (size_t)i), 0);
		memcpy(g->ink, keep, 3);
	}
	id_label(g, 320, 380, game_word(g, W_DIFF_TEXT, (size_t)g->id_key));
}

const struct screen screen_difficulty = {
	"difficulty", difficulty_enter, difficulty_event, NULL,
	difficulty_draw,
};

/*
 * Name entry, last, under the chosen portrait (VA 0x155fa, 0x157f4): the
 * field starts as "Incognito" (CSTN 92.4), which Enter or a click
 * accepts; the first letter typed replaces it.  At most 11 characters.
 */
#define W_NAMES			92
#define NAME_DEFAULT		4
#define NAME_CHARS		11

static void
name_enter(struct game *g)
{
	snprintf(g->career.name, sizeof(g->career.name), "%s",
	    game_word(g, W_NAMES, NAME_DEFAULT));
	g->name_default = 1;
}

static void
name_event(struct game *g, const struct ui_event *ev)
{
	size_t len = strlen(g->career.name);

	if (ev->type == EV_CHAR && ev->ch >= 0x20 && ev->ch < 0x7F) {
		if (g->name_default) {
			g->career.name[0] = '\0';
			g->name_default = 0;
			len = 0;
		}
		if (len < NAME_CHARS && len < NAME_MAX_LEN) {
			g->career.name[len] = (char)ev->ch;
			g->career.name[len + 1] = '\0';
		}
	} else if (ev->type == EV_KEY && ev->key == UI_KEY_BACKSPACE &&
	    !g->name_default && len > 0)
		g->career.name[len - 1] = '\0';
	else if ((ev->type == EV_CLICK || (ev->type == EV_KEY &&
	    ev->key == UI_KEY_ENTER)) && len > 0)
		game_switch(g, &screen_confirm);
	else if (ev->type == EV_KEY && ev->key == UI_KEY_ESCAPE)
		game_switch(g, &screen_difficulty);
}

static void
name_draw(struct game *g)
{
	char buf[NAME_MAX_LEN * 2 + 4];

	identity_draw_base(g, 1, 1);
	id_label(g, 320, 322, game_str(g, S_NAME_PROMPT));
	/* A blinking cursor after the name typed so far. */
	snprintf(buf, sizeof(buf), "%s%s", g->career.name,
	    !g->name_default && (g->ticks / 30) % 2 ? "_" : "");
	id_label(g, 320, 352, buf);
}

const struct screen screen_name = {
	"name", name_enter, name_event, NULL, name_draw,
};

/*
 * Confirmation (VA 0x15498): "Are ye satisfied with your choices,
 * Captain?" (CSTR 273), "Yes" or "Nay" on the scroll.  Nay starts the
 * identity over from the nationality (an expedition: the ability).
 */
#define S_SATISFIED		273
#define S_YES			274
#define S_NAY			275

/*
 * The Y/N prompts (quit, disembark) take Y and N in every language (the
 * keys at VA 0x31610).  The engine also takes the first letter of the
 * language's "Yes", O in French and J in German, unless it is N.
 */
int
game_yes(const struct game *g, const struct ui_event *ev)
{
	int c = (unsigned char)game_str(g, S_YES)[0];

	if (ev->type != EV_CHAR)
		return (0);
	if (c >= 'a' && c <= 'z')
		c -= 'a' - 'A';
	return (ev->ch == 'y' || ev->ch == 'Y' || (c >= 'A' && c <= 'Z' &&
	    c != 'N' && (ev->ch == c || ev->ch == c + ('a' - 'A'))));
}

static const struct { int x0, y0, x1, y1; } confirm_rect[2] = {
	{ 200, 368, 240, 399 }, { 380, 368, 440, 399 },
};

static void
confirm_enter(struct game *g)
{
	g->id_key = 0;
}

static void
confirm_choose(struct game *g, int i)
{
	if (i == 0) {
		/* The career starts now; its story comes first. */
		if (game_start_career(g) != 0) {
			snprintf(g->err, sizeof(g->err),
			    "cannot start the career");
			game_goto(g, &screen_summary);
			return;
		}
		g->story_pos = 0;
		game_goto(g, &screen_story);
		return;
	}
	bank_free(&g->id_spr[4]);
	if (g->career.expedition >= 0)
		game_switch(g, &screen_ability);
	else {
		bank_free(&g->id_spr[3]);
		load_bank(g, captain_portrait[0], 0, &g->id_spr[2]);
		game_switch(g, &screen_nation);
	}
}

static void
confirm_event(struct game *g, const struct ui_event *ev)
{
	int i;

	switch (ev->type) {
	case EV_MOVE:
	case EV_CLICK:
		for (i = 0; i < 2; i++)
			if (ev->x >= confirm_rect[i].x0 &&
			    ev->x <= confirm_rect[i].x1 &&
			    ev->y >= confirm_rect[i].y0 &&
			    ev->y <= confirm_rect[i].y1) {
				g->id_key = i;
				if (ev->type == EV_CLICK)
					confirm_choose(g, i);
				break;
			}
		break;
	case EV_KEY:
		if (ev->key == UI_KEY_LEFT || ev->key == UI_KEY_RIGHT ||
		    ev->key == UI_KEY_UP || ev->key == UI_KEY_DOWN)
			g->id_key ^= 1;
		else if (ev->key == UI_KEY_ENTER)
			confirm_choose(g, g->id_key);
		else if (ev->key == UI_KEY_ESCAPE)
			game_switch(g, &screen_name);
		break;
	default:
		break;
	}
}

static void
confirm_draw(struct game *g)
{
	uint8_t keep[3];

	identity_draw_base(g, 1, 1);
	draw(g, FONT_BODY, 180, 316, 280, game_str(g, S_SATISFIED), 0);
	memcpy(keep, g->ink, 3);
	if (g->id_key == 0)
		memcpy(g->ink, g->highlight, 3);
	draw(g, FONT_BODY, 208, 368, 100, game_str(g, S_YES), 0);
	memcpy(g->ink, keep, 3);
	if (g->id_key == 1)
		memcpy(g->ink, g->highlight, 3);
	draw(g, FONT_BODY, 390, 368, 100, game_str(g, S_NAY), 0);
	memcpy(g->ink, keep, 3);
}

const struct screen screen_confirm = {
	"confirm", confirm_enter, confirm_event, NULL, confirm_draw,
};

/*
 * The career's story (VA 0x139e2): on the frame picture with the
 * captain's portrait, pages of text from the career's steps (struct
 * ui_story_step, from data/stories or the executable): a text, a page
 * turned or a key waited for, the sword fight.
 */
#define STORY_TEXT_Y		340
#define STORY_INK		2
#define STORY_HIGHLIGHT		6

/*
 * The colour of palette entry "entry" of the background, for its inks.
 * A pack's picture without the original has the fixed palette, whose
 * entries 2 and 6 are dark blues: the engine's own orange ink and pale
 * yellow highlight stand in for the original's.
 */
static const uint8_t *
bg_colour(const struct game *g, int entry)
{
	static const uint8_t ink[3] = { 0xf0, 0x98, 0x38 };
	static const uint8_t highlight[3] = { 0xff, 0xe8, 0xa0 };

	if (image_has_fixed_palette(&g->bg)) {
		if (entry == STORY_INK)
			return (ink);
		if (entry == STORY_HIGHLIGHT)
			return (highlight);
	}
	return (&g->bg.palette[entry * 3]);
}
#define S_TREASURE_FLEET	46

/* The story of this career: its scenario's index. */
static int
story_index(const struct game *g)
{
	return (g->career.expedition >= 0 ? SCENARIO_CAREERS +
	    g->career.expedition : scenario_career(g->career.era,
	    (enum scenario_nation)(g->career.nation == NATION_SPAIN ? 3 :
	    g->career.nation - 1)));
}

/*
 * After the story, the sailing master, except for a famous expedition
 * (VA 0x13348, flag at VA 0x45e84): it starts at sea, in no town.
 */
static void
story_end(struct game *g)
{
	game_goto(g, g->career.expedition >= 0 ? &screen_sail :
	    &screen_master);
}

/* Read on to the next stop: a page to wait on, a fight, or the end. */
static void
story_advance(struct game *g)
{
	const struct ui_story_step *st;
	int n = story_index(g);

	g->story_waiting = 0;
	while (n >= 0 && n < UI_STORIES && g->story_pos < g->ui.nstory[n]) {
		st = &g->ui.story[n][g->story_pos++];
		switch (st->op) {
		case UI_STORY_PAGE:
		case UI_STORY_WAIT:
			g->story_waiting = st->op == UI_STORY_PAGE ? 1 : 2;
			return;
		case UI_STORY_FIGHT:
			g->sw_story = 1;
			game_goto(g, &screen_sword);
			return;
		default:
			g->story_text = st->text;
			break;
		}
	}
	story_end(g);
}

static void
story_enter(struct game *g)
{
	game_background(g, "pic/frame");
	game_portrait(g, NULL);
	load_bank(g, captain_portrait[g->career.difficulty & 3], 0,
	    &g->id_spr[2]);
	/* The game's ink here is palette entry 2 (VA 0x159df, table at VA
	 * 0x3421c), an orange in this picture. */
	if (g->bg.pixels != NULL)
		memcpy(g->ink, bg_colour(g, STORY_INK), 3);
	if (g->story_pos == 0)
		g->story_text = -1;
	story_advance(g);
}

static void
story_event(struct game *g, const struct ui_event *ev)
{
	/* Escape skips the story: an engine convenience. */
	if (ev->type == EV_KEY && ev->key == UI_KEY_ESCAPE) {
		story_end(g);
		return;
	}
	if (!g->story_waiting || !(ev->type == EV_CLICK ||
	    (ev->type == EV_KEY && ev->key == UI_KEY_ENTER)))
		return;
	if (g->story_waiting == 1)
		g->story_text = -1;		/* a new page */
	story_advance(g);
}

static void
story_draw(struct game *g)
{
	const char *arg[4];
	char text[1024], year[16];
	const struct sim_town *t;

	id_frame(g, 2, 0, 236, 58);
	if (g->story_text < 0)
		return;
	t = g->sim.ntowns > 0 ? &g->sim.towns[g->sim.fleet.leaving >= 0 ?
	    g->sim.fleet.leaving : 0] : NULL;
	snprintf(year, sizeof(year), "%d", g->sim.era_year);
	arg[0] = game_str(g, S_TREASURE_FLEET);
	arg[1] = t != NULL ? t->name : "";
	arg[2] = year;
	arg[3] = g->sim.player.name;
	fill_args(text, sizeof(text), game_str(g, g->story_text), arg, 4);
	draw(g, FONT_BODY, 40, STORY_TEXT_Y, 580, text, 0);
	if (g->story_waiting)
		continue_prompt(g);
}

const struct screen screen_story = {
	"story", story_enter, story_event, NULL, story_draw,
};

/*
 * The sailing master (VA 0x13e3f), when the story is over: on his
 * picture, "Here in <town>, you'll probably be wantin' to see the
 * <nation> governor..." (CSTR 68), then, for every other nation, its
 * nearest town (CSTR 69) by distance on the map, "a few" (CSTR 48)
 * days away under 31 or "many" (49), and in which direction (CSTN 75).
 * Then the career begins in the start town.
 */
#define S_MASTER		68
#define S_CLOSEST		69
#define S_A_FEW			48
#define S_MANY			49
#define W_DIRECTION		75
#define FEW_DAYS		31

static void
master_enter(struct game *g)
{
	game_background(g, "pic/smaster");
	game_portrait(g, NULL);
	g->ink[0] = 240;
	g->ink[1] = 220;
	g->ink[2] = 170;
	g->master_page = 0;
}

/*
 * Replace a text's last character (a line break) with the quote that
 * closes the one the text opens with: a pack may quote in its
 * language's style, the original only with '"'.
 */
static void
close_quote(char *text, size_t len)
{
	static const char *const pair[][2] = {
		{ "\xC2\xAB", " \xC2\xBB" },		/* « » */
		{ "\xC2\xBB", "\xC2\xAB" },		/* » « */
		{ "\xE2\x80\x9C", "\xE2\x80\x9D" },	/* “ ” */
		{ "\xE2\x80\x9E", "\xE2\x80\x9C" },	/* „ “ */
	};
	const char *close = "\"";
	size_t i, n = strlen(text);

	if (n == 0)
		return;
	for (i = 0; i < sizeof(pair) / sizeof(pair[0]); i++)
		if (strncmp(text, pair[i][0], strlen(pair[i][0])) == 0)
			close = pair[i][1];
	text[n - 1] = '\0';
	strncat(text, close, len - n);
}

static void
master_text(struct game *g, int page, char *out, size_t len)
{
	const struct sim *sm = &g->sim;
	const struct sim_town *here_t, *t;
	int best[4], dist[4], n, i, d, dx, dy, dir;
	const char *arg[4];
	char line[400], days[32];

	out[0] = '\0';
	if (sm->fleet.leaving < 0 || (size_t)sm->fleet.leaving >= sm->ntowns)
		return;
	here_t = &sm->towns[sm->fleet.leaving];
	if (page == 0) {
		fill(out, len, game_str(g, S_MASTER), here_t->name,
		    game_word(g, W_NATION, here_t->nation & 3));
		close_quote(out, len);
		return;
	}
	for (n = 0; n < 4; n++) {
		best[n] = -1;
		dist[n] = 0x7FFF;
	}
	for (i = 0; i < (int)sm->ntowns; i++) {
		t = &sm->towns[i];
		if (i == sm->fleet.leaving || t->nation > 3)
			continue;
		d = abs(t->x - here_t->x) + abs(t->y - here_t->y);
		if (d < dist[t->nation]) {
			dist[t->nation] = d;
			best[t->nation] = i;
		}
	}
	for (n = 0; n < 4; n++) {
		if (n == here_t->nation || best[n] < 0)
			continue;
		t = &sm->towns[best[n]];
		dx = t->x - here_t->x;
		dy = t->y - here_t->y;
		if (abs(dy) * 2 < abs(dx))
			dy = 0;
		if (abs(dx) * 2 < abs(dy))
			dx = 0;
		dir = dy < 0 ? (dx < 0 ? 4 : dx == 0 ? 0 : 5) :
		    dy == 0 ? (dx < 0 ? 3 : dx == 0 ? 8 : 2) :
		    (dx < 0 ? 6 : dx == 0 ? 1 : 7);
		snprintf(days, sizeof(days), "%s", game_str(g,
		    abs(dx) + abs(dy) < FEW_DAYS ? S_A_FEW : S_MANY));
		arg[0] = game_word(g, W_NATION, (size_t)n);
		arg[1] = t->name;
		arg[2] = days;
		arg[3] = game_word(g, W_DIRECTION, (size_t)dir);
		fill_args(line, sizeof(line), game_str(g, S_CLOSEST), arg, 4);
		strncat(out, line, len - strlen(out) - 1);
	}
	close_quote(out, len);
}

static void
master_event(struct game *g, const struct ui_event *ev)
{
	if (!(ev->type == EV_CLICK || (ev->type == EV_KEY &&
	    (ev->key == UI_KEY_ENTER || ev->key == UI_KEY_ESCAPE))))
		return;
	if (g->master_page == 0) {
		g->master_page = 1;
		g->idle = 0;
		return;
	}
	/* The career begins in the start town. */
	sim_enter_town(&g->sim, g->sim.fleet.leaving);
	g->town_free = 0;
	game_goto(g, &screen_town);
}

static void
master_draw(struct game *g)
{
	char text[2048];

	master_text(g, g->master_page, text, sizeof(text));
	draw(g, FONT_BODY, 340, 100, 280, text, 0);
	continue_prompt(g);
}

const struct screen screen_master = {
	"master", master_enter, master_event, NULL, master_draw,
};

/* Summary: the career is created; the voyage itself is the next phase. */

static void
summary_enter(struct game *g)
{
	parchment_enter(g);
	if (!g->started && g->err[0] == '\0' && game_start_career(g) != 0)
		snprintf(g->err, sizeof(g->err), "cannot start the career");
}

static void
summary_event(struct game *g, const struct ui_event *ev)
{
	if (g->started && (ev->type == EV_CLICK ||
	    (ev->type == EV_KEY && ev->key == UI_KEY_ENTER)))
		game_goto(g, &screen_cabin);
	else if (!g->started && ev->type == EV_KEY)
		game_goto(g, &screen_welcome);
}

static void
summary_draw(struct game *g)
{
	char buf[256];
	int y = 60;

	if (!g->started) {
		draw_centered(g, FONT_TITLE, 200, g->err);
		return;
	}
	draw_centered(g, FONT_TITLE, y, g->career.name);
	y += 40;
	draw_centered(g, FONT_TITLE, y, g->career.title);
	y += 50;
	snprintf(buf, sizeof(buf), "%s, %s",
	    game_word(g, W_DIFFICULTY, (size_t)g->career.difficulty),
	    game_word(g, W_ERA, (size_t)g->career.era));
	draw_centered(g, FONT_BODY, y, buf);
	y += 30;
	draw_centered(g, FONT_BODY, y,
	    game_word(g, W_ABILITY, (size_t)g->career.ability));
	continue_prompt(g);
}

const struct screen screen_summary = {
	"summary", summary_enter, summary_event, NULL, summary_draw,
};

/*
 * Replace each %s of a log sentence with the next argument, as the
 * game's printf-style strings expect, and each %1$s to %9$s with that
 * argument (an engine extension, so that a pack's text can reorder
 * them).
 */
void
fill_args(char *out, size_t len, const char *fmt, const char *const *arg,
    int nargs)
{
	const char *v;
	size_t n = 0;
	int k = 0, i;

	while (*fmt != '\0' && n + 1 < len) {
		if (fmt[0] == '%' && fmt[1] == 's') {
			i = k++;
			fmt += 2;
		} else if (fmt[0] == '%' && fmt[1] >= '1' && fmt[1] <= '9' &&
		    fmt[2] == '$' && fmt[3] == 's') {
			i = fmt[1] - '1';
			fmt += 4;
		} else {
			out[n++] = *fmt++;
			continue;
		}
		v = i < nargs && arg[i] != NULL ? arg[i] : "";
		while (*v != '\0' && n + 1 < len)
			out[n++] = *v++;
	}
	out[n] = '\0';
}

static void
fill(char *out, size_t len, const char *fmt, const char *a, const char *b)
{
	const char *arg[2] = { a, b };

	fill_args(out, len, fmt, arg, 2);
}

static const char *
town_name(const struct game *g, int i)
{
	return (i >= 0 && (size_t)i < g->sim.ntowns ? g->sim.towns[i].name :
	    "");
}

/* A news item as the game's log words it. */
/*
 * The latest news others tell (VA 0x1e040), newest first, at most "max":
 * world news only (codes 10-19); a town the player took reads as a
 * capture, a town plundered as pirates plundering.
 */
static size_t
public_news(const struct game *g, struct sim_news *out, size_t max)
{
	size_t kept = sim_news_kept(&g->sim), i, n = 0;
	struct sim_news e;

	for (i = kept; i > 0 && n < max; i--) {
		e = *sim_news_at(&g->sim, i - 1);
		if (e.code == NEWS_TAKEN) {
			e.code = NEWS_CAPTURE;
			e.a = e.b;
			e.b = sim_news_at(&g->sim, i - 1)->a;
		} else if (e.code == NEWS_PLUNDERED)
			e.code = NEWS_PLUNDER;
		if (e.code < 10 || e.code > 19)
			continue;
		out[n++] = e;
	}
	return (n);
}

static void	news_body(const struct game *, const struct sim_news *, char *,
		    size_t);

static void
news_text(const struct game *g, const struct sim_news *n, char *out,
    size_t len)
{
	const char *arg[3];
	char text[256], day[8];

	/* "Jan 16:  ..." (CSTN 20.0, VA 0x1e144). */
	news_body(g, n, text, sizeof(text));
	snprintf(day, sizeof(day), "%d", n->date % 30 + 1);
	arg[0] = game_word(g, W_MONTH_SHORT, (size_t)((n->date % 360) / 30));
	arg[1] = day;
	arg[2] = text;
	fill_args(out, len, game_word(g, W_LOG, 0), arg, 3);
}

static void
news_body(const struct game *g, const struct sim_news *n, char *out,
    size_t len)
{
	const char *fmt = game_word(g, W_LOG, (size_t)n->code);
	char name[80];

	switch (n->code) {
	case NEWS_TITLED:
		/* "an English" "Captain" (VA 0x10b10). */
		fill(out, len, fmt, game_word(g, 68, (size_t)(n->a / 64) % 5),
		    game_word(g, 78, (size_t)n->a & 63));
		break;
	case NEWS_CAPTURED:
	case NEWS_ENGAGED:
		/* "a Spanish" "Galleon": the original's tolower at VA
		 * 0x1e2a8 is discarded. */
		fill(out, len, fmt, game_word(g, 68, (size_t)(n->a / 16) % 5),
		    game_word(g, 8, (size_t)n->a & 15));
		break;
	case NEWS_WAR:
	case NEWS_PEACE:
	case NEWS_ALLIANCE:
		fill(out, len, fmt, game_word(g, W_NATION_NAME, (size_t)n->a & 3),
		    game_word(g, W_NATION_NAME, (size_t)n->b & 3));
		break;
	case NEWS_AMNESTY:
		fill(out, len, fmt, game_word(g, W_NATION_NAME,
		    (size_t)n->a & 3), NULL);
		break;
	case NEWS_CAPTURE:
		fill(out, len, fmt, game_word(g, W_NATION_NAME,
		    (size_t)n->a & 3), town_name(g, n->b));
		break;
	case NEWS_TAKEN:
		/* The town, then the nation it was taken for (VA 0x1e407). */
		fill(out, len, fmt, town_name(g, n->a), game_word(g,
		    W_NATION_NAME, (size_t)n->b & 3));
		break;
	case NEWS_VILLAIN:
		villain_name(g, n->a, name, sizeof(name));
		fill(out, len, fmt, name, town_name(g, n->b));
		break;
	case NEWS_RESCUED:
		fill(out, len, fmt, game_word(g, 37, (size_t)n->a & 3), NULL);
		break;
	case NEWS_PIRATE_OFF:
	case NEWS_PIRATE_TAKEN:
		captain_name(g, n->a, name, sizeof(name));
		fill(out, len, fmt, name, town_name(g, n->b));
		break;
	default:
		fill(out, len, fmt, town_name(g, n->a), NULL);
	}
}

static void
date_text(const struct game *g, char *out, size_t len)
{
	snprintf(out, len, "%s %d", game_word(g, W_MONTH,
	    (size_t)(g->sim.day_of_year / 30)), g->sim.year);
}

/*
 * The log book (cabin spot 4, VA 0xc46e), on shiplog.pcx in ink entry
 * 0: the date (CSTR 93: month, year) centred at (320, 20), "LOG BOOK"
 * (CSTN 95.6, PIRATE4) at (320, 50), and the menu (CSTR 312) at (250,
 * 100) up to x 500.  Each page has its title (CSTN 95) centred at (320,
 * 40) in PIRATE4 and waits for a click or a key:
 * - Ship's Log (VA 0x1fc0f): every log entry (codes 1-36), 10 a leaf
 *   at (140, 100) up to x 520 (none: the title alone, VA 0x1fce0);
 *   the arrows of the picture (200, 420)-(240, 450) and (380, 420)-(420,
 *   450) turn the leaves, the bookmark (0, 360)-(100, 479) leaves.
 * - Party Status (VA 0xa2ca): CSTR 104 at (160, 100) up to x 560.
 * - Personal Status (VA 0xa5a7): CSTR 105 at (140, 100) up to x 560.
 * - Ship Status: the fleet panel, only to look (VA 0xffae mode 0).
 * - Hostages (VA 0xa450): the prisoners and the pirate brought in, or
 *   "NONE" (CSTN 92.9), at (240, 100) up to x 560.
 */
#define S_LOG_DATE		93
#define S_LOG_MENU		312
#define S_PARTY			104
#define S_PERSONAL		105
#define S_STANDING_LINE		90
#define S_MARRIED		91
#define S_SINGLE		285
#define W_LOG_TITLE		95
#define LOG_TITLE_BOOK		6
#define W_STANDING		21	/* Letter of Marque, (no title)... */
#define W_HOSTAGE		22
#define W_REPUTATION		34
#define W_DIFFICULTY		35
#define W_HEALTH		36
#define S_HIDES			87	/* the special cargo's name by era, 3 */
#define LOG_LEAF		10
enum { LOG_MENU, LOG_SHIPS, LOG_PARTY, LOG_PERSONAL, LOG_STATUS,
    LOG_HOSTAGES, LOG_EXIT };

static void
logbook_enter(struct game *g)
{
	game_background(g, "pic/shiplog");
	game_portrait(g, NULL);
	if (g->bg.pixels != NULL)
		memcpy(g->ink, g->bg.palette, 3);
	g->log_page = LOG_MENU;
	g->selected = 0;
}

/* The log's entries a leaf at a time (VA 0x1fa07: codes 1-36). */
static size_t
log_entries(const struct game *g, const struct sim_news **out, size_t max)
{
	size_t kept = sim_news_kept(&g->sim), i, n = 0;
	const struct sim_news *e;

	for (i = 0; i < kept && n < max; i++) {
		e = sim_news_at(&g->sim, i);
		if (e->code > 0 && e->code < 37)
			out[n++] = e;
	}
	return (n);
}

static void
logbook_choose(struct game *g, int i)
{
	switch (i) {
	case LOG_SHIPS - 1:
	case LOG_PARTY - 1:
	case LOG_PERSONAL - 1:
	case LOG_HOSTAGES - 1:
		g->log_page = i + 1;
		g->log_leaf = 0;
		break;
	case LOG_STATUS - 1:
		g->panel_view = 1;
		game_goto(g, &screen_fleet_panel);
		break;
	default:
		game_goto(g, &screen_cabin);
		break;
	}
}

static void
logbook_event(struct game *g, const struct ui_event *ev)
{
	const struct sim_news *e[SIM_NEWS_MAX];
	size_t n;
	int x = ev->x, y = ev->y;

	if (g->log_page == LOG_MENU) {
		menu_event(g, ev, 6, NULL, logbook_choose, &screen_cabin);
		return;
	}
	if (g->log_page == LOG_SHIPS) {
		n = log_entries(g, e, SIM_NEWS_MAX);
		if ((ev->type == EV_CLICK && x >= 200 && x <= 240 && y >= 420 &&
		    y <= 450) || (ev->type == EV_KEY && ev->key == UI_KEY_LEFT)) {
			if (g->log_leaf > 0)
				g->log_leaf--;
			return;
		}
		if ((ev->type == EV_CLICK && x >= 380 && x <= 420 && y >= 420 &&
		    y <= 450) || (ev->type == EV_KEY && ev->key == UI_KEY_RIGHT)) {
			if ((size_t)(g->log_leaf + 1) * LOG_LEAF < n)
				g->log_leaf++;
			return;
		}
		if (ev->type == EV_CLICK && !(x <= 100 && y >= 360))
			return;		/* only the bookmark leaves */
	}
	if (ev->type == EV_CLICK || (ev->type == EV_KEY &&
	    (ev->key == UI_KEY_ENTER || ev->key == UI_KEY_ESCAPE))) {
		g->log_page = LOG_MENU;
		g->selected = 0;
	}
}

/* "   England Captain": a rank, or how the nation sees the player. */
static void
personal_standings(struct game *g, char *out, size_t len)
{
	const char *arg[2];
	char line[96];
	int k, n, st;

	out[0] = '\0';
	for (k = 1; k <= 4; k++) {
		n = k & 3;			/* England, France, Holland, Spain */
		st = g->sim.standing[n];
		arg[0] = game_word(g, W_NATION, (size_t)n);
		arg[1] = g->sim.rank[n] != 0 ? game_word(g, W_RANK,
		    g->sim.rank[n]) : game_word(g, W_STANDING, st >= 2 ? 0 :
		    st >= 0 ? 1 : st >= -3 ? 2 : 3);
		fill_args(line, sizeof(line), game_str(g, S_STANDING_LINE),
		    arg, 2);
		strncat(out, line, len - strlen(out) - 1);
	}
}

static void
logbook_draw(struct game *g)
{
	const struct sim_fleet *f = &g->sim.fleet;
	const struct sim_news *e[SIM_NEWS_MAX];
	const char *arg[9];
	char text[2048], line[200], a[9][96];
	size_t n, i;
	int k, lvl;

	if (g->log_page == LOG_MENU) {
		date_text(g, line, sizeof(line));
		draw_centered(g, FONT_BODY, 20, line);
		draw_centered(g, FONT_PIRATE4, 50, game_word(g, W_LOG_TITLE,
		    LOG_TITLE_BOOK));
		draw(g, FONT_BODY, 250, 100, 250, game_str(g, S_LOG_MENU), 1);
		return;
	}
	draw_centered(g, FONT_PIRATE4, 40, game_word(g, W_LOG_TITLE,
	    (size_t)(g->log_page - 1)));
	switch (g->log_page) {
	case LOG_SHIPS:
		n = log_entries(g, e, SIM_NEWS_MAX);
		text[0] = '\0';
		for (i = (size_t)g->log_leaf * LOG_LEAF; i < n &&
		    i < (size_t)(g->log_leaf + 1) * LOG_LEAF; i++) {
			news_text(g, e[i], line, sizeof(line));
			strncat(text, line, sizeof(text) - strlen(text) - 2);
			strcat(text, "\r");
		}
		/* No entries: the title alone (VA 0x1fce0). */
		draw(g, FONT_BODY, 140, 100, 380, text, 0);
		return;
	case LOG_PARTY:
		lvl = f->mood / 4 + 1;
		snprintf(a[0], sizeof(a[0]), "%s", g->sim.player.name);
		snprintf(a[1], sizeof(a[1]), "%d", f->crew);
		snprintf(a[2], sizeof(a[2]), "%d", f->guns);
		snprintf(a[3], sizeof(a[3]), "%ld", f->gold);
		snprintf(a[4], sizeof(a[4]), "%d", f->crew > 0 ?
		    (f->food * 256 + f->food_frac) / f->crew : 0);
		snprintf(a[5], sizeof(a[5]), "%s", game_word(g, W_MOOD,
		    (size_t)(lvl < 0 ? 0 : lvl > 3 ? 3 : lvl)));
		snprintf(a[6], sizeof(a[6]), "%d", f->goods);
		/* By the era code (VA 0x1d8b2): hides in 1560 (code
		 * 0), tobacco in 1600 and 1620 (2, 3), sugar after. */
		k = g->world != NULL ? g->world->era : 0;
		snprintf(a[7], sizeof(a[7]), "%s", game_str(g, S_HIDES +
		    (k < 1 ? 0 : k < 3 ? 1 : 2)));
		snprintf(a[8], sizeof(a[8]), "%d", f->sugar);
		for (k = 0; k < 9; k++)
			arg[k] = a[k];
		fill_args(text, sizeof(text), game_str(g, S_PARTY), arg, 9);
		draw(g, FONT_BODY, 160, 100, 400, text, 0);
		return;
	case LOG_PERSONAL:
		personal_standings(g, text, sizeof(text));
		lvl = g->sim.reputation / 4 + 1;
		snprintf(a[0], sizeof(a[0]), "%s", game_str(g,
		    g->sim.married ? S_MARRIED : S_SINGLE));
		snprintf(a[1], sizeof(a[1]), "%d", g->sim.year -
		    g->sim.start_year + 25);
		snprintf(a[2], sizeof(a[2]), "%s", game_word(g, W_HEALTH,
		    (size_t)sim_health(&g->sim)));
		snprintf(a[3], sizeof(a[3]), "%ld", g->sim.fortune);
		snprintf(a[4], sizeof(a[4]), "%d", g->sim.acres * 50);
		snprintf(a[5], sizeof(a[5]), "%s", game_word(g, W_REPUTATION,
		    (size_t)(lvl < 0 ? 0 : lvl > 5 ? 5 : lvl)));
		snprintf(a[6], sizeof(a[6]), "%s", game_word(g, W_DIFFICULTY,
		    (size_t)(g->sim.player.difficulty & 3)));
		arg[0] = text;
		for (k = 0; k < 7; k++)
			arg[k + 1] = a[k];
		{
			char out[2048];

			fill_args(out, sizeof(out), game_str(g, S_PERSONAL),
			    arg, 8);
			draw(g, FONT_BODY, 140, 100, 420, out, 0);
		}
		return;
	case LOG_HOSTAGES:
		text[0] = '\0';
		for (k = 0; k < SIM_PRISONERS; k++) {
			int p = g->sim.prisoners[k];
			char one[160];

			if (p == 0)
				continue;
			captain_name(g, p & 0xFF, line, sizeof(line));
			arg[0] = line;
			fill_args(one, sizeof(one), game_word(g, W_HOSTAGE,
			    p & 0x100 ? 2 : 1), arg, 1);
			strncat(text, one, sizeof(text) - strlen(text) - 2);
			strcat(text, "\r");
		}
		if (g->sim.mission.type == -MISSION_PIRATE) {
			char one[160];

			captain_name(g, g->sim.mission.pirate, line,
			    sizeof(line));
			arg[0] = line;
			fill_args(one, sizeof(one), game_word(g, W_HOSTAGE, 2),
			    arg, 1);
			strncat(text, one, sizeof(text) - strlen(text) - 1);
		}
		if (text[0] == '\0')
			snprintf(text, sizeof(text), "%s", game_word(g,
			    W_UNKNOWN, 9));
		draw(g, FONT_BODY, 240, 100, 320, text, 0);
		return;
	default:
		return;
	}
}

/*
 * The language (cabin key L, VA 0xc6bb): shiplog.pcx in ink entry 0,
 * the menu (CSTR 318: English, French, German, the order of the word
 * at VA 0x33ef0) at (250, 100) up to x 500.  Another language reloads
 * the texts and misc.crs (VA 0xba2, 0xcb0); then back to the cabin.
 * Escape goes back unchanged (an engine choice).  A pack's menu may add
 * a fourth option, Spanish, offered when the packs give its text.
 */
#define S_LANG_MENU		318

/* The menu's options: its "~" lines, at most one per language; the
 * original's three when the text has none. */
static int
language_count(const struct game *g)
{
	const char *s = game_str(g, S_LANG_MENU);
	int n = 0;

	for (; *s != '\0'; s++)
		if (*s == '~' && n < GAME_LANGS)
			n++;
	return (n > 0 ? n : 3);
}

static int
language_enabled(const struct game *g, int i)
{
	return (i < 3 || text_has_lang(g->packs, game_langs[i]));
}

static void
language_enter(struct game *g)
{
	game_background(g, "pic/shiplog");
	game_portrait(g, NULL);
	if (g->bg.pixels != NULL)
		memcpy(g->ink, g->bg.palette, 3);
	g->selected = 0;
}

static void
language_choose(struct game *g, int i)
{
	if (strcmp(g->lang, game_langs[i]) != 0 &&
	    game_set_lang(g, game_langs[i]) != 0)
		snprintf(g->msg, sizeof(g->msg), "%s", g->err);
	game_goto(g, &screen_cabin);
}

static void
language_event(struct game *g, const struct ui_event *ev)
{
	menu_event(g, ev, language_count(g), language_enabled,
	    language_choose, &screen_cabin);
}

static void
language_draw(struct game *g)
{
	draw(g, FONT_BODY, 250, 100, 250, game_str(g, S_LANG_MENU), 1);
}

const struct screen screen_language = {
	"language", language_enter, language_event, NULL, language_draw,
};

const struct screen screen_logbook = {
	"logbook", logbook_enter, logbook_event, NULL, logbook_draw,
};

/*
 * The captain's cabin (VA 0x9ffc): cabin.pcx, with cabin.spr frame 0 at
 * (325, 229) while no treasure map is held, and eleven spots (table at
 * VA 0x327e0: x0, y0, x1, y1, on).  The title, "Captain's Cabin" (CSTN
 * 92.7), is centred at (320, 38) in ink 0; pointing at a spot shows its
 * label there instead (CSTN 86).  Search, Divide party and Join party
 * are off unless a party is ashore.
 */
#define CABIN_SPOTS		11
#define W_CABIN			86
#define W_CABIN_TITLE		92
#define CABIN_TITLE		7
enum { CABIN_CONTINUE, CABIN_MAPS, CABIN_CAVE, CABIN_LOG, CABIN_MAP,
    CABIN_SEARCH, CABIN_DIVIDE, CABIN_JOIN, CABIN_SAVE, CABIN_EXIT,
    CABIN_PRACTICE };

static int
cabin_rect(const struct game *g, int i, int r[5])
{
	int k;

	if (i < 0 || i >= UI_CABIN_SPOTS)
		return (-1);
	for (k = 0; k < 5; k++)
		r[k] = g->ui.cabin[i][k];
	return (0);
}

static void	tmap_show(struct game *, int, const struct screen *);

static int
any_map(const struct game *g)
{
	int i;

	for (i = 0; i < SIM_MAPS; i++)
		if (g->sim.maps[i].pieces != 0)
			return (1);
	return (0);
}

static int
cabin_open(const struct game *g, int i)
{
	int r[5];

	if (cabin_rect(g, i, r) != 0)
		return (0);
	/* Saving only in town, searching only ashore (VA 0xa11c: the
	 * table's flags are set from the arrival mode). */
	if (i == CABIN_SAVE)
		return (g->sim.player_town >= 0 && g->save_dir[0] != '\0');
	if (i == CABIN_SEARCH)
		return (g->sim.fleet.ashore && g->sim.player_town < 0);
	if (i == CABIN_MAPS)
		return (any_map(g));
	return (r[4] != 0);
}

static int
cabin_hit(const struct game *g, int x, int y)
{
	int i, r[5];

	for (i = 0; i < CABIN_SPOTS; i++)
		if (cabin_open(g, i) && cabin_rect(g, i, r) == 0 &&
		    x >= r[0] && x <= r[2] && y >= r[1] && y <= r[3])
			return (i);
	return (-1);
}

static void
cabin_enter(struct game *g)
{
	game_background(g, "pic/cabin");
	game_portrait(g, NULL);
	load_bank(g, "spr/cabin", 0, &g->cabin_spr);
	if (g->bg.pixels != NULL)
		memcpy(g->ink, g->bg.palette, 3);
	g->cabin_spot = -1;
	g->cabin_key = CABIN_CONTINUE;
}

static void
cabin_choose(struct game *g, int i)
{
	const struct screen *next;

	g->msg[0] = '\0';
	switch (i) {
	case CABIN_CONTINUE:
		next = g->sim.player_town >= 0 || g->world == NULL ?
		    &screen_town : &screen_sail;
		g->town_free = next == &screen_town;	/* VA 0x18901 */
		game_goto(g, next);
		break;
	case CABIN_CAVE:
		g->cave_back = &screen_cabin;
		game_goto(g, &screen_cave);
		break;
	case CABIN_LOG:
		game_goto(g, &screen_logbook);
		break;
	case CABIN_MAPS:
		tmap_show(g, -1, &screen_cabin);
		break;
	case CABIN_MAP:
		game_goto(g, &screen_citymap);
		break;
	case CABIN_SEARCH:
		g->dig_i = g->dig_page = 0;
		game_goto(g, &screen_dig);
		break;
	case CABIN_SAVE:
		g->saving = 1;
		game_goto(g, &screen_slots);
		break;
	case CABIN_EXIT:
		/* "Quit (Y/N)" (VA 0x31df, misc.crs frame 4): the game
		 * ends there, without retirement. */
		g->quit_prompt = 1;
		break;
	case CABIN_PRACTICE:
		g->sw_practice = 1;
		game_goto(g, &screen_sword);
		break;
	default:
		snprintf(g->msg, sizeof(g->msg), "(%s: not implemented yet.)",
		    game_word(g, W_CABIN, (size_t)i));
		break;
	}
}

static void
cabin_step(struct game *g, int d)
{
	int n;

	for (n = 0; n < CABIN_SPOTS; n++) {
		g->cabin_key = (g->cabin_key + d + CABIN_SPOTS) % CABIN_SPOTS;
		if (cabin_open(g, g->cabin_key))
			break;
	}
	g->cabin_spot = g->cabin_key;
}

static void
cabin_event(struct game *g, const struct ui_event *ev)
{
	int i;

	if (g->quit_prompt) {
		/* Yes quits to the welcome menu, N or Escape stays. */
		if (game_yes(g, ev)) {
			g->quit_prompt = 0;
			g->started = 0;
			game_goto(g, &screen_welcome);
		} else if ((ev->type == EV_CHAR && (ev->ch == 'n' ||
		    ev->ch == 'N')) || (ev->type == EV_KEY &&
		    ev->key == UI_KEY_ESCAPE))
			g->quit_prompt = 0;
		return;
	}
	switch (ev->type) {
	case EV_MOVE:
		g->cabin_spot = cabin_hit(g, ev->x, ev->y);
		if (g->cabin_spot >= 0)
			g->cabin_key = g->cabin_spot;
		break;
	case EV_CLICK:
		if ((i = cabin_hit(g, ev->x, ev->y)) >= 0)
			cabin_choose(g, i);
		break;
	case EV_KEY:
		if (ev->key == UI_KEY_LEFT || ev->key == UI_KEY_UP)
			cabin_step(g, -1);
		else if (ev->key == UI_KEY_RIGHT || ev->key == UI_KEY_DOWN)
			cabin_step(g, 1);
		else if (ev->key == UI_KEY_ENTER)
			cabin_choose(g, g->cabin_key);
		else if (ev->key == UI_KEY_ESCAPE)
			cabin_choose(g, CABIN_CONTINUE);
		break;
	case EV_CHAR:
		/* Space continues too (VA 0xb3a5, keys at VA 0x32a60); L
		 * picks the language (VA 0xb3cd). */
		if (ev->ch == ' ')
			cabin_choose(g, CABIN_CONTINUE);
		else if (ev->ch == 'l' || ev->ch == 'L')
			game_goto(g, &screen_language);
		break;
	default:
		break;
	}
}

static void
cabin_draw(struct game *g)
{
	const char *label;
	int w;

	/* The chart on the table covers the maps' spot while there are
	 * none (VA 0xa0d3). */
	if (g->cabin_spr.count > 0 && !any_map(g))
		fb_frame(&g->fb, &g->cabin_spr.frames[0], 325, 229);
	/* The prompt over the chart, which it overlaps. */
	if (g->quit_prompt && g->misc.count > QUIT_FRAME)
		fb_frame(&g->fb, &g->misc.frames[QUIT_FRAME], 268, 218);
	label = g->cabin_spot >= 0 ?
	    game_word(g, W_CABIN, (size_t)g->cabin_spot) :
	    game_word(g, W_CABIN_TITLE, CABIN_TITLE);
	w = text_width(&g->font[FONT_TITLE], label);
	draw(g, FONT_TITLE, 320 - w / 2, 38, FB_W, label, 0);
	if (g->msg[0] != '\0')
		draw_centered(g, FONT_BODY, 444, g->msg);
}

const struct screen screen_cabin = {
	"cabin", cabin_enter, cabin_event, NULL, cabin_draw,
};

/* Save slots, for both saving and loading. */

static void
slots_enter(struct game *g)
{
	char path[600], err[256];
	int i;

	game_background(g, "pic/savegame");
	game_portrait(g, NULL);
	/* The slate behind the slots needs a light highlight. */
	g->highlight[0] = g->highlight[1] = g->highlight[2] = 240;
	for (i = 0; i < SAVE_SLOTS; i++) {
		save_slot_path(g->save_dir, i, path, sizeof(path));
		if (save_read(path, NULL, g->slot_desc[i],
		    sizeof(g->slot_desc[i]), err, sizeof(err)) != 0)
			g->slot_desc[i][0] = '\0';
	}
}

static int
slots_enabled(const struct game *g, int i)
{
	return (g->saving || g->slot_desc[i][0] != '\0');
}

static void
slots_choose(struct game *g, int i)
{
	char path[600], desc[SAVE_DESC_LEN + 1], date[64];

	save_slot_path(g->save_dir, i, path, sizeof(path));
	if (g->saving) {
		date_text(g, date, sizeof(date));
		snprintf(desc, sizeof(desc), "%s, %s, %s", g->sim.player.name,
		    g->sim.player.title, date);
		if (save_write(path, &g->sim, desc, g->err,
		    sizeof(g->err)) != 0)
			snprintf(g->msg, sizeof(g->msg), "%s", g->err);
		else
			snprintf(g->msg, sizeof(g->msg), "%s", desc);
		game_goto(g, &screen_cabin);
		return;
	}
	if (save_read(path, &g->sim, NULL, 0, g->err, sizeof(g->err)) != 0 ||
	    game_attach_world(g) != 0) {
		snprintf(g->msg, sizeof(g->msg), "%s", g->err);
		return;
	}
	g->started = 1;
	g->news_seen = g->sim.nnews;
	g->msg[0] = '\0';
	game_goto(g, &screen_cabin);
}

static void
slots_event(struct game *g, const struct ui_event *ev)
{
	menu_event(g, ev, SAVE_SLOTS, slots_enabled, slots_choose,
	    g->saving ? &screen_cabin : &screen_welcome);
}

static void
slots_draw(struct game *g)
{
	char buf[SAVE_SLOTS * 100], line[120];
	int i;

	draw_centered(g, FONT_TITLE, 40,
	    game_str(g, g->saving ? S_SAVE_GAME : S_LOAD_GAME));
	buf[0] = '\0';
	for (i = 0; i < SAVE_SLOTS; i++) {
		snprintf(line, sizeof(line), "%s~%d. %s", i > 0 ? "\r" : "",
		    i + 1, g->slot_desc[i][0] != '\0' ? g->slot_desc[i] :
		    "-");
		strncat(buf, line, sizeof(buf) - strlen(buf) - 1);
	}
	draw(g, FONT_BODY, 100, 100, 460, buf, 1);
	if (g->msg[0] != '\0')
		draw_centered(g, FONT_BODY, 400, g->msg);
}

const struct screen screen_slots = {
	"slots", slots_enter, slots_event, NULL, slots_draw,
};

/* Retirement: offer the Hall of Fame, as the game does. */

static void
retire_choose(struct game *g, int i)
{
	struct hof h;
	struct hof_entry e;
	char path[600];

	g->started = 0;
	g->hof_new = -1;
	if (i != 0 || g->save_dir[0] == '\0') {
		game_goto(g, &screen_welcome);
		return;
	}
	hof_path(g->save_dir, path, sizeof(path));
	if (hof_load(path, &h, g->err, sizeof(g->err)) != 0) {
		snprintf(g->msg, sizeof(g->msg), "%s", g->err);
		memset(&h, 0, sizeof(h));
	}
	memset(&e, 0, sizeof(e));
	sim_career_end(&g->sim, &g->end);
	e.score = g->end.score;
	e.rating = g->end.rating;
	snprintf(e.name, sizeof(e.name), "%s", g->sim.player.name);
	snprintf(e.title, sizeof(e.title), "%s", g->sim.player.title);
	g->hof_new = hof_add(&h, &e);
	if (g->hof_new >= 0 &&
	    hof_save(path, &h, g->err, sizeof(g->err)) != 0)
		snprintf(g->msg, sizeof(g->msg), "%s", g->err);
	game_goto(g, &screen_hof);
}

static void
retire_event(struct game *g, const struct ui_event *ev)
{
	if (ev->type == EV_KEY && ev->key == UI_KEY_ESCAPE) {
		retire_choose(g, 1);		/* not recorded */
		return;
	}
	menu_event(g, ev, 2, NULL, retire_choose, NULL);
}

static void
retire_draw(struct game *g)
{
	draw(g, FONT_BODY, 80, 180, 480, game_str(g, S_HOF_QUESTION), 1);
}

const struct screen screen_retire = {
	"retire", parchment_enter, retire_event, NULL, retire_draw,
};

/*
 * Hall of Fame (pic/fame), with the original's layout: rows 25 pixels
 * apart from y 200; rank, name, then score columns.
 */
#define HOF_TOP		200
#define HOF_ROW		25

static void
hof_enter(struct game *g)
{
	game_background(g, "pic/fame");
	game_portrait(g, NULL);
	/* Light text on the red cloth, gold for the new entry. */
	g->ink[0] = g->ink[1] = g->ink[2] = 235;
	g->highlight[0] = 250;
	g->highlight[1] = 210;
	g->highlight[2] = 60;
}

static void
hof_event(struct game *g, const struct ui_event *ev)
{
	if (ev->type == EV_CLICK || ev->type == EV_KEY)
		game_goto(g, &screen_welcome);
}

static void
hof_draw(struct game *g)
{
	struct hof h;
	char path[600], buf[96], err[256];
	size_t i;
	int y;

	if (g->save_dir[0] == '\0')
		return;
	hof_path(g->save_dir, path, sizeof(path));
	if (hof_load(path, &h, err, sizeof(err)) != 0)
		return;
	g->selected = 0;	/* the new entry is option 0 of its own line */
	for (i = 0; i < h.count; i++) {
		y = HOF_TOP + (int)i * HOF_ROW;
		/* "~" makes the new entry the highlighted option. */
		snprintf(buf, sizeof(buf), "%s%zu) %s", (int)i == g->hof_new ?
		    "~" : "", i + 1, h.entry[i].name);
		draw(g, FONT_BODY, 120, y, 200, buf, (int)i == g->hof_new);
		draw(g, FONT_BODY, 300, y, 180, h.entry[i].title, 0);
		snprintf(buf, sizeof(buf), "%ld", h.entry[i].score);
		draw(g, FONT_BODY, 480, y, 80, buf, 0);
	}
	if (g->msg[0] != '\0')
		draw_centered(g, FONT_BODY, 450, g->msg);
}

const struct screen screen_hof = {
	"hof", hof_enter, hof_event, NULL, hof_draw,
};

/*
 * Sailing: the map around the ship, as the original shows it.  The view
 * is tile aligned and jumps when the ship nears an edge, looking ahead in
 * the direction of travel (VA 0x231b8).  One simulation tick per frame.
 */
/*
 * Clouds over the sea (VA 0x24523, 0x231b8, 0x297c4): (|month - 5| + 1)
 * / 2 of them, none in June, three in winter, each a random cloud.cur
 * frame (0, 1, 2 or 0 again).  Every sailing pass cloud i drifts with
 * the wind by (4i + 6) x 2, in eighths; once out of -100..640 across or
 * -40..480 down from the view, it comes back at the right edge at a
 * random height.  Only a cloud found in view by its move is drawn (VA
 * 0x4c792), so one put back at the edge waits for the next pass.  Each is drawn with its shadow 30 pixels left and down,
 * which darkens what it covers through the table at VA 0x3400c.  A
 * pack's cloud sheet holds the three clouds then their three shadows,
 * dark and see-through.
 */
#define CLOUD_SHADOW		30

/*
 * Raw frames keep colour 0 clear: give those without a mask one, so
 * that the sub-pixel drawing (--smooth) skips it too.
 */
static void
key_masks(struct sprite_bank *b)
{
	struct frame *f;
	size_t i, k;

	for (i = 0; i < b->count; i++) {
		f = &b->frames[i];
		if (f->mask != NULL || f->rgba != NULL ||
		    (f->mask = malloc((size_t)f->w * f->h)) == NULL)
			continue;
		for (k = 0; k < (size_t)f->w * f->h; k++)
			f->mask[k] = f->pixels[k] != 0;
	}
}

static void
clouds_start(struct game *g)
{
	load_bank(g, "cur/cloud", 1, &g->cloud_spr);
	key_masks(&g->cloud_spr);
	load_bank(g, "spr/sailmisc", 0, &g->party_spr);
	g->march_held = g->march_tap = 0;
	sim_voyage_start(&g->sim);
}

/*
 * The sailing loop's pace (VA 0x231b8): each pass waits for the sound
 * driver's clock, 60.9 Hz (PIT divisor 0x4C90), to advance by 5, or by 1
 * once T has been pressed: 12.2 or 60.9 passes a second.  At 60 frames a
 * second, a pass every 5 frames or every frame.
 */
#define SAIL_FRAMES		5
#define SAIL_SUB		(8 * SAIL_FRAMES)
#define SAIL_JUMP		(16 * 8)	/* farther in a pass: a jump */
#define CLOUD_JUMP		16

/*
 * With --smooth, between two passes, the fleet and the clouds are drawn
 * on their way from the last pass to this one, a pass behind the rules
 * (an engine choice; the original moves them once a pass).  The pose at the last
 * pass: positions in 1/8 pixels for the fleet, pixels for the clouds.
 */
static void
sail_pose(const struct sim *s, struct sail_pose *p)
{
	int i;

	p->x8 = s->fleet.x * 8 + s->fleet.fx;
	p->y8 = s->fleet.y * 8 + s->fleet.fy;
	p->heading = s->fleet.heading;
	p->ashore = s->fleet.ashore;
	for (i = 0; i < 3; i++) {
		p->cloud_x[i] = s->clouds[i].x;
		p->cloud_y[i] = s->clouds[i].y;
	}
}

/* Frames since the last pass, SAIL_FRAMES when there is no catching up. */
static int
sail_lag(const struct game *g)
{
	return (g->sail_fast || !g->smooth ? SAIL_FRAMES : (int)g->sail_wait);
}

/* "from" to "to" "w" frames on, in 1/SAIL_FRAMES; farther than "jump" is
 * a jump, not a move. */
static int
sail_between(int from, int to, int w, int jump)
{
	if (abs(to - from) > jump)
		w = SAIL_FRAMES;
	return (from * (SAIL_FRAMES - w) + to * w);
}

static void
clouds_draw(struct game *g)
{
	const uint8_t *shade = g->ui.shade;
	const struct frame *f;
	const struct sim_cloud *c;
	int i, n = sim_clouds(&g->sim), sx, sy, w = sail_lag(g);

	for (i = 0; i < n && i < 3; i++) {
		c = &g->sim.clouds[i];
		if (!c->shown || (size_t)c->frame >= g->cloud_spr.count)
			continue;
		f = &g->cloud_spr.frames[c->frame];
		/* In 1/SAIL_FRAMES pixels. */
		sx = sail_between(g->sail_from.cloud_x[i], c->x, w,
		    CLOUD_JUMP) - g->sim.view_x * SAIL_FRAMES;
		sy = sail_between(g->sail_from.cloud_y[i], c->y, w,
		    CLOUD_JUMP) - g->sim.view_y * SAIL_FRAMES;
		if (f->rgba != NULL) {
			/* A pack's cloud: its shadow the frame 3 further on,
			 * dark and see-through, then the cloud over it. */
			if ((size_t)c->frame + 3 < g->cloud_spr.count)
				fb_frame_sub(&g->fb, &g->cloud_spr.frames[
				    c->frame + 3], sx - CLOUD_SHADOW *
				    SAIL_FRAMES, sy + CLOUD_SHADOW *
				    SAIL_FRAMES, SAIL_FRAMES);
			fb_frame_sub(&g->fb, f, sx, sy, SAIL_FRAMES);
			continue;
		}
		/* The original's: its shape shading the sea, then the
		 * cloud, colour 0 clear. */
		if (g->ui.has_shade)
			fb_shadow(&g->fb, f, sx - CLOUD_SHADOW * SAIL_FRAMES,
			    sy + CLOUD_SHADOW * SAIL_FRAMES, SAIL_FRAMES, shade);
		fb_frame_sub(&g->fb, f, sx, sy, SAIL_FRAMES);
	}
}

/* Sailing sprites by ship size, as the game picks them. */
static const char *
ship_sprite(const struct sim_fleet *f)
{
	int i, c = 0;

	for (i = 0; i < f->nships; i++)
		if ((f->ships[i] & 7) > c)
			c = f->ships[i] & 7;
	return (c < 3 ? "spr/ship0" : c < 6 ? "spr/ship1" : "spr/ship2");
}

static void
sail_enter(struct game *g)
{
	/* Can the crew sail the fleet (VA 0x1f65f, at each start)? */
	switch (sim_crew_check(&g->sim)) {
	case CHECK_ABANDON:
		g->panel_abandon = 1;
		game_goto(g, &screen_fleet_panel);
		return;
	case CHECK_LAST:
		game_goto(g, &screen_lastship);
		return;
	case CHECK_HOLD:
		g->looting = 1;
		g->jettison = 1;
		game_goto(g, &screen_merchant);
		return;
	default:
		break;
	}
	game_music(g, MUS_STOP);	/* setting sail (VA 0x28789) */
	image_free(&g->bg);
	game_portrait(g, NULL);
	memcpy(g->fb.pal, g->world->palette, sizeof(g->fb.pal));
	sea_init(&g->sea, g->fb.pal);
	bank_free(&g->ship);
	bank_load(g->packs, ship_sprite(&g->sim.fleet), BANK_RUNS, &g->ship);
	/* Replacement art of the map tiles; the world has their indices. */
	if (g->map_art.count == 0 &&
	    (tileset_load(g->packs, "tiles/map", &g->map_art) != 0 ||
	    g->map_art.tile_w != WORLD_TILE || g->map_art.count != 256))
		tileset_free(&g->map_art);
	g->turn = 0;
	g->ink[0] = g->ink[1] = g->ink[2] = 240;
	clouds_start(g);
	sail_pose(&g->sim, &g->sail_from);
}

static void
sail_event(struct game *g, const struct ui_event *ev)
{
	static const struct {
		int	key, bits;
	} walk[] = {
		{ UI_KEY_LEFT, 1 }, { UI_KEY_RIGHT, 2 }, { UI_KEY_UP, 4 },
		{ UI_KEY_DOWN, 8 }, { UI_KEY_UP_LEFT, 5 }, { UI_KEY_UP_RIGHT, 6 },
		{ UI_KEY_DOWN_LEFT, 9 }, { UI_KEY_DOWN_RIGHT, 10 },
	};
	size_t i;

	/* "Disembark" (VA 0x315d): yes or N; Enter and Escape as well. */
	if (g->ashore_prompt) {
		if (game_yes(g, ev) ||
		    (ev->type == EV_KEY && ev->key == UI_KEY_ENTER)) {
			g->ashore_prompt = 0;
			g->turn = 0;
			sim_go_ashore(&g->sim);
		} else if ((ev->type == EV_CHAR && (ev->ch == 'n' ||
		    ev->ch == 'N')) || (ev->type == EV_KEY &&
		    ev->key == UI_KEY_ESCAPE)) {
			g->ashore_prompt = 0;
			sim_turn_back(&g->sim);
		}
		return;
	}
	/* On foot, the arrows walk while held (VA 0x1b210). */
	if (g->sim.fleet.ashore && (ev->type == EV_KEY ||
	    ev->type == EV_KEYUP)) {
		for (i = 0; i < sizeof(walk) / sizeof(walk[0]); i++)
			if (walk[i].key == ev->key) {
				if (ev->type == EV_KEY) {
					g->march_held |= walk[i].bits;
					g->march_tap |= walk[i].bits;
				} else
					g->march_held &= ~walk[i].bits;
				return;
			}
	}
	if (ev->type == EV_KEY && ev->key == UI_KEY_LEFT)
		g->turn = g->turn_tap = -1;
	else if (ev->type == EV_KEY && ev->key == UI_KEY_RIGHT)
		g->turn = g->turn_tap = 1;
	else if (ev->type == EV_KEYUP &&
	    (ev->key == UI_KEY_LEFT || ev->key == UI_KEY_RIGHT))
		g->turn = 0;
	else if ((ev->type == EV_CHAR && ev->ch == ' ') ||
	    (ev->type == EV_KEY && ev->key == UI_KEY_ESCAPE)) {
		/* Space opens the cabin (key tables VA 0x33d90, 0x33c74);
		 * Escape too, an engine addition. */
		if (!g->sim.fleet.ashore)
			game_music(g, MUS_SILENCE);	/* VA 0x28789 */
		game_goto(g, &screen_cabin);
	} else if (ev->type == EV_CHAR && g->debug &&
	    strchr("NnEeRrFf", ev->ch) != NULL && ev->ch != 0) {
		/* The debug mode's keys (VA 0x1b700-0x1b74e at sea,
		 * 0x1b3ed-0x1b43b on foot): N no sails sighted, E sightings
		 * again, R invulnerable, F not. */
		switch (ev->ch | 0x20) {
		case 'n':
			g->sim.encounters = 0;
			break;
		case 'e':
			g->sim.encounters = 1;
			break;
		case 'r':
			g->sim.invulnerable = 1;
			break;
		default:
			g->sim.invulnerable = 0;
			break;
		}
	} else if (ev->type == EV_CHAR && (ev->ch == 't' || ev->ch == 'T'))
		g->sail_fast = !g->sail_fast;	/* VA 0x1b6ef */
	else if (ev->type == EV_CHAR && (ev->ch == 's' || ev->ch == 'S') &&
	    g->sim.fleet.ashore) {
		/* On foot S searches at once, as the cabin's Search (VA
		 * 0x1b3df: exit code 7, the dig at VA 0xa801). */
		g->dig_i = g->dig_page = 0;
		game_goto(g, &screen_dig);
	}
}

static void
sail_tick(struct game *g)
{
	int town, turn, walk;

	if (g->fading != 0)
		return;		/* the ship waits for the screen */
	if (!g->sail_fast && ++g->sail_wait < SAIL_FRAMES)
		return;
	g->sail_wait = 0;
	sail_pose(&g->sim, &g->sail_from);
	if (g->ashore_prompt)
		return;
	g->sail_passes++;
	/* A tap released between two passes still steers or walks on the
	 * next. */
	turn = g->turn != 0 ? g->turn : g->turn_tap;
	g->turn_tap = 0;
	walk = g->march_held | g->march_tap;
	g->march_tap = 0;
	switch (g->sim.fleet.ashore ? sim_march(&g->sim, walk & 1 ? -1 :
	    walk & 2 ? 1 : 0, walk & 4 ? -1 : walk & 8 ? 1 : 0, 1, &town) :
	    sim_sail(&g->sim, turn, 1, &town)) {
	case SAIL_TOWN:
		g->sail_fast = 0;		/* VA 0x284b6 */
		g->arrive_stage = 0;
		game_goto(g, &screen_arrive);
		break;
	case SAIL_ENCOUNTER:
		g->turn = 0;
		game_goto(g, &screen_encounter);
		break;
	case SAIL_LAND:
		g->ashore_prompt = 1;
		g->turn = 0;
		break;
	case SAIL_BOARDED:
		g->march_held = g->march_tap = 0;
		break;
	case SAIL_CREW:
		g->turn = 0;
		game_goto(g, &screen_crew);
		break;
	case SAIL_TICKS:
		break;
	}
}

static void
sail_draw(struct game *g)
{
	const struct sim_fleet *f = &g->sim.fleet;
	const struct sail_pose *from = &g->sail_from;
	struct sail_pose now;
	const uint8_t *t;
	const struct frame *fr;
	char buf[128], winds[64];
	int cx, cy, x0 = g->sim.view_x / WORLD_TILE, y0 = g->sim.view_y / WORLD_TILE;
	int w, x, y, h;
	uint8_t cell;

	sea_animate(&g->sea, g->sail_passes, g->fb.pal);
	for (cy = 0; cy < FB_H / WORLD_TILE; cy++)
		for (cx = 0; cx < FB_W / WORLD_TILE; cx++) {
			struct image tile;

			cell = g->world->map[(y0 + cy) * WORLD_CELLS_W + x0 +
			    cx];
			if (g->map_art.rgba != NULL)
				tileset_tile(&g->map_art, cell, g->sail_passes,
				    &tile);
			else {
				t = g->world->tiles + WORLD_TILE * WORLD_TILE *
				    cell;
				tile.w = tile.h = WORLD_TILE;
				tile.pixels = (uint8_t *)t;
				tile.has_palette = 0;
				tile.rgba = NULL;
			}
			fb_image(&g->fb, &tile, cx * WORLD_TILE, cy * WORLD_TILE,
			    0);
		}
	/* The fleet between passes: "x", "y" in 1/SAIL_SUB pixels, "h" in
	 * 1/SAIL_FRAMES heading steps. */
	sail_pose(&g->sim, &now);
	w = sail_lag(g);
	if (from->ashore != now.ashore || abs(now.x8 - from->x8) > SAIL_JUMP ||
	    abs(now.y8 - from->y8) > SAIL_JUMP)
		w = SAIL_FRAMES;
	x = sail_between(from->x8, now.x8, w, SAIL_JUMP);
	y = sail_between(from->y8, now.y8, w, SAIL_JUMP);
	if (!g->smooth) {		/* whole pixels, as the original */
		x = f->x * SAIL_SUB;
		y = f->y * SAIL_SUB;
	}
	h = (from->heading * SAIL_FRAMES + (((now.heading - from->heading +
	    128) & 255) - 128) * w + 256 * SAIL_FRAMES) % (256 * SAIL_FRAMES);
	/* As many headings as the bank has frames, frame 0 north. */
	if (g->ship.count > 0) {
		fr = &g->ship.frames[(long)h * (long)g->ship.count /
		    (256 * SAIL_FRAMES)];
		if (f->ashore)
			fb_frame(&g->fb, fr, f->ship_x - g->sim.view_x -
			    fr->w / 2, f->ship_y - g->sim.view_y - fr->h / 2);
		else
			fb_frame_sub(&g->fb, fr, x - (g->sim.view_x + fr->w /
			    2) * SAIL_SUB, y - (g->sim.view_y + fr->h / 2) *
			    SAIL_SUB, SAIL_SUB);
	}
	if (f->ashore) {
		/* The party: sailmisc.spr, four frames a direction (VA
		 * 0x36850), stepping every four passes. */
		static const int first[16] = { 0, 24, 8, 0, 0, 28, 4, 0, 16,
		    20, 12, 0, 0, 0, 0, 0 };
		int k = first[g->march_held & 15] +
		    (int)((g->sail_passes >> 2) & 3);

		if ((size_t)k < g->party_spr.count) {
			fr = &g->party_spr.frames[k];
			fb_frame_sub(&g->fb, fr, x - (g->sim.view_x + fr->w /
			    2) * SAIL_SUB, y - (g->sim.view_y + fr->h / 2) *
			    SAIL_SUB, SAIL_SUB);
		}
	}
	clouds_draw(g);
	if (g->ashore_prompt) {
		/* The language's "Disembark" plaque, misc.crs frame 8 at
		 * (268, 218) (VA 0x315d). */
		if (g->misc.count > DISEMBARK_FRAME)
			fb_frame(&g->fb, &g->misc.frames[DISEMBARK_FRAME], 268,
			    218);
	}
	date_text(g, buf, sizeof(buf));
	fill(winds, sizeof(winds), game_word(g, W_SAILING, SAILING_WINDS),
	    game_word(g, W_COMPASS, (size_t)(g->sim.wind >> 4)), NULL);
	snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
	    "   %s   %s%d", winds, game_word(g, W_SAILING, SAILING_KNOTS),
	    f->speed);
	draw(g, FONT_BODY, 10, 6, 620, buf, 0);
}

const struct screen screen_sail = {
	"sail", sail_enter, sail_event, sail_tick, sail_draw,
};

/*
 * Encounter: the game's dialogs over its pictures, "Sail ho!", then the
 * ship's portrait once she is identified, then her flag.  Sea battles are
 * a later phase; for now a battle ends the encounter with a notice.
 */
static const char *ship_portrait[9] = {
	"pic/s_pinn", "pic/s_sloop", "pic/s_barque", "pic/s_fluyt",
	"pic/s_merch", "pic/s_frig", "pic/s_war", "pic/s_gal", "pic/s_fast",
};

static const char *nation_colours[5] = {
	"pic/f-span", "pic/f-engl", "pic/f-french", "pic/f-dutch",
	"pic/fjolly01",
};

#define FLAG_FRAME	200
#define FLAG_FRAMES	8
#define FLAG_KEY	0	/* the flags' see-through colour */
#define FLAG_SHIFT	24	/* their colours 8-15 go to 32-39 */

/*
 * The colours shown (VA 0xd1fc): a named captain's Jolly Roger, by his
 * id's nation and surname (table at VA 0x32f90, pictures 0x5d-0x70),
 * else the nation's flag (pictures 0x43-0x46).  The original's picture
 * for an unnamed pirate is "null.pcx", which the game lacks; the engine
 * shows the first Jolly Roger.
 */
static const char *
encounter_flag(struct game *g, char *buf, size_t len)
{
	const struct sim_encounter *e = &g->sim.encounter;
	int n;

	if (e->captain != 0 &&
	    (n = g->ui.jolly[(e->captain >> 6) & 3][(e->captain >> 3) & 7]) >=
	    1 && n <= 20) {
		snprintf(buf, len, "pic/fjolly%02d", n);
		return (buf);
	}
	return (nation_colours[e->nation % 5]);
}

/*
 * The flag over the spyglass's sky (VA 0x108cf): each frame's
 * see-through pixels take BASEFLAG's, the others move up 24 colours,
 * and the flag's colours 8-15 join the base's palette at 32-39.
 */
static void
flag_composite(struct game *g)
{
	struct image base;
	int f, x, y;
	uint8_t *px;

	memset(&base, 0, sizeof(base));
	if (g->bg.pixels == NULL || g->bg.rgba != NULL ||
	    g->bg.w < 4 * FLAG_FRAME || g->bg.h < 2 * FLAG_FRAME ||
	    pic_load(g->packs, "pic/baseflag", &base) != 0)
		return;
	if (base.pixels != NULL && base.rgba == NULL && base.w >= FLAG_FRAME &&
	    base.h >= FLAG_FRAME) {
		for (f = 0; f < FLAG_FRAMES; f++)
			for (y = 0; y < FLAG_FRAME; y++) {
				px = g->bg.pixels + ((f / 4) * FLAG_FRAME + y) *
				    g->bg.w + (f % 4) * FLAG_FRAME;
				for (x = 0; x < FLAG_FRAME; x++)
					px[x] = px[x] == FLAG_KEY ?
					    base.pixels[y * base.w + x] :
					    (uint8_t)(px[x] + FLAG_SHIFT);
			}
		memcpy(base.palette + (8 + FLAG_SHIFT) * 3, g->bg.palette + 8 * 3,
		    8 * 3);
		memcpy(g->bg.palette, base.palette, sizeof(g->bg.palette));
		memcpy(g->fb.pal, g->bg.palette, sizeof(g->fb.pal));
		g->flag_composed = 1;
	}
	image_free(&base);
}

/*
 * The flag's next frame (VA 0xfc0a): it is due 2w + 1 ticks on, w the
 * battle's wind strength (VA 0xd58f), one less when rand(4) is 0, one
 * more when it is 3, and comes once that tick is passed.  The jitter is
 * cosmetic: it comes from the front end's own draws.
 */
static void
flag_advance(struct game *g)
{
	int r = rng_below(&g->fx_rng, 4);

	g->flag_next = g->ticks + 2 * (unsigned)g->sim.encounter.wind_strength +
	    1 + (r == 3) - (r == 0);
	g->flag_frame = (g->flag_frame + 1) % FLAG_FRAMES;
}

static void
encounter_background(struct game *g)
{
	const struct sim_encounter *e = &g->sim.encounter;
	char flag[32];

	g->flag_composed = 0;
	switch (e->stage) {
	case ENC_IDENTIFIED:
		game_background(g, ship_portrait[e->type % 9]);
		break;
	case ENC_COLOURS:
		game_background(g, encounter_flag(g, flag, sizeof(flag)));
		flag_composite(g);
		/* The first change comes at once (VA 0x32df8). */
		g->flag_frame = 0;
		flag_advance(g);
		break;
	default:
		game_background(g, "pic/sail-ho");
		break;
	}
	g->ink[0] = g->ink[1] = g->ink[2] = 235;
	g->highlight[0] = 250;
	g->highlight[1] = 210;
	g->highlight[2] = 60;
	if (e->stage == ENC_COLOURS) {
		/* Flag palettes have no yellow; their red reads well. */
		g->highlight[0] = 230;
		g->highlight[1] = 40;
		g->highlight[2] = 30;
	}
}

static void
encounter_enter(struct game *g)
{
	game_portrait(g, NULL);
	g->enc_result = -1;
	encounter_background(g);
	if (g->sim.encounter.stage == ENC_SIGHTED) {
		game_sfx(g, SFX_SAIL_HO);
		game_music(g, MUS_SAIL_HO);
	}
}

static void
encounter_choose(struct game *g, int i)
{
	static const int sighted[2] = { ACT_INVESTIGATE, ACT_IGNORE };
	static const int colours[3] = { ACT_BATTLE, ACT_HAIL, ACT_IGNORE };
	enum encounter_result r;
	int action;

	action = g->sim.encounter.stage == ENC_COLOURS ? colours[i % 3] :
	    sighted[i % 2];
	/* After a named captain's hail is answered (VA 0xd1fc). */
	if (g->sim.encounter.stage == ENC_COLOURS &&
	    g->sim.encounter.captain != 0)
		game_sfx(g, SFX_HAIL);
	r = sim_encounter_act(&g->sim, (enum encounter_action)action);
	if (r == RES_CONTINUE) {
		g->selected = 0;
		encounter_background(g);
		return;
	}
	if (r == RES_ESCAPED) {
		game_goto(g, &screen_sail);
		return;
	}
	if (r == RES_BATTLE) {
		game_goto(g, &screen_battle);
		return;
	}
	if (r == RES_NEWS) {
		game_background(g, "pic/shipnews");
		g->ink[0] = 240;	/* light, on the dark blue page */
		g->ink[1] = 220;
		g->ink[2] = 170;
	}
	g->enc_result = (int)r;	/* show the outcome until a key */
}

static void
encounter_event(struct game *g, const struct ui_event *ev)
{
	int count;

	if (g->enc_result >= 0) {
		if (ev->type == EV_CLICK || (ev->type == EV_KEY &&
		    ev->key == UI_KEY_ENTER))
			game_goto(g, g->sim.battle.active ? &screen_battle :
			    &screen_sail);
		return;
	}
	count = g->sim.encounter.stage == ENC_COLOURS ? 3 : 2;
	menu_event(g, ev, count, NULL, encounter_choose, NULL);
}

static void
encounter_tick(struct game *g)
{
	if (g->enc_result < 0 && g->sim.encounter.stage == ENC_COLOURS &&
	    g->ticks > g->flag_next)
		flag_advance(g);
}


/*
 * Ship portraits are sheets of 5 round views of about 200 pixels, and
 * flags sheets of 8 waving frames of 200x200 on a green key, both shown
 * in a 200x200 window at (220, 10) while the question waits (VA 0xfb24,
 * 0xfc0a; views at VA 0x32da8, 0x32db4, flag frames at VA 0x32dd8,
 * 0x32de8).  A ship's view lasts 11 ticks of the 60.9 Hz clock (delay
 * 10, VA 0x32e00); a flag's frame 2w + 2, give or take 1 (flag_advance).
 */
#define PORTRAIT_VIEW	200

static void
encounter_picture(struct game *g)
{
	const struct sim_encounter *e = &g->sim.encounter;
	int f, key;

	if (e->stage != ENC_IDENTIFIED && e->stage != ENC_COLOURS)
		return;		/* "Sail ho!" is a full picture */
	fb_clear(&g->fb, fb_nearest(&g->fb, 0, 0, 0));
	if (e->stage == ENC_IDENTIFIED) {
		static const int vx[5] = { 1, 202, 403, 1, 202 };
		static const int vy[5] = { 1, 1, 1, 202, 202 };

		f = (int)(g->ticks / 11) % 5;
		fb_image_part(&g->fb, &g->bg, vx[f], vy[f], PORTRAIT_VIEW,
		    PORTRAIT_VIEW, (FB_W - PORTRAIT_VIEW) / 2, 10, -1);
		return;
	}
	f = g->flag_frame;
	/* Composed over the sky, or a replacement brings its own sky in its
	 * true colours; else the flag keeps its key. */
	key = g->flag_composed || g->bg.rgba != NULL ? -1 :
	    fb_nearest(&g->fb, 0, 255, 0);
	fb_image_part(&g->fb, &g->bg, (f % 4) * FLAG_FRAME,
	    (f / 4) * FLAG_FRAME, FLAG_FRAME, FLAG_FRAME,
	    (FB_W - FLAG_FRAME) / 2, 10, key);
}

static void
encounter_draw(struct game *g)
{
	const struct sim_encounter *e = &g->sim.encounter;
	char text[512], waters[160];
	struct sim_news pub[NEWS_SHOWN];
	const struct sim_news *n;
	size_t i, np;
	int y, w;

	if (g->enc_result < 0)
		encounter_picture(g);

	if (g->enc_result == RES_NEWS) {
		/* The ship's news (VA 0x1e78d): the title and the latest
		 * items others tell at (20, 100) up to x 300, left of the
		 * picture, or "(no entries)" when none is told. */
		w = text_width(&g->font[FONT_TITLE],
		    game_word(g, W_NEWS, NEWS_TITLE));
		draw(g, FONT_TITLE, 160 - w / 2, 40, 300,
		    game_word(g, W_NEWS, NEWS_TITLE), 0);
		np = public_news(g, pub, NEWS_SHOWN);
		y = 100;
		if (np == 0)
			draw(g, FONT_BODY, 20, y, 280,
			    game_word(g, W_NEWS, NEWS_NONE), 0);
		for (i = np; i > 0 && y < 420; i--) {
			n = &pub[i - 1];
			news_text(g, n, text, sizeof(text));
			y += draw(g, FONT_BODY, 20, y, 280, text, 0) + 8;
		}
		continue_prompt(g);
		return;
	}
	if (g->enc_result >= 0) {
		y = 256;
		switch (g->enc_result) {
		case RES_OVERTAKEN:
			y += draw(g, FONT_BODY, 200, y, 340,
			    game_str(g, S_OVERTAKEN), 0);
			break;
		case RES_UNANSWERED:
			y += draw(g, FONT_BODY, 200, y, 340,
			    game_str(g, S_UNANSWERED), 0);
			break;
		}

		continue_prompt(g);
		return;
	}
	switch (e->stage) {
	case ENC_SIGHTED:
		/* Whose waters, below difficulty 2: the nearest town's
		 * nation, "unknown" (CSTN 71.5) more than 12 cells away
		 * (VA 0xd25f). */
		waters[0] = '\0';
		if (g->sim.player.difficulty < 2 && e->town >= 0)
			fill(waters, sizeof(waters), game_str(g, S_WATERS),
			    game_word(g, W_NATION, e->dist > 12 ?
			    NATION_UNKNOWN : (size_t)g->sim.towns[e->town].nation &
			    3), NULL);
		fill(text, sizeof(text), game_str(g, S_SAIL_HO), waters, NULL);
		break;
	case ENC_IDENTIFIED:
		fill(text, sizeof(text), game_str(g, S_LOOKS_LIKE),
		    game_word(g, W_A_SHIP, (size_t)e->type), NULL);
		break;
	default:
		if (e->hostile || e->named) {
			/* A chaser or the hunted pirate: "Arrgh! It's the
			 * %s %s, %s!" (CSTR 121, VA 0xd1fc): his repute,
			 * pirate or pirate-hunter, his name. */
			const char *arg[3];
			char name[80];

			captain_name(g, e->captain, name, sizeof(name));
			arg[0] = game_word(g, 34, (size_t)(e->named ? 1 :
			    (e->captain & 7) / 3));
			arg[1] = game_word(g, 105, e->nation == NATION_PIRATE ?
			    0 : 1);
			arg[2] = name;
			fill_args(text, sizeof(text), game_str(g, S_NAMED_CHASE),
			    arg, 3);
		} else
			fill(text, sizeof(text), game_str(g, S_COLOURS),
			    game_word(g, W_COLOURS, (size_t)e->nation), NULL);
		break;
	}
	/* No box: the text is written on the screen (VA 0x10560): "Sail
	 * ho!" at (100, 300), the later questions at (200, 256), all up
	 * to x 540. */
	if (e->stage == ENC_SIGHTED)
		draw(g, FONT_BODY, 100, 300, 440, text, 1);
	else
		draw(g, FONT_BODY, 200, 256, 340, text, 1);
}

const struct screen screen_encounter = {
	"encounter", encounter_enter, encounter_event, encounter_tick,
	encounter_draw,
};

/*
 * Sea battle (pic/sbat): a 480x480 battle area, the encounter's corner of
 * the map drawn with the 32x32 battle tiles, and a panel on the right
 * with each side's men, guns and damage picture, and the wind.  Left and
 * Right steer, Enter or Space fires, Up or Down switches the sails.
 */
#define AREA		480
#define AREA_CELLS	(AREA / 32)
#define PANEL_X		492

static const char *damage_picture[9] = {
	"cur/pinnabtl", "cur/sloopbtl", "cur/barqubtl", "cur/cargobtl",
	"cur/merchbtl", "cur/frigabtl", "cur/wargabtl", "cur/gallebtl",
	"cur/fastgbtl",
};

static void
load_bank(struct game *g, const char *id, int raw, struct sprite_bank *b)
{
	bank_free(b);
	bank_load(g->packs, id, raw ? BANK_RAW : BANK_RUNS, b);
}

static void
battle_sprites(struct game *g, int side, int type)
{
	static const char *a[3] = { "cur/ship0a", "cur/ship1a", "cur/ship2a" };
	static const char *b[3] = { "cur/ship0b", "cur/ship1b", "cur/ship2b" };
	int size = (type & 7) < 3 ? 0 : (type & 7) < 6 ? 1 : 2;

	load_bank(g, "cur/sbtlmisc", 1, &g->btl_misc);
	load_bank(g, a[size], 1, &g->btl_ship[side][0]);
	load_bank(g, b[size], 1, &g->btl_ship[side][1]);
	key_masks(&g->btl_ship[side][0]);
	key_masks(&g->btl_ship[side][1]);
	load_bank(g, side == 1 && g->sim.battle.fort ? "cur/fortbtl" :
	    damage_picture[type % 9], 1, &g->btl_damage[side]);
}

static void	prize_show(struct game *);
static void	battle_hurt_seed(struct game *);
static int	vnews_offer(struct game *, int, int, const struct screen *);


static void
battle_enter(struct game *g)
{
	/* Several ships: which one to command, before the battle shows
	 * (VA 0xf246). */
	if (g->sim.battle.outcome == BAT_NONE && g->sim.battle.tick == 0 &&
	    g->sim.fleet.nships > 1 && !g->btl_chosen) {
		game_goto(g, &screen_fleet_panel);
		return;
	}
	game_background(g, "pic/sbat");
	game_portrait(g, NULL);
	battle_sprites(g, 0, g->sim.battle.you.type);
	battle_sprites(g, 1, g->sim.battle.foe.type);
	g->btl_heading[0] = g->sim.battle.you.heading;
	g->btl_heading[1] = g->sim.battle.foe.heading;
	tileset_free(&g->btl_tiles);
	tileset_load(g->packs, "tiles/battle", &g->btl_tiles);
	sea_init(&g->sea, g->fb.pal);	/* its sea turns too (VA 0x939b) */
	g->btl_fire = g->btl_sails = 0;
	battle_hurt_seed(g);
	g->btl_stage = g->sim.battle.outcome == BAT_NONE ? 0 : 1;
	if (g->sim.battle.outcome == BAT_BOARDING &&
	    g->sim.sword.result == SWORD_WON)
		prize_show(g);		/* back from the fight: the prize */
	g->turn = 0;
	g->ink[0] = g->ink[1] = g->ink[2] = 240;
}


/* The prize's question (CSTR 164, VA 0xdb7b): "%sa %s ton %s with %s
 * guns... space for %s tons": 20 tons per class step, the guns rounded
 * up to even, and the space left in the hold. */
static void
prize_draw(struct game *g)
{
	const struct sim_battle *bt = &g->sim.battle;
	const char *arg[5];
	char text[600], tons[16], guns[16], space[16];

	snprintf(tons, sizeof(tons), "%d", (bt->foe.type & 0xF) * 20 + 20);
	snprintf(guns, sizeof(guns), "%d", (bt->foe.guns + 1) & ~1);
	snprintf(space, sizeof(space), "%d", sim_hold_space(&g->sim));
	arg[0] = "";
	arg[1] = tons;
	arg[2] = game_word(g, W_SHIP_TYPE, (size_t)(bt->foe.type % 10));
	arg[3] = guns;
	arg[4] = space;
	fill_args(text, sizeof(text), game_str(g, S_PRIZE), arg, 5);
	draw(g, FONT_BODY, 40, 50, 580, text, 1);
}

/* The prize (VA 0xdb7b): on plunder.pcx, in ink entry 0. */
static void
prize_show(struct game *g)
{
	g->btl_stage = 2;
	g->selected = 0;
	game_music(g, MUS_PRIZE);
	game_background(g, "pic/plunder");
	if (g->bg.pixels != NULL) {
		memcpy(g->ink, g->bg.palette, 3);
		memcpy(g->highlight, bg_colour(g, STORY_HIGHLIGHT), 3);
	}
}

static void
battle_finish(struct game *g, int keep)
{
	g->btl_landed = g->sim.battle.fort &&
	    g->sim.battle.outcome == BAT_LANDED;
	g->btl_aboard = g->sim.battle.crew_ashore;
	g->btl_hunted = g->sim.mission.type == MISSION_PIRATE;
	g->btl_chosen = 0;
	g->btl_final = (int)sim_battle_finish(&g->sim, keep);
	g->btl_news = g->sim.battle.news_town;
	g->btl_loot = g->btl_final == BAT_SURRENDER;
	if (g->sim.jail.fresh) {
		/* Beaten aboard, or the last ship gone (VA 0x16647). */
		game_goto(g, &screen_prison);
		return;
	}
	if (g->sim.prize_pending) {
		/* Eight ships: which makes room for the prize (CSTR 130)? */
		g->panel_replace = 1;
		game_goto(g, &screen_fleet_panel);
		return;
	}
	game_goto(g, &screen_battle_end);
}

/*
 * The steps after a sea battle, each coming back here: the captured
 * crew's news of villains near a town (VA 0xdb7b), the captain held,
 * the pirate a governor wanted, the assault after a landing.
 */
static void
after_battle_enter(struct game *g)
{
	int town = g->btl_news, landed = g->btl_landed;

	g->btl_news = -1;
	g->btl_landed = 0;
	if (town >= 0 && vnews_offer(g, 3, town, &screen_battle_end))
		return;
	if (g->sim.hostage != 0) {
		/* The captured captain speaks first (VA 0xdb7b). */
		g->hostage_said = -1;
		g->selected = 0;
		game_goto(g, &screen_hostage);
		return;
	}
	if (g->btl_hunted && g->sim.mission.type == -MISSION_PIRATE) {
		/* The pirate a governor wanted (CSTR 207, VA 0x1f8f7). */
		g->btl_hunted = 0;
		g->villain_back = &screen_battle_end;
		game_goto(g, &screen_contact);
		return;
	}
	if (g->btl_loot) {
		/* Her cargo on the board, then her sailors (VA 0xdb7b). */
		g->btl_loot = 0;
		g->looting = 1;
		g->prize_loot = 1;
		game_goto(g, &screen_merchant);
		return;
	}
	if (g->sim.battle.volunteers > 0) {
		game_goto(g, &screen_volunteers);
		return;
	}
	if (landed) {
		/* The assault follows (VA 0x18e3d). */
		g->assault_town = g->sim.encounter.town;
		g->assault_aboard = g->btl_aboard;
		sim_fort_battle_garrison(&g->sim, g->assault_town);
		game_goto(g, &screen_assault);
		return;
	}
	game_goto(g, &screen_sail);
}

static void
after_battle_event(struct game *g, const struct ui_event *ev)
{
	(void)g;
	(void)ev;
}

static void
after_battle_draw(struct game *g)
{
	(void)g;
}

const struct screen screen_battle_end = {
	"after the battle", after_battle_enter, after_battle_event, NULL,
	after_battle_draw,
};

/* "Shall we keep her?": yes, send a prize crew; no, plunder and sink. */
static void
prize_choose(struct game *g, int i)
{
	battle_finish(g, i == 0);
}

static void
battle_event(struct game *g, const struct ui_event *ev)
{
	int outcome = g->sim.battle.outcome;

	if (g->btl_stage == 2) {
		menu_event(g, ev, 2, NULL, prize_choose, NULL);
		return;
	}

	if (g->btl_stage == 1) {
		if (!((ev->type == EV_KEY && ev->key == UI_KEY_ENTER) ||
		    ev->type == EV_CLICK))
			return;
		if (outcome == BAT_SURRENDER)
			prize_show(g);
		else
			battle_finish(g, 0);
		return;
	}
	if (ev->type == EV_KEY && ev->key == UI_KEY_LEFT)
		g->turn = -1;
	else if (ev->type == EV_KEY && ev->key == UI_KEY_RIGHT)
		g->turn = 1;
	else if (ev->type == EV_KEYUP &&
	    (ev->key == UI_KEY_LEFT || ev->key == UI_KEY_RIGHT))
		g->turn = 0;
	else if ((ev->type == EV_KEY && ev->key == UI_KEY_ENTER) ||
	    (ev->type == EV_CHAR && ev->ch == ' '))
		g->btl_fire = 1;
	else if (ev->type == EV_KEY && ev->key == UI_KEY_UP)
		g->btl_sails = 1;
	else if (ev->type == EV_KEY && ev->key == UI_KEY_DOWN)
		g->btl_sails = -1;
}

/* The text of a battle's end (CSTR 167-170, 197, 198). */
static void
battle_end_text(struct game *g, char *text, size_t len)
{
	const struct sim_battle *bt = &g->sim.battle;

	switch (bt->outcome) {
	case BAT_PLAYER_SUNK:
		snprintf(text, len, "%s", game_str(g, S_YOU_SUNK));
		break;
	case BAT_ENEMY_SUNK:
		snprintf(text, len, "%s", game_str(g, S_FOE_SUNK));
		break;
	case BAT_SUNSET:
		snprintf(text, len, "%s", game_str(g, bt->fort ?
		    S_FORT_SUNSET : S_SUNSET));
		break;
	case BAT_SURRENDER:
		snprintf(text, len, "%s", game_str(g, S_STRUCK));
		break;
	case BAT_BEACHED:
		snprintf(text, len, "%s", game_str(g, S_BEACHED));
		break;
	case BAT_ESCAPED:
		/* Out of range (VA 0xe82a): the enemy pulled away when it
		 * was fleeing (decision 2), otherwise the player did. */
		snprintf(text, len, "%s", game_str(g, bt->fort ?
		    S_FORT_DRIVES_OFF : bt->decision == 2 ? S_FOE_ESCAPED :
		    S_YOU_ESCAPED));
		break;
	default:
		text[0] = '\0';
		break;
	}
}

/* A battle's end on its own picture: the sunset's text at (40, 150) up to
 * x 280, the others at (100, 300) up to x 560 (VA 0xe67b-0xe883). */
static void
battle_end_draw(struct game *g)
{
	char text[600];

	if (g->sim.battle.outcome == BAT_BEACHED) {
		fb_clear(&g->fb, fb_nearest(&g->fb, 0, 0, 0));
		fb_image_part(&g->fb, &g->bg, 0, 0, PORTRAIT_VIEW,
		    PORTRAIT_VIEW, (FB_W - PORTRAIT_VIEW) / 2, 50, -1);
	}
	battle_end_text(g, text, sizeof(text));
	if (g->sim.battle.outcome == BAT_SUNSET)
		draw(g, FONT_BODY, 40, 150, 240, text, 0);
	else
		draw(g, FONT_BODY, 100, 300, 460, text, 0);
	continue_prompt(g);
}

/* The picture of a battle's end (VA 0xe67b-0xe918), or NULL to stay on
 * the battle. */
static const char *
battle_end_picture(int outcome)
{
	switch (outcome) {
	case BAT_PLAYER_SUNK:
	case BAT_ENEMY_SUNK:
		return ("pic/sinking");
	case BAT_ESCAPED:
		return ("pic/outrun");
	case BAT_SUNSET:
		return ("pic/night");
	case BAT_BEACHED:		/* the enemy's portrait (VA 0xe9e6) */
		return ("beached");
	default:
		return (NULL);
	}
}

/*
 * A pass's sound as the game sends it: the digitised effect when the
 * driver has it, else the FM effect (VA 0x2747c, 0x9551).
 */
static void
play_sound(struct game *g, int fm, int digital)
{
	if (fm == 0)
		return;
	if (game_has_sfx(g, digital))
		game_sfx(g, digital);
	else
		game_fm(g, fm);
}

/*
 * --smooth: the heading drawn turns to the ship's over a few frames,
 * an eighth of the way each, instead of at once; more than a quarter
 * turn away, it is a jump.
 */
static void
battle_heading_follow(int *shown, int heading)
{
	int d = ((heading - *shown + 128) & 255) - 128;

	if (abs(d) > 64)
		*shown = heading;
	else
		*shown = (*shown + (d + (d > 0 ? 7 : d < 0 ? -7 : 0)) / 8) &
		    255;
}

/* A side's damage stage (VA 0xebd7): damage / 16, at most 4; a fort
 * has none (-1). */
static int
battle_hurt_stage(const struct sim_battle *bt, int side)
{
	const struct battle_ship *b = side == 0 ? &bt->you : &bt->foe;

	if (side == 1 && bt->fort)
		return (-1);
	return (b->damage / 16 > 4 ? 4 : b->damage / 16);
}

/* The stages the battle starts with show no word (VA 0x2a603). */
static void
battle_hurt_seed(struct game *g)
{
	int side;

	for (side = 0; side < 2; side++) {
		g->btl_hurt[side] = -1;
		g->btl_hurt_seen[side] = battle_hurt_stage(&g->sim.battle,
		    side);
	}
}

/*
 * A new damage stage writes its word on the side's status line (VA
 * 0xebd7, event 7); the next status event writes over it: for the
 * player firing, reloaded, or the sails starting or ending a change
 * (VA 0xef06, events 1, 2, 10, 11), for the enemy its sails changing
 * (event 12).
 */
static void
battle_hurt_update(struct game *g, int ev, int sail_timer, int foe_sails)
{
	const struct sim_battle *bt = &g->sim.battle;
	int side, lvl;

	if ((ev & (BEV_FIRED | BEV_RELOADED | BEV_SAILS)) != 0 ||
	    (sail_timer == 0 && bt->you.sail_timer > 0))
		g->btl_hurt[0] = -1;
	if (foe_sails != bt->foe.full_sails)
		g->btl_hurt[1] = -1;
	for (side = 0; side < 2; side++) {
		lvl = battle_hurt_stage(bt, side);
		if (lvl != g->btl_hurt_seen[side])
			g->btl_hurt[side] = g->btl_hurt_seen[side] = lvl;
	}
}

static void
battle_tick(struct game *g)
{
	int ev, sail_timer = g->sim.battle.you.sail_timer,
	    foe_sails = g->sim.battle.foe.full_sails;

	if (g->fading != 0 || g->btl_stage != 0)
		return;
	ev = sim_battle_tick(&g->sim, g->turn, g->btl_fire, g->btl_sails);
	battle_hurt_update(g, ev, sail_timer, foe_sails);
	battle_heading_follow(&g->btl_heading[0], g->sim.battle.you.heading);
	battle_heading_follow(&g->btl_heading[1], g->sim.battle.foe.heading);
	play_sound(g, g->sim.battle.sound, g->sim.battle.sound ==
	    FM_BROADSIDE ? SFX_BROADSIDE : -1);	/* VA 0x9551 */
	g->btl_fire = 0;
	g->btl_sails = 0;
	if (g->sim.battle.outcome == BAT_BOARDING) {
		g->turn = 0;
		g->btl_stage = 1;
		game_goto(g, &screen_sword);
	} else if (g->sim.battle.outcome == BAT_LANDED) {
		g->turn = 0;
		g->btl_stage = 1;
		battle_finish(g, 0);
	} else if (g->sim.battle.outcome != BAT_NONE) {
		g->btl_stage = 1;
		g->turn = 0;
		if (g->sim.battle.outcome == BAT_BEACHED) {
			game_background(g, ship_portrait[g->sim.battle.foe.type %
			    9]);
			g->ink[0] = g->ink[1] = g->ink[2] = 240;
		} else if (battle_end_picture(g->sim.battle.outcome) != NULL) {
			game_background(g,
			    battle_end_picture(g->sim.battle.outcome));
			/* The ink is palette entry 2 (VA 0xe8a9). */
			if (g->bg.pixels != NULL)
				memcpy(g->ink, bg_colour(g, STORY_INK),
				    3);
		}
	}
}

static void
battle_ship_draw(struct game *g, int side, const struct battle_ship *b)
{
	/* The headings split between banks A and B, as many as they have
	 * frames, frame 0 north (the original has 8 + 8). */
	int n = (int)g->btl_ship[side][0].count, idx;
	const struct sprite_bank *bank;
	const struct frame *f;

	if (n == 0 || g->btl_ship[side][1].count != (size_t)n)
		return;
	idx = (((g->smooth ? g->btl_heading[side] : b->heading) * 2 * n +
	    128) >> 8) % (2 * n);
	bank = &g->btl_ship[side][idx / n];
	f = &bank->frames[idx % n];
	/* Clipped to the area, colour 0 of raw frames clear; with
	 * --smooth at its exact place (1/256 pixels). */
	fb_frame_sub_clip(&g->fb, f, g->smooth ? b->x - f->w * 128 :
	    (b->x / 256 - f->w / 2) * 256, g->smooth ? b->y - f->h * 128 :
	    (b->y / 256 - f->h / 2) * 256, 256, 0, 0, AREA - 1, AREA - 1);
}

/*
 * Raw sbtlmisc.cur frame "n" with its corner at (x, y), colour 0 clear;
 * "field": clipped to the battle field.
 */
static void
battle_misc(struct game *g, int n, int x, int y, int field)
{
	const struct frame *f;
	int fx, fy, w = field ? AREA : FB_W, h = field ? AREA : FB_H;

	if (n < 0 || (size_t)n >= g->btl_misc.count)
		return;
	f = &g->btl_misc.frames[n];
	for (fy = 0; fy < f->h; fy++)
		for (fx = 0; fx < f->w; fx++) {
			uint8_t c = f->pixels[fy * f->w + fx];

			if (c != 0 && x + fx >= 0 && x + fx < w &&
			    y + fy >= 0 && y + fy < h)
				fb_put(&g->fb, (y + fy) * FB_W + x + fx, c);
		}
}

/*
 * The red cross off the player's bow (VA 0xf903, object type 0x30):
 * sbtlmisc.cur frame 20, at the ship plus the offset for heading / 16
 * (table at VA 0x40fbc, a circle of about 50 pixels around (32, 23))
 * plus (3, 2).  It shows which ship is the player's and where her bow
 * points; the guns fire abeam, not at it.  The engine centres that
 * circle on the ship.
 */
static void
battle_bow_mark(struct game *g, const struct battle_ship *b)
{
	const struct frame *f;
	int i = (b->heading >> 4) & 15;

	if (g->btl_misc.count <= BOW_MARK)
		return;
	f = &g->btl_misc.frames[BOW_MARK];
	battle_misc(g, BOW_MARK, b->x / 256 + g->world->bow_mark[2 * i] - 32 +
	    3 - f->w / 2, b->y / 256 + g->world->bow_mark[2 * i + 1] - 23 + 2 -
	    f->h / 2, 1);
}

/*
 * A side's shot (VA 0x83ef-0x8545, the enemy's 0x9080-0x91d0), its
 * sprites' corner 21, 6 off the shot: in flight, the balls (frame 4 +
 * the spread) on its line over their shadow (frame 0 + the spread),
 * the arc's height below them; once landed in the water, the splash
 * (frames 8-12, one per 8 ticks of landing) where the shadow was.
 */
static void
battle_shot_draw(struct game *g, const struct battle_shot *sh)
{
	int x = sh->x / 256 - 21, y = sh->y / 256 - 6, f;

	f = g->world->shot_frame[sh->step & (SHOT_STEPS - 1)] & 3;
	if (sh->ticks > 0 || sh->landing == 1) {
		battle_misc(g, SHOT_SHADOW + f, x, y -
		    g->world->shot_rise[sh->step & (SHOT_STEPS - 1)], 1);
		battle_misc(g, SHOT_BALLS + f, x, y, 1);
	} else if (sh->landing > 1 && !sh->hit)
		battle_misc(g, SHOT_SPLASH + (sh->landing >> 3), x, y -
		    g->world->shot_rise[sh->step & (SHOT_STEPS - 1)], 1);
}

/* A hit's burst (frames 24-28) on the centre of the ship hit (VA
 * 0x8494, 0x911f). */
static void
battle_burst_draw(struct game *g, const struct battle_shot *sh,
    const struct battle_ship *to)
{
	if (sh->ticks == 0 && sh->landing > 1 && sh->hit)
		battle_misc(g, SHOT_BURST + (sh->landing >> 3), to->x / 256 -
		    21, to->y / 256 - 6, 1);
}

/*
 * A side's panel: its type, men and guns, and its status line: the
 * damage word while "hurt" is a stage, else W_SAILING "status", or the
 * sails when it is -1; a fort (status -2) has none (VA 0xea3e, 0xebd7).
 */
static void
battle_side_text(struct game *g, int y, const struct battle_ship *b,
    int status, int hurt)
{
	char buf[96];

	draw(g, FONT_SMALL, PANEL_X, y, 140,
	    game_word(g, W_SHIP_TYPE, (size_t)(b->type % 10)), 0);
	snprintf(buf, sizeof(buf), "%s%d  %s%d",
	    game_word(g, W_SAILING, SAILING_MEN), b->crew,
	    game_word(g, W_SAILING, SAILING_GUNS), b->guns);
	draw(g, FONT_SMALL, PANEL_X, y + 72, 140, buf, 0);
	if (status < -1)
		return;
	snprintf(buf, sizeof(buf), "%s", hurt >= 0 ? game_word(g, W_DAMAGE,
	    (size_t)hurt) : game_word(g, W_SAILING, (size_t)(status >= 0 ?
	    status : b->full_sails ? SAILING_FULL_SAILS :
	    SAILING_BATTLE_SAILS)));
	draw(g, FONT_SMALL, PANEL_X, y + 102, 140, buf, 0);
}

static void
battle_speed(struct game *g, int y, const struct battle_ship *b)
{
	char buf[16];

	snprintf(buf, sizeof(buf), "%d", b->speed >> 2);
	draw(g, FONT_SMALL, 584, y, 56, buf, 0);
}

static void
battle_draw(struct game *g)
{
	const struct sim_battle *bt = &g->sim.battle;
	char text[600];
	int cx, cy, lvl;

	if (g->btl_stage == 1 && battle_end_picture(bt->outcome) != NULL) {
		battle_end_draw(g);
		return;
	}
	if (g->btl_stage == 2) {
		prize_draw(g);
		return;
	}
	if (g->btl_stage == 0)
		sea_turn(&g->sea, bt->tick / 8, g->fb.pal);
	/* The battle's screen: 15 x 15 battle tiles (VA 0x2a9e8). */
	if (g->btl_tiles.tile_w == 32)
		for (cy = 0; cy < BATTLE_SCREEN; cy++)
			for (cx = 0; cx < BATTLE_SCREEN; cx++) {
				struct image t;

				tileset_tile(&g->btl_tiles,
				    g->world->battle_screens[bt->screen][cy *
				    BATTLE_SCREEN + cx], 0, &t);
				fb_image(&g->fb, &t, cx * 32, cy * 32, 0);
			}
	battle_ship_draw(g, 0, &bt->you);
	battle_bow_mark(g, &bt->you);
	if (!bt->fort)		/* a fort is drawn in its screen (VA 0x8db6) */
		battle_ship_draw(g, 1, &bt->foe);
	/* Then the shots, and the bursts: on the player, on the enemy (VA
	 * 0xf6eb, objects 2-7). */
	battle_shot_draw(g, &bt->you.shot);
	battle_shot_draw(g, &bt->foe.shot);
	battle_burst_draw(g, &bt->foe.shot, &bt->you);
	battle_burst_draw(g, &bt->you.shot, &bt->foe);

	/* The panel: the enemy above, the player below, the wind between. */
	battle_side_text(g, 14, &bt->foe, bt->fort ? -2 : -1,
	    g->btl_hurt[1]);
	/* While the sails change, "Raising sail" or "Lowering sail" (VA
	 * 0xef06, event 10); reloading waits for them. */
	battle_side_text(g, 358, &bt->you, bt->you.sail_timer > 0 ?
	    (bt->you.full_sails ? SAILING_LOWERING : SAILING_RAISING) :
	    bt->you.reload > 0 ? SAILING_RELOADING : -1, g->btl_hurt[0]);
	/* A fort's damage shows in four stages of 20 (VA 0xebd7). */
	lvl = bt->fort ? (bt->fort_damage / 20 > 3 ? 3 : bt->fort_damage /
	    20) : bt->foe.damage / 16 > 4 ? 4 : bt->foe.damage / 16;
	if ((size_t)lvl < g->btl_damage[1].count)
		fb_frame(&g->fb, &g->btl_damage[1].frames[lvl], 489, 137);
	lvl = bt->you.damage / 16 > 4 ? 4 : bt->you.damage / 16;
	if (g->btl_damage[0].count == 5)
		fb_frame(&g->fb, &g->btl_damage[0].frames[lvl], 489, 258);
	/* Each ship's speed / 4, by the speed arrow of the picture (VA
	 * 0xf602, 0xf65c); a fort has none. */
	battle_speed(g, 398, &bt->you);
	if (!bt->fort)
		battle_speed(g, 54, &bt->foe);
	/* The wind (VA 0xee2b): its strength, and the compass pointing
	 * where it blows. */
	draw(g, FONT_SMALL, 524, 234, 60, game_word(g, W_WIND,
	    (size_t)(bt->wind_strength & 3)), 0);
	battle_misc(g, WIND_COMPASS + (bt->wind >> 4), 584, 229, 0);

	if (g->btl_stage == 0)
		return;

	/* The colours struck: written over the battle, no box. */
	battle_end_text(g, text, sizeof(text));
	draw(g, FONT_BODY, 100, 300, 460, text, 0);
	continue_prompt(g);
}

const struct screen screen_battle = {
	"battle", battle_enter, battle_event, battle_tick, battle_draw,
};

/*
 * Sword fight (boarding): the boarding picture, the weapon choice, then
 * the fight on the ship's deck.  The arrows move the fighter on a 3 x 3
 * pad, the opponent to the left: toward him attacks (a lunge, or a slash
 * with Shift), away steps back, up and down pick the line.  Clicking the
 * pad on the deck does the same.
 */
#define S_CHOOSE_WEAPON		85
#define W_WEAPON		70	/* Rapier, Longsword, Cutlass */
#define W_TITLE			74	/* Guard... Captain, First Mate */
#define TITLE_CAPTAIN		4
#define TITLE_FORMAT		6	/* a pack's "nation title" pattern */
#define SW_Y			140	/* fighters' line on the screen */
#define SW_FALL_DROP		24
#define SW_MOUSE_LEFT		308	/* the pointer's half-planes (VA */
#define SW_MOUSE_RIGHT		331	/* 0x27638): left of 308 attacks, */
#define SW_MOUSE_UP		313	/* right of 331 steps back, above */
#define SW_MOUSE_DOWN		334	/* 313 high, below 334 low */
#define SW_YOU_FLAG_X		584
#define SW_FOE_FLAG_X		10
#define H_LEFT			1
#define H_RIGHT			2
#define H_UP			4
#define H_DOWN			8
#define H_SHIFT			16

static const char *sw_layers[2][6] = {
	{ "grp/mybody", NULL, "grp/myhead", "grp/mychest", "grp/mylegs",
	    NULL },
	{ "grp/hebody", NULL, "grp/hehead", "grp/hechest", "grp/helegs",
	    "grp/hehead1" },
};
static const char *sw_weapon_layers[2][SWORD_WEAPONS] = {
	{ "grp/myrapier", "grp/mysword", "grp/mycutlas" },
	{ "grp/herapier", "grp/hesword", "grp/hecutlas" },
};

/*
 * Fighter layer "id" into a slot that remembers what it holds ("have"):
 * kept from the last fight when it is the same, so a fight starts at
 * once unless a weapon or a head changed.
 */
static void
load_group(struct game *g, const char *id, struct sprite_group *out,
    char *have, size_t size)
{
	if (id != NULL && strcmp(have, id) == 0)
		return;
	group_free(out);
	have[0] = '\0';
	if (id != NULL && group_load(g->packs, id, out) == 0)
		snprintf(have, size, "%s", id);
}

/* The story's fight (VA 0x14630): 200 men of factor 32 against 20 of
 * 16, no weapon to choose and nothing at stake. */
static void
sword_story_start(struct game *g)
{
	struct sword_setup in;

	memset(&in, 0, sizeof(in));
	in.kind = SWORD_STORY;
	in.you_men = 200;
	in.you_factor = 32;
	in.foe_men = 20;
	in.foe_factor = 16;
	in.title = 3;
	in.nation = 5;
	sim_sword_start(&g->sim, &in);
	game_background(g, "pic/shipdeck");
	g->ink[0] = g->ink[1] = g->ink[2] = 240;
	g->sw_stage = 2;
	g->sw_held = 0;
}

static void
sword_enter(struct game *g)
{
	struct blob b;

	game_music(g, MUS_STOP);
	g->sw_stage = 0;
	g->sw_held = 0;
	g->selected = SWORD_WEAPONS - 1;	/* the cutlass, named */
	g->sw_named = 1;
	game_background(g, "pic/boarding");
	g->ink[0] = g->ink[1] = g->ink[2] = 240;
	image_free(&g->sw_weapons);
	if (packs_load(g->packs, "img/wpnsel", &b) == 0) {
		pic_decode(g->packs, "img/wpnsel", &b, &g->sw_weapons);
		blob_free(&b);
	}
	load_bank(g, "spr/miscswrd", 0, &g->sw_misc);
	if (g->sw_suitor) {		/* in the alley (scene 0) */
		game_background(g, "pic/alley");
		g->sw_stage = 1;
	}
	if (g->sw_mutiny) {		/* on the deck (scene 3) */
		game_background(g, "pic/shipdeck");
		g->sw_stage = 1;
	}
	if (g->sw_guard || g->sw_villain) {
		/* The weapon is chosen on the alley itself (VA 0x16205). */
		game_background(g, "pic/alley");
		g->sw_stage = 1;
	}
	if (g->sw_practice) {		/* on the deck (scene 3) */
		game_background(g, "pic/shipdeck");
		g->sw_stage = 1;
	}
	if (g->sw_assault) {		/* the fort, or the alley */
		game_background(g, g->sim.towns[g->assault_town].forts > 0 ?
		    "pic/fort" : "pic/alley");
		g->sw_stage = 1;
	}
	if (g->sw_story) {
		sword_story_start(g);
		sword_load(g);
	}
}

/* The fight's assets, once both weapons are known. */
static void
sword_load(struct game *g)
{
	const char *id;
	int side, i;

	for (side = 0; side < 2; side++)
		for (i = 0; i < 6; i++) {
			id = i == 1 ? sw_weapon_layers[side][side == 0 ?
			    g->sim.sword.you.weapon : g->sim.sword.foe.weapon] :
			    side == 1 && i == 5 && g->sim.sword.scene == 0 ?
			    "grp/hehead2" : side == 1 && i == 5 &&
			    (g->sim.sword.scene == 1 || g->sim.sword.scene == 2) ?
			    "grp/hehead3" : sw_layers[side][i];
			load_group(g, id, &g->sw_parts[side][i],
			    g->sw_part_id[side][i],
			    sizeof(g->sw_part_id[side][i]));
		}
	g->sw_flag_y[0] = g->sw_flag_y[1] = 0;
	g->sw_wait = 1;
	g->sw_period = 1;
	g->sw_from[0] = g->sim.sword.you.x;
	g->sw_from[1] = g->sim.sword.foe.x;
	g->ink[0] = g->ink[1] = g->ink[2] = 240;	/* the fight's panel */
}

/* The blade under (x, y) on the weapon choice (VA 0x33628), or -1. */
static int
weapon_hit(const struct game *g, int x, int y)
{
	const int16_t *r;
	int i;

	for (i = 0; i < SWORD_WEAPONS; i++) {
		r = g->ui.weapon[i];
		if (x >= r[0] && x <= r[2] && y >= r[1] && y <= r[3])
			return (i);
	}
	return (-1);
}

/* The move the held keys ask for. */
static int
sword_code(int held)
{
	int line = held & H_UP ? 2 : held & H_DOWN ? 1 : 0;

	if (held & H_LEFT)
		return ((held & H_SHIFT ? SWM_SLASH : SWM_LUNGE) + line);
	if (held & H_RIGHT)
		return (SWM_BACK + line);
	return (SWM_GUARD + line);
}

static int
key_bit(int key)
{
	switch (key) {
	case UI_KEY_LEFT:
		return (H_LEFT);
	case UI_KEY_RIGHT:
		return (H_RIGHT);
	case UI_KEY_UP:
		return (H_UP);
	case UI_KEY_DOWN:
		return (H_DOWN);
	case UI_KEY_SHIFT:
		return (H_SHIFT);
	case UI_KEY_UP_LEFT:
		return (H_UP | H_LEFT);
	case UI_KEY_UP_RIGHT:
		return (H_UP | H_RIGHT);
	case UI_KEY_DOWN_LEFT:
		return (H_DOWN | H_LEFT);
	case UI_KEY_DOWN_RIGHT:
		return (H_DOWN | H_RIGHT);
	default:
		return (0);
	}
}

static void
sword_outcome_done(struct game *g)
{
	if (g->sw_story) {
		g->sw_story = 0;
		g->sim.sword.active = 0;
		game_goto(g, &screen_story);
		return;
	}
	if (g->sw_practice) {
		g->sw_practice = 0;
		sim_practice_end(&g->sim);
		game_goto(g, &screen_cabin);
		return;
	}
	if (g->sw_assault) {
		int r = sim_assault_end(&g->sim, g->assault_town);

		g->sw_assault = 0;
		if (r == 1) {
			sim_sack(&g->sim, g->assault_town, &g->sack);
			g->sack_stage = 0;
			game_goto(g, &screen_sack);
		} else
			game_goto(g, r < 0 ? &screen_prison : &screen_sail);
		return;
	}
	if (g->sw_suitor) {
		g->sw_suitor = 0;
		if (sim_daughter_duel_end(&g->sim, &g->daughter))
			game_goto(g, &screen_governor);	/* the wedding */
		else {
			g->gov_resume = 0;
			g->disgrace_back = &screen_town;
			game_goto(g, &screen_disgrace);
		}
		return;
	}
	if (g->sw_mutiny) {
		int lost = g->sim.sword.result != SWORD_WON;

		g->sw_mutiny = 0;
		sim_mutiny_end(&g->sim);
		g->sim.crew_event = CREW_NONE;
		g->disgrace_back = &screen_sail;
		game_goto(g, lost ? &screen_disgrace : &screen_sail);
		return;
	}
	if (g->sw_villain) {
		g->sw_villain = 0;
		if (sim_villain_duel_end(&g->sim)) {
			g->villain_stage = 1;
			game_goto(g, &screen_villain);
		} else {
			g->disgrace_back = g->villain_back;
			game_goto(g, &screen_disgrace);
		}
		return;
	}
	if (g->sw_guard) {
		g->sw_guard = 0;
		/* Back to sea either way (VA 0x195f8); a loss or a retreat
		 * is a disgrace (VA 0x165b8). */
		game_goto(g, sim_duel_end(&g->sim) ? &screen_disgrace :
		    &screen_sail);
		return;
	}
	if (g->sim.sword.result == SWORD_WON)
		game_goto(g, &screen_battle);	/* the prize */
	else if (g->sim.sword.result == SWORD_LOST) {
		/* Beaten: prison, "in the ship" at sea (VA 0x16647). */
		battle_finish(g, 0);
	} else
		battle_finish(g, 0);		/* driven back */
}

static void
sword_event(struct game *g, const struct ui_event *ev)
{
	int enter = (ev->type == EV_KEY && ev->key == UI_KEY_ENTER) ||
	    ev->type == EV_CLICK;
	int bit, cx, cy;

	switch (g->sw_stage) {
	case 0:
		if (enter) {
			game_background(g, "pic/shipdeck");
			g->ink[0] = g->ink[1] = g->ink[2] = 240;
			g->sw_stage = 1;
		}
		return;
	case 1:
		/* The blades left to right are the rapier, the cutlass and
		 * the longsword: the keys step along them without wrapping;
		 * pointing at a blade names it, clicking it takes it; Enter
		 * or Escape take the one named last (VA 0x16f03). */
		bit = ev->type == EV_MOVE || ev->type == EV_CLICK ?
		    weapon_hit(g, ev->x, ev->y) : -1;
		if (ev->type == EV_MOVE) {
			if (bit >= 0)
				g->selected = bit;
			g->sw_named = bit >= 0;
			return;
		}
		if (ev->type == EV_KEY && (ev->key == UI_KEY_LEFT ||
		    ev->key == UI_KEY_UP)) {
			g->selected = g->selected == 1 ? 2 : g->selected == 2 ?
			    0 : g->selected;
			g->sw_named = 1;
			return;
		}
		if (ev->type == EV_KEY && (ev->key == UI_KEY_RIGHT ||
		    ev->key == UI_KEY_DOWN)) {
			g->selected = g->selected == 0 ? 2 : g->selected == 2 ?
			    1 : g->selected;
			g->sw_named = 1;
			return;
		}
		if (ev->type == EV_CLICK) {
			if (bit < 0)
				return;
			g->selected = bit;
		} else if (!(ev->type == EV_KEY && (ev->key == UI_KEY_ENTER ||
		    ev->key == UI_KEY_ESCAPE)))
			return;
		if (g->sw_assault) {
			sim_assault_start(&g->sim, g->assault_town,
			    g->assault_aboard, g->selected);
			sword_load(g);
			g->sw_stage = 2;
		} else if (g->sw_practice) {
			sim_practice_start(&g->sim, g->selected);
			sword_load(g);
			g->sw_stage = 2;
		} else if (g->sw_guard) {
			sim_guard_duel(&g->sim, g->sim.fleet.leaving,
			    g->selected);
			sword_load(g);
			g->sw_stage = 2;
		} else if (g->sw_suitor) {
			sim_daughter_duel(&g->sim, &g->daughter, g->selected);
			sword_load(g);
			g->sw_stage = 2;
		} else if (g->sw_mutiny) {
			sim_mutiny_duel(&g->sim, g->selected);
			sword_load(g);
			g->sw_stage = 2;
		} else if (g->sw_villain) {
			sim_villain_duel(&g->sim, g->villain, g->selected);
			sword_load(g);
			g->sw_stage = 2;
		} else {
			sim_board(&g->sim, g->selected);
			sword_load(g);
			g->sw_stage = 2;
		}
		return;
	case 2:
		if (ev->type == EV_KEY && (bit = key_bit(ev->key)) != 0) {
			g->sw_held |= bit;
			if (bit != H_SHIFT)
				sim_sword_input(&g->sim, sword_code(g->sw_held));
		} else if (ev->type == EV_KEYUP)
			g->sw_held &= ~key_bit(ev->key);
		else if (ev->type == EV_CLICK || ev->type == EV_RCLICK) {
			/* Anywhere on the screen, by the pointer's side of
			 * the pad's centre; the right button slashes (VA
			 * 0x248dc). */
			cx = ev->x <= SW_MOUSE_LEFT ? H_LEFT :
			    ev->x >= SW_MOUSE_RIGHT ? H_RIGHT : 0;
			cy = ev->y <= SW_MOUSE_UP ? H_UP :
			    ev->y >= SW_MOUSE_DOWN ? H_DOWN : 0;
			sim_sword_input(&g->sim, sword_code(cx | cy |
			    (ev->type == EV_RCLICK ? H_SHIFT : 0)));
		}
		return;
	default:
		if (enter)
			sword_outcome_done(g);
		return;
	}
}

static void
sword_tick(struct game *g)
{
	const struct sword_tables *t;

	if (g->fading != 0 || g->sw_stage != 2 || g->world == NULL)
		return;
	if (--g->sw_wait > 0)
		return;
	/* A fight tick lasts 9-12 units of the sound driver's 60.9 Hz
	 * clock: taken as frames.  The original takes level 4's from
	 * level 0's (level & 3); the engine gives it level 3's, so the
	 * hardest level is not the slowest. */
	t = &g->world->sword;
	g->sw_wait = t->delay[g->sim.sword.level > 3 ? 3 :
	    g->sim.sword.level];
	if (g->sw_wait < 1)
		g->sw_wait = 1;
	g->sw_period = g->sw_wait;
	g->sw_from[0] = g->sim.sword.you.x;
	g->sw_from[1] = g->sim.sword.foe.x;
	{
		/* The blow's effect (VA 0x2747c). */
		const struct sim_sword *sw = &g->sim.sword;

		sim_sword_tick(&g->sim);
		play_sound(g, sw->sound, sw->effect);
	}
	if (g->sim.sword.result != SWORD_NONE) {
		sword_outcome_done(g);
	}
}

#define SW_JUMP		48

static void
sword_fighter(struct game *g, int side, const struct sword_side *f)
{
	static const int order[2][6] = {
		{ 0, 1, 2, 3, 4, -1 },	/* body, weapon, wounds */
		{ 0, 5, 1, 2, 3, 4 },	/* body, head, weapon, wounds */
	};
	const struct sword_tables *t = &g->world->sword;
	int move = f->move < 0 ? 0 : f->move & (SWORD_MOVES - 1);
	int frame = f->frame, y = SW_Y, i, layer, n = 1, w, x = f->x;
	const struct sprite_group *grp;

	/* --smooth: the fighter slides from the last fight tick's place to
	 * this one's over the tick, in 1/"n" pixels; the pose is this
	 * tick's.  Farther than SW_JUMP is a jump. */
	if (g->smooth && g->sw_stage == 2 && abs(f->x - g->sw_from[side]) <=
	    SW_JUMP) {
		n = g->sw_period;
		w = n - g->sw_wait;
		x = g->sw_from[side] * (n - w) + f->x * w;
	}

	if (frame >= t->frames[move])
		frame = t->frames[move] - 1;
	if (move == SWM_FALL && frame > 2)
		y -= SW_FALL_DROP;
	for (i = 0; i < 6; i++) {
		layer = order[side][i];
		/* The opponent's head is its own layer, always drawn: the
		 * deck's head here (hehead1; alleys and forts have theirs,
		 * VA 0x15f84). */
		if (layer < 0 ||
		    (layer >= 2 && layer <= 4 && f->wounds[layer - 2] == 0))
			continue;
		grp = &g->sw_parts[side][layer];
		if ((size_t)move < grp->count &&
		    (size_t)frame < grp->banks[move].count)
			fb_frame_sub(&g->fb, &grp->banks[move].frames[frame],
			    x, y * n, n);
	}
}

static void
sword_flag(struct game *g, int side, int x, int frame, int bar)
{
	const struct sword_tables *t = &g->world->sword;
	int target = t->flag_y[bar / 8 < 0 ? 0 : bar / 8 > 5 ? 5 : bar / 8];
	int *y = &g->sw_flag_y[side];

	/* Two pixels a frame, landing on the height even when it is an odd
	 * distance away (a pack's heights may be), not shaking about it. */
	if (*y == 0 || abs(target - *y) <= 2)
		*y = target;
	else if (*y < target)
		*y += 2;
	else
		*y -= 2;
	if ((size_t)frame < g->sw_misc.count)
		fb_frame(&g->fb, &g->sw_misc.frames[frame], x, *y);
}

/*
 * The pattern of a crew fighter's label, filled with the nation word
 * and the title: a pack's own (CSTN 74.6, which may reorder them with
 * %1$s and %2$s), else the original's: the nation first, but the French
 * captain after it, "Capitaine espagnol" (VA 0x16447); in Spanish,
 * every title before it, "Capitán inglés" (an engine choice).  The
 * German original drops the space; the engine keeps it.
 */
static const char *
title_format(const struct game *g, int title)
{
	const char *fmt = game_word(g, W_TITLE, TITLE_FORMAT);

	if (*fmt != '\0')
		return (fmt);
	if (title == TITLE_CAPTAIN && strcmp(g->lang, "fre") == 0)
		return ("%2$s %1$s");
	if (strcmp(g->lang, "spa") == 0)
		return ("%2$s %1$s");
	return ("%s %s");
}

static void
sword_draw(struct game *g)
{
	const struct sim_sword *sw = &g->sim.sword;
	char text[600], label[96];
	int w, nation;

	switch (g->sw_stage) {
	case 0:
		/* On the picture (VA 0xf10a; the original boxes it, VA
		 * 0xf170): at (274, 306) up to x 560, in palette entry 2. */
		if (g->bg.pixels != NULL)
			memcpy(g->ink, bg_colour(g, STORY_INK), 3);
		/* "...you spot %s approaching": a named captain by his
		 * name (VA 0xf130), else "the enemy captain". */
		if (g->sim.encounter.captain != 0)
			captain_name(g, g->sim.encounter.captain, label,
			    sizeof(label));
		else
			snprintf(label, sizeof(label), "%s", game_word(g,
			    W_SAILING, SAILING_CAPTAIN));
		fill(text, sizeof(text), game_str(g, S_BOARDING), label,
		    NULL);
		draw(g, FONT_BODY, 274, 306, 286, text, 0);
		continue_prompt(g);
		return;
	case 1:
		/* The picture, the prompt in its lower panel and the chosen
		 * weapon's name by its blade (VA 0x16e56, 0x16dda). */
		fb_image(&g->fb, &g->sw_weapons, 369, 56, 0);
		draw(g, FONT_BODY, 392, 354, 160,
		    game_str(g, S_CHOOSE_WEAPON), 0);
		if (g->bg.pixels != NULL)	/* ink entry 1 */
			memcpy(g->ink, bg_colour(g, STORY_INK), 3);
		if (g->sw_named) {	/* PIRATE1, centred on its blade */
			static const int name_x[SWORD_WEAPONS] = { 426, 506,
			    464 };
			const char *name = game_word(g, W_WEAPON,
			    (size_t)g->selected);

			w = text_width(&g->font[FONT_TITLE], name);
			draw(g, FONT_TITLE, name_x[g->selected] - w / 2, 178,
			    FB_W, name, 0);
		}
		return;
	default:
		break;
	}
	if (g->world == NULL)
		return;
	sword_fighter(g, 0, &sw->you);
	sword_fighter(g, 1, &sw->foe);
	nation = sw->nation < 0 || sw->nation > 4 ? 4 : sw->nation;
	/* The flags wave a frame per fight tick, not per screen frame. */
	sword_flag(g, 0, SW_YOU_FLAG_X, 6 + (int)(sw->counter & 7),
	    sw->you.bar);
	sword_flag(g, 1, SW_FOE_FLAG_X, 14 + 8 * nation + (int)(sw->counter &
	    7), sw->foe.bar);

	/* The nation leads the title only in crew fights against a known
	 * nation (VA 0x16205). */
	if (sw->nation == -2 && g->sim.villain_duel >= 0)
		villain_name(g, g->sim.villain_duel, label, sizeof(label));
	else if (sw->nation == -1)
		captain_name(g, sw->title, label, sizeof(label));
	else if (sw->kind == SWORD_CREW && sw->nation >= 0 && sw->nation != 5)
		fill(label, sizeof(label), title_format(g, sw->title),
		    game_word(g, W_NATION, (size_t)sw->nation),
		    game_word(g, W_TITLE, (size_t)sw->title));
	else
		snprintf(label, sizeof(label), "%s",
		    game_word(g, W_TITLE, (size_t)sw->title));
	w = text_width(&g->font[FONT_BODY], label);
	draw(g, FONT_BODY, 160 - w / 2, 336, 300, label, 0);
	w = text_width(&g->font[FONT_BODY], g->sim.player.name);
	draw(g, FONT_BODY, 480 - w / 2, 336, 300, g->sim.player.name, 0);
	/* The men are shown for kinds 0 and up, duels too (VA 0x16c5b):
	 * not in the story's fight nor in practice. */
	if (sw->kind >= 0) {
		snprintf(text, sizeof(text), "%s%d",
		    game_word(g, W_SAILING, SAILING_MEN), sw->foe.men);
		draw(g, FONT_BODY, 100, 380, 140, text, 0);
		snprintf(text, sizeof(text), "%s%d",
		    game_word(g, W_SAILING, SAILING_MEN), sw->you.men);
		draw(g, FONT_BODY, 420, 380, 140, text, 0);
	}

}

const struct screen screen_sword = {
	"sword", sword_enter, sword_event, sword_tick, sword_draw,
};

/*
 * Town (VA 0x1869b): the town picture with its buildings.  Pointing at
 * one shows its label on the scroll in place of the town's name; the
 * arrows step through them; Enter (or Escape) or a click goes in, Space
 * opens the captain's cabin.  The rectangles come from the executable.
 */
#define W_TOWN_SPOT		84	/* "Visit the Governor"... */
#define TOWN_SPOTS		7	/* plus the cabin, from the keyboard */
#define SPOT_GOVERNOR		0
#define SPOT_MERCHANT		1
#define SPOT_BANK		2
#define SPOT_TAVERN		3
#define SPOT_SHIPWRIGHT		4
#define SPOT_INFO		5
#define SPOT_LEAVE		6
#define SPOT_CABIN		7
#define TOWN_NAME_X		325
#define TOWN_NAME_Y		38
#define GOVERNOR_CITIZENS	6	/* fewer: no governor */

static int
town_rect(const struct game *g, int i, int r[4])
{
	int k;

	if (i < 0 || i >= UI_TOWN_SPOTS)
		return (-1);
	for (k = 0; k < 4; k++)
		r[k] = g->ui.town[i][k];
	return (0);
}

static const struct sim_town *
here(const struct game *g)
{
	return (g->sim.player_town >= 0 ? &g->sim.towns[g->sim.player_town] :
	    NULL);
}

/* Which buildings are open (VA 0x1869b, 0x18547). */
static int
town_open(const struct game *g, int i)
{
	const struct sim_town *t = here(g);

	switch (i) {
	case SPOT_GOVERNOR:
		return (t != NULL && t->citizens >= GOVERNOR_CITIZENS &&
		    !(g->sim.visit_flags & VISIT_GOVERNOR));
	case SPOT_BANK:
		return (!(g->sim.visit_flags & VISIT_SNEAKED));
	case SPOT_SHIPWRIGHT:		/* the ship must be in port */
		return (!g->sim.fleet.ashore);
	default:
		return (1);
	}
}

static int
town_hit(const struct game *g, int x, int y)
{
	int i, r[4];

	for (i = 0; i < TOWN_SPOTS; i++)
		if (town_open(g, i) && town_rect(g, i, r) == 0 &&
		    x >= r[0] && x <= r[2] && y >= r[1] && y <= r[3])
			return (i);
	return (-1);
}

static void
town_enter(struct game *g)
{
	char id[96];

	game_background(g, game_town_asset(g, "pic/town", id, sizeof(id)));
	game_portrait(g, NULL);
	load_bank(g, game_town_asset(g, "spr/town", id, sizeof(id)), 0,
	    &g->town_spr);
	/* Three days a pass, but none back from the city information or
	 * the cabin, nor on resuming a saved game (VA 0x18771). */
	if (g->town_free)
		g->town_free = 0;
	else
		sim_town_pass(&g->sim);
	g->town_spot = -1;
	g->ink[0] = g->ink[1] = g->ink[2] = 0;
	g->highlight[0] = g->highlight[1] = g->highlight[2] = 0;
}

static void
town_choose(struct game *g, int i)
{
	g->msg[0] = '\0';
	switch (i) {
	case SPOT_INFO:
		game_goto(g, &screen_cityinfo);
		break;
	case SPOT_SHIPWRIGHT:
		game_goto(g, &screen_shipwright);
		break;
	case SPOT_MERCHANT:
		game_goto(g, &screen_merchant);
		break;
	case SPOT_GOVERNOR:
		game_goto(g, &screen_governor);
		break;
	case SPOT_BANK:
		g->bank_stage = 0;
		game_goto(g, &screen_bank);
		break;
	case SPOT_TAVERN:
		sim_tavern_open(&g->sim);
		g->tavern_key = 6;
		game_goto(g, &screen_tavern);
		break;
	case SPOT_LEAVE:
		g->deserted_town = g->sim.player_town;
		sim_leave_town(&g->sim);
		bank_free(&g->town_spr);
		/* The roll call first (VA 0x17efb). */
		g->escape_page = g->sim.deserted <= 1;
		game_goto(g, g->sim.deserted > 1 || g->sim.escaped != 0 ?
		    &screen_deserted : g->world != NULL ? &screen_sail :
		    &screen_cabin);
		break;
	case SPOT_CABIN:
		game_goto(g, &screen_cabin);
		break;
	default:
		snprintf(g->msg, sizeof(g->msg), "(%s: not implemented yet.)",
		    game_word(g, W_TOWN_SPOT, (size_t)i));
		break;
	}
}

/* Step the keyboard selection, skipping closed buildings (VA 0x19cdd). */
static void
town_step(struct game *g, int d)
{
	int n;

	for (n = 0; n < TOWN_SPOTS; n++) {
		g->town_key += d;
		if (g->town_key < 0)
			g->town_key = TOWN_SPOTS - 1;
		else if (g->town_key >= TOWN_SPOTS)
			g->town_key = 0;
		if (town_open(g, g->town_key))
			break;
	}
	g->town_spot = g->town_key;
}

static void
town_event(struct game *g, const struct ui_event *ev)
{
	int i;

	switch (ev->type) {
	case EV_MOVE:
		g->town_spot = town_hit(g, ev->x, ev->y);
		if (g->town_spot >= 0)
			g->town_key = g->town_spot;
		break;
	case EV_CLICK:
		if ((i = town_hit(g, ev->x, ev->y)) >= 0)
			town_choose(g, i);
		break;
	case EV_KEY:
		if (ev->key == UI_KEY_LEFT || ev->key == UI_KEY_UP)
			town_step(g, -1);
		else if (ev->key == UI_KEY_RIGHT || ev->key == UI_KEY_DOWN)
			town_step(g, 1);
		else if ((ev->key == UI_KEY_ENTER ||
		    ev->key == UI_KEY_ESCAPE) && g->town_key >= 0 &&
		    town_open(g, g->town_key))
			town_choose(g, g->town_key);
		break;
	case EV_CHAR:
		if (ev->ch == ' ')
			town_choose(g, SPOT_CABIN);
		break;
	default:
		break;
	}
}

static void
town_draw(struct game *g)
{
	const struct sim_town *t = here(g);
	const char *label;
	int w;

	/* The harbour and the shipwright's shed when the ship is in port,
	 * the closed gate when the governor will not receive. */
	if (g->town_spr.count > 0 && !g->sim.fleet.ashore)
		fb_frame(&g->fb, &g->town_spr.frames[0], 0, 258);
	if (g->town_spr.count > 1 && !town_open(g, SPOT_GOVERNOR))
		fb_frame(&g->fb, &g->town_spr.frames[1], 582, 216);
	label = g->town_spot >= 0 ?
	    game_word(g, W_TOWN_SPOT, (size_t)g->town_spot) :
	    t != NULL ? t->name : "";
	w = text_width(&g->font[FONT_TITLE], label);
	draw(g, FONT_TITLE, TOWN_NAME_X - w / 2, TOWN_NAME_Y, FB_W, label, 0);
	if (g->msg[0] != '\0')		/* an engine note, in the ink */
		draw_centered(g, FONT_BODY, 444, g->msg);
}

const struct screen screen_town = {
	"town", town_enter, town_event, NULL, town_draw,
};

/*
 * City information (VA 0x19c62, 0x196d0): the arrival picture, by sea
 * or by land, and what the player knows of the town.
 */
#define W_CITY_INFO		23
#define W_NUMBER		79	/* 0 "s", then "One".."Five" */
#define W_WEALTH		9

static const struct sim_town *
info_town(const struct game *g)
{
	return (g->info_town >= 0 && (size_t)g->info_town < g->sim.ntowns ?
	    &g->sim.towns[g->info_town] : here(g));
}

static void
cityinfo_enter(struct game *g)
{
	const struct sim_town *t = info_town(g);

	/* By land for an inland town or a party on foot (VA 0x19c8a). */
	game_background(g, t != NULL && ((t->flags & TOWN_INLAND) ||
	    g->sim.fleet.ashore) ? "pic/arr_land" : "pic/arr_sea");
	game_portrait(g, NULL);
	g->ink[0] = 240;	/* light, on black */
	g->ink[1] = 220;
	g->ink[2] = 170;
}

static void
cityinfo_event(struct game *g, const struct ui_event *ev)
{
	if (ev->type == EV_CLICK || (ev->type == EV_KEY &&
	    (ev->key == UI_KEY_ENTER || ev->key == UI_KEY_ESCAPE))) {
		g->town_free = g->info_back == NULL;	/* VA 0x188b2 */
		game_goto(g, g->info_back != NULL ? g->info_back :
		    &screen_town);
		g->info_town = -1;
		g->info_back = NULL;
	}
}

/* The town's description as the game words it. */
static void
city_text(const struct game *g, const struct sim_town *t, char *out,
    size_t len)
{
	const char *arg[7];
	char forts[96], soldiers[16], citizens[16], gold[64], num[16];

	if (!(t->flags & 0x80)) {
		fill(out, len, game_word(g, W_CITY_INFO,
		    t->flags & 0x20 ? 4 : 1), t->name, NULL);
		return;
	}
	forts[0] = '\0';
	if (t->forts > 0)
		fill(forts, sizeof(forts), game_word(g, W_CITY_INFO, 2),
		    game_word(g, W_NUMBER, (size_t)(t->forts > 5 ? 5 :
		    t->forts)), t->forts == 1 ? "" : game_word(g, W_NUMBER, 0));
	gold[0] = '\0';
	if (t->gold > 0) {
		snprintf(num, sizeof(num), "%d", t->gold);
		fill(gold, sizeof(gold), game_word(g, W_CITY_INFO, 3), num,
		    NULL);
	}
	snprintf(soldiers, sizeof(soldiers), "%d", t->soldiers * 10);
	snprintf(citizens, sizeof(citizens), "%d", t->citizens * 100);
	arg[0] = t->name;
	arg[1] = game_word(g, W_NATION, (size_t)t->nation);
	arg[2] = game_word(g, W_WEALTH, (size_t)((t->wealth / 64) & 3));
	arg[3] = forts;
	arg[4] = soldiers;
	arg[5] = citizens;
	arg[6] = gold;
	fill_args(out, len, game_word(g, W_CITY_INFO, 0), arg, 7);
}

static void
cityinfo_draw(struct game *g)
{
	const struct sim_town *t = info_town(g);
	char text[512];

	if (t == NULL)
		return;
	city_text(g, t, text, sizeof(text));
	draw(g, FONT_BODY, 400, 150, 220, text, 0);
	continue_prompt(g);
}

const struct screen screen_cityinfo = {
	"cityinfo", cityinfo_enter, cityinfo_event, NULL, cityinfo_draw,
};

/*
 * Shipwright (VA 0x218c2): repair ships, sell ships, sell cannon.  The
 * original picks several ships at once on a fleet panel; here each
 * choice is one ship, and the list comes back until "None, thanks".
 */
#define S_NO_PIRATES		256
#define S_WRIGHT_MENU		277
#define S_ONLY_SHIP		279
#define S_NOTHING_WRONG		282
#define S_CANT_AFFORD		283
#define S_ONLY_CANNON		284
#define S_SHORT_FUNDS		319
#define W_WRIGHT		28	/* lists: titles, lines, "None" */
#define WRIGHT_TEXT_X		40
#define WRIGHT_TEXT_Y		300
#define WRIGHT_TEXT_W		540
#define WRIGHT_NOTE_X		100	/* his notes, up to x 540 (VA 0x21932) */
#define WRIGHT_NOTE_W		440

static void
wright_note(struct game *g, int cstr)
{
	g->wright_note = cstr;
	g->wright_stage = 3;
}

static void
shipwright_enter(struct game *g)
{
	game_music_as(g, MUS_TRADE, "music/shipwright");
	game_background(g, "pic/swright");
	game_portrait(g, NULL);
	g->ink[0] = 240;	/* light, on the dark blue */
	g->ink[1] = 220;
	g->ink[2] = 170;
	g->highlight[0] = 255;
	g->highlight[1] = 200;
	g->highlight[2] = 60;
	g->wright_stage = 0;
	if (sim_shipwright_refuses(&g->sim))
		wright_note(g, S_NO_PIRATES);
}

static int
any_damage(const struct game *g)
{
	int i;

	for (i = 0; i < g->sim.fleet.nships; i++)
		if (sim_repair_cost(&g->sim, i) > 0)
			return (1);
	return (0);
}

static void
wright_menu_choose(struct game *g, int i)
{
	switch (i) {
	case 0:		/* "Parting company" */
		game_goto(g, &screen_town);
		break;
	case 1:
		if (!any_damage(g))
			wright_note(g, S_NOTHING_WRONG);
		else
			g->wright_stage = 1;
		break;
	case 2:
		if (g->sim.fleet.nships < 2)
			wright_note(g, S_ONLY_SHIP);
		else
			g->wright_stage = 2;
		break;
	case 3:
		if (g->sim.fleet.guns < 2)
			wright_note(g, S_ONLY_CANNON);
		else
			game_goto(g, &screen_cannon);
		break;
	}
	g->selected = 0;
}

static void
wright_list_choose(struct game *g, int i)
{
	int r;

	if (i >= g->sim.fleet.nships) {		/* "None, thanks" */
		g->wright_stage = 0;
		g->selected = 0;
		return;
	}
	if (g->wright_stage == 1) {
		if (sim_repair_cost(&g->sim, i) == 0)
			return;
		r = sim_repair(&g->sim, 1u << i);
		if (r == REPAIR_TOO_DEAR)
			wright_note(g, S_CANT_AFFORD);
		else if (!any_damage(g))
			g->wright_stage = 0;
	} else {
		r = sim_sell_ships(&g->sim, 1u << i);
		if (r == SELL_SHORT)
			wright_note(g, S_SHORT_FUNDS);
		else if (g->sim.fleet.nships < 2)
			g->wright_stage = 0;
	}
	g->selected = 0;
}

static void
shipwright_event(struct game *g, const struct ui_event *ev)
{
	g->msg[0] = ev->type == EV_KEY || ev->type == EV_CLICK ? '\0' :
	    g->msg[0];
	switch (g->wright_stage) {
	case 0:
		menu_event(g, ev, 4, NULL, wright_menu_choose, NULL);
		break;
	case 1:
	case 2:
		menu_event(g, ev, g->sim.fleet.nships + 1, NULL,
		    wright_list_choose, NULL);
		break;
	default:
		if (ev->type == EV_CLICK || (ev->type == EV_KEY &&
		    (ev->key == UI_KEY_ENTER || ev->key == UI_KEY_ESCAPE))) {
			/* Refused: the note stays up while the town
			 * fades in. */
			if (g->wright_note == S_NO_PIRATES) {
				game_goto(g, &screen_town);
				return;
			}
			g->wright_stage = 0;
			g->selected = 0;
		}
		break;
	}
}

/* The ship list: "~%s%s: %s gold pieces", or "(no damage)". */
static void
wright_list(struct game *g, char *out, size_t len)
{
	const char *arg[3];
	char line[160], price[16];
	int i, cost;

	snprintf(out, len, "%s", game_word(g, W_WRIGHT,
	    g->wright_stage == 1 ? 1 : 0));
	for (i = 0; i < g->sim.fleet.nships; i++) {
		cost = g->wright_stage == 1 ? sim_repair_cost(&g->sim, i) :
		    sim_ship_price(&g->sim, i);
		arg[0] = game_word(g, W_SHIP_TYPE,
		    (size_t)(g->sim.fleet.ships[i] % 9));
		if (g->wright_stage == 1 && cost == 0) {
			fill(line, sizeof(line), game_word(g, W_WRIGHT, 4),
			    arg[0], NULL);
		} else {
			snprintf(price, sizeof(price), "%d", cost);
			arg[1] = g->sim.fleet.damage[i] >= 16 ?
			    game_word(g, W_SHIP_TYPE, 10) : "";
			arg[2] = price;
			fill_args(line, sizeof(line),
			    game_word(g, W_WRIGHT, 3), arg, 3);
		}
		strncat(out, "\r", len - strlen(out) - 1);
		strncat(out, line, len - strlen(out) - 1);
	}
	strncat(out, "\r", len - strlen(out) - 1);
	strncat(out, game_word(g, W_WRIGHT, 2), len - strlen(out) - 1);
}

static void
shipwright_draw(struct game *g)
{
	char text[1200];

	switch (g->wright_stage) {
	case 0:
		draw(g, FONT_BODY, WRIGHT_TEXT_X, WRIGHT_TEXT_Y, WRIGHT_TEXT_W,
		    game_str(g, S_WRIGHT_MENU), 1);
		break;
	case 1:
	case 2:
		wright_list(g, text, sizeof(text));
		draw(g, FONT_BODY, WRIGHT_TEXT_X, WRIGHT_TEXT_Y, WRIGHT_TEXT_W,
		    text, 1);
		break;
	default:
		draw(g, FONT_BODY, WRIGHT_NOTE_X, WRIGHT_TEXT_Y, WRIGHT_NOTE_W,
		    game_str(g, g->wright_note), 0);
		continue_prompt(g);
		return;
	}
	if (g->msg[0] != '\0')
		draw_centered(g, FONT_BODY, 450, g->msg);
}

const struct screen screen_shipwright = {
	"shipwright", shipwright_enter, shipwright_event, NULL,
	shipwright_draw,
};

/*
 * Merchant (VA 0x21e56): the greeting, then the trading board (VA
 * 0x20229): the merchant's side on the left, the player's on the
 * right, a row per good with its price, the merchant's stock and the
 * player's cargo, both purses and the space in the hold.  Up and Down
 * pick a good, Right buys one, Left sells one; a click on the left of a
 * row buys, on the right sells; Enter, Escape or the exit sign leaves.
 */
/* Rows: food, goods, sugar, and cannon on the loot board (VA 0x20229:
 * the merchant shows the first 23 layout records, the loot board all 29). */
#define BOARD_ROWS(g)		((g)->sim.market.plunder ? BOARD_GOODS : GOODS)
#define BOARD_PIECES(g)		((g)->sim.market.plunder ? 29 : 23)
#define BOARD_ROW_Y		116
#define BOARD_ROW_H		50
#define BOARD_SIGN		82	/* "Sell", then "Buy" */
#define BOARD_GAUGE		77
/* The slider's tracks (VA 0x34adc, 0x34aec): the board's rows, and the
 * shipwright's guns (row 4). */
#define TRACK_X0		249
#define TRACK_X1		374
#define BOARD_TRACK_Y		72
#define CANNON_TRACK_Y		318
#define GLYPH_BIG		0	/* 0-9, then highlighted 10-19 */
#define GLYPH_SMALL		20	/* '/', 0-9; 31 and 42: other inks */

/*
 * The pieces by set, then bank, then language (VA 0x20138, table at VA
 * 0x3487c; the Spanish, a pack's): the merchant's and the shipwright's,
 * then the loot board's (VA 0x2264f), whose sign reads Discard and Take.
 */
static const char *const board_files[2][3][GAME_LANGS] = {
	{ { "spr/buysell", "spr/buysellf", "spr/buysellg", "spr/buysells" },
	  { "spr/buyboard", "spr/buybrdf", "spr/buybrdg", "spr/buybrds" },
	  { "spr/buysell1", "spr/buysll1f", "spr/buysll1g", "spr/buysll1s" } },
	{ { "spr/take-dis", "spr/takedisf", "spr/takedisg", "spr/takediss" },
	  { "spr/takboard", "spr/takbrdf", "spr/takbrdg", "spr/takbrds" },
	  { "spr/takeds1", "spr/takeds1f", "spr/takeds1g", "spr/takeds1s" } },
};
/* The loot board's mode (VA 0x2264f): 1 overboard, 2 a capture, 3 a
 * sack; its name tag, layout record 0, is frame 88 + mode. */
#define BOARD_TAG		88
#define BOARD_MODE(g)		((g)->jettison ? 1 : (g)->prize_loot ? 2 : \
				    (g)->looting ? 3 : 0)

/* Layout record "i" (VA 0x348c4): frame, highlighted frame, x, y, and
 * whether the loot board shows it (VA 0x20229). */
static int
board_piece(const struct game *g, int i, int r[5])
{
	int k;

	if (i < 0 || i >= UI_BOARD_PIECES)
		return (-1);
	for (k = 0; k < 5; k++)
		r[k] = g->ui.board[i][k];
	return (0);
}

static void
board_frame(struct game *g, int bank, int frame, int x, int y)
{
	const struct sprite_bank *b = &g->shop_spr[bank];

	if (frame >= 0 && (size_t)frame < b->count)
		fb_frame(&g->fb, &b->frames[frame], x, y);
}

/* A number in the board's glyphs; "right" aligns it to "digits". */
static void
board_number(struct game *g, long v, int x, int y, int first, int w,
    int digits, int right)
{
	char buf[16];
	int i, n = snprintf(buf, sizeof(buf), "%ld", v < 0 ? 0 : v);

	if (right)
		x += (digits - n) * w;
	for (i = 0; i < n; i++, x += w)
		board_frame(g, 0, first + buf[i] - '0', x, y);
}

/* The good and the way chosen, with its sound (VA 0x21122). */
static void
merchant_row(struct game *g, int row, int buy)
{
	g->selected = row;
	g->shop_buying = buy;
	game_fm(g, FM_ROW);
}

static void
merchant_enter(struct game *g)
{
	int i;

	game_background(g, g->jettison || g->prize_loot ? "pic/plunder" :
	    g->looting ?
	    (g->sim.towns[g->assault_town].flags & TOWN_INLAND ?
	    "pic/arr_land" : "pic/arr_sea") : "pic/merchant");
	game_portrait(g, NULL);
	g->ink[0] = 240;
	g->ink[1] = 220;
	g->ink[2] = 170;
	for (i = 0; i < 3; i++)
		load_bank(g, game_lang_asset(g,
		    board_files[BOARD_MODE(g) != 0][i]), 0, &g->shop_spr[i]);
	g->selected = 0;
	g->shop_buying = 1;
	game_music(g, g->looting && !g->jettison ? MUS_PLUNDER : g->jettison ?
	    -1 : MUS_TRADE);
	if (g->jettison) {
		/* "You have more cargo than will fit in your hold! You
		 * must throw some overboard!" (CSTR 113, VA 0x1f65f), then
		 * the board, nothing to take. */
		static const int none[BOARD_GOODS] = { 0, 0, 0, 0 };

		sim_plunder_open(&g->sim, none);
		g->shop_stage = 3;
	} else if (g->prize_loot) {
		/* A capture's cargo and guns (VA 0xdb7b, board mode 2). */
		sim_plunder_open(&g->sim, g->sim.battle.loot);
		g->shop_stage = 1;
		merchant_row(g, 0, 1);
	} else if (g->looting) {	/* the loot, for nothing (VA 0x2264f) */
		int loot[BOARD_GOODS];

		memcpy(loot, g->sack.goods, sizeof(g->sack.goods));
		loot[3] = g->sack.cannon;	/* the forts' guns */
		sim_plunder_open(&g->sim, loot);
		g->shop_stage = 1;
		merchant_row(g, 0, 1);
	} else if (sim_merchant_refuses(&g->sim))
		g->shop_stage = 2;
	else {
		sim_merchant_open(&g->sim);
		g->shop_stage = 0;
	}
}

static void
merchant_leave(struct game *g)
{
	if (g->jettison && sim_hold_space(&g->sim) < 0)
		return;			/* not until the cargo fits */
	if (g->looting) {
		int thrown = g->jettison;

		g->looting = 0;
		g->jettison = 0;
		sim_plunder_close(&g->sim);
		if (g->prize_loot) {
			g->prize_loot = 0;
			game_goto(g, &screen_battle_end);
			return;
		}
		/* After a sack, the townsfolk's news of villains (VA
		 * 0x18b3f). */
		if (thrown || !vnews_offer(g, 0, g->assault_town, &screen_sail))
			game_goto(g, &screen_sail);
		return;
	}
	sim_merchant_close(&g->sim);
	/* After the business: dinner with the wife, or his daughter
	 * (VA 0x21e56). */
	switch (sim_merchant_after(&g->sim)) {
	case 2:
		g->merdaugh = 295;
		break;
	case 1:
		g->merdaugh = 296;
		break;
	default:
		g->merdaugh = 0;
		break;
	}
	g->selected = 0;
	game_goto(g, g->merdaugh != 0 ? &screen_merdaugh : &screen_town);
}

static void
merchant_trade(struct game *g, int row, int buy)
{
	g->selected = row;
	g->shop_buying = buy;
	/* Each unit that changes hands clicks (VA 0x20ad4, 0x20bcd). */
	if (sim_trade(&g->sim, row, buy ? 1 : -1) != 0)
		game_fm(g, FM_TRADE);
}

static void
merchant_event(struct game *g, const struct ui_event *ev)
{
	int go = ev->type == EV_CLICK || (ev->type == EV_KEY &&
	    (ev->key == UI_KEY_ENTER || ev->key == UI_KEY_ESCAPE));
	int row;

	if (g->shop_stage != 1) {
		if (go) {
			if (g->shop_stage == 2)
				game_goto(g, &screen_town);
			else {
				g->shop_stage = 1;
				merchant_row(g, 0, 0);	/* the board opens */
			}
		}
		return;
	}
	if (ev->type == EV_CLICK) {
		/* The board's areas (VA 0x20515, table at VA 0x34b04). */
		int x = ev->x, y = ev->y;

		if ((x >= 226 && x <= 248 && y >= 62 && y <= 90) ||
		    (x <= 83 && y <= 70))
			merchant_trade(g, g->selected, 0);	/* sell */
		else if ((x >= 386 && x <= 410 && y >= 62 && y <= 90) ||
		    (x >= 557 && y <= 70))
			merchant_trade(g, g->selected, 1);	/* buy */
		else if (x >= 321 && x <= 401 && y >= 100 && y <= 126)
			sim_take_all(&g->sim, g->selected);	/* Take all */
		else if (x >= 252 && x <= 391 && y >= 433)
			merchant_leave(g);			/* the exit */
		else if (y >= 107 && (row = (y - 107) / 50) < BOARD_ROWS(g) &&
		    (y - 107) % 50 <= 40 &&
		    ((x >= 169 && x <= 190) || (x >= 444 && x <= 465))) {
			/* A row's arrow: the good and the way, no trade
			 * yet (VA 0x21122). */
			merchant_row(g, row, x >= 444);
		}
		return;
	}
	if (ev->type != EV_KEY)
		return;
	switch (ev->key) {
	case UI_KEY_UP:		/* no wrapping (VA 0x206bb) */
		if (g->selected > 0)
			merchant_row(g, g->selected - 1, g->shop_buying);
		break;
	case UI_KEY_DOWN:
		if (g->selected < BOARD_ROWS(g) - 1)
			merchant_row(g, g->selected + 1, g->shop_buying);
		break;
	case UI_KEY_RIGHT:
		merchant_trade(g, g->selected, 1);
		break;
	case UI_KEY_LEFT:
		merchant_trade(g, g->selected, 0);
		break;
	case UI_KEY_ENTER:
	case UI_KEY_ESCAPE:
		merchant_leave(g);
		break;
	default:
		break;
	}
}

/*
 * The slider (VA 0x20ff3): along the track, the share of the row's
 * units on the player's side, in the original's single floats: the
 * track's width over the units, times the player's, truncated.
 */
static void
board_slider(struct game *g, int stock, int held, int y)
{
	float step;
	int x = 0;

	if (stock + held > 0) {
		step = (float)(TRACK_X1 - TRACK_X0) / (float)(stock + held);
		x = (int)((float)held * step);
	}
	board_frame(g, 0, BOARD_GAUGE, TRACK_X0 + x, y);
}

static void
merchant_draw(struct game *g)
{
	const struct sim_market *m = &g->sim.market;
	const struct sim_fleet *f = &g->sim.fleet;
	int i, r[5], row, sel, held, room;

	if (g->shop_stage != 1) {
		/* On the picture, no box, in ink 0 (VA 0x21e56): at (160,
		 * 40) up to x 480. */
		uint8_t ink[3];

		memcpy(ink, g->ink, 3);
		if (g->bg.pixels != NULL)
			memcpy(g->ink, g->bg.palette, 3);
		draw(g, FONT_BODY, 160, 40, 320, game_str(g,
		    g->shop_stage == 3 ? 113 : g->shop_stage == 2 ?
		    S_NO_PIRATES : sim_merchant_greeting(&g->sim)), 0);
		memcpy(g->ink, ink, 3);
		continue_prompt(g);
		return;
	}
	board_frame(g, 1, 0, 203, 17);
	/* "... gold pieces plundered!" (CSTN 10.6), not over cargo thrown
	 * overboard (VA 0x20229: the line is empty there). */
	if (g->looting && !g->jettison) {
		char line[96];

		snprintf(line, sizeof(line), "%ld%s", g->prize_loot ?
		    g->sim.battle.loot_gold : g->sack.gold,
		    game_word(g, W_PLUNDERED, PLUNDERED_WORD));
		draw_centered(g, FONT_BODY, 400, line);
	}
	for (i = 0; i < BOARD_PIECES(g); i++) {
		if (board_piece(g, i, r) != 0 ||
		    (g->sim.market.plunder && r[4] == 0))
			continue;
		row = i < 5 ? -1 : (i - 5) / 6;
		if (i == 0)
			r[0] = r[1] = BOARD_TAG + BOARD_MODE(g);
		board_frame(g, 0, row == g->selected ? r[1] : r[0], r[2], r[3]);
	}
	board_frame(g, 0, BOARD_SIGN + g->shop_buying, 235, 101);
	board_frame(g, 2, 0, 252, 430);
	for (i = 0; i < BOARD_ROWS(g); i++) {
		sel = i == g->selected;
		held = i == 0 ? f->food : i == 1 ? f->goods : i == 2 ?
		    f->sugar : f->guns;
		/* The merchant's stock, the player's cargo, and but on
		 * the loot board the price on both sides (VA 0x207f1,
		 * 0x20940). */
		board_number(g, m->stock[i], 106, 117 + i * BOARD_ROW_H,
		    GLYPH_BIG + (sel ? 10 : 0), 15, 4, 1);
		board_number(g, held, 475, 117 + i * BOARD_ROW_H,
		    GLYPH_BIG + (sel ? 10 : 0), 15, 4, 0);
		if (g->sim.market.plunder)
			continue;
		board_number(g, m->price[i], 71, 119 + i * BOARD_ROW_H,
		    GLYPH_SMALL + 1 + (sel ? 11 : 0), 7, 3, 1);
		board_number(g, m->price[i], 534, 119 + i * BOARD_ROW_H,
		    GLYPH_SMALL + 1 + (sel ? 11 : 0), 7, 3, 1);
	}
	/* The purses (VA 0x213c4) and the space in the hold (0x21612). */
	board_number(g, g->sim.merchant_gold > 999999 ? 999999 :
	    g->sim.merchant_gold, 20, 84, GLYPH_SMALL + 1, 8, 6, 1);
	board_number(g, f->gold > 999999 ? 999999 : f->gold, 576, 84,
	    GLYPH_SMALL + 1, 8, 6, 1);
	room = sim_hold_space(&g->sim);
	board_number(g, room, 336, 138, GLYPH_SMALL + 23, 8, 4, 1);
	/* The slider: the selected good's share on the player's side. */
	sel = g->selected;
	held = sel == 0 ? f->food : sel == 1 ? f->goods : sel == 2 ?
	    f->sugar : f->guns;
	board_slider(g, m->stock[sel], held, BOARD_TRACK_Y);
}

const struct screen screen_merchant = {
	"merchant", merchant_enter, merchant_event, NULL, merchant_draw,
};

/*
 * Tavern (VA 0x11867): the tavern's name on the scroll (CSTN 96, by
 * town) and ten spots: the map seller, the information seller, three
 * groups of seamen to recruit, the bartender's rumours, the barmaid, a
 * stranger, and the door.  People leave once spoken to and are back on
 * the next arrival.  The arrows step through them, Enter goes, Space
 * opens the cabin.
 */
#define W_TAVERN_NAME		96
#define W_REPUTATION		34	/* cowardly .. infamous */
#define W_GOSSIP		33	/* "your promising plans"... */
#define S_RECRUITS		151
#define S_NO_RECRUITS		309
#define S_GOSSIP		125
#define W_TAVERN_SPOT		90	/* Map Seller, Traveller... */
#define TAVERN_MAPMAN		0
#define TAVERN_INFOMAN		1
#define TAVERN_BARTENDER	3
#define TAVERN_BARMAID		4
#define TAVERN_STRANGER		5
#define TAVERN_DOOR		6
#define RUMOURS_SHOWN		12

static int
tavern_rect(const struct game *g, int i, int r[4])
{
	int k;

	if (i < 0 || i >= UI_TAVERN_SPOTS)
		return (-1);
	for (k = 0; k < 4; k++)
		r[k] = g->ui.tavern[i][k];
	return (0);
}

static int
tavern_open_spot(const struct game *g, int i)
{
	return (i == TAVERN_DOOR || (g->sim.tavern_spots & (1u << i)));
}

static int
tavern_hit(const struct game *g, int x, int y)
{
	int i, r[4];

	for (i = 0; i < TAVERN_SPOTS; i++)
		if (tavern_open_spot(g, i) && tavern_rect(g, i, r) == 0 &&
		    x >= r[0] && x <= r[2] && y >= r[1] && y <= r[3])
			return (i);
	return (-1);
}

static void
tavern_enter(struct game *g)
{
	char id[96];

	game_music(g, MUS_TAVERN);
	game_background(g, game_town_asset(g, "pic/tavern", id, sizeof(id)));
	game_portrait(g, NULL);
	load_bank(g, game_town_asset(g, "spr/tavern", id, sizeof(id)), 0,
	    &g->tavern_spr);
	g->tavern_spot = -1;
	g->ink[0] = g->ink[1] = g->ink[2] = 0;
	/* Back from the mystery man: still there while a villain hides
	 * here or the contact is due (VA 0x119de). */
	sim_tavern_stranger(&g->sim);
}

static void
tavern_choose(struct game *g, int i)
{
	g->msg[0] = '\0';
	if (i == TAVERN_DOOR) {
		bank_free(&g->tavern_spr);
		game_goto(g, &screen_town);
	} else if (TAVERN_CREW & (1u << i))
		game_goto(g, &screen_recruit);
	else if (i == TAVERN_BARTENDER)
		game_goto(g, &screen_rumours);
	else if (i == TAVERN_MAPMAN)
		game_goto(g, &screen_mapman);
	else if (i == TAVERN_INFOMAN)
		game_goto(g, &screen_traveller);
	else if (i == TAVERN_BARMAID)
		game_goto(g, &screen_barmaid);
	else if (i == SPOT_CABIN)
		game_goto(g, &screen_cabin);
	else if (i == TAVERN_STRANGER) {
		/* The mystery man (VA 0x119b5): a villain, else the
		 * mission's contact; he leaves when neither is left. */
		g->villain_back = &screen_tavern;
		g->villain = sim_villain_at(&g->sim, g->sim.player_town);
		if (g->villain >= 0) {
			g->villain_stage = 0;
			game_goto(g, &screen_villain);
		} else if (sim_mission_contact(&g->sim,
		    g->sim.player_town)) {
			sim_mission_done(&g->sim);
			game_goto(g, &screen_contact);
		} else
			sim_tavern_stranger(&g->sim);
	} else
		snprintf(g->msg, sizeof(g->msg), "(Not implemented yet.)");
}

static void
tavern_step(struct game *g, int d)
{
	int n;

	for (n = 0; n < TAVERN_SPOTS; n++) {
		g->tavern_key = (g->tavern_key + d + TAVERN_SPOTS) %
		    TAVERN_SPOTS;
		if (tavern_open_spot(g, g->tavern_key))
			break;
	}
}

static void
tavern_event(struct game *g, const struct ui_event *ev)
{
	int i;

	switch (ev->type) {
	case EV_CLICK:
		if ((i = tavern_hit(g, ev->x, ev->y)) >= 0)
			tavern_choose(g, i);
		break;
	case EV_MOVE:
		/* Pointing at someone names them on the scroll (VA
		 * 0x11acb, CSTN 90). */
		g->tavern_spot = i = tavern_hit(g, ev->x, ev->y);
		if (i >= 0)
			g->tavern_key = i;
		break;
	case EV_KEY:
		if (ev->key == UI_KEY_LEFT || ev->key == UI_KEY_UP)
			tavern_step(g, -1);
		else if (ev->key == UI_KEY_RIGHT || ev->key == UI_KEY_DOWN)
			tavern_step(g, 1);
		else if (ev->key == UI_KEY_ENTER || ev->key == UI_KEY_ESCAPE)
			tavern_choose(g, g->tavern_key);
		break;
	case EV_CHAR:
		if (ev->ch == ' ')
			tavern_choose(g, SPOT_CABIN);
		break;
	default:
		break;
	}
}

static void
tavern_draw(struct game *g)
{
	static const int who[3] = { TAVERN_MAPMAN, TAVERN_INFOMAN,
	    TAVERN_STRANGER };
	static const int at[3][2] = { { 0, 255 }, { 142, 259 },
	    { 349, 199 } };
	const char *name;
	int i, w;

	for (i = 0; i < 3; i++)
		if ((size_t)i < g->tavern_spr.count &&
		    (g->sim.tavern_spots & (1u << who[i])))
			fb_frame(&g->fb, &g->tavern_spr.frames[i], at[i][0],
			    at[i][1]);
	name = g->tavern_spot >= 0 ? game_word(g, W_TAVERN_SPOT,
	    (size_t)g->tavern_spot) : game_word(g, W_TAVERN_NAME,
	    (size_t)g->sim.player_town);
	w = text_width(&g->font[FONT_TITLE], name);
	draw(g, FONT_TITLE, 320 - w / 2, TOWN_NAME_Y, FB_W, name, 0);
	if (g->msg[0] != '\0')		/* an engine note, in the ink */
		draw_centered(g, FONT_BODY, 444, g->msg);
}

const struct screen screen_tavern = {
	"tavern", tavern_enter, tavern_event, NULL, tavern_draw,
};

/* Recruiting (VA 0x10f04): "Look! It's that famous sea-dog..." */
static void
recruit_enter(struct game *g)
{
	game_background(g, "pic/crew");
	game_portrait(g, NULL);
	/* The tavern's inks (VA 0x11867): palette entries 2 and 6. */
	if (g->bg.pixels != NULL) {
		memcpy(g->ink, bg_colour(g, STORY_INK), 3);
		memcpy(g->highlight, bg_colour(g, STORY_HIGHLIGHT), 3);
	}
}

static void
recruit_choose(struct game *g, int i)
{
	sim_recruit(&g->sim, i == 0);
	game_goto(g, &screen_tavern);
}

static void
recruit_event(struct game *g, const struct ui_event *ev)
{
	if (g->sim.recruits > 0) {
		menu_event(g, ev, 2, NULL, recruit_choose, NULL);
		return;
	}
	if (ev->type == EV_CLICK || (ev->type == EV_KEY &&
	    (ev->key == UI_KEY_ENTER || ev->key == UI_KEY_ESCAPE)))
		recruit_choose(g, 1);
}

static void
recruit_draw(struct game *g)
{
	const char *arg[4];
	const struct sim_town *t = here(g);
	char text[600], men[16];
	int r = g->sim.reputation / 4 + 1;

	/* On the picture, no box (VA 0x10f04): at (60, 300) up to x 620,
	 * the choice up to x 560. */
	if (g->sim.recruits == 0) {
		draw(g, FONT_BODY, 60, 300, 560, game_str(g, S_NO_RECRUITS), 0);
		continue_prompt(g);
		return;
	}
	snprintf(men, sizeof(men), "%d", g->sim.recruits);
	arg[0] = game_word(g, W_REPUTATION, (size_t)(r < 0 ? 0 : r > 5 ? 5 :
	    r));
	/* The rank with the town's nation (VA 0x10f2f). */
	arg[1] = game_word(g, W_RANK, t != NULL && t->nation <= NATION_PIRATE ?
	    g->sim.rank[t->nation] : 0);
	arg[2] = g->sim.player.name;
	arg[3] = men;
	fill_args(text, sizeof(text), game_str(g, S_RECRUITS), arg, 4);
	draw(g, FONT_BODY, 60, 300, 500, text, 1);
}

const struct screen screen_recruit = {
	"recruit", recruit_enter, recruit_event, NULL, recruit_draw,
};

/*
 * Rumours (VA 0x1183e): the bartender passes on the latest news, or,
 * with none, listens to the player's last exploit (CSTR 125).
 */
static void
rumours_enter(struct game *g)
{
	char id[96];

	game_background(g, game_town_asset(g, "pic/bartenda", id,
	    sizeof(id)));
	game_portrait(g, NULL);
	/* Ink entry 0 (VA 0x1e78d): palette index 0. */
	if (g->bg.pixels != NULL)
		memcpy(g->ink, g->bg.palette, 3);
}

static void
rumours_event(struct game *g, const struct ui_event *ev)
{
	struct sim_news pub[1];

	if (!(ev->type == EV_CLICK || (ev->type == EV_KEY &&
	    (ev->key == UI_KEY_ENTER || ev->key == UI_KEY_ESCAPE))))
		return;
	/* With news he stays.  Without, after the gossip, what he knows of
	 * villains, and he leaves for the stay (VA 0x1184c, 0x10b10,
	 * 0x1eba6). */
	if (public_news(g, pub, 1) == 0) {
		g->sim.tavern_spots &= ~TAVERN_BAR;
		if (vnews_offer(g, 1, g->sim.player_town, &screen_tavern))
			return;
	}
	game_goto(g, &screen_tavern);
}

/*
 * The player's last exploit, for the bartender (VA 0x10b10): from the
 * newest log entry back (a visit at the head passed over), the first
 * of codes 1-8 or 22-26; none, or a visit to this very town, is "your
 * promising plans" (CSTN 33.0).
 */
static void
gossip_text(const struct game *g, char *out, size_t len)
{
	const struct sim_news *n = NULL;
	size_t i = sim_news_kept(&g->sim);
	char word[160];
	int code = 0;

	if (i > 0 && sim_news_at(&g->sim, i - 1)->code == NEWS_VISITED)
		i--;
	for (; i > 0; i--) {
		n = sim_news_at(&g->sim, i - 1);
		if ((n->code < 9 || n->code > 21) && n->code <= 26)
			break;
	}
	if (i > 0 && !(n->code == NEWS_VISITED &&
	    n->a == g->sim.player_town))
		code = n->code & 0x3f;
	switch (code) {
	case NEWS_VISITED:
	case NEWS_PLUNDERED:
	case NEWS_NOT_TAKEN:
		fill(word, sizeof(word), game_word(g, W_GOSSIP, (size_t)code),
		    town_name(g, n->a), NULL);
		break;
	case NEWS_TITLED:
		fill(word, sizeof(word), game_word(g, W_GOSSIP, (size_t)code),
		    game_word(g, 68, (size_t)(n->a / 64) % 5),
		    game_word(g, 78, (size_t)n->a & 63));
		break;
	case NEWS_CAPTURED:
	case 5:
	case NEWS_ENGAGED:
		/* The class as written: the tolower at VA 0x10ce5 is
		 * discarded. */
		fill(word, sizeof(word), game_word(g, W_GOSSIP, (size_t)code),
		    game_word(g, 68, (size_t)(n->a / 16) % 5),
		    game_word(g, 8, (size_t)n->a & 15));
		break;
	case NEWS_TAKEN:
		fill(word, sizeof(word), game_word(g, W_GOSSIP, (size_t)code),
		    town_name(g, n->a), game_word(g, 71, (size_t)n->b & 3));
		break;
	case NEWS_RESCUED:
		fill(word, sizeof(word), game_word(g, W_GOSSIP, (size_t)code),
		    game_word(g, 37, (size_t)n->a & 3), NULL);
		break;
	default:
		snprintf(word, sizeof(word), "%s", game_word(g, W_GOSSIP,
		    (size_t)code));
	}
	fill(out, len, game_str(g, S_GOSSIP), word, NULL);
}

static void
rumours_draw(struct game *g)
{
	char text[512];
	struct sim_news pub[RUMOURS_SHOWN];
	size_t i, np = public_news(g, pub, RUMOURS_SHOWN);
	int y = 40;

	/* No box: the news at (222, 40) up to x 610 (VA 0x1e78d), the
	 * gossip at (320, 150) up to x 560 (VA 0x10b10). */
	if (np == 0) {
		gossip_text(g, text, sizeof(text));
		draw(g, FONT_BODY, 320, 150, 240, text, 0);
	} else {
		y += draw(g, FONT_BODY, 222, y, 388, game_word(g, W_NEWS, 1),
		    0) + 6;
		/* The latest public items, oldest first (VA 0x1e548). */
		for (i = np; i > 0 && y < 400; i--) {
			news_text(g, &pub[i - 1], text, sizeof(text));
			y += draw(g, FONT_BODY, 222, y, 388, text, 0) + 4;
		}
	}
	continue_prompt(g);
}

const struct screen screen_rumours = {
	"rumours", rumours_enter, rumours_event, NULL, rumours_draw,
};

/*
 * Governor (VA 0x13269): the political news (VA 0x11d64), a promotion
 * when earned (VA 0x12549), then a Letter of Marque or a pardon for
 * gold (VA 0x12d6d).  He receives the player once a stay.  His
 * missions, prisoners, ransoms, the family quest and his daughter are
 * not done yet.
 */
#define W_GOV			25	/* the greeting's pieces */
#define W_COUNTRY		81	/* Spain, England, France, Holland */
#define W_NATION_A		104	/* "a Spanish", "an English"... */
#define S_GOV_CHAT		149
#define S_PROMOTION		134
#define S_LAND			135
#define S_MARQUE		147
#define S_PARDON		148
#define S_NO_FUNDS		94
#define GOV_TEXT_X		320
#define GOV_TEXT_Y		150
#define GOV_TEXT_W		300
#define S_MISSION_FAILED	199
#define S_MISSION_ASK		200
#define S_MISSION_TASK		200	/* + type: 201-204 */
#define S_MISSION_THANKS	209
#define S_PIRATE_BROUGHT	133
#define S_VILLAIN_NEWS		137
#define S_VILLAIN_SEEN		138
enum { GOV_GREET, GOV_MISSION, GOV_REPORT, GOV_ASK, GOV_TASK, GOV_PROMOTED, GOV_LAND,
    GOV_VILLAIN_NEWS, GOV_VILLAIN_SEEN, GOV_OFFER, GOV_SHORT, GOV_DONE,
    GOV_RANSOM, GOV_DAUGHTER, GOV_SAID, GOV_CHALLENGE, GOV_WEDDING,
    GOV_INFORM, GOV_INFORM_FLEET, GOV_INFORM_TRAIN };
#define S_DAUGHTER		141
#define S_WEDDING		145
#define S_DAUGHTER_WON		146
#define W_DAUGHTER		89	/* daughter, niece */
#define W_INFORM		26
#define S_RANSOM		136

/* A captain's name from his id (VA 0x19fbb): the rank (CSTN 78, from
 * Captain), and a surname of the list CSTN 2 + (id >> 6 & 3). */
static void
captain_name(const struct game *g, int id, char *out, size_t len)
{
	snprintf(out, len, "%s %s", game_word(g, W_RANK,
	    (size_t)((id & 7) + 2)), game_word(g, 2 + (id >> 6 & 3),
	    (size_t)(id >> 3 & 7)));
}

/* A villain's name (VA 0x1a040): his rank and his nation's surname. */
static void
villain_name(const struct game *g, int k, char *out, size_t len)
{
	const struct sim_villain *v = &g->sim.villains[k & 3];

	snprintf(out, len, "%s %s", game_word(g, W_RANK, (size_t)v->rank),
	    game_word(g, 2 + v->nation, (size_t)v->surname));
}

#define W_VILLAIN_NATION	97	/* Spaniard, Englishman... */
#define W_RELATIVE_WHO		37	/* sister, father, niece, uncle */

/* The texts about a villain: 131 met, 132 beaten, 137 named, 138 where
 * he was last seen. */
static void
villain_text(struct game *g, int k, int str, char *out, size_t len)
{
	const struct sim_villain *v = &g->sim.villains[k & 3];
	const char *arg[4];
	char name[80];

	villain_name(g, k, name, sizeof(name));
	switch (str) {
	case 131:
		arg[0] = game_word(g, W_VILLAIN_NATION, (size_t)v->nation);
		arg[1] = name;
		fill_args(out, len, game_str(g, str), arg, 2);
		break;
	case 132:
		arg[0] = game_word(g, W_RELATIVE_WHO,
		    (size_t)(g->sim.relative & 3));
		fill_args(out, len, game_str(g, str), arg, 1);
		break;
	case 137:
		arg[0] = arg[3] = game_word(g, W_RELATIVE_WHO,
		    (size_t)(g->sim.relative & 3));
		arg[1] = game_word(g, W_VILLAIN_NATION, (size_t)v->nation);
		arg[2] = name;
		fill_args(out, len, game_str(g, str), arg, 4);
		break;
	default:
		arg[0] = (size_t)v->town < g->sim.ntowns ?
		    g->sim.towns[v->town].name : "";
		fill_args(out, len, game_str(g, str), arg, 1);
		break;
	}
}

static void
governor_inks(struct game *g, const char *pic)
{
	char id[96];

	game_background(g, game_town_asset(g, pic, id, sizeof(id)));
	game_portrait(g, NULL);
	g->ink[0] = 240;
	g->ink[1] = 220;
	g->ink[2] = 170;
	g->highlight[0] = 255;
	g->highlight[1] = 200;
	g->highlight[2] = 60;
}

static void	governor_next(struct game *);

static void
governor_enter(struct game *g)
{
	const struct sim_town *t;
	int k, enemies = 0;

	governor_inks(g, "pic/governor");
	if (g->gov_resume) {
		g->gov_resume = 0;
		if (g->gov_stage == GOV_WEDDING) {
			/* The suitor beaten: the wedding (VA 0x128c1). */
			governor_inks(g, "pic/wedding");
			game_music(g, MUS_WEDDING);
			return;
		}
		/* Back from the villain (VA 0x13269 goes on with the
		 * missions). */
		governor_next(g);
		return;
	}
	g->sim.visit_flags |= VISIT_GOVERNOR;
	/* His news, a call to arms when his nation has enemies and the
	 * player's standing is 2 or more (VA 0x1208e). */
	t = here(g);
	for (k = 0; t != NULL && t->nation < 4 && k < 4; k++)
		enemies += g->sim.relations[t->nation][k] == -1;
	game_music(g, enemies > 0 && g->sim.standing[t->nation] >= 2 ?
	    MUS_NEWS_TITLED : MUS_NEWS);
	g->gov_rank = g->gov_acres = 0;
	g->gov_villain = -1;
	g->gov_tries = 0;
	g->gov_stage = GOV_GREET;
}

/*
 * The next stage with something to say, in the order of VA 0x13269: a
 * villain hiding here, the mission (its report, or an offer), the
 * promotion and its land, the news of a new villain, his offer.
 */
static void
governor_next(struct game *g)
{
	long price;
	enum governor_offer o;
	char id[96];
	int v;

	switch (g->gov_stage) {
	case GOV_GREET:
		v = sim_villain_at(&g->sim, g->sim.player_town);
		if (v >= 0) {
			g->villain = v;
			g->villain_stage = 0;
			g->villain_back = &screen_governor;
			g->gov_resume = 1;
			g->gov_stage = GOV_MISSION;
			game_goto(g, &screen_villain);
			return;
		}
		/* FALLTHROUGH */
	case GOV_MISSION:
		if (g->sim.mission.type != MISSION_NONE) {
			g->gov_report = sim_mission_report(&g->sim);
			if (g->gov_report != 0) {
				g->gov_stage = GOV_REPORT;
				return;
			}
		} else if (sim_mission_draw(&g->sim) != MISSION_NONE) {
			g->gov_stage = GOV_ASK;
			g->selected = 0;
			return;
		}
		/* FALLTHROUGH */
	case GOV_REPORT:
	case GOV_ASK:
	case GOV_TASK:
		g->gov_rank = sim_governor_promote(&g->sim, &g->gov_acres);
		if (g->gov_rank > 0) {
			load_bank(g, game_town_asset(g, "spr/gov", id,
			    sizeof(id)), 0, &g->gov_spr);
			g->gov_stage = GOV_PROMOTED;
			game_music(g, MUS_PROMOTION);	/* VA 0x12549 */
			return;
		}
		/* FALLTHROUGH */
	case GOV_PROMOTED:
		if (g->gov_stage == GOV_PROMOTED && g->gov_acres > 0) {
			g->gov_stage = GOV_LAND;
			return;
		}
		/* FALLTHROUGH */
	case GOV_LAND:
		if (g->gov_rank > 0 &&
		    (g->gov_villain = sim_villain_appear(&g->sim)) >= 0) {
			governor_inks(g, "pic/evilone");
			g->gov_stage = GOV_VILLAIN_NEWS;
			return;
		}
		/* FALLTHROUGH */
	case GOV_VILLAIN_SEEN:
	case GOV_RANSOM:
		if (g->gov_stage == GOV_VILLAIN_SEEN)
			governor_inks(g, "pic/governor");
		/* Ransoms for the captains held (VA 0x127da). */
		if (sim_ransom_offer(&g->sim, &g->gov_tries) >= 0) {
			g->gov_stage = GOV_RANSOM;
			g->selected = 0;
			return;
		}
		o = sim_governor_offer(&g->sim, &price);
		if (o == OFFER_MARQUE || o == OFFER_PARDON) {
			g->gov_stage = GOV_OFFER;
			g->selected = 0;
			return;
		}
		/* From standing 2, his daughter or niece (VA 0x128c1). */
		if (o == OFFER_DAUGHTER) {
			int met = sim_daughter_meet(&g->sim, &g->daughter);

			if (met != 0) {
				governor_inks(g, g->daughter.niece ?
				    "pic/niece" : "pic/daughter");
				game_music(g, g->daughter.niece ? MUS_NIECE :
				    MUS_DAUGHTER);		/* VA 0x128c1 */
				g->gov_stage = met == 1 ? GOV_DAUGHTER :
				    GOV_INFORM;
				g->selected = 0;
				return;
			}
		}
		/* FALLTHROUGH */
	default:
		bank_free(&g->gov_spr);
		g->gov_stage = GOV_DONE;
		game_goto(g, &screen_town);
		return;
	case GOV_VILLAIN_NEWS:
		g->gov_stage = GOV_VILLAIN_SEEN;
		return;
	case GOV_INFORM:
		g->gov_stage = GOV_INFORM_FLEET;
		return;
	case GOV_INFORM_FLEET:
		g->gov_stage = GOV_INFORM_TRAIN;
		return;
	}
}

static void
governor_daughter_choose(struct game *g, int i)
{
	if (i == 0) {				/* conversation */
		if (sim_daughter_talk(&g->sim, &g->daughter)) {
			g->gov_said = S_DAUGHTER_WON;
			g->gov_stage = GOV_SAID;
		} else {
			g->gov_stage = GOV_SHORT;	/* nothing said: the end */
			governor_next(g);
		}
		return;
	}
	g->gov_said = sim_daughter_propose(&g->daughter);
	g->gov_stage = g->gov_said == 144 ? GOV_CHALLENGE : GOV_SAID;
	g->selected = 0;
}

static void
governor_challenge_choose(struct game *g, int i)
{
	if (i == 0) {
		g->sw_suitor = 1;		/* the duel in the alley */
		g->gov_resume = 1;
		g->gov_stage = GOV_WEDDING;
		game_goto(g, &screen_sword);
	} else {
		g->gov_stage = GOV_SHORT;	/* the visit ends */
		governor_next(g);
	}
}

static void
governor_ransom_choose(struct game *g, int i)
{
	if (i == 0)
		sim_ransom_take(&g->sim);
	governor_next(g);
}

static void
governor_ask_choose(struct game *g, int i)
{
	if (i == 0) {
		sim_mission_accept(&g->sim);
		g->gov_stage = GOV_TASK;
	} else
		governor_next(g);
}

static void
governor_offer_choose(struct game *g, int i)
{
	if (i == 0 && !sim_governor_pay(&g->sim)) {
		g->gov_stage = GOV_SHORT;
		return;
	}
	g->gov_stage = GOV_SHORT;	/* past the offer */
	governor_next(g);
}

static void
governor_event(struct game *g, const struct ui_event *ev)
{
	if (g->gov_stage == GOV_DONE)
		return;
	if (g->gov_stage == GOV_OFFER) {
		menu_event(g, ev, 2, NULL, governor_offer_choose, NULL);
		return;
	}
	if (g->gov_stage == GOV_ASK) {
		menu_event(g, ev, 2, NULL, governor_ask_choose, NULL);
		return;
	}
	if (g->gov_stage == GOV_RANSOM) {
		menu_event(g, ev, 2, NULL, governor_ransom_choose, NULL);
		return;
	}
	if (g->gov_stage == GOV_DAUGHTER) {
		menu_event(g, ev, 2, NULL, governor_daughter_choose, NULL);
		return;
	}
	if (g->gov_stage == GOV_CHALLENGE) {
		menu_event(g, ev, 2, NULL, governor_challenge_choose, NULL);
		return;
	}
	if ((g->gov_stage == GOV_SAID || g->gov_stage == GOV_WEDDING ||
	    g->gov_stage == GOV_INFORM_TRAIN) && (ev->type == EV_CLICK ||
	    (ev->type == EV_KEY && (ev->key == UI_KEY_ENTER ||
	    ev->key == UI_KEY_ESCAPE)))) {
		g->gov_stage = GOV_SHORT;	/* the visit ends */
		governor_next(g);
		return;
	}
	if (ev->type == EV_CLICK || (ev->type == EV_KEY &&
	    (ev->key == UI_KEY_ENTER || ev->key == UI_KEY_ESCAPE)))
		governor_next(g);
}

/* "We are allied with X, Y and Z": CSTN 25.3-25.5 over the names. */
static void
nation_list(struct game *g, const int *n, int count, char *out,
    size_t len)
{
	const char *arg[3];
	int i;

	for (i = 0; i < count && i < 3; i++)
		arg[i] = game_word(g, W_COUNTRY, (size_t)n[i]);
	fill_args(out, len, game_word(g, W_GOV, (size_t)(count + 2)), arg,
	    count);
}

/* The greeting with the political situation (VA 0x11d64). */
static void
governor_greeting(struct game *g, char *out, size_t len)
{
	const struct sim_town *t = here(g);
	int nat = t != NULL ? t->nation & 3 : 0, allies[4], enemies[4];
	int na = 0, ne = 0, k;
	char part[256], list[160];

	for (k = 0; k < 4; k++) {
		if (k == nat)
			continue;
		if (g->sim.relations[nat][k] == 1)
			allies[na++] = k;
		else if (g->sim.relations[nat][k] == -1)
			enemies[ne++] = k;
	}
	if (na + ne == 0) {
		snprintf(out, len, "%s", game_str(g, S_GOV_CHAT));
		return;
	}
	fill(out, len, game_word(g, W_GOV, 0),
	    game_word(g, W_RANK, g->sim.rank[nat]), g->sim.player.name);
	if (na > 0) {
		nation_list(g, allies, na, list, sizeof(list));
		snprintf(part, sizeof(part), "%s%s", game_word(g, W_GOV, 1),
		    list);
		strncat(out, part, len - strlen(out) - 1);
	}
	if (ne > 0) {
		nation_list(g, enemies, ne, list, sizeof(list));
		fill(part, sizeof(part), game_word(g, W_GOV, 2),
		    na > 0 ? game_word(g, W_GOV, 7) : "", NULL);
		strncat(out, part, len - strlen(out) - 1);
		strncat(out, list, len - strlen(out) - 1);
	}
	strncat(out, game_word(g, W_GOV, ne == 0 || g->sim.standing[nat] < 2 ?
	    8 : 6), len - strlen(out) - 1);
}

/* Her pages: daughter.pcx at (350, 100) up to x 620, niece.pcx at (10,
 * 150) up to x 300 (VA 0x128c1). */
static void
daughter_draw(struct game *g)
{
	const struct daughter *d = &g->daughter;
	const char *arg[2];
	char text[700], name[80];
	int x = d->niece ? 10 : 350, y = d->niece ? 150 : 100;
	int w = d->niece ? 290 : 270, menu = 0, at;

	switch (g->gov_stage) {
	case GOV_DAUGHTER:
		captain_name(g, d->suitor, name, sizeof(name));
		fill(text, sizeof(text), game_str(g, S_DAUGHTER),
		    game_word(g, W_DAUGHTER, (size_t)d->niece), name);
		menu = 1;
		break;
	case GOV_SAID:
		if (g->gov_said == S_DAUGHTER_WON)
			snprintf(text, sizeof(text), "%s", game_str(g,
			    S_DAUGHTER_WON));
		else
			fill(text, sizeof(text), game_str(g, g->gov_said),
			    game_word(g, 35, (size_t)(g->sim.player.difficulty &
			    3)), NULL);
		break;
	case GOV_CHALLENGE:
		at = g->sim.towns[d->town].nation & 3;
		fill(text, sizeof(text), game_str(g, 144),
		    game_word(g, W_RANK, g->sim.rank[at]), g->sim.player.name);
		menu = 1;
		break;
	case GOV_INFORM:
		fill(text, sizeof(text), game_word(g, W_INFORM, 0),
		    game_word(g, W_INFORM, g->sim.towns[d->town].governor &
		    GOV_WIFE ? 2 : 1), NULL);
		break;
	default:			/* the convoys, as the hostage tells */
		at = g->gov_stage == GOV_INFORM_FLEET ? g->sim.fleet_town :
		    g->sim.train_town;
		arg[0] = game_str(g, g->gov_stage == GOV_INFORM_FLEET ? 46 :
		    45);
		arg[1] = at >= 0 && (size_t)at < g->sim.ntowns ?
		    g->sim.towns[at].name : game_word(g, 27,
		    g->gov_stage == GOV_INFORM_FLEET ? 1 : 2);
		fill_args(text, sizeof(text), game_word(g, 27, 0), arg, 2);
		break;
	}
	draw(g, FONT_BODY, x, y, w, text, menu);
	if (!menu)
		continue_prompt(g);
}

static void
governor_draw(struct game *g)
{
	const struct sim_town *t = here(g);
	int nat = t != NULL ? t->nation & 3 : 0;
	char text[1024], num[32], num2[80];
	long price;
	enum governor_offer o;

	switch (g->gov_stage) {
	case GOV_GREET:
		governor_greeting(g, text, sizeof(text));
		break;
	case GOV_PROMOTED:
	case GOV_LAND:
		if (g->gov_spr.count >= 2) {
			fb_frame(&g->fb, &g->gov_spr.frames[0], 140, 35);
			fb_frame(&g->fb, &g->gov_spr.frames[1], 113, 262);
		}
		if (g->gov_stage == GOV_PROMOTED)
			fill(text, sizeof(text), game_str(g, S_PROMOTION),
			    game_word(g, W_NATION, (size_t)nat),
			    game_word(g, W_RANK, (size_t)g->gov_rank));
		else {
			snprintf(num, sizeof(num), "%d", g->gov_acres);
			fill(text, sizeof(text), game_str(g, S_LAND), num,
			    NULL);
		}
		break;
	case GOV_REPORT:
		if (g->gov_report == 2) {
			captain_name(g, g->sim.mission.pirate, num2,
			    sizeof(num2));
			fill(text, sizeof(text), game_str(g, S_PIRATE_BROUGHT),
			    num2, NULL);
		} else
			snprintf(text, sizeof(text), "%s", game_str(g,
			    g->gov_report > 0 ? S_MISSION_THANKS :
			    S_MISSION_FAILED));
		break;
	case GOV_ASK:
		draw(g, FONT_BODY, GOV_TEXT_X, GOV_TEXT_Y, GOV_TEXT_W,
		    game_str(g, S_MISSION_ASK), 1);
		return;
	case GOV_DAUGHTER:
	case GOV_SAID:
	case GOV_CHALLENGE:
	case GOV_INFORM:
	case GOV_INFORM_FLEET:
	case GOV_INFORM_TRAIN:
		daughter_draw(g);
		return;
	case GOV_WEDDING:
		draw(g, FONT_BODY, 360, 150, 260, game_str(g, S_WEDDING), 0);
		continue_prompt(g);
		return;
	case GOV_RANSOM:
		captain_name(g, g->sim.prisoners[g->sim.ransom_slot & 7] & 0xFF,
		    num2, sizeof(num2));
		snprintf(num, sizeof(num), "%ld", g->sim.ransom_gold);
		fill(text, sizeof(text), game_str(g, S_RANSOM), num2, num);
		draw(g, FONT_BODY, GOV_TEXT_X, GOV_TEXT_Y, GOV_TEXT_W, text, 1);
		return;
	case GOV_TASK:
		if (g->sim.mission.type == MISSION_PIRATE) {
			captain_name(g, g->sim.mission.pirate, num2,
			    sizeof(num2));
			fill(text, sizeof(text), game_str(g, S_MISSION_TASK +
			    MISSION_PIRATE), num2,
			    g->sim.towns[g->sim.mission.dest].name);
		} else
			fill(text, sizeof(text), game_str(g, S_MISSION_TASK +
			    g->sim.mission.type),
			    g->sim.towns[g->sim.mission.dest].name, NULL);
		break;
	case GOV_VILLAIN_NEWS:
	case GOV_VILLAIN_SEEN:
		villain_text(g, g->gov_villain, g->gov_stage ==
		    GOV_VILLAIN_NEWS ? S_VILLAIN_NEWS : S_VILLAIN_SEEN, text,
		    sizeof(text));
		draw(g, FONT_BODY, 340, 150, 280, text, 0);
		continue_prompt(g);
		return;
	case GOV_OFFER:
		o = sim_governor_offer(&g->sim, &price);
		snprintf(num, sizeof(num), "%ld", price);
		fill(text, sizeof(text), game_str(g, o == OFFER_PARDON ?
		    S_PARDON : S_MARQUE), game_word(g, o == OFFER_PARDON ?
		    W_NATION : W_NATION_A, (size_t)nat), num);
		draw(g, FONT_BODY, GOV_TEXT_X, GOV_TEXT_Y, GOV_TEXT_W, text, 1);
		return;
	case GOV_SHORT:
		snprintf(text, sizeof(text), "%s", game_str(g, S_NO_FUNDS));
		break;
	default:
		return;
	}
	draw(g, FONT_BODY, GOV_TEXT_X, GOV_TEXT_Y, GOV_TEXT_W, text, 0);
	continue_prompt(g);
}

const struct screen screen_governor = {
	"governor", governor_enter, governor_event, NULL, governor_draw,
};

/*
 * Bank (VA 0x180c8): "Are you sure you want to divide the plunder?",
 * then each man's share and the crew's mood (CSTR 155), the captain's
 * portions (CSTR 156), the treasure the captain has put aside (the cave,
 * VA 0xbdfe), and "Plan another expedition / Retire / Advance to the
 * next level" (CSTR 157 and 97).  The date is on the small sign.
 */
#define S_SURE			86
#define S_DIVIDE		95
#define S_END_GAME		96
#define S_ADVANCE		97
#define S_SHARES		155
#define S_PORTIONS		156
#define S_PLAN			157
#define S_REFIT			158
#define S_EXPEDITION_OVER	159	/* also the prison's end pages */
#define S_TOO_OLD		160
#define BANK_TEXT_X		40
#define BANK_TEXT_Y		280
#define BANK_TEXT_W		580
enum { BANK_ASK, BANK_SHARES, BANK_PORTIONS, BANK_PLAN, BANK_SURE_END,
    BANK_REFIT, BANK_OVER };

static void
bank_enter(struct game *g)
{
	/* Back from the hideout, the career may end before any plan (VA
	 * 0x1f1d7): a famous expedition is over (CSTR 159), or at health
	 * 3 the officers leave for a younger captain (CSTR 160), on a
	 * cleared screen. */
	if (g->bank_stage == BANK_PLAN && (g->sim.player.expedition >= 0 ||
	    sim_health(&g->sim) == 3))
		g->bank_stage = BANK_OVER;
	if (g->bank_stage == BANK_ASK)
		game_music(g, MUS_BANK);
	game_background(g, g->bank_stage == BANK_OVER ? "pic/textback" :
	    "pic/bank");
	game_portrait(g, NULL);
	g->ink[0] = 240;
	g->ink[1] = 220;
	g->ink[2] = 170;
	g->highlight[0] = 255;
	g->highlight[1] = 200;
	g->highlight[2] = 60;
	g->selected = 0;
}

static void
bank_choose(struct game *g, int i)
{
	switch (g->bank_stage) {
	case BANK_ASK:
		if (i != 0) {
			game_goto(g, &screen_town);
			return;
		}
		sim_divide_plunder(&g->sim, &g->plunder);
		g->bank_stage = BANK_SHARES;
		break;
	case BANK_PLAN:
		if (i == 1) {
			g->bank_stage = BANK_SURE_END;
			break;
		}
		sim_new_expedition(&g->sim, i == 2);
		g->bank_stage = BANK_REFIT;
		break;
	case BANK_SURE_END:
		if (i == 0) {
			g->end_forced = 0;
			game_goto(g, &screen_ending);
			return;
		}
		sim_new_expedition(&g->sim, 0);
		g->bank_stage = BANK_REFIT;
		break;
	default:
		break;
	}
	g->selected = 0;
}

static void
bank_event(struct game *g, const struct ui_event *ev)
{
	int go = ev->type == EV_CLICK || (ev->type == EV_KEY &&
	    (ev->key == UI_KEY_ENTER || ev->key == UI_KEY_ESCAPE));

	switch (g->bank_stage) {
	case BANK_ASK:
	case BANK_SURE_END:
		menu_event(g, ev, 2, NULL, bank_choose, NULL);
		break;
	case BANK_PLAN:
		menu_event(g, ev, g->sim.player.difficulty < 3 ? 3 : 2, NULL,
		    bank_choose, NULL);
		break;
	case BANK_SHARES:
		if (go)
			g->bank_stage = BANK_PORTIONS;
		break;
	case BANK_PORTIONS:
		if (go) {
			g->bank_stage = BANK_PLAN;
			g->cave_back = &screen_bank;
			game_goto(g, &screen_cave);
		}
		break;
	case BANK_REFIT:
		if (go)
			game_goto(g, &screen_town);
		break;
	case BANK_OVER:
		if (go) {
			g->end_forced = 1;	/* no lure back to the sea */
			game_goto(g, &screen_ending);
		}
		break;
	}
}

static void
bank_draw(struct game *g)
{
	char text[1200], a[32], b[32], date[64], line[160];
	const char *arg[3];
	int menu = 0;

	if (g->bank_stage == BANK_OVER) {
		if (g->sim.player.expedition >= 0)
			draw(g, FONT_BODY, 100, 40, 440,
			    game_str(g, S_EXPEDITION_OVER), 0);
		else
			draw(g, FONT_BODY, 200, 200, 420,
			    game_str(g, S_TOO_OLD), 0);
		continue_prompt(g);
		return;
	}
	date_text(g, date, sizeof(date));
	{
		int w = text_width(&g->font[FONT_BODY], date);

		draw(g, FONT_BODY, 306 - w / 2, 244, 200, date, 0);
	}
	switch (g->bank_stage) {
	case BANK_ASK:
	case BANK_SURE_END:
		fill(text, sizeof(text), game_str(g, S_SURE), game_str(g,
		    g->bank_stage == BANK_ASK ? S_DIVIDE : S_END_GAME), NULL);
		menu = 1;
		break;
	case BANK_SHARES:
		snprintf(a, sizeof(a), "%d", g->plunder.share);
		fill(text, sizeof(text), game_str(g, S_SHARES), a,
		    game_word(g, W_MOOD, (size_t)g->plunder.mood));
		break;
	case BANK_PORTIONS:
		snprintf(a, sizeof(a), "%d", g->plunder.portions);
		snprintf(b, sizeof(b), "%ld", g->plunder.yours);
		arg[0] = game_word(g, W_DIFFICULTY,
		    (size_t)g->sim.player.difficulty);
		arg[1] = a;
		arg[2] = b;
		fill_args(text, sizeof(text), game_str(g, S_PORTIONS), arg, 3);
		break;
	case BANK_PLAN:
		snprintf(text, sizeof(text), "%s", game_str(g, S_PLAN));
		if (g->sim.player.difficulty < 3) {
			fill(line, sizeof(line), game_str(g, S_ADVANCE),
			    game_word(g, W_DIFFICULTY,
			    (size_t)g->sim.player.difficulty + 1), NULL);
			if (text[0] != '\0' && text[strlen(text) - 1] != '\r')
				strncat(text, "\r", sizeof(text) -
				    strlen(text) - 1);
			strncat(text, line, sizeof(text) - strlen(text) - 1);
		}
		menu = 1;
		break;
	case BANK_REFIT:
		snprintf(text, sizeof(text), "%s", game_str(g, S_REFIT));
		break;
	}
	draw(g, FONT_BODY, BANK_TEXT_X, BANK_TEXT_Y, BANK_TEXT_W, text, menu);
	if (!menu)
		continue_prompt(g);
}

const struct screen screen_bank = {
	"bank", bank_enter, bank_event, NULL, bank_draw,
};

/*
 * The treasure cave (VA 0xbdfe): eight piles (tcave0-7), each shown
 * once the captain's fortune passes its mark (table at VA 0x32928: x,
 * y, mark; 500, 1000, then by thousands).
 */
static const char *cave_pile[8] = {
	"spr/tcave0", "spr/tcave1", "spr/tcave2", "spr/tcave3",
	"spr/tcave4", "spr/tcave5", "spr/tcave6", "spr/tcave7",
};

static int
cave_pile_at(const struct game *g, int i, int r[3])
{
	int k;

	if (i < 0 || i >= UI_CAVE_PILES)
		return (-1);
	for (k = 0; k < 3; k++)
		r[k] = g->ui.cave[i][k];
	return (0);
}

static void
cave_enter(struct game *g)
{
	int i;

	game_background(g, "pic/trescave");
	game_portrait(g, NULL);
	for (i = 0; i < 8; i++)
		load_bank(g, cave_pile[i], 0, &g->cave[i]);
}

static void
cave_event(struct game *g, const struct ui_event *ev)
{
	if (ev->type == EV_CLICK || (ev->type == EV_KEY &&
	    (ev->key == UI_KEY_ENTER || ev->key == UI_KEY_ESCAPE)))
		game_goto(g, g->cave_back != NULL ? g->cave_back :
		    &screen_bank);
}

static void
cave_draw(struct game *g)
{
	int i, r[3];

	for (i = 0; i < 8; i++)
		if (cave_pile_at(g, i, r) == 0 && r[2] < g->sim.fortune &&
		    g->cave[i].count > 0)
			fb_frame(&g->fb, &g->cave[i].frames[0], r[0], r[1]);
	continue_prompt(g);
}

const struct screen screen_cave = {
	"cave", cave_enter, cave_event, NULL, cave_draw,
};

/*
 * Selling cannon (VA 0x22c76): the trading board, lower on the
 * shipwright's picture, with a single row: the shipwright's stock and
 * the player's guns, 50 gold each, paid from the shipwright's gold.
 * Left sells, Right buys back; Enter, Escape or the sign leaves.
 */
#define S_CANNON_PRICE		313

static void
cannon_enter(struct game *g)
{
	int i;

	game_background(g, "pic/swright");
	game_portrait(g, NULL);
	g->ink[0] = 240;	/* light, on the dark blue */
	g->ink[1] = 220;
	g->ink[2] = 170;
	for (i = 0; i < 3; i++)
		load_bank(g, game_lang_asset(g, board_files[0][i]), 0,
		    &g->shop_spr[i]);
	g->shop_buying = 0;
	/* Each opening starts his side at none (VA 0x22e12): guns sold
	 * before can no longer be bought back. */
	g->sim.wright_cannon = 0;
	sim_shipwright_gold(&g->sim);
}

/* A gun traded, with the board's click (VA 0x22c76). */
static void
cannon_trade(struct game *g, int buy)
{
	g->shop_buying = buy;
	if (sim_trade_cannon(&g->sim, buy ? 1 : -1) != 0)
		game_fm(g, FM_TRADE);
}

static void
cannon_event(struct game *g, const struct ui_event *ev)
{
	if (ev->type == EV_CLICK) {
		if (ev->y >= 420)
			game_goto(g, &screen_shipwright);
		else if (ev->y >= 340) {
			g->shop_buying = ev->x < FB_W / 2;
			cannon_trade(g, g->shop_buying);
		}
		return;
	}
	if (ev->type != EV_KEY)
		return;
	if (ev->key == UI_KEY_LEFT)
		cannon_trade(g, 0);
	else if (ev->key == UI_KEY_RIGHT)
		cannon_trade(g, 1);
	else if (ev->key == UI_KEY_ENTER || ev->key == UI_KEY_ESCAPE)
		game_goto(g, &screen_shipwright);
}

static void
cannon_draw(struct game *g)
{
	int room;

	board_frame(g, 1, 0, 204, 265);
	board_frame(g, 0, 84, 71, 326);		/* the gold boxes */
	board_frame(g, 0, 84, 491, 326);
	board_frame(g, 0, 87, 487, 248);	/* the player's colours */
	board_frame(g, 0, 88, 67, 250);		/* the shipwright's */
	board_frame(g, 2, 1, 63, 360);		/* the cannon, each side */
	board_frame(g, 2, 2, 483, 360);
	board_frame(g, 2, 0, 252, 430);		/* the sign */
	board_frame(g, 0, BOARD_SIGN + g->shop_buying, 238, 350);
	board_number(g, g->sim.wright_gold > 999999 ? 999999 :
	    g->sim.wright_gold, 87, 336, GLYPH_SMALL + 1, 8, 6, 1);
	board_number(g, g->sim.fleet.gold > 999999 ? 999999 :
	    g->sim.fleet.gold, 507, 336, GLYPH_SMALL + 1, 8, 6, 1);
	board_number(g, g->sim.wright_cannon, 78, 372, GLYPH_BIG + 10, 15, 4,
	    1);
	board_number(g, g->sim.fleet.guns, 520, 372, GLYPH_BIG + 10, 15, 4, 0);
	/* The space in the hold and its gauge, as the merchant's, lower. */
	room = sim_hold_space(&g->sim);
	board_number(g, room, 336, 386, GLYPH_SMALL + 23, 8, 4, 1);
	board_slider(g, g->sim.wright_cannon, g->sim.fleet.guns,
	    CANNON_TRACK_Y);
	draw(g, FONT_BODY, 150, 20, 470, game_str(g, S_CANNON_PRICE), 0);
}

const struct screen screen_cannon = {
	"cannon", cannon_enter, cannon_event, NULL, cannon_draw,
};

/*
 * Arrival (VA 0x18fa9): on arr_sea.pcx (arr_land.pcx inland), "You have
 * arrived at the lovely <seaside town> of <name>. <forts> The <nation>
 * flag flies over the town." (CSTR 114, at (330, 40) up to x 620), and
 * the choices (CSTR 115, at (330, 260)): sail into the harbour, attack,
 * sneak in, or leave.  The fort's answer is written in place; a guard's
 * challenge (CSTR 163) is on landpir.pcx, before the duel.
 */
#define S_ARRIVED		114
#define S_ARRIVE_MENU		115
#define S_FORT_FIRES		116
#define S_SHIP_SUNK		117
#define S_RECOGNIZED		163
#define S_IMPRISONED		175
#define S_TRADED_BACK		190
#define S_DISGRACE		174
#define W_ARRIVAL		29
#define ARR_ONE_FORT		0	/* +8 inland */
#define ARR_FORTS		1
#define ARR_SAIL_IN		4	/* 5: march into town */
#define ARR_INLAND		6	/* 7: seaside town */
#define ARR_TEXT_X		330
enum { ARRIVE_MENU, ARRIVE_ANSWER, ARRIVE_GUARD };

static const struct sim_town *
arrive_town(const struct game *g)
{
	int i = g->sim.fleet.leaving;

	return (i >= 0 && (size_t)i < g->sim.ntowns ? &g->sim.towns[i] :
	    NULL);
}

static void
arrive_enter(struct game *g)
{
	const struct sim_town *t = arrive_town(g);
	int inland = t != NULL && ((t->flags & TOWN_INLAND) ||
	    g->sim.fleet.ashore);

	game_background(g, g->arrive_stage == ARRIVE_GUARD ? "pic/landpir" :
	    inland ? "pic/arr_land" : "pic/arr_sea");
	game_portrait(g, NULL);
	if (g->bg.pixels != NULL) {
		memcpy(g->ink, bg_colour(g, STORY_INK), 3);
		memcpy(g->highlight, bg_colour(g, STORY_HIGHLIGHT), 3);
	}
	g->selected = 0;
	g->msg[0] = '\0';
	if (g->arrive_stage == ARRIVE_MENU && t != NULL)
		sim_arrive(&g->sim, g->sim.fleet.leaving);
	/* Into a harbour from the sea (VA 0x18fa9, arr_sea.pcx). */
	if (g->arrive_stage == ARRIVE_MENU && t != NULL && !inland)
		game_sfx(g, SFX_HARBOUR);
	if (g->arrive_stage == ARRIVE_MENU && t != NULL)
		game_music(g, MUS_HARBOUR);		/* VA 0x18fa9 */
}

static void
arrive_choose(struct game *g, int i)
{
	int town = g->sim.fleet.leaving;

	switch (i) {
	case 0:
		g->arrive_result = sim_sail_in(&g->sim, town);
		if (g->arrive_result == ARRIVE_ENTER)
			game_goto(g, &screen_town);
		else
			g->arrive_stage = ARRIVE_ANSWER;
		break;
	case 1:
		/* The convoys' guards join the garrison (VA 0x18b3f). From
		 * the sea, a fort first fights the fleet; a town without
		 * forts goes straight to the assault. */
		sim_attack_begin(&g->sim, town);
		if (g->sim.towns[town].forts > 0 && !g->sim.fleet.ashore) {
			sim_fort_battle_start(&g->sim, town);
			game_goto(g, &screen_battle);
		} else if (g->sim.fleet.ashore &&
		    sim_land_battle_needed(&g->sim, town)) {
			/* On foot against a strong garrison: a land battle
			 * first (VA 0x18c90). */
			g->assault_town = town;
			g->assault_aboard = 0;
			sim_land_start(&g->sim, town);
			game_sfx(g, SFX_LAND_BATTLE);	/* VA 0x169b7 */
			game_goto(g, &screen_land);
		} else {
			g->assault_town = town;
			g->assault_aboard = 0;
			game_goto(g, &screen_assault);
		}
		break;
	case 2:
		if (sim_sneak_in(&g->sim, town) == ARRIVE_ENTER)
			game_goto(g, &screen_town);
		else {
			g->arrive_stage = ARRIVE_GUARD;
			game_switch(g, &screen_arrive);
		}
		break;
	default:
		game_goto(g, &screen_sail);
		break;
	}
}

static void
arrive_event(struct game *g, const struct ui_event *ev)
{
	int go = ev->type == EV_CLICK || (ev->type == EV_KEY &&
	    (ev->key == UI_KEY_ENTER || ev->key == UI_KEY_ESCAPE));

	switch (g->arrive_stage) {
	case ARRIVE_MENU:
		menu_event(g, ev, 4, NULL, arrive_choose, NULL);
		break;
	case ARRIVE_ANSWER:
		if (go)
			game_goto(g, g->sim.jail.fresh ? &screen_prison :
			    &screen_sail);
		break;
	default:
		if (go) {
			g->sw_guard = 1;
			game_goto(g, &screen_sword);
		}
		break;
	}
}

static void
arrive_draw(struct game *g)
{
	const struct sim_town *t = arrive_town(g);
	char text[1024], forts[160], label[64];
	const char *arg[5];
	int inland, n;

	if (t == NULL)
		return;
	inland = (t->flags & TOWN_INLAND) != 0;
	if (g->arrive_stage == ARRIVE_GUARD) {
		draw(g, FONT_BODY, 40, 150, 200, game_str(g, S_RECOGNIZED), 0);
		continue_prompt(g);
		return;
	}
	if (g->arrive_stage == ARRIVE_ANSWER) {
		snprintf(text, sizeof(text), "%s", game_str(g, S_FORT_FIRES));
		if (g->arrive_result == ARRIVE_SUNK)
			snprintf(text + strlen(text), sizeof(text) -
			    strlen(text), " %s", game_str(g, S_SHIP_SUNK));

		draw(g, FONT_BODY, ARR_TEXT_X, 40, 290, text, 0);
		continue_prompt(g);
		return;
	}
	n = t->forts;
	forts[0] = '\0';
	if (n == 1)
		snprintf(forts, sizeof(forts), "%s", game_word(g, W_ARRIVAL,
		    ARR_ONE_FORT + (inland ? 8 : 0)));
	else if (n > 1)
		fill(forts, sizeof(forts), game_word(g, W_ARRIVAL, ARR_FORTS +
		    (inland ? 8 : 0)), game_word(g, W_NUMBER, (size_t)n),
		    NULL);
	arg[0] = game_word(g, W_ARRIVAL, ARR_INLAND + (inland ? 0 : 1));
	arg[1] = t->name;
	arg[2] = forts;
	arg[3] = game_word(g, W_NATION, t->nation & 3);
	/* "The treasure fleet is in the harbor!" (CSTN 29.2), or "The
	 * silver train is in town!" (29.3), which wins (VA 0x18fa9). */
	n = sim_convoys_at(&g->sim, (int)(t - g->sim.towns));
	arg[4] = n & CONVOY_TRAIN ? game_word(g, W_ARRIVAL, 3) :
	    n & CONVOY_FLEET ? game_word(g, W_ARRIVAL, 2) : "";
	fill_args(text, sizeof(text), game_str(g, S_ARRIVED), arg, 5);
	draw(g, FONT_BODY, ARR_TEXT_X, 40, 290, text, 0);
	snprintf(label, sizeof(label), "%s", game_word(g, W_ARRIVAL,
	    ARR_SAIL_IN + (g->sim.fleet.ashore ? 1 : 0)));
	fill(text, sizeof(text), game_str(g, S_ARRIVE_MENU), label, NULL);
	draw(g, FONT_BODY, ARR_TEXT_X, 260, 290, text, 1);
	if (g->msg[0] != '\0')
		draw(g, FONT_SMALL, ARR_TEXT_X, 440, 290, g->msg, 0);
}

const struct screen screen_arrive = {
	"arrive", arrive_enter, arrive_event, NULL, arrive_draw,
};

/*
 * Disgrace (VA 0x165b8): the captain's portrait in its frame, as for the
 * story (VA 0x159df), and CSTR 174 at (100, 350) up to x 560.
 */
static void
disgrace_enter(struct game *g)
{
	game_background(g, "pic/frame");
	game_portrait(g, NULL);
	load_bank(g, captain_portrait[g->sim.player.difficulty & 3], 0,
	    &g->id_spr[2]);
	if (g->bg.pixels != NULL)
		memcpy(g->ink, bg_colour(g, STORY_INK), 3);
}

static void
disgrace_event(struct game *g, const struct ui_event *ev)
{
	const struct screen *back = g->disgrace_back != NULL ?
	    g->disgrace_back : &screen_sail;

	if (ev->type == EV_CLICK || (ev->type == EV_KEY &&
	    (ev->key == UI_KEY_ENTER || ev->key == UI_KEY_ESCAPE))) {
		g->disgrace_back = NULL;
		game_goto(g, back);
	}
}

static void
disgrace_draw(struct game *g)
{
	id_frame(g, 2, 0, 236, 58);
	draw(g, FONT_BODY, 100, 350, 460, game_str(g, S_DISGRACE), 0);
	continue_prompt(g);
}

const struct screen screen_disgrace = {
	"disgrace", disgrace_enter, disgrace_event, NULL, disgrace_draw,
};

/*
 * The assault (VA 0x18b3f): on landpir.pcx, "Your men scale the walls of
 * the fort..." (CSTR 192), or in a town without forts "As you and your
 * men enter the town..." (CSTR 211), at (20, 150) up to x 250.  Then
 * the sword fight with the commander.
 */
#define S_SCALE_WALLS		192
#define S_ENTER_TOWN		211
#define S_TOWN_AT_MERCY		193
#define S_INSTALL		194
#define S_GOLD_HIDDEN		195
#define W_HIDDEN		15

static void
assault_enter(struct game *g)
{
	game_background(g, "pic/landpir");
	game_portrait(g, NULL);
	if (g->bg.pixels != NULL)
		memcpy(g->ink, bg_colour(g, STORY_INK), 3);
}

static void
assault_event(struct game *g, const struct ui_event *ev)
{
	if (ev->type == EV_CLICK || (ev->type == EV_KEY &&
	    (ev->key == UI_KEY_ENTER || ev->key == UI_KEY_ESCAPE))) {
		g->sw_assault = 1;
		game_goto(g, &screen_sword);
	}
}

static void
assault_draw(struct game *g)
{
	draw(g, FONT_BODY, 20, 150, 230, game_str(g,
	    g->sim.towns[g->assault_town].forts > 0 ? S_SCALE_WALLS :
	    S_ENTER_TOWN), 0);
	continue_prompt(g);
}

const struct screen screen_assault = {
	"assault", assault_enter, assault_event, NULL, assault_draw,
};

/*
 * The sack (VA 0x22764): on the town's arrival picture, "The once-proud
 * town of %s is at your mercy..." (CSTR 193, at (340, 150) up to x 620);
 * when warned, "%s of their gold has been hidden away" (CSTR 195); with
 * a crew big enough, on governor.pcx, the new governor's nation (CSTR
 * 194); then the loot on the trading board.  The plunder's cannon join
 * the fleet at once (an engine choice: the original offers them on the
 * board).
 */
enum { SACK_MERCY, SACK_HIDDEN, SACK_INSTALL, SACK_SON, SACK_VILLAIN,
    SACK_FLEET, SACK_TRAIN };
#define S_FLEET_TAKEN		186
#define S_TRAIN_TAKEN		187
#define S_SON_FREED		208

static void	sack_after(struct game *);

static void
sack_picture(struct game *g)
{
	const struct sim_town *t = &g->sim.towns[g->assault_town];

	game_background(g, g->sack_stage == SACK_INSTALL ? "pic/governor" :
	    g->sack_stage == SACK_FLEET ? "pic/fleet" :
	    g->sack_stage == SACK_TRAIN ? "pic/silvertr" :
	    t->flags & TOWN_INLAND ? "pic/arr_land" : "pic/arr_sea");
	game_portrait(g, NULL);
	if (g->bg.pixels != NULL) {
		memcpy(g->ink, bg_colour(g, STORY_INK), 3);
		memcpy(g->highlight, bg_colour(g, STORY_HIGHLIGHT), 3);
	}
	g->selected = 0;
}

static void
sack_enter(struct game *g)
{
	sack_picture(g);
	if (g->sack_stage == SACK_VILLAIN)	/* back from the villain */
		sack_after(g);
}

static void
sack_loot(struct game *g)
{
	g->looting = 1;
	game_goto(g, &screen_merchant);
}

static void
sack_install(struct game *g, int i)
{
	sim_install_governor(&g->sim, g->assault_town, i);
	sack_loot(g);
}

static void
sack_event(struct game *g, const struct ui_event *ev)
{
	int go = ev->type == EV_CLICK || (ev->type == EV_KEY &&
	    (ev->key == UI_KEY_ENTER || ev->key == UI_KEY_ESCAPE));

	if (g->sack_stage == SACK_INSTALL) {
		menu_event(g, ev, 4, NULL, sack_install, NULL);
		return;
	}
	if (!go)
		return;
	/* The order of VA 0x22764: the son freed, the hidden gold, a
	 * villain hiding there, then the governor or the loot. */
	if (g->sack_stage == SACK_MERCY && g->sack.son_freed)
		g->sack_stage = SACK_SON;
	else if ((g->sack_stage == SACK_MERCY || g->sack_stage == SACK_SON) &&
	    (g->sack.convoys & CONVOY_FLEET)) {
		g->sack_stage = SACK_FLEET;	/* fleet.pcx, CSTR 186 */
		sack_picture(g);
	} else if ((g->sack_stage == SACK_MERCY || g->sack_stage == SACK_SON ||
	    g->sack_stage == SACK_FLEET) && (g->sack.convoys & CONVOY_TRAIN)) {
		g->sack_stage = SACK_TRAIN;	/* silvertr.pcx, CSTR 187 */
		sack_picture(g);
	} else if (g->sack_stage != SACK_HIDDEN && g->sack.hidden >= 0) {
		g->sack_stage = SACK_HIDDEN;
		sack_picture(g);
	} else
		sack_after(g);
}

static void
sack_after(struct game *g)
{
	int v = sim_villain_at(&g->sim, g->assault_town);

	if (g->sack_stage != SACK_VILLAIN && v >= 0) {
		g->sack_stage = SACK_VILLAIN;
		g->villain = v;
		g->villain_stage = 0;
		g->villain_back = &screen_sack;
		game_goto(g, &screen_villain);
	} else if (g->sack.conquest) {
		g->sack_stage = SACK_INSTALL;
		sack_picture(g);
	} else
		sack_loot(g);
}

static void
sack_draw(struct game *g)
{
	char text[600];

	switch (g->sack_stage) {
	case SACK_MERCY:
		fill(text, sizeof(text), game_str(g, S_TOWN_AT_MERCY),
		    g->sim.towns[g->assault_town].name, NULL);
		break;
	case SACK_HIDDEN:
		fill(text, sizeof(text), game_str(g, S_GOLD_HIDDEN),
		    game_word(g, W_HIDDEN, (size_t)g->sack.hidden), NULL);
		break;
	case SACK_SON:
		fill(text, sizeof(text), game_str(g, S_SON_FREED),
		    g->sim.towns[g->sim.mission.giver].name, NULL);
		break;
	case SACK_FLEET:
	case SACK_TRAIN:
		draw(g, FONT_BODY, 100, 300, 520, game_str(g, g->sack_stage ==
		    SACK_FLEET ? S_FLEET_TAKEN : S_TRAIN_TAKEN), 0);
		continue_prompt(g);
		return;
	case SACK_VILLAIN:
		return;
	default:
		draw(g, FONT_BODY, 340, 150, 280, game_str(g, S_INSTALL), 1);
		return;
	}
	draw(g, FONT_BODY, 340, 150, 280, text, 0);
	continue_prompt(g);
}

const struct screen screen_sack = {
	"sack", sack_enter, sack_event, NULL, sack_draw,
};

/*
 * Prison (VA 0x16647, 0x1f803), told from the sim's last imprisonment:
 * on prison.pcx at (300, 150) up to x 560 (after a fort's salvo, VA
 * 0x1f2a1, at (320, 150) up to x 620), "...You are imprisoned in %s"
 * (CSTR 175, the town or "the ship"); marooned, on maroon.pcx at
 * (100, 320) up to x 620, "With your last ship gone, you are washed
 * ashore..." (CSTR 161).  Then the months (CSTR 176-179 by health; at
 * (40, 320) when marooned) and "...take you back as %s captain" (CSTR
 * 190, or 162 when marooned).  A career that ends there says so (CSTR
 * 159, 160, at (100, 40) and (200, 200) on a cleared page) and goes to
 * the Hall of Fame.
 */
#define S_MONTHS_PASS		176	/* + health: 176-179 */

static void	retire_choose(struct game *, int);

static void
prison_enter(struct game *g)
{
	game_music(g, MUS_PRISON);
	game_background(g, g->sim.jail.cause == JAIL_MAROONED ?
	    "pic/maroon" : "pic/prison");
	game_portrait(g, NULL);
	if (g->bg.pixels != NULL)
		memcpy(g->ink, bg_colour(g, STORY_INK), 3);
	g->sack_stage = 0;
}

static void
prison_event(struct game *g, const struct ui_event *ev)
{
	if (!(ev->type == EV_CLICK || (ev->type == EV_KEY &&
	    (ev->key == UI_KEY_ENTER || ev->key == UI_KEY_ESCAPE))))
		return;
	g->sack_stage++;
	if (g->sim.jail.end && g->sack_stage == 1) {
		game_background(g, "pic/textback");
		return;
	}
	if (g->sack_stage < 3 && !g->sim.jail.end)
		return;
	g->sim.jail.fresh = 0;
	if (g->sim.jail.end) {
		g->end_forced = 1;		/* the career's end */
		game_goto(g, &screen_ending);
	}
	else
		game_goto(g, g->world != NULL ? &screen_sail : &screen_cabin);
}

static void
prison_draw(struct game *g)
{
	const struct sim_jail *j = &g->sim.jail;
	int maroon = j->cause == JAIL_MAROONED;
	char text[600];

	if (g->sack_stage == 0) {
		if (maroon) {
			draw(g, FONT_BODY, 100, 320, 520, game_str(g, S_ASHORE),
			    0);
			continue_prompt(g);
			return;
		}
		fill(text, sizeof(text), game_str(g, S_IMPRISONED),
		    j->cause == JAIL_SHIP || j->town < 0 ||
		    (size_t)j->town >= g->sim.ntowns ?
		    game_word(g, W_THE_SHIP, 4) : g->sim.towns[j->town].name,
		    NULL);
	} else if (j->end) {
		if (j->end == 1)
			draw(g, FONT_BODY, 100, 40, 440,
			    game_str(g, S_EXPEDITION_OVER), 0);
		else
			draw(g, FONT_BODY, 200, 200, 420,
			    game_str(g, S_TOO_OLD), 0);
		continue_prompt(g);
		return;
	} else if (g->sack_stage == 1)
		snprintf(text, sizeof(text), "%s", game_str(g,
		    S_MONTHS_PASS + (j->health & 3)));
	else
		fill(text, sizeof(text), game_str(g, maroon ? S_PICKED_UP :
		    S_TRADED_BACK), game_word(g, W_CAPTAIN,
		    (size_t)g->sim.player.difficulty), NULL);
	if (maroon)
		draw(g, FONT_BODY, 40, 320, 580, text, 0);
	else if (j->cause == JAIL_FORT)
		draw(g, FONT_BODY, 320, 150, 300, text, 0);	/* VA 0x1f320 */
	else
		draw(g, FONT_BODY, 300, 150, 260, text, 0);
	continue_prompt(g);
}

const struct screen screen_prison = {
	"prison", prison_enter, prison_event, NULL, prison_draw,
};

/*
 * The tavern's people (VA 0x11867).  The map seller (VA 0x112ef, on
 * mapman.pcx): part of a treasure map, CSTR 153 at (340, 150) up to x
 * 620; it costs 50 gold, although his words say 500.  The traveller (VA
 * 0x1116f, infoman.pcx): news of a town for his price, CSTR 152 at (20,
 * 150) up to x 280, then that town's information.  The barmaid (VA
 * 0x114d4, barmaid.pcx, texts at (40, 150) up to x 320).  Without the
 * gold: "You have insufficient funds!" (CSTR 94).
 */
#define S_TRAVELLER		152
#define S_MAP_SELLER		153
#define S_NO_FUNDS		94
#define S_DRINK			301
#define S_LOOKING_FOR		302
#define S_EVENING		303
#define S_HEART			304
#define S_NOTHING_TO_SAY	305
#define S_INFORMANT		306
#define S_GOSSIP_ON		307
#define S_MAP_PIECE		308

static void
talk_inks(struct game *g)
{
	if (g->bg.pixels != NULL) {
		memcpy(g->ink, bg_colour(g, STORY_INK), 3);
		memcpy(g->highlight, bg_colour(g, STORY_HIGHLIGHT), 3);
	}
	g->selected = 0;
}

static int
talk_go(const struct ui_event *ev)
{
	return (ev->type == EV_CLICK || (ev->type == EV_KEY &&
	    (ev->key == UI_KEY_ENTER || ev->key == UI_KEY_ESCAPE)));
}

/* The map seller: 0 the offer, 1 bought, 2 no funds. */
static void
mapman_enter(struct game *g)
{
	game_background(g, "pic/mapman");
	game_portrait(g, NULL);
	talk_inks(g);
	g->talk = 0;
}

static void
mapman_choose(struct game *g, int i)
{
	if (i != 0) {			/* he leaves anyway (VA 0x1191b) */
		g->sim.tavern_spots &= ~TAVERN_MAPS;
		game_goto(g, &screen_tavern);
		return;
	}
	if (sim_buy_map(&g->sim))
		tmap_show(g, MAP_PIRATE, &screen_tavern);  /* VA 0x1f08a */
	else
		g->talk = 2;
}

static void
mapman_event(struct game *g, const struct ui_event *ev)
{
	if (g->talk == 0)
		menu_event(g, ev, 2, NULL, mapman_choose, NULL);
	else if (talk_go(ev))
		game_goto(g, &screen_tavern);
}

static void
mapman_draw(struct game *g)
{
	if (g->talk == 0) {
		draw(g, FONT_BODY, 340, 150, 280, game_str(g, S_MAP_SELLER),
		    1);
		return;
	}
	draw(g, FONT_BODY, 340, 150, 280, game_str(g, S_NO_FUNDS), 0);
	continue_prompt(g);
}

const struct screen screen_mapman = {
	"mapman", mapman_enter, mapman_event, NULL, mapman_draw,
};

/* The traveller: 0 the offer, 2 no funds. */
static void
traveller_enter(struct game *g)
{
	game_background(g, "pic/infoman");
	game_portrait(g, NULL);
	talk_inks(g);
	g->talk = 0;
}

static void
traveller_choose(struct game *g, int i)
{
	int town;

	if (i != 0) {			/* he leaves anyway (VA 0x11935) */
		g->sim.tavern_spots &= ~TAVERN_NEWS;
		game_goto(g, &screen_tavern);
		return;
	}
	if ((town = sim_buy_news(&g->sim)) < 0) {
		g->talk = 2;
		return;
	}
	g->info_town = town;
	g->info_back = &screen_tavern;
	game_goto(g, &screen_cityinfo);
	/* Then what he knows of villains there (VA 0x1116f). */
	g->vnews_n = sim_villain_news(&g->sim, town, g->vnews_list);
	if (g->vnews_n > 0) {
		g->vnews_source = 2;
		g->vnews_town = town;
		g->vnews_back = &screen_tavern;
		g->vnews_said = 0;
		g->info_back = &screen_vnews;
	}
}

static void
traveller_event(struct game *g, const struct ui_event *ev)
{
	if (g->talk == 0)
		menu_event(g, ev, 2, NULL, traveller_choose, NULL);
	else if (talk_go(ev))
		game_goto(g, &screen_tavern);
}

static void
traveller_draw(struct game *g)
{
	char text[512], price[16];

	if (g->talk != 0) {
		draw(g, FONT_BODY, 20, 150, 260, game_str(g, S_NO_FUNDS), 0);
		continue_prompt(g);
		return;
	}
	snprintf(price, sizeof(price), "%d", g->sim.news_price);
	fill(text, sizeof(text), game_str(g, S_TRAVELLER),
	    g->sim.towns[g->sim.news_town].name, price);
	draw(g, FONT_BODY, 20, 150, 260, text, 1);
}

const struct screen screen_traveller = {
	"traveller", traveller_enter, traveller_event, NULL, traveller_draw,
};

/*
 * The barmaid.  Stages: 0 "a drink?" (CSTR 301), 1 "what would ye be
 * lookin' for?" (CSTR 302), 2 a pleasant evening (303), 3 her heart won
 * (304), 4 nothing to report (305), 5 her report (306), 6 the gossip on
 * a town (307), 7 a map piece (308).
 */
enum { MAID_DRINK, MAID_ASK, MAID_EVENING, MAID_HEART, MAID_NOTHING,
    MAID_REPORT, MAID_GOSSIP, MAID_MAP };

static void
barmaid_enter(struct game *g)
{
	char id[96];

	game_music(g, MUS_BARMAID);	/* VA 0x114d4 */
	game_background(g, game_town_asset(g, "pic/barmaid", id,
	    sizeof(id)));
	game_portrait(g, NULL);
	talk_inks(g);
	switch (sim_barmaid_meet(&g->sim)) {
	case 0:
		g->talk = MAID_DRINK;
		break;
	case 1:
		g->talk = MAID_ASK;
		break;
	default:
		g->talk = g->sim.tavern_spots & (TAVERN_NEWS | TAVERN_MAPS) ?
		    MAID_REPORT : MAID_NOTHING;
		break;
	}
}

static void
barmaid_choose(struct game *g, int i)
{
	int r;

	if (g->talk == MAID_DRINK) {
		if (i == 0)
			sim_barmaid(&g->sim, BARMAID_DRINK);
		game_goto(g, &screen_tavern);
		return;
	}
	r = sim_barmaid(&g->sim, i);
	if (i == BARMAID_COMPANY && r >= 0)
		g->talk = r ? MAID_HEART : MAID_EVENING;
	else
		game_goto(g, &screen_tavern);
}

static void
barmaid_event(struct game *g, const struct ui_event *ev)
{
	int town;

	if (g->talk == MAID_DRINK || g->talk == MAID_ASK) {
		menu_event(g, ev, g->talk == MAID_DRINK ? 2 : 3, NULL,
		    barmaid_choose, NULL);
		return;
	}
	if (!talk_go(ev))
		return;
	if (g->talk == MAID_NOTHING) {
		g->sim.tavern_spots &= ~TAVERN_MAID;
		game_goto(g, &screen_tavern);
	} else if (g->talk == MAID_REPORT) {
		switch (sim_barmaid_report(&g->sim, &town)) {
		case 1:
			g->talk_town = town;
			g->talk = MAID_GOSSIP;
			break;
		default:
			g->talk = MAID_MAP;
			break;
		}
	} else if (g->talk == MAID_MAP) {
		tmap_show(g, MAP_PIRATE, &screen_tavern);	/* VA 0x1f08a */
	} else if (g->talk == MAID_GOSSIP) {
		/* The town's information (VA 0x196d0). */
		g->info_town = g->talk_town;
		g->info_back = &screen_tavern;
		game_goto(g, &screen_cityinfo);
	} else
		game_goto(g, &screen_tavern);
}

static void
barmaid_draw(struct game *g)
{
	static const int text_of[] = { S_DRINK, S_LOOKING_FOR, S_EVENING,
	    S_HEART, S_NOTHING_TO_SAY, S_INFORMANT, S_GOSSIP_ON, S_MAP_PIECE };
	char text[512];

	if (g->talk == MAID_GOSSIP)
		fill(text, sizeof(text), game_str(g, S_GOSSIP_ON),
		    g->sim.towns[g->talk_town].name, NULL);
	else
		snprintf(text, sizeof(text), "%s",
		    game_str(g, text_of[g->talk]));
	draw(g, FONT_BODY, 40, 150, 280, text,
	    g->talk == MAID_DRINK || g->talk == MAID_ASK);
	if (g->talk != MAID_DRINK && g->talk != MAID_ASK)
		continue_prompt(g);
}

const struct screen screen_barmaid = {
	"barmaid", barmaid_enter, barmaid_event, NULL, barmaid_draw,
};

/*
 * The fleet panel (VA 0xffae), here to pick the ship to command (VA
 * 0xf246): shipsel.pcx, the ships in two rows of four, each its battle
 * picture at its damage stage, with its type (CSTN 8) and damage (CSTN
 * 14) centred below.  Clicking a ship marks it, in ink entry 3; clicking
 * it again unmarks it; "SELECT" (CSTN 87.1) ends the choice once one is
 * marked.  The keys step through the ships and the button; Enter acts.
 * The prompt, "Which ship will you command?" (CSTR 286), is centred
 * across x 5..420 at y 400.  The layout is the table at VA 0x32e68.
 */
#define PANEL_SHIPS		8
#define PANEL_DONE		8
#define W_DAMAGE		14
#define W_PANEL_BUTTON		87

static int
panel_hit(const struct game *g, int x, int y)
{
	const int16_t *r;
	int i;

	for (i = 0; i <= PANEL_DONE; i++) {
		if (i < PANEL_DONE && i >= g->sim.fleet.nships)
			continue;
		r = g->ui.panel.spot[i];
		if (x >= r[0] && y >= r[1] && x <= r[2] && y <= r[3])
			return (i);
	}
	return (-1);
}

static int
panel_level(int damage)
{
	return (damage / 16 > 3 ? 3 : damage / 16);
}

static void
panel_enter(struct game *g)
{
	int i;

	game_background(g, "pic/shipsel");
	game_portrait(g, NULL);
	for (i = 0; i < PANEL_SHIPS; i++)
		if (i < g->sim.fleet.nships)
			load_bank(g, damage_picture[g->sim.fleet.ships[i] % 9],
			    1, &g->panel_spr[i]);
		else
			bank_free(&g->panel_spr[i]);
	g->panel_cursor = 0;
	g->panel_marked = 0;
}

/* Only looking (the log book's Ship Status): any click or key leaves. */
static int
panel_viewing(struct game *g, const struct ui_event *ev)
{
	if (!g->panel_view)
		return (0);
	if (ev->type == EV_CLICK || (ev->type == EV_KEY &&
	    (ev->key == UI_KEY_ENTER || ev->key == UI_KEY_ESCAPE))) {
		g->panel_view = 0;
		game_goto(g, &screen_logbook);
	}
	return (1);
}

static void
panel_act(struct game *g, int i)
{
	int k;

	if (i == PANEL_DONE) {
		if (g->panel_marked == 0)
			return;
		for (k = 0; k < PANEL_SHIPS; k++)
			if (g->panel_marked & (1u << k))
				break;
		if (g->panel_replace) {
			g->panel_replace = 0;
			sim_prize_place(&g->sim, k, g->sim.prize_type,
			    g->sim.prize_damage);
			game_goto(g, &screen_battle_end);
			return;
		}
		if (g->panel_abandon) {
			/* The ship left behind, her cargo kept (VA
			 * 0x1f65f); the check runs again at sea. */
			g->panel_abandon = 0;
			sim_abandon_ship(&g->sim, k);
			game_goto(g, &screen_sail);
			return;
		}
		sim_battle_command(&g->sim, k);
		g->btl_chosen = 1;
		game_goto(g, &screen_battle);
		return;
	}
	if (i < 0 || i >= g->sim.fleet.nships)
		return;
	if (g->panel_marked & (1u << i))
		g->panel_marked &= ~(1u << i);
	else if (g->panel_marked == 0)		/* one ship at most */
		g->panel_marked = 1u << i;
}

static void
panel_event(struct game *g, const struct ui_event *ev)
{
	int n = g->sim.fleet.nships < PANEL_SHIPS ? g->sim.fleet.nships :
	    PANEL_SHIPS;

	if (panel_viewing(g, ev))
		return;
	if (ev->type == EV_CLICK) {
		panel_act(g, panel_hit(g, ev->x, ev->y));
		return;
	}
	if (ev->type != EV_KEY)
		return;
	if (ev->key == UI_KEY_RIGHT || ev->key == UI_KEY_DOWN)
		g->panel_cursor = g->panel_cursor == PANEL_DONE ? 0 :
		    g->panel_cursor + 1 >= n ? PANEL_DONE : g->panel_cursor + 1;
	else if (ev->key == UI_KEY_LEFT || ev->key == UI_KEY_UP)
		g->panel_cursor = g->panel_cursor == 0 ? PANEL_DONE :
		    g->panel_cursor == PANEL_DONE ? n - 1 : g->panel_cursor - 1;
	else if (ev->key == UI_KEY_ENTER)
		panel_act(g, g->panel_cursor);
}

static void
panel_draw(struct game *g)
{
	const struct sim_fleet *f = &g->sim.fleet;
	const char *word;
	/* A replacement picture's light plaques and dark wood suit neither
	 * entry 31 nor entry 0: a red marks a ship, a parchment prompt. */
	static const uint8_t art_marked[3] = { 0xa8, 0x18, 0x10 };
	static const uint8_t art_prompt[3] = { 0xf2, 0xe4, 0xc0 };
	uint8_t plain[3], marked[3];
	int i, lvl, w;

	memcpy(plain, g->bg.pixels != NULL ? g->bg.palette : g->ink, 3);
	memcpy(marked, g->bg.rgba != NULL ? art_marked :
	    g->bg.pixels != NULL ? &g->bg.palette[31 * 3] : g->ink, 3);
	for (i = 0; i < f->nships && i < PANEL_SHIPS; i++) {
		lvl = panel_level(f->damage[i]);
		if ((size_t)lvl < g->panel_spr[i].count)
			fb_frame(&g->fb, &g->panel_spr[i].frames[lvl],
			    g->ui.panel.ship_x[i], g->ui.panel.ship_y[i]);
		memcpy(g->ink, g->panel_marked & (1u << i) ? marked : plain, 3);
		word = game_word(g, W_SHIP_TYPE, (size_t)(f->ships[i] % 9));
		w = text_width(&g->font[FONT_TITLE], word);
		draw(g, FONT_TITLE, g->ui.panel.type_x[i] - w / 2,
		    g->ui.panel.type_y[i], FB_W, word, 0);
		word = game_word(g, W_DAMAGE, (size_t)lvl);
		w = text_width(&g->font[FONT_BODY], word);
		draw(g, FONT_BODY, g->ui.panel.damage_x[i] - w / 2,
		    g->ui.panel.damage_y[i], FB_W, word, 0);
	}
	memcpy(g->ink, plain, 3);
	if (g->panel_view)
		return;		/* no prompt, a blank button (CSTN 87.0) */
	word = game_str(g, g->panel_replace ? S_NO_OFFICERS :
	    g->panel_abandon ? S_TOO_FEW : S_COMMAND);
	w = text_width(&g->font[FONT_BODY], word);
	if (g->bg.rgba != NULL)
		memcpy(g->ink, art_prompt, 3);
	draw(g, FONT_BODY, 5 + (415 - (w < 415 ? w : 415)) / 2, 400, 415,
	    word, 0);
	memcpy(g->ink, plain, 3);
	word = game_word(g, W_PANEL_BUTTON, 1);
	w = text_width(&g->font[FONT_TITLE], word);
	draw(g, FONT_TITLE, 510 - w / 2, 436, FB_W, word, 0);
}

const struct screen screen_fleet_panel = {
	"fleet panel", panel_enter, panel_event, NULL, panel_draw,
};

/*
 * The land battle (VA 0x5a80): the town's LBTL screen on the left,
 * lbat.pcx's panel on the right.  A pass every 0.6 seconds (VA 0x8ac,
 * 600 ms): 36 frames.  The arrows move the selected unit; the button
 * (Space, Enter, a click) selects the next one, and held, moves every
 * unit without a goal.  The panel (VA 0x5c35, 0x6c9a, 0x7787): the
 * party's and the defenders' names, each side's morale (CSTN 69), men
 * and muskets, and the terrain (CSTN 32) with its picture.
 */
#define LAND_FRAMES		36
#define LAND_JUMP		64	/* farther in a pass: a jump */
#define W_LAND			32	/* clear... "Terrain:", "Morale:" */
#define W_MORALE		69
#define W_POSSESSIVE		92	/* 0: "'s " */
#define S_ATTACK_FAILED		191

static const char *land_banks[5] = {
	"cur/lbtlmisc", "cur/urlbtl", "cur/hsilbtl", "cur/hsclbtl",
	"cur/fortbtl",
};

/* The units where this pass leaves them, to glide from (--smooth). */
static void
land_pose(struct game *g)
{
	int i;

	for (i = 0; i < LAND_UNITS; i++) {
		g->land_from[i][0] = g->sim.land.u[i].x;
		g->land_from[i][1] = g->sim.land.u[i].y;
	}
}

static void
land_enter(struct game *g)
{
	int i;

	game_background(g, "pic/lbat");
	game_portrait(g, NULL);
	for (i = 0; i < 5; i++)
		load_bank(g, land_banks[i], 1, &g->land_spr[i]);
	if (g->btl_tiles.tile_w != 32)
		tileset_load(g->packs, "tiles/battle", &g->btl_tiles);
	g->march_held = g->march_tap = 0;
	g->land_button = 0;
	g->land_wait = 0;
	g->land_stage = 0;
	land_pose(g);
}

static void
land_over(struct game *g)
{
	int won = sim_land_end(&g->sim);

	if (won) {
		game_goto(g, &screen_assault);
		return;
	}
	/* "Your men are scattered and exhausted..." (CSTR 191), but a
	 * lone captain goes to prison without it (VA 0x5b61, 0x18d35). */
	sim_log_failure(&g->sim, g->assault_town);
	if (g->sim.jail.fresh) {
		game_goto(g, &screen_prison);
		return;
	}
	g->land_stage = 1;
}

static void
land_event(struct game *g, const struct ui_event *ev)
{
	static const struct {
		int	key, bits;
	} walk[] = {
		{ UI_KEY_LEFT, 1 }, { UI_KEY_RIGHT, 2 }, { UI_KEY_UP, 4 },
		{ UI_KEY_DOWN, 8 }, { UI_KEY_UP_LEFT, 5 }, { UI_KEY_UP_RIGHT, 6 },
		{ UI_KEY_DOWN_LEFT, 9 }, { UI_KEY_DOWN_RIGHT, 10 },
	};
	size_t i;

	if (g->land_stage == 1) {
		if (ev->type == EV_CLICK || (ev->type == EV_KEY &&
		    (ev->key == UI_KEY_ENTER || ev->key == UI_KEY_ESCAPE)))
			game_goto(g, g->sim.jail.fresh ? &screen_prison :
			    &screen_sail);
		return;
	}
	if (ev->type == EV_CLICK || (ev->type == EV_CHAR && ev->ch == ' ') ||
	    (ev->type == EV_KEY && ev->key == UI_KEY_ENTER)) {
		g->land_button = 2;	/* a press, held until the next pass */
		return;
	}
	if (ev->type != EV_KEY && ev->type != EV_KEYUP)
		return;
	for (i = 0; i < sizeof(walk) / sizeof(walk[0]); i++)
		if (walk[i].key == ev->key) {
			if (ev->type == EV_KEY) {
				g->march_held |= walk[i].bits;
				g->march_tap |= walk[i].bits;
			} else
				g->march_held &= ~walk[i].bits;
		}
}

static void
land_tick(struct game *g)
{
	int walk;

	if (g->fading != 0 || g->land_stage != 0)
		return;
	if (++g->land_wait < LAND_FRAMES)
		return;
	g->land_wait = 0;
	land_pose(g);
	/* A tap released between two passes still moves on the next. */
	walk = g->march_held | g->march_tap;
	g->march_tap = 0;
	sim_land_tick(&g->sim, walk & 1 ? -1 : walk & 2 ? 1 : 0, walk & 4 ?
	    -1 : walk & 8 ? 1 : 0, g->land_button > 0);
	play_sound(g, g->sim.land.sound, -1);
	if (g->land_button > 0)
		g->land_button--;
	if (g->sim.land.result != 0)
		land_over(g);
}

/* A side's lines on the panel: morale, men and muskets, terrain. */
static void
land_side(struct game *g, const struct land_unit *u, int y_morale,
    int y_men, int y_terrain, int bank)
{
	const struct sim_land *l = &g->sim.land;
	char buf[32];
	int t, m;

	if (u->flags == 0 && (u->flags & 7) != LAND_FORT)
		return;
	m = u->morale >> 4;
	draw(g, FONT_SMALL, 496, y_morale, 80, game_word(g, W_LAND, 9), 0);
	draw(g, FONT_SMALL, 570, y_morale, 70,
	    game_word(g, W_MORALE, (size_t)(m > 5 ? 5 : m)), 0);
	snprintf(buf, sizeof(buf), "%d", u->men);
	draw(g, FONT_SMALL, 512, y_men, 60, buf, 0);
	snprintf(buf, sizeof(buf), "%d", u->muskets);
	draw(g, FONT_SMALL, 590, y_men, 40, buf, 0);
	t = g->world->land_ground[g->world->land_screens[l->screen][((u->y /
	    32) % 15) * 15 + (u->x / 32) % 15]];
	draw(g, FONT_SMALL, 496, y_terrain, 80, game_word(g, W_LAND, 8), 0);
	draw(g, FONT_SMALL, 570, y_terrain, 70, game_word(g, W_LAND,
	    (size_t)t), 0);
	if ((size_t)t < g->land_spr[bank].count)
		fb_frame(&g->fb, &g->land_spr[bank].frames[t], 489,
		    bank == 1 ? 260 : 137);
}

static void
land_draw(struct game *g)
{
	const struct sim_land *l = &g->sim.land;
	const struct land_unit *u;
	char name[96];
	int cx, cy, i, k, w, nation, x, y;

	if (g->btl_tiles.tile_w == 32)
		for (cy = 0; cy < BATTLE_SCREEN; cy++)
			for (cx = 0; cx < BATTLE_SCREEN; cx++) {
				struct image t;

				tileset_tile(&g->btl_tiles,
				    g->world->land_screens[l->screen][cy *
				    BATTLE_SCREEN + cx], 0, &t);
				fb_image(&g->fb, &t, cx * 32, cy * 32, 0);
			}
	nation = g->sim.towns[l->town].nation & 7;
	for (i = 1; i < LAND_UNITS; i++) {
		u = &l->u[i];
		if (u->flags == 0 || (u->flags & 7) == LAND_FORT)
			continue;
		/* The party 1 (0 selected); the town nation + 2, horsemen
		 * nation + 6 (VA 0x6034). */
		k = i < 4 ? (i == l->selected ? 0 : 1) :
		    (u->flags & 4) ? nation + 6 : nation + 2;
		if ((size_t)k >= g->land_spr[0].count)
			continue;
		/* With --smooth, on the way from the last pass, a pass
		 * behind the rules, in 1/LAND_FRAMES pixels. */
		w = g->smooth && g->land_stage == 0 && abs(u->x - g->land_from[i][0]) <= LAND_JUMP &&
		    abs(u->y - g->land_from[i][1]) <= LAND_JUMP ? g->land_wait :
		    LAND_FRAMES;
		x = g->land_from[i][0] * (LAND_FRAMES - w) + u->x * w;
		y = g->land_from[i][1] * (LAND_FRAMES - w) + u->y * w;
		fb_frame_sub(&g->fb, &g->land_spr[0].frames[k], x, y,
		    LAND_FRAMES);
	}
	g->ink[0] = g->ink[1] = g->ink[2] = 0;
	snprintf(name, sizeof(name), "%s%s", g->sim.player.name,
	    game_word(g, W_POSSESSIVE, 0));
	w = text_width(&g->font[FONT_SMALL], name);
	draw(g, FONT_SMALL, 560 - w / 2, 354, FB_W, name, 0);
	w = text_width(&g->font[FONT_SMALL], game_word(g, W_LAND, 6));
	draw(g, FONT_SMALL, 560 - w / 2, 370, FB_W, game_word(g, W_LAND, 6), 0);
	snprintf(name, sizeof(name), "%s", game_word(g, W_NATION,
	    (size_t)(nation & 3)));
	w = text_width(&g->font[FONT_SMALL], name);
	draw(g, FONT_SMALL, 560 - w / 2, 9, FB_W, name, 0);
	w = text_width(&g->font[FONT_SMALL], game_word(g, W_LAND, 7));
	draw(g, FONT_SMALL, 560 - w / 2, 25, FB_W, game_word(g, W_LAND, 7), 0);
	land_side(g, &l->u[l->selected], 400, 429, 457, 1);
	u = &l->u[l->shown];
	land_side(g, u, 55, 85, 113, (u->flags & 7) == LAND_HORSE ? 3 :
	    (u->flags & 7) == LAND_FORT ? 4 : 2);
	if (g->land_stage == 1) {
		char text[300];

		g->ink[0] = 240;
		g->ink[1] = 220;
		g->ink[2] = 170;
		fill(text, sizeof(text), game_str(g, S_ATTACK_FAILED),
		    g->sim.towns[l->town].name, NULL);
		draw(g, FONT_BODY, 100, 200, 520, text, 0);
		continue_prompt(g);
	}
}

const struct screen screen_land = {
	"land battle", land_enter, land_event, land_tick, land_draw,
};

/*
 * A treasure map (VA 0x29ece): wood.pcx, the held quarters of
 * mapframe.pcx over it, torn edges along the missing ones (treasmap.spr
 * frames 5-16), and the world drawn in tmap.imb's parchment tiles, the
 * spot at the centre, through the frame's colour 6.  The mark (frame 0
 * or 1 for the relative's map, 2 for the others) sits on the spot; the
 * title (CSTN 31) is centred at the top, "Somewhere near %s" at the
 * bottom.  The whole page takes tmap.imb's palette.
 */
#define TMAP_HOLE		6	/* the frame's see-through colour */
#define TMAP_INK		2
#define W_MAP_TITLE		31
#define MAP_TITLE_NEAR		4
#define W_RELATIVE		37
#define W_PEOPLE		93

/* Each quarter's rectangle in the frame (VA 0x41438). */
static const int tmap_quarter[4][4] = {
	{ 0, 0, 318, 251 }, { 319, 0, 639, 251 },
	{ 0, 252, 318, 479 }, { 319, 252, 639, 479 },
};

/* The torn edges for each set of quarters held: frame, x, y. */
static const struct {
	int	n;
	int	edge[4][3];
} tmap_torn[16] = {
	[1] = { 3, { { 5, 319, 30 }, { 6, 28, 252 }, { 13, 318, 252 } } },
	[2] = { 3, { { 7, 306, 34 }, { 8, 318, 252 }, { 14, 307, 252 } } },
	[3] = { 2, { { 6, 28, 252 }, { 8, 318, 252 } } },
	[4] = { 3, { { 9, 319, 252 }, { 10, 28, 238 }, { 15, 319, 244 } } },
	[5] = { 2, { { 5, 319, 30 }, { 9, 319, 252 } } },
	[6] = { 4, { { 9, 319, 252 }, { 10, 28, 238 }, { 7, 306, 34 },
	    { 8, 318, 252 } } },
	[7] = { 2, { { 9, 319, 252 }, { 8, 318, 252 } } },
	[8] = { 3, { { 11, 309, 252 }, { 12, 319, 240 }, { 16, 311, 241 } } },
	[9] = { 4, { { 5, 319, 30 }, { 6, 28, 252 }, { 11, 309, 252 },
	    { 12, 319, 240 } } },
	[10] = { 2, { { 11, 309, 252 }, { 7, 306, 34 } } },
	[11] = { 2, { { 11, 309, 252 }, { 6, 28, 252 } } },
	[12] = { 2, { { 10, 28, 238 }, { 12, 319, 240 } } },
	[13] = { 2, { { 5, 319, 30 }, { 12, 319, 240 } } },
	[14] = { 2, { { 7, 306, 34 }, { 10, 28, 238 } } },
};

/*
 * The map in true colours, for a pack's frame, torn edges and tiles: as
 * the original's, with the key colour (pure magenta) for its holes.  The
 * page is composed at the canvas's size: the wood, each held quarter of
 * the frame, the torn edges, and in every key pixel the world's tile;
 * then the mark.  NULL when the pieces are not all true colours.
 */
#define TMAP_KEY(p)	((p)[0] == 255 && (p)[1] == 0 && (p)[2] == 255)

static const uint8_t *
tmap_sample(const struct image *img, int cx, int cy, int w, int h)
{
	int stride = img->rgba_stride ? img->rgba_stride : img->rgba_w;

	return (img->rgba + ((size_t)((long)cy * img->rgba_h / h) * stride +
	    (size_t)((long)cx * img->rgba_w / w)) * 4);
}

static void
tmap_put(uint8_t *d, const uint8_t *a, const uint8_t *world)
{
	int k;

	if (a[3] == 0)
		return;
	if (TMAP_KEY(a))
		a = world;
	if (a[3] == 255) {
		memcpy(d, a, 3);
		return;
	}
	for (k = 0; k < 3; k++)
		d[k] = (uint8_t)((a[k] * a[3] + d[k] * (255 - a[3]) + 127) / 255);
}

static uint8_t *
tmap_compose_hd(struct game *g, const struct image *wood,
    const struct image *frame, const struct sprite_bank *spr,
    const struct tileset *ts, int held, int mark, int mx, int my)
{
	static const uint8_t none[4] = { 238, 222, 180, 255 };
	const struct sim_map *m = &g->sim.maps[g->tmap_maps[g->tmap_i]];
	int W = g->fb.out_w, H = g->fb.out_h, cx, cy, k, x0, y0, x1, y1;
	int ax, ay, wx, wy, cell;
	const struct frame *f;
	const uint8_t *p, *world;
	struct image tile;
	uint8_t *out, wpx[4];

	if (wood->rgba == NULL || frame->rgba == NULL || ts->rgba == NULL ||
	    g->world == NULL || (out = malloc((size_t)W * H * 4)) == NULL)
		return (NULL);
	for (cy = 0; cy < H; cy++)
		for (cx = 0; cx < W; cx++) {
			uint8_t *d = out + ((size_t)cy * W + cx) * 4;
			int lx = (int)((long)cx * FB_W / W);
			int ly = (int)((long)cy * FB_H / H);
			const uint8_t *layers[5];
			int n = 0, e;

			/* The world under this pixel, should a hole show it. */
			ax = (int)((long)cx * 40 * ts->art_tile / W);
			ay = (int)((long)cy * 30 * ts->art_tile / H);
			wx = m->x - 20 + ax / ts->art_tile;
			wy = m->y - 15 + ay / ts->art_tile;
			world = none;
			if (wx >= 0 && wy >= 0 && wx < WORLD_CELLS_W &&
			    wy < WORLD_CELLS_H) {
				cell = g->world->map[wy * WORLD_CELLS_W + wx];
				tileset_tile(ts, (size_t)cell, 0, &tile);
				if (tile.rgba != NULL) {
					memcpy(wpx, tile.rgba +
					    ((size_t)(ay % ts->art_tile) *
					    tile.rgba_stride + ax %
					    ts->art_tile) * 4, 3);
					wpx[3] = 255;
					world = wpx;
				}
			}
			p = tmap_sample(wood, cx, cy, W, H);
			memcpy(d, p, 3);
			d[3] = 255;
			for (k = 0; k < 4; k++)
				if ((held & (1 << k)) && lx >= tmap_quarter[k][0] &&
				    lx <= tmap_quarter[k][2] &&
				    ly >= tmap_quarter[k][1] &&
				    ly <= tmap_quarter[k][3])
					layers[n++] = tmap_sample(frame, cx, cy,
					    W, H);
			for (e = 0; e < tmap_torn[held].n; e++) {
				if ((size_t)tmap_torn[held].edge[e][0] >=
				    spr->count)
					continue;
				f = &spr->frames[tmap_torn[held].edge[e][0]];
				x0 = (int)((long)tmap_torn[held].edge[e][1] * W /
				    FB_W);
				y0 = (int)((long)tmap_torn[held].edge[e][2] * H /
				    FB_H);
				x1 = (int)((long)(tmap_torn[held].edge[e][1] +
				    f->w) * W / FB_W);
				y1 = (int)((long)(tmap_torn[held].edge[e][2] +
				    f->h) * H / FB_H);
				if (f->rgba == NULL || cx < x0 || cx >= x1 ||
				    cy < y0 || cy >= y1 || n >= 5)
					continue;
				layers[n++] = f->rgba + ((size_t)((long)(cy -
				    y0) * f->rgba_h / (y1 - y0)) * f->rgba_w +
				    (size_t)((long)(cx - x0) * f->rgba_w /
				    (x1 - x0))) * 4;
			}
			for (k = 0; k < n; k++)
				tmap_put(d, layers[k], world);
		}
	/* The mark, on the spot. */
	if ((size_t)mark < spr->count && (f = &spr->frames[mark])->rgba !=
	    NULL) {
		x0 = (int)((long)(FB_W / 2 + mx) * W / FB_W);
		y0 = (int)((long)(FB_H / 2 + my) * H / FB_H);
		x1 = (int)((long)(FB_W / 2 + mx + f->w) * W / FB_W);
		y1 = (int)((long)(FB_H / 2 + my + f->h) * H / FB_H);
		for (cy = y0 < 0 ? 0 : y0; cy < y1 && cy < H; cy++)
			for (cx = x0 < 0 ? 0 : x0; cx < x1 && cx < W; cx++)
				tmap_put(out + ((size_t)cy * W + cx) * 4,
				    f->rgba + ((size_t)((long)(cy - y0) *
				    f->rgba_h / (y1 - y0)) * f->rgba_w +
				    (size_t)((long)(cx - x0) * f->rgba_w /
				    (x1 - x0))) * 4, none);
	}
	return (out);
}

/* A sprite frame into a picture, clipped. */
static void
image_frame(struct image *img, const struct frame *f, int x, int y)
{
	int fx, fy, px, py;

	for (fy = 0; fy < f->h; fy++)
		for (fx = 0; fx < f->w; fx++) {
			px = x + fx;
			py = y + fy;
			if (px < 0 || py < 0 || px >= img->w || py >= img->h ||
			    (f->mask != NULL && !f->mask[fy * f->w + fx]))
				continue;
			img->pixels[py * img->w + px] = f->pixels[fy * f->w + fx];
		}
}

static void
tmap_compose(struct game *g)
{
	const struct sim_map *m = &g->sim.maps[g->tmap_maps[g->tmap_i]];
	struct image frame = { 0 };
	struct sprite_bank spr = { 0 };
	struct tileset ts = { 0 };
	struct blob b;
	int k, x, y, cx, cy, held = m->pieces & MAP_WHOLE, mark, mx, my;

	struct image wood = { 0 };
	struct tileset hd = { 0 };
	uint8_t *rgba;

	game_background(g, "pic/wood");
	game_portrait(g, NULL);
	if (g->bg.pixels == NULL || g->bg.w != FB_W || g->bg.h != FB_H)
		return;
	/* The map is composed on the indices; a pack's wood is kept for the
	 * true colours. */
	wood.rgba = g->bg.rgba;
	wood.rgba_w = g->bg.rgba_w;
	wood.rgba_h = g->bg.rgba_h;
	wood.rgba_stride = g->bg.rgba_stride;
	g->bg.rgba = NULL;
	image_drop_rgba(&g->bg);
	pic_load(g->packs, "pic/mapframe", &frame);
	if (frame.pixels != NULL && frame.w == FB_W && frame.h == FB_H)
		for (k = 0; k < 4; k++) {
			if (!(held & (1 << k)))
				continue;
			for (y = tmap_quarter[k][1]; y <= tmap_quarter[k][3];
			    y++)
				memcpy(&g->bg.pixels[y * FB_W +
				    tmap_quarter[k][0]],
				    &frame.pixels[y * FB_W + tmap_quarter[k][0]],
				    (size_t)(tmap_quarter[k][2] -
				    tmap_quarter[k][0] + 1));
		}
	load_bank(g, "spr/treasmap", 0, &spr);
	for (k = 0; k < tmap_torn[held].n; k++)
		if ((size_t)tmap_torn[held].edge[k][0] < spr.count)
			image_frame(&g->bg, &spr.frames[tmap_torn[held].edge[k][0]],
			    tmap_torn[held].edge[k][1],
			    tmap_torn[held].edge[k][2]);
	/* The world, 40 x 30 cells around the spot, under the holes. */
	if (packs_load(g->packs, "tiles/tmap", &b) == 0) {
		if (tileset_decode(b.data, b.len, &ts) != 0)
			memset(&ts, 0, sizeof(ts));
		blob_free(&b);
	}
	if (ts.pixels != NULL && ts.tile_w == WORLD_TILE && g->world != NULL) {
		for (y = 0; y < FB_H; y++)
			for (x = 0; x < FB_W; x++) {
				if (g->bg.pixels[y * FB_W + x] != TMAP_HOLE)
					continue;
				cx = m->x - 20 + x / WORLD_TILE;
				cy = m->y - 15 + y / WORLD_TILE;
				if (cx < 0 || cy < 0 || cx >= WORLD_CELLS_W ||
				    cy >= WORLD_CELLS_H)
					continue;
				k = g->world->map[cy * WORLD_CELLS_W + cx];
				if ((size_t)k >= ts.count)
					continue;
				g->bg.pixels[y * FB_W + x] = ts.pixels[(size_t)k *
				    WORLD_TILE * WORLD_TILE + (y % WORLD_TILE) *
				    WORLD_TILE + x % WORLD_TILE];
			}
		if (ts.has_palette) {
			memcpy(g->bg.palette, ts.palette, sizeof(g->bg.palette));
			memcpy(g->fb.pal, ts.palette, sizeof(g->fb.pal));
		}
	}
	tileset_free(&ts);
	/* The mark: the relative's (frame 0 when the relative is 1, else
	 * 1), the cross for the others. */
	if (g->tmap_maps[g->tmap_i] == MAP_RELATIVE) {
		mark = g->sim.relative == 1 ? 0 : 1;
		mx = mark == 0 ? -8 : -7;
		my = mark == 0 ? -10 : -15;
	} else {
		mark = 2;
		mx = -8;
		my = -11;
	}
	if ((size_t)mark < spr.count)
		image_frame(&g->bg, &spr.frames[mark], FB_W / 2 + mx,
		    FB_H / 2 + my);
	memcpy(g->ink, bg_colour(g, TMAP_INK), 3);
	/* A pack's map, in true colours over the indices. */
	if (tileset_load(g->packs, "tiles/tmap", &hd) == 0 &&
	    (rgba = tmap_compose_hd(g, &wood, &frame, &spr, &hd, held, mark,
	    mx, my)) != NULL) {
		g->bg.rgba = rgba;
		g->bg.rgba_w = g->fb.out_w;
		g->bg.rgba_h = g->fb.out_h;
		g->bg.rgba_stride = 0;
		g->ink[0] = 240;	/* amber, on the wood */
		g->ink[1] = 170;
		g->ink[2] = 60;
	}
	tileset_free(&hd);
	free(wood.rgba);
	image_free(&frame);
	bank_free(&spr);
}

static void
tmap_enter(struct game *g)
{
	g->tmap_i = 0;
	tmap_compose(g);
}

static void
tmap_event(struct game *g, const struct ui_event *ev)
{
	if (!(ev->type == EV_CLICK || (ev->type == EV_KEY &&
	    (ev->key == UI_KEY_ENTER || ev->key == UI_KEY_ESCAPE))))
		return;
	if (++g->tmap_i < g->tmap_n)
		tmap_compose(g);
	else
		game_goto(g, g->tmap_back);
}

static void
tmap_draw(struct game *g)
{
	int map = g->tmap_maps[g->tmap_i];
	const struct sim_map *m = &g->sim.maps[map];
	char text[160];

	if (map == MAP_RELATIVE)
		fill(text, sizeof(text), game_word(g, W_MAP_TITLE, 1),
		    game_word(g, W_RELATIVE, (size_t)(g->sim.relative & 3)),
		    NULL);
	else
		snprintf(text, sizeof(text), "%s",
		    game_word(g, W_MAP_TITLE, (size_t)map));
	draw_centered(g, FONT_TITLE, 0, text);
	if (m->pieces & MAP_SHOWN && (size_t)m->town < g->sim.ntowns) {
		fill(text, sizeof(text), game_word(g, W_MAP_TITLE,
		    MAP_TITLE_NEAR), g->sim.towns[m->town].name, NULL);
		draw_centered(g, FONT_BODY, 460, text);
	}
}

const struct screen screen_tmap = {
	"treasure map", tmap_enter, tmap_event, NULL, tmap_draw,
};

/* Show one map ("map" >= 0) or all those held, then go to "back". */
static void
tmap_show(struct game *g, int map, const struct screen *back)
{
	int i;

	g->tmap_n = 0;
	for (i = 0; i < SIM_MAPS; i++)
		if ((map < 0 || i == map) && g->sim.maps[i].pieces != 0)
			g->tmap_maps[g->tmap_n++] = i;
	if (g->tmap_n == 0) {
		game_goto(g, back);
		return;
	}
	g->tmap_back = back;
	game_goto(g, &screen_tmap);
}

/*
 * Digging (VA 0xa801), after the 3 days: nothing (search.pcx, CSTR 189
 * at (100, 300) up to x 560); the pirate's chest (find-t.pcx, CSTR 183);
 * the relative (reunionf/reunionm.pcx, CSTR 184 then the lost treasure's
 * tale, CSTR 188, at (340, 150) - (320, 150) for the men - up to x 620,
 * then the lost treasure's map); the lost treasure (inca, aztec, olmec,
 * maya.pcx, CSTR 185 at (100, 320) up to x 560).  Texts in ink entry 1.
 */
#define S_CHEST			183
#define S_REUNION		184
#define S_LOST_FOUND		185
#define S_LOST_TALE		188
#define S_NO_TREASURE		189

static const char *const dig_people_pic[4] = {
	"pic/inca", "pic/aztec", "pic/olmec", "pic/maya",
};

static void
dig_page(struct game *g)
{
	const struct sim_find *f = &g->dig[g->dig_i];
	const char *pic;

	if (g->dig_n > 0 && g->dig_page == 0)	/* VA 0xa801 */
		game_music(g, f->map == MAP_PIRATE ? MUS_TREASURE :
		    f->map == MAP_RELATIVE ? MUS_RESCUE : MUS_LOST_TREASURE);
	if (g->dig_n == 0)
		pic = "pic/search";
	else if (f->map == MAP_PIRATE)
		pic = "pic/find-t";
	else if (f->map == MAP_RELATIVE)
		pic = f->value & 1 ? "pic/reunionm" : "pic/reunionf";
	else
		pic = dig_people_pic[f->value & 3];
	game_background(g, pic);
	game_portrait(g, NULL);
	if (g->bg.pixels != NULL)
		memcpy(g->ink, bg_colour(g, TMAP_INK), 3);
}

static void
dig_enter(struct game *g)
{
	if (g->dig_i == 0 && g->dig_page == 0)
		g->dig_n = sim_search(&g->sim, g->dig);
	dig_page(g);
}

static void
dig_event(struct game *g, const struct ui_event *ev)
{
	const struct sim_find *f = &g->dig[g->dig_i];

	if (!(ev->type == EV_CLICK || (ev->type == EV_KEY &&
	    (ev->key == UI_KEY_ENTER || ev->key == UI_KEY_ESCAPE))))
		return;
	if (g->dig_n > 0 && f->map == MAP_RELATIVE && g->dig_page == 0) {
		g->dig_page = 1;		/* the tale */
		return;
	}
	g->dig_page = 0;
	if (++g->dig_i < g->dig_n) {
		if (f->map == MAP_RELATIVE) {
			/* The lost treasure's map, then the next find. */
			tmap_show(g, MAP_LOST, &screen_dig);
			return;
		}
		dig_page(g);
		return;
	}
	g->dig_i = 0;
	if (g->dig_n > 0 && f->map == MAP_RELATIVE)
		tmap_show(g, MAP_LOST, &screen_sail);
	else
		game_goto(g, &screen_sail);
}

static void
dig_draw(struct game *g)
{
	const struct sim_find *f = &g->dig[g->dig_i];
	char text[640], num[24];
	const char *who;

	if (g->dig_n == 0) {
		draw(g, FONT_BODY, 100, 300, 460, game_str(g, S_NO_TREASURE),
		    0);
	} else if (f->map == MAP_PIRATE) {
		snprintf(num, sizeof(num), "%ld", f->gold);
		fill(text, sizeof(text), game_str(g, S_CHEST), num, NULL);
		draw(g, FONT_BODY, 100, 300, 460, text, 0);
	} else if (f->map == MAP_RELATIVE) {
		who = game_word(g, W_RELATIVE, (size_t)(f->value & 3));
		if (g->dig_page == 0)
			fill(text, sizeof(text), game_str(g, S_REUNION), who,
			    who);
		else
			fill(text, sizeof(text), game_str(g, S_LOST_TALE),
			    game_word(g, W_PEOPLE, (size_t)(f->value & 3)),
			    NULL);
		draw(g, FONT_BODY, f->value & 1 ? 320 : 340, 150,
		    f->value & 1 ? 300 : 280, text, 0);
	} else {
		fill(text, sizeof(text), game_str(g, S_LOST_FOUND),
		    game_word(g, W_PEOPLE, (size_t)(f->value & 3)), NULL);
		draw(g, FONT_BODY, 100, 320, 460, text, 0);
	}
	continue_prompt(g);
}

const struct screen screen_dig = {
	"dig", dig_enter, dig_event, NULL, dig_draw,
};

/*
 * A villain met in his town (VA 0x18913), on evilone.pcx at (340, 150)
 * up to x 620: "You spot the evil %s %s sneaking out a back entrance"
 * (CSTR 131), the duel in the alley, and when he is beaten "I will tell
 * you what I know" (CSTR 132) and a piece of the relative's map.  Then
 * back where the player was.
 */
#define S_VILLAIN_MET		131
#define S_VILLAIN_BEATEN	132

static void
villain_enter(struct game *g)
{
	game_background(g, "pic/evilone");
	game_portrait(g, NULL);
	if (g->bg.pixels != NULL)
		memcpy(g->ink, bg_colour(g, TMAP_INK), 3);
}

static void
villain_event(struct game *g, const struct ui_event *ev)
{
	if (!(ev->type == EV_CLICK || (ev->type == EV_KEY &&
	    (ev->key == UI_KEY_ENTER || ev->key == UI_KEY_ESCAPE))))
		return;
	if (g->villain_stage == 0) {
		g->sw_villain = 1;
		game_goto(g, &screen_sword);
	} else
		tmap_show(g, MAP_RELATIVE, g->villain_back);
}

static void
villain_draw(struct game *g)
{
	char text[640];

	villain_text(g, g->villain, g->villain_stage == 0 ? S_VILLAIN_MET :
	    S_VILLAIN_BEATEN, text, sizeof(text));
	draw(g, FONT_BODY, 340, 150, 280, text, 0);
	continue_prompt(g);
}

const struct screen screen_villain = {
	"villain", villain_enter, villain_event, NULL, villain_draw,
};

/*
 * The mission's contact (VA 0x1f8f7): "The cousin of the governor of %s
 * accepts the letter..." (CSTR 204 + type) at (200, 200) up to x 620.
 * The original clears the screen first (VA 0x262d3, not traced); the
 * engine writes on the parchment page.
 */
#define S_MISSION_DONE		204	/* + type: 205-208 */

static void
contact_enter(struct game *g)
{
	game_background(g, "pic/textback");
	game_portrait(g, NULL);
}

static void
contact_event(struct game *g, const struct ui_event *ev)
{
	if (ev->type == EV_CLICK || (ev->type == EV_KEY &&
	    (ev->key == UI_KEY_ENTER || ev->key == UI_KEY_ESCAPE)))
		game_goto(g, g->villain_back);
}

static void
contact_draw(struct game *g)
{
	const struct sim_mission *m = &g->sim.mission;
	int type = m->type < 0 ? -m->type : m->type;
	char text[640], pirate[80];

	captain_name(g, m->pirate, pirate, sizeof(pirate));
	fill(text, sizeof(text), game_str(g, S_MISSION_DONE + type),
	    (size_t)m->giver < g->sim.ntowns ? g->sim.towns[m->giver].name :
	    "", pirate);
	draw(g, FONT_BODY, 200, 200, 420, text, 0);
	continue_prompt(g);
}

const struct screen screen_contact = {
	"contact", contact_enter, contact_event, NULL, contact_draw,
};

/*
 * The crew speaks when a month passes at sea (VA 0x1dd0e): the sailing
 * master warns on smaster.pcx at (340, 150) up to x 620 (CSTR 140, or
 * 210 after 12 months); a mutiny is told on mutiny.pcx at (50, 350) up
 * to x 620 (CSTR 139), then the duel with the leader on the deck.
 */
#define S_MUTINY		139
#define S_RESTLESS		140
#define S_ANXIOUS		210
#define S_FOOD_LEFT		124	/* "...only %s days food left!" */
#define S_STARVING		180	/* 181 on foot */
#define S_WRECKED		182	/* "...a shallow reef..." */

static void
crew_enter(struct game *g)
{
	if (g->sim.crew_event == CREW_WRECKED)
		game_fm(g, FM_CRASH);	/* before the message (VA 0x2849c) */
	game_background(g, g->sim.crew_event == CREW_MUTINY ? "pic/mutiny" :
	    "pic/smaster");
	game_portrait(g, NULL);
	if (g->bg.pixels != NULL)
		memcpy(g->ink, bg_colour(g, TMAP_INK), 3);
}

static void
crew_event(struct game *g, const struct ui_event *ev)
{
	if (!(ev->type == EV_CLICK || (ev->type == EV_KEY &&
	    (ev->key == UI_KEY_ENTER || ev->key == UI_KEY_ESCAPE))))
		return;
	if (g->sim.crew_event == CREW_MUTINY) {
		g->sw_mutiny = 1;
		game_goto(g, &screen_sword);
		return;
	}
	g->sim.crew_event = CREW_NONE;
	/* The last ship on a reef: marooned (VA 0x1f2a1). */
	game_goto(g, g->sim.jail.fresh ? &screen_prison : &screen_sail);
}

static void
crew_draw(struct game *g)
{
	if (g->sim.crew_event == CREW_MUTINY)
		draw(g, FONT_BODY, 50, 350, 570, game_str(g, S_MUTINY), 0);
	else if (g->sim.crew_event == CREW_HUNGRY) {
		/* On smaster.pcx, the box at (340, 150) up to x 620 (VA
		 * 0x2849c). */
		char text[256], days[16];

		snprintf(days, sizeof(days), "%d", g->sim.food_days);
		fill(text, sizeof(text), game_str(g, S_FOOD_LEFT), days, NULL);
		draw(g, FONT_BODY, 340, 150, 280, text, 0);
	} else if (g->sim.crew_event == CREW_WRECKED)
		draw(g, FONT_BODY, 340, 150, 280, game_str(g, S_WRECKED), 0);
	else if (g->sim.crew_event == CREW_STARVING)
		draw(g, FONT_BODY, 340, 150, 280, game_str(g, S_STARVING +
		    (g->sim.fleet.ashore ? 1 : 0)), 0);
	else
		draw(g, FONT_BODY, 340, 150, 280, game_str(g,
		    g->sim.crew_event == CREW_ANXIOUS ? S_ANXIOUS :
		    S_RESTLESS), 0);
	continue_prompt(g);
}

const struct screen screen_crew = {
	"crew", crew_enter, crew_event, NULL, crew_draw,
};

/*
 * The roll call on leaving town (VA 0x17efb): on the town's arrival
 * picture, "Roll call reveals that %s crew members have deserted" (CSTR
 * 109, at (340, 150) up to x 620).
 */
#define S_DESERTED		109
#define S_ESCAPED		110

static void
deserted_enter(struct game *g)
{
	int t = g->deserted_town;

	game_background(g, g->sim.fleet.ashore || (t >= 0 &&
	    (size_t)t < g->sim.ntowns && (g->sim.towns[t].flags &
	    TOWN_INLAND)) ? "pic/arr_land" : "pic/arr_sea");
	game_portrait(g, NULL);
	if (g->bg.pixels != NULL)
		memcpy(g->ink, bg_colour(g, TMAP_INK), 3);
}

static void
deserted_event(struct game *g, const struct ui_event *ev)
{
	if (!(ev->type == EV_CLICK || (ev->type == EV_KEY &&
	    (ev->key == UI_KEY_ENTER || ev->key == UI_KEY_ESCAPE))))
		return;
	if (!g->escape_page && g->sim.escaped != 0) {
		g->escape_page = 1;	/* "...jumps overboard" (CSTR 110) */
		return;
	}
	game_goto(g, g->world != NULL ? &screen_sail : &screen_cabin);
}

static void
deserted_draw(struct game *g)
{
	char text[320], num[16];

	if (g->escape_page) {
		char name[80];

		captain_name(g, g->sim.escaped, name, sizeof(name));
		fill(text, sizeof(text), game_str(g, S_ESCAPED), name, NULL);
		draw(g, FONT_BODY, 340, 150, 280, text, 0);
		continue_prompt(g);
		return;
	}
	snprintf(num, sizeof(num), "%d", g->sim.deserted);
	fill(text, sizeof(text), game_str(g, S_DESERTED), num, NULL);
	draw(g, FONT_BODY, 340, 150, 280, text, 0);
	continue_prompt(g);
}

const struct screen screen_deserted = {
	"deserted", deserted_enter, deserted_event, NULL, deserted_draw,
};

/*
 * The Map of the Caribbean (VA 0xb464), on citymap.pcx: a dot per town
 * (misc.crs frame 1 at town fields 29, 30 less 3), the list of 20 town
 * names centred at x 554 from y 40 every 20, scrolled by the arrows at
 * the top and bottom of the right panel, and the selected town's panel:
 * its name centred at (84, 384), then "Forts:", "Status:", "Nation:",
 * "Soldiers:", "Citizens:" (CSTN 94) at (243, 384), (404, 384), (84,
 * 440), (243, 440), (404, 440), "?" while its information is not known
 * (CSTN 92.5).  The selected town's mark (frame 2) and the player's
 * (frame 3: his town's dot, or at sea his cell x 0.75) blink in turn.
 * A click picks a town on the map (within 2 pixels) or in the list;
 * DONE, Escape or Space leaves.  The engine's Up and Down select the
 * previous and next town (the original moves a bar over the rows).
 */
#define W_TOWN_FIELD		94
#define CM_ROWS			20
#define CM_BLINK		8	/* frames between the marks */

static int
cm_in(int x, int y, int x0, int y0, int x1, int y1)
{
	return (x >= x0 && x <= x1 && y >= y0 && y <= y1);
}

static void
citymap_show_town(struct game *g, int t)
{
	if (t < 0 || (size_t)t >= g->sim.ntowns)
		return;
	g->cm_sel = t;
	if (t < g->cm_scroll)
		g->cm_scroll = t;
	else if (t >= g->cm_scroll + CM_ROWS)
		g->cm_scroll = t - CM_ROWS + 1;
}

static void
citymap_enter(struct game *g)
{
	struct era_towns e;
	size_t i;

	game_background(g, "pic/citymap");
	game_portrait(g, NULL);
	if (g->bg.pixels != NULL)
		memcpy(g->ink, g->bg.palette, 3);
	memset(g->cm_x, 0, sizeof(g->cm_x));
	memset(g->cm_y, 0, sizeof(g->cm_y));
	if (towns_for_era(g->packs, g->layout, g->exe, g->exe_len,
	    g->world != NULL ? g->world->era : 0, &e, g->err,
	    sizeof(g->err)) == 0)
		for (i = 0; i < e.count && i < g->sim.ntowns; i++) {
			g->cm_x[i] = e.towns[i].citymap_x;
			g->cm_y[i] = e.towns[i].citymap_y;
		}
	g->cm_scroll = 0;
	g->cm_move = 0;
	if ((size_t)g->cm_sel >= g->sim.ntowns)
		g->cm_sel = 0;
	if (here(g) != NULL) {
		g->cm_px = g->cm_x[g->sim.player_town];
		g->cm_py = g->cm_y[g->sim.player_town];
	} else {
		g->cm_px = (int)((double)(g->sim.fleet.x / WORLD_TILE) * 0.75);
		g->cm_py = (int)((double)(g->sim.fleet.y / WORLD_TILE) * 0.75);
	}
}

/*
 * Leaving the map: in the debug mode, the ship goes to the spot last
 * right-clicked, cell = pixel / 0.75, with 10 more food (VA 0xb6d3).
 * The engine moves it only at sea.
 */
static void
citymap_done(struct game *g)
{
	struct sim_fleet *f = &g->sim.fleet;

	if (g->debug && g->cm_move && here(g) == NULL && !f->ashore) {
		f->x = (int)(g->cm_mx / 0.75) * WORLD_TILE;
		f->y = (int)(g->cm_my / 0.75) * WORLD_TILE;
		f->fx = f->fy = 0;
		f->speed = 0;
		f->leaving = -1;
		f->food += 10;
	}
	g->cm_move = 0;
	game_goto(g, &screen_cabin);
}

static void
citymap_event(struct game *g, const struct ui_event *ev)
{
	int x = ev->x, y = ev->y, max = (int)g->sim.ntowns - CM_ROWS;
	size_t i;

	if (ev->type == EV_RCLICK && g->debug && x < 480 && y < 480) {
		g->cm_move = 1;
		g->cm_mx = g->cm_px = x;
		g->cm_my = g->cm_py = y;
		return;
	}
	if (ev->type == EV_KEY) {
		if (ev->key == UI_KEY_ESCAPE || ev->key == UI_KEY_ENTER)
			citymap_done(g);
		else if (ev->key == UI_KEY_UP && g->cm_sel > 0)
			citymap_show_town(g, g->cm_sel - 1);
		else if (ev->key == UI_KEY_DOWN)
			citymap_show_town(g, g->cm_sel + 1);
		return;
	}
	if (ev->type == EV_CHAR && ev->ch == ' ') {
		citymap_done(g);
		return;
	}
	if (ev->type != EV_CLICK)
		return;
	/* DONE: the original reaches up to y 315; the engine starts it at
	 * 326, below the dots of the coast towns it covers. */
	if (cm_in(x, y, 348, 326, 479, 359))
		citymap_done(g);
	else if (cm_in(x, y, 0, 0, 479, 359)) {		/* the map */
		for (i = 0; i < g->sim.ntowns; i++)
			if (abs(g->cm_x[i] - x) <= 2 && abs(g->cm_y[i] - y) <= 2) {
				g->cm_sel = (int)i;
				break;
			}
	} else if (cm_in(x, y, 480, 30, 639, 449)) {	/* the list */
		if (y >= 40 && (size_t)((y - 40) / 20 + g->cm_scroll) <
		    g->sim.ntowns && (y - 40) / 20 < CM_ROWS)
			g->cm_sel = (y - 40) / 20 + g->cm_scroll;
	} else if (cm_in(x, y, 480, 0, 639, 29)) {
		if (g->cm_scroll > 0)
			g->cm_scroll--;
	} else if (cm_in(x, y, 480, 450, 639, 479)) {
		if (g->cm_scroll < max)
			g->cm_scroll++;
	}
}

static void
citymap_draw(struct game *g)
{
	static const int at[6][2] = { { 84, 384 }, { 243, 384 },
	    { 404, 384 }, { 84, 440 }, { 243, 440 }, { 404, 440 } };
	const struct sim_town *t;
	char text[6][96], num[24];
	size_t i;
	int k, w, blink = (int)(g->ticks / CM_BLINK) & 1;

	if (g->misc.count > 3) {
		for (i = 0; i < g->sim.ntowns; i++)
			fb_frame(&g->fb, &g->misc.frames[1], g->cm_x[i] - 3,
			    g->cm_y[i] - 3);
		if (blink)
			fb_frame(&g->fb, &g->misc.frames[2],
			    g->cm_x[g->cm_sel] - 3, g->cm_y[g->cm_sel] - 3);
		else
			fb_frame(&g->fb, &g->misc.frames[3], g->cm_px - 3,
			    g->cm_py - 3);
	}
	for (k = 0; k < CM_ROWS && (size_t)(g->cm_scroll + k) <
	    g->sim.ntowns; k++) {
		const char *name = g->sim.towns[g->cm_scroll + k].name;

		w = text_width(&g->font[FONT_SMALL], name);
		draw(g, FONT_SMALL, 554 - w / 2, 33 + 20 * k, 150, name, 0);
	}
	t = &g->sim.towns[g->cm_sel];
	snprintf(text[0], sizeof(text[0]), "%s", t->name);
	for (k = 1; k < 6; k++) {
		if (!(t->flags & 0x80)) {
			snprintf(text[k], sizeof(text[k]), "%s%s",
			    game_word(g, W_TOWN_FIELD, (size_t)(k - 1)),
			    game_word(g, W_UNKNOWN, 5));
			continue;
		}
		switch (k) {
		case 1:
			snprintf(num, sizeof(num), "%d", t->forts);
			break;
		case 2:
			snprintf(num, sizeof(num), "%s", game_word(g, 9,
			    (size_t)((t->wealth / 64) & 3)));
			break;
		case 3:
			snprintf(num, sizeof(num), "%s", game_word(g, W_NATION,
			    (size_t)(t->nation <= 4 ? t->nation : 4)));
			break;
		case 4:
			snprintf(num, sizeof(num), "%d", t->soldiers * 10);
			break;
		default:
			snprintf(num, sizeof(num), "%d", t->citizens * 10);
			break;
		}
		snprintf(text[k], sizeof(text[k]), "%s%s",
		    game_word(g, W_TOWN_FIELD, (size_t)(k - 1)), num);
	}
	for (k = 0; k < 6; k++) {	/* centred in their cells */
		w = text_width(&g->font[FONT_SMALL], text[k]);
		draw(g, FONT_SMALL, at[k][0] - w / 2, at[k][1] - 6, 158,
		    text[k], 0);
	}
}

const struct screen screen_citymap = {
	"caribbean map", citymap_enter, citymap_event, NULL, citymap_draw,
};

/*
 * A captured Spaniard or pirate (VA 0xdb7b), on hostage.pcx at (340,
 * 150) up to x 620: "I have information which may be of value to you,"
 * %s says (CSTR 165): the treasure fleet, the silver train, or ransom.
 * The news: "The %s is in %s." (CSTN 27.0) with "Treasure Fleet" or
 * "Silver Train" (CSTR 46, 45) and the town it is in, or when away
 * "Spain" or "Peru" (CSTN 27.1, 27.2).
 */
#define S_HOSTAGE_TALK		165

static void
hostage_enter(struct game *g)
{
	game_background(g, "pic/hostage");
	game_portrait(g, NULL);
	if (g->bg.pixels != NULL)
		memcpy(g->ink, bg_colour(g, TMAP_INK), 3);
}

static void
hostage_choose(struct game *g, int i)
{
	sim_hostage_answer(&g->sim, i);
	if (i == 2)
		game_goto(g, &screen_battle_end);
	else
		g->hostage_said = i;
}

static void
hostage_event(struct game *g, const struct ui_event *ev)
{
	if (g->hostage_said < 0) {
		menu_event(g, ev, 3, NULL, hostage_choose, NULL);
		return;
	}
	if (ev->type == EV_CLICK || (ev->type == EV_KEY &&
	    (ev->key == UI_KEY_ENTER || ev->key == UI_KEY_ESCAPE)))
		game_goto(g, &screen_battle_end);
}

static void
hostage_draw(struct game *g)
{
	char text[512], name[80];
	const char *arg[2];

	if (g->hostage_said < 0) {
		captain_name(g, g->sim.hostage, name, sizeof(name));
		fill(text, sizeof(text), game_str(g, S_HOSTAGE_TALK), name,
		    NULL);
		draw(g, FONT_BODY, 340, 150, 280, text, 1);
		return;
	}
	{
		int at = g->hostage_said == 0 ? g->sim.fleet_town :
		    g->sim.train_town;

		arg[0] = game_str(g, g->hostage_said == 0 ? 46 : 45);
		arg[1] = at >= 0 && (size_t)at < g->sim.ntowns ?
		    g->sim.towns[at].name : game_word(g, 27,
		    g->hostage_said == 0 ? 1 : 2);
	}
	fill_args(text, sizeof(text), game_word(g, 27, 0), arg, 2);
	draw(g, FONT_BODY, 340, 150, 280, text, 0);
	continue_prompt(g);
}

const struct screen screen_hostage = {
	"hostage", hostage_enter, hostage_event, NULL, hostage_draw,
};

/*
 * The merchant's daughter (VA 0x21e56), on merdaugh.pcx at (320, 150) up
 * to x 620: "That evening, you enjoy a delicious meal with your lovely
 * wife" (CSTR 295) when married into the family; else "...The merchant
 * introduces you to his daughter..." (296): conversation (nothing more)
 * or a proposal: "...my father will not let me wed anyone who is not
 * wealthy" (297), or his debt (298, at (320, 50)): pay it or withdraw.
 * Short of 500 gold, on merchant.pcx at (160, 40) up to x 480, "The
 * merchant sneers at your offer of help..." (300); paid, on
 * wedding.pcx at (360, 150), "Amid great merriment, you are married..."
 * (299).
 */
static void
merdaugh_page(struct game *g)
{
	game_music(g, g->merdaugh == 299 ? MUS_WEDDING : MUS_FAMILY);
	game_background(g, g->merdaugh == 300 ? "pic/merchant" :
	    g->merdaugh == 299 ? "pic/wedding" : "pic/merdaugh");
	game_portrait(g, NULL);
	if (g->bg.pixels != NULL)
		memcpy(g->ink, bg_colour(g, TMAP_INK), 3);
	g->selected = 0;
}

static void
merdaugh_enter(struct game *g)
{
	merdaugh_page(g);
}

static void
merdaugh_choose(struct game *g, int i)
{
	if (g->merdaugh == 296) {
		if (i == 0) {			/* conversation: no more */
			game_goto(g, &screen_town);
			return;
		}
		g->merdaugh = sim_merchant_propose(&g->sim);
	} else {				/* 298: the debt */
		if (i != 0) {
			game_goto(g, &screen_town);
			return;
		}
		g->merdaugh = sim_merchant_pay_debt(&g->sim);
	}
	merdaugh_page(g);
}

static void
merdaugh_event(struct game *g, const struct ui_event *ev)
{
	if (g->merdaugh == 296 || g->merdaugh == 298) {
		menu_event(g, ev, 2, NULL, merdaugh_choose, NULL);
		return;
	}
	if (ev->type == EV_CLICK || (ev->type == EV_KEY &&
	    (ev->key == UI_KEY_ENTER || ev->key == UI_KEY_ESCAPE)))
		game_goto(g, &screen_town);
}

static void
merdaugh_draw(struct game *g)
{
	int menu = g->merdaugh == 296 || g->merdaugh == 298;

	if (g->merdaugh == 300)
		draw(g, FONT_BODY, 160, 40, 320, game_str(g, 300), 0);
	else if (g->merdaugh == 299)
		draw(g, FONT_BODY, 360, 150, 260, game_str(g, 299), 0);
	else
		draw(g, FONT_BODY, 320, g->merdaugh == 298 ? 50 : 150, 300,
		    game_str(g, g->merdaugh), menu);
	if (!menu)
		continue_prompt(g);
}

const struct screen screen_merdaugh = {
	"merchant's daughter", merdaugh_enter, merdaugh_event, NULL,
	merdaugh_draw,
};

/*
 * News of villains (VA 0x1eba6): "The %s quite familiar with recent
 * comings and goings of prominent persons of %s. Do you wish to inquire
 * about... ~%s?~No one?" (CSTR 154: CSTN 16 citizens, bartender,
 * traveller or crewmen; the town; the villains whose town or trail it
 * is, one a line), then "%s is now in %s." (108) or "%s left for %s a
 * few months ago." (107), and the question again with two or more.
 * Pictures and boxes:
 * citizens town.pcx (20, 50) up to x 250, bartender bartenda.pcx (320,
 * 150) up to 560, traveller infoman.pcx (20, 150) up to 280.
 */
static const struct {
	const char	*pic;
	int		 x, y, w;
} vnews_at[4] = {
	{ "pic/town", 20, 50, 230 }, { "pic/bartenda", 320, 150, 240 },
	{ "pic/infoman", 20, 150, 260 }, { "pic/crew", 40, 350, 520 },
};

/* Offer the news of "town" by "source", then go to "back"; false when
 * nobody is known there. */
static int
vnews_offer(struct game *g, int source, int town, const struct screen *back)
{
	g->vnews_n = sim_villain_news(&g->sim, town, g->vnews_list);
	if (g->vnews_n == 0)
		return (0);
	g->vnews_source = source & 3;
	g->vnews_town = town;
	g->vnews_back = back;
	g->vnews_said = 0;
	g->selected = 0;
	game_goto(g, &screen_vnews);
	return (1);
}

static void
vnews_enter(struct game *g)
{
	game_background(g, vnews_at[g->vnews_source].pic);
	game_portrait(g, NULL);
	if (g->bg.pixels != NULL)
		memcpy(g->ink, bg_colour(g, TMAP_INK), 3);
}

static void
vnews_choose(struct game *g, int i)
{
	if (i >= g->vnews_n) {			/* "No one?" */
		game_goto(g, g->vnews_back);
		return;
	}
	g->vnews_who = g->vnews_list[i];
	g->vnews_said = sim_villain_whereabouts(&g->sim, g->vnews_who,
	    g->vnews_town, &g->vnews_where);
}

static void
vnews_event(struct game *g, const struct ui_event *ev)
{
	if (g->vnews_said == 0) {
		menu_event(g, ev, g->vnews_n + 1, NULL, vnews_choose, NULL);
		return;
	}
	if (ev->type == EV_CLICK || (ev->type == EV_KEY &&
	    (ev->key == UI_KEY_ENTER || ev->key == UI_KEY_ESCAPE))) {
		/* The question again, unless he was the only one (VA
		 * 0x1ef56). */
		if (g->vnews_n == 1) {
			game_goto(g, g->vnews_back);
			return;
		}
		g->vnews_said = 0;
		g->selected = 0;
	}
}

static void
vnews_draw(struct game *g)
{
	const char *arg[3];
	char text[900], names[400], one[80];
	int k;

	if (g->vnews_said != 0) {
		villain_name(g, g->vnews_who, one, sizeof(one));
		fill(text, sizeof(text), game_str(g, g->vnews_said), one,
		    town_name(g, g->vnews_where));
		draw(g, FONT_BODY, vnews_at[g->vnews_source].x,
		    vnews_at[g->vnews_source].y, vnews_at[g->vnews_source].w,
		    text, 0);
		continue_prompt(g);
		return;
	}
	names[0] = '\0';
	for (k = 0; k < g->vnews_n; k++) {
		villain_name(g, g->vnews_list[k], one, sizeof(one));
		if (k > 0)
			strncat(names, "\r~", sizeof(names) - strlen(names) - 1);
		strncat(names, one, sizeof(names) - strlen(names) - 1);
	}
	arg[0] = game_word(g, 16, (size_t)g->vnews_source);
	arg[1] = town_name(g, g->vnews_town);
	arg[2] = names;
	fill_args(text, sizeof(text), game_str(g, 154), arg, 3);
	draw(g, FONT_BODY, vnews_at[g->vnews_source].x,
	    vnews_at[g->vnews_source].y, vnews_at[g->vnews_source].w, text, 1);
}

const struct screen screen_vnews = {
	"villain news", vnews_enter, vnews_event, NULL, vnews_draw,
};

/*
 * The end of a career (VA 0x45d0), on the sunset picture of its rating
 * (sunset4.pcx for ratings 0-3 down to sunset1.pcx from 12): "The
 * Further Adventures of the Pirate %s..." (CSTR 74, at (20, 200) up to
 * x 260); the summary (VA 0x4a1a, at (20, 100) up to x 260): the titles
 * (CSTR 75, CSTN 38), the wealth (CSTN 39), the repute (CSTN 40), and
 * the age, the bride, the family found, the wounds and the mood (CSTR
 * 76, CSTN 41, CSTR 77, 78, CSTN 42); a famous expedition gives its
 * share instead (CSTN 40.1).  Then the verdict (VA 0x51c2): "Your
 * pirating days are over. Your seafaring adventures have made you a %s
 * in %s, where %s" (CSTR 82: CSTN 43 by rating, the town, CSTN 44 +
 * rating, one of three).  Unless the career was cut short, "The lure of
 * the sea is strong... Remain in retirement / Return to pirating?" (CSTR
 * 72); then the Hall of Fame (CSTR 73).
 */
enum { END_FURTHER, END_TITLES, END_WEALTH, END_REPUTE, END_LIFE,
    END_VERDICT, END_LURE };

static void
ending_enter(struct game *g)
{
	static const char *sunset[4] = { "pic/sunset4", "pic/sunset3",
	    "pic/sunset2", "pic/sunset1" };
	int tier;

	sim_career_end(&g->sim, &g->end);
	tier = g->end.rating / 4 > 3 ? 3 : g->end.rating / 4;
	game_background(g, sunset[tier]);
	game_portrait(g, NULL);
	if (g->bg.pixels != NULL)
		memcpy(g->ink, bg_colour(g, TMAP_INK), 3);
	g->end_page = END_FURTHER;
	g->end_phrase = rng_below(&g->sim.rng, 3);	/* VA 0x51c2 */
	g->selected = 0;
}

static void
ending_lure(struct game *g, int i)
{
	if (i == 1) {				/* back to pirating */
		g->sim.fleet.ashore = 0;
		game_goto(g, g->sim.player_town >= 0 ? &screen_town :
		    &screen_cabin);
		return;
	}
	g->selected = 0;
	game_goto(g, &screen_retire);
}

static void
ending_event(struct game *g, const struct ui_event *ev)
{
	if (g->end_page == END_LURE) {
		menu_event(g, ev, 2, NULL, ending_lure, NULL);
		return;
	}
	if (!(ev->type == EV_CLICK || (ev->type == EV_KEY &&
	    (ev->key == UI_KEY_ENTER || ev->key == UI_KEY_ESCAPE))))
		return;
	if (g->end_page == END_FURTHER && g->sim.player.expedition >= 0)
		g->end_page = END_REPUTE;	/* the expedition's share */
	else if (g->end_page == END_VERDICT) {
		if (g->end_forced || g->sim.player.expedition >= 0) {
			game_goto(g, &screen_retire);
			return;
		}
		g->selected = 0;
		g->end_page = END_LURE;
		return;
	}
	if (g->end_page == END_REPUTE && g->sim.player.expedition >= 0)
		g->end_page = END_LIFE;		/* straight to the verdict */
	g->end_page++;
	if (g->end_page == END_VERDICT)
		game_music(g, MUS_VERDICT);		/* VA 0x51c2 */
}

/* The titles held: "a Spanish Captain and an English Major". */
static void
ending_titles(struct game *g, char *out, size_t len)
{
	const char *arg[3];
	char list[300], one[96];
	int k, first = 1;

	list[0] = '\0';
	for (k = 0; k < 4; k++) {
		if (g->sim.rank[k] == 0)
			continue;
		arg[0] = game_word(g, 68, (size_t)k);
		arg[1] = game_word(g, W_RANK, g->sim.rank[k]);
		fill_args(one, sizeof(one), game_word(g, 38, first ? 2 : 3),
		    arg, 2);
		strncat(list, one, sizeof(list) - strlen(list) - 1);
		first = 0;
	}
	arg[0] = game_word(g, 38, g->end.ranks == 0 ? 0 : 1);
	arg[1] = g->end.ranks == 0 ? "" : list;
	arg[2] = game_word(g, 38, (size_t)g->end.member);
	fill_args(out, len, game_str(g, 75), arg, 3);
}

static void
ending_life(struct game *g, char *out, size_t len)
{
	const char *arg[2];
	char part[300], num[16];

	snprintf(num, sizeof(num), "%d", g->end.age);
	fill(out, len, game_str(g, 76), num, NULL);
	strncat(out, " ", len - strlen(out) - 1);
	if (g->sim.married == 0)
		snprintf(part, sizeof(part), "%s", game_word(g, 41, 0));
	else
		fill(part, sizeof(part), game_word(g, 41, 1), game_word(g, 41,
		    (size_t)(g->sim.married + 1)), NULL);
	strncat(out, part, len - strlen(out) - 1);
	if (g->sim.relative > 0) {
		/* The number word as written: the tolower at VA 0x4ed8
		 * is discarded. */
		arg[0] = game_word(g, 79, (size_t)g->sim.relative);
		arg[1] = g->sim.relative == 1 ? "" : game_word(g, 79, 0);
		fill_args(part, sizeof(part), game_str(g, 77), arg, 2);
		strncat(out, " ", len - strlen(out) - 1);
		strncat(out, part, len - strlen(out) - 1);
	}
	if (g->sim.injuries > 0) {
		fill(part, sizeof(part), game_str(g, 78), game_word(g, 80,
		    (size_t)(g->sim.injuries - 1 > 2 ? 2 :
		    g->sim.injuries - 1)), NULL);
		strncat(out, " ", len - strlen(out) - 1);
		strncat(out, part, len - strlen(out) - 1);
	}
	fill(part, sizeof(part), game_word(g, 42, 0), game_word(g, 42,
	    (size_t)g->end.mood), NULL);
	strncat(out, " ", len - strlen(out) - 1);
	strncat(out, part, len - strlen(out) - 1);
}

static void
ending_draw(struct game *g)
{
	const char *arg[3];
	char text[1200], num[24], land[80], acres[24];
	int t;

	switch (g->end_page) {
	case END_FURTHER:
		fill(text, sizeof(text), game_str(g, 74), g->sim.player.name,
		    NULL);
		draw(g, FONT_BODY, 20, 200, 240, text, 0);
		continue_prompt(g);
		return;
	case END_TITLES:
		ending_titles(g, text, sizeof(text));
		break;
	case END_WEALTH:
		snprintf(num, sizeof(num), "%ld", g->sim.fortune);
		land[0] = '\0';
		if (g->sim.acres > 0) {
			snprintf(acres, sizeof(acres), "%d", g->sim.acres * 50);
			fill(land, sizeof(land), game_word(g, 39, 1), acres,
			    NULL);
		}
		arg[0] = num;
		arg[1] = land;
		arg[2] = game_word(g, 39, (size_t)g->end.wealth);
		fill_args(text, sizeof(text), game_word(g, 39, 0), arg, 3);
		break;
	case END_REPUTE:
		if (g->sim.player.expedition >= 0) {
			snprintf(num, sizeof(num), "%ld", g->sim.fortune);
			fill(text, sizeof(text), game_word(g, 40, 1), num,
			    game_word(g, 40, (size_t)g->end.repute));
		} else
			fill(text, sizeof(text), game_word(g, 40, 0),
			    game_word(g, 40, (size_t)g->end.repute), NULL);
		break;
	case END_LIFE:
		ending_life(g, text, sizeof(text));
		break;
	case END_VERDICT:
		t = g->sim.player_town >= 0 ? g->sim.player_town :
		    g->sim.fleet.leaving;
		arg[0] = game_word(g, 43, (size_t)g->end.rating);
		arg[1] = t >= 0 ? town_name(g, t) : "";
		arg[2] = game_word(g, (size_t)(44 + g->end.rating),
		    (size_t)g->end_phrase);
		fill_args(text, sizeof(text), game_str(g, 82), arg, 3);
		break;
	default:
		draw(g, FONT_BODY, 20, 100, 240, game_str(g, 72), 1);
		return;
	}
	draw(g, FONT_BODY, 20, 100, 240, text, 0);
	continue_prompt(g);
}

const struct screen screen_ending = {
	"ending", ending_enter, ending_event, NULL, ending_draw,
};

/*
 * The last ship abandoned (VA 0x1f65f): on plunder.pcx at (100, 40) up
 * to x 620, "You do not have enough crewmen to man your last ship, so
 * regretfully you abandon it." (CSTR 127); then marooned, or imprisoned
 * in the town.
 */
static void
lastship_enter(struct game *g)
{
	game_music(g, MUS_PRISON);
	game_background(g, "pic/plunder");
	game_portrait(g, NULL);
	if (g->bg.pixels != NULL)
		memcpy(g->ink, bg_colour(g, TMAP_INK), 3);
}

static void
lastship_event(struct game *g, const struct ui_event *ev)
{
	if (ev->type == EV_CLICK || (ev->type == EV_KEY &&
	    (ev->key == UI_KEY_ENTER || ev->key == UI_KEY_ESCAPE))) {
		sim_abandon_last(&g->sim);
		game_goto(g, &screen_prison);
	}
}

static void
lastship_draw(struct game *g)
{
	draw(g, FONT_BODY, 100, 40, 520, game_str(g, 127), 0);
	continue_prompt(g);
}

const struct screen screen_lastship = {
	"last ship", lastship_enter, lastship_event, NULL, lastship_draw,
};

/*
 * The sailors of a captured ship (VA 0xdb7b): on crew.pcx, "Captain, %s
 * sailors wish to join our crew. ~Take them~Leave them" (CSTR 166) at
 * (100, 280) up to x 540.
 */
static void
volunteers_enter(struct game *g)
{
	game_background(g, "pic/crew");
	game_portrait(g, NULL);
	if (g->bg.pixels != NULL)
		memcpy(g->ink, bg_colour(g, TMAP_INK), 3);
}

static void
volunteers_choose(struct game *g, int i)
{
	if (i == 0)
		sim_take_volunteers(&g->sim);
	g->sim.battle.volunteers = 0;
	game_goto(g, &screen_battle_end);
}

static void
volunteers_event(struct game *g, const struct ui_event *ev)
{
	menu_event(g, ev, 2, NULL, volunteers_choose, NULL);
}

static void
volunteers_draw(struct game *g)
{
	char text[256], men[16];

	snprintf(men, sizeof(men), "%d", g->sim.battle.volunteers);
	fill(text, sizeof(text), game_str(g, 166), men, NULL);
	draw(g, FONT_BODY, 100, 280, 440, text, 1);
}

const struct screen screen_volunteers = {
	"volunteers", volunteers_enter, volunteers_event, NULL,
	volunteers_draw,
};
