/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026, Olivier Cochard-Labbé
 *
 * Format and pack tests on synthetic data; they need no game files.
 */

#define _POSIX_C_SOURCE 200809L

#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "data/exe.h"
#include "data/fsys.h"
#include "data/pack.h"
#include "data/sha256.h"
#include "assets/credits.h"
#include "assets/font.h"
#include "assets/fonts.h"
#include "assets/img.h"
#include "assets/pcx.h"
#include "assets/pic.h"
#include "assets/png.h"
#include "assets/rsd.h"
#include "assets/sheet.h"
#include "assets/sprite.h"
#include "assets/text.h"
#include "assets/tiled.h"
#include "assets/tiles.h"
#include "assets/tileset.h"
#include "assets/towns.h"
#include "assets/world.h"
#include "gfx/fb.h"
#include "gfx/text.h"

static int failures;

#define CHECK(cond) do {						\
	if (!(cond)) {							\
		fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__,	\
		    __LINE__, #cond);					\
		failures++;						\
	}								\
} while (0)

/*
 * 3x2 image, bytes per line 4.  The first run (5 x 0x07) crosses the end
 * of row 0, as the original encoder allows.  0xC5 must be escaped.
 */
static size_t
make_pcx(uint8_t *buf)
{
	static const uint8_t rle[] = { 0xC5, 0x07, 0xC1, 0xC5, 0x02, 0x03 };
	size_t n;

	memset(buf, 0, 128);
	buf[0] = 10;		/* manufacturer */
	buf[1] = 5;		/* version */
	buf[2] = 1;		/* RLE */
	buf[3] = 8;		/* bpp */
	buf[8] = 2;		/* xmax */
	buf[10] = 1;		/* ymax */
	buf[65] = 1;		/* planes */
	buf[66] = 4;		/* bytes per line */
	n = 128;
	memcpy(buf + n, rle, sizeof(rle));
	n += sizeof(rle);
	buf[n++] = 0x0C;
	memset(buf + n, 0xAB, 768);
	buf[n] = 0x12;
	return (n + 768);
}

static void
test_pcx(void)
{
	uint8_t buf[1024];
	struct image img;
	size_t len;
	static const uint8_t want[] = { 7, 7, 7, 7, 0xC5, 2 };

	len = make_pcx(buf);
	CHECK(pcx_decode(buf, len, &img) == 0);
	CHECK(img.w == 3 && img.h == 2);
	CHECK(img.pixels != NULL &&
	    memcmp(img.pixels, want, sizeof(want)) == 0);
	CHECK(img.has_palette && img.palette[0] == 0x12 &&
	    img.palette[767] == 0xAB);
	image_free(&img);

	/* Truncated RLE data must fail, not read past the buffer. */
	memmove(buf + 129, buf + 134, len - 134);
	CHECK(pcx_decode(buf, len - 5, &img) == -1);
	CHECK(pcx_decode(buf, 100, &img) == -1);
}

static void
test_rsd(void)
{
	uint8_t px[304];
	static const uint8_t ok[] = {
		0x82,			/* skip 2 */
		0x02, 9, 8,		/* copy 2 */
		0x80, 0x2C, 0x01,	/* skip 300, cursor at 304 */
		0x80, 0x00, 0x00,	/* end */
	};
	static const uint8_t overflow[] = { 0x80, 0x2E, 0x01, 0x03, 1, 2, 3 };
	static const uint8_t zero_op[] = { 0x00, 0x80, 0x00, 0x00 };

	memset(px, 0, sizeof(px));
	CHECK(rsd_apply(ok, sizeof(ok), px, sizeof(px)) == 0);
	CHECK(px[1] == 0 && px[2] == 9 && px[3] == 8 && px[4] == 0);
	/* Skipping past the image, or a missing end marker, is an error. */
	CHECK(rsd_apply(ok, sizeof(ok), px, sizeof(px) - 1) == -1);
	CHECK(rsd_apply(ok, sizeof(ok) - 3, px, sizeof(px)) == -1);
	/* Copy of 3 bytes at 302 would write past 304 pixels. */
	CHECK(rsd_apply(overflow, sizeof(overflow), px, sizeof(px)) == -1);
	CHECK(rsd_apply(zero_op, sizeof(zero_op), px, sizeof(px)) == -1);
}

static int
write_text(const char *dir, const char *name, const char *text)
{
	char path[1024];
	FILE *fp;

	snprintf(path, sizeof(path), "%s/%s", dir, name);
	if ((fp = fopen(path, "w")) == NULL)
		return (-1);
	fputs(text, fp);
	return (fclose(fp));
}

static void
test_packs(void)
{
	char low[] = "/tmp/flibustier-test-XXXXXX";
	char high[] = "/tmp/flibustier-test-XXXXXX";
	char path[1024];
	struct packs *packs;
	struct blob b;

	if (fsys_mkdtemp(low) == NULL || fsys_mkdtemp(high) == NULL) {
		CHECK(!"mkdtemp");
		return;
	}
	CHECK(write_text(low, "pack.ini",
	    "# base pack\n[pack]\nname = Low\nlicense = CC0-1.0\n"
	    "[assets]\nmusic/title = title.ogg\npic/town = town.png\n") == 0);
	CHECK(write_text(low, "title.ogg", "low-title") == 0);
	CHECK(write_text(low, "town.png", "low-town") == 0);
	CHECK(write_text(high, "pack.ini",
	    "[pack]\nname = High\n[assets]\nmusic/title = Remix.OGG\n") == 0);
	CHECK(write_text(high, "Remix.OGG", "high-title") == 0);

	packs = packs_new();
	CHECK(packs != NULL);
	CHECK(packs_add_dir(packs, low) == 0);
	CHECK(packs_add_dir(packs, high) == 0);
	CHECK(packs_count(packs) == 2);
	CHECK(strcmp(packs_name(packs, 0), "High") == 0);
	CHECK(strcmp(packs_license(packs, 1), "CC0-1.0") == 0);

	/* The higher pack overrides one asset; the rest falls through. */
	CHECK(packs_load(packs, "music/title", &b) == 0);
	CHECK(b.len == 10 && memcmp(b.data, "high-title", 10) == 0);
	CHECK(strcmp(b.ext, "ogg") == 0 && strcmp(b.pack, "High") == 0);
	blob_free(&b);
	CHECK(packs_load(packs, "pic/town", &b) == 0);
	CHECK(b.len == 8 && strcmp(b.pack, "Low") == 0);
	blob_free(&b);

	CHECK(packs_load(packs, "pic/missing", &b) == -1);
	CHECK(packs_load(packs, "../etc/passwd", &b) == -1);
	CHECK(packs_load(packs, "Pic/Town", &b) == -1);
	packs_free(packs);

	/* Fragments in pack.d/ add assets after pack.ini, which keeps its
	 * name and its entries first. */
	snprintf(path, sizeof(path), "%s/pack.d", low);
	CHECK(fsys_mkdir(path) == 0);
	CHECK(write_text(path, "music.ini", "[pack]\nname = Ignored\n"
	    "[assets]\nmusic/harbour = town.png\npic/town = title.ogg\n") == 0);
	CHECK(write_text(path, "notes.txt", "[assets]\nbad line\n") == 0);
	packs = packs_new();
	CHECK(packs_add_dir(packs, low) == 0);
	CHECK(strcmp(packs_name(packs, 0), "Low") == 0);
	CHECK(packs_load(packs, "music/harbour", &b) == 0 && b.len == 8);
	blob_free(&b);
	CHECK(packs_load(packs, "pic/town", &b) == 0 &&
	    memcmp(b.data, "low-town", 8) == 0);	/* pack.ini first */
	blob_free(&b);
	packs_free(packs);
	CHECK(write_text(path, "music.ini", "[assets]\nno equals\n") == 0);
	packs = packs_new();
	CHECK(packs_add_dir(packs, low) == -1 &&
	    strstr(packs_error(packs), "pack.d/music.ini:2") != NULL);
	packs_free(packs);
	snprintf(path, sizeof(path), "%s/pack.d/music.ini", low);
	unlink(path);
	snprintf(path, sizeof(path), "%s/pack.d/notes.txt", low);
	unlink(path);
	snprintf(path, sizeof(path), "%s/pack.d", low);
	rmdir(path);

	/* Manifests may not point outside their pack. */
	CHECK(write_text(high, "pack.ini",
	    "[assets]\npic/town = ../town.png\n") == 0);
	packs = packs_new();
	CHECK(packs_add_dir(packs, high) == -1);
	CHECK(strstr(packs_error(packs), "syntax error") != NULL);
	packs_free(packs);

	snprintf(path, sizeof(path), "%s/pack.ini", low);
	unlink(path);
	snprintf(path, sizeof(path), "%s/title.ogg", low);
	unlink(path);
	snprintf(path, sizeof(path), "%s/town.png", low);
	unlink(path);
	snprintf(path, sizeof(path), "%s/pack.ini", high);
	unlink(path);
	snprintf(path, sizeof(path), "%s/Remix.OGG", high);
	unlink(path);
	rmdir(low);
	rmdir(high);
}

/* 6x4 RGBA, each 2x2 block one colour: red, green, blue, then white,
 * black, near red. */
static const uint8_t png_6x4[] = {
	0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
	0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x04,
	0x08, 0x06, 0x00, 0x00, 0x00, 0xad, 0x04, 0x4e, 0x43, 0x00, 0x00, 0x00,
	0x22, 0x49, 0x44, 0x41, 0x54, 0x78, 0xda, 0x63, 0xf8, 0xcf, 0xc0, 0xf0,
	0x1f, 0x84, 0xe1, 0x10, 0xc2, 0x45, 0xb0, 0x30, 0x25, 0xa0, 0x80, 0x01,
	0x24, 0x09, 0xc4, 0xbf, 0xb8, 0x58, 0xc1, 0x18, 0xa7, 0x04, 0x00, 0xeb,
	0x8d, 0x33, 0xf5, 0x48, 0x32, 0x5b, 0x98, 0x00, 0x00, 0x00, 0x00, 0x49,
	0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82,
};

static int
write_bytes(const char *dir, const char *name, const uint8_t *data,
    size_t len)
{
	char path[1024];
	FILE *fp;

	snprintf(path, sizeof(path), "%s/%s", dir, name);
	if ((fp = fopen(path, "wb")) == NULL)
		return (-1);
	if (fwrite(data, 1, len, fp) != len) {
		fclose(fp);
		return (-1);
	}
	return (fclose(fp));
}

static void
remove_files(const char *dir, const char *const *names)
{
	char path[1024];

	for (; *names != NULL; names++) {
		snprintf(path, sizeof(path), "%s/%s", dir, *names);
		unlink(path);
	}
	rmdir(dir);
}

