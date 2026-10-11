/* Source-level smoke test: real Grit storage, simulation and raster. */
#define main visual_program_main
#include "demo.c"
#undef main
#include <assert.h>
static uint64_t hash_view(void)
{
	uint64_t h = 14695981039346656037ull;
	for (int i = 0; i < loop.view.w * loop.view.h; i++)
		h = (h ^ loop.view.pixels[i]) * 1099511628211ull;
	return h;
}
int main(void)
{
	loop.view = (View){calloc(WIDTH * HEIGHT, 4), WIDTH, HEIGHT, 0, 0, WIDTH, HEIGHT};
	assert(loop.view.pixels);
	loop.paused = 1;
	assert(reset());
	assert(render(&loop.view));
	uint64_t initial = hash_view();
	assert(active.len == DEMO_GRIT_PARTICLES && by_id.len == active.len && ticks == 0);
	assert(render(&loop.view));
	assert(hash_view() == initial && ticks == 0);
	for (int i = 0; i < 120; i++)
		assert(step((i + 1) / 60.0));
	assert(ticks == 120 && recycled == 10);
	assert(render(&loop.view));
	assert(hash_view() != initial);
	assert(stack.offset == 0 && arena.curr_offset == active.len * 8);
	assert(view_rgba(0xff123456) == 0xff123456);
	loop.stress = 1;
	assert(reset());
	assert(active.len == DEMO_GRIT_MAX_PARTICLES);
	assert(render(&loop.view));
	for (int i = 0; i < 120; i++)
		assert(step((i + 1) / 60.0));
	assert(render(&loop.view));
	loop.stress = 0;
	loop.elapsed = 0;
	for (int i = 0; i < 10; i++) {
		assert(reset());
		assert(render(&loop.view));
		assert(hash_view() == initial);
	}
	destroy();
	free(loop.view.pixels);
	printf("Grit visual: PASS (%d/%d particles; deterministic paused/restart/stress; map/scratch "
	       "invariants)\n",
	       DEMO_GRIT_PARTICLES, DEMO_GRIT_MAX_PARTICLES);
	return 0;
}
