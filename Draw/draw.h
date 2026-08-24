/* ===========================================================================
 * DRAW - software raster into an RGBA32 buffer - see LICENCE at end of file.
 * =========================================================================== */

#ifndef DRAW_H
#define DRAW_H

/* CHANGE LOG
 * 0.0.0 - @vasco - prototyping
 */

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define DRAW__MIN(a, b) ((a) < (b) ? (a) : (b))
#define DRAW__MAX(a, b) ((a) > (b) ? (a) : (b))

typedef struct Draw {
	uint32_t *px;
	int w, h;
	int cx, cy, cw, ch;
} Draw;

static void draw_begin(Draw *d, uint32_t *px, int w, int h);
static void draw_clip(Draw *d, int x, int y, int w, int h);
static void draw_clip_reset(Draw *d);
static void draw_pixel(Draw *d, int x, int y, uint32_t color);
static uint32_t draw__lerp(uint32_t c0, uint32_t c1, float t);
static float draw__cross(float ax, float ay, float bx, float by, float cx, float cy);
static void draw_blit(Draw *d, int x, int y, const uint32_t *src, int sw, int sh);
static void draw_rect(Draw *d, int x, int y, int w, int h, uint32_t color);
static void draw_rect_gradient(Draw *d, int x, int y, int w, int h, uint32_t c0, uint32_t c1, uint32_t c2, uint32_t c3);
static void draw_tri(Draw *d, int x0, int y0, int x1, int y1, int x2, int y2, uint32_t c0, uint32_t c1, uint32_t c2);
static void draw_line(Draw *d, int x0, int y0, int x1, int y1, int width, uint32_t c0, uint32_t c1);

void
draw_begin(Draw *d, uint32_t *px, int w, int h)
{
	d->px = px;
	d->w = w;
	d->h = h;
	d->cx = 0;
	d->cy = 0;
	d->cw = w;
	d->ch = h;
}

void
draw_clip(Draw *d, int x, int y, int w, int h)
{
	d->cx = x;
	d->cy = y;
	d->cw = w;
	d->ch = h;
}

void
draw_clip_reset(Draw *d)
{
	d->cx = 0;
	d->cy = 0;
	d->cw = d->w;
	d->ch = d->h;
}

void
draw_pixel(Draw *d, int x, int y, uint32_t color)
{
	if (x < d->cx || x >= d->cx + d->cw || y < d->cy || y >= d->cy + d->ch)
		return;
	if (x < 0 || x >= d->w || y < 0 || y >= d->h)
		return;
	d->px[y * d->w + x] = color;
}

uint32_t
draw__lerp(uint32_t c0, uint32_t c1, float t)
{
	uint32_t a0, a1, r0, r1, g0, g1, b0, b1;
	uint32_t a, r, g, b;
	if (t <= 0.0f)
		return c0;
	if (t >= 1.0f)
		return c1;
	a0 = (c0 >> 24) & 0xFF;
	a1 = (c1 >> 24) & 0xFF;
	r0 = (c0 >> 16) & 0xFF;
	r1 = (c1 >> 16) & 0xFF;
	g0 = (c0 >> 8) & 0xFF;
	g1 = (c1 >> 8) & 0xFF;
	b0 = c0 & 0xFF;
	b1 = c1 & 0xFF;
	a = (uint32_t)(a0 + t * ((float)a1 - (float)a0));
	r = (uint32_t)(r0 + t * ((float)r1 - (float)r0));
	g = (uint32_t)(g0 + t * ((float)g1 - (float)g0));
	b = (uint32_t)(b0 + t * ((float)b1 - (float)b0));
	return (a << 24) | (r << 16) | (g << 8) | b;
}

float
draw__cross(float ax, float ay, float bx, float by, float cx, float cy)
{
	return (cx - ax) * (by - ay) - (cy - ay) * (bx - ax);
}

void
draw_blit(Draw *d, int x, int y, const uint32_t *src, int sw, int sh)
{
	int row, ty, x0, x1;
	for (row = 0; row < sh; ++row) {
		ty = y + row;
		if (ty < d->cy || ty >= d->cy + d->ch || ty < 0 || ty >= d->h)
			continue;
		x0 = DRAW__MAX(x, d->cx);
		x1 = DRAW__MIN(x + sw, d->cx + d->cw);
		if (x0 >= x1)
			continue;
		memcpy(&d->px[ty * d->w + x0], &src[row * sw + (x0 - x)], (size_t)(x1 - x0) * sizeof(uint32_t));
	}
}

