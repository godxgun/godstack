/*
 * snake — Peak window/input, Grit state/math, Rend instanced cubes
 * 0.1.0 - first cut
 * 0.1.1 - frame the board; slither / stretch / eat pop
 * 0.1.2 - auto-fit camera to the window
 */

#include "rend.h"
#include "peak.c"
#include "rend.c"
#include "grit.h"
#include "grit.c"
#include "demos/headless.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SNAKE_W 16
#define SNAKE_H 16
#define SNAKE_MAX (SNAKE_W * SNAKE_H)
#define SNAKE_STEP_START 0.16f
#define SNAKE_STEP_MIN 0.07f

typedef struct SnakeCell {
    int x;
    int y;
    int px;
    int py;
} SnakeCell;

typedef struct SnakeGame {
    GritDArray body;
    GritRng rng;
    int dx;
    int dy;
    int next_dx;
    int next_dy;
    int food_x;
    int food_y;
    int pop_x;
    int pop_y;
    int popping;
    int alive;
    int win;
    float dead_t;
    float eat_t;
} SnakeGame;

typedef struct SnakeVertex {
    float pos[3];
    float shade;
} SnakeVertex;

typedef struct SnakeInstance {
    float pos[4];
    float scale[4];
    float color[4];
} SnakeInstance;

typedef struct SnakePush {
    float mvp[16];
} SnakePush;

static uint8_t *snake_load_spv(const char *a, const char *b, unsigned long *size);
static void snake_reset(SnakeGame *g);
static int snake_occupied(SnakeGame *g, int x, int y);
static void snake_place_food(SnakeGame *g);
static void snake_try_turn(SnakeGame *g, int dx, int dy);
static void snake_drop_tail(SnakeGame *g);
static void snake_tick(SnakeGame *g);
static void snake_emit(SnakeInstance *out, float x, float y, float z, float sx, float sy, float sz, float r, float g, float b);
static void snake_cell_pos(const SnakeCell *c, float ease, float *x, float *z);
static uint32_t snake_fill_instances(SnakeGame *g, SnakeInstance *out, float time, float move_t);
static void snake_mvp(float *mvp, uint32_t fb_w, uint32_t fb_h);

static const SnakeVertex snake_cube_verts[] = {
    {{0.0f, 1.0f, 0.0f}, 1.00f}, {{1.0f, 1.0f, 0.0f}, 1.00f}, {{1.0f, 1.0f, 1.0f}, 1.00f}, {{0.0f, 1.0f, 1.0f}, 1.00f},
    {{0.0f, 0.0f, 0.0f}, 0.35f}, {{0.0f, 0.0f, 1.0f}, 0.35f}, {{1.0f, 0.0f, 1.0f}, 0.35f}, {{1.0f, 0.0f, 0.0f}, 0.35f},
    {{0.0f, 0.0f, 1.0f}, 0.78f}, {{1.0f, 0.0f, 1.0f}, 0.78f}, {{1.0f, 1.0f, 1.0f}, 0.78f}, {{0.0f, 1.0f, 1.0f}, 0.78f},
    {{1.0f, 0.0f, 0.0f}, 0.55f}, {{0.0f, 0.0f, 0.0f}, 0.55f}, {{0.0f, 1.0f, 0.0f}, 0.55f}, {{1.0f, 1.0f, 0.0f}, 0.55f},
    {{1.0f, 0.0f, 0.0f}, 0.88f}, {{1.0f, 1.0f, 0.0f}, 0.88f}, {{1.0f, 1.0f, 1.0f}, 0.88f}, {{1.0f, 0.0f, 1.0f}, 0.88f},
    {{0.0f, 0.0f, 0.0f}, 0.62f}, {{0.0f, 0.0f, 1.0f}, 0.62f}, {{0.0f, 1.0f, 1.0f}, 0.62f}, {{0.0f, 1.0f, 0.0f}, 0.62f},
};

static const uint16_t snake_cube_inds[] = {
    0, 1, 2, 0, 2, 3,
    4, 5, 6, 4, 6, 7,
    8, 9, 10, 8, 10, 11,
    12, 13, 14, 12, 14, 15,
    16, 17, 18, 16, 18, 19,
    20, 21, 22, 20, 22, 23,
};