/* A written PNG reads back the same, across several stored blocks. */
static void
test_png_write(void)
{
	enum { W = 200, H = 150 };	/* 90150 filtered bytes: two blocks */
	static uint8_t rgb[W * H * 3];
	char dir[] = "/tmp/flibustier-test-XXXXXX", path[1024];
	struct image img;
	struct blob b;
	FILE *fp;
	size_t i, bad = 0;
	long n;

	for (i = 0; i < sizeof(rgb); i++)
		rgb[i] = (uint8_t)(i * 7 + i / 600);
	if (fsys_mkdtemp(dir) == NULL) {
		CHECK(!"mkdtemp");
		return;
	}
	snprintf(path, sizeof(path), "%s/t.png", dir);
	CHECK(png_write_rgb(path, rgb, W, H) == 0);
	memset(&b, 0, sizeof(b));
	if ((fp = fopen(path, "rb")) != NULL) {
		fseek(fp, 0, SEEK_END);
		n = ftell(fp);
		rewind(fp);
		if (n > 0 && (b.data = malloc((size_t)n)) != NULL)
			b.len = fread(b.data, 1, (size_t)n, fp);
		fclose(fp);
	}
	CHECK(b.data != NULL &&
	    png_decode(b.data, b.len, 480, &img) == 0);
	if (b.data != NULL && img.rgba != NULL) {
		CHECK(img.rgba_w == W && img.rgba_h == H);
		for (i = 0; i < (size_t)W * H; i++)
			bad += memcmp(&img.rgba[4 * i], &rgb[3 * i], 3) != 0 ||
			    img.rgba[4 * i + 3] != 255;
		CHECK(bad == 0);
		image_free(&img);
	}
	free(b.data);
	unlink(path);
	rmdir(dir);
}

static void
test_png(void)
{
	struct image img;

	/* Drawn for a 960 pixel high screen: half size in 640x480. */
	CHECK(png_decode(png_6x4, sizeof(png_6x4), 960, &img) == 0);
	CHECK(img.w == 3 && img.h == 2);
	CHECK(img.rgba_w == 6 && img.rgba_h == 4);
	CHECK(img.pixels == NULL && img.rgba != NULL);
	CHECK(img.rgba != NULL && img.rgba[0] == 255 && img.rgba[1] == 0 &&
	    img.rgba[3] == 255);
	/* Art pixel (4, 2): the near red block. */
	CHECK(img.rgba != NULL && img.rgba[4 * (2 * 6 + 4)] == 250 &&
	    img.rgba[4 * (2 * 6 + 4) + 1] == 10);
	image_free(&img);
	/* For 1080p, 6x4 art covers 2.67x1.78 pixels: rounded. */
	CHECK(png_decode(png_6x4, sizeof(png_6x4), 1080, &img) == 0);
	CHECK(img.w == 3 && img.h == 2);
	image_free(&img);
	CHECK(png_decode(png_6x4, sizeof(png_6x4) - 20, 960, &img) == -1);
	CHECK(png_decode(png_6x4, sizeof(png_6x4), 0, &img) == -1);
	CHECK(png_decode(png_6x4, sizeof(png_6x4), 4000, &img) == -1);
}

/*
 * Replacement art: true colours at the pack's art height, with the
 * original picture's palette and pixels when it is there, else a fixed
 * palette.
 */
static void
test_pic(void)
{
	static const char *const low_files[] = { "pack.ini", "town.pcx",
	    "panel.img", NULL };
	static const char *const high_files[] = { "pack.ini", "town.png",
	    NULL };
	static const uint8_t want_orig[] = { 7, 7, 7, 7, 0xC5, 2 };
	/* Cube indices of red, green, blue, white, black and near red. */
	static const uint8_t want_fixed[] = { 180, 30, 5, 215, 0, 180 };
	char low[] = "/tmp/flibustier-test-XXXXXX";
	char high[] = "/tmp/flibustier-test-XXXXXX";
	uint8_t pcx[1024];
	struct packs *packs;
	struct image img;
	struct blob b;
	size_t len;

	if (fsys_mkdtemp(low) == NULL || fsys_mkdtemp(high) == NULL) {
		CHECK(!"mkdtemp");
		return;
	}
	len = make_pcx(pcx);
	CHECK(write_text(low, "pack.ini", "[assets]\npic/town = town.pcx\n")
	    == 0);
	CHECK(write_bytes(low, "town.pcx", pcx, len) == 0);
	CHECK(write_text(high, "pack.ini", "[pack]\nart_height = 960\n"
	    "[assets]\npic/town = town.png\n") == 0);
	CHECK(write_bytes(high, "town.png", png_6x4, sizeof(png_6x4)) == 0);

	/* Over the original: its palette and pixels. */
	packs = packs_new();
	CHECK(packs_add_dir(packs, low) == 0);
	CHECK(packs_add_dir(packs, high) == 0);
	CHECK(packs_art_height(packs) == 960);
	CHECK(packs_load(packs, "pic/town", &b) == 0);
	CHECK(b.art_height == 960 && strcmp(b.ext, "png") == 0);
	blob_free(&b);
	CHECK(packs_load_ext(packs, "pic/town", "pcx", &b) == 0);
	CHECK(b.art_height == 480 && strcmp(b.ext, "pcx") == 0);
	blob_free(&b);
	CHECK(packs_load_ext(packs, "pic/town", "img", &b) == -1);
	CHECK(pic_load(packs, "pic/town", &img) == 0);
	CHECK(img.w == 3 && img.h == 2 && img.rgba != NULL);
	CHECK(img.pixels != NULL &&
	    memcmp(img.pixels, want_orig, sizeof(want_orig)) == 0);
	CHECK(img.palette[0] == 0x12);
	image_free(&img);
	packs_free(packs);

	/* Alone: reduced to the fixed palette. */
	packs = packs_new();
	CHECK(packs_add_dir(packs, high) == 0);
	CHECK(pic_load(packs, "pic/town", &img) == 0);
	CHECK(img.pixels != NULL &&
	    memcmp(img.pixels, want_fixed, sizeof(want_fixed)) == 0);
	CHECK(img.has_palette && img.palette[3 * 180] == 255 &&
	    img.palette[3 * 180 + 1] == 0);
	image_free(&img);
	packs_free(packs);

	/* An img/ picture: the original IMG's pixels, none of its own
	 * palette; the PNG's true colours. */
	{
		static const uint8_t raw_img[] = { 3, 0, 2, 0, 0, 0,
		    1, 2, 3, 4, 5, 6 };

		CHECK(write_bytes(low, "panel.img", raw_img, sizeof(raw_img))
		    == 0);
		CHECK(write_text(low, "pack.ini",
		    "[assets]\npic/town = town.pcx\nimg/panel = panel.img\n")
		    == 0);
		CHECK(write_text(high, "pack.ini", "[pack]\nart_height = 960\n"
		    "[assets]\nimg/panel = town.png\n") == 0);
		packs = packs_new();
		CHECK(packs_add_dir(packs, low) == 0);
		CHECK(pic_load(packs, "img/panel", &img) == 0);
		CHECK(img.w == 3 && img.h == 2 && img.rgba == NULL &&
		    img.pixels[5] == 6);
		image_free(&img);
		CHECK(packs_add_dir(packs, high) == 0);
		CHECK(pic_load(packs, "img/panel", &img) == 0);
		CHECK(img.w == 3 && img.h == 2 && img.rgba != NULL &&
		    memcmp(img.pixels, raw_img + 6, 6) == 0);
		image_free(&img);
		packs_free(packs);
	}

	/* An art height is a whole number from 240 to 4320. */
	packs = packs_new();
	CHECK(write_text(high, "pack.ini", "[pack]\nart_height = 100\n") == 0);
	CHECK(packs_add_dir(packs, high) == -1);
	CHECK(write_text(high, "pack.ini", "[pack]\nart_height = 1080p\n")
	    == 0);
	CHECK(packs_add_dir(packs, high) == -1);
	CHECK(packs_art_height(packs) == 480);
	packs_free(packs);

	remove_files(low, low_files);
	remove_files(high, high_files);
}

/* Canvas pixel (x, y) of a converted frame "w" pixels wide. */
static const uint8_t *
at(const uint8_t *rgb, int w, int x, int y)
{
	return (rgb + 3 * ((size_t)y * w + x));
}

/*
 * The canvas: true-colour art sampled at the output resolution, palette
 * colours enlarged, by a whole or a fractional factor.
 */
static void
test_fb(void)
{
	static struct fb fb;
	struct image img;
	uint8_t *rgb;
	const uint8_t *p;

	CHECK(fb_init(&fb, 100) == -1);
	CHECK(fb_init(&fb, 5000) == -1);
	CHECK(fb_init(&fb, 1081) == 0);		/* rounded to 4:3 */
	CHECK(fb.out_w == 1440 && fb.out_h == 1080);
	if ((rgb = malloc((size_t)1440 * 1080 * 3)) == NULL) {
		CHECK(!"malloc");
		fb_free(&fb);
		return;
	}
	CHECK(png_decode(png_6x4, sizeof(png_6x4), 960, &img) == 0);
	image_fixed_palette(&img);
	CHECK(image_quantize(&img) == 0);

	/* 2x: the 6x4 art fills the 3x2 picture's 6x4 canvas pixels. */
	CHECK(fb_init(&fb, 960) == 0);
	CHECK(fb.out_w == 1280 && fb.out_h == 960);
	fb_clear(&fb, 0);
	fb_image(&fb, &img, 0, 0, 1);
	fb_rect(&fb, 1, 0, 1, 1, 9);		/* over the green block */
	fb_put(&fb, 2 * FB_W + 2, 5);		/* past the picture */
	CHECK(fb.px[1] == 9 && fb.px[0] == 180);	/* 640x480 indices */
	fb_to_rgb(&fb, 256, rgb);
	p = at(rgb, 1280, 0, 0);		/* red art */
	CHECK(p[0] == 255 && p[1] == 0 && p[2] == 0);
	p = at(rgb, 1280, 3, 1);		/* palette entry 9 */
	CHECK(p[0] == 0 && p[1] == 51 && p[2] == 153);
	p = at(rgb, 1280, 5, 3);		/* near red art */
	CHECK(p[0] == 250 && p[1] == 10 && p[2] == 5);
	p = at(rgb, 1280, 4, 4);		/* entry 5 */
	CHECK(p[0] == 0 && p[1] == 0 && p[2] == 255);
	/* Fades dim the art as well. */
	fb_to_rgb(&fb, 128, rgb);
	CHECK(rgb[0] == 127);

	/* 2.25x: 640x480 pixels cover 2 or 3 canvas pixels each way. */
	CHECK(fb_init(&fb, 1080) == 0);
	fb_clear(&fb, 0);
	fb_image(&fb, &img, 0, 0, 1);		/* canvas 0..5 x 0..3 */
	fb_rect(&fb, 3, 0, 1, 1, 9);		/* canvas 6..8 x 0..1 */
	fb_to_rgb(&fb, 256, rgb);
	p = at(rgb, 1440, 5, 3);		/* near red art, 1:1 */
	CHECK(p[0] == 250 && p[1] == 10 && p[2] == 5);
	p = at(rgb, 1440, 8, 1);
	CHECK(p[0] == 0 && p[1] == 51 && p[2] == 153);
	p = at(rgb, 1440, 9, 0);		/* entry 0, black */
	CHECK(p[0] == 0 && p[1] == 0 && p[2] == 0);

	/* Without true colours, the picture's indices are enlarged. */
	image_drop_rgba(&img);
	fb_image(&fb, &img, 0, 0, 0);
	fb_to_rgb(&fb, 256, rgb);
	p = at(rgb, 1440, 1, 1);		/* entry 180 */
	CHECK(p[0] == 255 && p[1] == 0 && p[2] == 0);
	p = at(rgb, 1440, 2, 1);		/* entry 30, green */
	CHECK(p[0] == 0 && p[1] == 255 && p[2] == 0);

	/* An original frame half a pixel over (2x): the canvas shifts by
	 * one canvas pixel, the 640x480 indices go to the nearest pixel. */
	{
		uint8_t dot = 7;
		struct frame f = { 0 };

		f.w = f.h = 1;
		f.pixels = &dot;
		CHECK(fb_init(&fb, 960) == 0);
		fb_clear(&fb, 0);
		fb_frame_sub(&fb, &f, 21, 20, 2);	/* (10.5, 10) */
		CHECK(fb.px[10 * FB_W + 11] == 7 && fb.px[10 * FB_W + 10] == 0);
		CHECK(fb.idx[20 * 1280 + 20] == 0 &&
		    fb.idx[20 * 1280 + 21] == 7 &&
		    fb.idx[21 * 1280 + 22] == 7 &&
		    fb.idx[20 * 1280 + 23] == 0);
		/* On a whole pixel, as fb_frame(). */
		fb_clear(&fb, 0);
		fb_frame_sub(&fb, &f, 20, 20, 2);
		CHECK(fb.idx[20 * 1280 + 20] == 7 &&
		    fb.idx[20 * 1280 + 21] == 7 &&
		    fb.idx[20 * 1280 + 22] == 0);
		/* Its shadow, half a pixel over: entry 3 shaded to 4. */
		{
			uint8_t shade[256];
			int k;

			for (k = 0; k < 256; k++)
				shade[k] = (uint8_t)k;
			shade[3] = 4;
			fb_clear(&fb, 3);
			fb_shadow(&fb, &f, 21, 20, 2, shade);
			CHECK(fb.px[10 * FB_W + 11] == 4 &&
			    fb.px[10 * FB_W + 10] == 3);
			CHECK(fb.idx[20 * 1280 + 20] == 3 &&
			    fb.idx[20 * 1280 + 21] == 4 &&
			    fb.idx[21 * 1280 + 22] == 4 &&
			    fb.idx[20 * 1280 + 23] == 3);
		}
	}
	image_free(&img);
	fb_free(&fb);
	free(rgb);
}

