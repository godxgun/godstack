/* Headless Fuse Rend geometry and recorded draw-order regression.
 * No Vulkan, window, shader, font, or GPU coverage. Rend calls below record
 * uploads/draws only. Link with section GC to discard unused GPU setup APIs:
 * cc -std=c99 -Wall -Werror -ffunction-sections -fdata-sections \
 *    fuse_rend_test.c -Wl,--gc-sections -lm -o /tmp/<unique>/fuse-rend-test
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Rend's concrete value types must precede fuse_rend.h, as in its host TU. */
#include "../Rend/rend_internal.h"
#include "../Type/type.c"
#include "fuse_rend.c"

typedef struct TestDraw {
	RendPipeline pipeline;
	size_t offset, count;
} TestDraw;

static void test_reset(void);
static void test_glyph(FuseRend *fr);
static void test_order(void);
static void test_capacity(void);
static void test_ranges(void);
static void test_clips(void);

static struct rend_pipeline_t solid_pipeline, glyph_pipeline;
static struct rend_renderer_t renderer;
static TestDraw draws[32];
static size_t draw_n, upload_n, bound_offset;
static RendPipeline bound_pipeline;

void
rend_texture_copy_data(RendRenderer r, RendTexture *texture, const void *data, size_t size)
{
	(void)r; (void)texture; (void)data; (void)size;
	assert(0); /* No text atlas in these geometry tests. */
}

void
rend_buffer_write(RendRenderer r, RendBuffer *buffer, const void *data, size_t size, size_t offset)
{
	assert(r == &renderer && buffer && data && size && offset == 0);
	upload_n++;
}

void
rend_cmd_bind_pipeline(RendPipeline pipeline)
{
	bound_pipeline = pipeline;
}

void
rend_cmd_push_constants(RendPipeline pipeline, void *data, size_t size)
{
	assert(pipeline == bound_pipeline && data && size == 4 * sizeof(float));
}

void
rend_cmd_bind_vertex_buffer(RendPipeline pipeline, uint32_t binding, RendBuffer buffer, size_t offset)
{
	(void)buffer;
	assert(pipeline == bound_pipeline && binding == 0);
	bound_offset = offset;
}

void
rend_cmd_draw(RendPipeline pipeline, size_t count, uint32_t instances)
{
	assert(pipeline == bound_pipeline && instances == 1 && draw_n < 32);
	draws[draw_n++] = (TestDraw){pipeline, bound_offset, count};
}

static void
test_reset(void)
{
	draw_n = upload_n = 0;
	bound_pipeline = NULL;
	bound_offset = 0;
}

static void
test_glyph(FuseRend *fr)
{
	TypeGlyph g;
	memset(&g, 0, sizeof(g));
	g.width = g.height = 8;
	fuse_rend_internal_glyph(fr, 10, 10, &g, 0xffffffff);
}

static void
test_order(void)
{
	FuseRend fr = {0};
	FuseRendVertex verts[36];
	FuseRendGlyph glyphs[36];
	FuseRendBatch batches[4];
	size_t i;
	fr.verts = verts; fr.vert_cap = 36;
	fr.glyphs = glyphs; fr.glyph_cap = 36;
	fr.atlas_w = fr.atlas_h = 64;
	fr.pipeline = &solid_pipeline; fr.text_pipeline = &glyph_pipeline;
	fuse_rend_batches(&fr, batches, 4);
	fuse_rend_begin(&fr, 100, 100);
	/* Pre-layer geometry must not disappear at the first explicit layer. */
	fuse_rend_quad(&fr, 0, 0, 20, 20, 0xff000000);
	test_glyph(&fr);
	assert(fuse_rend_layer(&fr));
	assert(batches[0].vertex_start == 0 && batches[0].glyph_start == 0);
	assert(fuse_rend_layer(&fr));
	/* An empty layer must not reorder or duplicate adjacent geometry. */
	assert(fuse_rend_layer(&fr));
	fuse_rend_quad(&fr, 0, 0, 20, 20, 0xffffffff);
	test_glyph(&fr);
	assert(fuse_rend_layer(&fr));
	test_glyph(&fr);
	test_reset();
	fuse_rend_flush(&fr, &renderer);
	assert(upload_n == 2 && draw_n == 5);
	assert(draws[0].pipeline == &solid_pipeline && draws[0].offset == 0);
	assert(draws[1].pipeline == &glyph_pipeline && draws[1].offset == 0);
	/* Lower glyphs are drawn before the opaque foreground panel. */
	assert(draws[2].pipeline == &solid_pipeline && draws[2].offset == 6 * sizeof(*verts));
	assert(draws[3].pipeline == &glyph_pipeline && draws[3].offset == 6 * sizeof(*glyphs));
	assert(draws[4].pipeline == &glyph_pipeline && draws[4].offset == 12 * sizeof(*glyphs));
	for (i = 0; i < draw_n; i++) assert(draws[i].count == 6);
	/* Backing without layer calls keeps existing single-layer behavior. */
	fuse_rend_begin(&fr, 100, 100);
	fuse_rend_quad(&fr, 0, 0, 20, 20, 0xffffffff);
	test_glyph(&fr);
	fuse_rend_quad(&fr, 0, 0, 20, 20, 0xffffffff);
	test_reset();
	fuse_rend_flush(&fr, &renderer);
	assert(upload_n == 2 && draw_n == 2);
	assert(draws[0].pipeline == &solid_pipeline && draws[0].count == 12);
	assert(draws[1].pipeline == &glyph_pipeline && draws[1].count == 6);
	fuse_rend_batches(&fr, NULL, 100);
	assert(fr.batch_cap == 0);
	test_reset();
	fuse_rend_flush(&fr, &renderer);
	assert(draw_n == 2 && draws[0].count == 12);
}