uint8_t *
snake_load_spv(const char *a, const char *b, unsigned long *size)
{
    uint8_t *p;

    p = peak_file_alloc(a, size);
    if (p)
        return p;
    return peak_file_alloc(b, size);
}

void
snake_reset(SnakeGame *g)
{
    SnakeCell c;
    int i;

    g->body.len = 0;
    g->dx = 1;
    g->dy = 0;
    g->next_dx = 1;
    g->next_dy = 0;
    g->alive = 1;
    g->win = 0;
    g->popping = 0;
    g->dead_t = 0.0f;
    g->eat_t = 0.0f;
    c.y = SNAKE_H / 2;
    c.py = c.y;
    for (i = 0; i < 3; ++i) {
        c.x = SNAKE_W / 2 - 2 + i;
        c.px = c.x;
        grit_darray_push(&g->body, &c);
    }
    snake_place_food(g);
}

int
snake_occupied(SnakeGame *g, int x, int y)
{
    size_t i;
    SnakeCell *c;

    for (i = 0; i < g->body.len; ++i) {
        c = grit_darray_get(&g->body, i);
        if (c && c->x == x && c->y == y)
            return 1;
    }
    return 0;
}

void
snake_place_food(SnakeGame *g)
{
    int x;
    int y;
    int n;

    if (g->body.len >= SNAKE_MAX) {
        g->win = 1;
        g->alive = 0;
        return;
    }
    n = 0;
    do {
        x = (int)(grit_rng_u32(&g->rng) % (uint32_t)SNAKE_W);
        y = (int)(grit_rng_u32(&g->rng) % (uint32_t)SNAKE_H);
        n++;
    } while (snake_occupied(g, x, y) && n < SNAKE_MAX * 4);
    g->food_x = x;
    g->food_y = y;
}

void
snake_try_turn(SnakeGame *g, int dx, int dy)
{
    if (dx == -g->dx && dy == -g->dy)
        return;
    g->next_dx = dx;
    g->next_dy = dy;
}

void
snake_drop_tail(SnakeGame *g)
{
    size_t i;
    SnakeCell *dst;
    SnakeCell *src;

    if (g->body.len == 0)
        return;
    for (i = 0; i + 1 < g->body.len; ++i) {
        dst = grit_darray_get(&g->body, i);
        src = grit_darray_get(&g->body, i + 1);
        *dst = *src;
    }
    grit_darray_pop(&g->body);
}

void
snake_tick(SnakeGame *g)
{
    SnakeCell old[SNAKE_MAX];
    SnakeCell *cell;
    SnakeCell *head;
    SnakeCell next;
    size_t old_len;
    size_t i;
    int eat;

    if (!g->alive)
        return;
    g->dx = g->next_dx;
    g->dy = g->next_dy;
    old_len = g->body.len;
    for (i = 0; i < old_len; ++i)
        old[i] = *(SnakeCell *)grit_darray_get(&g->body, i);
    head = grit_darray_get(&g->body, old_len - 1);
    next.x = head->x + g->dx;
    next.y = head->y + g->dy;
    next.px = head->x;
    next.py = head->y;
    if (next.x < 0 || next.x >= SNAKE_W || next.y < 0 || next.y >= SNAKE_H) {
        g->alive = 0;
        return;
    }
    eat = (next.x == g->food_x && next.y == g->food_y);
    if (!eat && snake_occupied(g, next.x, next.y)) {
        g->alive = 0;
        return;
    }
    if (eat) {
        g->pop_x = g->food_x;
        g->pop_y = g->food_y;
        g->eat_t = 1.0f;
        g->popping = 1;
        grit_darray_push(&g->body, &next);
        cell = grit_darray_get(&g->body, 0);
        cell->px = old[0].x;
        cell->py = old[0].y;
        for (i = 1; i < g->body.len; ++i) {
            cell = grit_darray_get(&g->body, i);
            cell->px = old[i - 1].x;
            cell->py = old[i - 1].y;
        }
        snake_place_food(g);
        return;
    }
    snake_drop_tail(g);
    grit_darray_push(&g->body, &next);
    for (i = 0; i < g->body.len; ++i) {
        cell = grit_darray_get(&g->body, i);
        cell->px = old[i].x;
        cell->py = old[i].y;
    }
}