static void
put16(uint8_t *p, unsigned v)
{
	p[0] = (uint8_t)(v & 0xFF);
	p[1] = (uint8_t)(v >> 8);
}

static void
test_sprites(void)
{
	uint8_t buf[256];
	struct sprite_bank bank;
	struct sprite_group grp;
	size_t n;

	/*
	 * Two run-coded frames.  Frame 0 is 3x2: skip 1, 2 pixels on row 0,
	 * then skip to row 1 with the 640 stride (640 - 3 + 1), 1 pixel.
	 */
	memset(buf, 0, sizeof(buf));
	put16(buf, 4);			/* frame 0 offset = 2 * 2 frames */
	put16(buf + 2, 30);		/* frame 1 offset */
	put16(buf + 4, 3);
	put16(buf + 6, 2);
	put16(buf + 8, 0x49);		/* key */
	put16(buf + 10, 1);
	put16(buf + 12, 2);
	buf[14] = 5;
	buf[15] = 6;
	put16(buf + 16, 638);
	put16(buf + 18, 1);
	buf[20] = 7;
	put16(buf + 21, 0xFFFF);	/* 7 padding bytes follow */
	put16(buf + 30, 1);		/* frame 1: 1x1, one run */
	put16(buf + 32, 1);
	put16(buf + 34, 0);
	put16(buf + 36, 0);
	put16(buf + 38, 1);
	buf[40] = 9;
	put16(buf + 41, 0xFFFF);
	n = 43;
	CHECK(bank_decode_runs(buf, n, &bank) == 0);
	CHECK(bank.count == 2 && bank.frames[0].w == 3 &&
	    bank.frames[0].key == 0x49);
	CHECK(bank.frames[0].pixels[1] == 5 && bank.frames[0].pixels[2] == 6 &&
	    bank.frames[0].pixels[4] == 7);
	CHECK(bank.frames[0].mask[0] == 0 && bank.frames[0].mask[1] == 1 &&
	    bank.frames[0].mask[3] == 0 && bank.frames[0].mask[4] == 1);
	CHECK(bank.frames[1].pixels[0] == 9);
	bank_free(&bank);

	/* A run wrapping its row, or a missing end marker, is rejected. */
	put16(buf + 12, 3);
	CHECK(bank_decode_runs(buf, n, &bank) == -1);
	put16(buf + 12, 2);
	CHECK(bank_decode_runs(buf, 41, &bank) == -1);
	/* An offset outside the file is rejected. */
	put16(buf + 2, 200);
	CHECK(bank_decode_runs(buf, n, &bank) == -1);
	put16(buf + 2, 30);

	/* GRP: one u32 offset to the bank above. */
	memset(buf + 100, 0, 4);
	buf[100] = 4;
	memcpy(buf + 104, buf, n);
	CHECK(group_decode(buf + 100, 4 + n, &grp) == 0);
	CHECK(grp.count == 1 && grp.banks[0].count == 2);
	group_free(&grp);

	/* Raw frame: 2x2 pixels then 4 trailing bytes. */
	memset(buf, 0, sizeof(buf));
	put16(buf, 2);
	put16(buf + 2, 2);
	put16(buf + 4, 2);
	put16(buf + 6, 0xFE);
	memcpy(buf + 8, "\1\2\3\4", 4);
	CHECK(bank_decode_raw(buf, 16, &bank) == 0);
	CHECK(bank.count == 1 && bank.frames[0].mask == NULL &&
	    bank.frames[0].pixels[3] == 4 && bank.frames[0].key == 0xFE);
	bank_free(&bank);
	CHECK(bank_decode_raw(buf, 11, &bank) == -1);
}

static void
test_img(void)
{
	/* 3x2 RLE: a zero-length run, row 0 = 7 7 then a run of 2 x 8
	 * overflowing the row by one pixel, row 1 = literal 1 2 3. */
	static const uint8_t rle[] = {
		3, 0, 2, 0, 1, 0,
		0x80, 0x02, 7, 0x02, 8, 0x83, 1, 2, 3,
	};
	static const uint8_t want[] = { 7, 7, 8, 1, 2, 3 };
	static const uint8_t raw[] = { 2, 0, 1, 0, 0, 0, 4, 5 };
	struct image img;

	CHECK(img_decode(rle, sizeof(rle), &img) == 0);
	CHECK(img.w == 3 && img.h == 2 &&
	    memcmp(img.pixels, want, sizeof(want)) == 0);
	image_free(&img);
	CHECK(img_decode(rle, sizeof(rle) - 1, &img) == -1);
	CHECK(img_decode(raw, sizeof(raw), &img) == 0);
	CHECK(img.w == 2 && img.pixels[1] == 5 && !img.has_palette);
	image_free(&img);
	CHECK(img_decode(raw, sizeof(raw) - 1, &img) == -1);
}

#define IMB_LEN	(0x318 + 2 * (10 + 4 + 2))

/* IMB tileset: two 2x2 tiles of colours 1 and 2, colour 1 red. */
static void
make_imb(uint8_t *buf)
{
	size_t off;
	int i;

	memset(buf, 0, IMB_LEN);
	put16(buf, 3);
	put16(buf + 2, 1);
	put16(buf + 6, 2);		/* tile count */
	buf[0x09 + 3] = 63;		/* colour 1 red = full */
	for (off = 0x318, i = 0; i < 2; i++, off += 16) {
		put16(buf + off, 4 + 6);
		put16(buf + off + 2, 2);
		put16(buf + off + 4, 2);
		put16(buf + off + 6, 1);
		put16(buf + off + 8, 1);
		memset(buf + off + 10, i + 1, 4);
	}
}

static void
test_tileset(void)
{
	static uint8_t buf[IMB_LEN];
	struct tileset ts;

	make_imb(buf);
	CHECK(tileset_decode(buf, sizeof(buf), &ts) == 0);
	CHECK(ts.count == 2 && ts.tile_w == 2 && ts.pixels[4] == 2);
	CHECK(ts.has_palette && ts.palette[3] == 255 && ts.palette[0] == 0);
	tileset_free(&ts);
	CHECK(tileset_decode(buf, sizeof(buf) - 1, &ts) == -1);
	put16(buf + 0x318 + 16 + 2, 3);	/* second tile of another size */
	CHECK(tileset_decode(buf, sizeof(buf), &ts) == -1);
}

static void
test_font(void)
{
	static uint8_t buf[144 * 8];
	struct font font;

	/* Text font with 1-row glyphs: entry = ink, advance, 4 bytes. */
	memset(buf, 0, sizeof(buf));
	buf[0] = 0x01;
	buf[1] = 0xFF;
	buf[8] = 3;			/* '!' ink */
	buf[10] = 4;			/* '!' advance */
	buf[13] = 0x80;			/* pixel at column 8 */
	CHECK(font_decode(buf, sizeof(buf), &font) == 0);
	CHECK(font.count == 143 && font.first == 0x21 && font.height == 1);
	CHECK(font.glyphs[0].ink == 3 && font.glyphs[0].advance == 4);
	CHECK(font.glyphs[0].advance_sub == 4 * FONT_SUB);
	CHECK(font_pixel(&font, 0, 8, 0) == 1 && font_pixel(&font, 0, 7, 0) == 0);
	CHECK(font_pixel(&font, 0, 99, 0) == 0);
	font_free(&font);
	CHECK(font_decode(buf, sizeof(buf) - 1, &font) == -1);
	buf[0] = 0;
	CHECK(font_decode(buf, sizeof(buf), &font) == -1);
}

/* The open text format: absent entries empty, '\n' read as '\r'. */
static void
test_text_json(void)
{
	static const char cstr[] =
	    "{\"strings\": {\"2\": \"~One\\n~Two\", \"0\": \"C\\u00f4te %s\"}}";
	static const char cstn[] =
	    "{\"groups\": {\"1\": [\"a\", \"\\u00df\"], \"3\": []}}";
	static const char bad_word[] = "{\"groups\": {\"0\": [1]}}";
	static const char *const bad[] = {
		"{\"strings\": {\"01\": \"x\"}}",	/* leading zero */
		"{\"strings\": {\"1024\": \"x\"}}",	/* out of range */
		"{\"strings\": {\"x\": \"x\"}}",
		"{\"strings\": {\"1\": 3}}",
		"{\"strings\": {\"1\": \"a\", \"1\": \"b\"}}",
		"{\"strings\": []}",
		"{\"words\": {}}",
		"{\"strings\": {",
		NULL
	};
	struct strtab tab;
	struct strgroups g;
	struct jbuf b = { 0 };
	char err[128];
	size_t i;

	CHECK(cstr_json_decode((const uint8_t *)cstr, strlen(cstr), &tab, err,
	    sizeof(err)) == 0);
	CHECK(tab.count == 3 && strcmp(tab.str[0], "C\xc3\xb4te %s") == 0 &&
	    strcmp(tab.str[1], "") == 0 &&
	    strcmp(tab.str[2], "~One\r~Two") == 0);
	/* Written back with "\n", it reads the same. */
	cstr_json_encode(&tab, &b);
	CHECK(b.s != NULL && strstr(b.s, "\"~One\\n~Two\"") != NULL);
	strtab_free(&tab);
	CHECK(b.s != NULL && cstr_json_decode((const uint8_t *)b.s, b.len,
	    &tab, err, sizeof(err)) == 0 && tab.count == 3 &&
	    strcmp(tab.str[2], "~One\r~Two") == 0);
	strtab_free(&tab);
	jb_free(&b);
	for (i = 0; bad[i] != NULL; i++) {
		err[0] = '\0';
		CHECK(cstr_json_decode((const uint8_t *)bad[i], strlen(bad[i]),
		    &tab, err, sizeof(err)) == -1 && err[0] != '\0');
	}

	CHECK(cstn_json_decode((const uint8_t *)cstn, strlen(cstn), &g, err,
	    sizeof(err)) == 0);
	CHECK(g.count == 4 && g.groups[0].count == 0 &&
	    g.groups[1].count == 2 && g.groups[3].count == 0 &&
	    strcmp(g.groups[1].str[1], "\xc3\x9f") == 0);
	strgroups_free(&g);
	CHECK(cstn_json_decode((const uint8_t *)bad_word, strlen(bad_word),
	    &g, err, sizeof(err)) == -1);
}

