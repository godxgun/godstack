# Grit

Documentation: https://example.org/docs/grit/
Description: Choose caller-backed allocators and use Grit utilities with explicit lifetimes.


# Grit

Grit is a collection of small utilities: arena, stack and pool allocators over memory you
provide, a dynamic array and a hash map, a seeded random number generator, fast approximate math
with vectors and 4x4 matrices, and a software rasterizer that draws into a pixel buffer. Each
part can be used on its own.

Define `GRIT_IMPLEMENTATION` in exactly one translation unit before including `Grit.h`. Other
translation units include the header without it. Define `GRIT_DEBUG` as well to turn on assertions
that catch misuse, such as freeing a pointer into the wrong allocator.

## Allocating from an Arena

The first thing to try is an arena: allocations come from one buffer, one after another, and are
all released together:

```c
#include <stdio.h>
#define GRIT_IMPLEMENTATION
#include "Grit.h"

int main(void)
{
    static unsigned char storage[1024];
    GritArena arena;
    grit_arena_init(&arena, storage, sizeof storage);

    int *squares = grit_arena_alloc(&arena, 10 * sizeof *squares);
    if (!squares) return 1;
    for (int i = 0; i < 10; ++i) squares[i] = i * i;
    printf("squares[9] = %d, %zu bytes used\n", squares[9], arena.curr_offset);

    grit_arena_free_all(&arena);   /* every allocation is gone; storage is reusable */
    grit_arena_destroy(&arena);    /* detaches the arena; storage stays yours */
    return 0;
}
```

`grit_arena_init()` borrows your buffer; Grit never frees it. Each allocation is zeroed and
aligned to `GRIT_DEFAULT_ALIGN` (two pointers); `grit_arena_alloc_align()` takes an explicit
power-of-two alignment. When the buffer is full the call returns NULL rather than growing, so
check every result.

`grit_arena_resize()` grows or shrinks an allocation. The most recent allocation is resized in
place; any other is copied to a new block at the end of the arena. Arenas suit memory with one
shared lifetime, such as everything built during one frame or one request.

## Stack Allocation

A stack allocator also takes memory from a buffer, but can release the most recent allocations
first. `grit_stack_free()` rewinds the stack to just before the pointer you pass, releasing that
allocation and everything allocated after it:

```c
static unsigned char storage[4096];
GritStack stack;
grit_stack_init(&stack, storage, sizeof storage);

float *points = grit_stack_alloc(&stack, 256 * sizeof(float), 16);
char *scratch = grit_stack_alloc(&stack, 512, 1);
if (!points || !scratch) return 1;

grit_stack_free(&stack, scratch);   /* releases scratch */
grit_stack_free(&stack, points);    /* releases points */
```

The last argument of `grit_stack_alloc()` is the alignment (a power of two, at most 128). Each
allocation stores a small header in front of it so the stack knows how far to rewind.
`grit_stack_free_all()` empties it in one step.

## Pools of Equal-Sized Objects

A pool splits a buffer into equal chunks that can be allocated and freed in any order, which
suits objects that come and go, such as particles or connections:

```c
typedef struct Particle { float position[2], velocity[2]; uint32_t id; } Particle;

static unsigned char slots[4096];
GritPool pool;
grit_pool_init(&pool, slots, sizeof slots, sizeof(Particle), 16);

Particle *p = grit_pool_alloc(&pool);
if (!p) return 1;          /* every chunk is in use */
p->id = 7;
grit_pool_free(&pool, p);  /* the chunk can be handed out again */
```

The chunk size is rounded up to the alignment (here 20 bytes become 32, so the buffer holds up to 128
chunks) and must hold at least a pointer. Allocation and free are constant time; allocated chunks
are zeroed. With `GRIT_DEBUG` defined, allocating from an empty pool triggers an assertion instead
of returning NULL, which helps find undersized pools during development. `grit_pool_free_all()` returns every chunk.

## Dynamic Arrays and Hash Maps

`GritDArray` and `GritHashMap` grow as needed and, unlike the allocators, use `malloc()` and
`realloc()`. Both store fixed-size elements by copying their bytes:

```c
typedef struct Player { uint32_t id; int score; } Player;

GritDArray players = grit_darray_create(8, sizeof(Player));
GritHashMap by_id = grit_hashmap_create(16, sizeof(uint32_t), sizeof(size_t));

Player alice = {42, 1200};
grit_darray_push(&players, &alice);
size_t index = players.len - 1;
grit_hashmap_put(&by_id, &alice.id, &index);

uint32_t key = 42;
size_t *found = grit_hashmap_get(&by_id, &key);
if (found) {
    Player *player = grit_darray_get(&players, *found);
    player->score += 10;
}

grit_hashmap_del(&by_id, &key);
grit_hashmap_destroy(&by_id);
grit_darray_destroy(&players);
```

