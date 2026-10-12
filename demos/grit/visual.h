/* Demo-private software framebuffer and Peak loop. Identical local copies keep
 * each demo independently buildable; no dependency beyond Peak.h and libc.
 * Drawing colors are 0xAARRGGBB; native Peak uses that format, while
 * Peak web ImageData needs LE RGBA (0xAABBGGRR). */
#ifndef DEMO_VISUAL_H
#define DEMO_VISUAL_H
#define _POSIX_C_SOURCE 200809L
#define PEAK_NO_AUDIO
#define PEAK_NO_GAMEPAD
#define PEAK_IMPLEMENTATION
#include "Peak.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct View {
	uint32_t *pixels;
	int w, h, x0, y0, x1, y1;
} View;
static uint32_t view_rgba(uint32_t c)
{
#ifdef PEAK_WEB
	return (c & 0xff00ff00u) | ((c & 255u) << 16) | ((c >> 16) & 255u);
#else
	return c;
#endif
}
static void view_clip(View *v, int x, int y, int w, int h)
{
	v->x0 = x > 0 ? x : 0;
	v->y0 = y > 0 ? y : 0;
	v->x1 = x + w < v->w ? x + w : v->w;
	v->y1 = y + h < v->h ? y + h : v->h;
}
static void view_pixel(View *v, int x, int y, uint32_t c)
{
	if (x < v->x0 || y < v->y0 || x >= v->x1 || y >= v->y1)
		return;
	uint32_t *p = v->pixels + y * v->w + x, s = view_rgba(c), a = c >> 24;
	if (a == 255)
		*p = s;
	else if (a) {
		uint32_t d = *p, out = 0xff000000u;
		for (int k = 0; k < 24; k += 8)
			out |= ((((s >> k) & 255) * a + ((d >> k) & 255) * (255 - a)) / 255) << k;
		*p = out;
	}
}
static void view_rect(View *v, int x, int y, int w, int h, uint32_t c)
{
	int x0 = x > v->x0 ? x : v->x0, y0 = y > v->y0 ? y : v->y0;
	int x1 = x + w < v->x1 ? x + w : v->x1, y1 = y + h < v->y1 ? y + h : v->y1;
	for (int j = y0; j < y1; j++)
		for (int i = x0; i < x1; i++)
			view_pixel(v, i, j, c);
}
static inline void view_line(View *v, int x0, int y0, int x1, int y1, int width, uint32_t c)
{
	int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1,
	    err = dx + dy;
	if (width < 1)
		width = 1;
	for (;;) {
		view_rect(v, x0 - width / 2, y0 - width / 2, width, width, c);
		if (x0 == x1 && y0 == y1)
			break;
		int e = err * 2;
		if (e >= dy) {
			err += dy;
			x0 += sx;
		}
		if (e <= dx) {
			err += dx;
			y0 += sy;
		}
	}
}
static inline void view_dot(View *v, int x, int y, int r, uint32_t c)
{
	for (int j = -r; j <= r; j++)
		for (int i = -r; i <= r; i++)
			if (i * i + j * j <= r * r)
				view_pixel(v, x + i, y + j, c);
}
/* Readable 5x7 uppercase bitmap alphabet; lowercase is folded for labels/source. */
static const unsigned char view_font[36][7] = {
    {14, 17, 17, 31, 17, 17, 17}, {30, 17, 17, 30, 17, 17, 30}, {14, 17, 16, 16, 16, 17, 14},
    {30, 17, 17, 17, 17, 17, 30}, {31, 16, 16, 30, 16, 16, 31}, {31, 16, 16, 30, 16, 16, 16},
    {14, 17, 16, 23, 17, 17, 15}, {17, 17, 17, 31, 17, 17, 17}, {14, 4, 4, 4, 4, 4, 14},
    {7, 2, 2, 2, 18, 18, 12},     {17, 18, 20, 24, 20, 18, 17}, {16, 16, 16, 16, 16, 16, 31},
    {17, 27, 21, 21, 17, 17, 17}, {17, 25, 21, 19, 17, 17, 17}, {14, 17, 17, 17, 17, 17, 14},
    {30, 17, 17, 30, 16, 16, 16}, {14, 17, 17, 17, 21, 18, 13}, {30, 17, 17, 30, 20, 18, 17},
    {15, 16, 16, 14, 1, 1, 30},   {31, 4, 4, 4, 4, 4, 4},       {17, 17, 17, 17, 17, 17, 14},
    {17, 17, 17, 17, 17, 10, 4},  {17, 17, 17, 21, 21, 21, 10}, {17, 17, 10, 4, 10, 17, 17},
    {17, 17, 10, 4, 4, 4, 4},     {31, 1, 2, 4, 8, 16, 31},     {14, 17, 19, 21, 25, 17, 14},
    {4, 12, 4, 4, 4, 4, 14},      {14, 17, 1, 2, 4, 8, 31},     {30, 1, 1, 14, 1, 1, 30},
    {2, 6, 10, 18, 31, 2, 2},     {31, 16, 16, 30, 1, 1, 30},   {14, 16, 16, 30, 17, 17, 14},
    {31, 1, 2, 4, 8, 8, 8},       {14, 17, 17, 14, 17, 17, 14}, {14, 17, 17, 15, 1, 1, 14}};