void
snake_emit(SnakeInstance *out, float x, float y, float z, float sx, float sy, float sz, float r, float g, float b)
{
    out->pos[0] = x;
    out->pos[1] = y;
    out->pos[2] = z;
    out->pos[3] = 0.0f;
    out->scale[0] = sx;
    out->scale[1] = sy;
    out->scale[2] = sz;
    out->scale[3] = 0.0f;
    out->color[0] = r;
    out->color[1] = g;
    out->color[2] = b;
    out->color[3] = 1.0f;
}

void
snake_cell_pos(const SnakeCell *c, float ease, float *x, float *z)
{
    *x = (float)c->px;
    *z = (float)c->py;
    grit_lerpf(x, (float)c->x, ease);
    grit_lerpf(z, (float)c->y, ease);
}

uint32_t
snake_fill_instances(SnakeGame *g, SnakeInstance *out, float time, float move_t)
{
    uint32_t n;
    int x;
    int y;
    int is_head;
    size_t i;
    SnakeCell *c;
    float ease;
    float fx;
    float fz;
    float mdx;
    float mdz;
    float stretch;
    float base;
    float sx;
    float sy;
    float sz;
    float wy;
    float wave;
    float dead;
    float t;
    float bob;
    float pulse;
    float shade;
    float cr;
    float cg;
    float cb;
    float pop;

    ease = move_t;
    if (ease < 0.0f)
        ease = 0.0f;
    if (ease > 1.0f)
        ease = 1.0f;
    ease = ease * ease * (3.0f - 2.0f * ease);
    dead = 1.0f / (1.0f + g->dead_t * 3.2f);

    n = 0;
    snake_emit(&out[n++], -0.45f, -0.22f, -0.45f, (float)SNAKE_W + 0.90f, 0.22f, (float)SNAKE_H + 0.90f, 0.06f, 0.07f, 0.08f);
    for (y = 0; y < SNAKE_H; ++y) {
        for (x = 0; x < SNAKE_W; ++x) {
            if (((x + y) & 1) == 0)
                snake_emit(&out[n++], (float)x + 0.04f, 0.0f, (float)y + 0.04f, 0.92f, 0.10f, 0.92f, 0.16f, 0.18f, 0.20f);
            else
                snake_emit(&out[n++], (float)x + 0.04f, 0.0f, (float)y + 0.04f, 0.92f, 0.10f, 0.92f, 0.11f, 0.12f, 0.14f);
        }
    }
    for (i = 0; i < g->body.len; ++i) {
        c = grit_darray_get(&g->body, i);
        snake_cell_pos(c, ease, &fx, &fz);
        is_head = (i + 1 == g->body.len);
        shade = (g->body.len <= 1) ? 1.0f : (float)i / (float)(g->body.len - 1);
        if (g->alive) {
            cr = 0.20f + 0.25f * shade;
            cg = 0.55f + 0.40f * shade;
            cb = 0.28f + 0.15f * shade;
        } else {
            cr = 0.35f + 0.15f * shade;
            cg = 0.36f + 0.10f * shade;
            cb = 0.38f + 0.08f * shade;
        }
        base = is_head ? 0.86f : 0.72f;
        if (is_head)
            base += 0.10f * g->eat_t;
        mdx = (float)(c->x - c->px);
        mdz = (float)(c->y - c->py);
        if (mdx < 0.0f)
            mdx = -mdx;
        if (mdz < 0.0f)
            mdz = -mdz;
        stretch = 1.0f - 4.0f * (ease - 0.5f) * (ease - 0.5f);
        if (stretch < 0.0f)
            stretch = 0.0f;
        sx = base + 0.28f * mdx * stretch;
        sz = base + 0.28f * mdz * stretch;
        sy = (is_head ? 0.92f : 0.68f) - 0.12f * stretch;
        wave = time * 11.0f + (float)i * 0.75f;
        grit_sinf(&wave);
        wy = 0.12f + 0.05f * wave * (g->alive ? 1.0f : 0.0f);
        sy *= dead;
        wy *= dead;
        snake_emit(&out[n++], fx + 0.5f - sx * 0.5f, wy, fz + 0.5f - sz * 0.5f, sx, sy, sz, cr, cg, cb);
    }
    if (!g->win) {
        t = time * 4.0f;
        grit_sinf(&t);
        bob = 0.18f + 0.08f * t;
        pulse = time * 3.0f;
        grit_sinf(&pulse);
        pulse = 0.75f + 0.25f * pulse;
        snake_emit(&out[n++], (float)g->food_x + 0.22f, bob, (float)g->food_y + 0.22f, 0.56f, 0.56f, 0.56f, 0.95f * pulse, 0.22f, 0.24f);
    }
    if (g->popping && g->eat_t > 0.0f) {
        pop = g->eat_t;
        snake_emit(&out[n++], (float)g->pop_x + 0.5f - 0.28f * pop, 0.18f * pop, (float)g->pop_y + 0.5f - 0.28f * pop, 0.56f * pop, 0.56f * pop, 0.56f * pop, 1.0f, 0.55f * pop, 0.20f);
    }
    return n;
}

