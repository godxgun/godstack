/* fuse_rend.c - FuseCmd to Rend triangles
 * 0.0.1 - @vasco - rect, cpu clip
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "fuse_rend.h"

typedef struct FuseRendPC {
    float screen_w, screen_h, pad0, pad1;
} FuseRendPC;

static void fuse_rend_internal_color(uint32_t c, float *r, float *g, float *b, float *a);
static int fuse_rend_internal_clip_rect(FuseRend *fr, float *x, float *y, float *w, float *h);
static void fuse_rend_internal_emit6(FuseRend *fr, float x0, float y0, float x1, float y1, uint32_t color);

static void
fuse_rend_internal_color(uint32_t c, float *r, float *g, float *b, float *a)
{
    *a = ((c >> 24) & 0xFFu) / 255.0f;
    *r = ((c >> 16) & 0xFFu) / 255.0f;
    *g = ((c >> 8) & 0xFFu) / 255.0f;
    *b = (c & 0xFFu) / 255.0f;
}

static int
fuse_rend_internal_clip_rect(FuseRend *fr, float *x, float *y, float *w, float *h)
{
    float x0, y0, x1, y1, cx, cy, cw, ch;
    if (fr->clip_n == 0)
        return 1;
    cx = fr->clip[fr->clip_n - 1][0];
    cy = fr->clip[fr->clip_n - 1][1];
    cw = fr->clip[fr->clip_n - 1][2];
    ch = fr->clip[fr->clip_n - 1][3];
    x0 = *x > cx ? *x : cx;
    y0 = *y > cy ? *y : cy;
    x1 = (*x + *w) < (cx + cw) ? (*x + *w) : (cx + cw);
    y1 = (*y + *h) < (cy + ch) ? (*y + *h) : (cy + ch);
    if (x1 <= x0 || y1 <= y0)
        return 0;
    *x = x0;
    *y = y0;
    *w = x1 - x0;
    *h = y1 - y0;
    return 1;
}

static void
fuse_rend_internal_emit6(FuseRend *fr, float x0, float y0, float x1, float y1, uint32_t color)
{
    FuseRendVertex *v;
    float r, g, b, a;
    size_t i;
    if (fr->vert_count + 6 > fr->vert_cap)
        return;
    fuse_rend_internal_color(color, &r, &g, &b, &a);
    v = &fr->verts[fr->vert_count];
    v[0].x = x0; v[0].y = y0;
    v[1].x = x1; v[1].y = y0;
    v[2].x = x0; v[2].y = y1;
    v[3].x = x0; v[3].y = y1;
    v[4].x = x1; v[4].y = y0;
    v[5].x = x1; v[5].y = y1;
    for (i = 0; i < 6; i++) {
        v[i].r = r;
        v[i].g = g;
        v[i].b = b;
        v[i].a = a;
    }
    fr->vert_count += 6;
}

size_t
fuse_rend_memory(size_t max_verts)
{
    return max_verts * sizeof (FuseRendVertex);
}

int
fuse_rend_init(FuseRend *fr, void *cpu_buf, size_t cpu_size,
               RendRenderer renderer,
               uint8_t *vert_spv, size_t vert_size,
               uint8_t *frag_spv, size_t frag_size)
{
    RendVertexBinding bind;
    RendVertexAttributes attrs[2];
    RendPushConstantInfo pc;
    size_t cap;
    if (!fr || !cpu_buf || !renderer || !vert_spv || !frag_spv)
        return 0;
    cap = cpu_size / sizeof (FuseRendVertex);
    if (cap < 6)
        return 0;
    memset(fr, 0, sizeof *fr);
    fr->verts = cpu_buf;
    fr->vert_cap = cap;
    bind.binding = 0;
    bind.stride = sizeof (FuseRendVertex);
    bind.input_rate = REND_INPUT_RATE_VERTEX;
    attrs[0].location = 0;
    attrs[0].binding = 0;
    attrs[0].offset = offsetof(FuseRendVertex, x);
    attrs[0].format = REND_FORMAT_2_SFLOAT32;
    attrs[1].location = 1;
    attrs[1].binding = 0;
    attrs[1].offset = offsetof(FuseRendVertex, r);
    attrs[1].format = REND_FORMAT_4_SFLOAT32;
    pc.offset = 0;
    pc.size = sizeof (FuseRendPC);
    fr->pipeline = rend_pipeline_create_graphics_spirv(
        renderer,
        vert_spv, vert_size,
        frag_spv, frag_size,
        &bind, 1,
        attrs, 2,
        &pc, 1,
        REND_POLYGON_MODE_FILL,
        REND_CULL_MODE_NONE,
        REND_TOPOLOGY_TRIANGLE_LIST,
        REND_FORMAT_UNDEFINED,
        false);
    if (!fr->pipeline)
        return 0;
    fr->vbo = rend_buffer_create(renderer, cap * sizeof (FuseRendVertex), REND_BUFFER_VERTEX, false);
    return 1;
}

void
fuse_rend_shutdown(FuseRend *fr)
{
    if (!fr)
        return;
    rend_buffer_destroy(&fr->vbo);
    fr->verts = NULL;
    fr->pipeline = NULL;
}

void
fuse_rend_begin(FuseRend *fr, float screen_w, float screen_h)
{
    if (!fr)
        return;
    fr->vert_count = 0;
    fr->screen_w = screen_w;
    fr->screen_h = screen_h;
    fr->clip_n = 1;
    fr->clip[0][0] = 0.0f;
    fr->clip[0][1] = 0.0f;
    fr->clip[0][2] = screen_w;
    fr->clip[0][3] = screen_h;
}

void
fuse_rend_quad(FuseRend *fr, float x, float y, float w, float h, uint32_t color)
{
    if (!fr)
        return;
    if (!fuse_rend_internal_clip_rect(fr, &x, &y, &w, &h))
        return;
    fuse_rend_internal_emit6(fr, x, y, x + w, y + h, color);
}

void
fuse_rend_cmds(FuseRend *fr, const FuseCmd *cmds, size_t n)
{
    size_t i;
    if (!fr || !cmds)
        return;
    for (i = 0; i < n; i++) {
        switch (cmds[i].type) {
        case FUSE_CMD_RECT:
            fuse_rend_quad(fr, cmds[i].rect.x, cmds[i].rect.y,
                           cmds[i].rect.w, cmds[i].rect.h, cmds[i].rect.color);
            break;
        case FUSE_CMD_CLIP_START:
            if (fr->clip_n < FUSE_REND_CLIP_MAX) {
                float x, y, w, h;
                x = cmds[i].clip.x;
                y = cmds[i].clip.y;
                w = cmds[i].clip.w;
                h = cmds[i].clip.h;
                fuse_rend_internal_clip_rect(fr, &x, &y, &w, &h);
                fr->clip[fr->clip_n][0] = x;
                fr->clip[fr->clip_n][1] = y;
                fr->clip[fr->clip_n][2] = w;
                fr->clip[fr->clip_n][3] = h;
                fr->clip_n++;
            }
            break;
        case FUSE_CMD_CLIP_END:
            if (fr->clip_n > 1)
                fr->clip_n--;
            break;
        default:
            break;
        }
    }
}

void
fuse_rend_flush(FuseRend *fr, RendRenderer renderer)
{
    FuseRendPC pc;
    if (!fr || !renderer || !fr->pipeline || fr->vert_count == 0)
        return;
    pc.screen_w = fr->screen_w;
    pc.screen_h = fr->screen_h;
    pc.pad0 = 0.0f;
    pc.pad1 = 0.0f;
    rend_buffer_write(renderer, &fr->vbo, fr->verts, fr->vert_count * sizeof (FuseRendVertex), 0);
    rend_cmd_bind_pipeline(fr->pipeline);
    rend_cmd_push_constants(fr->pipeline, &pc, sizeof pc);
    rend_cmd_bind_vertex_buffer(fr->pipeline, 0, fr->vbo, 0);
    rend_cmd_draw(fr->pipeline, fr->vert_count, 1);
}