static unsigned view_glyph(unsigned char c, int y)
{
	static const unsigned char punctuation[256][7] = {
	    [33] = {4, 4, 4, 4, 0, 0, 4},        [34] = {10, 10, 0, 0, 0, 0, 0},
	    [35] = {10, 10, 31, 10, 31, 10, 10}, [37] = {17, 1, 2, 4, 4, 8, 17},
	    [39] = {4, 4, 0, 0, 0, 0, 0},        [40] = {6, 8, 8, 8, 8, 8, 6},
	    [41] = {12, 2, 2, 2, 2, 2, 12},      [42] = {0, 0, 10, 31, 10, 0, 0},
	    [43] = {0, 4, 4, 31, 4, 4, 0},       [44] = {0, 0, 0, 0, 0, 4, 8},
	    [45] = {0, 0, 0, 14, 0, 0, 0},       [46] = {0, 0, 0, 0, 0, 0, 4},
	    [47] = {1, 1, 2, 4, 4, 8, 16},       [58] = {0, 0, 4, 0, 0, 4, 0},
	    [59] = {0, 0, 4, 0, 0, 4, 8},        [60] = {2, 4, 8, 16, 8, 4, 2},
	    [61] = {0, 0, 31, 0, 31, 0, 0},      [62] = {16, 8, 4, 2, 4, 8, 16},
	    [63] = {14, 17, 2, 4, 0, 0, 4},      [91] = {6, 8, 8, 8, 8, 8, 6},
	    [92] = {16, 16, 8, 4, 4, 2, 1},      [93] = {12, 2, 2, 2, 2, 2, 12},
	    [95] = {0, 0, 0, 0, 0, 0, 31},       [123] = {6, 8, 8, 8, 8, 8, 6},
	    [124] = {4, 4, 4, 4, 4, 4, 4},       [125] = {12, 2, 2, 2, 2, 2, 12},
	};
	if (c >= 'a' && c <= 'z')
		c -= 32;
	if (c >= 'A' && c <= 'Z')
		return view_font[c - 'A'][y];
	if (c >= '0' && c <= '9')
		return view_font[26 + c - '0'][y];
	return punctuation[c][y];
}
static void view_text(View *v, int x, int y, int scale, const char *s, uint32_t c)
{
	int start = x;
	if (scale < 1)
		scale = 1;
	for (; *s; s++) {
		if (*s == '\n') {
			y += 9 * scale;
			x = start;
			continue;
		}
		for (int j = 0; j < 7; j++) {
			unsigned bits = view_glyph((unsigned char)*s, j);
			for (int i = 0; i < 5; i++)
				if (bits & (16u >> i))
					view_rect(v, x + i * scale, y + j * scale, scale, scale, c);
		}
		x += 6 * scale;
	}
}

