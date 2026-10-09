/* ===========================================================================
 * FUSE REND - Tessellate FuseCmd into a Rend 2D pipeline
 *
 * USAGE:
 *     #define FUSE_REND_IMPLEMENTATION in exactly one translation unit
 *     #include "FuseRend.h"
 *     Other translation units include it without the implementation macro.
 *     Define FUSE_IMPLEMENTATION separately for core/docking; link Rend, Type
 *     and libm. This header never enables their implementations implicitly.
 *     Override FUSE_REND_REND_HEADER / FUSE_REND_TYPE_HEADER with quoted header
 *     paths when using dependencies outside the default project layout.
 *
 * uint32_t color is AARRGGBB. CLIP_START/END are applied on the CPU.
 * Extra quads can be appended after fuse cmds, then flush.
 *
 * Text is Type. FUSE_CMD_TEXT is a clipped run plus an optional caret.
 * fuse_rend_text_init takes the coverage-atlas shaders
 * (godstack/FuseTextExample.slang). The renderer must expose texture
 * binding 0. fuse_rend_text_type borrows a placed TypeCtx; font bytes
 * stay alive. Glyphs are emitted after fuse_rend_begin. fuse_rend_prepare
 * uploads changed resources after emission, outside a frame/render pass.
 * Cached text needs no upload. Each renderer tracks its own atlas revision.
 * fuse_rend_flush draws solids, then glyphs in each ordered batch (or once
 * for the whole frame without batches). Color emoji is not sampled.
 * =========================================================================== */

#ifndef FUSE_REND_H
#define FUSE_REND_H

#include "Fuse.h"
#ifndef FUSE_REND_REND_HEADER
#define FUSE_REND_REND_HEADER "Rend.h"
#endif
#ifndef FUSE_REND_TYPE_HEADER
#define FUSE_REND_TYPE_HEADER "Type.h"
#endif
/* Reuse dependencies already supplied by the consumer's unity build. */
#ifndef _REND_H_
#include FUSE_REND_REND_HEADER
#endif
#ifndef TYPE_H
#include FUSE_REND_TYPE_HEADER
#endif

#define FUSE_REND_CLIP_MAX 16
#define FUSE_REND_BATCH_RANGE_API 1

typedef struct FuseRendVertex {
    float x, y;
    float r, g, b, a;
} FuseRendVertex;

typedef struct FuseRendGlyph {
    float x, y, u, v;
    float r, g, b, a;
} FuseRendGlyph;

typedef struct FuseRendBatch {
    size_t vertex_start, glyph_start;
} FuseRendBatch;

typedef struct FuseRend {
    FuseRendBatch *batches;
    size_t batch_cap, batch_count;
    RendRenderer prepared_renderer; /* Geometry uploaded by prepare this frame. */
    RendPipeline pipeline;
    RendBuffer vbo;
    FuseRendVertex *verts;
    size_t vert_cap;
    size_t vert_count;
    int overflow; /* Solid/glyph/batch/clip capacity exceeded; reset by begin.
                   * Flush suppresses the entire frame, including uploads. */
    float clip[FUSE_REND_CLIP_MAX][4];
    uint32_t clip_n;
    float screen_w, screen_h;
    RendPipeline text_pipeline;
    RendBuffer text_vbo;
    RendTexture atlas;
    RendRenderer text_renderer;
    FuseRendGlyph *glyphs;
    size_t glyph_cap;
    size_t glyph_count;
    TypeCtx *type;
    float atlas_w, atlas_h;
    int atlas_on;
    uint64_t atlas_revision;
    int atlas_uploaded;
    const FuseFieldRun *fields;
    size_t field_n;
} FuseRend;

size_t fuse_rend_memory(size_t max_verts);

int  fuse_rend_init(FuseRend *fr, void *cpu_buf, size_t cpu_size,
                    RendRenderer renderer,
                    uint8_t *vert_spv, size_t vert_size,
                    uint8_t *frag_spv, size_t frag_size);
/* After fuse_rend_init. cpu_buf holds FuseRendGlyph. Shaders sample binding 0. */
int  fuse_rend_text_init(FuseRend *fr, void *cpu_buf, size_t cpu_size,
                         RendRenderer renderer,
                         uint8_t *vert_spv, size_t vert_size,
                         uint8_t *frag_spv, size_t frag_size);
