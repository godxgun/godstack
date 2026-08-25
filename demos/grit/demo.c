#define GRIT_DEBUG
#include "grit.h"
#include "grit.c"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define CANVAS_W 48
#define CANVAS_H 18

static int g_fails;

static void expect(int ok, const char *what);
static int nearly(float a, float b, float eps);
static void test_arena(void);
static void test_stack(void);
static void test_pool(void);
static void test_darray(void);
static void test_hashmap(void);
static void test_math(void);
static void test_rng(void);
static void test_draw(void);
static void dump_canvas(const GritDraw *d);

void
expect(int ok, const char *what)
{
    if (ok) {
        printf("  ok   %s\n", what);
        return;
    }
    printf("  FAIL %s\n", what);
    g_fails++;
}

int
nearly(float a, float b, float eps)
{
    float d;
    d = a - b;
    if (d < 0.0f)
        d = -d;
    return d <= eps;
}

void
test_arena(void)
{
    unsigned char buf[4096];
    GritArena a;
    void *p, *q, *r;
    int *n;
    printf("arena\n");
    grit_arena_init(&a, buf, sizeof buf);
    p = grit_arena_alloc(&a, 64);
    expect(p != NULL, "alloc");
    n = grit_arena_alloc_align(&a, sizeof (int), 8);
    expect(n != NULL, "alloc_align");
    *n = 7;
    q = grit_arena_resize(&a, n, sizeof (int), sizeof (int) * 4);
    expect(q == n, "resize last block in place");
    expect(((int *)q)[0] == 7, "resize kept value");
    r = grit_arena_resize_align(&a, p, 64, 128, 16);
    expect(r != NULL, "resize_align older block");
    grit_arena_free_all(&a);
    expect(grit_arena_alloc(&a, 32) != NULL, "alloc after free_all");
    grit_arena_destroy(&a);
    expect(a.buf == NULL, "destroy");
}

void
test_stack(void)
{
    unsigned char buf[4096];
    GritStack s;
    void *p, *q;
    printf("stack\n");
    grit_stack_init(&s, buf, sizeof buf);
    p = grit_stack_alloc(&s, 32, 8);
    q = grit_stack_alloc(&s, 16, 8);
    expect(p && q, "alloc");
    grit_stack_free(&s, q);
    expect(grit_stack_alloc(&s, 16, 8) != NULL, "alloc after LIFO free");
    grit_stack_free_all(&s);
    expect(s.offset == 0, "free_all");
    grit_stack_destroy(&s);
    expect(s.buf == NULL, "destroy");
}

void
test_pool(void)
{
    unsigned char buf[4096];
    GritPool p;
    void *a, *b;
    printf("pool\n");
    grit_pool_init(&p, buf, sizeof buf, 64, 8);
    a = grit_pool_alloc(&p);
    b = grit_pool_alloc(&p);
    expect(a && b && a != b, "alloc two chunks");
    grit_pool_free(&p, a);
    expect(grit_pool_alloc(&p) != NULL, "reuse freed chunk");
    grit_pool_free_all(&p);
    expect(grit_pool_alloc(&p) != NULL, "alloc after free_all");
    grit_pool_destroy(&p);
    expect(p.buf == NULL, "destroy");
}

void
test_darray(void)
{
    GritDArray a;
    int v, i;
    int *got;
    printf("darray\n");
    a = grit_darray_create(2, sizeof (int));
    expect(a.cap == 2 && a.len == 0, "create");
    for (i = 0; i < 20; ++i) {
        v = i * 3;
        grit_darray_push(&a, &v);
    }
    expect(a.len == 20 && a.cap >= 20, "push grew");
    got = grit_darray_get(&a, 5);
    expect(got && *got == 15, "get");
    grit_darray_pop(&a);
    expect(a.len == 19, "pop");
    grit_darray_destroy(&a);
    expect(a.data == NULL, "destroy");
}

void
test_hashmap(void)
{
    GritHashMap m;
    int k, v, i;
    int *got;
    printf("hashmap\n");
    m = grit_hashmap_create(4, sizeof (int), sizeof (int));
    expect(m.cap >= 4, "create");
    k = 3;
    v = 9;
    grit_hashmap_put(&m, &k, &v);
    got = grit_hashmap_get(&m, &k);
    expect(got && *got == 9, "put/get");
    v = 11;
    grit_hashmap_put(&m, &k, &v);
    got = grit_hashmap_get(&m, &k);
    expect(got && *got == 11, "overwrite");
    for (i = 0; i < 64; ++i) {
        k = i;
        v = i + 100;
        grit_hashmap_put(&m, &k, &v);
    }
    expect(m.len == 64, "grew");
    k = 20;
    got = grit_hashmap_get(&m, &k);
    expect(got && *got == 120, "get after grow");
    grit_hashmap_del(&m, &k);
    expect(grit_hashmap_get(&m, &k) == NULL, "del");
    expect(m.len == 63, "len after del");
    grit_hashmap_destroy(&m);
    expect(m.keys == NULL, "destroy");
}

