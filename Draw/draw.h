/* ===========================================================================
 * DRAW - software raster into an RGBA32 buffer - see LICENCE at end of file.
 * =========================================================================== */

#ifndef DRAW_H
#define DRAW_H

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

typedef struct Draw {
    uint32_t *px;
    int w, h;
    int cx, cy, cw, ch;
} Draw;

#define DRAW__MIN(a, b) ((a) < (b) ? (a) : (b))
#define DRAW__MAX(a, b) ((a) > (b) ? (a) : (b))

static inline void
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

static inline void
draw_clip(Draw *d, int x, int y, int w, int h)
{
    d->cx = x;
    d->cy = y;
    d->cw = w;
    d->ch = h;
}

static inline void
draw_clip_reset(Draw *d)
{
    d->cx = 0;
    d->cy = 0;
    d->cw = d->w;
    d->ch = d->h;
}

static inline void
draw_pixel(Draw *d, int x, int y, uint32_t color)
{
    if (x < d->cx || x >= d->cx + d->cw || y < d->cy || y >= d->cy + d->ch) return;
    if (x < 0 || x >= d->w || y < 0 || y >= d->h) return;
    d->px[y * d->w + x] = color;
}

static inline uint32_t
draw__lerp(uint32_t c0, uint32_t c1, float t)
{
    if (t <= 0.0f) return c0;
    if (t >= 1.0f) return c1;
    uint32_t a0 = (c0 >> 24) & 0xFF, a1 = (c1 >> 24) & 0xFF;
    uint32_t r0 = (c0 >> 16) & 0xFF, r1 = (c1 >> 16) & 0xFF;
    uint32_t g0 = (c0 >> 8)  & 0xFF, g1 = (c1 >> 8)  & 0xFF;
    uint32_t b0 = c0 & 0xFF,         b1 = c1 & 0xFF;
    uint32_t a = (uint32_t)(a0 + t * ((float)a1 - (float)a0));
    uint32_t r = (uint32_t)(r0 + t * ((float)r1 - (float)r0));
    uint32_t g = (uint32_t)(g0 + t * ((float)g1 - (float)g0));
    uint32_t b = (uint32_t)(b0 + t * ((float)b1 - (float)b0));
    return (a << 24) | (r << 16) | (g << 8) | b;
}

static inline float
draw__cross(float ax, float ay, float bx, float by, float cx, float cy)
{
    return (cx - ax) * (by - ay) - (cy - ay) * (bx - ax);
}

static inline void
draw_blit(Draw *d, int x, int y, const uint32_t *src, int sw, int sh)
{
    for (int row = 0; row < sh; ++row) {
        int ty = y + row;
        if (ty < d->cy || ty >= d->cy + d->ch || ty < 0 || ty >= d->h) continue;
        int x0 = DRAW__MAX(x, d->cx);
        int x1 = DRAW__MIN(x + sw, d->cx + d->cw);
        if (x0 >= x1) continue;
        memcpy(&d->px[ty * d->w + x0], &src[row * sw + (x0 - x)], (size_t)(x1 - x0) * sizeof(uint32_t));
    }
}

static inline void
draw_rect(Draw *d, int x, int y, int w, int h, uint32_t color)
{
    int x0 = DRAW__MAX(x, d->cx);
    int y0 = DRAW__MAX(y, d->cy);
    int x1 = DRAW__MIN(x + w, d->cx + d->cw);
    int y1 = DRAW__MIN(y + h, d->cy + d->ch);
    for (int py = y0; py < y1; ++py) {
        if (py < 0 || py >= d->h) continue;
        for (int px = x0; px < x1; ++px) {
            if (px < 0 || px >= d->w) continue;
            d->px[py * d->w + px] = color;
        }
    }
}

static inline void
draw_rect_gradient(Draw *d, int x, int y, int w, int h, uint32_t c0, uint32_t c1, uint32_t c2, uint32_t c3)
{
    for (int ry = 0; ry < h; ++ry) {
        float ty = (h > 1) ? (float)ry / (float)(h - 1) : 0.0f;
        uint32_t left  = draw__lerp(c0, c3, ty);
        uint32_t right = draw__lerp(c1, c2, ty);
        for (int rx = 0; rx < w; ++rx) {
            float tx = (w > 1) ? (float)rx / (float)(w - 1) : 0.0f;
            draw_pixel(d, x + rx, y + ry, draw__lerp(left, right, tx));
        }
    }
}

static inline void
draw_tri(Draw *d, int x0, int y0, int x1, int y1, int x2, int y2, uint32_t c0, uint32_t c1, uint32_t c2)
{
    int min_x = DRAW__MIN(x0, DRAW__MIN(x1, x2));
    int max_x = DRAW__MAX(x0, DRAW__MAX(x1, x2));
    int min_y = DRAW__MIN(y0, DRAW__MIN(y1, y2));
    int max_y = DRAW__MAX(y0, DRAW__MAX(y1, y2));
    float area = draw__cross((float)x0, (float)y0, (float)x1, (float)y1, (float)x2, (float)y2);
    if (area == 0.0f) return;
    int solid = (c0 == c1 && c1 == c2);
    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            float w0 = draw__cross((float)x1, (float)y1, (float)x2, (float)y2, (float)x, (float)y);
            float w1 = draw__cross((float)x2, (float)y2, (float)x0, (float)y0, (float)x, (float)y);
            float w2 = draw__cross((float)x0, (float)y0, (float)x1, (float)y1, (float)x, (float)y);
            if ((w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0)) {
                uint32_t c = c0;
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

static inline void
draw_line(Draw *d, int x0, int y0, int x1, int y1, int width, uint32_t c0, uint32_t c1)
{
    float total = 0.0f;
    int grad = (c0 != c1);
    if (grad) total = sqrtf((float)((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0)));
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    int ox0 = x0, oy0 = y0;
    int hw = width / 2;
    for (;;) {
        uint32_t c = c0;
        if (grad) {
            float cd = sqrtf((float)((x0 - ox0) * (x0 - ox0) + (y0 - oy0) * (y0 - oy0)));
            c = draw__lerp(c0, c1, total > 0.0f ? cd / total : 0.0f);
        }
        for (int ox = -hw; ox <= hw; ++ox)
            for (int oy = -hw; oy <= hw; ++oy)
                draw_pixel(d, x0 + ox, y0 + oy, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
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
