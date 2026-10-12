/*
 * snake — Peak window/input, Grit state/math, Rend instanced cubes.
 * 0.1.0 - first cut; 0.1.1 - board framing and animation; 0.1.2 - camera fit.
 */
#define _POSIX_C_SOURCE 200809L
#if !defined(__EMSCRIPTEN__) && !defined(PEAK_VULKAN)
#define PEAK_VULKAN
#endif
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Peak defines the native Vulkan platform macros before Vulkan is included. */
#include "Peak.h"
#include "../../Rend.h"
#define GRIT_IMPLEMENTATION
#include "Grit.h"

#define PEAK_IMPLEMENTATION
#include "Peak.h"
#define REND_IMPLEMENTATION
#include "../../Rend.h"

#define SNAKE_VERTEX_COUNT 24u
#define SNAKE_INSTANCE_CAPACITY (16u * 16u + 16u * 16u + 8u)

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
    {{0, 1, 0}, 1},    {{1, 1, 0}, 1},    {{1, 1, 1}, 1},    {{0, 1, 1}, 1},    {{0, 0, 0}, .35f},
    {{0, 0, 1}, .35f}, {{1, 0, 1}, .35f}, {{1, 0, 0}, .35f}, {{0, 0, 1}, .78f}, {{1, 0, 1}, .78f},
    {{1, 1, 1}, .78f}, {{0, 1, 1}, .78f}, {{1, 0, 0}, .55f}, {{0, 0, 0}, .55f}, {{0, 1, 0}, .55f},
    {{1, 1, 0}, .55f}, {{1, 0, 0}, .88f}, {{1, 1, 0}, .88f}, {{1, 1, 1}, .88f}, {{1, 0, 1}, .88f},
    {{0, 0, 0}, .62f}, {{0, 0, 1}, .62f}, {{0, 1, 1}, .62f}, {{0, 1, 0}, .62f},
};
typedef char SnakeVertexCountMustMatch
    [(sizeof snake_cube_verts / sizeof snake_cube_verts[0] == SNAKE_VERTEX_COUNT) ? 1 : -1];

static const uint16_t snake_cube_inds[] = {0,  1,  2,  0,  2,  3,  4,  5,  6,  4,  6,  7,
                                           8,  9,  10, 8,  10, 11, 12, 13, 14, 12, 14, 15,
                                           16, 17, 18, 16, 18, 19, 20, 21, 22, 20, 22, 23};

/* The same buffers, pipeline state and draw calls run on Vulkan and WebGL2.
 * Only the library selects the shader artifact and platform backend. */
typedef struct SnakeRender {
	RendRenderer renderer;
	RendPipeline pipeline;
	RendBuffer vertices, indices, instances;
	uint32_t width, height;
} SnakeRender;

