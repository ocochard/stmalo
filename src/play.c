/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026, Olivier Cochard-Labbé
 *
 * raylib front end of the game shell: input in, framebuffer out.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raylib.h"

#include "assets/fmmusic.h"
#include "audio/music.h"
#include "crt.h"
#include "play.h"
#include "sim/save.h"
#include "ui/game.h"
#include "ui/script.h"
#ifdef HAVE_WINDOW_ICON
#include "window_icon.h"
#endif

#define WINDOW_SCALE	2

static const struct {
	int	raylib;
	int	ui;
} keymap[] = {
	{ KEY_UP, UI_KEY_UP }, { KEY_DOWN, UI_KEY_DOWN },
	{ KEY_LEFT, UI_KEY_LEFT }, { KEY_RIGHT, UI_KEY_RIGHT },
	{ KEY_ENTER, UI_KEY_ENTER }, { KEY_KP_ENTER, UI_KEY_ENTER },
	{ KEY_ESCAPE, UI_KEY_ESCAPE }, { KEY_BACKSPACE, UI_KEY_BACKSPACE },
	{ KEY_LEFT_SHIFT, UI_KEY_SHIFT }, { KEY_RIGHT_SHIFT, UI_KEY_SHIFT },
	/* The numeric keypad: its arrows, and its corners as diagonals. */
	{ KEY_KP_8, UI_KEY_UP }, { KEY_KP_2, UI_KEY_DOWN },
	{ KEY_KP_4, UI_KEY_LEFT }, { KEY_KP_6, UI_KEY_RIGHT },
	{ KEY_KP_7, UI_KEY_UP_LEFT }, { KEY_KP_9, UI_KEY_UP_RIGHT },
	{ KEY_KP_1, UI_KEY_DOWN_LEFT }, { KEY_KP_3, UI_KEY_DOWN_RIGHT },
};

/* The save directory, or "" (saving disabled) with a warning. */
static void
resolve_saves(const char *saves, char *dir, size_t len)
{
	char err[256];

	if (saves != NULL) {
		snprintf(dir, len, "%s", saves);
		return;
	}
	if (save_dir(dir, len, err, sizeof(err)) != 0) {
		fprintf(stderr, "saved games disabled: %s\n", err);
		dir[0] = '\0';
	}
}

/* Where the 640x480 screen sits in the window, keeping its shape. */
static Rectangle
letterbox(void)
{
	float sx = (float)GetScreenWidth() / FB_W;
	float sy = (float)GetScreenHeight() / FB_H;
	float s = sx < sy ? sx : sy;

	return ((Rectangle){ (GetScreenWidth() - FB_W * s) / 2,
	    (GetScreenHeight() - FB_H * s) / 2, FB_W * s, FB_H * s });
}

static void
feed_input(struct game *g, Vector2 *last_mouse)
{
	struct ui_event ev;
	Rectangle box = letterbox();
	Vector2 m = GetMousePosition();
	size_t i;
	int c;

	for (i = 0; i < sizeof(keymap) / sizeof(keymap[0]); i++) {
		if (IsKeyPressed(keymap[i].raylib) ||
		    IsKeyPressedRepeat(keymap[i].raylib)) {
			ev.type = EV_KEY;
			ev.key = keymap[i].ui;
			game_event(g, &ev);
		}
		if (IsKeyReleased(keymap[i].raylib)) {
			ev.type = EV_KEYUP;
			ev.key = keymap[i].ui;
			game_event(g, &ev);
		}
	}
	while ((c = GetCharPressed()) != 0) {
		ev.type = EV_CHAR;
		ev.ch = c;
		game_event(g, &ev);
	}
	ev.x = (int)((m.x - box.x) * FB_W / box.width);
	ev.y = (int)((m.y - box.y) * FB_H / box.height);
	if (m.x != last_mouse->x || m.y != last_mouse->y) {
		ev.type = EV_MOVE;
		game_event(g, &ev);
		*last_mouse = m;
	}
	if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
		ev.type = EV_CLICK;
		game_event(g, &ev);
	}
	if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
		ev.type = EV_RCLICK;
		game_event(g, &ev);
	}
}

