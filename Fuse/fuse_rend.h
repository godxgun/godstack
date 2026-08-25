/* ===========================================================================
 * FUSE REND - Tessellate FuseCmd into a Rend 2D pipeline
 *
 * uint32_t color is AARRGGBB. CLIP_START/END are applied on the CPU.
 * Extra quads can be appended after fuse cmds, then flush.
 * =========================================================================== */

#ifndef FUSE_REND_H
#define FUSE_REND_H

#include "fuse.h"
#include "rend.h"

#define FUSE_REND_CLIP_MAX 16

typedef struct FuseRendVertex {
    float x, y;
    float r, g, b, a;
} FuseRendVertex;

typedef struct FuseRend {
    RendPipeline pipeline;
    RendBuffer vbo;
    FuseRendVertex *verts;
    size_t vert_cap;
    size_t vert_count;
    float clip[FUSE_REND_CLIP_MAX][4];
    uint32_t clip_n;
    float screen_w, screen_h;
} FuseRend;

size_t fuse_rend_memory(size_t max_verts);

int  fuse_rend_init(FuseRend *fr, void *cpu_buf, size_t cpu_size,
                    RendRenderer renderer,
                    uint8_t *vert_spv, size_t vert_size,
                    uint8_t *frag_spv, size_t frag_size);
void fuse_rend_shutdown(FuseRend *fr);

void fuse_rend_begin(FuseRend *fr, float screen_w, float screen_h);
void fuse_rend_cmds(FuseRend *fr, const FuseCmd *cmds, size_t n);
void fuse_rend_quad(FuseRend *fr, float x, float y, float w, float h, uint32_t color);
void fuse_rend_flush(FuseRend *fr, RendRenderer renderer);

#endif /* FUSE_REND_H */
