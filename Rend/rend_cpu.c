#include <math.h>
#include <stdint.h>
#include <string.h>

#define REND_CPU_MAX_PIPELINES 32
#define REND_CPU_MAX_ATTR 16
#define REND_CPU_ATTR_STRIDE 16
#define REND_CPU_PUSH_MAX 256

#define REND_CPU_ISA_SCALAR 0
#define REND_CPU_ISA_SSE    1
#define REND_CPU_ISA_SSE2   2
#define REND_CPU_ISA_AVX    3

#if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
#define REND_CPU_X86 1
#else
#define REND_CPU_X86 0
#endif

#if REND_CPU_X86 && (defined(__GNUC__) || defined(__clang__))
#define REND_CPU_TARGET_SSE  __attribute__((target("sse")))
#define REND_CPU_TARGET_SSE2 __attribute__((target("sse2")))
#define REND_CPU_TARGET_AVX  __attribute__((target("avx")))
#define REND_CPU_AVX_CODE 1
#elif REND_CPU_X86 && defined(__AVX__)
#define REND_CPU_TARGET_SSE
#define REND_CPU_TARGET_SSE2
#define REND_CPU_TARGET_AVX
#define REND_CPU_AVX_CODE 1
#else
#define REND_CPU_TARGET_SSE
#define REND_CPU_TARGET_SSE2
#define REND_CPU_TARGET_AVX
#define REND_CPU_AVX_CODE 0
#endif

#if REND_CPU_X86
#include <xmmintrin.h>
#include <emmintrin.h>
#if REND_CPU_AVX_CODE
#include <immintrin.h>
#endif
#if defined(_MSC_VER)
#include <intrin.h>
#endif
#endif

typedef struct RendCpuPipeline {
    RendCpuVertFn vert;
    RendCpuFragFn frag;
    RendVertexBinding binds[REND_MAX_BINDINGS];
    RendVertexAttributes attrs[REND_CPU_MAX_ATTR];
    RendBuffer vbo[REND_MAX_BINDINGS];
    size_t vbo_off[REND_MAX_BINDINGS];
    uint8_t vbo_set[REND_MAX_BINDINGS];
    uint8_t push[REND_CPU_PUSH_MAX];
    uint32_t bind_count;
    uint32_t attr_count;
    uint32_t push_size;
    uint16_t topology;
    uint16_t cull;
    uint8_t blend;
} RendCpuPipeline;

typedef struct RendCpuContext {
    PeakWindow *window;
    RendTexture color;
    RendTexture *pass;
    RendTexture *textures[REND_MAX_BINDINGS];
    RendCpuPipeline pipelines[REND_CPU_MAX_PIPELINES];
    uint32_t pipeline_count;
    uint32_t tex_ids;
    uint8_t vsync;
} RendCpuContext;

typedef struct RendCpuScan {
    RendCpuContext *ctx;
    RendCpuPipeline *p;
    uint8_t *base;
    float dvdx[REND_CPU_VARYING_FLOATS];
    float dvdy[REND_CPU_VARYING_FLOATS];
    float row_d[REND_CPU_VARYING_FLOATS];
    float dw0x, dw1x, dw2x;
    float dw0y, dw1y, dw2y;
    float row_w0, row_w1, row_w2;
    size_t bpp;
    size_t stride;
    int minx, miny, maxx, maxy;
    uint32_t format;
    uint32_t flat[REND_CPU_VARYING_FLATS];
} RendCpuScan;

static uint32_t rend_cpu_isa;

static uint8_t rend_cpu_u8(float x);
static int rend_cpu_ifloor(float x);
static size_t rend_cpu_tex_bytes(const RendTexture *tex);
static uint8_t *rend_cpu_tex_px(RendTexture *tex);
static void rend_cpu_detect(void);
static void rend_cpu_fill32(uint32_t *q, size_t n, uint32_t packed);
static void rend_cpu_store(uint8_t *p, uint32_t format, float r, float g, float b, float a);
static void rend_cpu_load(const uint8_t *p, uint32_t format, float *r, float *g, float *b, float *a);
static void rend_cpu_clear(RendTexture *tex, float r, float g, float b, float a);
static void rend_cpu_shade(RendCpuPipeline *p, RendCpuFragArgs *fa, uint8_t *dst, uint32_t format);
static void rend_cpu_color_from_window(RendCpuContext *ctx);
static RendContextHandle rend_cpu_ctx_alloc(void);
static float rend_cpu_sample(uint32_t binding, float u, float v, void *user);
static void rend_cpu_fetch(RendCpuPipeline *p, uint32_t vid, uint32_t iid, uint8_t *attr);
static int rend_cpu_instance_rate(RendCpuPipeline *p);
static void rend_cpu_clip_to_screen(const RendCpuVarying *v, uint32_t w, uint32_t h, float *sx, float *sy);
static void rend_cpu_quad_scan(RendCpuScan *s);
static void rend_cpu_tri_scan(RendCpuScan *s);
static void rend_cpu_tri(RendCpuContext *ctx, RendCpuPipeline *p, RendCpuVarying *v0, RendCpuVarying *v1, RendCpuVarying *v2);
static void rend_cpu_quad(RendCpuContext *ctx, RendCpuPipeline *p, RendCpuVarying *vs, const float *sx, const float *sy);
static void rend_cpu_draw_prim(RendCpuContext *ctx, RendCpuPipeline *p, RendCpuVarying *vs, uint32_t i0, uint32_t i1, uint32_t i2);
#if REND_CPU_X86
REND_CPU_TARGET_SSE static void rend_cpu_fill32_sse(uint32_t *q, size_t n, uint32_t packed);
REND_CPU_TARGET_SSE2 static void rend_cpu_fill32_sse2(uint32_t *q, size_t n, uint32_t packed);
REND_CPU_TARGET_SSE static void rend_cpu_quad_scan_sse(RendCpuScan *s);
REND_CPU_TARGET_SSE static void rend_cpu_tri_scan_sse(RendCpuScan *s);
#if REND_CPU_AVX_CODE
REND_CPU_TARGET_AVX static void rend_cpu_fill32_avx(uint32_t *q, size_t n, uint32_t packed);
REND_CPU_TARGET_AVX static void rend_cpu_quad_scan_avx(RendCpuScan *s);
REND_CPU_TARGET_AVX static void rend_cpu_tri_scan_avx(RendCpuScan *s);
#endif
#endif

RendContextHandle rend_cpu_renderer_create(PeakWindow *window, RendBindingInfo *bind_info, bool vsync);
RendContextHandle rend_cpu_renderer_create_offscreen(uint32_t width, uint32_t height, RendFormat format, RendBindingInfo *bind_info);
void rend_cpu_renderer_destroy(RendContextHandle handle);
bool rend_cpu_renderer_frame_begin(RendContextHandle handle);
void rend_cpu_renderer_frame_end(RendContextHandle handle, float *delta);
RendTexture *rend_cpu_color_target(RendContextHandle handle);
void rend_cpu_descriptor_write_buffer(RendContextHandle handle, RendBuffer buf, uint32_t binding, uint32_t slot, uint32_t offset, uint32_t size, bool is_ubo);
void rend_cpu_descriptor_write_texture(RendContextHandle handle, RendTexture *texture, uint32_t binding, uint32_t slot);
RendBuffer rend_cpu_buffer_create_lifetime(RendContextHandle handle, size_t size, RendBufferType type, bool gpu, int lifetime);
void rend_cpu_buffer_destroy(RendBuffer *buffer);
void rend_cpu_buffer_copy(RendContextHandle handle, RendBuffer *dest, size_t dest_offset, RendBuffer *src, size_t src_offset, size_t bytes);
RendTexture rend_cpu_texture_create(RendContextHandle handle, uint32_t width, uint32_t height, uint32_t depth, uint32_t mip_levels, uint32_t layers, RendFormat format);
void rend_cpu_texture_destroy(RendContextHandle handle, RendTexture *tex);
void rend_cpu_texture_copy_buffer(RendContextHandle handle, RendTexture *texture, RendBuffer *buffer);
void rend_cpu_texture_copy_to_buffer(RendContextHandle handle, RendTexture *texture, RendBuffer *buffer);
void rend_cpu_texture_blit(RendContextHandle handle, RendTexture *src, RendTexture *dst, uint32_t src_x, uint32_t src_y, uint32_t src_w, uint32_t src_h, uint32_t dst_x, uint32_t dst_y, uint32_t dst_w, uint32_t dst_h);
bool rend_cpu_pipeline_create(RendContextHandle handle, RendPipeline pipeline, Rend__PipelineConfig config, uint8_t type, const uint8_t *shader1, size_t bytes1, const uint8_t *shader2, size_t bytes2, const uint8_t *shader3, size_t bytes3);
void rend_cpu_pipeline_bind(RendPipeline pipeline);
void rend_cpu_pipeline_push_constants(RendPipeline pipeline, void *push_data, size_t size);
void rend_cpu_pipeline_bind_vertex_buffer(RendPipeline pipeline, uint32_t binding, RendBuffer buffer, size_t offset);
void rend_cpu_pipeline_bind_index_buffer(RendPipeline pipeline, RendBuffer buffer, size_t offset, RendIndexType index_type);
void rend_cpu_pipeline_dispatch(RendPipeline pipeline, uint32_t x, uint32_t y, uint32_t z);
void rend_cpu_pipeline_draw(RendPipeline pipeline, size_t count, uint32_t instance_count);
void rend_cpu_pipeline_draw_indexed(RendPipeline pipeline, uint32_t index_count, uint32_t first_index, int32_t vertex_offset, uint32_t instance_count);
void rend_cpu_pipeline_set_blend(RendPipeline pipeline, bool blend);
void rend_cpu_renderer_render_pass_begin(RendContextHandle handle, float r, float g, float b, float a);
void rend_cpu_renderer_render_pass_begin_texture(RendContextHandle handle, RendTexture *texture);
void rend_cpu_renderer_render_pass_end(RendContextHandle handle);
void rend_cpu_renderer_render_pass_end_texture(RendContextHandle handle, RendTexture *texture);