static int snake_render_init(SnakeRender *r, int headless, uint32_t width, uint32_t height,
                             uint32_t device, PeakWindow *window, const uint8_t *vs, size_t vs_size,
                             const uint8_t *fs, size_t fs_size)
{
	static const char vertex_glsl[] =
	    "#version 300 es\nprecision highp float;"
	    "layout(location=0) in vec3 position;layout(location=1) in float shade;"
	    "layout(location=2) in vec3 offset;layout(location=3) in vec3 scale;layout(location=4) in "
	    "vec3 color;"
	    "layout(std140) uniform RendPushConstants {mat4 transform;};out vec3 vColor;"
	    "void main(){vec4 p=transform*vec4(position*scale+offset,1.);"
	    "gl_Position=vec4(p.x,-p.y,2.*p.z-p.w,p.w);vColor=color*(.35+.65*shade);}";
	static const char fragment_glsl[] =
	    "#version 300 es\nprecision mediump float;in vec3 vColor;out vec4 fragColor;"
	    "void main(){fragColor=vec4(vColor,1.);}";
	const RendVertexBinding bindings[] = {{0, sizeof(SnakeVertex), REND_INPUT_RATE_VERTEX},
	                                      {1, sizeof(SnakeInstance), REND_INPUT_RATE_INSTANCE}};
	const RendVertexAttributes attrs[] = {
	    {0, 0, offsetof(SnakeVertex, pos), REND_FORMAT_3_SFLOAT32},
	    {1, 0, offsetof(SnakeVertex, shade), REND_FORMAT_1_SFLOAT32},
	    {2, 1, offsetof(SnakeInstance, pos), REND_FORMAT_3_SFLOAT32},
	    {3, 1, offsetof(SnakeInstance, scale), REND_FORMAT_3_SFLOAT32},
	    {4, 1, offsetof(SnakeInstance, color), REND_FORMAT_3_SFLOAT32}};
	const RendPushConstantInfo push = {0, 64};
	if (device) {
		fprintf(stderr, "snake: legacy Rend selects its device automatically\n");
		return 0;
	}
	r->width = width;
	r->height = height;
	r->renderer = headless ? rend_renderer_create_offscreen(
	                             width, height, REND_FORMAT_R8G8B8A8_UNORM, REND_BACKEND_AUTO, NULL)
	                       : rend_renderer_create(window, REND_BACKEND_AUTO, NULL, true, NULL);
	if (!r->renderer)
		return 0;
	r->pipeline = rend_pipeline_create_graphics_portable(
	    r->renderer, vs, vs_size, fs, fs_size, vertex_glsl, fragment_glsl, bindings, 2, attrs, 5,
	    &push, 1, REND_POLYGON_MODE_FILL, REND_CULL_MODE_NONE, REND_TOPOLOGY_TRIANGLE_LIST,
	    REND_FORMAT_UNDEFINED, true);
	if (!r->pipeline)
		return 0;
	r->vertices =
	    rend_buffer_create(r->renderer, sizeof snake_cube_verts, REND_BUFFER_VERTEX, true);
	r->indices = rend_buffer_create(r->renderer, sizeof snake_cube_inds, REND_BUFFER_INDEX, true);
	r->instances = rend_buffer_create(r->renderer, sizeof(SnakeInstance) * SNAKE_INSTANCE_CAPACITY,
	                                  REND_BUFFER_VERTEX, false);
	if (!r->vertices.handle || !r->indices.handle || !r->instances.handle)
		return 0;
	rend_buffer_write(r->renderer, &r->vertices, snake_cube_verts, sizeof snake_cube_verts, 0);
	rend_buffer_write(r->renderer, &r->indices, snake_cube_inds, sizeof snake_cube_inds, 0);
	return 1;
}

static int snake_render_draw(SnakeRender *r, const SnakeInstance *instances, uint32_t count,
                             float *mvp)
{
	if (!rend_renderer_frame_begin(r->renderer))
		return 0;
	rend_buffer_write(r->renderer, &r->instances, instances, sizeof *instances * count, 0);
	rend_cmd_render_begin(r->renderer, .04f, .05f, .06f, 1);
	rend_cmd_bind_pipeline(r->pipeline);
	rend_cmd_bind_vertex_buffer(r->pipeline, 0, r->vertices, 0);
	rend_cmd_bind_vertex_buffer(r->pipeline, 1, r->instances, 0);
	rend_cmd_bind_index_buffer(r->pipeline, r->indices, 0, REND_INDEX_UINT16);
	rend_cmd_push_constants(r->pipeline, mvp, 64);
	rend_cmd_draw_indexed(r->pipeline, 36, 0, 0, count);
	rend_cmd_render_end(r->renderer);
	rend_renderer_frame_end(r->renderer, NULL);
	return 1;
}

static int snake_render_capture(SnakeRender *r, const char *path)
{
	size_t pixels = (size_t)r->width * r->height, i;
	uint8_t *rgba = malloc(pixels * 4);
	FILE *f;
	int ok;
	if (!rgba)
		return 0;
	rend_renderer_read(r->renderer, rgba, pixels * 4);
	if (!path) {
		free(rgba);
		return 1;
	}
	f = fopen(path, "wb");
	if (!f) {
		free(rgba);
		return 0;
	}
	ok = fprintf(f, "P6\n%u %u\n255\n", r->width, r->height) > 0;
	for (i = 0; i < pixels && ok; ++i)
		ok = fwrite(rgba + 4 * i, 1, 3, f) == 3;
	if (fclose(f))
		ok = 0;
	free(rgba);
	return ok;
}

static void snake_reset(SnakeGame *g);
static int snake_occupied(SnakeGame *g, int x, int y);
static void snake_place_food(SnakeGame *g);
static void snake_try_turn(SnakeGame *g, int dx, int dy);
static void snake_drop_tail(SnakeGame *g);
static void snake_tick(SnakeGame *g);
static void snake_emit(SnakeInstance *out, float x, float y, float z, float sx, float sy, float sz,
                       float r, float g, float b);
static void snake_cell_pos(const SnakeCell *c, float ease, float *x, float *z);
static uint32_t snake_fill_instances(SnakeGame *g, SnakeInstance *out, float time, float move_t);
static void snake_mvp(float *mvp, uint32_t fb_w, uint32_t fb_h);
static int snake_parse_uint(const char *text, unsigned long maximum, unsigned long *out);
static int snake_options(int argc, char **argv, int *headless, int *frames, int *frames_set,
                         const char **ppm, uint32_t *device);