`grit_darray_get()` returns a pointer to the element, or NULL when the index is out of range; it
stays valid until the array grows. Pushes report nothing on allocation failure, so compare `len`
if it matters. `grit_darray_pop()` drops the last element.

Hash map keys are compared byte for byte. Integers work directly; for structs, zero the whole key
(including padding) before filling it, and store strings in fixed-size buffers. `put` copies the
value and replaces an existing one; `get` returns a pointer to the stored value, or NULL. Pointers
returned by `get` are invalidated when the map grows.

## Random Numbers

`GritRng` is a small, fast xorshift generator. The same seed always gives the same sequence,
which makes simulations reproducible. It is not suitable for cryptography:

```c
GritRng rng;
grit_rng_seed(&rng, 1234);
uint32_t roll = grit_rng_u32(&rng) % 6 + 1;
float t;
grit_rng_f32(&rng, &t);   /* uniform in [0, 1) */
```

## Vectors and Matrices

The math functions favor speed over accuracy and write through their first argument instead of
returning values. Vectors are `float[2]` and `float[3]`; matrices are `float[16]` in column-major
order:

```c
float eye[3], center[3], up[3], offset[3];
grit_vec3f(eye, 0, 2, 5);
grit_vec3f(center, 0, 0, 0);
grit_vec3f(up, 0, 1, 0);
grit_vec3f(offset, 1, 0, 0);

float model[16], view[16], mvp[16];
grit_mat4_translate(model, offset);
grit_mat4_rotate_y_by(model, GRIT_PI * 0.25f);
grit_mat4_lookat(view, eye, center, up);
grit_mat4_perspective(mvp, GRIT_PI / 3, 16.0f / 9.0f, 0.1f, 100.0f);
grit_mat4_mul(mvp, view);    /* mvp = projection * view */
grit_mat4_mul(mvp, model);   /* mvp = projection * view * model */
```

`grit_mat4_mul(a, b)` stores `a * b` in `a`, and the `_by` variants multiply an existing matrix
by a translation or rotation. The perspective matrix is right-handed and maps depth to `0..1`, as
Vulkan expects. `grit_sinf()`, `grit_cosf()`, `grit_atan2f()` and the vector lengths are
approximations: good for animation and graphics, not for numerics.

## Drawing Pixels

`GritDraw` draws into a buffer of `0xAARRGGBB` pixels that you own, row by row with no padding.
Colors are written as given, without blending. This program draws a small scene and saves it as a
PPM image:

```c
#include <stdio.h>
#define GRIT_IMPLEMENTATION
#include "Grit.h"

enum { W = 160, H = 100 };

int main(void)
{
    static uint32_t pixels[W * H];
    GritDraw d;
    grit_draw_begin(&d, pixels, W, H);

    grit_draw_clear(&d, 0xFF101418u);
    grit_draw_rect_gradient(&d, 0, 0, W, 40, 0xFF203050u, 0xFF203050u, 0xFF101418u, 0xFF101418u);
    grit_draw_tri(&d, 20, 90, 60, 30, 100, 90, 0xFFFF0000u, 0xFF00FF00u, 0xFF0000FFu);
    grit_draw_line(&d, 0, 95, W - 1, 60, 2, 0xFFFFFFFFu, 0xFFFFAA00u);
    grit_draw_clip(&d, 110, 20, 40, 40);
    grit_draw_circle(&d, 130, 40, 30, 0xFFFFD040u);   /* clipped to the 40x40 box */
    grit_draw_clip_reset(&d);

    FILE *out = fopen("grit.ppm", "wb");
    if (!out) return 1;
    fprintf(out, "P6\n%d %d\n255\n", W, H);
    for (int i = 0; i < W * H; ++i) {
        unsigned char rgb[3] = {(pixels[i] >> 16) & 0xFF, (pixels[i] >> 8) & 0xFF, pixels[i] & 0xFF};
        fwrite(rgb, 1, 3, out);
    }
    return fclose(out) == 0 ? 0 : 1;
}
```

Every primitive clips to the buffer and to the rectangle set with `grit_draw_clip()`, except
`grit_draw_clear()`, which fills the whole buffer. Gradient corners run top-left, top-right,
bottom-right, bottom-left; triangles interpolate their three vertex colors; lines take a width and
blend from the first color to the second. `grit_draw_blit()` copies a block of pixels and
`grit_draw_pixel()` sets one. To show the result in a window, upload the buffer with your platform
layer, as the [Grit demo](https://github.com/godxgun/godstack/tree/main/demos/grit) does with
[Peak](/docs/peak/).

See [Grit symbols](/symbols/grit/) for the generated declarations.