void
draw_rect(Draw *d, int x, int y, int w, int h, uint32_t color)
{
	int x0, y0, x1, y1;
	int py, px;
	x0 = DRAW__MAX(x, d->cx);
	y0 = DRAW__MAX(y, d->cy);
	x1 = DRAW__MIN(x + w, d->cx + d->cw);
	y1 = DRAW__MIN(y + h, d->cy + d->ch);
	for (py = y0; py < y1; ++py) {
		if (py < 0 || py >= d->h)
			continue;
		for (px = x0; px < x1; ++px) {
			if (px < 0 || px >= d->w)
				continue;
			d->px[py * d->w + px] = color;
		}
	}
}

void
draw_rect_gradient(Draw *d, int x, int y, int w, int h, uint32_t c0, uint32_t c1, uint32_t c2, uint32_t c3)
{
	int ry, rx;
	float ty, tx;
	uint32_t left, right;
	for (ry = 0; ry < h; ++ry) {
		ty = (h > 1) ? (float)ry / (float)(h - 1) : 0.0f;
		left  = draw__lerp(c0, c3, ty);
		right = draw__lerp(c1, c2, ty);
		for (rx = 0; rx < w; ++rx) {
			tx = (w > 1) ? (float)rx / (float)(w - 1) : 0.0f;
			draw_pixel(d, x + rx, y + ry, draw__lerp(left, right, tx));
		}
	}
}

void
draw_tri(Draw *d, int x0, int y0, int x1, int y1, int x2, int y2, uint32_t c0, uint32_t c1, uint32_t c2)
{
	int min_x, max_x, min_y, max_y;
	int solid;
	int x, y;
	float area;
	float w0, w1, w2;
	uint32_t c;
	min_x = DRAW__MIN(x0, DRAW__MIN(x1, x2));
	max_x = DRAW__MAX(x0, DRAW__MAX(x1, x2));
	min_y = DRAW__MIN(y0, DRAW__MIN(y1, y2));
	max_y = DRAW__MAX(y0, DRAW__MAX(y1, y2));
	area = draw__cross((float)x0, (float)y0, (float)x1, (float)y1, (float)x2, (float)y2);
	if (area == 0.0f)
		return;
	solid = (c0 == c1 && c1 == c2);
	for (y = min_y; y <= max_y; ++y) {
		for (x = min_x; x <= max_x; ++x) {
			w0 = draw__cross((float)x1, (float)y1, (float)x2, (float)y2, (float)x, (float)y);
			w1 = draw__cross((float)x2, (float)y2, (float)x0, (float)y0, (float)x, (float)y);
			w2 = draw__cross((float)x0, (float)y0, (float)x1, (float)y1, (float)x, (float)y);
			if ((w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0)) {
				c = c0;
				if (!solid) {
					w0 /= area;
					w1 /= area;
					w2 /= area;
					c = draw__lerp(c0, c1, w1 / (w0 + w1 + 1e-6f));
					c = draw__lerp(c, c2, w2);
				}
				draw_pixel(d, x, y, c);
			}
		}
	}
}

void
draw_line(Draw *d, int x0, int y0, int x1, int y1, int width, uint32_t c0, uint32_t c1)
{
	float total;
	int grad;
	int dx, sx, dy, sy, err;
	int ox0, oy0, hw;
	int ox, oy, e2;
	uint32_t c;
	total = 0.0f;
	grad = (c0 != c1);
	if (grad)
		total = sqrtf((float)((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0)));
	dx = abs(x1 - x0);
	sx = x0 < x1 ? 1 : -1;
	dy = -abs(y1 - y0);
	sy = y0 < y1 ? 1 : -1;
	err = dx + dy;
	ox0 = x0;
	oy0 = y0;
	hw = width / 2;
	for (;;) {
		c = c0;
		if (grad) {
			float cd;
			cd = sqrtf((float)((x0 - ox0) * (x0 - ox0) + (y0 - oy0) * (y0 - oy0)));
			c = draw__lerp(c0, c1, total > 0.0f ? cd / total : 0.0f);
		}
		for (ox = -hw; ox <= hw; ++ox)
			for (oy = -hw; oy <= hw; ++oy)
				draw_pixel(d, x0 + ox, y0 + oy, c);
		if (x0 == x1 && y0 == y1)
			break;
		e2 = 2 * err;
		if (e2 >= dy) {
			err += dy;
			x0 += sx;
		}
		if (e2 <= dx) {
			err += dx;
			y0 += sy;
		}
	}
}

#endif /* DRAW_H */

/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2026 Vasco Alves
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/