/*
 * The effects through raylib's audio device, loaded once and played when
 * the game asks: DIGITAL.PIR's, each replaced by a pack's recording
 * ("sound/broadside") when there is one, and the recordings a pack gives
 * the FM effects ("sound/swing"), played instead of the driver's.
 * Without a device the game stays silent.
 */
#define FM_COMMANDS	64

struct sounds {
	Sound		 digital[DIGITAL_EFFECTS];
	Sound		 fm[FM_COMMANDS];	/* by driver command */
};

/* A pack's recording "id" as a sound; 0 when there is none. */
static int
sound_from_pack(struct game *g, const char *id, Sound *out)
{
	struct blob b;
	char type[16];
	Wave w;

	if (id == NULL || !packs_provides(g->packs, id) ||
	    packs_load(g->packs, id, &b) != 0)
		return (0);
	snprintf(type, sizeof(type), ".%s", b.ext);
	w = LoadWaveFromMemory(type, b.data, (int)b.len);
	blob_free(&b);
	if (w.frameCount == 0) {
		fprintf(stderr, "%s: cannot play this %s file\n", id, type + 1);
		return (0);
	}
	*out = LoadSoundFromWave(w);
	UnloadWave(w);
	return (out->frameCount > 0);
}

static void
sounds_load(struct game *g, struct sounds *sd)
{
	Wave w;
	int i;

	memset(sd, 0, sizeof(*sd));
	InitAudioDevice();
	if (!IsAudioDeviceReady())
		return;
	for (i = 0; i < DIGITAL_EFFECTS; i++) {
		if (sound_from_pack(g, game_sfx_id(i), &sd->digital[i])) {
			g->sfx_packed |= 1u << i;
			continue;
		}
		if (g->sfx.pcm[i] == NULL)
			continue;
		w = (Wave){ (unsigned)g->sfx.len[i], DIGITAL_RATE, 8, 1,
		    g->sfx.pcm[i] };
		sd->digital[i] = LoadSoundFromWave(w);
	}
	for (i = 0; i < FM_COMMANDS; i++)
		sound_from_pack(g, game_fm_id(i), &sd->fm[i]);
}

static void
sounds_play(struct game *g, struct sounds *sd)
{
	int i;

	for (i = 0; i < DIGITAL_EFFECTS; i++)
		if ((g->sfx_pending & (1u << i)) &&
		    sd->digital[i].frameCount > 0)
			PlaySound(sd->digital[i]);
	g->sfx_pending = 0;
	/* An FM effect with a recording plays it, not the driver. */
	if (g->fm_request > 0 && g->fm_request < FM_COMMANDS &&
	    sd->fm[g->fm_request].frameCount > 0) {
		PlaySound(sd->fm[g->fm_request]);
		g->fm_request = 0;
	}
}

static void
sounds_unload(struct sounds *sd)
{
	int i;

	for (i = 0; i < DIGITAL_EFFECTS; i++)
		if (sd->digital[i].frameCount > 0)
			UnloadSound(sd->digital[i]);
	for (i = 0; i < FM_COMMANDS; i++)
		if (sd->fm[i].frameCount > 0)
			UnloadSound(sd->fm[i]);
	if (IsAudioDeviceReady())
		CloseAudioDevice();
}

/*
 * The music: the FM driver's tunes rendered as the stream asks, the
 * game's requests taken each frame.  A pack may replace a tune with a
 * recording ("music/title" as OGG, MP3, WAV or another format raylib
 * reads), streamed in its place; the FM effects still play.  Without a
 * device, no music; without the driver, only the replacements.
 */
#define MUSIC_CHUNK	2048	/* frames per stream buffer */
/* A recording's stream buffers: long enough to ride out a frame spent
 * decoding a pack's picture (150 ms and more), which would starve the
 * driver's short ones.  Only the effects need those short. */
#define TUNE_CHUNK	32768
#define TUNE_FADE	3.0f	/* seconds, about the driver's fade */