void
test_math(void)
{
    float x, y, d;
    float a[3], b[3], c[3];
    float m[16], n[16];
    float eye[3], center[3], up[3], t[3];
    printf("math\n");
    x = 1.2f;
    grit_ceil(&x);
    expect(nearly(x, 2.0f, 0.01f), "ceil");
    x = 0.0f;
    grit_sinf(&x);
    expect(nearly(x, 0.0f, 0.02f), "sinf 0");
    x = 0.0f;
    grit_cosf(&x);
    expect(nearly(x, 1.0f, 0.05f), "cosf 0");
    y = 1.0f;
    grit_atan2f(&y, 0.0f);
    expect(nearly(y, GRIT_PI_HALF, 0.08f), "atan2");
    x = 0.0f;
    grit_lerpf(&x, 10.0f, 0.5f);
    expect(nearly(x, 5.0f, 0.001f), "lerpf");

    grit_vec2f(a, 3.0f, 4.0f);
    grit_vec2f_len(&d, a);
    expect(nearly(d, 5.0f, 0.05f), "vec2 len");
    grit_vec2f_norm(a);
    grit_vec2f_len(&d, a);
    expect(nearly(d, 1.0f, 0.05f), "vec2 norm");
    grit_vec2f(a, 1.0f, 2.0f);
    grit_vec2f(b, 3.0f, 4.0f);
    grit_vec2f_add(a, b);
    expect(nearly(a[0], 4.0f, 0.001f) && nearly(a[1], 6.0f, 0.001f), "vec2 add");
    grit_vec2f_sub(a, b);
    expect(nearly(a[0], 1.0f, 0.001f) && nearly(a[1], 2.0f, 0.001f), "vec2 sub");
    grit_vec2f_mult(a, 2.0f);
    expect(nearly(a[0], 2.0f, 0.001f) && nearly(a[1], 4.0f, 0.001f), "vec2 mult");
    grit_vec2f(a, 1.0f, 0.0f);
    grit_vec2f(b, 0.0f, 1.0f);
    grit_vec2f_dot(&d, a, b);
    expect(nearly(d, 0.0f, 0.001f), "vec2 dot");
    grit_vec2f_cross(&d, a, b);
    expect(nearly(d, 1.0f, 0.001f), "vec2 cross");
    grit_vec2f(a, 0.0f, 0.0f);
    grit_vec2f(b, 2.0f, 2.0f);
    grit_vec2f_lerp(a, b, 0.5f);
    expect(nearly(a[0], 1.0f, 0.001f) && nearly(a[1], 1.0f, 0.001f), "vec2 lerp");

    grit_vec3f(a, 1.0f, 0.0f, 0.0f);
    grit_vec3f(b, 0.0f, 1.0f, 0.0f);
    grit_vec3f_add(a, b);
    grit_vec3f_sub(a, b);
    grit_vec3f_mult(a, 2.0f);
    expect(nearly(a[0], 2.0f, 0.001f), "vec3 add/sub/mult");
    grit_vec3f(a, 1.0f, 0.0f, 0.0f);
    grit_vec3f_dot(&d, a, b);
    expect(nearly(d, 0.0f, 0.001f), "vec3 dot");
    grit_vec3f(c, 1.0f, 0.0f, 0.0f);
    grit_vec3f_cross(c, b);
    expect(nearly(c[2], 1.0f, 0.001f), "vec3 cross");
    grit_vec3f(a, 0.0f, 3.0f, 4.0f);
    grit_vec3f_len(&d, a);
    expect(nearly(d, 5.0f, 0.05f), "vec3 len");
    grit_vec3f_norm(a);
    grit_vec3f_len(&d, a);
    expect(nearly(d, 1.0f, 0.05f), "vec3 norm");
    grit_vec3f(a, 0.0f, 0.0f, 0.0f);
    grit_vec3f(b, 2.0f, 2.0f, 2.0f);
    grit_vec3f_lerp(a, b, 0.5f);
    expect(nearly(a[2], 1.0f, 0.001f), "vec3 lerp");

    grit_mat4_identity(m);
    expect(nearly(m[0], 1.0f, 0.001f) && nearly(m[15], 1.0f, 0.001f), "mat4 identity");
    grit_vec3f(t, 1.0f, 2.0f, 3.0f);
    grit_mat4_translate(n, t);
    grit_mat4_mul(m, n);
    expect(nearly(m[12], 1.0f, 0.001f) && nearly(m[14], 3.0f, 0.001f), "mat4 mul/translate");
    grit_mat4_translate_by(m, t);
    expect(nearly(m[12], 2.0f, 0.05f), "mat4 translate_by");
    grit_mat4_rotate_x(m, 0.3f);
    grit_mat4_rotate_x_by(m, 0.1f);
    grit_mat4_rotate_y(m, 0.2f);
    grit_mat4_rotate_y_by(m, 0.1f);
    grit_mat4_rotate_z(m, 0.4f);
    grit_mat4_perspective(m, 1.0f, 1.333f, 0.1f, 100.0f);
    expect(m[11] == -1.0f, "perspective");
    grit_mat4_ortho(m, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
    expect(nearly(m[0], 1.0f, 0.001f) && nearly(m[15], 1.0f, 0.001f), "ortho");
    grit_vec3f(eye, 0.0f, 0.0f, 3.0f);
    grit_vec3f(center, 0.0f, 0.0f, 0.0f);
    grit_vec3f(up, 0.0f, 1.0f, 0.0f);
    grit_mat4_lookat(m, eye, center, up);
    expect(nearly(m[15], 1.0f, 0.001f), "lookat");
}

void
test_rng(void)
{
    GritRng r;
    uint64_t a, b;
    uint32_t u;
    float f;
    printf("rng\n");
    grit_rng_seed(&r, 1);
    a = grit_rng_u64(&r);
    b = grit_rng_u64(&r);
    expect(a != b, "u64 advances");
    u = grit_rng_u32(&r);
    expect(u != 0 || a != 0, "u32");
    grit_rng_f32(&r, &f);
    expect(f >= 0.0f && f < 1.0f, "f32 in [0,1)");
    grit_rng_seed(&r, 1);
    expect(grit_rng_u64(&r) == a, "seed replay");
}

void
dump_canvas(const GritDraw *d)
{
    static const char ramp[] = " .:-=+*#%@";
    int x, y;
    uint32_t c;
    unsigned lum, idx;
    for (y = 0; y < d->h; ++y) {
        for (x = 0; x < d->w; ++x) {
            c = d->px[y * d->w + x];
            lum = ((c >> 16) & 0xFFu) + ((c >> 8) & 0xFFu) + (c & 0xFFu);
            idx = lum * (sizeof ramp - 2) / (255u * 3u);
            putchar(ramp[idx]);
        }
        putchar('\n');
    }
}

void
test_draw(void)
{
    uint32_t px[CANVAS_W * CANVAS_H];
    uint32_t stamp[4];
    GritDraw d;
    printf("draw\n");
    grit_draw_begin(&d, px, CANVAS_W, CANVAS_H);
    grit_draw_clear(&d, 0xFF101018);
    grit_draw_rect(&d, 2, 2, 10, 6, 0xFF335577);
    grit_draw_rect_gradient(&d, 14, 2, 12, 6, 0xFFFF0000, 0xFF00FF00, 0xFF0000FF, 0xFFFFFFFF);
    grit_draw_tri(&d, 30, 2, 44, 2, 37, 10, 0xFFFFFF00, 0xFFFF00FF, 0xFF00FFFF);
    grit_draw_line(&d, 2, 10, 30, 16, 1, 0xFFFFFFFF, 0xFF4488FF);
    grit_draw_circle(&d, 40, 13, 3, 0xFFFFAA44);
    grit_draw_pixel(&d, 1, 1, 0xFFFFFFFF);
    expect(px[1 * CANVAS_W + 1] == 0xFFFFFFFF, "pixel");
    stamp[0] = stamp[1] = stamp[2] = stamp[3] = 0xFFAAAAAA;
    grit_draw_blit(&d, 0, CANVAS_H - 2, stamp, 2, 2);
    grit_draw_clip(&d, 0, 0, 4, 4);
    grit_draw_rect(&d, 0, 0, 20, 20, 0xFF222222);
    grit_draw_clip_reset(&d);
    expect(px[3 * CANVAS_W + 3] != 0xFF101018, "rect wrote");
    dump_canvas(&d);
}

int
main(void)
{
    test_arena();
    test_stack();
    test_pool();
    test_darray();
    test_hashmap();
    test_math();
    test_rng();
    test_draw();
    if (g_fails) {
        printf("\n%d failed\n", g_fails);
        return 1;
    }
    printf("\nall ok\n");
    return 0;
}