static uint8_t
rend_cpu_u8(float x)
{
    int v;

    v = (int)(x * 255.f + 0.5f);
    if (v < 0)
        v = 0;
    if (v > 255)
        v = 255;
    return (uint8_t)v;
}

static int
rend_cpu_ifloor(float x)
{
    int i;

    i = (int)x;
    return (x < (float)i) ? i - 1 : i;
}

static size_t
rend_cpu_tex_bytes(const RendTexture *tex)
{
    if (!tex || tex->format >= REND_FORMAT_COUNT)
        return 0;
    return (size_t)tex->width * tex->height * rend_format_size[tex->format];
}

static uint8_t *
rend_cpu_tex_px(RendTexture *tex)
{
    if (!tex)
        return NULL;
    return tex->memory.host_mapped_memory;
}

static void
rend_cpu_detect(void)
{
    static int done;

    if (done)
        return;
    done = 1;
    rend_cpu_isa = REND_CPU_ISA_SCALAR;
#if REND_CPU_X86
#if defined(__GNUC__) || defined(__clang__)
    if (__builtin_cpu_supports("sse"))
        rend_cpu_isa = REND_CPU_ISA_SSE;
    if (__builtin_cpu_supports("sse2"))
        rend_cpu_isa = REND_CPU_ISA_SSE2;
#if REND_CPU_AVX_CODE
    if (__builtin_cpu_supports("avx"))
        rend_cpu_isa = REND_CPU_ISA_AVX;
#endif
#elif defined(_MSC_VER)
    {
        int info[4];

        __cpuid(info, 1);
        if (info[3] & (1 << 25))
            rend_cpu_isa = REND_CPU_ISA_SSE;
        if (info[3] & (1 << 26))
            rend_cpu_isa = REND_CPU_ISA_SSE2;
#if REND_CPU_AVX_CODE
        if ((info[2] & (1 << 28)) && (info[2] & (1 << 27))) {
            unsigned long long xcr;

            xcr = _xgetbv(0);
            if ((xcr & 6ull) == 6ull)
                rend_cpu_isa = REND_CPU_ISA_AVX;
        }
#endif
    }
#endif
#endif
    PDEBUG("CPU raster ISA %u", rend_cpu_isa);
}

#if REND_CPU_X86
REND_CPU_TARGET_SSE
static void
rend_cpu_fill32_sse(uint32_t *q, size_t n, uint32_t packed)
{
    float f;
    __m128 v;
    size_t i;
    size_t n4;

    memcpy(&f, &packed, 4);
    v = _mm_set_ps(f, f, f, f);
    n4 = n & ~(size_t)3;
    for (i = 0; i < n4; i += 4)
        _mm_storeu_ps((float *)(void *)(q + i), v);
    for (; i < n; i++)
        q[i] = packed;
}

REND_CPU_TARGET_SSE2
static void
rend_cpu_fill32_sse2(uint32_t *q, size_t n, uint32_t packed)
{
    __m128i v;
    size_t i;
    size_t n4;

    v = _mm_set1_epi32((int)packed);
    n4 = n & ~(size_t)3;
    for (i = 0; i < n4; i += 4)
        _mm_storeu_si128((__m128i *)(void *)(q + i), v);
    for (; i < n; i++)
        q[i] = packed;
}

#if REND_CPU_AVX_CODE
REND_CPU_TARGET_AVX
static void
rend_cpu_fill32_avx(uint32_t *q, size_t n, uint32_t packed)
{
    __m256i v;
    size_t i;
    size_t n8;

    v = _mm256_set1_epi32((int)packed);
    n8 = n & ~(size_t)7;
    for (i = 0; i < n8; i += 8)
        _mm256_storeu_si256((__m256i *)(void *)(q + i), v);
    for (; i < n; i++)
        q[i] = packed;
}
#endif
#endif

static void
rend_cpu_fill32(uint32_t *q, size_t n, uint32_t packed)
{
    size_t i;

#if REND_CPU_X86
#if REND_CPU_AVX_CODE
    if (rend_cpu_isa >= REND_CPU_ISA_AVX) {
        rend_cpu_fill32_avx(q, n, packed);
        return;
    }
#endif
    if (rend_cpu_isa >= REND_CPU_ISA_SSE2) {
        rend_cpu_fill32_sse2(q, n, packed);
        return;
    }
    if (rend_cpu_isa >= REND_CPU_ISA_SSE) {
        rend_cpu_fill32_sse(q, n, packed);
        return;
    }
#endif
    for (i = 0; i < n; i++)
        q[i] = packed;
}

static void
rend_cpu_store(uint8_t *p, uint32_t format, float r, float g, float b, float a)
{
    uint8_t ir, ig, ib, ia;
    uint32_t packed;

    ir = rend_cpu_u8(r);
    ig = rend_cpu_u8(g);
    ib = rend_cpu_u8(b);
    ia = rend_cpu_u8(a);
    switch (format) {
    case REND_FORMAT_R8_UNORM:
        p[0] = ir;
        break;
    case REND_FORMAT_R8G8B8A8_UNORM: /* FALLTHROUGH */
    case REND_FORMAT_R8G8B8A8_SRGB:
        p[0] = ir;
        p[1] = ig;
        p[2] = ib;
        p[3] = ia;
        break;
    default:
#if defined(PEAK_WEB)
        packed = (uint32_t)ir | ((uint32_t)ig << 8) | ((uint32_t)ib << 16) | ((uint32_t)ia << 24);
#else
        packed = ((uint32_t)ia << 24) | ((uint32_t)ir << 16) | ((uint32_t)ig << 8) | (uint32_t)ib;
#endif
        memcpy(p, &packed, 4);
        break;
    }
}

static void
rend_cpu_load(const uint8_t *p, uint32_t format, float *r, float *g, float *b, float *a)
{
    uint32_t packed;

    switch (format) {
    case REND_FORMAT_R8_UNORM:
        *r = *g = *b = (float)p[0] / 255.f;
        *a = 1.f;
        break;
    case REND_FORMAT_R8G8B8A8_UNORM: /* FALLTHROUGH */
    case REND_FORMAT_R8G8B8A8_SRGB:
        *r = (float)p[0] / 255.f;
        *g = (float)p[1] / 255.f;
        *b = (float)p[2] / 255.f;
        *a = (float)p[3] / 255.f;
        break;
    default:
        memcpy(&packed, p, 4);
#if defined(PEAK_WEB)
        *r = (float)(packed & 255u) / 255.f;
        *g = (float)((packed >> 8) & 255u) / 255.f;
        *b = (float)((packed >> 16) & 255u) / 255.f;
        *a = (float)((packed >> 24) & 255u) / 255.f;
#else
        *a = (float)((packed >> 24) & 255u) / 255.f;
        *r = (float)((packed >> 16) & 255u) / 255.f;
        *g = (float)((packed >> 8) & 255u) / 255.f;
        *b = (float)(packed & 255u) / 255.f;
#endif
        break;
    }
}