struct jukebox {
	int		 ready;		/* an audio device */
	struct music	*m;
	AudioStream	 stream;
	int		 playing;	/* the last tune started */
	/* A pack's recording of that tune, while it plays. */
	struct blob	 tune_data;	/* raylib streams from it */
	Music		 tune;
	int		 tune_on;
	float		 tune_volume;
	int		 tune_fading;
	int		 paused;	/* held for the game's pause */
};

static void
jukebox_open(struct game *g, struct jukebox *j)
{
	struct fmmusic d;
	struct blob b;

	memset(j, 0, sizeof(*j));
	j->playing = -1;
	if (!IsAudioDeviceReady())
		return;
	j->ready = 1;
	if (packs_load(g->packs, "driver/psound", &b) != 0)
		return;
	if (fmmusic_decode(b.data, b.len, &d) == 0) {
		j->m = music_new(&d);
		fmmusic_free(&d);
	}
	blob_free(&b);
	if (j->m == NULL)
		return;
	SetAudioStreamBufferSizeDefault(MUSIC_CHUNK);
	j->stream = LoadAudioStream(MUSIC_RATE, 16, 2);
	PlayAudioStream(j->stream);
}

static void
tune_stop(struct jukebox *j)
{
	if (!j->tune_on)
		return;
	StopMusicStream(j->tune);
	UnloadMusicStream(j->tune);
	blob_free(&j->tune_data);
	j->tune_on = 0;
}

#define TUNE_VARIANTS	9	/* "music/tavern", then "-2" to "-9" */

/*
 * The recording to play for "base": one of its variants ("music/tavern",
 * "music/tavern-2"...) at random.  Returns 0 and the ID in "id", or -1
 * when no pack has any.
 */
static int
tune_pick(struct game *g, const char *base, char *id, size_t len)
{
	char found[TUNE_VARIANTS][64];
	int n = 0, k;

	for (k = 1; k <= TUNE_VARIANTS; k++) {
		if (k == 1)
			snprintf(found[n], sizeof(found[n]), "%s", base);
		else
			snprintf(found[n], sizeof(found[n]), "%s-%d", base, k);
		if (packs_provides(g->packs, found[n]))
			n++;
	}
	if (n == 0)
		return (-1);
	snprintf(id, len, "%s", found[GetRandomValue(0, n - 1)]);
	return (0);
}

/* Start a pack's recording of tune "cmd" (or of the screen's "alt");
 * 0 when no pack has one. */
static int
tune_start(struct game *g, struct jukebox *j, int cmd, const char *alt)
{
	const char *base = game_tune_id(cmd);
	struct blob b;
	char id[64], type[16];

	if (base == NULL || ((alt == NULL || tune_pick(g, alt, id,
	    sizeof(id)) != 0) && tune_pick(g, base, id, sizeof(id)) != 0) ||
	    packs_load(g->packs, id, &b) != 0)
		return (0);
	snprintf(type, sizeof(type), ".%s", b.ext);
	tune_stop(j);
	SetAudioStreamBufferSizeDefault(TUNE_CHUNK);
	j->tune = LoadMusicStreamFromMemory(type, b.data, (int)b.len);
	SetAudioStreamBufferSizeDefault(MUSIC_CHUNK);
	if (!IsMusicValid(j->tune)) {
		fprintf(stderr, "%s: cannot play this %s file\n", id, b.ext);
		blob_free(&b);
		return (0);
	}
	j->tune_data = b;
	j->tune.looping = false;	/* as the driver's tunes, once */
	j->tune_volume = 1.0f;
	j->tune_fading = 0;
	SetMusicVolume(j->tune, 1.0f);
	PlayMusicStream(j->tune);
	j->tune_on = 1;
	return (1);
}

