/* fuse_rend.c - FuseCmd to Rend triangles, Type glyphs to a coverage atlas
 * 0.0.1 - @vasco - rect, cpu clip
 * 0.1.0 - @vasco - type font rendering
 */

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "fuse_rend.h"

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
    if (fr->glyph_count + 6 > fr->glyph_cap)
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
    fr->vert_count = 0;
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

void
fuse_rend_prepare(FuseRend *fr, RendRenderer renderer)
{
    const uint8_t *px;
    uint32_t w, h;
    uint64_t revision;
    if (!fr || !renderer || !fr->atlas_on || !fr->type)
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
fuse_rend_flush(FuseRend *fr, RendRenderer renderer)
{
    FuseRendPC pc;
    FuseRendTextPC tpc;
    if (!fr || !renderer)
        return;
    if (fr->pipeline && fr->vert_count) {
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
    if (!fr->text_pipeline || fr->glyph_count == 0)
        return;
    tpc.screen_w = fr->screen_w;
    tpc.screen_h = fr->screen_h;
    tpc.atlas_w = fr->atlas_w;
    tpc.atlas_h = fr->atlas_h;
    rend_buffer_write(renderer, &fr->text_vbo, fr->glyphs, fr->glyph_count * sizeof (FuseRendGlyph), 0);
    rend_cmd_bind_pipeline(fr->text_pipeline);
    rend_cmd_push_constants(fr->text_pipeline, &tpc, sizeof tpc);
    rend_cmd_bind_vertex_buffer(fr->text_pipeline, 0, fr->text_vbo, 0);
    rend_cmd_draw(fr->text_pipeline, fr->glyph_count, 1);
}