static void
test_ranges(void)
{
	FuseRend fr = {0};
	FuseRendVertex verts[12];
	FuseRendGlyph glyphs[12];
	FuseRendBatch batches[3];

	fr.verts = verts; fr.vert_cap = 12;
	fr.glyphs = glyphs; fr.glyph_cap = 12;
	fr.atlas_w = fr.atlas_h = 64;
	fr.pipeline = &solid_pipeline; fr.text_pipeline = &glyph_pipeline;
	fuse_rend_batches(&fr, batches, 3);
	fuse_rend_begin(&fr, 100, 100);
	assert(fuse_rend_layer(&fr));
	fuse_rend_quad(&fr, 0, 0, 20, 20, 0xffffffff);
	test_glyph(&fr);
	assert(fuse_rend_layer(&fr)); /* Empty middle batch. */
	assert(fuse_rend_layer(&fr));
	fuse_rend_quad(&fr, 0, 0, 20, 20, 0xffffffff);
	test_glyph(&fr);
	test_reset();
	assert(!fuse_rend_flush_range(&fr, &renderer, 0, 1));
	assert(!upload_n && !draw_n);
	fuse_rend_prepare(&fr, &renderer);
	fuse_rend_prepare(&fr, &renderer);
	assert(upload_n == 2 && !draw_n);
	assert(fuse_rend_flush_range(&fr, &renderer, 0, 1));
	assert(draw_n == 2 && draws[0].offset == 0 && draws[1].offset == 0);
	assert(fuse_rend_flush_range(&fr, &renderer, 1, 3));
	assert(draw_n == 4 && upload_n == 2);
	assert(draws[2].offset == 6 * sizeof(*verts) && draws[3].offset == 6 * sizeof(*glyphs));
	assert(fuse_rend_flush_range(&fr, &renderer, 3, 3));
	assert(!fuse_rend_flush_range(&fr, &renderer, 2, 1));
	assert(!fuse_rend_flush_range(&fr, &renderer, 0, SIZE_MAX));
	batches[2].vertex_start = 13;
	assert(!fuse_rend_flush_range(&fr, &renderer, 0, 1)); /* Validate whole list. */
	batches[2].vertex_start = 0; /* Non-monotonic. */
	assert(!fuse_rend_flush_range(&fr, &renderer, 0, 1));
	batches[2].vertex_start = 6;
	fr.overflow = 1;
	assert(!fuse_rend_flush_range(&fr, &renderer, 0, 3));
	assert(draw_n == 4 && upload_n == 2);
	fuse_rend_begin(&fr, 100, 100);
	assert(!fr.prepared_renderer);
	assert(!fuse_rend_flush_range(&fr, &renderer, 0, 1));
	assert(fuse_rend_flush_range(&fr, &renderer, 0, 0));
}