static void
jukebox_feed(struct game *g, struct jukebox *j)
{
	static int16_t buf[2 * MUSIC_CHUNK];
	int req = g->music_request;
	const char *alt = g->music_alt;

	if (!j->ready)
		return;
	/* The pause holds the music where it is (driver calls 6 and 7,
	 * VA 0x2e91). */
	if ((g->paused != 0) != j->paused) {
		j->paused = g->paused != 0;
		if (j->m != NULL && j->paused)
			PauseAudioStream(j->stream);
		else if (j->m != NULL)
			ResumeAudioStream(j->stream);
		if (j->tune_on && j->paused)
			PauseMusicStream(j->tune);
		else if (j->tune_on)
			ResumeMusicStream(j->tune);
	}
	if (j->paused)
		return;
	g->music_request = -1;
	g->music_alt = NULL;
	if (req == MUS_STOP || req == MUS_SILENCE) {
		if (j->m != NULL) {
			if (req == MUS_STOP)
				music_stop(j->m);
			else
				music_silence(j->m);
		}
		if (req == MUS_SILENCE)
			tune_stop(j);
		else
			j->tune_fading = j->tune_on;
		j->playing = -1;
	} else if (req > 0 && !(req == j->playing && ((j->tune_on &&
	    !j->tune_fading) || (j->m != NULL && music_active(j->m))))) {
		/* A tune already playing goes on, as the title's handler
		 * checks (driver VA 0x82b). */
		if (tune_start(g, j, req, alt)) {
			if (j->m != NULL)
				music_stop(j->m);
		} else {
			tune_stop(j);
			if (j->m != NULL)
				music_command(j->m, req);
		}
		j->playing = req;
	}
	if (j->tune_on) {
		UpdateMusicStream(j->tune);
		if (j->tune_fading) {
			j->tune_volume -= GetFrameTime() / TUNE_FADE;
			SetMusicVolume(j->tune, j->tune_volume > 0 ?
			    j->tune_volume : 0);
		}
		if (j->tune_volume <= 0 || !IsMusicStreamPlaying(j->tune))
			tune_stop(j);
	}
	if (j->m == NULL) {
		g->fm_request = 0;
		return;
	}
	/* An effect plays over the tune, on the driver's effect voices. */
	if (g->fm_request > 0)
		music_command(j->m, g->fm_request);
	g->fm_request = 0;
	while (IsAudioStreamProcessed(j->stream)) {
		music_render(j->m, buf, MUSIC_CHUNK);
		UpdateAudioStream(j->stream, buf, MUSIC_CHUNK);
	}
}

static void
jukebox_close(struct jukebox *j)
{
	tune_stop(j);
	if (j->m == NULL)
		return;
	UnloadAudioStream(j->stream);
	music_free(j->m);
}