/* Creates the R8 atlas from type_ctx_atlas and binds it. Call once. */
int  fuse_rend_text_type(FuseRend *fr, RendRenderer renderer, TypeCtx *type);
void fuse_rend_shutdown(FuseRend *fr);

/* Optional caller backing, borrowed through shutdown. Set between frames.
 * NULL disables batches. Non-NULL with zero capacity cannot begin a layer. */
void fuse_rend_batches(FuseRend *fr, FuseRendBatch *batches, size_t capacity);
/* Begins an ordered solids/glyphs layer; returns 1 on success, 0 on failure.
 * First call includes any geometry already emitted this frame. Subsequent
 * calls start at the current offsets; empty layers are allowed. begin resets
 * layers, not backing. Without layer calls flush retains single-layer order.
 * Missing backing/capacity exhaustion suppresses the frame until next begin. */
int fuse_rend_layer(FuseRend *fr);
/* Replay a complete command stream's clip structure and only this layer's
 * geometry. Restores the incoming clip stack. Does not begin a batch. */
void fuse_rend_cmds_layer(FuseRend *fr, const FuseCmd *cmds, size_t n, int16_t layer);
void fuse_rend_begin(FuseRend *fr, float screen_w, float screen_h);
/* Binds the runs from fuse_canvas_fields. Call before fuse_rend_cmds. */
void fuse_rend_fields(FuseRend *fr, const FuseFieldRun *runs, size_t n);
void fuse_rend_cmds(FuseRend *fr, const FuseCmd *cmds, size_t n);
void fuse_rend_quad(FuseRend *fr, float x, float y, float w, float h, uint32_t color);
/* CPU-clipped triangle. Capacity failure emits nothing and sets overflow. */
void fuse_rend_triangle(FuseRend *fr, const FuseCmdTriangle *triangle);
/* Baseline y. Pen starts at x. Returns the advance in pixels. */
float fuse_rend_text(FuseRend *fr, float x, float y, float size_px, const char *utf8, uint32_t color);
float fuse_rend_text_width(FuseRend *fr, float size_px, const char *utf8);
float fuse_rend_text_styled(FuseRend *fr, float x, float y, float size_px, const char *utf8, TypeStyle style, uint32_t color);
/* After all geometry/glyph emission, before frame_begin. Uploads geometry once
 * per begin and only changed atlas resources. No emission after preparation.
 * Overflow suppresses all uploads. Must run with no visible glyphs too: clears
 * invalidate old GPU pixels. No atlas work when its revision is unchanged. */
void fuse_rend_prepare(FuseRend *fr, RendRenderer renderer);
/* Compatibility wrapper; prefer fuse_rend_prepare. */
void fuse_rend_text_sync(FuseRend *fr, RendRenderer renderer);
/* Draw half-open batch range [first, end) in the current pass, after prepare.
 * No uploads/allocations. Without explicit batches the frame has one batch.
 * Empty ranges succeed; invalid bounds/offsets, overflow or missing preparation
 * return 0 without backend calls. Validate before submitting any part of a frame.
 * Full flush remains compatible and prepares geometry lazily if necessary. */
int fuse_rend_flush_range(FuseRend *fr, RendRenderer renderer, size_t first, size_t end);
void fuse_rend_flush(FuseRend *fr, RendRenderer renderer);

#endif /* FUSE_REND_H */

#ifdef FUSE_REND_IMPLEMENTATION
#undef FUSE_REND_IMPLEMENTATION
#ifndef FUSE_REND_IMPLEMENTATION_ONCE
#define FUSE_REND_IMPLEMENTATION_ONCE

/* Renderer adapter - FuseCmd to Rend triangles, Type glyphs to a coverage atlas
 * 0.0.1 - @vasco - rect, cpu clip
 * 0.1.0 - @vasco - type font rendering
 * 0.2.0 - prepared geometry and bounded batch-range drawing
 */

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>


typedef struct FuseRendPC {
    float screen_w, screen_h, pad0, pad1;
} FuseRendPC;

typedef struct FuseRendTextPC {
    float screen_w, screen_h, atlas_w, atlas_h;
} FuseRendTextPC;

