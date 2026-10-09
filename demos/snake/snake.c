/*
 * snake — Peak window/input, Grit state/math, Rend2 vertex-pulled cubes.
 * 0.1.0 - first cut; 0.1.1 - board framing and animation; 0.1.2 - camera fit.
 */
#define _POSIX_C_SOURCE 200809L
#ifndef PEAK_VULKAN
#define PEAK_VULKAN
#endif
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Peak defines the native Vulkan platform macros before Vulkan is included. */
#include "Peak.h"
#include "../../Rend2.h"
#define GRIT_IMPLEMENTATION
#include "Grit.h"
#include "snake_render.h"

#define PEAK_IMPLEMENTATION
#include "Peak.h"
#define REND2_IMPLEMENTATION
#include "../../Rend2.h"
#include "snake_render.c"

#define SNAKE_W 16
#define SNAKE_H 16
#define SNAKE_MAX (SNAKE_W * SNAKE_H)
#define SNAKE_STEP_START 0.16f
#define SNAKE_STEP_MIN 0.07f

typedef struct SnakeCell {
	int x, y, px, py;
} SnakeCell;
typedef struct SnakeGame {
	GritDArray body;
	GritRng rng;
	int dx, dy, next_dx, next_dy;
	int food_x, food_y, pop_x, pop_y;
	int popping, alive, win;
	float dead_t, eat_t;
} SnakeGame;
typedef struct SnakeVertex {
	float pos[3];
	float shade;
} SnakeVertex;
typedef struct SnakeInstance {
	float pos[4], scale[4], color[4];
} SnakeInstance;

static const SnakeVertex snake_cube_verts[] = {
    {{0, 1, 0}, 1},
    {{1, 1, 0}, 1},
    {{1, 1, 1}, 1},
    {{0, 1, 1}, 1},
    {{0, 0, 0}, .35f},
    {{0, 0, 1}, .35f},
    {{1, 0, 1}, .35f},
    {{1, 0, 0}, .35f},
    {{0, 0, 1}, .78f},
    {{1, 0, 1}, .78f},
    {{1, 1, 1}, .78f},
    {{0, 1, 1}, .78f},
    {{1, 0, 0}, .55f},
    {{0, 0, 0}, .55f},
    {{0, 1, 0}, .55f},
    {{1, 1, 0}, .55f},
    {{1, 0, 0}, .88f},
    {{1, 1, 0}, .88f},
    {{1, 1, 1}, .88f},
    {{1, 0, 1}, .88f},
    {{0, 0, 0}, .62f},
    {{0, 0, 1}, .62f},
    {{0, 1, 1}, .62f},
    {{0, 1, 0}, .62f},
};
typedef char SnakeVertexCountMustMatch[(sizeof snake_cube_verts / sizeof snake_cube_verts[0] == SNAKE_GPU_VERTEX_COUNT) ? 1 : -1];

static const uint16_t snake_cube_inds[] = {
    0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7, 8, 9, 10, 8, 10, 11,
    12, 13, 14, 12, 14, 15, 16, 17, 18, 16, 18, 19, 20, 21, 22, 20, 22, 23};

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
static int snake_parse_uint(const char *text, unsigned long maximum, unsigned long *out);
static int snake_options(int argc, char **argv, int *headless, int *frames, int *frames_set, const char **ppm, uint32_t *device);
static uint8_t *snake_load_spv(const char *path, size_t *size);
static int snake_ppm_finish(SnakeRender *render, const char *ppm);

static void
snake_reset(SnakeGame *g)
{
	SnakeCell c;
	int i;
	g->body.len = 0;
	g->dx = g->next_dx = 1;
	g->dy = g->next_dy = 0;
	g->alive = 1;
	g->win = g->popping = 0;
	g->dead_t = g->eat_t = 0.0f;
	c.y = c.py = SNAKE_H / 2;
	for (i = 0; i < 3; ++i) {
		c.x = c.px = SNAKE_W / 2 - 2 + i;
		grit_darray_push(&g->body, &c);
	}
	snake_place_food(g);
}

static int
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

static void
snake_place_food(SnakeGame *g)
{
	int n;
	if (g->body.len >= SNAKE_MAX) {
		g->win = 1;
		g->alive = 0;
		return;
	}
	n = 0;
	do {
		g->food_x = (int)(grit_rng_u32(&g->rng) % SNAKE_W);
		g->food_y = (int)(grit_rng_u32(&g->rng) % SNAKE_H);
		++n;
	} while (snake_occupied(g, g->food_x, g->food_y) && n < SNAKE_MAX * 4);
}