int
play_run(struct packs *packs, const char *lang, const char *saves,
    const char *shot, int shot_frames, int crt_on, int debug, int smooth)
{
	static struct game g;
	uint8_t *rgb;
	Vector2 last_mouse = { -1, -1 };
	Texture2D tex;
	Image im, screen;
	char dir[512];
	int frames = 0, rc = 0, k, big_k, tw, th;
	RenderTexture2D big;
	static struct sounds sd;
	struct jukebox jb;
	struct crt crt = { 0 };
	Rectangle box;

	resolve_saves(saves, dir, sizeof(dir));
	if (game_init(&g, packs, lang, dir) != 0) {
		fprintf(stderr, "%s\n", g.err);
		game_free(&g);
		return (1);
	}
	g.debug = debug;
	g.smooth = smooth;
	tw = g.fb.out_w;	/* the frame, at the art's resolution */
	th = g.fb.out_h;
	if ((rgb = malloc((size_t)tw * th * 3)) == NULL) {
		fprintf(stderr, "out of memory\n");
		game_free(&g);
		return (1);
	}
	SetTraceLogLevel(getenv("FLIBUSTIER_GL_LOG") != NULL ? LOG_INFO : LOG_WARNING);
	SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
	InitWindow(FB_W * WINDOW_SCALE, FB_H * WINDOW_SCALE, "flibustier");
#ifdef HAVE_WINDOW_ICON
	{
		Image icon = LoadImageFromMemory(".png", window_icon_png,
		    (int)sizeof(window_icon_png));

		SetWindowIcon(icon);
		UnloadImage(icon);
	}
#endif
	SetExitKey(KEY_NULL);	/* Escape is a game key */
	HideCursor();		/* the game draws its own sword pointer */
	SetTargetFPS(60);
	sounds_load(&g, &sd);
	jukebox_open(&g, &jb);
	im = (Image){ rgb, tw, th, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8 };
	tex = LoadTextureFromImage(im);
	SetTextureFilter(tex, TEXTURE_FILTER_POINT);
	/*
	 * "Sharp bilinear": the frame is first enlarged by the largest
	 * whole factor that fits, pixel for pixel, then smoothed only over
	 * the remaining fraction, so that strokes keep an even width at any
	 * window size.
	 */
	big = (RenderTexture2D){ 0 };
	big_k = 0;
	if (crt_load(&crt) != 0 && crt_on)
		fprintf(stderr, "the CRT look is not available here\n");
	while (!WindowShouldClose() && !g.quit) {
		feed_input(&g, &last_mouse);
		game_tick(&g);
		sounds_play(&g, &sd);
		jukebox_feed(&g, &jb);
		game_draw(&g);
		fb_to_rgb(&g.fb, g.brightness, rgb);
		UpdateTexture(tex, rgb);
		if (IsKeyPressed(KEY_F2))
			crt_on = !crt_on;
		box = letterbox();
		if (crt_on && crt.ok) {
			BeginDrawing();
			ClearBackground(BLACK);
			crt_draw(&crt, tex, box);
			EndDrawing();
			goto shown;
		}
		k = (int)(box.width / tw);
		if (k < 1)
			k = 1;
		if (k != big_k) {
			if (big_k > 0)
				UnloadRenderTexture(big);
			big = LoadRenderTexture(tw * k, th * k);
			SetTextureFilter(big.texture, TEXTURE_FILTER_BILINEAR);
			big_k = k;
		}
		BeginTextureMode(big);
		DrawTexturePro(tex, (Rectangle){ 0, 0, tw, th },
		    (Rectangle){ 0, 0, tw * k, th * k }, (Vector2){ 0, 0 },
		    0, WHITE);
		EndTextureMode();
		BeginDrawing();
		ClearBackground(BLACK);
		/* Render textures are stored upside down: flip the source. */
		DrawTexturePro(big.texture, (Rectangle){ 0, 0, tw * k,
		    -th * k }, box, (Vector2){ 0, 0 }, 0, WHITE);
		EndDrawing();
shown:
		if (shot != NULL && ++frames >= shot_frames) {
			screen = LoadImageFromScreen();
			if (!ExportImage(screen, shot))
				rc = 1;
			UnloadImage(screen);
			break;
		}
	}
	if (big_k > 0)
		UnloadRenderTexture(big);
	crt_unload(&crt);
	UnloadTexture(tex);
	jukebox_close(&jb);
	sounds_unload(&sd);
	CloseWindow();
	free(rgb);
	game_free(&g);
	return (rc);
}

int
play_headless(struct packs *packs, const char *lang, const char *saves,
    const char *script, const char *out, int debug)
{
	static struct game g;
	uint8_t *rgb;
	char dir[512];
	Image im;
	int rc = 0;

	resolve_saves(saves, dir, sizeof(dir));
	if (game_init(&g, packs, lang, dir) != 0) {
		fprintf(stderr, "%s\n", g.err);
		game_free(&g);
		return (1);
	}
	g.debug = debug;
	if (game_script(&g, script) != 0) {
		fprintf(stderr, "%s\n", g.err);
		game_free(&g);
		return (1);
	}
	if ((rgb = malloc((size_t)g.fb.out_w * g.fb.out_h * 3)) == NULL) {
		fprintf(stderr, "out of memory\n");
		game_free(&g);
		return (1);
	}
	fb_to_rgb(&g.fb, g.brightness, rgb);
	im = (Image){ rgb, g.fb.out_w, g.fb.out_h, 1,
	    PIXELFORMAT_UNCOMPRESSED_R8G8B8 };
	SetTraceLogLevel(getenv("FLIBUSTIER_GL_LOG") != NULL ? LOG_INFO : LOG_WARNING);
	if (out != NULL && !ExportImage(im, out))
		rc = 1;
	free(rgb);
	printf("screen: %s\n", g.screen != NULL ? "shown" : "quit");
	game_free(&g);
	return (rc);
}