static void
rend_cpu_clear(RendTexture *tex, float r, float g, float b, float a)
{
    uint8_t *p;
    size_t bpp;
    size_t n;
    size_t i;
    uint8_t pix[16];

    p = rend_cpu_tex_px(tex);
    if (!p || !tex->width || !tex->height || tex->format >= REND_FORMAT_COUNT)
        return;
    bpp = rend_format_size[tex->format];
    if (!bpp || bpp > sizeof pix)
        return;
    n = (size_t)tex->width * tex->height;
    rend_cpu_store(pix, tex->format, r, g, b, a);
    if (bpp == 4) {
        uint32_t packed;

        memcpy(&packed, pix, 4);
        rend_cpu_fill32((uint32_t *)(void *)p, n, packed);
        return;
    }
    if (bpp == 1) {
        memset(p, pix[0], n);
        return;
    }
    for (i = 0; i < n; i++)
        memcpy(p + i * bpp, pix, bpp);
}

static void
rend_cpu_shade(RendCpuPipeline *p, RendCpuFragArgs *fa, uint8_t *dst, uint32_t format)
{
    float rgba[4];
    float dr, dg, db, da;

    rgba[0] = rgba[1] = rgba[2] = 0.f;
    rgba[3] = 1.f;
    p->frag(rgba, fa);
    if (p->blend) {
        rend_cpu_load(dst, format, &dr, &dg, &db, &da);
        rgba[0] = rgba[0] * rgba[3] + dr * (1.f - rgba[3]);
        rgba[1] = rgba[1] * rgba[3] + dg * (1.f - rgba[3]);
        rgba[2] = rgba[2] * rgba[3] + db * (1.f - rgba[3]);
        rgba[3] = rgba[3] + da * (1.f - rgba[3]);
    }
    rend_cpu_store(dst, format, rgba[0], rgba[1], rgba[2], rgba[3]);
}

static void
rend_cpu_color_from_window(RendCpuContext *ctx)
{
    size_t w = 0, h = 0;
    uint32_t *px;

    px = peak_window_backbuffer(ctx->window, &w, &h);
    ctx->color.memory.host_mapped_memory = px;
    ctx->color.handle = px ? (uint64_t)(uintptr_t)px : 0;
    ctx->color.width = (uint32_t)w;
    ctx->color.height = (uint32_t)h;
    ctx->color.format = REND_FORMAT_B8G8R8A8_UNORM;
    ctx->color.borrowed = 1;
    ctx->color.backend = REND_BACKEND_CPU;
}

static RendContextHandle
rend_cpu_ctx_alloc(void)
{
    RendCpuContext *ctx;

    ctx = rmalloc(sizeof *ctx);
    if (!ctx)
        return NULL;
    memset(ctx, 0, sizeof *ctx);
    rend_cpu_detect();
    return ctx;
}

static float
rend_cpu_sample(uint32_t binding, float u, float v, void *user)
{
    RendCpuContext *ctx;
    RendTexture *tex;
    const uint8_t *p;
    int w, h, x0, y0, x1, y1;
    float x, y, fx, fy, c00, c10, c01, c11;

    ctx = user;
    if (!ctx || binding >= REND_MAX_BINDINGS)
        return 0.f;
    tex = ctx->textures[binding];
    if (!tex || !tex->memory.host_mapped_memory || !tex->width || !tex->height)
        return 0.f;
    p = tex->memory.host_mapped_memory;
    w = (int)tex->width;
    h = (int)tex->height;
    x = u * (float)w - 0.5f;
    y = v * (float)h - 0.5f;
    x0 = rend_cpu_ifloor(x);
    y0 = rend_cpu_ifloor(y);
    fx = x - (float)x0;
    fy = y - (float)y0;
    x1 = x0 + 1;
    y1 = y0 + 1;
    if (x0 < 0)
        x0 = 0;
    if (y0 < 0)
        y0 = 0;
    if (x1 < 0)
        x1 = 0;
    if (y1 < 0)
        y1 = 0;
    if (x0 >= w)
        x0 = w - 1;
    if (y0 >= h)
        y0 = h - 1;
    if (x1 >= w)
        x1 = w - 1;
    if (y1 >= h)
        y1 = h - 1;
    if (tex->format == REND_FORMAT_R8_UNORM) {
        c00 = (float)p[(size_t)y0 * (size_t)w + (size_t)x0];
        c10 = (float)p[(size_t)y0 * (size_t)w + (size_t)x1];
        c01 = (float)p[(size_t)y1 * (size_t)w + (size_t)x0];
        c11 = (float)p[(size_t)y1 * (size_t)w + (size_t)x1];
        return ((c00 * (1.f - fx) + c10 * fx) * (1.f - fy) + (c01 * (1.f - fx) + c11 * fx) * fy) * (1.f / 255.f);
    }
    {
        uint32_t bpp;

        if (tex->format >= REND_FORMAT_COUNT)
            return 0.f;
        bpp = (uint32_t)rend_format_size[tex->format];
        if (!bpp)
            return 0.f;
        c00 = (float)p[((size_t)y0 * (size_t)w + (size_t)x0) * bpp];
        c10 = (float)p[((size_t)y0 * (size_t)w + (size_t)x1) * bpp];
        c01 = (float)p[((size_t)y1 * (size_t)w + (size_t)x0) * bpp];
        c11 = (float)p[((size_t)y1 * (size_t)w + (size_t)x1) * bpp];
        return ((c00 * (1.f - fx) + c10 * fx) * (1.f - fy) + (c01 * (1.f - fx) + c11 * fx) * fy) * (1.f / 255.f);
    }
}

static void
rend_cpu_fetch(RendCpuPipeline *p, uint32_t vid, uint32_t iid, uint8_t *attr)
{
    uint32_t i;
    uint32_t b;

    memset(attr, 0, (size_t)REND_CPU_MAX_ATTR * REND_CPU_ATTR_STRIDE);
    for (i = 0; i < p->attr_count; i++) {
        RendVertexAttributes *a;
        const uint8_t *base;
        size_t nbytes;
        uint32_t loc;
        uint64_t stride;
        uint8_t rate;

        a = &p->attrs[i];
        if (a->binding >= REND_MAX_BINDINGS || !p->vbo_set[a->binding])
            continue;
        if (!p->vbo[a->binding].mapped_memory)
            continue;
        stride = 0;
        rate = REND_INPUT_RATE_VERTEX;
        for (b = 0; b < p->bind_count; b++) {
            if (p->binds[b].binding == a->binding) {
                stride = p->binds[b].stride;
                rate = p->binds[b].input_rate;
                break;
            }
        }
        if (!stride)
            continue;
        if (rate == REND_INPUT_RATE_INSTANCE)
            base = (const uint8_t *)p->vbo[a->binding].mapped_memory + p->vbo_off[a->binding] + (size_t)iid * stride;
        else
            base = (const uint8_t *)p->vbo[a->binding].mapped_memory + p->vbo_off[a->binding] + (size_t)vid * stride;
        loc = (uint32_t)a->location;
        if (loc >= REND_CPU_MAX_ATTR)
            continue;
        if (a->format >= REND_FORMAT_COUNT)
            continue;
        nbytes = rend_format_size[a->format];
        if (nbytes > REND_CPU_ATTR_STRIDE)
            nbytes = REND_CPU_ATTR_STRIDE;
        memcpy(attr + loc * REND_CPU_ATTR_STRIDE, base + a->offset, nbytes);
    }
}

static int
rend_cpu_instance_rate(RendCpuPipeline *p)
{
    uint32_t i;
    uint32_t b;

    for (i = 0; i < p->attr_count; i++) {
        uint8_t rate;

        rate = REND_INPUT_RATE_VERTEX;
        for (b = 0; b < p->bind_count; b++) {
            if (p->binds[b].binding == p->attrs[i].binding) {
                rate = p->binds[b].input_rate;
                break;
            }
        }
        if (rate != REND_INPUT_RATE_INSTANCE)
            return 0;
    }
    return 1;
}

static void
rend_cpu_clip_to_screen(const RendCpuVarying *v, uint32_t w, uint32_t h, float *sx, float *sy)
{
    float iw;

    iw = v->position[3];
    iw = (iw == 0.f) ? 1.f : 1.f / iw;
    *sx = (v->position[0] * iw * 0.5f + 0.5f) * (float)w;
    *sy = (v->position[1] * iw * 0.5f + 0.5f) * (float)h;
}