#if !defined(__EMSCRIPTEN__)
static uint8_t *snake_load_spv(const char *path, size_t *size);
#endif
static int snake_ppm_finish(SnakeRender *render, const char *ppm);

static void snake_reset(SnakeGame *g)
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

static int snake_occupied(SnakeGame *g, int x, int y)
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

static void snake_place_food(SnakeGame *g)
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

static void snake_try_turn(SnakeGame *g, int dx, int dy)
{
	if (dx == -g->dx && dy == -g->dy)
		return;
	g->next_dx = dx;
	g->next_dy = dy;
}

static void snake_drop_tail(SnakeGame *g)
{
	size_t i;
	if (!g->body.len)
		return;
	for (i = 0; i + 1 < g->body.len; ++i)
		*(SnakeCell *)grit_darray_get(&g->body, i) = *(SnakeCell *)grit_darray_get(&g->body, i + 1);
	grit_darray_pop(&g->body);
}

static void snake_tick(SnakeGame *g)
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

static void snake_emit(SnakeInstance *out, float x, float y, float z, float sx, float sy, float sz,
                       float r, float g, float b)
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

static void snake_cell_pos(const SnakeCell *c, float ease, float *x, float *z)
{
	*x = (float)c->px;
	*z = (float)c->py;
	grit_lerpf(x, (float)c->x, ease);
	grit_lerpf(z, (float)c->y, ease);
}

static uint32_t snake_fill_instances(SnakeGame *g, SnakeInstance *out, float time, float move_t)
{
	uint32_t n = 0;
	int x, y, is_head;
	size_t i;
	SnakeCell *c;
	float ease, fx, fz, mdx, mdz, stretch, base, sx, sy, sz, wy, wave, dead;
	float t, bob, pulse, shade, cr, cg, cb, pop;
	ease = move_t < 0 ? 0 : move_t > 1 ? 1 : move_t;
	ease = ease * ease * (3.0f - 2.0f * ease);
	dead = 1.0f / (1.0f + g->dead_t * 3.2f);
	snake_emit(&out[n++], -.45f, -.22f, -.45f, SNAKE_W + .90f, .22f, SNAKE_H + .90f, .06f, .07f,
	           .08f);
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
		snake_emit(&out[n++], g->food_x + .22f, bob, g->food_y + .22f, .56f, .56f, .56f,
		           .95f * pulse, .22f, .24f);
	}
	if (g->popping && g->eat_t > 0) {
		pop = g->eat_t;
		snake_emit(&out[n++], g->pop_x + .5f - .28f * pop, .18f * pop, g->pop_y + .5f - .28f * pop,
		           .56f * pop, .56f * pop, .56f * pop, 1, .55f * pop, .20f);
	}
	return n;
}

static void snake_mvp(float *mvp, uint32_t fb_w, uint32_t fb_h)
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

static int snake_parse_uint(const char *text, unsigned long maximum, unsigned long *out)
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

static int snake_options(int argc, char **argv, int *headless, int *frames, int *frames_set,
                         const char **ppm, uint32_t *device)
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

#if !defined(__EMSCRIPTEN__)
static uint8_t *snake_load_spv(const char *path, size_t *size)
{
	unsigned long bytes = 0;
	uint8_t *data = (uint8_t *)peak_file_alloc(path, &bytes);
	if (data)
		*size = (size_t)bytes;
	return data;
}

#endif

static int snake_ppm_finish(SnakeRender *render, const char *ppm)
{
	return snake_render_capture(render, ppm);
}

#if defined(SNAKE_BROWSER_TEST)
static int snake_test_values[6];
static PeakWindow *snake_test_window;
EMSCRIPTEN_KEEPALIVE int snake_test_resize(void)
{
	uint32_t *previous;
	if (!snake_test_window)
		return 0;
	previous = snake_test_window->buffer;
	peak_window_set_size(snake_test_window, 1200, 900);
	if (snake_test_window->buffer == previous || snake_test_window->width != 1200 ||
	    snake_test_window->height != 900 || snake_test_window->bufsize != 1200 * 900)
		return 0;
	peak_window_clear(snake_test_window, .1f, .2f, .3f, 1);
	return snake_test_window->buffer[0] == snake_test_window->buffer[1200 * 900 - 1];
}
EMSCRIPTEN_KEEPALIVE int snake_test_state(int field)
{
	return field >= 0 && field < 6 ? snake_test_values[field] : -1;
}
#endif

