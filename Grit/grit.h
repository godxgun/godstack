/* ===========================================================================
 * GRIT - Copyright @ Vasco Alves - See LICENSE at the end of file.
 *
 * Utilities library. Includes:
 * - Memory allocators (arena, stack & pool)
 * - RNG.
 * - Common data structures such as dynamic arrays and hashmaps.
 * - Fast math function.
 * - Linear algrebra with no vector types, instead ops write in place.
 * - Speed over accuracy.
 * - Software raster functions.
 *
 * PREFIX: GRIT (macros)  Grit (types)  grit_ (functions)
 *
 * TYPES (math):
 * - vec2 = float[2]
 * - vec3 = float[3]
 * - mat4 = float[16] column-major
 * A pointer that is not backed by the correct amount of memory
 * is undefined behaviour and misuse of the library.
 *
 * USAGE:
 *     #include "grit.h"
 *
 *     unsigned char buf[1 << 16];
 *     GritArena arena;
 *     grit_arena_init(&arena, buf, sizeof buf);
 *
 * =========================================================================== */

#ifndef GRIT_H
#define GRIT_H

#define GRIT_MAJOR 0
#define GRIT_MINOR 2
#define GRIT_PATCH 1

/* CHANGE LOG
 * 0.0.0 - @vasco - math, draw, arena, stack, pool
 * 0.1.0 - @vasco - hash maps!!!
 * 0.2.0 - @vasco - RNG!!!
 * 0.2.1 - @vasco - include grit.c
 */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(GRIT_DEBUG)
#define GASSERT_N(_1, _2, N, ...) N
#define GASSERT(...) GASSERT_N(__VA_ARGS__, GASSERT2, GASSERT1)(__VA_ARGS__)
#define GASSERT1(a) assert(a)
#define GASSERT2(a, s) assert((a) && (s))
#else
#define GASSERT(...) ((void)0)
#endif

#define GRIT_TODO \
    do { \
        fprintf(stderr, "GRIT TODO: %s() in %s:%d\n", __func__, __FILE__, __LINE__); \
        abort(); \
    } while (0)

#ifndef GRIT_DEFAULT_ALIGN
#define GRIT_DEFAULT_ALIGN (2 * sizeof(void *))
#endif

#define GRIT_PI      3.14159265358979323846f
#define GRIT_PI2     (2.0f * GRIT_PI)
#define GRIT_PI_HALF (GRIT_PI * 0.5f)
#define GRIT_PI_POW2 (GRIT_PI * GRIT_PI)

#define GRIT_MIN(a, b) ((a) < (b) ? (a) : (b))
#define GRIT_MAX(a, b) ((a) > (b) ? (a) : (b))

typedef struct GritArena {
    unsigned char *buf;
    size_t buf_len;
    size_t curr_offset;
    size_t prev_offset;
} GritArena;

typedef struct GritStackHeader {
    uint8_t padding;
} GritStackHeader;

typedef struct GritStack {
    unsigned char *buf;
    size_t buf_len;
    size_t offset;
} GritStack;

typedef struct GritPoolNode GritPoolNode;

typedef struct GritPool {
    unsigned char *buf;
    size_t chunk_size;
    size_t pool_size;
    GritPoolNode *head;
} GritPool;

struct GritPoolNode {
    struct GritPoolNode *next;
};

typedef struct GritDArray {
    void *data;
    size_t len;
    size_t cap;
    size_t elem_size;
} GritDArray;

typedef struct GritHashMap {
    void *keys;
    void *vals;
    uint8_t *state;
    size_t len;
    size_t cap;
    size_t tombed;
    size_t key_size;
    size_t val_size;
} GritHashMap;

typedef struct GritDraw {
    uint32_t *px;
    int w, h;
    int cx, cy, cw, ch;
} GritDraw;

typedef struct GritRng {
    uint64_t state;
} GritRng;

void grit_arena_init(GritArena *a, void *buf, size_t len);
void *grit_arena_alloc_align(GritArena *a, size_t size, size_t align);
void *grit_arena_alloc(GritArena *a, size_t size);
void *grit_arena_resize_align(GritArena *a, void *old, size_t old_size, size_t new_size, size_t align);
void *grit_arena_resize(GritArena *a, void *old, size_t old_size, size_t new_size);
void grit_arena_free_all(GritArena *a);
void grit_arena_destroy(GritArena *a);

void grit_stack_init(GritStack *s, void *buf, size_t len);
void *grit_stack_alloc(GritStack *s, size_t len, size_t alignment);
void grit_stack_free(GritStack *s, void *ptr);
void grit_stack_free_all(GritStack *s);
void grit_stack_destroy(GritStack *s);