/* The credits as text: blocks, their marks left out, the errors. */
static void
test_credits(void)
{
	static const char good[] = "{\"ink\": \"#10a0Ff\", \"blocks\": ["
	    "{\"heading\": \"~Thanks\", \"lines\": [\"`A\", \"B ^C\"]},"
	    "{\"lines\": [\"D\"], \"needs\": \"music/title\"},"
	    "{\"heading\": \"E\"}]}";
	static const char one[] = "{\"blocks\": [{\"heading\": \"x\"}]}";
	static const char *const bad[] = {
		"{\"blocks\": []}",
		"{\"block\": [{\"heading\": \"x\"}]}",
		"{\"blocks\": [{}]}",				/* empty */
		"{\"blocks\": [{\"heading\": \"\", \"lines\": []}]}",
		"{\"blocks\": [{\"heading\": 1}]}",
		"{\"blocks\": [{\"heading\": \"x\", \"needs\": 1}]}",
		"{\"blocks\": [{\"lines\": [\"a\\nb\"]}]}",	/* a break */
		"{\"blocks\": [{\"lines\": [2]}]}",
		"{\"blocks\": [\"x\"]}",
		"{\"blocks\": [{\"heading\": \"x\"}], \"ink\": \"#12345\"}",
		"{\"blocks\": [{\"heading\": \"x\"}], \"ink\": \"#12345g\"}",
		"{\"blocks\": [{\"lines\": [\"1\", \"2\", \"3\", \"4\", \"5\", "
		    "\"6\", \"7\", \"8\", \"9\", \"10\", \"11\", \"12\", "
		    "\"13\"]}]}",
		"{\"blocks\": [",
		NULL
	};
	struct credits c;
	struct jbuf b = { 0 };
	char err[128];
	size_t i;

	CHECK(credits_decode((const uint8_t *)good, strlen(good), &c, err,
	    sizeof(err)) == 0);
	CHECK(c.nblocks == 3 && c.ink[0] == 0x10 && c.ink[1] == 0xa0 &&
	    c.ink[2] == 0xff);
	CHECK(c.nblocks == 3 && strcmp(c.blocks[0].heading, "Thanks") == 0 &&
	    c.blocks[0].nlines == 2 && strcmp(c.blocks[0].lines[0], "A") == 0 &&
	    strcmp(c.blocks[0].lines[1], "B ^C") == 0 &&
	    strcmp(c.blocks[1].heading, "") == 0 && c.blocks[1].nlines == 1 &&
	    c.blocks[2].nlines == 0);
	CHECK(c.blocks[0].needs == NULL &&
	    strcmp(c.blocks[1].needs, "music/title") == 0);
	credits_drop(&c, 1);
	CHECK(c.nblocks == 2 && strcmp(c.blocks[1].heading, "E") == 0);
	credits_free(&c);
	CHECK(c.nblocks == 0 && c.blocks == NULL);
	CHECK(credits_decode((const uint8_t *)one, strlen(one), &c, err,
	    sizeof(err)) == 0 && c.ink[0] == 0 &&
	    c.ink[1] == 0 && c.ink[2] == 0);
	credits_free(&c);
	/* 16 blocks at most. */
	jb_raw(&b, "{\"blocks\": [");
	for (i = 0; i < CREDITS_MAX_BLOCKS + 1; i++)
		jb_raw(&b, i == 0 ? "{\"heading\": \"x\"}" :
		    ", {\"heading\": \"x\"}");
	jb_raw(&b, "]}");
	CHECK(b.s != NULL && credits_decode((const uint8_t *)b.s, b.len, &c,
	    err, sizeof(err)) == -1);
	jb_free(&b);
	for (i = 0; bad[i] != NULL; i++) {
		err[0] = '\0';
		CHECK(credits_decode((const uint8_t *)bad[i], strlen(bad[i]),
		    &c, err, sizeof(err)) == -1 && err[0] != '\0' &&
		    c.nblocks == 0);
	}
}

static void
test_text(void)
{
	/* CSTR: 3 entries, the second empty, the last unterminated. */
	static const uint8_t cstr[] = {
		6, 0, 11, 0, 11, 0,
		'C', 0x8E, 't', 'e', 0,
		'~', 'Y', 'a',
	};
	/* CSTN: 2 groups; group 0 declares 1 string but holds 2. */
	static const uint8_t cstn[] = {
		4, 0, 10, 0,
		1, 0, 'a', 0, 0xA7, 0,
		0, 0,
	};
	struct strtab tab;
	struct strgroups g;
	char *u;

	CHECK(cstr_decode(cstr, sizeof(cstr), &tab) == 0);
	CHECK(tab.count == 3);
	CHECK(strcmp(tab.str[0], "C\xc3\xa9te") == 0);	/* é */
	CHECK(strcmp(tab.str[1], "") == 0 && strcmp(tab.str[2], "~Ya") == 0);
	strtab_free(&tab);

	CHECK(cstn_decode(cstn, sizeof(cstn), &g) == 0);
	CHECK(g.count == 2 && g.declared[0] == 1 && g.groups[0].count == 2);
	CHECK(strcmp(g.groups[0].str[1], "\xc3\x9f") == 0);	/* ß */
	CHECK(g.groups[1].count == 0);
	strgroups_free(&g);

	/* Offsets going backwards are rejected. */
	CHECK(cstr_decode((const uint8_t *)"\4\0\2\0", 4, &tab) == -1);

	test_text_json();
	test_credits();

	u = macroman_to_utf8((const uint8_t *)"\xd1\xca", 2);
	CHECK(u != NULL && strcmp(u, "\xe2\x80\x94\xc2\xa0") == 0);
	free(u);
}

static void
test_sha256(void)
{
	static uint8_t big[1000];
	char hex[65];

	sha256_hex((const uint8_t *)"", 0, hex);
	CHECK(strcmp(hex, "e3b0c44298fc1c149afbf4c8996fb924"
	    "27ae41e4649b934ca495991b7852b855") == 0);
	sha256_hex((const uint8_t *)"abc", 3, hex);
	CHECK(strcmp(hex, "ba7816bf8f01cfea414140de5dae2223"
	    "b00361a396177a9cb410ff61f20015ad") == 0);
	/* 56 bytes: the length no longer fits in the first padding block. */
	sha256_hex((const uint8_t *)
	    "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56,
	    hex);
	CHECK(strcmp(hex, "248d6a61d20638b8e5c026930c3e6039"
	    "a33ce45964ff2167f6ecedd419db06c1") == 0);
	memset(big, 'a', sizeof(big));
	sha256_hex(big, sizeof(big), hex);
	CHECK(strcmp(hex, "41edece42d63e8d9bf515a9ba6932e1c"
	    "20cbc9f5a5d134645adb5db1b9737ea3") == 0);
	CHECK(exe_identify(big, sizeof(big)) == NULL);
}

static void
test_towns(void)
{
	static uint8_t exe[16 + 0xF24 + 0x780];
	struct exe_layout layout;
	struct era_towns era;
	uint8_t *r;
	int i;

	memset(&layout, 0, sizeof(layout));
	layout.image_offset = 16;
	for (i = 0; i < EXE_ERAS; i++)
		layout.town_va[i] = 0;
	memset(exe, 0, sizeof(exe));
	put16(exe + 16 + 0x20, 2);	/* last record index: 3 records */
	r = exe + 16 + 36;
	put16(r, TOWN_INLAND);
	put16(r + 2, 245);
	put16(r + 4, 179);
	put16(r + 8, 4);		/* forts */
	put16(r + 16, 195);		/* wealth */
	put16(r + 28, 6);		/* id */
	memcpy(r + 64, "Havana", 6);
	memcpy(r + 80 + 64, "Channel A", 9);
	memcpy(r + 160 + 64, "Channel B", 9);
	/* 1660's arrival list, after the block: two cells, then the end. */
	layout.arrival_va[4] = 0xF24;
	r = exe + 16 + 0xF24;
	put16(r, 245);
	put16(r + 2, 180);
	put16(r + 6, 246);
	put16(r + 8, 179);
	put16(r + 12, 0xFFFF);
	r = exe + 16 + 36;
	CHECK(towns_load(&layout, exe, sizeof(exe), 4, &era) == 0);
	CHECK(era.narrival == 2 && era.arrival[0].y == 180 &&
	    era.arrival[1].x == 246 && era.arrival[1].town == 0);
	/* The nearest: the first of two cells at the same distance. */
	CHECK(town_cells_nearest(era.arrival, era.narrival, 246, 180) == 0);
	era.arrival[1].town = 1;
	CHECK(town_cells_nearest(era.arrival, era.narrival, 246, 180) == 0);
	CHECK(town_cells_nearest(era.arrival, era.narrival, 247, 179) == 1);
	CHECK(town_cells_nearest(era.arrival, 0, 247, 179) == -1);
	CHECK(era.year == 1660 && era.count == 1);
	CHECK(strcmp(era.towns[0].name, "Havana") == 0);
	CHECK(era.towns[0].x == 245 && era.towns[0].y == 179 &&
	    era.towns[0].forts == 4 && era.towns[0].wealth == 195 &&
	    era.towns[0].id == 6 && (era.towns[0].flags & TOWN_INLAND));
	CHECK(strcmp(era.channel[1].name, "Channel B") == 0);

	/* A name filling all 16 bytes stays terminated. */
	memcpy(r + 64, "ABCDEFGHIJKLMNOP", 16);
	CHECK(towns_load(&layout, exe, sizeof(exe), 0, &era) == 0);
	CHECK(strlen(era.towns[0].name) == 16);
	/* No list for 1560: the town's cell and the eight around. */
	CHECK(era.narrival == 9 && era.arrival[4].x == 245 &&
	    era.arrival[4].y == 179 && era.arrival[8].town == 0);
	/* A cell for a town past the count, or no end, is refused. */
	put16(exe + 16 + 0xF24 + 4, 1);
	CHECK(towns_load(&layout, exe, sizeof(exe), 4, &era) == -1);
	put16(exe + 16 + 0xF24 + 4, 0);
	CHECK(towns_load(&layout, exe, sizeof(exe) - 1, 4, &era) == -1);

	CHECK(towns_load(&layout, exe, 16 + 0xF24 - 1, 0, &era) == -1);
	CHECK(towns_load(&layout, exe, sizeof(exe), 6, &era) == -1);
	put16(r + 6, 7);		/* no such nation */
	CHECK(towns_load(&layout, exe, sizeof(exe), 0, &era) == -1);
	put16(r + 6, 0);
	put16(exe + 16 + 0x20, 48);	/* 49 records do not fit */
	CHECK(towns_load(&layout, exe, sizeof(exe), 0, &era) == -1);
}