void
snake_mvp(float *mvp, uint32_t fb_w, uint32_t fb_h)
{
    float view[16];
    float proj[16];
    float t[3];
    float p[3];
    float q[3];
    float aspect;
    float tilt;
    float fov;
    float fill;
    float half;
    float s;
    float c;
    float tan_half;
    float dist;
    float need;
    float cx;
    float cy;
    float cz;
    int i;
    int ix;
    int iy;
    int iz;

    /* NOTE: fit the board AABB to ~85% of the viewport. */
    tilt = 0.62f;
    fov = 0.70f;
    fill = 0.86f;
    cx = (float)SNAKE_W * 0.5f;
    cy = 0.30f;
    cz = (float)SNAKE_H * 0.5f;
    aspect = (fb_h == 0) ? 1.0f : (float)fb_w / (float)fb_h;
    half = fov * 0.5f;
    s = half;
    c = half;
    grit_sinf(&s);
    grit_cosf(&c);
    tan_half = s / c;
    s = tilt;
    c = tilt;
    grit_sinf(&s);
    grit_cosf(&c);
    dist = 2.0f;
    for (ix = 0; ix < 2; ++ix) {
        for (iy = 0; iy < 2; ++iy) {
            for (iz = 0; iz < 2; ++iz) {
                p[0] = (ix ? (float)SNAKE_W + 0.5f : -0.5f) - cx;
                p[1] = (iy ? 1.2f : -0.25f) - cy;
                p[2] = (iz ? (float)SNAKE_H + 0.5f : -0.5f) - cz;
                q[0] = p[0];
                q[1] = c * p[1] - s * p[2];
                q[2] = s * p[1] + c * p[2];
                if (q[0] < 0.0f)
                    q[0] = -q[0];
                if (q[1] < 0.0f)
                    q[1] = -q[1];
                need = q[0] / (fill * aspect * tan_half);
                if (q[1] / (fill * tan_half) > need)
                    need = q[1] / (fill * tan_half);
                if (q[2] + need > dist)
                    dist = q[2] + need;
            }
        }
    }
    grit_mat4_identity(view);
    grit_vec3f(t, 0.0f, 0.0f, -dist);
    grit_mat4_translate_by(view, t);
    grit_mat4_rotate_x_by(view, tilt);
    grit_vec3f(t, -cx, -cy, -cz);
    grit_mat4_translate_by(view, t);
    grit_mat4_perspective(proj, fov, aspect, 0.1f, dist + 40.0f);
    proj[5] *= -1.0f;
    for (i = 0; i < 16; ++i)
        mvp[i] = proj[i];
    grit_mat4_mul(mvp, view);
}