#if REND_CPU_X86
#define REND_CPU_ADD12_SSE(dst, src) do { \
    __m128 _a0 = _mm_loadu_ps(dst); \
    __m128 _a1 = _mm_loadu_ps((dst) + 4); \
    __m128 _a2 = _mm_loadu_ps((dst) + 8); \
    _a0 = _mm_add_ps(_a0, _mm_loadu_ps(src)); \
    _a1 = _mm_add_ps(_a1, _mm_loadu_ps((src) + 4)); \
    _a2 = _mm_add_ps(_a2, _mm_loadu_ps((src) + 8)); \
    _mm_storeu_ps(dst, _a0); \
    _mm_storeu_ps((dst) + 4, _a1); \
    _mm_storeu_ps((dst) + 8, _a2); \
} while (0)

#define REND_CPU_ADD12_MUL_SSE(dst, src, f) do { \
    __m128 _s = _mm_set1_ps(f); \
    __m128 _a0 = _mm_loadu_ps(dst); \
    __m128 _a1 = _mm_loadu_ps((dst) + 4); \
    __m128 _a2 = _mm_loadu_ps((dst) + 8); \
    _a0 = _mm_add_ps(_a0, _mm_mul_ps(_mm_loadu_ps(src), _s)); \
    _a1 = _mm_add_ps(_a1, _mm_mul_ps(_mm_loadu_ps((src) + 4), _s)); \
    _a2 = _mm_add_ps(_a2, _mm_mul_ps(_mm_loadu_ps((src) + 8), _s)); \
    _mm_storeu_ps(dst, _a0); \
    _mm_storeu_ps((dst) + 4, _a1); \
    _mm_storeu_ps((dst) + 8, _a2); \
} while (0)

#if REND_CPU_AVX_CODE
#define REND_CPU_ADD12_AVX(dst, src) do { \
    __m256 _a = _mm256_loadu_ps(dst); \
    __m128 _c = _mm_loadu_ps((dst) + 8); \
    _a = _mm256_add_ps(_a, _mm256_loadu_ps(src)); \
    _c = _mm_add_ps(_c, _mm_loadu_ps((src) + 8)); \
    _mm256_storeu_ps(dst, _a); \
    _mm_storeu_ps((dst) + 8, _c); \
} while (0)

#define REND_CPU_ADD12_MUL_AVX(dst, src, f) do { \
    __m256 _s = _mm256_set1_ps(f); \
    __m256 _a = _mm256_loadu_ps(dst); \
    __m128 _c = _mm_loadu_ps((dst) + 8); \
    _a = _mm256_add_ps(_a, _mm256_mul_ps(_mm256_loadu_ps(src), _s)); \
    _c = _mm_add_ps(_c, _mm_mul_ps(_mm_loadu_ps((src) + 8), _mm_set1_ps(f))); \
    _mm256_storeu_ps(dst, _a); \
    _mm_storeu_ps((dst) + 8, _c); \
} while (0)
#endif
#endif

static void
rend_cpu_quad_scan(RendCpuScan *s)
{
    RendCpuVarying var;
    RendCpuFragArgs fa;
    int x, y, i;

    memset(&var, 0, sizeof var);
    for (i = 0; i < REND_CPU_VARYING_FLATS; i++)
        var.flat[i] = s->flat[i];
    fa.v = &var;
    fa.push = s->p->push;
    fa.sample = rend_cpu_sample;
    fa.sample_ctx = s->ctx;
    for (y = s->miny; y < s->maxy; y++) {
        uint8_t *row;

        memcpy(var.data, s->row_d, sizeof var.data);
        row = s->base + (size_t)y * s->stride;
        for (x = s->minx; x < s->maxx; x++) {
            rend_cpu_shade(s->p, &fa, row + (size_t)x * s->bpp, s->format);
            for (i = 0; i < REND_CPU_VARYING_FLOATS; i++)
                var.data[i] += s->dvdx[i];
        }
        for (i = 0; i < REND_CPU_VARYING_FLOATS; i++)
            s->row_d[i] += s->dvdy[i];
    }
}

static void
rend_cpu_tri_scan(RendCpuScan *s)
{
    RendCpuVarying var;
    RendCpuFragArgs fa;
    int x, y, i;

    memset(&var, 0, sizeof var);
    for (i = 0; i < REND_CPU_VARYING_FLATS; i++)
        var.flat[i] = s->flat[i];
    fa.v = &var;
    fa.push = s->p->push;
    fa.sample = rend_cpu_sample;
    fa.sample_ctx = s->ctx;
    for (y = s->miny; y < s->maxy; y++) {
        float ew0, ew1, ew2;
        uint8_t *row;

        ew0 = s->row_w0;
        ew1 = s->row_w1;
        ew2 = s->row_w2;
        memcpy(var.data, s->row_d, sizeof var.data);
        row = s->base + (size_t)y * s->stride;
        for (x = s->minx; x < s->maxx; x++) {
            if (ew0 >= 0.f && ew1 >= 0.f && ew2 >= 0.f)
                rend_cpu_shade(s->p, &fa, row + (size_t)x * s->bpp, s->format);
            ew0 += s->dw0x;
            ew1 += s->dw1x;
            ew2 += s->dw2x;
            for (i = 0; i < REND_CPU_VARYING_FLOATS; i++)
                var.data[i] += s->dvdx[i];
        }
        s->row_w0 += s->dw0y;
        s->row_w1 += s->dw1y;
        s->row_w2 += s->dw2y;
        for (i = 0; i < REND_CPU_VARYING_FLOATS; i++)
            s->row_d[i] += s->dvdy[i];
    }
}

#if REND_CPU_X86
REND_CPU_TARGET_SSE
static void
rend_cpu_quad_scan_sse(RendCpuScan *s)
{
    RendCpuVarying var;
    RendCpuFragArgs fa;
    int x, y, i;

    memset(&var, 0, sizeof var);
    for (i = 0; i < REND_CPU_VARYING_FLATS; i++)
        var.flat[i] = s->flat[i];
    fa.v = &var;
    fa.push = s->p->push;
    fa.sample = rend_cpu_sample;
    fa.sample_ctx = s->ctx;
    for (y = s->miny; y < s->maxy; y++) {
        uint8_t *row;

        memcpy(var.data, s->row_d, sizeof var.data);
        row = s->base + (size_t)y * s->stride;
        for (x = s->minx; x < s->maxx; x++) {
            rend_cpu_shade(s->p, &fa, row + (size_t)x * s->bpp, s->format);
            REND_CPU_ADD12_SSE(var.data, s->dvdx);
        }
        REND_CPU_ADD12_SSE(s->row_d, s->dvdy);
    }
}

REND_CPU_TARGET_SSE
static void
rend_cpu_tri_scan_sse(RendCpuScan *s)
{
    RendCpuVarying var;
    RendCpuFragArgs fa;
    __m128 z, off, dw0, dw1, dw2, step0, step1, step2, w0, w1, w2, m;
    int x, y, i, k, bits;
    float ew0, ew1, ew2;

    memset(&var, 0, sizeof var);
    for (i = 0; i < REND_CPU_VARYING_FLATS; i++)
        var.flat[i] = s->flat[i];
    fa.v = &var;
    fa.push = s->p->push;
    fa.sample = rend_cpu_sample;
    fa.sample_ctx = s->ctx;
    z = _mm_setzero_ps();
    off = _mm_setr_ps(0.f, 1.f, 2.f, 3.f);
    dw0 = _mm_set1_ps(s->dw0x);
    dw1 = _mm_set1_ps(s->dw1x);
    dw2 = _mm_set1_ps(s->dw2x);
    step0 = _mm_mul_ps(dw0, _mm_set1_ps(4.f));
    step1 = _mm_mul_ps(dw1, _mm_set1_ps(4.f));
    step2 = _mm_mul_ps(dw2, _mm_set1_ps(4.f));
    for (y = s->miny; y < s->maxy; y++) {
        uint8_t *row;

        memcpy(var.data, s->row_d, sizeof var.data);
        row = s->base + (size_t)y * s->stride;
        ew0 = s->row_w0;
        ew1 = s->row_w1;
        ew2 = s->row_w2;
        w0 = _mm_add_ps(_mm_set1_ps(ew0), _mm_mul_ps(off, dw0));
        w1 = _mm_add_ps(_mm_set1_ps(ew1), _mm_mul_ps(off, dw1));
        w2 = _mm_add_ps(_mm_set1_ps(ew2), _mm_mul_ps(off, dw2));
        x = s->minx;
        for (; x + 4 <= s->maxx; x += 4) {
            m = _mm_and_ps(_mm_and_ps(_mm_cmpge_ps(w0, z), _mm_cmpge_ps(w1, z)), _mm_cmpge_ps(w2, z));
            bits = _mm_movemask_ps(m);
            if (bits) {
                for (k = 0; k < 4; k++) {
                    if (bits & (1 << k))
                        rend_cpu_shade(s->p, &fa, row + (size_t)(x + k) * s->bpp, s->format);
                    REND_CPU_ADD12_SSE(var.data, s->dvdx);
                }
            } else {
                REND_CPU_ADD12_MUL_SSE(var.data, s->dvdx, 4.f);
            }
            w0 = _mm_add_ps(w0, step0);
            w1 = _mm_add_ps(w1, step1);
            w2 = _mm_add_ps(w2, step2);
        }
        ew0 = _mm_cvtss_f32(w0);
        ew1 = _mm_cvtss_f32(w1);
        ew2 = _mm_cvtss_f32(w2);
        for (; x < s->maxx; x++) {
            if (ew0 >= 0.f && ew1 >= 0.f && ew2 >= 0.f)
                rend_cpu_shade(s->p, &fa, row + (size_t)x * s->bpp, s->format);
            ew0 += s->dw0x;
            ew1 += s->dw1x;
            ew2 += s->dw2x;
            REND_CPU_ADD12_SSE(var.data, s->dvdx);
        }
        REND_CPU_ADD12_SSE(s->row_d, s->dvdy);
        s->row_w0 += s->dw0y;
        s->row_w1 += s->dw1y;
        s->row_w2 += s->dw2y;
    }
}

