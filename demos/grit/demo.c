/* Grit particle observatory: one C/Peak program for native and Emscripten.
 * Original public-API console workload preserved verbatim in workload.c. */
#include "visual.h"
#include <math.h>
#define GRIT_IMPLEMENTATION
#include "Grit.h"
#define WIDTH 720
#define HEIGHT 420
#ifndef DEMO_GRIT_PARTICLES
#define DEMO_GRIT_PARTICLES 1536
#endif
#ifndef DEMO_GRIT_MAX_PARTICLES
#define DEMO_GRIT_MAX_PARTICLES 4096
#endif
#if DEMO_GRIT_PARTICLES < 1 || DEMO_GRIT_PARTICLES > DEMO_GRIT_MAX_PARTICLES
#error Invalid demo particle capacity
#endif
#define BACKGROUND UINT32_C(0xff141413)
typedef struct Particle {
	float position[2], velocity[2], previous[2];
	uint32_t id, color;
} Particle;
typedef union PoolStorage {
	long double align;
	unsigned char bytes[DEMO_GRIT_MAX_PARTICLES * 48 + 32];
} PoolStorage;
typedef union ArenaStorage {
	long double align;
	unsigned char bytes[DEMO_GRIT_MAX_PARTICLES * 8 + 32];
} ArenaStorage;
typedef union StackStorage {
	long double align;
	unsigned char bytes[8192];
} StackStorage;
static PoolStorage pool_storage;
static ArenaStorage arena_storage;
static StackStorage stack_storage;
static GritPool pool;
static GritArena arena;
static GritStack stack;
static GritDArray active;
static GritHashMap by_id;
static GritRng rng;
static GritDraw draw;
static uint32_t pixels[WIDTH * HEIGHT], next_id;
static int initialized;
static unsigned ticks, recycled;
static void spawn(Particle *p, uint32_t id)
{
	float a, r, jitter;
	grit_rng_f32(&rng, &a);
	grit_rng_f32(&rng, &r);
	grit_rng_f32(&rng, &jitter);
	a *= 6.2831853f;
	r = 18 + sqrtf(r) * 305;
	grit_vec2f(p->position, 360 + cosf(a) * r, 219 + sinf(a) * r * .43f);
	grit_vec2f(p->velocity, -sinf(a) * (12 + jitter * 24), cosf(a) * (8 + jitter * 16));
	memcpy(p->previous, p->position, sizeof(p->position));
	p->id = id;
	const uint32_t colors[] = {0xffff8800, 0xffd2b48c, 0xffaaa99f, 0xffefc879, 0xffeeedea};
	p->color = colors[grit_rng_u32(&rng) % 5];
}
static void destroy(void)
{
	if (!initialized)
		return;
	grit_hashmap_destroy(&by_id);
	grit_darray_destroy(&active);
	grit_stack_destroy(&stack);
	grit_arena_destroy(&arena);
	grit_pool_destroy(&pool);
	initialized = 0;
}
static int reset(void)
{
	destroy();
	grit_pool_init(&pool, pool_storage.bytes, sizeof(pool_storage.bytes), sizeof(Particle), 16);
	grit_arena_init(&arena, arena_storage.bytes, sizeof(arena_storage.bytes));
	grit_stack_init(&stack, stack_storage.bytes, sizeof(stack_storage.bytes));
	active = grit_darray_create(DEMO_GRIT_MAX_PARTICLES, sizeof(Particle *));
	by_id = grit_hashmap_create(DEMO_GRIT_MAX_PARTICLES * 2, sizeof(uint32_t), sizeof(Particle *));
	initialized = 1;
	if (!active.data || !by_id.keys || !by_id.vals || !by_id.state)
		return 0;
	grit_rng_seed(&rng, 42);
	next_id = 1;
	ticks = recycled = 0;
	size_t count = loop.stress ? DEMO_GRIT_MAX_PARTICLES : DEMO_GRIT_PARTICLES;
	for (size_t i = 0; i < count; i++) {
		Particle *p = grit_pool_alloc(&pool);
		if (!p)
			return 0;
		spawn(p, next_id++);
		grit_darray_push(&active, &p);
		grit_hashmap_put(&by_id, &p->id, &p);
		if (active.len != i + 1 || by_id.len != i + 1)
			return 0;
	}
	grit_draw_begin(&draw, pixels, WIDTH, HEIGHT);
	return 1;
}
static int step(double elapsed)
{
	(void)elapsed;
	if (ticks % 12 == 0) {
		size_t index = (ticks / 12) % active.len;
		Particle **entry = grit_darray_get(&active, index);
		if (!entry)
			return 0;
		grit_hashmap_del(&by_id, &(*entry)->id);
		grit_pool_free(&pool, *entry);
		Particle *p = grit_pool_alloc(&pool);
		if (!p)
			return 0;
		spawn(p, next_id++);
		*entry = p;
		grit_hashmap_put(&by_id, &p->id, &p);
		recycled++;
		if (by_id.len != active.len)
			return 0;
	}
	for (size_t i = 0; i < active.len; i++) {
		Particle **entry = grit_darray_get(&active, i), **found;
		if (!entry || !(found = grit_hashmap_get(&by_id, &(*entry)->id)) || *found != *entry)
			return 0;
		Particle *p = *found;
		memcpy(p->previous, p->position, sizeof(p->position));
		float delta[2] = {p->position[0] - 360, (p->position[1] - 219) * 2.2f}, distance;
		grit_vec2f_len(&distance, delta);
		distance = fmaxf(distance, 15);
		float force[2] = {-delta[1] / distance * 32 - delta[0] * .035f,
		                  delta[0] / distance * 17 - delta[1] * .035f};
		grit_vec2f_mult(force, 1.0f / 60);
		grit_vec2f_add(p->velocity, force);
		grit_vec2f_mult(p->velocity, .997f);
		float move[2];
		memcpy(move, p->velocity, sizeof(move));
		grit_vec2f_mult(move, 1.0f / 60);
		grit_vec2f_add(p->position, move);
		for (int axis = 0; axis < 2; axis++) {
			float lo = axis ? 76 : 16, hi = axis ? 360 : 704;
			if (!isfinite(p->position[axis]) || !isfinite(p->velocity[axis]))
				return 0;
			if (p->position[axis] < lo) {
				p->position[axis] = lo;
				p->velocity[axis] = fabsf(p->velocity[axis]);
			}
			if (p->position[axis] > hi) {
				p->position[axis] = hi;
				p->velocity[axis] = -fabsf(p->velocity[axis]);
			}
		}
	}
	ticks++;
	return 1;
}
static int render(View *v)
{
	grit_arena_free_all(&arena);
	int (*points)[2] = grit_arena_alloc(&arena, active.len * sizeof(*points));
	if (!points)
		return 0;
	grit_draw_clear(&draw, BACKGROUND);
	grit_draw_rect_gradient(&draw, 12, 12, 696, 353, 0xff1c1c1a, 0xff20201e, 0xff242422,
	                        0xff191918);
	grit_draw_clip(&draw, 12, 12, 696, 353);
	for (int x = 24; x < 700; x += 32)
		grit_draw_line(&draw, x, 12, x, 365, 1, 0xff282826, 0xff2b2b28);
	for (int y = 20; y < 365; y += 32)
		grit_draw_line(&draw, 12, y, 708, y, 1, 0xff282826, 0xff2b2b28);
	for (size_t i = 0; i < active.len; i++) {
		Particle **entry = grit_darray_get(&active, i), **found;
		if (!entry || !(found = grit_hashmap_get(&by_id, &(*entry)->id)) || *entry != *found)
			return 0;
		points[i][0] = (int)(*found)->position[0];
		points[i][1] = (int)(*found)->position[1];
	}
	for (size_t i = 1; i < active.len; i++) {
		int dx = points[i][0] - points[i - 1][0], dy = points[i][1] - points[i - 1][1];
		if (dx * dx + dy * dy < 2500)
			grit_draw_line(&draw, points[i - 1][0], points[i - 1][1], points[i][0], points[i][1], 1,
			               0xff383833, 0xff45453e);
	}
	for (size_t i = 0; i < active.len; i++) {
		Particle *p = *(Particle **)grit_darray_get(&active, i);
		uint32_t dim = 0xff000000u | ((p->color & 0x00fcfcfcu) >> 2);
		grit_draw_circle(&draw, points[i][0], points[i][1], i % 9 == 0 ? 3 : 2, dim);
		grit_draw_line(&draw, (int)p->previous[0], (int)p->previous[1], points[i][0], points[i][1],
		               1, dim, p->color);
		grit_draw_circle(&draw, points[i][0], points[i][1], i % 9 == 0 ? 1 : 0, p->color);
	}
	grit_draw_clip_reset(&draw);
	unsigned lit = 0;
	uint32_t checksum = 2166136261u;
	for (int y = 0; y < HEIGHT; y++) {
		uint32_t *row = grit_stack_alloc(&stack, WIDTH * sizeof(*row), 16);
		if (!row)
			return 0;
		memcpy(row, pixels + y * WIDTH, WIDTH * sizeof(*row));
		for (int x = 0; x < WIDTH; x++) {
			uint32_t c = row[x];
			checksum = (checksum ^ c) * 16777619u;
			lit += ((c >> 16) & 255) + ((c >> 8) & 255) + (c & 255) > 180;
			/* GritDraw is AARRGGBB: native copies directly, web swaps R/B. */
			v->pixels[y * WIDTH + x] = view_rgba(c);
		}
		grit_stack_free(&stack, row);
	}
	if (stack.offset || by_id.len != active.len)
		return 0;
	char text[160];
	snprintf(text, sizeof(text), "%zu PARTICLES / %u LIT PIXELS / STEP %u", active.len, lit, ticks);
	view_text(v, 22, 376, 1, text, 0xffff8800);
	snprintf(text, sizeof(text), "%zu VERIFIED IDS / %u RECYCLES / %zu SCRATCH BYTES / HASH %08X",
	         by_id.len, recycled, (size_t)arena.curr_offset, (unsigned)checksum);
	view_text(v, 22, 390, 1, text, 0xffb5b4ad);
	demo_controls(v, 407);
	return 1;
}
int main(int argc, char **argv)
{
	loop.reset = reset;
	loop.step = step;
	loop.render = render;
	loop.destroy = destroy;
	return demo_run(argc, argv, "Particle observatory", WIDTH, HEIGHT);
}