/* 4x2 RGBA: a 2x2 white block, a column of half-transparent red, then a
 * clear one. */
static const uint8_t png_4x2a[] = {
	0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
	0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x02,
	0x08, 0x06, 0x00, 0x00, 0x00, 0x7f, 0xa8, 0x7d, 0x63, 0x00, 0x00, 0x00,
	0x13, 0x49, 0x44, 0x41, 0x54, 0x78, 0xda, 0x63, 0xf8, 0x0f, 0x03, 0x0c,
	0x0c, 0x0d, 0x0c, 0x20, 0x80, 0x2e, 0x00, 0x00, 0x7d, 0x40, 0x12, 0xef,
	0x8a, 0xc8, 0xf4, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44,
	0xae, 0x42, 0x60, 0x82,
};

/* Raw bank: one 2x2 frame, key 0xFE. */
static size_t
make_raw_bank(uint8_t *buf)
{
	memset(buf, 0, 16);
	put16(buf, 2);
	put16(buf + 2, 2);
	put16(buf + 4, 2);
	put16(buf + 6, 0xFE);
	memcpy(buf + 8, "\1\2\3\4", 4);
	return (16);
}

/* Write "text" as the sheet "name" of pack "dir" and try to load it. */
static int
try_sheet(const char *dir, const char *text)
{
	struct packs *packs = packs_new();
	struct sprite_bank bank;
	int rc;

	if (write_text(dir, "sprites/flag.sheet", text) != 0 ||
	    packs_add_dir(packs, dir) != 0) {
		packs_free(packs);
		return (-2);
	}
	rc = bank_load(packs, "spr/flag", BANK_RUNS, &bank);
	bank_free(&bank);
	packs_free(packs);
	return (rc);
}

/*
 * Sprite sheets: frames cut from a PNG, sized by the pack's art height,
 * indices from the original bank when there, else from the art.
 */
static void
test_sheets(void)
{
	static const char *const low_files[] = { "pack.ini", "boat.cur", NULL };
	static const char *const high_files[] = { "pack.ini", "boat.sheet",
	    "atlas.png", "fight.sheet", "sprites/flag.sheet",
	    "sprites/flag.png", NULL };
	static const char ini[] = "[pack]\nart_height = 960\n[assets]\n"
	    "cur/boat = boat.sheet\nspr/flag = sprites/flag.sheet\n"
	    "grp/fight = fight.sheet\n";
	static const char flag[] = "[sheet]\nimage = flag.png\n\n[frames]\n"
	    "# frame = x y w h\n1 = 0 0 2 2\n0 = 0 0 4 2\n";
	static struct fb fb;
	char low[] = "/tmp/flibustier-test-XXXXXX";
	char high[] = "/tmp/flibustier-test-XXXXXX";
	char path[1024];
	uint8_t raw[16], *rgb;
	struct packs *packs;
	struct sprite_bank bank;
	struct sprite_group grp;
	const struct frame *f;
	const uint8_t *p;

	if (fsys_mkdtemp(low) == NULL || fsys_mkdtemp(high) == NULL) {
		CHECK(!"mkdtemp");
		return;
	}
	snprintf(path, sizeof(path), "%s/sprites", high);
	CHECK(fsys_mkdir(path) == 0);
	CHECK(write_text(low, "pack.ini", "[assets]\ncur/boat = boat.cur\n")
	    == 0);
	CHECK(write_bytes(low, "boat.cur", raw, make_raw_bank(raw)) == 0);
	CHECK(write_text(high, "pack.ini", ini) == 0);
	CHECK(write_bytes(high, "atlas.png", png_6x4, sizeof(png_6x4)) == 0);
	CHECK(write_text(high, "boat.sheet", "[sheet]\nimage = atlas.png\n"
	    "[frames]\n0 = 0 0 4 4\n") == 0);
	CHECK(write_text(high, "fight.sheet", "[sheet]\nimage = atlas.png\n"
	    "[frames]\n0.0 = 0 0 2 2\n1.0 = 2 0 2 2\n1.1 = 0 0 4 2\n") == 0);
	CHECK(write_bytes(high, "sprites/flag.png", png_4x2a,
	    sizeof(png_4x2a)) == 0);
	CHECK(write_text(high, "sprites/flag.sheet", flag) == 0);

	/* Over the original: its indices and key, the sheet's colours. */
	packs = packs_new();
	CHECK(packs_add_dir(packs, low) == 0);
	CHECK(packs_add_dir(packs, high) == 0);
	CHECK(bank_load(packs, "cur/boat", BANK_RAW, &bank) == 0);
	CHECK(bank.count == 1);
	if (bank.count == 1) {
		f = &bank.frames[0];
		CHECK(f->w == 2 && f->h == 2 && f->key == 0xFE);
		CHECK(memcmp(f->pixels, "\1\2\3\4", 4) == 0);
		CHECK(f->rgba != NULL && f->rgba_w == 4 && f->rgba_h == 4);
	}
	bank_free(&bank);

	/* Alone, from a subdirectory: alpha gives the mask. */
	CHECK(bank_load(packs, "spr/flag", BANK_RUNS, &bank) == 0);
	CHECK(bank.count == 2);
	if (bank.count == 2) {
		f = &bank.frames[0];
		CHECK(f->w == 2 && f->h == 1);
		CHECK(f->mask != NULL && f->mask[0] == 1 && f->mask[1] == 0);
		CHECK(f->pixels[0] == 215 && f->pixels[1] == 0);
		CHECK(bank.frames[1].w == 1 && bank.frames[1].h == 1);
	}

	/* Drawn at 2x: opaque art copied, half alpha blended, clear kept. */
	if (bank.count == 2 && fb_init(&fb, 960) == 0 &&
	    (rgb = malloc((size_t)1280 * 960 * 3)) != NULL) {
		palette_fixed(fb.pal);
		fb_clear(&fb, 9);
		fb_frame(&fb, &bank.frames[0], 1, 1);
		CHECK(fb.px[FB_W + 1] == 215 && fb.px[FB_W + 2] == 9);
		fb_to_rgb(&fb, 256, rgb);
		p = at(rgb, 1280, 2, 2);
		CHECK(p[0] == 255 && p[1] == 255 && p[2] == 255);
		p = at(rgb, 1280, 4, 3);
		CHECK(p[0] == 128 && p[1] == 25 && p[2] == 76);
		p = at(rgb, 1280, 5, 2);
		CHECK(p[0] == 0 && p[1] == 51 && p[2] == 153);
		/* Clipped to 640x480 column 1: canvas columns 2 and 3. */
		fb_clear(&fb, 9);
		fb_frame_clip(&fb, &bank.frames[0], 0, 0, 1, 0, 1, 0);
		fb_to_rgb(&fb, 256, rgb);
		CHECK(at(rgb, 1280, 1, 0)[1] == 51 && at(rgb, 1280, 2, 0)[1] ==
		    25 && at(rgb, 1280, 3, 0)[1] == 51);
		free(rgb);
	}
	fb_free(&fb);
	bank_free(&bank);

	/* An offset, at half the art height: moved by half its art pixels,
	 * either way; the original's indices are not used with it. */
	CHECK(write_text(high, "boat.sheet", "[sheet]\nimage = atlas.png\n"
	    "[frames]\n0 = 0 0 4 4 -2 3\n") == 0);
	CHECK(bank_load(packs, "cur/boat", BANK_RAW, &bank) == 0);
	if (bank.count == 1) {
		f = &bank.frames[0];
		CHECK(f->ox == -1 && f->oy == 2 && f->w == 2 && f->h == 2);
		if (fb_init(&fb, 960) == 0) {
			fb_clear(&fb, 9);
			fb_frame(&fb, f, 3, 3);
			CHECK(fb.px[5 * FB_W + 2] == f->pixels[0] &&
			    fb.px[5 * FB_W + 1] == 9 &&
			    fb.px[4 * FB_W + 2] == 9);
		}
		fb_free(&fb);
	}
	bank_free(&bank);
	CHECK(write_text(high, "boat.sheet", "[sheet]\nimage = atlas.png\n"
	    "[frames]\n0 = 0 0 4 4\n") == 0);

	/* Groups: "bank.frame". */
	CHECK(group_load(packs, "grp/fight", &grp) == 0);
	CHECK(grp.count == 2 && grp.banks[0].count == 1 &&
	    grp.banks[1].count == 2);
	if (grp.count == 2 && grp.banks[1].count == 2)
		CHECK(grp.banks[1].frames[1].w == 2);
	group_free(&grp);
	packs_free(packs);

	/* Gaps, rectangles past the image, bad lines, no image. */
	CHECK(try_sheet(high, flag) == 0);
	CHECK(try_sheet(high, "[sheet]\nimage = flag.png\n[frames]\n"
	    "0 = 0 0 1 1\n2 = 0 0 1 1\n") == -1);
	CHECK(try_sheet(high, "[sheet]\nimage = flag.png\n[frames]\n"
	    "0 = 0 0 5 2\n") == -1);
	CHECK(try_sheet(high, "[sheet]\nimage = flag.png\n[frames]\n"
	    "0 = 0 0 2\n") == -1);
	CHECK(try_sheet(high, "[sheet]\nimage = flag.png\n[frames]\n"
	    "0 = 0 0 0 2\n") == -1);
	CHECK(try_sheet(high, "[sheet]\nimage = flag.png\n[frames]\n"
	    "0 = 0 0 1 1 -1\n") == -1);
	CHECK(try_sheet(high, "[sheet]\nimage = flag.png\n[frames]\n"
	    "0 = 0 0 1 1 1 x\n") == -1);
	CHECK(try_sheet(high, "[sheet]\nimage = ../flag.png\n[frames]\n"
	    "0 = 0 0 1 1\n") == -1);
	CHECK(try_sheet(high, "[frames]\n0 = 0 0 1 1\n") == -1);

	remove_files(low, low_files);
	snprintf(path, sizeof(path), "%s/sprites", high);
	remove_files(high, high_files);
	rmdir(path);
	rmdir(high);
}

/* A TrueType font of the system for the font tests, or NULL. */
static const char *
test_font_file(void)
{
	static const char *const paths[] = {
		"/usr/local/share/fonts/dejavu/DejaVuSans.ttf",
		"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
		"/usr/share/fonts/TTF/DejaVuSans.ttf",
		"/usr/share/fonts/dejavu/DejaVuSans.ttf",
		"/Library/Fonts/Arial Unicode.ttf",
		"C:/Windows/Fonts/arial.ttf",
	};
	const char *env = getenv("FLIBUSTIER_TEST_FONT");
	size_t i;

	if (env != NULL && access(env, R_OK) == 0)
		return (env);
	for (i = 0; i < sizeof(paths) / sizeof(paths[0]); i++)
		if (access(paths[i], R_OK) == 0)
			return (paths[i]);
	return (NULL);
}