static int snake_events(PeakWindow *window, SnakeRender *render, SnakeGame *game, uint32_t *width,
                        uint32_t *height, float *acc, float *step)
{
	PeakEvent event;
	while (peak_window_epoll(window, &event)) {
		if (event.type == PEAK_EVENT_WINDOW_CLOSE) {
			return 0;
			break;
		}
		if (event.type == PEAK_EVENT_WINDOW_RESIZE) {
			*width = event.resize.width;
			*height = event.resize.height;
			/* Rend recreates its target from Peak dimensions at frame begin. */
			render->width = *width;
			render->height = *height;
		}
		if (event.type == PEAK_EVENT_KEY_DOWN)
			switch (event.key.key) {
			case PEAK_KEY_ESCAPE:
				return 0;
				break;
			case PEAK_KEY_W: /* FALLTHROUGH */
			case PEAK_KEY_UP:
				snake_try_turn(game, 0, -1);
				break;
			case PEAK_KEY_S: /* FALLTHROUGH */
			case PEAK_KEY_DOWN:
				snake_try_turn(game, 0, 1);
				break;
			case PEAK_KEY_A: /* FALLTHROUGH */
			case PEAK_KEY_LEFT:
				snake_try_turn(game, -1, 0);
				break;
			case PEAK_KEY_D: /* FALLTHROUGH */
			case PEAK_KEY_RIGHT:
				snake_try_turn(game, 1, 0);
				break;
			case PEAK_KEY_SPACE: /* FALLTHROUGH */
			case PEAK_KEY_ENTER:
				if (!game->alive) {
					snake_reset(game);
					*acc = 0;
					*step = SNAKE_STEP_START;
				}
				break;
			default:
				break;
			}
	}
	return 1;
}

#if !defined(__EMSCRIPTEN__)
static int snake_load_shaders(uint8_t **vert_spv, size_t *vert_bytes, uint8_t **frag_spv,
                              size_t *frag_bytes)
{
	*vert_spv = snake_load_spv("demos/snake/snake.vert.spv", vert_bytes);
	if (!*vert_spv)
		*vert_spv = snake_load_spv("snake.vert.spv", vert_bytes);
	*frag_spv = snake_load_spv("demos/snake/snake.frag.spv", frag_bytes);
	if (!*frag_spv)
		*frag_spv = snake_load_spv("snake.frag.spv", frag_bytes);
	if (!*vert_spv || !*frag_spv) {
		fprintf(stderr, "snake: shader artifacts not found (run ./build snake)\n");
		return 0;
	}
	return 1;
}
#endif

typedef struct SnakeDemo {

	PeakCtx *peak_ctx;
	PeakWindow window;
	SnakeRender render;
	SnakeGame game;
	SnakeInstance instances[SNAKE_INSTANCE_CAPACITY];
	uint8_t *vert_spv, *frag_spv;
	size_t vert_bytes, frag_bytes;
	uint64_t start, frame_time, now;
	float acc, step, time, dt, move_t, mvp[16];
	uint32_t width, height, device, count;
	uint64_t frame;
	int headless, frames, frames_set, running, ok, failed;
	const char *ppm;
} SnakeDemo;

static int snake_open(SnakeDemo *d, int argc, char **argv)
{

	memset(&d->window, 0, sizeof d->window);
	memset(&d->render, 0, sizeof d->render);
	memset(&d->game, 0, sizeof d->game);
	if (!snake_options(argc, argv, &d->headless, &d->frames, &d->frames_set, &d->ppm, &d->device)) {
		fprintf(stderr, "usage: snake [--headless] [--frames N] [--ppm FILE] [--device N]\n");
		return -1;
	}
	if (!d->headless) {
		if (!(d->peak_ctx = peak_init())) {
			fprintf(stderr, "snake: Peak initialization failed\n");
			return 0;
		}
		d->window = peak_window_open(d->peak_ctx,
#if defined(__EMSCRIPTEN__)
		                             "canvas",
#else
		                             "snake",
#endif
		                             d->width, d->height, 0);
		if (!d->window.running) {
			fprintf(stderr, "snake: window creation failed\n");
			return 0;
		}
		d->width = d->window.width;
		d->height = d->window.height;
	}
#if !defined(__EMSCRIPTEN__)
	if (!snake_load_shaders(&d->vert_spv, &d->vert_bytes, &d->frag_spv, &d->frag_bytes))
		return 0;
#endif
	if (!snake_render_init(&d->render, d->headless, d->width, d->height, d->device,
	                       d->headless ? NULL : &d->window, d->vert_spv, d->vert_bytes, d->frag_spv,
	                       d->frag_bytes))
		return 0;
	d->game.body = grit_darray_create(SNAKE_MAX, sizeof(SnakeCell));
	if (!d->game.body.data) {
		fprintf(stderr, "snake: game allocation failed\n");
		return 0;
	}
	grit_rng_seed(&d->game.rng, d->headless ? 1ull : peak_get_time());
	snake_reset(&d->game);
#if defined(SNAKE_BROWSER_TEST)
	snake_test_window = d->headless ? NULL : &d->window;
#endif
	d->start = peak_get_time();
	d->frame_time = d->start;
	return 1;
}

