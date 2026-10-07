/* Grit public-API particle workload. See LICENSE.
 * Fixed pool particles, an ID map and active array, per-frame arena/stack
 * scratch, vector/RNG updates, and a software-rendered final scene. */
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define GRIT_DEBUG
#include "grit.h"
#include "grit.c"

#define DEMO_WIDTH 64
#define DEMO_HEIGHT 24
#define DEMO_PARTICLES 64
#define DEMO_FRAMES 512

typedef struct Particle {
	float position[2], velocity[2];
	uint32_t id, color, padding[2];
} Particle;

typedef union DemoStorage {
	long double alignment;
	unsigned char bytes[8192];
} DemoStorage;

static void demo_particle_spawn(Particle *particle, GritRng *rng, uint32_t id);

void
demo_particle_spawn(Particle *particle, GritRng *rng, uint32_t id)
{
	float x, y;

	grit_rng_f32(rng, &x);
	grit_rng_f32(rng, &y);
	grit_vec2f(particle->position, 1 + x * (DEMO_WIDTH - 3), 1 + y * (DEMO_HEIGHT - 3));
	grit_rng_f32(rng, &x);
	grit_rng_f32(rng, &y);
	grit_vec2f(particle->velocity, (x - 0.5f) * 0.8f, (y - 0.5f) * 0.8f);
	particle->id = id;
	particle->color = UINT32_C(0xFF808080) | (grit_rng_u32(rng) & UINT32_C(0x007F7F7F));
}

int
main(void)
{
	DemoStorage pool_storage, arena_storage, stack_storage;
	GritPool pool;
	GritArena arena;
	GritStack stack;
	GritDArray active;
	GritHashMap by_id;
	GritRng rng;
	GritDraw draw;
	uint32_t pixels[DEMO_WIDTH * DEMO_HEIGHT], next_id = 1;
	uint64_t checksum = UINT64_C(14695981039346656037);
	size_t frame, i, visible = 0;
	int ok = 0;

	grit_pool_init(&pool, pool_storage.bytes, sizeof(pool_storage.bytes), sizeof(Particle), 16);
	grit_arena_init(&arena, arena_storage.bytes, sizeof(arena_storage.bytes));
	grit_stack_init(&stack, stack_storage.bytes, sizeof(stack_storage.bytes));
	active = grit_darray_create(DEMO_PARTICLES, sizeof(Particle *));
	by_id = grit_hashmap_create(256, sizeof(uint32_t), sizeof(Particle *));
	grit_rng_seed(&rng, 42);
	grit_draw_begin(&draw, pixels, DEMO_WIDTH, DEMO_HEIGHT);
	for (i = 0; i < DEMO_PARTICLES; i++) {
		Particle *particle = grit_pool_alloc(&pool);
		if (!particle)
			goto done;
		demo_particle_spawn(particle, &rng, next_id++);
		grit_darray_push(&active, &particle);
		grit_hashmap_put(&by_id, &particle->id, &particle);
	}
	for (frame = 0; frame < DEMO_FRAMES; frame++) {
		int (*points)[2];
		grit_arena_free_all(&arena);
		if (!(points = grit_arena_alloc(&arena, DEMO_PARTICLES * sizeof(*points))))
			goto done;
		if (!(frame % 8)) {
			Particle **last = grit_darray_get(&active, DEMO_PARTICLES - 1);
			Particle *replacement;
			if (!last)
				goto done;
			grit_hashmap_del(&by_id, &(*last)->id);
			grit_pool_free(&pool, *last);
			grit_darray_pop(&active);
			if (!(replacement = grit_pool_alloc(&pool)))
				goto done;
			demo_particle_spawn(replacement, &rng, next_id++);
			grit_darray_push(&active, &replacement);
			grit_hashmap_put(&by_id, &replacement->id, &replacement);
		}
		grit_draw_clear(&draw, UINT32_C(0xFF101018));
		grit_draw_rect_gradient(&draw, 1, 1, DEMO_WIDTH - 2, DEMO_HEIGHT - 2,
			UINT32_C(0xFF102030), UINT32_C(0xFF203040), UINT32_C(0xFF201020), UINT32_C(0xFF302030));
		grit_draw_clip(&draw, 1, 1, DEMO_WIDTH - 2, DEMO_HEIGHT - 2);
		for (i = 0; i < DEMO_PARTICLES; i++) {
			Particle **entry = grit_darray_get(&active, i), **found;
			Particle *particle;
			int axis;
			if (!entry || !(found = grit_hashmap_get(&by_id, &(*entry)->id)) || *found != *entry)
				goto done;
			particle = *found;
			grit_vec2f_add(particle->position, particle->velocity);
			for (axis = 0; axis < 2; axis++) {
				float upper = axis ? DEMO_HEIGHT - 2 : DEMO_WIDTH - 2;
				if (particle->position[axis] < 1) {
					particle->position[axis] = 1;
					particle->velocity[axis] = -particle->velocity[axis];
				} else if (particle->position[axis] > upper) {
					particle->position[axis] = upper;
					particle->velocity[axis] = -particle->velocity[axis];
				}
				points[i][axis] = (int)particle->position[axis];
			}
			grit_draw_circle(&draw, points[i][0], points[i][1], 1, particle->color);
			if (i)
				grit_draw_line(&draw, points[i - 1][0], points[i - 1][1], points[i][0], points[i][1],
					1, particle->color, UINT32_C(0xFF303048));
		}
		grit_draw_tri(&draw, 4, 3, 12, 3, 8, 8, UINT32_C(0xFFFF0000),
			UINT32_C(0xFF00FF00), UINT32_C(0xFF0000FF));
		grit_draw_clip_reset(&draw);
		/* Consume each raster row with LIFO scratch, retaining only a checksum. */
		for (i = 0; i < DEMO_HEIGHT; i++) {
			uint32_t *row = grit_stack_alloc(&stack, DEMO_WIDTH * sizeof(*row), 16);
			size_t x;
			if (!row)
				goto done;
			memcpy(row, pixels + i * DEMO_WIDTH, DEMO_WIDTH * sizeof(*row));
			for (x = 0; x < DEMO_WIDTH; x++) {
				checksum = (checksum ^ row[x]) * UINT64_C(1099511628211);
				if (frame == DEMO_FRAMES - 1) {
					const char ramp[] = " .:-=+*#%@";
					unsigned luminance = ((row[x] >> 16) & 255) + ((row[x] >> 8) & 255) + (row[x] & 255);
					visible += row[x] != UINT32_C(0xFF101018);
					putchar(ramp[luminance * (sizeof(ramp) - 2) / 765]);
				}
			}
			if (frame == DEMO_FRAMES - 1)
				putchar('\n');
			grit_stack_free(&stack, row);
		}
	}
	if (!visible)
		goto done;
	printf("Grit: %d frames, %d particles, %zu scene pixels, checksum %016" PRIx64 "\n",
		DEMO_FRAMES, DEMO_PARTICLES, visible, checksum);
	ok = 1;
done:
	grit_hashmap_destroy(&by_id);
	grit_darray_destroy(&active);
	grit_stack_destroy(&stack);
	grit_arena_destroy(&arena);
	grit_pool_destroy(&pool);
	if (!ok)
		fprintf(stderr, "grit demo: workload failed\n");
	return ok ? 0 : 1;
}