int
main(int argc, char **argv)
{
    PeakWindow win;
    PeakEvent ev;
    RendBindingInfo bind_info;
    RendRenderer renderer;
    RendBuffer vert_buf;
    RendBuffer ind_buf;
    RendBuffer inst_buf;
    RendPipeline pipeline;
    RendVertexBinding binds[2];
    RendVertexAttributes attrs[5];
    RendPushConstantInfo pc_info;
    SnakeGame game;
    SnakeInstance instances[SNAKE_MAX * 2 + 8];
    SnakePush pc;
    uint8_t *vert_spv;
    uint8_t *frag_spv;
    unsigned long vert_bytes;
    unsigned long frag_bytes;
    uint32_t inst_count;
    uint64_t t0;
    uint64_t frame_t;
    uint64_t now;
    float acc;
    float step;
    float time;
    float dt;
    float move_t;
    int running;
    int headless;
    int frames;
    int frame_i;
    const char *ppm;
    uint32_t width;
    uint32_t height;

    width = 960;
    height = 720;
    memset(&win, 0, sizeof win);
    headless = headless_parse(argc, argv, &frames, &ppm);

    if (!headless) {
        if (!peak_init()) {
            PFATAL("Failed to init Peak!");
            return 1;
        }

        win = peak_window_open("snake", width, height, 0);
        if (!win.running) {
            PFATAL("Failed to open a window!");
            peak_quit();
            return 1;
        }
        width = win.width;
        height = win.height;
    }

    memset(&bind_info, 0, sizeof bind_info);
    if (headless)
        renderer = rend_renderer_create_offscreen(width, height, REND_FORMAT_R8G8B8A8_UNORM, REND_BACKEND_AUTO, &bind_info);
    else
        renderer = rend_renderer_create(&win, REND_BACKEND_AUTO, NULL, true, &bind_info);
    if (!renderer) {
        PFATAL("Failed to create renderer!");
        if (!headless) {
            peak_window_close(&win);
            peak_quit();
        }
        return 1;
    }

    vert_bytes = 0;
    frag_bytes = 0;
    vert_spv = snake_load_spv("snake.vert.spv", "demos/snake/snake.vert.spv", &vert_bytes);
    frag_spv = snake_load_spv("snake.frag.spv", "demos/snake/snake.frag.spv", &frag_bytes);
    if (!vert_spv || !frag_spv) {
        PFATAL("Failed to load snake shaders!");
        return 1;
    }

    vert_buf = rend_buffer_create(renderer, sizeof snake_cube_verts, REND_BUFFER_VERTEX, true);
    ind_buf = rend_buffer_create(renderer, sizeof snake_cube_inds, REND_BUFFER_INDEX, true);
    inst_buf = rend_buffer_create(renderer, sizeof instances, REND_BUFFER_VERTEX, false);
    rend_buffer_write(renderer, &vert_buf, snake_cube_verts, sizeof snake_cube_verts, 0);
    rend_buffer_write(renderer, &ind_buf, snake_cube_inds, sizeof snake_cube_inds, 0);

    binds[0].binding = 0;
    binds[0].stride = sizeof (SnakeVertex);
    binds[0].input_rate = REND_INPUT_RATE_VERTEX;
    binds[1].binding = 1;
    binds[1].stride = sizeof (SnakeInstance);
    binds[1].input_rate = REND_INPUT_RATE_INSTANCE;

    attrs[0].binding = 0;
    attrs[0].location = 0;
    attrs[0].format = REND_FORMAT_3_SFLOAT32;
    attrs[0].offset = offsetof(SnakeVertex, pos);
    attrs[1].binding = 0;
    attrs[1].location = 1;
    attrs[1].format = REND_FORMAT_1_SFLOAT32;
    attrs[1].offset = offsetof(SnakeVertex, shade);
    attrs[2].binding = 1;
    attrs[2].location = 2;
    attrs[2].format = REND_FORMAT_3_SFLOAT32;
    attrs[2].offset = offsetof(SnakeInstance, pos);
    attrs[3].binding = 1;
    attrs[3].location = 3;
    attrs[3].format = REND_FORMAT_3_SFLOAT32;
    attrs[3].offset = offsetof(SnakeInstance, scale);
    attrs[4].binding = 1;
    attrs[4].location = 4;
    attrs[4].format = REND_FORMAT_3_SFLOAT32;
    attrs[4].offset = offsetof(SnakeInstance, color);

    pc_info.offset = 0;
    pc_info.size = sizeof (SnakePush);
    pipeline = rend_pipeline_create_graphics_spirv(
        renderer,
        vert_spv, vert_bytes,
        frag_spv, frag_bytes,
        binds, 2,
        attrs, 5,
        &pc_info, 1,
        REND_POLYGON_MODE_FILL,
        REND_CULL_MODE_NONE,
        REND_TOPOLOGY_TRIANGLE_LIST,
        REND_FORMAT_UNDEFINED,
        true);
    free(vert_spv);
    free(frag_spv);
    if (!pipeline) {
        PFATAL("Failed to create snake pipeline!");
        return 1;
    }

    game.body = grit_darray_create(16, sizeof (SnakeCell));
    grit_rng_seed(&game.rng, headless ? 1ull : peak_get_time());
    snake_reset(&game);

    running = 1;
    acc = 0.0f;
    step = SNAKE_STEP_START;
    t0 = peak_get_time();
    frame_t = t0;
    frame_i = 0;
    while (running) {
        if (headless) {
            if (frame_i >= frames)
                break;
            dt = 1.0f / 60.0f;
            time = (float)frame_i * dt;
        } else {
            now = peak_get_time();
            dt = (float)(now - frame_t) / (float)NANOS_PER_SEC;
            frame_t = now;
            if (dt > 0.05f)
                dt = 0.05f;
            time = (float)(now - t0) / (float)NANOS_PER_SEC;
        }

        while (!headless && peak_window_epoll(&win, &ev)) {
            if (ev.type == PEAK_EVENT_WINDOW_CLOSE) {
                running = 0;
                break;
            }
            if (ev.type == PEAK_EVENT_KEY_DOWN) {
                switch (ev.key.key) {
                case PEAK_KEY_ESCAPE:
                    running = 0;
                    break;
                case PEAK_KEY_W: /* FALLTHROUGH */
                case PEAK_KEY_UP:
                    snake_try_turn(&game, 0, -1);
                    break;
                case PEAK_KEY_S: /* FALLTHROUGH */
                case PEAK_KEY_DOWN:
                    snake_try_turn(&game, 0, 1);
                    break;
                case PEAK_KEY_A: /* FALLTHROUGH */
                case PEAK_KEY_LEFT:
                    snake_try_turn(&game, -1, 0);
                    break;
                case PEAK_KEY_D: /* FALLTHROUGH */
                case PEAK_KEY_RIGHT:
                    snake_try_turn(&game, 1, 0);
                    break;
                case PEAK_KEY_SPACE: /* FALLTHROUGH */
                case PEAK_KEY_ENTER:
                    if (!game.alive) {
                        snake_reset(&game);
                        acc = 0.0f;
                        step = SNAKE_STEP_START;
                    }
                    break;
                default:
                    break;
                }
            }
        }

        if (game.alive) {
            acc += dt;
            step = SNAKE_STEP_START - (float)game.body.len * 0.003f;
            if (step < SNAKE_STEP_MIN)
                step = SNAKE_STEP_MIN;
            while (acc >= step) {
                acc -= step;
                snake_tick(&game);
            }
            move_t = (step > 0.0f) ? (acc / step) : 1.0f;
        } else {
            move_t = 1.0f;
            game.dead_t += dt;
        }
        if (game.eat_t > 0.0f) {
            game.eat_t -= dt * 4.0f;
            if (game.eat_t < 0.0f)
                game.eat_t = 0.0f;
        }

        inst_count = snake_fill_instances(&game, instances, time, move_t);
        rend_buffer_write(renderer, &inst_buf, instances, inst_count * sizeof *instances, 0);
        snake_mvp(pc.mvp, width, height);

        if (rend_renderer_frame_begin(renderer)) {
            rend_renderer_render_pass_begin(renderer, 0.04f, 0.05f, 0.06f, 1.0f);
            rend_pipeline_bind(pipeline);
            rend_pipeline_push_constants(pipeline, &pc, sizeof pc);
            rend_pipeline_bind_vertex_buffer(pipeline, 0, vert_buf, 0);
            rend_pipeline_bind_vertex_buffer(pipeline, 1, inst_buf, 0);
            rend_pipeline_bind_index_buffer(pipeline, ind_buf, 0, REND_INDEX_UINT16);
            rend_pipeline_draw_indexed(pipeline, (uint32_t)(sizeof snake_cube_inds / sizeof snake_cube_inds[0]), 0, 0, inst_count);
            rend_renderer_render_pass_end(renderer);
            rend_renderer_frame_end(renderer, NULL);
        }
        frame_i++;
    }

    if (headless && !headless_finish(renderer, width, height, REND_FORMAT_R8G8B8A8_UNORM, ppm))
        running = 0;

    grit_darray_destroy(&game.body);
    rend_buffer_destroy(&vert_buf);
    rend_buffer_destroy(&ind_buf);
    rend_buffer_destroy(&inst_buf);
    rend_renderer_destroy(renderer);
    if (!headless) {
        peak_window_close(&win);
        peak_quit();
    }
    rend_quit();
    return (headless && running == 0) ? 1 : 0;
}