static void
snake_try_turn(SnakeGame *g, int dx, int dy)
{
	if (dx == -g->dx && dy == -g->dy)
		return;
	g->next_dx = dx;
	g->next_dy = dy;
}

static void
snake_drop_tail(SnakeGame *g)
{
	size_t i;
	if (!g->body.len)
		return;
	for (i = 0; i + 1 < g->body.len; ++i)
		*(SnakeCell *)grit_darray_get(&g->body, i) = *(SnakeCell *)grit_darray_get(&g->body, i + 1);
	grit_darray_pop(&g->body);
}

static void
snake_tick(SnakeGame *g)
{
	SnakeCell old[SNAKE_MAX], next, *cell, *head;
	size_t n, i;
	int eat;
	if (!g->alive)
		return;
	g->dx = g->next_dx;
	g->dy = g->next_dy;
	n = g->body.len;
	for (i = 0; i < n; ++i)
		old[i] = *(SnakeCell *)grit_darray_get(&g->body, i);
	head = grit_darray_get(&g->body, n - 1);
	next.x = head->x + g->dx;
	next.y = head->y + g->dy;
	next.px = head->x;
	next.py = head->y;
	if (next.x < 0 || next.x >= SNAKE_W || next.y < 0 || next.y >= SNAKE_H) {
		g->alive = 0;
		return;
	}
	eat = next.x == g->food_x && next.y == g->food_y;
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

static void
snake_emit(SnakeInstance *out, float x, float y, float z, float sx, float sy, float sz, float r, float g, float b)
{
	out->pos[0] = x;
	out->pos[1] = y;
	out->pos[2] = z;
	out->pos[3] = 0;
	out->scale[0] = sx;
	out->scale[1] = sy;
	out->scale[2] = sz;
	out->scale[3] = 0;
	out->color[0] = r;
	out->color[1] = g;
	out->color[2] = b;
	out->color[3] = 1;
}

static void
snake_cell_pos(const SnakeCell *c, float ease, float *x, float *z)
{
	*x = (float)c->px;
	*z = (float)c->py;
	grit_lerpf(x, (float)c->x, ease);
	grit_lerpf(z, (float)c->y, ease);
}

static uint32_t
snake_fill_instances(SnakeGame *g, SnakeInstance *out, float time, float move_t)
{
	uint32_t n = 0;
	int x, y, is_head;
	size_t i;
	SnakeCell *c;
	float ease, fx, fz, mdx, mdz, stretch, base, sx, sy, sz, wy, wave, dead;
	float t, bob, pulse, shade, cr, cg, cb, pop;
	ease = move_t < 0 ? 0 : move_t > 1 ? 1
	                                   : move_t;
	ease = ease * ease * (3.0f - 2.0f * ease);
	dead = 1.0f / (1.0f + g->dead_t * 3.2f);
	snake_emit(&out[n++], -.45f, -.22f, -.45f, SNAKE_W + .90f, .22f, SNAKE_H + .90f, .06f, .07f, .08f);
	for (y = 0; y < SNAKE_H; ++y)
		for (x = 0; x < SNAKE_W; ++x)
			if (((x + y) & 1) == 0)
				snake_emit(&out[n++], x + .04f, 0, y + .04f, .92f, .10f, .92f, .16f, .18f, .20f);
			else
				snake_emit(&out[n++], x + .04f, 0, y + .04f, .92f, .10f, .92f, .11f, .12f, .14f);
	for (i = 0; i < g->body.len; ++i) {
		c = grit_darray_get(&g->body, i);
		snake_cell_pos(c, ease, &fx, &fz);
		is_head = i + 1 == g->body.len;
		shade = g->body.len <= 1 ? 1 : (float)i / (float)(g->body.len - 1);
		if (g->alive) {
			cr = .20f + .25f * shade;
			cg = .55f + .40f * shade;
			cb = .28f + .15f * shade;
		} else {
			cr = .35f + .15f * shade;
			cg = .36f + .10f * shade;
			cb = .38f + .08f * shade;
		}
		base = is_head ? .86f : .72f;
		if (is_head)
			base += .10f * g->eat_t;
		mdx = (float)(c->x - c->px);
		mdz = (float)(c->y - c->py);
		if (mdx < 0)
			mdx = -mdx;
		if (mdz < 0)
			mdz = -mdz;
		stretch = 1 - 4 * (ease - .5f) * (ease - .5f);
		if (stretch < 0)
			stretch = 0;
		sx = base + .28f * mdx * stretch;
		sz = base + .28f * mdz * stretch;
		sy = (is_head ? .92f : .68f) - .12f * stretch;
		wave = time * 11 + (float)i * .75f;
		grit_sinf(&wave);
		wy = .12f + .05f * wave * (g->alive ? 1 : 0);
		sy *= dead;
		wy *= dead;
		snake_emit(&out[n++], fx + .5f - sx * .5f, wy, fz + .5f - sz * .5f, sx, sy, sz, cr, cg, cb);
	}
	if (!g->win) {
		t = time * 4;
		grit_sinf(&t);
		bob = .18f + .08f * t;
		pulse = time * 3;
		grit_sinf(&pulse);
		pulse = .75f + .25f * pulse;
		snake_emit(&out[n++], g->food_x + .22f, bob, g->food_y + .22f, .56f, .56f, .56f, .95f * pulse, .22f, .24f);
	}
	if (g->popping && g->eat_t > 0) {
		pop = g->eat_t;
		snake_emit(&out[n++], g->pop_x + .5f - .28f * pop, .18f * pop, g->pop_y + .5f - .28f * pop, .56f * pop, .56f * pop, .56f * pop, 1, .55f * pop, .20f);
	}
	return n;
}

static void
snake_mvp(float *mvp, uint32_t fb_w, uint32_t fb_h)
{
	float view[16], proj[16], t[3], p[3], q[3], aspect, tilt, fov, fill;
	float half, s, c, tan_half, dist, need, cx, cy, cz;
	int i, ix, iy, iz;
	tilt = .62f;
	fov = .70f;
	fill = .86f;
	cx = SNAKE_W * .5f;
	cy = .30f;
	cz = SNAKE_H * .5f;
	aspect = fb_h ? (float)fb_w / (float)fb_h : 1;
	half = fov * .5f;
	s = half;
	c = half;
	grit_sinf(&s);
	grit_cosf(&c);
	tan_half = s / c;
	s = tilt;
	c = tilt;
	grit_sinf(&s);
	grit_cosf(&c);
	dist = 2;
	for (ix = 0; ix < 2; ++ix)
		for (iy = 0; iy < 2; ++iy)
			for (iz = 0; iz < 2; ++iz) {
				p[0] = (ix ? SNAKE_W + .5f : -.5f) - cx;
				p[1] = (iy ? 1.2f : -.25f) - cy;
				p[2] = (iz ? SNAKE_H + .5f : -.5f) - cz;
				q[0] = p[0];
				q[1] = c * p[1] - s * p[2];
				q[2] = s * p[1] + c * p[2];
				if (q[0] < 0)
					q[0] = -q[0];
				if (q[1] < 0)
					q[1] = -q[1];
				need = q[0] / (fill * aspect * tan_half);
				if (q[1] / (fill * tan_half) > need)
					need = q[1] / (fill * tan_half);
				if (q[2] + need > dist)
					dist = q[2] + need;
			}
	grit_mat4_identity(view);
	grit_vec3f(t, 0, 0, -dist);
	grit_mat4_translate_by(view, t);
	grit_mat4_rotate_x_by(view, tilt);
	grit_vec3f(t, -cx, -cy, -cz);
	grit_mat4_translate_by(view, t);
	grit_mat4_perspective(proj, fov, aspect, .1f, dist + 40);
	proj[5] *= -1;
	for (i = 0; i < 16; ++i)
		mvp[i] = proj[i];
	grit_mat4_mul(mvp, view);
}

static int
snake_parse_uint(const char *text, unsigned long maximum, unsigned long *out)
{
	char *end;
	unsigned long value;
	if (!text || text[0] < '0' || text[0] > '9')
		return 0;
	errno = 0;
	value = strtoul(text, &end, 10);
	if (errno || end == text || *end || value > maximum)
		return 0;
	*out = value;
	return 1;
}

static int
snake_options(int argc, char **argv, int *headless, int *frames, int *frames_set, const char **ppm, uint32_t *device)
{
	int i;
	unsigned long value;
	*headless = 0;
	*frames = 2;
	*frames_set = 0;
	*ppm = NULL;
	*device = 0;
	for (i = 1; i < argc; ++i) {
		if (!strcmp(argv[i], "--headless"))
			*headless = 1;
		else if (!strcmp(argv[i], "--ppm")) {
			if (++i >= argc)
				return 0;
			*ppm = argv[i];
		} else if (!strcmp(argv[i], "--frames")) {
			if (++i >= argc || !snake_parse_uint(argv[i], 1000000, &value) || !value)
				return 0;
			*frames = (int)value;
			*frames_set = 1;
		} else if (!strcmp(argv[i], "--device")) {
			if (++i >= argc || !snake_parse_uint(argv[i], UINT32_MAX, &value))
				return 0;
			*device = (uint32_t)value;
		} else {
			fprintf(stderr, "snake: unknown option: %s\n", argv[i]);
			return 0;
		}
	}
	if (!*headless && !*frames_set)
		*frames = 0;
	return 1;
}

static uint8_t *
snake_load_spv(const char *path, size_t *size)
{
	unsigned long bytes = 0;
	uint8_t *data = (uint8_t *)peak_file_alloc(path, &bytes);
	if (data)
		*size = (size_t)bytes;
	return data;
}

static int
snake_ppm_finish(SnakeRender *render, const char *ppm)
{
	return snake_render_capture(render, ppm);
}

int
main(int argc, char **argv)
{
	PeakCtx *peak_ctx = NULL;
	PeakWindow window;
	PeakEvent event;
	SnakeRender render;
	SnakeGame game;
	SnakeInstance instances[SNAKE_GPU_INSTANCE_CAPACITY];
	SnakeGpuInstance gpu_instances[SNAKE_GPU_INSTANCE_CAPACITY];
	SnakeGpuVertex gpu_vertices[SNAKE_GPU_VERTEX_COUNT];
	SnakeGpuRoot root;
	uint8_t *vert_spv = NULL, *frag_spv = NULL;
	size_t vert_bytes = 0, frag_bytes = 0;
	uint64_t start, frame_time, now;
	float acc = 0, step = SNAKE_STEP_START, time, dt, move_t, mvp[16];
	uint32_t width = 960, height = 720, device = 0, count, i;
	uint64_t frame = 0;
	int headless, frames, frames_set, running = 1, ok = 0, failed = 0;
	const char *ppm;
	memset(&window, 0, sizeof window);
	memset(&render, 0, sizeof render);
	memset(&game, 0, sizeof game);
	if (!snake_options(argc, argv, &headless, &frames, &frames_set, &ppm, &device)) {
		fprintf(stderr, "usage: snake [--headless] [--frames N] [--ppm FILE] [--device N]\n");
		return 2;
	}
	if (!headless) {
		if (!(peak_ctx = peak_init_legacy())) {
			fprintf(stderr, "snake: Peak initialization failed\n");
			goto cleanup;
		}
		window = peak_window_open(peak_ctx, "snake", width, height, 0);
		if (!window.running) {
			fprintf(stderr, "snake: window creation failed\n");
			goto cleanup;
		}
		width = window.width;
		height = window.height;
	}
	vert_spv = snake_load_spv("demos/snake/snake.vert.spv", &vert_bytes);
	if (!vert_spv)
		vert_spv = snake_load_spv("snake.vert.spv", &vert_bytes);
	frag_spv = snake_load_spv("demos/snake/snake.frag.spv", &frag_bytes);
	if (!frag_spv)
		frag_spv = snake_load_spv("snake.frag.spv", &frag_bytes);
	if (!vert_spv || !frag_spv) {
		fprintf(stderr, "snake: shader artifacts not found (run ./build snake)\n");
		goto cleanup;
	}
	if (!snake_render_init(&render, headless, width, height, device, headless ? NULL : &window,
	                       vert_spv, vert_bytes, frag_spv, frag_bytes))
		goto cleanup;
	game.body = grit_darray_create(SNAKE_MAX, sizeof(SnakeCell));
	if (!game.body.data) {
		fprintf(stderr, "snake: game allocation failed\n");
		goto cleanup;
	}
	grit_rng_seed(&game.rng, headless ? 1ull : peak_get_time());
	snake_reset(&game);
	for (i = 0; i < SNAKE_GPU_VERTEX_COUNT; ++i) {
		gpu_vertices[i].pos.x = snake_cube_verts[i].pos[0];
		gpu_vertices[i].pos.y = snake_cube_verts[i].pos[1];
		gpu_vertices[i].pos.z = snake_cube_verts[i].pos[2];
		gpu_vertices[i].shade = snake_cube_verts[i].shade;
	}
	start = peak_get_time();
	frame_time = start;
	while (running && (!frames || frame < (uint64_t)frames)) {
		if (headless) {
			dt = 1.0f / 60.0f;
			time = (float)frame * dt;
		} else {
			now = peak_get_time();
			dt = (float)(now - frame_time) / (float)NANOS_PER_SEC;
			frame_time = now;
			if (dt > .05f)
				dt = .05f;
			time = (float)(now - start) / (float)NANOS_PER_SEC;
		}
		while (!headless && peak_window_epoll(&window, &event)) {
			if (event.type == PEAK_EVENT_WINDOW_CLOSE) {
				running = 0;
				break;
			}
			if (event.type == PEAK_EVENT_WINDOW_RESIZE) {
				width = event.resize.width;
				height = event.resize.height;
				if (width && height) {
					int resized = snake_render_recreate_presentation(&render, width, height);
					if (!resized) {
						fprintf(stderr, "snake: presentation resize/recreation failed\n");
						failed = 1;
						running = 0;
					} else {
						width = render.width;
						height = render.height;
					}
				} else if (!snake_render_minimize(&render)) {
					fprintf(stderr, "snake: failed to retire presentation while minimized\n");
					failed = 1;
					running = 0;
				}
			}
			if (event.type == PEAK_EVENT_KEY_DOWN)
				switch (event.key.key) {
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
						acc = 0;
						step = SNAKE_STEP_START;
					}
					break;
				default:
					break;
				}
		}
		if (failed || !running)
			break;
		if (!headless && (!width || !height)) {
			peak_wait(&window, NULL, 0, -1);
			continue;
		}
		if (game.alive) {
			acc += dt;
			step = SNAKE_STEP_START - (float)game.body.len * .003f;
			if (step < SNAKE_STEP_MIN)
				step = SNAKE_STEP_MIN;
			while (acc >= step) {
				acc -= step;
				snake_tick(&game);
			}
			move_t = step > 0 ? acc / step : 1;
		} else {
			move_t = 1;
			game.dead_t += dt;
		}
		if (game.eat_t > 0) {
			game.eat_t -= dt * 4;
			if (game.eat_t < 0)
				game.eat_t = 0;
		}
		count = snake_fill_instances(&game, instances, time, move_t);
		for (i = 0; i < count; ++i) {
			gpu_instances[i].pos.x = instances[i].pos[0];
			gpu_instances[i].pos.y = instances[i].pos[1];
			gpu_instances[i].pos.z = instances[i].pos[2];
			gpu_instances[i].pos_pad = 0;
			gpu_instances[i].scale.x = instances[i].scale[0];
			gpu_instances[i].scale.y = instances[i].scale[1];
			gpu_instances[i].scale.z = instances[i].scale[2];
			gpu_instances[i].scale_pad = 0;
			gpu_instances[i].color.x = instances[i].color[0];
			gpu_instances[i].color.y = instances[i].color[1];
			gpu_instances[i].color.z = instances[i].color[2];
			gpu_instances[i].color_pad = 0;
		}
		snake_mvp(mvp, width, height);
		for (i = 0; i < 4; ++i) {
			uint32_t j;
			for (j = 0; j < 4; ++j)
				root.mvp.rows[i][j] = mvp[j * 4 + i];
		}
		root.instance_count = count;
		root.reserved = 0;
		if (!snake_render_draw(&render, gpu_vertices, snake_cube_inds, 36, gpu_instances, count, &root,
		                       headless && frame + 1 == (uint64_t)frames)) {
			RendDiagnostic diag = render.failure;
			if (!headless && diag.code == REND_DIAG_OUT_OF_DATE) {
				if (width && height && snake_render_recreate_presentation(&render, width, height)) {
					width = render.width;
					height = render.height;
					continue;
				}
				fprintf(stderr, "snake: out-of-date presentation could not be recreated\n");
			} else if (diag.code != REND_DIAG_NONE)
				fprintf(stderr, "snake: frame failed (%u): %s\n", (unsigned)diag.code, diag.message);
			else
				fprintf(stderr, "snake: frame submission failed\n");
			failed = 1;
			break;
		}
		++frame;
	}
	if (headless && !failed && !snake_ppm_finish(&render, ppm)) {
		fprintf(stderr, "snake: readback/PPM capture failed\n");
		failed = 1;
	}
	ok = !failed;
cleanup:
	if (game.body.data)
		grit_darray_destroy(&game.body);
	if (render.backend || render.discovery_memory) {
		if (!snake_render_destroy(&render)) {
			fprintf(stderr, "snake: teardown failed; retaining native dependencies until process exit\n");
			return 1;
		}
	}
	if (vert_spv)
		free(vert_spv);
	if (frag_spv)
		free(frag_spv);
	if (window.running)
		peak_window_close(&window);
	if (peak_ctx)
		peak_quit(peak_ctx);
	return ok && !failed ? 0 : 1;
}