static void snake_advance(SnakeDemo *d)
{
	if (d->game.alive) {
		d->acc += d->dt;
		d->step = SNAKE_STEP_START - (float)d->game.body.len * .003f;
		if (d->step < SNAKE_STEP_MIN)
			d->step = SNAKE_STEP_MIN;
		while (d->acc >= d->step) {
			d->acc -= d->step;
			snake_tick(&d->game);
		}
		d->move_t = d->step > 0 ? d->acc / d->step : 1;
	} else {
		d->move_t = 1;
		d->game.dead_t += d->dt;
	}
	if (d->game.eat_t > 0) {
		d->game.eat_t -= d->dt * 4;
		if (d->game.eat_t < 0)
			d->game.eat_t = 0;
	}
}

static int snake_frame(SnakeDemo *d)
{

	if (d->headless) {
		d->dt = 1.0f / 60.0f;
		d->time = (float)d->frame * d->dt;
	} else {
		d->now = peak_get_time();
		d->dt = (float)(d->now - d->frame_time) / (float)NANOS_PER_SEC;
		d->frame_time = d->now;
		if (d->dt > .05f)
			d->dt = .05f;
		d->time = (float)(d->now - d->start) / (float)NANOS_PER_SEC;
	}
	if (!d->headless)
		d->running = snake_events(&d->window, &d->render, &d->game, &d->width, &d->height, &d->acc,
		                          &d->step);
	if (d->failed || !d->running)
		return 0;
	if (!d->headless && (!d->width || !d->height)) {
		peak_wait(&d->window, NULL, 0, -1);
		return 1;
	}
	snake_advance(d);
	d->count = snake_fill_instances(&d->game, d->instances, d->time, d->move_t);
	snake_mvp(d->mvp, d->width, d->height);
	if (!snake_render_draw(&d->render, d->instances, d->count, d->mvp)) {
		fprintf(stderr, "snake: frame submission failed\n");
		d->failed = 1;
		return 0;
	}
	++d->frame;
#if defined(SNAKE_BROWSER_TEST)
	snake_test_values[0] = (int)d->frame;
	snake_test_values[1] = d->game.dx;
	snake_test_values[2] = d->game.dy;
	snake_test_values[3] = d->game.alive;
	snake_test_values[4] = (int)d->game.body.len;
#endif
#if defined(__EMSCRIPTEN__)
	emscripten_sleep(16); /* Asyncify yields for input and canvas presentation. */
#endif
	return 1;
}

static void snake_shutdown(SnakeDemo *d)
{

	if (d->game.body.data)
		grit_darray_destroy(&d->game.body);
	if (d->render.renderer)
		rend_renderer_destroy(d->render.renderer);
	rend_quit();
	if (d->vert_spv)
		free(d->vert_spv);
	if (d->frag_spv)
		free(d->frag_spv);
	if (d->window.running)
		peak_window_close(&d->window);
	if (d->peak_ctx)
		peak_quit(d->peak_ctx);
#if defined(SNAKE_BROWSER_TEST)
	snake_test_window = NULL;
	snake_test_values[5] = d->ok && !d->failed ? 1 : -1;
	printf("SNAKE_TEST_EXIT %d\n", snake_test_values[5]);
#endif
}

int main(int argc, char **argv)
{
	static SnakeDemo demo;
	SnakeDemo *d = &demo;
	d->width = 960;
	d->height = 720;
	d->running = 1;
	d->step = SNAKE_STEP_START;
	int opened = snake_open(d, argc, argv);
	if (opened <= 0) {
		snake_shutdown(d);
		return opened < 0 ? 2 : 1;
	}
	while (d->running && (!d->frames || d->frame < (uint64_t)d->frames)) {
		if (!snake_frame(d))
			break;
	}

	if (d->headless && !d->failed && !snake_ppm_finish(&d->render, d->ppm)) {
		fprintf(stderr, "snake: readback/PPM capture failed\n");
		d->failed = 1;
	}
	d->ok = !d->failed;
	snake_shutdown(d);
	return d->ok && !d->failed ? 0 : 1;
}