static void fuse_rend_internal_color(uint32_t c, float *r, float *g, float *b, float *a);
static int fuse_rend_internal_clip_rect(FuseRend *fr, float *x, float *y, float *w, float *h);
static int fuse_rend_internal_clip_uv(FuseRend *fr, float *x, float *y, float *w, float *h, float *u0, float *v0, float *u1, float *v1);
static void fuse_rend_internal_emit6(FuseRend *fr, float x0, float y0, float x1, float y1, uint32_t color);
static void fuse_rend_internal_glyph(FuseRend *fr, float x, float y, const TypeGlyph *g, uint32_t color);
static float fuse_rend_text_nwidth(FuseRend *fr, float size_px, const char *utf8, int nbytes);
static float fuse_rend_baseline(FuseRend *fr, float y, float h, float size);
static void fuse_rend_text_cmd(FuseRend *fr, const FuseCmd *cmd);
static int fuse_rend_internal_valid(const FuseRend *fr);
static void fuse_rend_internal_prepare_geometry(FuseRend *fr, RendRenderer renderer);

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

static int
fuse_rend_internal_clip_uv(FuseRend *fr, float *x, float *y, float *w, float *h, float *u0, float *v0, float *u1, float *v1)
{
    float x0, y0, x1, y1, nx0, ny0, nx1, ny1;
    float cx, cy, cw, ch, du, dv;
    if (fr->clip_n == 0)
        return 1;
    cx = fr->clip[fr->clip_n - 1][0];
    cy = fr->clip[fr->clip_n - 1][1];
    cw = fr->clip[fr->clip_n - 1][2];
    ch = fr->clip[fr->clip_n - 1][3];
    x0 = *x;
    y0 = *y;
    x1 = *x + *w;
    y1 = *y + *h;
    nx0 = x0 > cx ? x0 : cx;
    ny0 = y0 > cy ? y0 : cy;
    nx1 = x1 < (cx + cw) ? x1 : (cx + cw);
    ny1 = y1 < (cy + ch) ? y1 : (cy + ch);
    if (nx1 <= nx0 || ny1 <= ny0 || *w <= 0.0f || *h <= 0.0f)
        return 0;
    du = (*u1 - *u0) / *w;
    dv = (*v1 - *v0) / *h;
    *u0 = *u0 + (nx0 - x0) * du;
    *u1 = *u1 - (x1 - nx1) * du;
    *v0 = *v0 + (ny0 - y0) * dv;
    *v1 = *v1 - (y1 - ny1) * dv;
    *x = nx0;
    *y = ny0;
    *w = nx1 - nx0;
    *h = ny1 - ny0;
    return 1;
}