typedef struct DemoLoop {
	PeakCtx *ctx;
	PeakWindow window;
	View view;
	int paused, stress, failed, closed;
	unsigned frames, frame_limit;
	unsigned keys;
	uint64_t last;
	double elapsed, accumulator;
	int (*reset)(void);
	int (*step)(double);
	int (*render)(View *);
	void (*event)(const PeakEvent *, float, float);
	void (*destroy)(void);
} DemoLoop;
static DemoLoop loop;
static void demo_close(void)
{
	if (loop.closed)
		return;
	loop.closed = 1;
	if (loop.destroy)
		loop.destroy();
	peak_window_close(&loop.window);
	if (loop.ctx)
		peak_quit(loop.ctx);
	loop.ctx = NULL;
	free(loop.view.pixels);
	loop.view.pixels = NULL;
}
static int demo_present(PeakWindow *win)
{
	size_t w = 0, h = 0;
	uint32_t *dst = peak_window_backbuffer(win, &w, &h);
	if (!w || !h)
		return 1; /* A minimized window has no frame to upload. */
	if (!dst)
		return 0;
	/* Fixed workspace, aspect-preserving nearest-neighbor letterboxing. */
	size_t dw = w, dh = w * loop.view.h / loop.view.w;
	if (dh > h) {
		dh = h;
		dw = h * loop.view.w / loop.view.h;
	}
	if (!dw || !dh)
		return 1;
	size_t ox = (w - dw) / 2, oy = (h - dh) / 2;
	for (size_t i = 0; i < w * h; i++)
		dst[i] = view_rgba(0xff141413u);
	for (size_t y = 0; y < dh; y++)
		for (size_t x = 0; x < dw; x++)
			dst[(y + oy) * w + x + ox] =
			    loop.view.pixels[(y * loop.view.h / dh) * loop.view.w + x * loop.view.w / dw];
	peak_window_present(win);
	return 1;
}
static void demo_pointer(float *x, float *y)
{
	size_t w = loop.window.width, h = loop.window.height;
	size_t dw = w, dh = w * loop.view.h / loop.view.w;
	if (dh > h) {
		dh = h;
		dw = h * loop.view.w / loop.view.h;
	}
	if (!dw || !dh) {
		*x = *y = -100;
		return;
	}
	*x = (*x - (float)(w - dw) / 2) * loop.view.w / dw;
	*y = (*y - (float)(h - dh) / 2) * loop.view.h / dh;
}
static int demo_keys(const PeakEvent *ev)
{
	unsigned bit = 0;
	if (ev->type == PEAK_EVENT_KEY_DOWN || ev->type == PEAK_EVENT_KEY_UP)
		bit = ev->key.key == PEAK_KEY_SPACE ? 1
		      : ev->key.key == PEAK_KEY_R   ? 2
		      : ev->key.key == PEAK_KEY_S   ? 4
		                                    : 0;
	if (ev->type == PEAK_EVENT_KEY_UP)
		loop.keys &= ~bit;
	if (ev->type == PEAK_EVENT_FOCUS && !ev->focus.on)
		loop.keys = 0;
	if (ev->type == PEAK_EVENT_KEY_DOWN && !(loop.keys & bit)) {
		loop.keys |= bit;
		if (bit == 1) {
			loop.paused = !loop.paused;
			loop.accumulator = 0;
		}
		if (bit == 2 || bit == 4) {
			if (bit == 4)
				loop.stress = !loop.stress;
			loop.elapsed = loop.accumulator = 0;
			if (!loop.reset())
				return -1;
		}
	}
	return 1;
}