static int
copy_file(const char *from, const char *dir, const char *name)
{
	FILE *in, *out;
	char path[1024], buf[8192];
	size_t n;
	int rc = 0;

	snprintf(path, sizeof(path), "%s/%s", dir, name);
	if ((in = fopen(from, "rb")) == NULL)
		return (-1);
	if ((out = fopen(path, "wb")) == NULL) {
		fclose(in);
		return (-1);
	}
	while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
		if (fwrite(buf, 1, n, out) != n)
			rc = -1;
	fclose(in);
	return (fclose(out) != 0 ? -1 : rc);
}

/* Load font/pirate1 from "dir" after writing "desc" as its descriptor. */
static int
try_font(const char *dir, const char *desc)
{
	struct packs *packs = packs_new();
	struct font f;
	int rc;

	if (write_text(dir, "f.font", desc) != 0 ||
	    packs_add_dir(packs, dir) != 0) {
		packs_free(packs);
		return (-2);
	}
	rc = font_load(packs, "font/pirate1", 960, &f);
	font_free(&f);
	packs_free(packs);
	return (rc);
}

/*
 * Replacement fonts: 640x480 metrics with fractional advances, 1-bit
 * glyphs for the screen, coverage glyphs for the canvas.
 */
static void
test_fonts(void)
{
	static const char *const files[] = { "pack.ini", "f.font", "f.ttf",
	    "caps.font", NULL };
	static const char good[] = "[font]\nfile = f.ttf\nheight = 25\n"
	    "size = 22.5\nbaseline = 19\n";
	static struct fb fb;
	const char *ttf = test_font_file();
	char dir[] = "/tmp/flibustier-test-XXXXXX";
	struct packs *packs;
	struct font f, caps;
	const struct glyph *a;
	uint8_t *rgb;
	size_t i, n;
	int x, y, inked;

	if (ttf == NULL) {
		printf("font tests skipped: no TrueType font found "
		    "(set FLIBUSTIER_TEST_FONT)\n");
		return;
	}
	if (fsys_mkdtemp(dir) == NULL) {
		CHECK(!"mkdtemp");
		return;
	}
	CHECK(copy_file(ttf, dir, "f.ttf") == 0);
	CHECK(write_text(dir, "pack.ini", "[pack]\nart_height = 960\n"
	    "[assets]\nfont/pirate1 = f.font\nfont/caps = caps.font\n") == 0);
	CHECK(write_text(dir, "f.font", good) == 0);
	CHECK(write_text(dir, "caps.font", "[font]\nfile = f.ttf\n"
	    "height = 34\n") == 0);
	packs = packs_new();
	CHECK(packs_add_dir(packs, dir) == 0);

	CHECK(font_load(packs, "font/pirate1", 960, &f) == 0);
	CHECK(f.height == 25 && f.first == 0x21 && f.count == 0xFF - 0x21 + 1);
	CHECK(f.space_sub > 0 && f.hd != NULL && f.hd_height == 960);
	if (f.count > 'A' - 0x21) {
		a = &f.glyphs['A' - 0x21];
		CHECK(a->advance_sub > 0 &&
		    a->advance == (a->advance_sub + FONT_SUB / 2) / FONT_SUB);
		CHECK(f.hd['A' - 0x21].cov != NULL);
		/* "A" stands on the baseline, 19 pixels down. */
		inked = 0;
		for (y = 0; y < f.height; y++)
			for (x = 0; x < f.cell_w; x++)
				if (font_pixel(&f, 'A' - 0x21, x, y)) {
					inked++;
					CHECK(y < 19);
				}
		CHECK(inked > 20);
		/* Widths keep fractions: ten letters are ten advances. */
		CHECK(text_width(&f, "AAAAAAAAAA") ==
		    (10 * a->advance_sub + FONT_SUB / 2) / FONT_SUB);
	}

	/* On a 2x canvas: ink on the screen and blended on the canvas. */
	if (fb_init(&fb, 960) == 0 &&
	    (rgb = malloc((size_t)1280 * 960 * 3)) != NULL) {
		palette_fixed(fb.pal);
		fb_clear(&fb, 215);		/* white */
		fb_glyph(&fb, &f, 'I' - 0x21, 100 * FONT_SUB, 100, 0);
		inked = 0;
		for (i = 0; i < sizeof(fb.px); i++)
			inked += fb.px[i] == 0;
		CHECK(inked > 10);
		fb_to_rgb(&fb, 256, rgb);
		n = 0;
		inked = 0;
		for (i = 0; i < (size_t)1280 * 960; i++) {
			if (rgb[3 * i] == 0)
				inked++;
			else if (rgb[3 * i] < 255)
				n++;		/* antialiased edge */
		}
		CHECK(inked > 40 && n > 0);
		free(rgb);
	}
	fb_free(&fb);
	font_free(&f);

	/* Drop capitals, in caps_char() order. */
	CHECK(font_load(packs, "font/caps", 960, &caps) == 0);
	CHECK(caps.count == CAPS_COUNT && caps.height == 34 &&
	    caps.glyphs[0].ink > 0);
	CHECK(caps_char(0) == 'A' && caps_index('a') == 0 &&
	    caps_index(caps_char(30)) == 30 && caps_char(CAPS_COUNT) == -1);
	font_free(&caps);
	packs_free(packs);

	CHECK(try_font(dir, good) == 0);
	CHECK(try_font(dir, "[font]\nheight = 25\n") == -1);
	CHECK(try_font(dir, "[font]\nfile = f.ttf\n") == -1);
	CHECK(try_font(dir, "[font]\nfile = f.ttf\nheight = 2.5\n") == -1);
	CHECK(try_font(dir, "[font]\nfile = f.ttf\nheight = 25\nsize = x\n")
	    == -1);
	CHECK(try_font(dir, "[font]\nfile = none.ttf\nheight = 25\n") == -1);
	CHECK(try_font(dir, "[font]\nfile = pack.ini\nheight = 25\n") == -1);
	remove_files(dir, files);
}

/*
 * 8x16 RGBA tile atlas: 4x4 tiles, two per row.  Frame 0 (rows 0-7):
 * red, green, blue, white; frame 1 (rows 8-15): all black.
 */
static const uint8_t png_tiles[] = {
	0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
	0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x10,
	0x08, 0x06, 0x00, 0x00, 0x00, 0x2b, 0x8a, 0x3e, 0x7d, 0x00, 0x00, 0x00,
	0x20, 0x49, 0x44, 0x41, 0x54, 0x78, 0xda, 0x63, 0xf8, 0xcf, 0xc0, 0xf0,
	0x1f, 0x19, 0x63, 0x81, 0x34, 0x57, 0x80, 0x26, 0x8f, 0x01, 0xe8, 0xa3,
	0x00, 0xe2, 0x10, 0x3c, 0x78, 0x24, 0x28, 0x00, 0x00, 0xb3, 0x23, 0xdf,
	0x21, 0x1e, 0xbc, 0xcb, 0x86, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e,
	0x44, 0xae, 0x42, 0x60, 0x82,
};

/* Load tiles/sea from "dir" after writing "desc" as its sheet. */
static int
try_tiles(const char *dir, const char *desc)
{
	struct packs *packs = packs_new();
	struct tileset ts;
	int rc;

	if (write_text(dir, "sea.tiles", desc) != 0 ||
	    packs_add_dir(packs, dir) != 0) {
		packs_free(packs);
		return (-2);
	}
	rc = tileset_load(packs, "tiles/sea", &ts);
	tileset_free(&ts);
	packs_free(packs);
	return (rc);
}

/*
 * Tile sheets: tiles in rows, animation frames below each other, indices
 * from the original tileset when there, else from the first frame.
 */
static void
test_tiles(void)
{
	static const char *const low_files[] = { "pack.ini", "pair.imb", NULL };
	static const char *const high_files[] = { "pack.ini", "atlas.png",
	    "sea.tiles", "pair.tiles", NULL };
	static const char sea[] = "[tiles]\nimage = atlas.png\ntile = 4\n"
	    "count = 4\nframes = 2\nperiod = 4\n";
	static uint8_t imb[IMB_LEN];
	static struct fb fb;
	char low[] = "/tmp/flibustier-test-XXXXXX";
	char high[] = "/tmp/flibustier-test-XXXXXX";
	struct packs *packs;
	struct tileset ts;
	struct image t;
	uint8_t *rgb;
	const uint8_t *p;

	if (fsys_mkdtemp(low) == NULL || fsys_mkdtemp(high) == NULL) {
		CHECK(!"mkdtemp");
		return;
	}
	make_imb(imb);
	CHECK(write_text(low, "pack.ini", "[assets]\ntiles/pair = pair.imb\n")
	    == 0);
	CHECK(write_bytes(low, "pair.imb", imb, sizeof(imb)) == 0);
	CHECK(write_text(high, "pack.ini", "[pack]\nart_height = 960\n"
	    "[assets]\ntiles/sea = sea.tiles\ntiles/pair = pair.tiles\n") == 0);
	CHECK(write_bytes(high, "atlas.png", png_tiles, sizeof(png_tiles)) ==
	    0);
	CHECK(write_text(high, "sea.tiles", sea) == 0);
	CHECK(write_text(high, "pair.tiles", "[tiles]\nimage = atlas.png\n"
	    "tile = 4\ncount = 2\n") == 0);

	/* Over the original: its indices and palette. */
	packs = packs_new();
	CHECK(packs_add_dir(packs, low) == 0);
	CHECK(packs_add_dir(packs, high) == 0);
	CHECK(tileset_load(packs, "tiles/pair", &ts) == 0);
	CHECK(ts.count == 2 && ts.tile_w == 2 && ts.rgba != NULL);
	CHECK(ts.pixels != NULL && ts.pixels[0] == 1 && ts.pixels[4] == 2);
	CHECK(ts.has_palette && ts.palette[3] == 255);
	tileset_free(&ts);

	/* Alone: indices from frame 0; the pass picks the frame. */
	CHECK(tileset_load(packs, "tiles/sea", &ts) == 0);
	CHECK(ts.count == 4 && ts.tile_w == 2 && ts.columns == 2 &&
	    ts.frames == 2);
	CHECK(ts.pixels != NULL && ts.pixels[0] == 180 && ts.pixels[4] == 30 &&
	    ts.pixels[12] == 215);
	tileset_tile(&ts, 1, 3, &t);		/* green, frame 0 */
	CHECK(t.w == 2 && t.pixels == ts.pixels + 4 && t.rgba_w == 4 &&
	    t.rgba_stride == 8);
	CHECK(t.rgba != NULL && t.rgba[0] == 0 && t.rgba[1] == 255);
	tileset_tile(&ts, 1, 4, &t);		/* frame 1: black */
	CHECK(t.rgba != NULL && t.rgba[1] == 0 && t.rgba[3] == 255);
	tileset_tile(&ts, 1, 8, &t);		/* back to frame 0 */
	CHECK(t.rgba != NULL && t.rgba[1] == 255);

	/* Drawn at 2x, a tile's art fills its 4x4 canvas pixels. */
	if (fb_init(&fb, 960) == 0 &&
	    (rgb = malloc((size_t)1280 * 960 * 3)) != NULL) {
		fb_clear(&fb, 0);
		tileset_tile(&ts, 2, 0, &t);	/* blue */
		fb_image(&fb, &t, 2, 0, 0);
		tileset_tile(&ts, 3, 0, &t);	/* white */
		fb_image(&fb, &t, 4, 0, 0);
		fb_to_rgb(&fb, 256, rgb);
		p = at(rgb, 1280, 7, 3);
		CHECK(p[0] == 0 && p[1] == 0 && p[2] == 255);
		p = at(rgb, 1280, 8, 3);
		CHECK(p[0] == 255 && p[1] == 255 && p[2] == 255);
		CHECK(fb.px[2] == 5 && fb.px[4] == 215);
		free(rgb);
	}
	fb_free(&fb);
	tileset_free(&ts);
	packs_free(packs);

	/* Errors: too short for its frames, no tile size, bad numbers. */
	CHECK(try_tiles(high, sea) == 0);
	CHECK(try_tiles(high, "[tiles]\nimage = atlas.png\ntile = 4\n"
	    "count = 4\nframes = 3\n") == -1);
	CHECK(try_tiles(high, "[tiles]\nimage = atlas.png\n") == -1);
	CHECK(try_tiles(high, "[tiles]\nimage = atlas.png\ntile = 9\n") == -1);
	CHECK(try_tiles(high, "[tiles]\nimage = atlas.png\ntile = 4\n"
	    "frames = 0\n") == -1);
	CHECK(try_tiles(high, "[tiles]\nimage = none.png\ntile = 4\n") == -1);

	remove_files(low, low_files);
	remove_files(high, high_files);
}