static void
test_capacity(void)
{
	FuseRend fr = {0};
	FuseRendVertex verts[6];
	FuseRendGlyph glyphs[6];
	FuseRendBatch batch;
	FuseCmdTriangle triangle = {0, 0, 20, 0, 0, 20, 0xffffffff};
	fr.verts = verts; fr.vert_cap = 6;
	fr.glyphs = glyphs; fr.glyph_cap = 6;
	fr.atlas_w = fr.atlas_h = 64;
	fr.pipeline = &solid_pipeline; fr.text_pipeline = &glyph_pipeline;
	fuse_rend_batches(NULL, NULL, 0);
	assert(!fuse_rend_layer(NULL));
	fuse_rend_cmds_layer(NULL, NULL, 0, 0);
	fuse_rend_batches(&fr, &batch, 1);
	fuse_rend_begin(&fr, 100, 100);
	assert(fuse_rend_layer(&fr));
	fuse_rend_quad(&fr, 0, 0, 20, 20, 0xffffffff);
	test_glyph(&fr);
	assert(!fr.overflow && fr.vert_count == 6 && fr.glyph_count == 6);
	assert(!fuse_rend_layer(&fr) && fr.overflow && fr.batch_count == 1);
	test_reset();
	fuse_rend_flush(&fr, &renderer);
	assert(draw_n == 0 && upload_n == 0);
	fuse_rend_begin(&fr, 100, 100);
	assert(!fr.overflow && fr.batch_count == 0);
	fuse_rend_quad(&fr, 0, 0, 20, 20, 0xffffffff);
	fuse_rend_quad(&fr, 0, 0, 20, 20, 0xffffffff);
	assert(fr.overflow && fr.vert_count == 6);
	fuse_rend_begin(&fr, 100, 100);
	fuse_rend_triangle(&fr, &triangle);
	fuse_rend_triangle(&fr, &triangle);
	assert(!fr.overflow && fr.vert_count == 6);
	fuse_rend_triangle(&fr, &triangle);
	assert(fr.overflow && fr.vert_count == 6);
	fuse_rend_begin(&fr, 100, 100);
	test_glyph(&fr);
	/* A fully clipped glyph must not report a false capacity failure. */
	fr.clip[0][2] = fr.clip[0][3] = 1;
	test_glyph(&fr);
	assert(!fr.overflow && fr.glyph_count == 6);
	fr.clip[0][2] = fr.clip[0][3] = 100;
	test_glyph(&fr);
	assert(fr.overflow && fr.glyph_count == 6);
	test_reset();
	fuse_rend_flush(&fr, &renderer);
	assert(draw_n == 0 && upload_n == 0);
	fuse_rend_batches(&fr, &batch, 0);
	fuse_rend_begin(&fr, 100, 100);
	assert(!fuse_rend_layer(&fr) && fr.overflow);
}

static void
test_clips(void)
{
	FuseRend fr = {0};
	FuseRendVertex verts[24];
	FuseCmd cmds[5] = {0}, deep[FUSE_REND_CLIP_MAX] = {0};
	size_t i;
	fr.verts = verts; fr.vert_cap = 24;
	cmds[0].type = FUSE_CMD_CLIP_START;
	cmds[0].clip = (FuseCmdClip){10, 10, 20, 20};
	cmds[1].type = FUSE_CMD_CLIP_START; cmds[1].z = 2;
	cmds[1].clip = (FuseCmdClip){15, 15, 20, 20};
	cmds[2].type = FUSE_CMD_RECT; cmds[2].z = 2;
	cmds[2].rect = (FuseCmdRect){0, 0, 100, 100, 0xffffffff};
	cmds[3].type = FUSE_CMD_CLIP_END; cmds[3].z = 2;
	cmds[4].type = FUSE_CMD_CLIP_END;
	fuse_rend_begin(&fr, 100, 100);
	fuse_rend_cmds_layer(&fr, cmds, 5, 2);
	assert(fr.vert_count == 6 && fr.clip_n == 1);
	assert(verts[0].x == 15 && verts[0].y == 15 && verts[5].x == 30 && verts[5].y == 30);
	fuse_rend_cmds_layer(&fr, cmds, 5, 0);
	assert(fr.vert_count == 6 && fr.clip_n == 1);
	/* Labels after replay use the caller clip, not a layer's residual stack. */
	fuse_rend_cmds_layer(&fr, cmds, 3, 2);
	assert(fr.clip_n == 1);
	fuse_rend_quad(&fr, 0, 0, 5, 5, 0xffffffff);
	assert(fr.vert_count == 18 && verts[12].x == 0);
	for (i = 0; i < FUSE_REND_CLIP_MAX; i++) {
		deep[i].type = FUSE_CMD_CLIP_START;
		deep[i].clip = (FuseCmdClip){0, 0, 100, 100};
	}
	fuse_rend_begin(&fr, 100, 100);
	fuse_rend_cmds_layer(&fr, deep, FUSE_REND_CLIP_MAX, 0);
	assert(fr.overflow && fr.clip_n == 1);
}

int
main(void)
{
	(void)rend_format_size;
	(void)p_prefix;
	test_order();
	test_capacity();
	test_ranges();
	test_clips();
	puts("Fuse Rend headless geometry/draw-record tests passed (no GPU coverage)");
	return 0;
}
