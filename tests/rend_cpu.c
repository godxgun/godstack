/* Offscreen Rend CPU raster. No window, no Vulkan. */

#include "rend.h"
#include "peak.c"
#include "rend.c"

#include <stdio.h>
#include <string.h>

static int fail(const char *msg);
static void test_vert(RendCpuVarying *out, const RendCpuVertArgs *in);
static void test_frag(float rgba[4], const RendCpuFragArgs *in);

int
fail(const char *msg)
{
    fprintf(stderr, "fail: %s\n", msg);
    return 1;
}

void
test_vert(RendCpuVarying *out, const RendCpuVertArgs *in)
{
    float xs[4];
    float ys[4];
    uint32_t id;

    xs[0] = -1.f;
    xs[1] = 1.f;
    xs[2] = -1.f;
    xs[3] = 1.f;
    ys[0] = -1.f;
    ys[1] = -1.f;
    ys[2] = 1.f;
    ys[3] = 1.f;
    id = in->vertex_id;
    if (id > 3)
        id = 3;
    out->position[0] = xs[id];
    out->position[1] = ys[id];
    out->position[2] = 0.f;
    out->position[3] = 1.f;
    out->data[0] = 1.f;
    out->data[1] = 0.25f;
    out->data[2] = 0.5f;
}

void
test_frag(float rgba[4], const RendCpuFragArgs *in)
{
    rgba[0] = in->v->data[0];
    rgba[1] = in->v->data[1];
    rgba[2] = in->v->data[2];
    rgba[3] = 1.f;
}

int
main(void)
{
    RendRenderer r;
    RendPipeline p;
    RendPipeline tp;
    RendTexture tex;
    RendTexture *color;
    uint8_t px[64 * 64 * 4];
    uint8_t srcpix[16];
    uint32_t i;
    uint32_t nred;

    r = rend_renderer_create_offscreen(64, 64, REND_FORMAT_R8G8B8A8_UNORM, REND_BACKEND_CPU, NULL);
    if (!r)
        return fail("create");
    p = rend_pipeline_create_graphics_c(r, (void *)test_vert, 0, (void *)test_frag, 0,
        NULL, 0, NULL, 0, NULL, 0,
        REND_POLYGON_MODE_FILL, REND_CULL_MODE_NONE, REND_TOPOLOGY_TRIANGLE_STRIP,
        REND_FORMAT_R8G8B8A8_UNORM, false);
    if (!p)
        return fail("pipeline");
    if (!rend_renderer_frame_begin(r))
        return fail("begin");
    rend_cmd_render_begin(r, 0.f, 0.f, 0.f, 1.f);
    rend_cmd_bind_pipeline(p);
    rend_cmd_draw(p, 4, 1);
    rend_cmd_render_end(r);
    rend_renderer_frame_end(r, NULL);
    rend_renderer_read(r, px, sizeof px);
    if (px[0] != 255 || px[1] != 64 || px[2] != 128)
        return fail("quad");
    if (px[63 * 4] != 255 || px[63 * 4 + 1] != 64 || px[63 * 4 + 2] != 128)
        return fail("quad corner");

    tp = rend_pipeline_create_graphics_c(r, (void *)test_vert, 0, (void *)test_frag, 0,
        NULL, 0, NULL, 0, NULL, 0,
        REND_POLYGON_MODE_FILL, REND_CULL_MODE_NONE, REND_TOPOLOGY_TRIANGLE_LIST,
        REND_FORMAT_R8G8B8A8_UNORM, false);
    if (!tp)
        return fail("tri pipeline");
    if (!rend_renderer_frame_begin(r))
        return fail("begin tri");
    rend_cmd_render_begin(r, 0.f, 0.f, 0.f, 1.f);
    rend_cmd_bind_pipeline(tp);
    rend_cmd_draw(tp, 3, 1);
    rend_cmd_render_end(r);
    rend_renderer_frame_end(r, NULL);
    rend_renderer_read(r, px, sizeof px);
    if (px[0] != 255 || px[1] != 64 || px[2] != 128)
        return fail("tri origin");
    if (px[(63 * 64 + 63) * 4] != 0)
        return fail("tri far");

    memset(srcpix, 0, sizeof srcpix);
    srcpix[0] = 9;
    srcpix[4] = 10;
    srcpix[8] = 11;
    srcpix[12] = 12;
    tex = rend_texture_create_from_data(r, srcpix, 2, 2, REND_FORMAT_R8G8B8A8_UNORM);
    if (!tex.handle)
        return fail("tex");
    if (!rend_renderer_frame_begin(r))
        return fail("begin blit");
    color = rend_renderer_color_target(r);
    if (!color)
        return fail("color");
    rend_cmd_blit(r, &tex, color, 0, 0, 2, 2, 0, 0, 2, 2);
    rend_renderer_frame_end(r, NULL);
    rend_renderer_read(r, px, sizeof px);
    if (px[0] != 9 || px[4] != 10 || px[64 * 4] != 11)
        return fail("blit");
    nred = 0;
    for (i = 0; i < 64u * 64u; i++) {
        if (px[i * 4] == 9)
            nred++;
    }
    if (nred != 1)
        return fail("blit area");
    rend_quit();
    return 0;
}