void grit_pool_init(GritPool *p, void *buf, size_t buf_len, size_t chunk_size, size_t chunk_align);
void *grit_pool_alloc(GritPool *p);
void grit_pool_free(GritPool *p, void *ptr);
void grit_pool_free_all(GritPool *p);
void grit_pool_destroy(GritPool *p);

GritDArray grit_darray_create(size_t init_capacity, size_t elem_size);
void grit_darray_destroy(GritDArray *a);
void grit_darray_push(GritDArray *a, const void *elem);
void *grit_darray_get(GritDArray *a, size_t i);
void grit_darray_pop(GritDArray *a);

GritHashMap grit_hashmap_create(size_t init_capacity, size_t key_size, size_t val_size);
void grit_hashmap_destroy(GritHashMap *m);
void grit_hashmap_put(GritHashMap *m, const void *key, const void *val);
void *grit_hashmap_get(GritHashMap *m, const void *key);
void grit_hashmap_del(GritHashMap *m, const void *key);

void grit_ceil(float *x);
void grit_sinf(float *x);
void grit_cosf(float *x);
void grit_atan2f(float *y, float x);
void grit_lerpf(float *a, float b, float t);

void grit_vec2f(float *v, float x, float y);
void grit_vec2f_add(float *a, const float *b);
void grit_vec2f_sub(float *a, const float *b);
void grit_vec2f_mult(float *a, float s);
void grit_vec2f_dot(float *out, const float *a, const float *b);
void grit_vec2f_cross(float *out, const float *a, const float *b);
void grit_vec2f_len(float *out, const float *a);
void grit_vec2f_norm(float *a);
void grit_vec2f_lerp(float *a, const float *b, float t);

void grit_vec3f(float *v, float x, float y, float z);
void grit_vec3f_add(float *a, const float *b);
void grit_vec3f_sub(float *a, const float *b);
void grit_vec3f_mult(float *a, float s);
void grit_vec3f_dot(float *out, const float *a, const float *b);
void grit_vec3f_cross(float *a, const float *b);
void grit_vec3f_len(float *out, const float *a);
void grit_vec3f_norm(float *a);
void grit_vec3f_lerp(float *a, const float *b, float t);

void grit_mat4_identity(float *m);
void grit_mat4_mul(float *a, const float *b);
void grit_mat4_translate(float *m, const float *v);
void grit_mat4_translate_by(float *m, const float *v);
void grit_mat4_rotate_x(float *m, float rad);
void grit_mat4_rotate_x_by(float *m, float rad);
void grit_mat4_rotate_y(float *m, float rad);
void grit_mat4_rotate_y_by(float *m, float rad);
void grit_mat4_rotate_z(float *m, float rad);
void grit_mat4_perspective(float *m, float fov_rad, float aspect, float near_z, float far_z);
void grit_mat4_ortho(float *m, float left, float right, float bottom, float top, float near_z, float far_z);
void grit_mat4_lookat(float *m, const float *eye, const float *center, const float *up);

void grit_rng_seed(GritRng *r, uint64_t seed);
uint64_t grit_rng_u64(GritRng *r);
uint32_t grit_rng_u32(GritRng *r);
void grit_rng_f32(GritRng *r, float *out);

void grit_draw_begin(GritDraw *d, uint32_t *px, int w, int h);
void grit_draw_clip(GritDraw *d, int x, int y, int w, int h);
void grit_draw_clip_reset(GritDraw *d);
void grit_draw_clear(GritDraw *d, uint32_t color);
void grit_draw_pixel(GritDraw *d, int x, int y, uint32_t color);
void grit_draw_blit(GritDraw *d, int x, int y, const uint32_t *src, int sw, int sh);
void grit_draw_rect(GritDraw *d, int x, int y, int w, int h, uint32_t color);
void grit_draw_rect_gradient(GritDraw *d, int x, int y, int w, int h, uint32_t c0, uint32_t c1, uint32_t c2, uint32_t c3);
void grit_draw_tri(GritDraw *d, int x0, int y0, int x1, int y1, int x2, int y2, uint32_t c0, uint32_t c1, uint32_t c2);
void grit_draw_line(GritDraw *d, int x0, int y0, int x1, int y1, int width, uint32_t c0, uint32_t c1);
void grit_draw_circle(GritDraw *d, int cx, int cy, int radius, uint32_t color);

#endif /* GRIT_H */

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