#if REND_CPU_AVX_CODE
REND_CPU_TARGET_AVX
static void
rend_cpu_quad_scan_avx(RendCpuScan *s)
{
    RendCpuVarying var;
    RendCpuFragArgs fa;
    int x, y, i;

    memset(&var, 0, sizeof var);
    for (i = 0; i < REND_CPU_VARYING_FLATS; i++)
        var.flat[i] = s->flat[i];
    fa.v = &var;
    fa.push = s->p->push;
    fa.sample = rend_cpu_sample;
    fa.sample_ctx = s->ctx;
    for (y = s->miny; y < s->maxy; y++) {
        uint8_t *row;

        memcpy(var.data, s->row_d, sizeof var.data);
        row = s->base + (size_t)y * s->stride;
        for (x = s->minx; x < s->maxx; x++) {
            rend_cpu_shade(s->p, &fa, row + (size_t)x * s->bpp, s->format);
            REND_CPU_ADD12_AVX(var.data, s->dvdx);
        }
        REND_CPU_ADD12_AVX(s->row_d, s->dvdy);
    }
}

REND_CPU_TARGET_AVX
static void
rend_cpu_tri_scan_avx(RendCpuScan *s)
{
    RendCpuVarying var;
    RendCpuFragArgs fa;
    __m256 z, off, dw0, dw1, dw2, step0, step1, step2, w0, w1, w2, m;
    int x, y, i, k, bits;
    float ew0, ew1, ew2;

    memset(&var, 0, sizeof var);
    for (i = 0; i < REND_CPU_VARYING_FLATS; i++)
        var.flat[i] = s->flat[i];
    fa.v = &var;
    fa.push = s->p->push;
    fa.sample = rend_cpu_sample;
    fa.sample_ctx = s->ctx;
    z = _mm256_setzero_ps();
    off = _mm256_setr_ps(0.f, 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f);
    dw0 = _mm256_set1_ps(s->dw0x);
    dw1 = _mm256_set1_ps(s->dw1x);
    dw2 = _mm256_set1_ps(s->dw2x);
    step0 = _mm256_mul_ps(dw0, _mm256_set1_ps(8.f));
    step1 = _mm256_mul_ps(dw1, _mm256_set1_ps(8.f));
    step2 = _mm256_mul_ps(dw2, _mm256_set1_ps(8.f));
    for (y = s->miny; y < s->maxy; y++) {
        uint8_t *row;

        memcpy(var.data, s->row_d, sizeof var.data);
        row = s->base + (size_t)y * s->stride;
        ew0 = s->row_w0;
        ew1 = s->row_w1;
        ew2 = s->row_w2;
        w0 = _mm256_add_ps(_mm256_set1_ps(ew0), _mm256_mul_ps(off, dw0));
        w1 = _mm256_add_ps(_mm256_set1_ps(ew1), _mm256_mul_ps(off, dw1));
        w2 = _mm256_add_ps(_mm256_set1_ps(ew2), _mm256_mul_ps(off, dw2));
        x = s->minx;
        for (; x + 8 <= s->maxx; x += 8) {
            m = _mm256_and_ps(_mm256_and_ps(_mm256_cmp_ps(w0, z, _CMP_GE_OQ), _mm256_cmp_ps(w1, z, _CMP_GE_OQ)), _mm256_cmp_ps(w2, z, _CMP_GE_OQ));
            bits = _mm256_movemask_ps(m);
            if (bits) {
                for (k = 0; k < 8; k++) {
                    if (bits & (1 << k))
                        rend_cpu_shade(s->p, &fa, row + (size_t)(x + k) * s->bpp, s->format);
                    REND_CPU_ADD12_AVX(var.data, s->dvdx);
                }
            } else {
                REND_CPU_ADD12_MUL_AVX(var.data, s->dvdx, 8.f);
            }
            w0 = _mm256_add_ps(w0, step0);
            w1 = _mm256_add_ps(w1, step1);
            w2 = _mm256_add_ps(w2, step2);
        }
        ew0 = _mm_cvtss_f32(_mm256_castps256_ps128(w0));
        ew1 = _mm_cvtss_f32(_mm256_castps256_ps128(w1));
        ew2 = _mm_cvtss_f32(_mm256_castps256_ps128(w2));
        for (; x < s->maxx; x++) {
            if (ew0 >= 0.f && ew1 >= 0.f && ew2 >= 0.f)
                rend_cpu_shade(s->p, &fa, row + (size_t)x * s->bpp, s->format);
            ew0 += s->dw0x;
            ew1 += s->dw1x;
            ew2 += s->dw2x;
            REND_CPU_ADD12_AVX(var.data, s->dvdx);
        }
        REND_CPU_ADD12_AVX(s->row_d, s->dvdy);
        s->row_w0 += s->dw0y;
        s->row_w1 += s->dw1y;
        s->row_w2 += s->dw2y;
    }
}
#endif
#endif

static void
rend_cpu_tri(RendCpuContext *ctx, RendCpuPipeline *p, RendCpuVarying *v0, RendCpuVarying *v1, RendCpuVarying *v2)
{
    RendCpuScan s;
    RendTexture *tex;
    uint8_t *base;
    float x0, y0, x1, y1, x2, y2, area, inv_a;
    int i;
    uint32_t w, h;

    tex = ctx->pass;
    base = rend_cpu_tex_px(tex);
    if (!tex || !base)
        return;
    w = tex->width;
    h = tex->height;
    if (!w || !h || tex->format >= REND_FORMAT_COUNT)
        return;
    memset(&s, 0, sizeof s);
    s.format = tex->format;
    s.bpp = rend_format_size[s.format];
    if (!s.bpp)
        return;
    s.ctx = ctx;
    s.p = p;
    s.base = base;
    s.stride = (size_t)w * s.bpp;
    rend_cpu_clip_to_screen(v0, w, h, &x0, &y0);
    rend_cpu_clip_to_screen(v1, w, h, &x1, &y1);
    rend_cpu_clip_to_screen(v2, w, h, &x2, &y2);
    area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
    if (p->cull == REND_CULL_MODE_FRONT_AND_BACK)
        return;
    if (p->cull == REND_CULL_MODE_BACK && area < 0.f)
        return;
    if (p->cull == REND_CULL_MODE_FRONT && area > 0.f)
        return;
    if (area < 0.f) {
        RendCpuVarying *tmp;
        float tx, ty;

        tmp = v1;
        v1 = v2;
        v2 = tmp;
        tx = x1;
        x1 = x2;
        x2 = tx;
        ty = y1;
        y1 = y2;
        y2 = ty;
        area = -area;
    }
    if (area < 1e-8f)
        return;
    inv_a = 1.f / area;
    s.dw0x = (y1 - y2) * inv_a;
    s.dw0y = (x2 - x1) * inv_a;
    s.dw1x = (y2 - y0) * inv_a;
    s.dw1y = (x0 - x2) * inv_a;
    s.dw2x = (y0 - y1) * inv_a;
    s.dw2y = (x1 - x0) * inv_a;
    for (i = 0; i < REND_CPU_VARYING_FLOATS; i++) {
        s.dvdx[i] = v0->data[i] * s.dw0x + v1->data[i] * s.dw1x + v2->data[i] * s.dw2x;
        s.dvdy[i] = v0->data[i] * s.dw0y + v1->data[i] * s.dw1y + v2->data[i] * s.dw2y;
    }
    s.minx = (int)floorf(fminf(x0, fminf(x1, x2)));
    s.miny = (int)floorf(fminf(y0, fminf(y1, y2)));
    s.maxx = (int)ceilf(fmaxf(x0, fmaxf(x1, x2)));
    s.maxy = (int)ceilf(fmaxf(y0, fmaxf(y1, y2)));
    if (s.minx < 0)
        s.minx = 0;
    if (s.miny < 0)
        s.miny = 0;
    if (s.maxx > (int)w)
        s.maxx = (int)w;
    if (s.maxy > (int)h)
        s.maxy = (int)h;
    if (s.minx >= s.maxx || s.miny >= s.maxy)
        return;
    {
        float px = (float)s.minx + 0.5f;
        float py = (float)s.miny + 0.5f;

        s.row_w0 = ((x1 - px) * (y2 - py) - (x2 - px) * (y1 - py)) * inv_a;
        s.row_w1 = ((x2 - px) * (y0 - py) - (x0 - px) * (y2 - py)) * inv_a;
        s.row_w2 = ((x0 - px) * (y1 - py) - (x1 - px) * (y0 - py)) * inv_a;
        for (i = 0; i < REND_CPU_VARYING_FLOATS; i++)
            s.row_d[i] = v0->data[i] * s.row_w0 + v1->data[i] * s.row_w1 + v2->data[i] * s.row_w2;
    }
    for (i = 0; i < REND_CPU_VARYING_FLATS; i++)
        s.flat[i] = v0->flat[i];
#if REND_CPU_X86
#if REND_CPU_AVX_CODE
    if (rend_cpu_isa >= REND_CPU_ISA_AVX) {
        rend_cpu_tri_scan_avx(&s);
        return;
    }
#endif
    if (rend_cpu_isa >= REND_CPU_ISA_SSE) {
        rend_cpu_tri_scan_sse(&s);
        return;
    }
#endif
    rend_cpu_tri_scan(&s);
}