static void
fuse_rend_internal_emit6(FuseRend *fr, float x0, float y0, float x1, float y1, uint32_t color)
{
    FuseRendVertex *v;
    float r, g, b, a;
    size_t i;
    if (fr->vert_count > fr->vert_cap || fr->vert_cap - fr->vert_count < 6) {
        fr->overflow = 1;
        return;
    }
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

static void
fuse_rend_internal_glyph(FuseRend *fr, float x, float y, const TypeGlyph *g, uint32_t color)
{
    FuseRendGlyph *v;
    float gx, gy, w, h, u0, v0, u1, v1;
    float r, gv, b, a;
    size_t i;
    if (!g || g->width == 0 || g->height == 0 || g->color)
        return;
    if (fr->atlas_w <= 0.0f || fr->atlas_h <= 0.0f)
        return;
    gx = floorf(x + g->bearing_x + 0.5f);
    gy = floorf(y - g->bearing_y + 0.5f);
    w = (float)g->width;
    h = (float)g->height;
    u0 = (float)g->atlas_x / fr->atlas_w;
    v0 = (float)g->atlas_y / fr->atlas_h;
    u1 = (float)(g->atlas_x + g->width) / fr->atlas_w;
    v1 = (float)(g->atlas_y + g->height) / fr->atlas_h;
    if (!fuse_rend_internal_clip_uv(fr, &gx, &gy, &w, &h, &u0, &v0, &u1, &v1))
        return;
    if (fr->glyph_count > fr->glyph_cap || fr->glyph_cap - fr->glyph_count < 6) {
        fr->overflow = 1;
        return;
    }
    fuse_rend_internal_color(color, &r, &gv, &b, &a);
    v = &fr->glyphs[fr->glyph_count];
    v[0].x = gx;     v[0].y = gy;     v[0].u = u0; v[0].v = v0;
    v[1].x = gx + w; v[1].y = gy;     v[1].u = u1; v[1].v = v0;
    v[2].x = gx;     v[2].y = gy + h; v[2].u = u0; v[2].v = v1;
    v[3].x = gx;     v[3].y = gy + h; v[3].u = u0; v[3].v = v1;
    v[4].x = gx + w; v[4].y = gy;     v[4].u = u1; v[4].v = v0;
    v[5].x = gx + w; v[5].y = gy + h; v[5].u = u1; v[5].v = v1;
    for (i = 0; i < 6; i++) {
        v[i].r = r;
        v[i].g = gv;
        v[i].b = b;
        v[i].a = a;
    }
    fr->glyph_count += 6;
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

int
fuse_rend_text_init(FuseRend *fr, void *cpu_buf, size_t cpu_size,
                    RendRenderer renderer,
                    uint8_t *vert_spv, size_t vert_size,
                    uint8_t *frag_spv, size_t frag_size)
{
    RendVertexBinding bind;
    RendVertexAttributes attrs[3];
    RendPushConstantInfo pc;
    size_t cap;
    if (!fr || !cpu_buf || !renderer || !vert_spv || !frag_spv)
        return 0;
    cap = cpu_size / sizeof (FuseRendGlyph);
    if (cap < 6)
        return 0;
    fr->glyphs = cpu_buf;
    fr->glyph_cap = cap;
    fr->glyph_count = 0;
    fr->text_renderer = renderer;
    bind.binding = 0;
    bind.stride = sizeof (FuseRendGlyph);
    bind.input_rate = REND_INPUT_RATE_VERTEX;
    attrs[0].location = 0;
    attrs[0].binding = 0;
    attrs[0].offset = offsetof(FuseRendGlyph, x);
    attrs[0].format = REND_FORMAT_2_SFLOAT32;
    attrs[1].location = 1;
    attrs[1].binding = 0;
    attrs[1].offset = offsetof(FuseRendGlyph, u);
    attrs[1].format = REND_FORMAT_2_SFLOAT32;
    attrs[2].location = 2;
    attrs[2].binding = 0;
    attrs[2].offset = offsetof(FuseRendGlyph, r);
    attrs[2].format = REND_FORMAT_4_SFLOAT32;
    pc.offset = 0;
    pc.size = sizeof (FuseRendTextPC);
    fr->text_pipeline = rend_pipeline_create_graphics_spirv(
        renderer,
        vert_spv, vert_size,
        frag_spv, frag_size,
        &bind, 1,
        attrs, 3,
        &pc, 1,
        REND_POLYGON_MODE_FILL,
        REND_CULL_MODE_NONE,
        REND_TOPOLOGY_TRIANGLE_LIST,
        REND_FORMAT_UNDEFINED,
        false);
    if (!fr->text_pipeline)
        return 0;
    rend_pipeline_set_blend(fr->text_pipeline, true);
    fr->text_vbo = rend_buffer_create(renderer, cap * sizeof (FuseRendGlyph), REND_BUFFER_VERTEX, false);
    return 1;
}

int
fuse_rend_text_type(FuseRend *fr, RendRenderer renderer, TypeCtx *type)
{
    const uint8_t *px;
    uint32_t w, h;
    if (!fr || !renderer || !type || !fr->text_pipeline)
        return 0;
    px = type_ctx_atlas(type, &w, &h);
    if (!px || w == 0 || h == 0)
        return 0;
    if (fr->atlas_on)
        rend_texture_destroy(renderer, &fr->atlas);
    fr->atlas = rend_texture_create(renderer, w, h, 1, 0, 1, REND_FORMAT_R8_UNORM);
    fr->atlas_w = (float)w;
    fr->atlas_h = (float)h;
    fr->type = type;
    fr->atlas_on = 1;
    fr->atlas_uploaded = 0;
    fr->atlas_revision = 0;
    rend_descriptor_write_texture(renderer, &fr->atlas, 0, 0);
    return 1;
}

void
fuse_rend_shutdown(FuseRend *fr)
{
    if (!fr)
        return;
    if (fr->atlas_on && fr->text_renderer) {
        rend_texture_destroy(fr->text_renderer, &fr->atlas);
        fr->atlas_on = 0;
    }
    if (fr->text_pipeline)
        rend_buffer_destroy(&fr->text_vbo);
    rend_buffer_destroy(&fr->vbo);
    fr->verts = NULL;
    fr->glyphs = NULL;
    fr->pipeline = NULL;
    fr->text_pipeline = NULL;
    fr->type = NULL;
}

void
fuse_rend_fields(FuseRend *fr, const FuseFieldRun *runs, size_t n)
{
    if (!fr)
        return;
    fr->fields = runs;
    fr->field_n = n;
}

void
fuse_rend_begin(FuseRend *fr, float screen_w, float screen_h)
{
    if (!fr)
        return;
    fr->prepared_renderer = NULL;
    fr->batch_count = 0;
    fr->vert_count = 0;
    fr->overflow = 0;
    fr->glyph_count = 0;
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
fuse_rend_triangle(FuseRend *fr, const FuseCmdTriangle *triangle)
{
    float points[2][8][2], bound, a, b, t, r, g, blue, alpha;
    int n = 3, out, edge, axis, i, j, k, source = 0;
    int inside_a, inside_b;
    size_t needed;
    FuseRendVertex *v;

    if (!fr || !triangle)
        return;
    points[0][0][0] = triangle->x1; points[0][0][1] = triangle->y1;
    points[0][1][0] = triangle->x2; points[0][1][1] = triangle->y2;
    points[0][2][0] = triangle->x3; points[0][2][1] = triangle->y3;
    for (i = 0; i < 3; i++)
        if (!isfinite(points[0][i][0]) || !isfinite(points[0][i][1]))
            return;
    if (fr->clip_n) {
        const float *clip = fr->clip[fr->clip_n - 1];
        if (clip[2] <= 0.0f || clip[3] <= 0.0f)
            return;
        /* A triangle clipped to a rectangle has at most seven vertices. */
        for (edge = 0; edge < 4 && n; edge++) {
            axis = edge / 2;
            bound = clip[axis] + (edge % 2 ? clip[axis + 2] : 0.0f);
            out = 0;
            for (i = 0; i < n; i++) {
                j = (i + n - 1) % n;
                a = points[source][j][axis];
                b = points[source][i][axis];
                inside_a = edge % 2 ? a <= bound : a >= bound;
                inside_b = edge % 2 ? b <= bound : b >= bound;
                if (inside_a != inside_b) {
                    t = (bound - a) / (b - a);
                    for (k = 0; k < 2; k++)
                        points[!source][out][k] = points[source][j][k] +
                            t * (points[source][i][k] - points[source][j][k]);
                    points[!source][out++][axis] = bound;
                }
                if (inside_b) {
                    points[!source][out][0] = points[source][i][0];
                    points[!source][out++][1] = points[source][i][1];
                }
            }
            n = out;
            source = !source;
        }
    }
    if (n < 3)
        return;
    needed = (size_t)(n - 2) * 3;
    if (fr->vert_count > fr->vert_cap || needed > fr->vert_cap - fr->vert_count) {
        fr->overflow = 1;
        return;
    }
    fuse_rend_internal_color(triangle->color, &r, &g, &blue, &alpha);
    v = fr->verts + fr->vert_count;
    for (i = 1; i < n - 1; i++) {
        int indices[3] = {0, i, i + 1};
        for (j = 0; j < 3; j++, v++) {
            v->x = points[source][indices[j]][0];
            v->y = points[source][indices[j]][1];
            v->r = r; v->g = g; v->b = blue; v->a = alpha;
        }
    }
    fr->vert_count += needed;
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
        case FUSE_CMD_TRIANGLE:
            fuse_rend_triangle(fr, &cmds[i].triangle);
            break;
        case FUSE_CMD_CLIP_START:
            if (fr->clip_n < FUSE_REND_CLIP_MAX) {
                float x, y, w, h;
                x = cmds[i].clip.x;
                y = cmds[i].clip.y;
                w = cmds[i].clip.w;
                h = cmds[i].clip.h;
                if (!fuse_rend_internal_clip_rect(fr, &x, &y, &w, &h))
                    w = h = 0.0f;
                fr->clip[fr->clip_n][0] = x;
                fr->clip[fr->clip_n][1] = y;
                fr->clip[fr->clip_n][2] = w;
                fr->clip[fr->clip_n][3] = h;
                fr->clip_n++;
            } else {
                /* Never silently pop a parent after ignoring a deeper push. */
                fr->overflow = 1;
                return;
            }
            break;
        case FUSE_CMD_CLIP_END:
            if (fr->clip_n > 1)
                fr->clip_n--;
            break;
        case FUSE_CMD_TEXT:
            fuse_rend_text_cmd(fr, &cmds[i]);
            break;
        default:
            break;
        }
    }
}

static float
fuse_rend_text_nwidth(FuseRend *fr, float size_px, const char *utf8, int nbytes)
{
    const char *p;
    float pen;
    int left;
    if (!fr || !fr->type || !utf8 || !utf8[0] || !(size_px > 0.0f) || nbytes == 0)
        return 0.0f;
    if (nbytes < 0)
        nbytes = (int)strlen(utf8);
    pen = 0.0f;
    p = utf8;
    left = nbytes;
    while (left > 0 && *p) {
        TypeGlyph g;
        uint32_t cp;
        uint32_t n;
        TypeError err;
        cp = 0;
        n = type_utf8_next(p, (size_t)left, &cp);
        if (n == 0)
            break;
        err = type_glyph(fr->type, cp, size_px, &g);
        if (err == TYPE_ERR_ATLAS_FULL) {
            type_clear_atlas(fr->type);
            err = type_glyph(fr->type, cp, size_px, &g);
        }
        if (err != TYPE_OK)
            break;
        pen += g.advance;
        p += n;
        left -= (int)n;
    }
    return pen;
}

static float
fuse_rend_baseline(FuseRend *fr, float y, float h, float size)
{
    TypeMetrics m;

    if (!fr || !fr->type || type_metrics(fr->type, size, &m) != TYPE_OK)
        return y + h * 0.72f;
    return y + (h - (m.ascender - m.descender)) * 0.5f + m.ascender;
}

static void
fuse_rend_text_cmd(FuseRend *fr, const FuseCmd *cmd)
{
    const FuseFieldRun *run;
    const char *s;
    float prefix;
    float scroll;
    float base;
    float pen;
    float ch;
    float x, y, w, h;

    if (!fr || !cmd)
        return;
    if (!fr->fields || cmd->text.slot >= fr->field_n)
        return;
    run = &fr->fields[cmd->text.slot];
    s = run->str ? run->str : "";
    x = cmd->text.x;
    y = cmd->text.y;
    w = cmd->text.w;
    h = cmd->text.h;
    prefix = 0.0f;
    if (run->caret > 0)
        prefix = fuse_rend_text_nwidth(fr, run->size, s, run->caret);
    scroll = 0.0f;
    if (prefix > w - 1.0f)
        scroll = prefix - (w - 1.0f);
    base = fuse_rend_baseline(fr, y, h, run->size);
    pen = x - scroll;
    if (s[0])
        fuse_rend_text(fr, pen, base, run->size, s, run->color);
    if (run->caret < 0)
        return;
    ch = h - 12.0f;
    if (ch < 2.0f)
        ch = h > 2.0f ? h - 2.0f : h;
    fuse_rend_quad(fr, pen + prefix, y + (h - ch) * 0.5f, 1.0f, ch, run->caret_color);
}

float
fuse_rend_text_width(FuseRend *fr, float size_px, const char *utf8)
{
    const char *p;
    float pen;
    size_t left;
    if (!fr || !fr->type || !utf8 || !utf8[0] || !(size_px > 0.0f))
        return 0.0f;
    pen = 0.0f;
    p = utf8;
    left = strlen(utf8);
    while (left) {
        TypeGlyph g;
        uint32_t cp;
        uint32_t n;
        TypeError err;
        cp = 0;
        n = type_utf8_next(p, left, &cp);
        if (n == 0)
            break;
        err = type_glyph(fr->type, cp, size_px, &g);
        if (err == TYPE_ERR_ATLAS_FULL) {
            type_clear_atlas(fr->type);
            err = type_glyph(fr->type, cp, size_px, &g);
        }
        if (err != TYPE_OK)
            break;
        pen += g.advance;
        p += n;
        left -= n;
    }
    return pen;
}

float
fuse_rend_text(FuseRend *fr, float x, float y, float size_px, const char *utf8, uint32_t color)
{
    TypeStyle style;
    memset(&style, 0, sizeof style);
    return fuse_rend_text_styled(fr, x, y, size_px, utf8, style, color);
}

float
fuse_rend_text_styled(FuseRend *fr, float x, float y, float size_px, const char *utf8, TypeStyle style, uint32_t color)
{
    const char *p;
    float pen;
    size_t left;
    if (!fr || !fr->type || !fr->glyphs || !utf8 || !utf8[0] || !(size_px > 0.0f))
        return 0.0f;
    pen = floorf(x + 0.5f);
    y = floorf(y + 0.5f);
    p = utf8;
    left = strlen(utf8);
    while (left) {
        TypeGlyph g;
        uint32_t cp;
        uint32_t n;
        TypeError err;
        cp = 0;
        n = type_utf8_next(p, left, &cp);
        if (n == 0)
            break;
        err = type_glyph_styled(fr->type, cp, size_px, style, &g);
        if (err == TYPE_ERR_ATLAS_FULL) {
            type_clear_atlas(fr->type);
            err = type_glyph_styled(fr->type, cp, size_px, style, &g);
        }
        if (err != TYPE_OK)
            break;
        fuse_rend_internal_glyph(fr, pen, y, &g, color);
        pen += g.advance;
        p += n;
        left -= n;
    }
    return pen - floorf(x + 0.5f);
}

int
fuse_rend_internal_valid(const FuseRend *fr)
{
	size_t i, vertex = 0, glyph = 0;

	if (!fr || fr->overflow || fr->vert_count > fr->vert_cap || fr->glyph_count > fr->glyph_cap ||
		fr->batch_count > fr->batch_cap || (fr->batch_count && !fr->batches))
		return 0;
	for (i = 0; i < fr->batch_count; i++) {
		if (fr->batches[i].vertex_start < vertex || fr->batches[i].vertex_start > fr->vert_count ||
			fr->batches[i].glyph_start < glyph || fr->batches[i].glyph_start > fr->glyph_count)
			return 0;
		vertex = fr->batches[i].vertex_start;
		glyph = fr->batches[i].glyph_start;
	}
	return !fr->batch_count || (!fr->batches[0].vertex_start && !fr->batches[0].glyph_start);
}

void
fuse_rend_internal_prepare_geometry(FuseRend *fr, RendRenderer renderer)
{
	if (!renderer || !fuse_rend_internal_valid(fr) || fr->prepared_renderer == renderer)
		return;
	if (fr->pipeline && fr->vert_count)
		rend_buffer_write(renderer, &fr->vbo, fr->verts, fr->vert_count * sizeof(FuseRendVertex), 0);
	if (fr->text_pipeline && fr->glyph_count)
		rend_buffer_write(renderer, &fr->text_vbo, fr->glyphs, fr->glyph_count * sizeof(FuseRendGlyph), 0);
	fr->prepared_renderer = renderer;
}

void
fuse_rend_prepare(FuseRend *fr, RendRenderer renderer)
{
    const uint8_t *px;
    uint32_t w, h;
    uint64_t revision;
    if (!renderer || !fuse_rend_internal_valid(fr))
        return;
    fuse_rend_internal_prepare_geometry(fr, renderer);
    if (!fr->atlas_on || !fr->type)
        return;
    revision = type_ctx_atlas_revision(fr->type);
    if (fr->atlas_uploaded && fr->atlas_revision == revision)
        return;
    px = type_ctx_atlas(fr->type, &w, &h);
    if (!px || w == 0 || h == 0)
        return;
    if ((float)w != fr->atlas_w || (float)h != fr->atlas_h)
        return;
    rend_texture_copy_data(renderer, &fr->atlas, px, (size_t)w * (size_t)h);
    fr->atlas_revision = revision;
    fr->atlas_uploaded = 1;
}

void
fuse_rend_text_sync(FuseRend *fr, RendRenderer renderer)
{
    fuse_rend_prepare(fr, renderer);
}

void
fuse_rend_batches(FuseRend *fr, FuseRendBatch *batches, size_t capacity)
{
	if (!fr)
		return;
	fr->batches = batches;
	fr->batch_cap = batches ? capacity : 0;
	fr->batch_count = 0;
}

int
fuse_rend_layer(FuseRend *fr)
{
	if (!fr)
		return 0;
	if (fr->overflow)
		return 0;
	if (!fr->batches || fr->batch_count >= fr->batch_cap) {
		fr->overflow = 1;
		return 0;
	}
	/* Geometry emitted before the first explicit layer belongs to that layer. */
	fr->batches[fr->batch_count] = fr->batch_count ?
		(FuseRendBatch){fr->vert_count, fr->glyph_count} : (FuseRendBatch){0, 0};
	fr->batch_count++;
	return 1;
}

void
fuse_rend_cmds_layer(FuseRend *fr, const FuseCmd *cmds, size_t n, int16_t layer)
{
	float clip[FUSE_REND_CLIP_MAX][4];
	uint32_t clip_n;
	size_t i;
	if (!fr || !cmds || fr->overflow)
		return;
	clip_n = fr->clip_n;
	memcpy(clip, fr->clip, sizeof(clip));
	/* Ancestor clips may belong to another layer. Replay all clip structure,
	 * but only the selected layer's geometry. Do not leak its clips to labels. */
	for (i = 0; i < n && !fr->overflow; i++)
		if (cmds[i].z == layer || cmds[i].type == FUSE_CMD_CLIP_START ||
		    cmds[i].type == FUSE_CMD_CLIP_END)
			fuse_rend_cmds(fr, cmds + i, 1);
	memcpy(fr->clip, clip, sizeof(clip));
	fr->clip_n = clip_n;
}

int
fuse_rend_flush_range(FuseRend *fr, RendRenderer renderer, size_t first, size_t limit)
{
	FuseRendPC pc;
	FuseRendTextPC tpc;
	size_t i, start, end, count;
	if (!renderer || !fuse_rend_internal_valid(fr))
		return 0;
	count = fr->batch_count ? fr->batch_count : 1;
	if (first > limit || limit > count)
		return 0;
	if (first == limit)
		return 1;
	if (fr->prepared_renderer != renderer)
		return 0;
	pc = (FuseRendPC){fr->screen_w, fr->screen_h, 0, 0};
	tpc = (FuseRendTextPC){fr->screen_w, fr->screen_h, fr->atlas_w, fr->atlas_h};
	for (i = first; i < limit; i++) {
		start = fr->batch_count ? fr->batches[i].vertex_start : 0;
		end = i + 1 < fr->batch_count ? fr->batches[i + 1].vertex_start : fr->vert_count;
		if (fr->pipeline && end > start) {
			rend_cmd_bind_pipeline(fr->pipeline);
			rend_cmd_push_constants(fr->pipeline, &pc, sizeof(pc));
			rend_cmd_bind_vertex_buffer(fr->pipeline, 0, fr->vbo, start * sizeof(FuseRendVertex));
			rend_cmd_draw(fr->pipeline, end - start, 1);
		}
		start = fr->batch_count ? fr->batches[i].glyph_start : 0;
		end = i + 1 < fr->batch_count ? fr->batches[i + 1].glyph_start : fr->glyph_count;
		if (fr->text_pipeline && end > start) {
			rend_cmd_bind_pipeline(fr->text_pipeline);
			rend_cmd_push_constants(fr->text_pipeline, &tpc, sizeof(tpc));
			rend_cmd_bind_vertex_buffer(fr->text_pipeline, 0, fr->text_vbo, start * sizeof(FuseRendGlyph));
			rend_cmd_draw(fr->text_pipeline, end - start, 1);
		}
	}
	return 1;
}

void
fuse_rend_flush(FuseRend *fr, RendRenderer renderer)
{
	fuse_rend_internal_prepare_geometry(fr, renderer);
	if (fr)
		fuse_rend_flush_range(fr, renderer, 0, fr->batch_count ? fr->batch_count : 1);
}

#endif /* FUSE_REND_IMPLEMENTATION_ONCE */
#endif /* FUSE_REND_IMPLEMENTATION */

/*
------------------------------------------------------------------------------
MIT License
Copyright (c) 2026 Vasco Alves
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
*/