/*
 * 4x4 terrain masks for the tile atlas (2x2 pixel tiles, two a row):
 * sea; land; shallows; land over sea.  The bad one has a red pixel.
 */
static const uint8_t png_mask[] = {
	0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
	0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04,
	0x08, 0x06, 0x00, 0x00, 0x00, 0xa9, 0xf1, 0x9e, 0x7e, 0x00, 0x00, 0x00,
	0x19, 0x49, 0x44, 0x41, 0x54, 0x78, 0xda, 0x63, 0x60, 0x60, 0xf8, 0xff,
	0x1f, 0x8c, 0x61, 0x10, 0x53, 0xe0, 0x3f, 0x88, 0x83, 0x4d, 0x00, 0xaa,
	0x12, 0x00, 0x78, 0x04, 0x23, 0xdd, 0xe1, 0x14, 0x4c, 0xfe, 0x00, 0x00,
	0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82,
};
static const uint8_t png_mask_bad[] = {
	0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
	0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04,
	0x08, 0x06, 0x00, 0x00, 0x00, 0xa9, 0xf1, 0x9e, 0x7e, 0x00, 0x00, 0x00,
	0x1e, 0x49, 0x44, 0x41, 0x54, 0x78, 0xda, 0x63, 0x60, 0x60, 0xf8, 0xff,
	0x1f, 0x8c, 0x61, 0x10, 0xc4, 0x01, 0x89, 0x20, 0x04, 0x40, 0xfc, 0xff,
	0xff, 0xb1, 0x08, 0x40, 0xb5, 0x02, 0x00, 0x7a, 0x02, 0x23, 0xdd, 0x7d,
	0x76, 0xfa, 0xac, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae,
	0x42, 0x60, 0x82,
};

/* Tile sheets with their own terrain and land battle ground. */
static void
test_tile_terrain(void)
{
	static const char *const files[] = { "pack.ini", "atlas.png",
	    "mask.png", "bad.png", "sea.tiles", NULL };
	static const char head[] = "[tiles]\nimage = atlas.png\ntile = 4\n"
	    "count = 4\n";
	static const uint8_t want[4][4] = {
		{ TERRAIN_SEA, TERRAIN_SEA, TERRAIN_SEA, TERRAIN_SEA },
		{ TERRAIN_LAND, TERRAIN_LAND, TERRAIN_LAND, TERRAIN_LAND },
		{ TERRAIN_SHALLOWS, TERRAIN_SHALLOWS, TERRAIN_SHALLOWS,
		    TERRAIN_SHALLOWS },
		{ TERRAIN_LAND, TERRAIN_LAND, TERRAIN_SEA, TERRAIN_SEA },
	};
	char dir[] = "/tmp/flibustier-test-XXXXXX";
	char text[512];
	struct packs *packs;
	struct tileset ts;

	if (fsys_mkdtemp(dir) == NULL) {
		CHECK(!"mkdtemp");
		return;
	}
	CHECK(write_text(dir, "pack.ini", "[pack]\nart_height = 960\n"
	    "[assets]\ntiles/sea = sea.tiles\n") == 0);
	CHECK(write_bytes(dir, "atlas.png", png_tiles, sizeof(png_tiles)) ==
	    0);
	CHECK(write_bytes(dir, "mask.png", png_mask, sizeof(png_mask)) == 0);
	CHECK(write_bytes(dir, "bad.png", png_mask_bad, sizeof(png_mask_bad))
	    == 0);
	snprintf(text, sizeof(text), "%sterrain = mask.png\n\n[ground]\n"
	    "# tile = kind\n1 = cover\n3 = water\n", head);
	CHECK(write_text(dir, "sea.tiles", text) == 0);

	packs = packs_new();
	CHECK(packs_add_dir(packs, dir) == 0);
	CHECK(tileset_load(packs, "tiles/sea", &ts) == 0);
	CHECK(ts.terrain != NULL && ts.ground != NULL);
	if (ts.terrain != NULL)
		CHECK(memcmp(ts.terrain, want, sizeof(want)) == 0);
	if (ts.ground != NULL)
		CHECK(ts.ground[0] == GROUND_CLEAR &&
		    ts.ground[1] == GROUND_COVER &&
		    ts.ground[2] == GROUND_CLEAR &&
		    ts.ground[3] == GROUND_WATER);
	tileset_free(&ts);
	packs_free(packs);

	/* Without them, none: the world falls back on colours. */
	CHECK(try_tiles(dir, head) == 0);
	snprintf(text, sizeof(text), "%sterrain = bad.png\n", head);
	CHECK(try_tiles(dir, text) == -1);	/* a red pixel */
	snprintf(text, sizeof(text), "%sterrain = atlas.png\n", head);
	CHECK(try_tiles(dir, text) == -1);	/* art is not terrain */
	snprintf(text, sizeof(text), "%sterrain = none.png\n", head);
	CHECK(try_tiles(dir, text) == -1);
	snprintf(text, sizeof(text), "%s[ground]\n4 = cover\n", head);
	CHECK(try_tiles(dir, text) == -1);	/* only 4 tiles */
	snprintf(text, sizeof(text), "%s[ground]\n1 = rock\n", head);
	CHECK(try_tiles(dir, text) == -1);
	snprintf(text, sizeof(text), "%s[ground]\nx = cover\n", head);
	CHECK(try_tiles(dir, text) == -1);
	remove_files(dir, files);
}

/* Read a 3x2 Tiled map "text" of "layers" layers into "out". */
static int
read_map(const char *text, size_t layers, uint8_t *out)
{
	char err[256];

	return (tiled_read((const uint8_t *)text, strlen(text), 3, 2, layers,
	    out, err, sizeof(err)));
}

/*
 * Tiled maps: what an author saves with Tiled reads back as tile
 * numbers; what the engine cannot take is refused.
 */