static void
rend_cpu_quad(RendCpuContext *ctx, RendCpuPipeline *p, RendCpuVarying *vs, const float *sx, const float *sy)
{
    RendCpuScan s;
    RendTexture *tex;
    uint8_t *base;
    float x0, y0, x1, y1, inv_w, inv_h;
    int i;
    uint32_t w, h;

    tex = ctx->pass;
    base = rend_cpu_tex_px(tex);
    if (!tex || !base)
        return;
    w = tex->width;
    h = tex->height;
    if (!w || !h || tex->format >= REND_FORMAT_COUNT)
        return;
    memset(&s, 0, sizeof s);
    s.format = tex->format;
    s.bpp = rend_format_size[s.format];
    if (!s.bpp)
        return;
    s.ctx = ctx;
    s.p = p;
    s.base = base;
    s.stride = (size_t)w * s.bpp;
    x0 = sx[0];
    y0 = sy[0];
    x1 = sx[1];
    y1 = sy[2];
    inv_w = x1 - x0;
    inv_h = y1 - y0;
    if (inv_w < 0.5f && inv_w > -0.5f)
        return;
    if (inv_h < 0.5f && inv_h > -0.5f)
        return;
    inv_w = 1.f / inv_w;
    inv_h = 1.f / inv_h;
    for (i = 0; i < REND_CPU_VARYING_FLOATS; i++) {
        s.dvdx[i] = (vs[1].data[i] - vs[0].data[i]) * inv_w;
        s.dvdy[i] = (vs[2].data[i] - vs[0].data[i]) * inv_h;
    }
    s.minx = (int)floorf(fminf(x0, x1));
    s.miny = (int)floorf(fminf(y0, y1));
    s.maxx = (int)ceilf(fmaxf(x0, x1));
    s.maxy = (int)ceilf(fmaxf(y0, y1));
    if (s.minx < 0)
        s.minx = 0;
    if (s.miny < 0)
        s.miny = 0;
    if (s.maxx > (int)w)
        s.maxx = (int)w;
    if (s.maxy > (int)h)
        s.maxy = (int)h;
    if (s.minx >= s.maxx || s.miny >= s.maxy)
        return;
    {
        float fx0 = (float)s.minx + 0.5f - x0;
        float fy0 = (float)s.miny + 0.5f - y0;

        for (i = 0; i < REND_CPU_VARYING_FLOATS; i++)
            s.row_d[i] = vs[0].data[i] + fx0 * s.dvdx[i] + fy0 * s.dvdy[i];
    }
    for (i = 0; i < REND_CPU_VARYING_FLATS; i++)
        s.flat[i] = vs[0].flat[i];
#if REND_CPU_X86
#if REND_CPU_AVX_CODE
    if (rend_cpu_isa >= REND_CPU_ISA_AVX) {
        rend_cpu_quad_scan_avx(&s);
        return;
    }
#endif
    if (rend_cpu_isa >= REND_CPU_ISA_SSE) {
        rend_cpu_quad_scan_sse(&s);
        return;
    }
#endif
    rend_cpu_quad_scan(&s);
}

static void
rend_cpu_draw_prim(RendCpuContext *ctx, RendCpuPipeline *p, RendCpuVarying *vs, uint32_t i0, uint32_t i1, uint32_t i2)
{
    rend_cpu_tri(ctx, p, &vs[i0], &vs[i1], &vs[i2]);
}

RendContextHandle
rend_cpu_renderer_create(PeakWindow *window, RendBindingInfo *bind_info, bool vsync)
{
    RendCpuContext *ctx;

    (void)bind_info;
    if (!window)
        return NULL;
    ctx = rend_cpu_ctx_alloc();
    if (!ctx)
        return NULL;
    ctx->window = window;
    ctx->vsync = vsync ? 1 : 0;
    rend_cpu_color_from_window(ctx);
    if (!ctx->color.memory.host_mapped_memory) {
        rfree(ctx);
        return NULL;
    }
    return ctx;
}

RendContextHandle
rend_cpu_renderer_create_offscreen(uint32_t width, uint32_t height, RendFormat format, RendBindingInfo *bind_info)
{
    RendCpuContext *ctx;
    size_t bytes;
    void *px;

    (void)bind_info;
    if (!width || !height || format >= REND_FORMAT_COUNT || !rend_format_size[format])
        return NULL;
    ctx = rend_cpu_ctx_alloc();
    if (!ctx)
        return NULL;
    bytes = (size_t)width * height * rend_format_size[format];
    px = rmalloc(bytes);
    if (!px) {
        rfree(ctx);
        return NULL;
    }
    memset(px, 0, bytes);
    ctx->color.memory.host_mapped_memory = px;
    ctx->color.handle = (uint64_t)(uintptr_t)px;
    ctx->color.width = width;
    ctx->color.height = height;
    ctx->color.format = format;
    ctx->color.borrowed = 0;
    ctx->color.backend = REND_BACKEND_CPU;
    return ctx;
}

void
rend_cpu_renderer_destroy(RendContextHandle handle)
{
    RendCpuContext *ctx;

    ctx = handle;
    if (!ctx)
        return;
    if (!ctx->color.borrowed && ctx->color.memory.host_mapped_memory)
        rfree(ctx->color.memory.host_mapped_memory);
    rfree(ctx);
}

bool
rend_cpu_renderer_frame_begin(RendContextHandle handle)
{
    RendCpuContext *ctx;

    ctx = handle;
    RASSERT(ctx, "Invalid CPU renderer.");
    if (!ctx)
        return false;
    if (ctx->window) {
        rend_cpu_color_from_window(ctx);
        if (!ctx->color.memory.host_mapped_memory)
            return false;
    }
    return ctx->color.memory.host_mapped_memory != NULL;
}

void
rend_cpu_renderer_frame_end(RendContextHandle handle, float *delta)
{
    RendCpuContext *ctx;

    ctx = handle;
    RASSERT(ctx, "Invalid CPU renderer.");
    if (delta)
        *delta = 0.f;
    if (!ctx)
        return;
    ctx->pass = NULL;
    if (ctx->window)
        peak_window_present(ctx->window);
}

RendTexture *
rend_cpu_color_target(RendContextHandle handle)
{
    RendCpuContext *ctx;

    ctx = handle;
    return ctx ? &ctx->color : NULL;
}

void
rend_cpu_descriptor_write_buffer(RendContextHandle handle, RendBuffer buf, uint32_t binding, uint32_t slot, uint32_t offset, uint32_t size, bool is_ubo)
{
    (void)handle;
    (void)buf;
    (void)binding;
    (void)slot;
    (void)offset;
    (void)size;
    (void)is_ubo;
}