static int demo_events(PeakWindow *win)
{
	PeakEvent ev;
	while (peak_window_epoll(win, &ev)) {
		if (ev.type == PEAK_EVENT_WINDOW_CLOSE) {
			demo_close();
			return 0;
		}
		if (demo_keys(&ev) < 0)
			return -1;
		if (ev.type == PEAK_EVENT_KEY_DOWN && ev.key.key == PEAK_KEY_ESCAPE) {
			demo_close();
			return 0;
		}
		if (loop.event) {
			float x = 0, y = 0;
			if (ev.type == PEAK_EVENT_POINTER) {
				x = ev.pointer.x;
				y = ev.pointer.y;
				demo_pointer(&x, &y);
			}
			loop.event(&ev, x, y);
		}
	}
	return 1;
}

static int demo_frame(PeakWindow *win, void *unused)
{
	(void)unused;
	int events = demo_events(win);
	if (events < 0)
		goto error;
	if (!events)
		return 0;
	uint64_t now = peak_get_time();
	double dt = (double)(now - loop.last) / 1e9;
	loop.last = now;
	if (dt > .05)
		dt = .05;
	if (!loop.paused) {
		loop.accumulator += dt;
		for (int n = 0; loop.accumulator >= 1.0 / 60 && n < 3; n++) {
			loop.elapsed += 1.0 / 60;
			if (!loop.step(loop.elapsed))
				goto error;
			loop.accumulator -= 1.0 / 60;
		}
	}
	view_clip(&loop.view, 0, 0, loop.view.w, loop.view.h);
	if (!loop.render(&loop.view) || !demo_present(win))
		goto error;
	if (loop.frame_limit && ++loop.frames >= loop.frame_limit) {
		demo_close();
		return 0;
	}
#ifndef PEAK_WEB
	/* Pace CPU-only native rendering; actual simulation uses Peak's clock. */
	uint64_t used = peak_get_time() - now;
	if (used < 16666667)
		peak_sleep_ns((int64_t)(16666667 - used));
#endif
	return 1;
error:
	fprintf(stderr, "visual demo: API/render/presentation failure\n");
	loop.failed = 1;
	demo_close();
	return 0;
}
static void demo_controls(View *v, int y)
{
	char text[128];
	snprintf(text, sizeof(text), "SPACE PAUSE | R RESTART | S STRESS | ESC QUIT | %s | STRESS %s",
	         loop.paused ? "PAUSED" : "RUNNING", loop.stress ? "ON" : "OFF");
	view_text(v, 24, y, 1, text, 0xffb5b4adu);
}
static int demo_run(int argc, char **argv, const char *title, int w, int h)
{
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--paused"))
			loop.paused = 1;
		else if (!strcmp(argv[i], "--frames") && i + 1 < argc) {
			char *end = NULL;
			unsigned long n = strtoul(argv[++i], &end, 10);
			if (!end || *end || !n || n > UINT32_MAX) {
				fprintf(stderr, "%s: --frames requires a positive frame count\n", title);
				return 1;
			}
			loop.frame_limit = (unsigned)n;
		} else {
			fprintf(stderr, "usage: %s [--paused] [--frames N]\n", title);
			return 1;
		}
	}
	loop.view = (View){NULL, w, h, 0, 0, w, h};
	loop.view.pixels = malloc((size_t)w * h * sizeof(uint32_t));
	if (!loop.view.pixels || !(loop.ctx = peak_init()))
		goto error;
	loop.window = peak_window_open(loop.ctx, "canvas", (uint32_t)w, (uint32_t)h, 0);
	if (!loop.window.running)
		goto error;
	peak_window_set_title(&loop.window, title);
	if (!loop.reset() || !loop.render(&loop.view) || !demo_present(&loop.window))
		goto error;
	loop.last = peak_get_time();
	/* Static window/userdata survive Emscripten's asynchronous main-loop handoff. */
	peak_window_run(&loop.window, demo_frame, NULL);
	demo_close();
	return loop.failed ? 1 : 0;
error:
	fprintf(stderr, "%s: initialization failed\n", title);
	loop.failed = 1;
	demo_close();
	return 1;
}
#endif