static void
test_tiled(void)
{
	static const uint8_t grid[12] = { 0, 1, 2, 3, 4, 5, 255, 7, 8, 9, 10,
	    11 };
	/* As Tiled writes it: reals, an object layer, firstgid 5. */
	static const char good[] =
	    "{\"type\":\"map\",\"version\":\"1.10\",\"orientation\":"
	    "\"orthogonal\",\"infinite\":false,\"width\":3,\"height\":2,"
	    "\"tilewidth\":16,\"tileheight\":16,"
	    "\"tilesets\":[{\"firstgid\":5,\"source\":\"map.tsj\"}],"
	    "\"layers\":[{\"type\":\"tilelayer\",\"name\":\"a\",\"width\":3,"
	    "\"height\":2,\"opacity\":0.5,\"offsetx\":1.25e1,"
	    "\"data\":[5,6,7,8,9,10]},"
	    "{\"type\":\"objectgroup\",\"objects\":[{\"x\":1.5}]},"
	    "{\"type\":\"tilelayer\",\"name\":\"b\",\"width\":3,"
	    "\"height\":2,\"encoding\":\"csv\",\"data\":[260,5,5,5,5,5]}]}";
	static const char *const bad[] = {
		/* base64 data */
		"{\"width\":3,\"height\":2,\"tilesets\":[{\"firstgid\":1}],"
		"\"layers\":[{\"type\":\"tilelayer\",\"width\":3,\"height\":2,"
		"\"encoding\":\"base64\",\"data\":\"AQAAAA==\"}]}",
		/* a flipped tile */
		"{\"width\":3,\"height\":2,\"tilesets\":[{\"firstgid\":1}],"
		"\"layers\":[{\"type\":\"tilelayer\",\"width\":3,\"height\":2,"
		"\"data\":[1,1,1,1,1,2147483649]}]}",
		/* an empty cell */
		"{\"width\":3,\"height\":2,\"tilesets\":[{\"firstgid\":1}],"
		"\"layers\":[{\"type\":\"tilelayer\",\"width\":3,\"height\":2,"
		"\"data\":[1,1,0,1,1,1]}]}",
		/* tile 256 */
		"{\"width\":3,\"height\":2,\"tilesets\":[{\"firstgid\":1}],"
		"\"layers\":[{\"type\":\"tilelayer\",\"width\":3,\"height\":2,"
		"\"data\":[1,1,257,1,1,1]}]}",
		/* the wrong size */
		"{\"width\":2,\"height\":3,\"tilesets\":[{\"firstgid\":1}],"
		"\"layers\":[{\"type\":\"tilelayer\",\"width\":2,\"height\":3,"
		"\"data\":[1,1,1,1,1,1]}]}",
		/* two tilesets */
		"{\"width\":3,\"height\":2,\"tilesets\":[{\"firstgid\":1},"
		"{\"firstgid\":300}],\"layers\":[{\"type\":\"tilelayer\","
		"\"width\":3,\"height\":2,\"data\":[1,1,1,1,1,1]}]}",
		/* a group layer */
		"{\"width\":3,\"height\":2,\"tilesets\":[{\"firstgid\":1}],"
		"\"layers\":[{\"type\":\"group\",\"layers\":[]}]}",
		/* an infinite map */
		"{\"width\":3,\"height\":2,\"infinite\":true,"
		"\"tilesets\":[{\"firstgid\":1}],\"layers\":[]}",
		/* not orthogonal */
		"{\"width\":3,\"height\":2,\"orientation\":\"isometric\","
		"\"tilesets\":[{\"firstgid\":1}],\"layers\":[]}",
		/* too short a layer */
		"{\"width\":3,\"height\":2,\"tilesets\":[{\"firstgid\":1}],"
		"\"layers\":[{\"type\":\"tilelayer\",\"width\":3,\"height\":2,"
		"\"data\":[1,1,1]}]}",
		"not json",
	};
	struct jbuf b = { 0 };
	uint8_t out[12];
	size_t i;
	char err[256];
	struct json *j;

	/* Strict JSON for the sim, reals for other programs' files. */
	CHECK(json_parse("[1.5]", err, sizeof(err)) == NULL);
	j = json_parse_reals("[1.5,-2e3,0.25E-1,7]", err, sizeof(err));
	CHECK(j != NULL && j->count == 4 && j->items[0]->type == JSON_REAL &&
	    j->items[0]->num == 1 && j->items[1]->num == -2 &&
	    j->items[3]->type == JSON_INT);
	json_free(j);
	CHECK(json_parse_reals("[1.]", err, sizeof(err)) == NULL);
	CHECK(json_parse_reals("[1e]", err, sizeof(err)) == NULL);

	memset(out, 0xEE, sizeof(out));
	CHECK(read_map(good, 2, out) == 0);
	CHECK(out[0] == 0 && out[5] == 5 && out[6] == 255 && out[7] == 0);
	CHECK(read_map(good, 1, out) == -1);	/* too many tile layers */
	CHECK(read_map(good, 3, out) == -1);	/* too few */
	for (i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
		CHECK(read_map(bad[i], 1, out) == -1);

	/* What the engine writes, it reads. */
	tiled_write(&b, 3, 2, 32, 32, 2, grid, "screen", "battle.tsj");
	CHECK(!b.err && b.s != NULL && strstr(b.s, "\"screen 1\"") != NULL);
	memset(out, 0, sizeof(out));
	CHECK(b.s != NULL && read_map(b.s, 2, out) == 0 &&
	    memcmp(out, grid, sizeof(grid)) == 0);
	jb_free(&b);
	tiled_write_tileset(&b, "battle", 32, 256, "battle.png", 512, 512);
	CHECK(b.s != NULL && (j = json_parse(b.s, err, sizeof(err))) != NULL);
	if (b.s != NULL && j != NULL) {
		CHECK(json_get_int(j, "columns", 0) == 16 &&
		    json_get_int(j, "tilecount", 0) == 256);
		json_free(j);
	}
	jb_free(&b);
}

/* Town files: one valid record, then what is refused. */
static void
test_town_files(void)
{
	static const char one[] = "{\"towns\": [{\"name\": \"Havana\", "
	    "\"nation\": \"spain\", \"id\": 6, \"x\": 245, \"y\": 179, "
	    "\"flags\": 80, \"forts\": 4, \"soldiers\": 300, \"citizens\": 9, "
	    "\"gold\": 50, \"wealth\": 195, \"standing\": 1, \"garrison\": 2, "
	    "\"governor\": 3, \"family\": 64, \"barmaid\": 5, "
	    "\"land\": [1, 2], \"fort\": [3, 4], \"ship\": [5, 6, 7], "
	    "\"party\": [8, 9], \"sea\": [10, 11, 12], "
	    "\"citymap\": [13, 14]}]}";
	static struct era_towns era, back;
	struct jbuf b = { 0 };
	char err[256], text[1024];
	const struct town *t = &era.towns[0];

	CHECK(towns_decode((const uint8_t *)one, strlen(one), 4, &era, err,
	    sizeof(err)) == 0);
	CHECK(era.year == 1660 && era.count == 1);
	CHECK(strcmp(t->name, "Havana") == 0 && t->nation == NATION_SPAIN &&
	    t->flags == 80 && t->soldiers == 300 && t->family == 64 &&
	    t->standing == 1 && t->garrison == 2 && t->governor == 3 &&
	    t->barmaid == 5 && t->citizens == 9 && t->gold == 50 &&
	    t->wealth == 195 && t->forts == 4 && t->id == 6 &&
	    t->land_x == 1 && t->fort_x == 3 && t->fort_y == 4 &&
	    t->ship_x == 5 && t->ship_y == 6 && t->party_y == 9 &&
	    t->sea_y == 11 && t->citymap_x == 13 && t->citymap_y == 14 &&
	    t->land_y == 2 && t->ship_heading == 7 && t->sea_x == 10 &&
	    t->sea_heading == 12);

	/* No arrival cells given: its cell and the eight around. */
	CHECK(era.narrival == 9 && era.arrival[0].x == 244 &&
	    era.arrival[8].y == 180);

	/* What the engine writes, it reads. */
	towns_encode(&era, &b);
	CHECK(b.s != NULL && towns_decode((const uint8_t *)b.s, b.len, 4,
	    &back, err, sizeof(err)) == 0 && back.count == 1 &&
	    back.towns[0].party_x == 8 && back.towns[0].governor == 3 &&
	    back.narrival == 9);
	jb_free(&b);

	/* Arrival cells given; an empty list is refused. */
	snprintf(text, sizeof(text), "%.*s, \"arrival\": [[245, 180], "
	    "[246, 179]]}]}", (int)(strlen(one) - 3), one);
	CHECK(towns_decode((const uint8_t *)text, strlen(text), 4, &era, err,
	    sizeof(err)) == 0 && era.narrival == 2 &&
	    era.arrival[1].x == 246 && era.arrival[1].y == 179);
	snprintf(text, sizeof(text), "%.*s, \"arrival\": []}]}",
	    (int)(strlen(one) - 3), one);
	CHECK(towns_decode((const uint8_t *)text, strlen(text), 4, &era, err,
	    sizeof(err)) == -1);

	/* A missing member, a bad nation, a number out of range, a short
	 * point, a name too long, no towns. */
	snprintf(text, sizeof(text), "%s", one);
	memcpy(strstr(text, "\"gold\""), "\"gilt\"", 6);
	CHECK(towns_decode((const uint8_t *)text, strlen(text), 4, &era, err,
	    sizeof(err)) == -1 && strstr(err, "Havana") != NULL);
	snprintf(text, sizeof(text), "%s", one);
	memcpy(strstr(text, "spain"), "spaim", 5);
	CHECK(towns_decode((const uint8_t *)text, strlen(text), 4, &era, err,
	    sizeof(err)) == -1);
	snprintf(text, sizeof(text), "%s", one);
	memcpy(strstr(text, "195"), "-95", 3);
	CHECK(towns_decode((const uint8_t *)text, strlen(text), 4, &era, err,
	    sizeof(err)) == -1);
	snprintf(text, sizeof(text), "%s", one);
	memcpy(strstr(text, "[1, 2]"), "[1    ]", 6);
	CHECK(towns_decode((const uint8_t *)text, strlen(text), 4, &era, err,
	    sizeof(err)) == -1);
	CHECK(towns_decode((const uint8_t *)"{\"towns\": []}", 13, 4, &era,
	    err, sizeof(err)) == -1);
	CHECK(towns_decode((const uint8_t *)one, strlen(one), 6, &era, err,
	    sizeof(err)) == -1);
	snprintf(text, sizeof(text), "{\"towns\": [{\"name\": "
	    "\"Seventeen chars!!\", \"nation\": \"spain\"}]}");
	CHECK(towns_decode((const uint8_t *)text, strlen(text), 4, &era, err,
	    sizeof(err)) == -1 && strstr(err, "name") != NULL);
}

/*
 * A part of a picture with true colours, drawn at twice the screen's
 * size: its art comes along, through the key.
 */
static void
test_image_part(void)
{
	static struct fb fb;
	static uint8_t px[4 * 2] = { 1, 2, 3, 4, 5, 6, 7, 0 };
	static uint8_t art[8 * 4 * 4];
	struct image img;
	uint8_t *rgb;
	int x, y;

	/* Art pixel (x, y) is (10 x, 10 y, 7). */
	for (y = 0; y < 4; y++)
		for (x = 0; x < 8; x++) {
			art[(y * 8 + x) * 4] = (uint8_t)(10 * x);
			art[(y * 8 + x) * 4 + 1] = (uint8_t)(10 * y);
			art[(y * 8 + x) * 4 + 2] = 7;
			art[(y * 8 + x) * 4 + 3] = 255;
		}
	memset(&img, 0, sizeof(img));
	img.w = 4;
	img.h = 2;
	img.pixels = px;
	img.rgba = art;
	img.rgba_w = 8;
	img.rgba_h = 4;
	if (fb_init(&fb, 960) != 0 ||
	    (rgb = malloc((size_t)1280 * 960 * 3)) == NULL) {
		CHECK(!"fb_init");
		return;
	}
	palette_fixed(fb.pal);
	fb_clear(&fb, 9);
	/* Picture pixels (2, 0)-(3, 1) at (5, 5); key 0, at (3, 1). */
	fb_image_part(&fb, &img, 2, 0, 2, 2, 5, 5, 0);
	CHECK(fb.px[5 * FB_W + 5] == 3 && fb.px[6 * FB_W + 6] == 9);
	fb_to_rgb(&fb, 256, rgb);
	/* Canvas (10, 10) shows art (4, 0); (13, 11), art (7, 1). */
	CHECK(at(rgb, 1280, 10, 10)[0] == 40 && at(rgb, 1280, 10, 10)[1] == 0 &&
	    at(rgb, 1280, 10, 10)[2] == 7);
	CHECK(at(rgb, 1280, 11, 11)[0] == 50 && at(rgb, 1280, 11, 11)[1] == 10);
	CHECK(at(rgb, 1280, 12, 10)[0] == 60);
	/* The keyed pixel keeps what was there, not art (6..7, 2..3). */
	CHECK(at(rgb, 1280, 12, 12)[2] != 7);
	free(rgb);
	fb_free(&fb);
}

/* Sets "name" to "value", or removes it when "value" is NULL. */
static void
set_env(const char *name, const char *value)
{
#ifdef _WIN32
	_putenv_s(name, value != NULL ? value : "");
#else
	if (value != NULL)
		setenv(name, value, 1);
	else
		unsetenv(name);
#endif
}

static void
test_user_lang(void)
{
	char code[8];

	set_env("LC_ALL", NULL);
	set_env("LC_MESSAGES", NULL);
	set_env("LANG", "fr_FR.UTF-8");
	CHECK(fsys_user_lang(code, sizeof(code)) == 0 &&
	    strcmp(code, "fr") == 0);
	set_env("LANG", "DE-de");
	CHECK(fsys_user_lang(code, sizeof(code)) == 0 &&
	    strcmp(code, "de") == 0);
	/* LC_ALL decides over LANG, as in setlocale(). */
	set_env("LC_ALL", "es_ES@euro");
	CHECK(fsys_user_lang(code, sizeof(code)) == 0 &&
	    strcmp(code, "es") == 0);
	/* Too short a buffer, or no language in the name, is no answer. */
	CHECK(fsys_user_lang(code, 2) != 0);
	set_env("LC_ALL", "C");
	if (fsys_user_lang(code, sizeof(code)) == 0)
		CHECK(strcmp(code, "c") != 0);
	set_env("LC_ALL", NULL);
	set_env("LANG", NULL);
}

int
main(void)
{
	test_pcx();
	test_rsd();
	test_sprites();
	test_img();
	test_tileset();
	test_font();
	test_text();
	test_sha256();
	test_towns();
	test_packs();
	test_png();
	test_png_write();
	test_pic();
	test_fb();
	test_sheets();
	test_image_part();
	test_fonts();
	test_tiles();
	test_tile_terrain();
	test_tiled();
	test_town_files();
	test_user_lang();
	if (failures > 0) {
		fprintf(stderr, "%d check(s) failed\n", failures);
		return (1);
	}
	printf("all format tests passed\n");
	return (0);
}