void
rend_cpu_descriptor_write_texture(RendContextHandle handle, RendTexture *texture, uint32_t binding, uint32_t slot)
{
    RendCpuContext *ctx;

    ctx = handle;
    (void)slot;
    if (!ctx || binding >= REND_MAX_BINDINGS)
        return;
    ctx->textures[binding] = texture;
}

RendBuffer
rend_cpu_buffer_create_lifetime(RendContextHandle handle, size_t size, RendBufferType type, bool gpu, int lifetime)
{
    RendBuffer buf;
    void *p;

    (void)handle;
    (void)type;
    (void)gpu;
    (void)lifetime;
    memset(&buf, 0, sizeof buf);
    if (!size)
        return buf;
    p = rmalloc(size);
    if (!p)
        return buf;
    memset(p, 0, size);
    buf.memory.host_mapped_memory = p;
    buf.mapped_memory = p;
    buf.handle = (uint64_t)(uintptr_t)p;
    buf.gpu_address = (uint64_t)(uintptr_t)p;
    buf.size = (uint32_t)size;
    buf.backend = REND_BACKEND_CPU;
    return buf;
}

void
rend_cpu_buffer_destroy(RendBuffer *buffer)
{
    if (!buffer)
        return;
    if (buffer->mapped_memory)
        rfree(buffer->mapped_memory);
    memset(buffer, 0, sizeof *buffer);
}

void
rend_cpu_buffer_copy(RendContextHandle handle, RendBuffer *dest, size_t dest_offset, RendBuffer *src, size_t src_offset, size_t bytes)
{
    uint8_t *d;
    uint8_t *s;

    (void)handle;
    if (!dest || !src || !bytes)
        return;
    if (!dest->mapped_memory || !src->mapped_memory)
        return;
    if (dest_offset > dest->size || src_offset > src->size)
        return;
    if (bytes > dest->size - dest_offset || bytes > src->size - src_offset)
        return;
    d = dest->mapped_memory;
    s = src->mapped_memory;
    memcpy(d + dest_offset, s + src_offset, bytes);
}

RendTexture
rend_cpu_texture_create(RendContextHandle handle, uint32_t width, uint32_t height, uint32_t depth, uint32_t mip_levels, uint32_t layers, RendFormat format)
{
    RendCpuContext *ctx;
    RendTexture tex;
    size_t bytes;
    void *px;

    ctx = handle;
    memset(&tex, 0, sizeof tex);
    (void)depth;
    (void)mip_levels;
    (void)layers;
    if (!width || !height || format >= REND_FORMAT_COUNT || !rend_format_size[format])
        return tex;
    bytes = (size_t)width * height * rend_format_size[format];
    px = rmalloc(bytes);
    if (!px)
        return tex;
    memset(px, 0, bytes);
    tex.memory.host_mapped_memory = px;
    tex.handle = (uint64_t)(uintptr_t)px;
    tex.width = width;
    tex.height = height;
    tex.format = format;
    tex.depth = depth ? depth : 1;
    tex.mip_levels = mip_levels ? mip_levels : 1;
    tex.layers = layers ? layers : 1;
    tex.backend = REND_BACKEND_CPU;
    if (ctx) {
        ctx->tex_ids++;
        tex.id = ctx->tex_ids;
    }
    return tex;
}

void
rend_cpu_texture_destroy(RendContextHandle handle, RendTexture *tex)
{
    (void)handle;
    if (!tex)
        return;
    if (!tex->borrowed && tex->memory.host_mapped_memory)
        rfree(tex->memory.host_mapped_memory);
    memset(tex, 0, sizeof *tex);
}

void
rend_cpu_texture_copy_buffer(RendContextHandle handle, RendTexture *texture, RendBuffer *buffer)
{
    size_t n;
    size_t cap;

    (void)handle;
    if (!texture || !buffer || !texture->memory.host_mapped_memory || !buffer->mapped_memory)
        return;
    n = rend_cpu_tex_bytes(texture);
    cap = buffer->size;
    if (cap < n)
        n = cap;
    memcpy(texture->memory.host_mapped_memory, buffer->mapped_memory, n);
}

void
rend_cpu_texture_copy_to_buffer(RendContextHandle handle, RendTexture *texture, RendBuffer *buffer)
{
    size_t n;
    size_t cap;

    (void)handle;
    if (!texture || !buffer || !texture->memory.host_mapped_memory || !buffer->mapped_memory)
        return;
    n = rend_cpu_tex_bytes(texture);
    cap = buffer->size;
    if (cap < n)
        n = cap;
    memcpy(buffer->mapped_memory, texture->memory.host_mapped_memory, n);
}

void
rend_cpu_texture_blit(RendContextHandle handle, RendTexture *src, RendTexture *dst, uint32_t src_x, uint32_t src_y, uint32_t src_w, uint32_t src_h, uint32_t dst_x, uint32_t dst_y, uint32_t dst_w, uint32_t dst_h)
{
    uint32_t y;
    uint32_t x;
    size_t sbpp;
    size_t dbpp;

    (void)handle;
    if (!src || !dst || !src->memory.host_mapped_memory || !dst->memory.host_mapped_memory)
        return;
    if (!src_w || !src_h || !dst_w || !dst_h)
        return;
    if (src->format >= REND_FORMAT_COUNT || dst->format >= REND_FORMAT_COUNT)
        return;
    sbpp = rend_format_size[src->format];
    dbpp = rend_format_size[dst->format];
    if (!sbpp || !dbpp)
        return;
    if (src->format == dst->format && src_w == dst_w && src_h == dst_h) {
        const uint8_t *sp;
        uint8_t *dp;
        size_t rowb;
        uint32_t dw, dh;

        dw = dst_w;
        dh = dst_h;
        if (dst_x >= dst->width || dst_y >= dst->height)
            return;
        if (src_x >= src->width || src_y >= src->height)
            return;
        if (dst_x + dw > dst->width)
            dw = dst->width - dst_x;
        if (dst_y + dh > dst->height)
            dh = dst->height - dst_y;
        if (src_x + dw > src->width)
            dw = src->width - src_x;
        if (src_y + dh > src->height)
            dh = src->height - src_y;
        rowb = (size_t)dw * sbpp;
        sp = (const uint8_t *)src->memory.host_mapped_memory + ((size_t)src_y * src->width + src_x) * sbpp;
        dp = (uint8_t *)dst->memory.host_mapped_memory + ((size_t)dst_y * dst->width + dst_x) * dbpp;
        for (y = 0; y < dh; y++) {
            memcpy(dp, sp, rowb);
            sp += (size_t)src->width * sbpp;
            dp += (size_t)dst->width * dbpp;
        }
        return;
    }
    if (src->format == dst->format && sbpp == 4) {
        const uint32_t *sp;
        uint32_t *dp;

        sp = src->memory.host_mapped_memory;
        dp = dst->memory.host_mapped_memory;
        for (y = 0; y < dst_h; y++) {
            uint32_t sy;
            uint32_t dy;

            dy = dst_y + y;
            if (dy >= dst->height)
                break;
            sy = src_y + y * src_h / dst_h;
            if (sy >= src->height)
                sy = src->height - 1;
            for (x = 0; x < dst_w; x++) {
                uint32_t sx;
                uint32_t dx;

                dx = dst_x + x;
                if (dx >= dst->width)
                    break;
                sx = src_x + x * src_w / dst_w;
                if (sx >= src->width)
                    sx = src->width - 1;
                dp[(size_t)dy * dst->width + dx] = sp[(size_t)sy * src->width + sx];
            }
        }
        return;
    }
    for (y = 0; y < dst_h; y++) {
        uint32_t sy;
        uint32_t dy;

        dy = dst_y + y;
        if (dy >= dst->height)
            break;
        sy = src_y + y * src_h / dst_h;
        if (sy >= src->height)
            sy = src->height - 1;
        for (x = 0; x < dst_w; x++) {
            uint32_t sx;
            uint32_t dx;
            float r, g, b, a;
            const uint8_t *sp;
            uint8_t *dp;

            dx = dst_x + x;
            if (dx >= dst->width)
                break;
            sx = src_x + x * src_w / dst_w;
            if (sx >= src->width)
                sx = src->width - 1;
            sp = (const uint8_t *)src->memory.host_mapped_memory + ((size_t)sy * src->width + sx) * sbpp;
            dp = (uint8_t *)dst->memory.host_mapped_memory + ((size_t)dy * dst->width + dx) * dbpp;
            rend_cpu_load(sp, src->format, &r, &g, &b, &a);
            rend_cpu_store(dp, dst->format, r, g, b, a);
        }
    }
}

