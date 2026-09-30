/* ===========================================================================
 * FUSE REND - Tessellate FuseCmd into a Rend 2D pipeline
 *
 * uint32_t color is AARRGGBB. CLIP_START/END are applied on the CPU.
 * Extra quads can be appended after fuse cmds, then flush.
 *
 * Text is Type. FUSE_CMD_TEXT is a clipped run plus an optional caret.
 * fuse_rend_text_init takes the coverage-atlas shaders
 * (godstack/Fuse/fuse_text.slang). The renderer must expose texture
 * binding 0. fuse_rend_text_type borrows a placed TypeCtx; font bytes
 * stay alive. Glyphs are emitted after fuse_rend_begin. fuse_rend_prepare
 * uploads changed resources after emission, outside a frame/render pass.
 * Cached text needs no upload. Each renderer tracks its own atlas revision.
 * fuse_rend_flush draws solids, then glyphs. Color emoji is not sampled.
 * =========================================================================== */

#ifndef FUSE_REND_H
#define FUSE_REND_H

#include "fuse.h"
#include "../Rend/rend.h"
#include "../Type/type.h"

#define FUSE_REND_CLIP_MAX 16

typedef struct FuseRendVertex {
    float x, y;
    float r, g, b, a;
} FuseRendVertex;

typedef struct FuseRendGlyph {
    float x, y, u, v;
    float r, g, b, a;
} FuseRendGlyph;

typedef struct FuseRend {
    RendPipeline pipeline;
    RendBuffer vbo;
    FuseRendVertex *verts;
    size_t vert_cap;
    size_t vert_count;
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

void fuse_rend_begin(FuseRend *fr, float screen_w, float screen_h);
/* Binds the runs from fuse_canvas_fields. Call before fuse_rend_cmds. */
void fuse_rend_fields(FuseRend *fr, const FuseFieldRun *runs, size_t n);
void fuse_rend_cmds(FuseRend *fr, const FuseCmd *cmds, size_t n);
void fuse_rend_quad(FuseRend *fr, float x, float y, float w, float h, uint32_t color);
/* Baseline y. Pen starts at x. Returns the advance in pixels. */
float fuse_rend_text(FuseRend *fr, float x, float y, float size_px, const char *utf8, uint32_t color);
float fuse_rend_text_width(FuseRend *fr, float size_px, const char *utf8);
float fuse_rend_text_styled(FuseRend *fr, float x, float y, float size_px, const char *utf8, TypeStyle style, uint32_t color);
/* After all glyph emission, before frame_begin. Uploads only changed resources.
 * Must run even when no glyphs are visible: clears invalidate old GPU pixels.
 * No allocation, pixel scan, or GPU work when the atlas revision is unchanged. */
void fuse_rend_prepare(FuseRend *fr, RendRenderer renderer);
/* Compatibility wrapper; prefer fuse_rend_prepare. */
void fuse_rend_text_sync(FuseRend *fr, RendRenderer renderer);
void fuse_rend_flush(FuseRend *fr, RendRenderer renderer);

#endif /* FUSE_REND_H */