bool
rend_cpu_pipeline_create(RendContextHandle handle, RendPipeline pipeline, Rend__PipelineConfig config, uint8_t type, const uint8_t *shader1, size_t bytes1, const uint8_t *shader2, size_t bytes2, const uint8_t *shader3, size_t bytes3)
{
    RendCpuContext *ctx;
    RendCpuPipeline *p;
    uint32_t i;

    (void)shader3;
    (void)bytes3;
    ctx = handle;
    if (!ctx || !pipeline)
        return false;
    if (type != REND__PIPELINE_GRAPHICS_C)
        return false;
    if (bytes1 || bytes2)
        return false;
    if (!shader1 || !shader2)
        return false;
    if (ctx->pipeline_count >= REND_CPU_MAX_PIPELINES)
        return false;
    if (config.vertex_binding_count > REND_MAX_BINDINGS)
        return false;
    if (config.vertex_attribute_count > REND_CPU_MAX_ATTR)
        return false;
    pipeline->idx = ctx->pipeline_count;
    pipeline->backend_ctx = ctx;
    p = &ctx->pipelines[pipeline->idx];
    memset(p, 0, sizeof *p);
    p->vert = (RendCpuVertFn)(uintptr_t)shader1;
    p->frag = (RendCpuFragFn)(uintptr_t)shader2;
    p->bind_count = config.vertex_binding_count;
    p->attr_count = config.vertex_attribute_count;
    p->topology = config.topology;
    p->cull = config.cull_mode;
    for (i = 0; i < p->bind_count; i++)
        p->binds[i] = config.vertex_bindings[i];
    for (i = 0; i < p->attr_count; i++)
        p->attrs[i] = config.vertex_attributes[i];
    for (i = 0; i < config.push_constant_count; i++)
        p->push_size += config.push_constants[i].size;
    if (p->push_size > REND_CPU_PUSH_MAX)
        p->push_size = REND_CPU_PUSH_MAX;
    ctx->pipeline_count++;
    return true;
}

void
rend_cpu_pipeline_bind(RendPipeline pipeline)
{
    (void)pipeline;
}

void
rend_cpu_pipeline_push_constants(RendPipeline pipeline, void *push_data, size_t size)
{
    RendCpuContext *ctx;
    RendCpuPipeline *p;

    if (!pipeline || !pipeline->backend_ctx || !push_data)
        return;
    ctx = pipeline->backend_ctx;
    p = &ctx->pipelines[pipeline->idx];
    if (size > REND_CPU_PUSH_MAX)
        size = REND_CPU_PUSH_MAX;
    memcpy(p->push, push_data, size);
}

void
rend_cpu_pipeline_bind_vertex_buffer(RendPipeline pipeline, uint32_t binding, RendBuffer buffer, size_t offset)
{
    RendCpuContext *ctx;
    RendCpuPipeline *p;

    if (!pipeline || !pipeline->backend_ctx || binding >= REND_MAX_BINDINGS)
        return;
    ctx = pipeline->backend_ctx;
    p = &ctx->pipelines[pipeline->idx];
    p->vbo[binding] = buffer;
    p->vbo_off[binding] = offset;
    p->vbo_set[binding] = 1;
}

void
rend_cpu_pipeline_bind_index_buffer(RendPipeline pipeline, RendBuffer buffer, size_t offset, RendIndexType index_type)
{
    (void)pipeline;
    (void)buffer;
    (void)offset;
    (void)index_type;
}

void
rend_cpu_pipeline_dispatch(RendPipeline pipeline, uint32_t x, uint32_t y, uint32_t z)
{
    (void)pipeline;
    (void)x;
    (void)y;
    (void)z;
}

void
rend_cpu_pipeline_draw(RendPipeline pipeline, size_t count, uint32_t instance_count)
{
    RendCpuContext *ctx;
    RendCpuPipeline *p;
    uint32_t inst;
    uint32_t i;
    int inst_rate;
    RendCpuVarying vs[16];
    uint8_t attr[REND_CPU_MAX_ATTR * REND_CPU_ATTR_STRIDE];

    if (!pipeline || !pipeline->backend_ctx || !count || !instance_count)
        return;
    ctx = pipeline->backend_ctx;
    p = &ctx->pipelines[pipeline->idx];
    if (!p->vert || !p->frag || !ctx->pass)
        return;
    if (count > 16)
        count = 16;
    inst_rate = rend_cpu_instance_rate(p);
    for (inst = 0; inst < instance_count; inst++) {
        RendCpuVertArgs va;

        if (inst_rate)
            rend_cpu_fetch(p, 0, inst, attr);
        for (i = 0; i < (uint32_t)count; i++) {
            memset(&vs[i], 0, sizeof vs[i]);
            if (!inst_rate)
                rend_cpu_fetch(p, i, inst, attr);
            va.vertex_id = i;
            va.instance_id = inst;
            va.attributes = attr;
            va.push = p->push;
            p->vert(&vs[i], &va);
        }
        if (p->topology == REND_TOPOLOGY_TRIANGLE_STRIP && count == 4) {
            RendTexture *tex;
            float sx[4], sy[4];
            uint32_t fw, fh;
            float eps;

            tex = ctx->pass;
            fw = tex->width;
            fh = tex->height;
            for (i = 0; i < 4; i++)
                rend_cpu_clip_to_screen(&vs[i], fw, fh, &sx[i], &sy[i]);
            eps = 0.01f;
            if (fabsf(sy[0] - sy[1]) <= eps && fabsf(sy[2] - sy[3]) <= eps
                && fabsf(sx[0] - sx[2]) <= eps && fabsf(sx[1] - sx[3]) <= eps
                && fabsf(sx[1] - sx[0]) >= 0.5f && fabsf(sy[2] - sy[0]) >= 0.5f) {
                float area;

                area = (sx[1] - sx[0]) * (sy[2] - sy[0]);
                if (p->cull == REND_CULL_MODE_FRONT_AND_BACK)
                    continue;
                if (p->cull == REND_CULL_MODE_BACK && area < 0.f)
                    continue;
                if (p->cull == REND_CULL_MODE_FRONT && area > 0.f)
                    continue;
                rend_cpu_quad(ctx, p, vs, sx, sy);
                continue;
            }
        }
        if (p->topology == REND_TOPOLOGY_TRIANGLE_STRIP) {
            for (i = 0; i + 2 < (uint32_t)count; i++) {
                if (i & 1)
                    rend_cpu_draw_prim(ctx, p, vs, i + 1, i, i + 2);
                else
                    rend_cpu_draw_prim(ctx, p, vs, i, i + 1, i + 2);
            }
        } else if (p->topology == REND_TOPOLOGY_TRIANGLE_LIST) {
            for (i = 0; i + 2 < (uint32_t)count; i += 3)
                rend_cpu_draw_prim(ctx, p, vs, i, i + 1, i + 2);
        }
    }
}

void
rend_cpu_pipeline_draw_indexed(RendPipeline pipeline, uint32_t index_count, uint32_t first_index, int32_t vertex_offset, uint32_t instance_count)
{
    (void)pipeline;
    (void)index_count;
    (void)first_index;
    (void)vertex_offset;
    (void)instance_count;
}

void
rend_cpu_pipeline_set_blend(RendPipeline pipeline, bool blend)
{
    RendCpuContext *ctx;
    RendCpuPipeline *p;

    if (!pipeline || !pipeline->backend_ctx)
        return;
    ctx = pipeline->backend_ctx;
    p = &ctx->pipelines[pipeline->idx];
    p->blend = blend ? 1 : 0;
}

void
rend_cpu_renderer_render_pass_begin(RendContextHandle handle, float r, float g, float b, float a)
{
    RendCpuContext *ctx;

    ctx = handle;
    if (!ctx)
        return;
    ctx->pass = &ctx->color;
    rend_cpu_clear(ctx->pass, r, g, b, a);
}

void
rend_cpu_renderer_render_pass_begin_texture(RendContextHandle handle, RendTexture *texture)
{
    RendCpuContext *ctx;

    ctx = handle;
    if (!ctx || !texture)
        return;
    ctx->pass = texture;
}

void
rend_cpu_renderer_render_pass_end(RendContextHandle handle)
{
    RendCpuContext *ctx;

    ctx = handle;
    if (ctx)
        ctx->pass = NULL;
}

void
rend_cpu_renderer_render_pass_end_texture(RendContextHandle handle, RendTexture *texture)
{
    (void)texture;
    rend_cpu_renderer_render_pass_end(handle);
}
