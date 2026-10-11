/* ===========================================================================
 * TYPE - Fast font rendering and utf8 parsing - Copyright (c) 2025-2026 Vasco
 * Alves GPL-3.0-only OR a separately negotiated commercial license; see
 * LICENSE.
 *
 * UTF-8 in, glyphs and metrics out. Multiple faces, fallbacks, weight,
 * style, ligatures, CJK, and emoji are in scope. Port of velocitty
 * src/type.zig.
 *
 * FEATURES:
 * - Take either codepoints or UTF-8 bytes. Layout validates UTF-8, then
 *   walks codepoints.
 * - Load bounded sfnt TrueType, OpenType CFF/CFF2 and TTC fonts:
 *      - Deterministic nonzero-fill coverage, quadratic and cubic curves.
 *      - Color emoji is not implemented; variation coordinates default to zero.
 *      - Glyph metrics, advance, and bearings.
 *      - Signed-distance bitmaps are declared and return
 * TYPE_ERR_UNIMPLEMENTED.
 * - Glyph-ID runs accept external shaping/bidi results with clusters and
 *   fractional advances/offsets. UTF-8 layout remains an unshaped helper.
 * - Width/emoji/combining property queries are currently placeholders.
 *
 * ANTI-FEATURES:
 * - No heap allocation. Place persistent state in caller storage and bind a
 *   caller-owned temporary scratch view before cold-glyph rendering.
 * - Does not read files. Font bytes must remain immutable and outlive every
 *   face that points at them. Malformed and over-budget structures fail closed.
 * - Does not grow the face list, the fallback list, or the glyph cache.
 *   Sizes are fixed when the context is placed.
 *
 * PREFIX: TYPE_ (macros)  Type (types)  type_ (functions)
 *
 * USAGE:
 *     #include "Type.h"
 *
 *     TypeParams params;
 *     type_params_default(&params);
 *     unsigned char *buf = ...; // type_memory(&params) bytes
 *     TypeCtx *type = type_place(buf, type_memory(&params), &params);
 *     uint32_t id;
 *     // All admission paths perform the same bounded validation.
 *     type_add_font(type, bytes, len, NULL, &id);
 *     TypeScratch scratch;
 *     type_scratch_init(&scratch, temporary_bytes, temporary_capacity);
 *     type_set_scratch(type, &scratch); // detach before temporary arena reset
 *     TypeGlyph g;
 *     type_glyph(type, 'A', 16.0f, &g);
 *
 * ===========================================================================
 */

#ifndef TYPE_H
#define TYPE_H

#define TYPE_MAJOR 0
#define TYPE_MINOR 3
#define TYPE_PATCH 0

/* CHANGE LOG
 * 0.3.0 - bounded native sfnt/glyf/CFF/CFF2 parser and coverage rasterizer.
 *         No public signature removals. Default admission now parses safely;
 *         trusted calls are aliases. TTC defaults to face 0; place_index
 * selects. Adds TYPE_ERR_LIMIT, outlines, diagnostic names and vertical
 * metrics. Variation outlines use default coordinates; hints are safely
 * ignored. See the Type section of README.md for limits and tests. 0.2.0 -
 * caller scratch; fail-closed font admission; external shaped runs. 0.1.0 -
 * @vasco - public API from velocitty type.zig; caller-provided memory
 */

#include <stddef.h>
#include <stdint.h>

#define TYPE_DEFAULT_ATLAS_WIDTH 1024u
#define TYPE_DEFAULT_ATLAS_HEIGHT 1024u
#define TYPE_DEFAULT_CACHE 2048u
#define TYPE_DEFAULT_MAX_FACES 8u
#define TYPE_DEFAULT_MAX_FALLBACK 8u

#define TYPE_ASCII_LO 32u
#define TYPE_ASCII_HI 126u
#define TYPE_ASCII_N (TYPE_ASCII_HI - TYPE_ASCII_LO + 1u)
#define TYPE_REPLACEMENT 0xFFFDu

/* GlyphKey.flags. Bold is bit 0, italic is bit 1. */
#define TYPE_STYLE_BOLD 1u
#define TYPE_STYLE_ITALIC 2u

typedef struct TypeTTF TypeTTF;     /* Parsed sfnt face: glyf, CFF or CFF2. */
typedef struct TypeOTF TypeOTF;     /* OpenType placement view. */
typedef struct TypeFont TypeFont;   /* Face view: parsed file + weight + style. Borrows the file. */
typedef struct TypeAtlas TypeAtlas; /* Shelf-packed coverage atlas. */
typedef struct TypeAtlasRgba TypeAtlasRgba;
typedef struct TypeCtx TypeCtx; /* Faces, fallbacks, atlases, glyph LRU. */

typedef enum {
    TYPE_OK = 0,
    TYPE_ERR_INVALID_FONT,
    TYPE_ERR_UNSUPPORTED_TABLE,
    TYPE_ERR_GLYPH_NOT_FOUND,
    TYPE_ERR_ATLAS_FULL,
    TYPE_ERR_INVALID_UTF8,
    TYPE_ERR_UNIMPLEMENTED,
    TYPE_ERR_BUF_TOO_SMALL,
    TYPE_ERR_SCRATCH_REQUIRED,
    TYPE_ERR_SCRATCH_FULL,
    TYPE_ERR_UNTRUSTED_FONT, /* retained enum value for source compatibility */
    TYPE_ERR_LIMIT,
} TypeError;

/* Caller-owned temporary-arena view. No allocation or heap fallback.
 * Initialize once per borrowed slice. Operations restore used, retain peak and
 * failed diagnostics. Do not share a view across concurrent/reentrant calls.
 * data must not overlap context, font bytes, output or bitmap storage. */
typedef struct TypeScratch {
    uint8_t *data;
    size_t capacity, used, peak;
    int failed;
} TypeScratch;

void type_scratch_init(TypeScratch *scratch, void *data, size_t capacity);
/* Borrow until detached (NULL). No operation may overlap an arena reset. */
void type_set_scratch(TypeCtx *ctx, TypeScratch *scratch);

typedef enum {
    TYPE_AA_NONE = 0, /* binary coverage */
    TYPE_AA_BOX = 1,  /* box-filter antialiasing */
    TYPE_AA_SDF = 2,  /* signed distance; raster returns TYPE_ERR_UNIMPLEMENTED */
} TypeAntiAlias;

/* CSS-ish usWeightClass. Any u16 is legal; these are the named stops. */
typedef enum {
    TYPE_WEIGHT_THIN = 100,
    TYPE_WEIGHT_EXTRA_LIGHT = 200,
    TYPE_WEIGHT_LIGHT = 300,
    TYPE_WEIGHT_REGULAR = 400,
    TYPE_WEIGHT_MEDIUM = 500,
    TYPE_WEIGHT_SEMI_BOLD = 600,
    TYPE_WEIGHT_BOLD = 700,
    TYPE_WEIGHT_EXTRA_BOLD = 800,
    TYPE_WEIGHT_BLACK = 900,
} TypeWeight;

/* UAX #11. Neutral is the default outside the wide and ambiguous ranges. */
typedef enum {
    TYPE_WIDTH_NARROW = 0,
    TYPE_WIDTH_WIDE,
    TYPE_WIDTH_AMBIGUOUS,
    TYPE_WIDTH_NEUTRAL,
} TypeWidth;

typedef struct TypeStyle {
    uint8_t italic;
    uint8_t bold;
} TypeStyle;

/* weight 0 means "unset": bold style selects TYPE_WEIGHT_BOLD, otherwise
 * REGULAR. */
typedef struct TypeFaceParams {
    uint16_t weight;
    TypeStyle style;
} TypeFaceParams;

/*
 * atlas_* and cache_capacity must be > 0 (type_params_default fills the
 * velocitty defaults). max_faces and max_fallbacks bound storage inside the
 * context buffer. ligatures/color_emoji are reserved; the unshaped helper
 * applies neither GSUB nor color glyphs. antialias is TypeAntiAlias.
 */
typedef struct TypeParams {
    uint32_t atlas_width;
    uint32_t atlas_height;
    uint32_t cache_capacity;
    uint32_t max_faces;
    uint32_t max_fallbacks;
    uint8_t ligatures;
    uint8_t color_emoji;
    uint8_t antialias;
} TypeParams;

typedef struct TypeMetrics {
    float ascender;
    float descender;
    float line_gap;
    uint16_t units_per_em;
} TypeMetrics;

/* pixels is coverage (1 byte) or straight RGBA8 (4 bytes) when color != 0.
 * advance is pixels at the requested size. Bearing is the bitmap origin
 * relative to the pen, y up. */
typedef struct TypeBitmap {
    uint16_t width;
    uint16_t height;
    int16_t bearing_x;
    int16_t bearing_y;
    uint16_t advance;
    uint8_t *pixels;
    uint8_t color;
} TypeBitmap;

typedef struct TypeGlyph {
    uint32_t font_id;
    uint16_t glyph_id;
    float advance;
    float bearing_x;
    float bearing_y;
    uint16_t width;
    uint16_t height;
    uint16_t atlas_x;
    uint16_t atlas_y;
    uint8_t color; /* 0 coverage atlas, 1 color atlas */
} TypeGlyph;

typedef struct TypeCell {
    TypeGlyph glyph;
    float x;
    float y;
    uint32_t codepoint;
} TypeCell;

/* External shaper output, in visual glyph order. cluster is a UTF-8 byte
 * offset in the caller's logical text, never a glyph index. Positions are
 * pixels, fractional, y down (negate HarfBuzz y values on import). font_id
 * and glyph_id must refer to exactly the face shaped, including glyph 0.
 * Run size uses the same integer-pixel rounding as type_glyph. */
typedef struct TypeShapedGlyph {
    uint32_t font_id, glyph_id, cluster;
    float x_advance, y_advance, x_offset, y_offset;
} TypeShapedGlyph;

typedef struct TypePositionedGlyph {
    TypeGlyph glyph;
    uint32_t cluster;
    float x, y;
} TypePositionedGlyph;

/* Ink bounds relative to initial baseline; advance includes invisible glyphs.
 * has_ink == 0 means all bounds are zero. */
typedef struct TypeTextMetrics {
    float advance_x, advance_y;
    float min_x, min_y, max_x, max_y;
    uint8_t has_ink;
} TypeTextMetrics;

typedef struct TypeGlyphKey {
    uint32_t font_id;
    uint32_t glyph_id;
    uint16_t size_px;
    uint16_t flags; /* TYPE_STYLE_BOLD | TYPE_STYLE_ITALIC */
} TypeGlyphKey;

typedef struct TypeGlyphStats {
    uint64_t hits;
    uint64_t misses;
    uint64_t raster_ns;
    uint64_t atlas_ns;
} TypeGlyphStats;

typedef struct TypeRect {
    uint32_t x, y, w, h;
} TypeRect;

void type_params_default(TypeParams *params); /* 1024^2 atlas, 2048 cache, 8 faces, ligatures, color
                                                 emoji, box AA. */
void type_face_params_default(TypeFaceParams *face); /* weight unset, roman. */

/* ---------------------------------------------------------------------------
 * Parsed files. Bytes are borrowed for the life of the placed object.
 * Type.h implementation
 * ------------------------------------------------------------------------- */

size_t type_ttf_memory(void);
size_t type_otf_memory(void);

/* All admission paths validate bounded table views. Nested outlines are checked
 * on access and may return TYPE_ERR_INVALID_FONT, TYPE_ERR_UNSUPPORTED_TABLE or
 * TYPE_ERR_LIMIT. Font bytes must remain immutable. Placement is 16-byte
 * aligned. The trusted suffix is retained as an alias, without bypassing
 * validation. Both placement types accept glyf/CFF/CFF2. TTC chooses face zero
 * by default. */
TypeTTF *type_ttf_place_index(void *buf, size_t bufsize, const void *bytes, size_t len,
                              uint32_t face_index);
TypeTTF *type_ttf_place(void *buf, size_t bufsize, const void *bytes, size_t len);
TypeTTF *type_ttf_place_trusted(void *buf, size_t bufsize, const void *bytes, size_t len);
TypeOTF *type_otf_place(void *buf, size_t bufsize, const void *bytes, size_t len);

TypeError type_ttf_error(const TypeTTF *font); /* TYPE_OK, or why place failed. place(NULL
                                                  buf) is not required. */
TypeError type_otf_error(const TypeOTF *font);

int type_ttf_outline(const TypeTTF *font); /* 1 when supported outlines are usable. */
int type_otf_outline(const TypeOTF *font);
int type_ttf_color(const TypeTTF *font); /* Reserved; color formats are not implemented. */
int type_otf_color(const TypeOTF *font);

/* ---------------------------------------------------------------------------
 * Face. Borrows the placed file; does not copy glyph bytes.
 * ------------------------------------------------------------------------- */

size_t type_font_memory(void);

TypeFont *type_font_from_ttf(void *buf, size_t bufsize, const TypeTTF *ttf,
                             const TypeFaceParams *face);
TypeFont *type_font_from_otf(void *buf, size_t bufsize, const TypeOTF *otf,
                             const TypeFaceParams *face);

uint16_t type_font_weight(const TypeFont *font);
TypeStyle type_font_style(const TypeFont *font);

TypeError type_font_metrics(const TypeFont *font, float size_px, TypeMetrics *out);
TypeError type_font_glyph_index(const TypeFont *font, uint32_t codepoint,
                                uint16_t *out_gid); /* TYPE_ERR_GLYPH_NOT_FOUND when
                                                       unmapped. .notdef is not a hit. */
TypeError type_font_advance(const TypeFont *font, uint16_t glyph_id,
                            uint16_t *out_fu);                       /* font units */
int type_font_has_drawable(const TypeFont *font, uint16_t glyph_id); /* 1 outline or color bitmap */

/* Outline vertices are in font units, y up; MOVE starts a contour, LINE
 * includes the closing edge, QUAD/CUBIC carry one/two controls. count-only
 * calls use vertices == NULL. glyf needs temporary point scratch; CFF does not.
 * Scratch used is restored on all returns. Outputs are only valid on TYPE_OK.
 */
enum { TYPE_VERTEX_MOVE = 1, TYPE_VERTEX_LINE, TYPE_VERTEX_QUAD, TYPE_VERTEX_CUBIC };
typedef struct TypeVertex {
    double x, y, cx, cy, cx1, cy1;
    uint8_t type;
} TypeVertex;
TypeError type_font_outline(const TypeFont *font, uint16_t glyph_id, TypeScratch *scratch,
                            TypeVertex *vertices, uint32_t capacity, uint32_t *count);
/* Name bytes are borrowed, in the encoding identified by platform (typically
 * platform 3 = UTF-16BE). No decoding or allocation is performed. */
TypeError type_font_name(const TypeFont *font, uint16_t name_id, const void **bytes, size_t *len,
                         uint16_t *platform);
TypeError type_font_vertical_advance(const TypeFont *font, uint16_t glyph_id, uint16_t *advance,
                                     int16_t *bearing);

/* Fill everything except pixels. Use type_bitmap_bytes to size the caller
 * buffer. */
TypeError type_font_bitmap_info(const TypeFont *font, uint16_t glyph_id, float size_px,
                                TypeBitmap *out);
size_t type_bitmap_bytes(const TypeBitmap *info); /* width*height, or *4 when color */

/* pixels must hold type_bitmap_bytes of the info at this size. Empty glyphs
 * (width or height 0) succeed and leave pixels untouched. On raster scratch
 * exhaustion coverage is zeroed and out is not published. */
/* Legacy entry point needs no scratch for empty glyphs; otherwise returns
 * TYPE_ERR_SCRATCH_REQUIRED. Use the explicit-scratch variant for rendering. */
TypeError type_font_render_bitmap(const TypeFont *font, uint16_t glyph_id, float size_px,
                                  TypeAntiAlias aa, void *pixels, size_t len, TypeBitmap *out);
TypeError type_font_render_bitmap_scratch(const TypeFont *font, uint16_t glyph_id, float size_px,
                                          TypeAntiAlias aa, void *pixels, size_t len,
                                          TypeScratch *scratch, TypeBitmap *out);
TypeError type_font_render_sdf(const TypeFont *font, uint16_t glyph_id, float size_px, void *pixels,
                               size_t len, TypeBitmap *out);

/* ---------------------------------------------------------------------------
 * Shelf atlas. One byte per pixel, or one u32 (straight RGBA8) for color.
 * Storage is width*height samples inside the same buffer as the header.
 * ------------------------------------------------------------------------- */

size_t type_atlas_memory(uint32_t width, uint32_t height);
size_t type_atlas_rgba_memory(uint32_t width, uint32_t height);

TypeAtlas *type_atlas_place(void *buf, size_t bufsize, uint32_t width, uint32_t height);
TypeAtlasRgba *type_atlas_rgba_place(void *buf, size_t bufsize, uint32_t width, uint32_t height);

const uint8_t *type_atlas_pixels(const TypeAtlas *atlas, uint32_t *width, uint32_t *height);
const uint32_t *type_atlas_rgba_pixels(const TypeAtlasRgba *atlas, uint32_t *width,
                                       uint32_t *height);

int type_atlas_pack(TypeAtlas *atlas, uint32_t w, uint32_t h,
                    TypeRect *out); /* 1 placed, 0 does not fit */
int type_atlas_rgba_pack(TypeAtlasRgba *atlas, uint32_t w, uint32_t h, TypeRect *out);
void type_atlas_blit(TypeAtlas *atlas, const TypeRect *rect,
                     const uint8_t *src); /* src is w*h coverage */
void type_atlas_rgba_blit(TypeAtlasRgba *atlas, const TypeRect *rect,
                          const uint8_t *src); /* src is w*h*4 */
void type_atlas_clear(TypeAtlas *atlas);
void type_atlas_rgba_clear(TypeAtlasRgba *atlas);

/*
 * Context reserves memory for all our needs,
 * based on the parameters we pass to it.
 *
 * NOTE(vasco): First added face becomes primary. A bold+italic face is
 * remembered as bold-italic; a bold face as bold; an italic face as italic.
 * Later adds of the same kind replace that slot. Fallback order is the
 * set_fallbacks list.
 *
 * Resolve: color emoji prefers a color fallback, then the style face
 * (bold-italic, else italic, else bold), then primary, then fallbacks.
 * A miss draws the primary .notdef (glyph id 0).
 *
 * Printable ASCII (32..126) and U+FFFD at the current pixel size bypass the
 * LRU. Styled lookups always use the LRU. Changing size drops those slots
 * and warms ASCII again.
 */
size_t type_memory(const TypeParams *params); /* 0 when params are unusable. */
TypeCtx *type_place(void *buf, size_t bufsize,
                    const TypeParams *params); /* buf must be 16-byte aligned. */

/* Imported and bundled bytes receive identical bounded validation. */
TypeError type_add_font(TypeCtx *ctx, const void *bytes, size_t len, const TypeFaceParams *face,
                        uint32_t *out_id);
TypeError type_add_font_trusted(TypeCtx *ctx, const void *bytes, size_t len,
                                const TypeFaceParams *face, uint32_t *out_id);
void type_clear_fonts(TypeCtx *ctx); /* Drops faces. Font bytes may be freed
                                        after this returns. */
void type_select(TypeCtx *ctx, uint32_t font_id);
TypeError type_set_fallbacks(TypeCtx *ctx, const uint32_t *ids, uint32_t count);

/* Coverage pixel revision; cache hits/packing alone do not change it.
 * Non-consuming: each GPU consumer keeps its own last uploaded revision. */
uint64_t type_ctx_atlas_revision(const TypeCtx *ctx);
const uint8_t *type_ctx_atlas(const TypeCtx *ctx, uint32_t *width, uint32_t *height);
const uint32_t *type_ctx_color_atlas(const TypeCtx *ctx, uint32_t *width, uint32_t *height);

TypeError type_metrics(const TypeCtx *ctx, float size_px, TypeMetrics *out);

/* Cache probe only. Does not rasterize, does not move the LRU, does not bump
 * stats. TYPE_ERR_GLYPH_NOT_FOUND when the size does not match the ASCII cache
 * or the key is cold. */
TypeError type_peek_glyph(const TypeCtx *ctx, uint32_t codepoint, float size_px, TypeGlyph *out);
TypeError type_peek_glyph_styled(const TypeCtx *ctx, uint32_t codepoint, float size_px,
                                 TypeStyle style, TypeGlyph *out);
TypeError type_glyph(TypeCtx *ctx, uint32_t codepoint, float size_px, TypeGlyph *out);
TypeError type_glyph_styled(TypeCtx *ctx, uint32_t codepoint, float size_px, TypeStyle style,
                            TypeGlyph *out);
TypeError type_warm_ascii(TypeCtx *ctx, float size_px);

/* Raster/cache by font-local ID, with no codepoint remapping or ASCII warmup.
 */
TypeError type_glyph_id(TypeCtx *ctx, uint32_t font_id, uint32_t glyph_id, float size_px,
                        TypeGlyph *out);
/* No shaping, bidi, wrapping or fallback is invented here. Use the actual
 * shaper's glyph order, IDs, clusters and positions. On error, written and
 * metrics describe the completed prefix; atlas/cache may retain that prefix.
 * metrics can be requested with out == NULL (cap still bounds the operation).
 * Glyph advance in out remains nominal; metrics uses shaped advances. */
TypeError type_layout_shaped(TypeCtx *ctx, const TypeShapedGlyph *run, uint32_t count,
                             float size_px, TypePositionedGlyph *out, uint32_t cap,
                             uint32_t *written, TypeTextMetrics *metrics);
/* Same ink/advance computation without rasterization, scratch, atlas mutation
 * or cache statistics. count itself is the caller's work bound. */
TypeError type_measure_shaped(const TypeCtx *ctx, const TypeShapedGlyph *run, uint32_t count,
                              float size_px, uint32_t *written, TypeTextMetrics *metrics);

/* Clear both atlases and the LRU, then warm ASCII at the last size. */
void type_clear_atlas(TypeCtx *ctx);

/* One line at baseline y = 0. x advances by glyph.advance. No wrap, no bidi.
 * On TYPE_ERR_BUF_TOO_SMALL, *written is cap and *bytes_used is the next
 * unconsumed byte. On TYPE_ERR_INVALID_UTF8, *bytes_used is the bad byte and
 * cells before it are kept. Both out-counts may be NULL. */
TypeError type_layout_utf8(TypeCtx *ctx, const char *text, size_t len, float size_px, TypeCell *out,
                           uint32_t cap, uint32_t *written, size_t *bytes_used);

void type_stats(const TypeCtx *ctx, TypeGlyphStats *out);

/* ---------------------------------------------------------------------------
 * UTF-8. type_utf8_next returns the byte length, or 0 when the sequence is
 * short or illegal (*codepoint is unchanged).
 * ------------------------------------------------------------------------- */

uint32_t type_utf8_next(const char *s, size_t len, uint32_t *codepoint);
int type_utf8_validate(const char *s, size_t len); /* 1 valid, 0 not */

/* ---------------------------------------------------------------------------
 * East Asian width and emoji class. cell width is 1 or 2.
 * ------------------------------------------------------------------------- */

TypeWidth type_width(uint32_t codepoint);
int type_is_cjk(uint32_t codepoint);
int type_is_emoji(uint32_t codepoint);
int type_cell_width(uint32_t codepoint);
int type_is_combining(uint32_t codepoint);

#endif /* TYPE_H */

#if defined(TYPE_IMPLEMENTATION) && !defined(TYPE_IMPLEMENTATION_ONCE)
#define TYPE_IMPLEMENTATION_ONCE
/* Bounded font views. Every font read goes through TypeCursor. */
#include <float.h>
#include <limits.h>
#include <math.h>
#include <string.h>

#define TYPE_MAX_FONT_BYTES (128u * 1024u * 1024u)
#define TYPE_MAX_TABLES 256u
#define TYPE_MAX_POINTS 16384u
#define TYPE_MAX_CONTOURS 4096u
#define TYPE_MAX_COMPONENTS 256u
#define TYPE_MAX_DEPTH 16u
#define TYPE_MAX_INSTRUCTIONS 32768u
#define TYPE_MAX_VERTICES 32768u
#define TYPE_MAX_EDGES 65536u
#define TYPE_MAX_CFF_STACK 513u
#define TYPE_MAX_CFF_DEPTH 32u
#define TYPE_MAX_OPERATIONS 100000u
#define TYPE_MAX_RASTER_WORK 64000000u
#define TYPE_TAG(a, b, c, d)                                                                       \
    (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(c) << 8) | (uint32_t)(d))

typedef struct TypeView {
    const uint8_t *p;
    size_t n;
} TypeView;
typedef struct TypeCursor {
    TypeView v;
    size_t at;
    int bad;
} TypeCursor;
typedef struct TypeIndex {
    TypeView v;
    uint32_t count;
    size_t base, data;
    unsigned offsize;
} TypeIndex;
static TypeView type_slice(TypeView v, size_t at, size_t n)
{
    TypeView r = {NULL, 0};
    if (at <= v.n && n <= v.n - at) {
        r.p = v.p ? v.p + at : NULL;
        r.n = n;
    }
    return r;
}
static TypeCursor type_cursor(TypeView v)
{
    TypeCursor c;
    c.v = v;
    c.at = 0;
    c.bad = 0;
    return c;
}
static uint32_t type_read(TypeCursor *c, unsigned n)
{
    uint32_t r = 0;
    unsigned i;
    if (!c->v.p || c->at > c->v.n || n > c->v.n - c->at) {
        c->bad = 1;
        return 0;
    }
    for (i = 0; i < n; i++)
        r = (r << 8) | c->v.p[c->at++];
    return r;
}
static uint32_t type_at(TypeView v, size_t at, unsigned n)
{
    TypeCursor c = type_cursor(type_slice(v, at, n));
    return type_read(&c, n);
}
static int32_t type_signed(uint32_t n, unsigned bits)
{
    return n & (1u << (bits - 1))
               ? (int32_t)(n & ((1u << (bits - 1)) - 1)) - (int32_t)(1u << (bits - 1))
               : (int32_t)n;
}
static int type_skip(TypeCursor *c, size_t n)
{
    if (c->at > c->v.n || n > c->v.n - c->at) {
        c->bad = 1;
        return 0;
    }
    c->at += n;
    return 1;
}
static void *ttf_scratch_alloc(size_t size, void *user)
{
    TypeScratch *s = (TypeScratch *)user;
    size_t pad;
    void *p;
    if (!s)
        return NULL;
    if (!s->data || s->used > s->capacity)
        goto full;
    pad = (size_t)(-(uintptr_t)(s->data + s->used)) & 15u;
    if (pad > s->capacity - s->used || size > s->capacity - s->used - pad)
        goto full;
    p = s->data + s->used + pad;
    s->used += pad + size;
    if (s->used > s->peak)
        s->peak = s->used;
    return p;
full:
    s->failed = 1;
    return NULL;
}
void type_scratch_init(TypeScratch *s, void *data, size_t capacity)
{
    if (!s)
        return;
    memset(s, 0, sizeof *s);
    s->data = (uint8_t *)data;
    s->capacity = data ? capacity : 0;
}

enum {
    T_HEAD,
    T_HHEA,
    T_HMTX,
    T_MAXP,
    T_CMAP,
    T_LOCA,
    T_GLYF,
    T_CFF,
    T_OS2,
    T_NAME,
    T_POST,
    T_VHEA,
    T_VMTX,
    T_COUNT
};
struct TypeTTF {
    const uint8_t *bytes;
    size_t len;
    TypeError err;
    int outline, num_glyphs;
    uint16_t units_per_em, hmetrics, vmetrics;
    int16_t ascent, descent, line_gap;
    TypeView table[T_COUNT], cmap, variation;
    TypeIndex chars, gsubrs, fds, subrs;
    TypeView fdselect, vstore;
    double matrix[6];
    int cff2, long_loca;
    uint32_t charset, default_vs;
};
struct TypeOTF {
    TypeTTF font;
};
struct TypeFont {
    const TypeTTF *ttf;
    uint16_t weight;
    TypeStyle style;
};

static int type_index(TypeView v, size_t *pos, int wide, TypeIndex *out)
{
    TypeCursor c = type_cursor(v);
    uint32_t i, last = 0, n;
    unsigned s;
    size_t data;
    memset(out, 0, sizeof *out);
    c.at = *pos;
    n = type_read(&c, wide ? 4 : 2);
    if (c.bad || n > 65535u)
        return 0;
    out->v = v;
    out->count = n;
    if (!n) {
        *pos = c.at;
        return 1;
    }
    s = type_read(&c, 1);
    if (c.bad || s < 1 || s > 4)
        return 0;
    out->offsize = s;
    out->base = c.at;
    if ((size_t)n + 1 > (v.n - c.at) / s)
        return 0;
    data = c.at + ((size_t)n + 1) * s;
    out->data = data;
    for (i = 0; i <= n; i++) {
        uint32_t k = type_read(&c, s);
        if (c.bad || !k || (i == 0 && k != 1) || k < last || k - 1 > v.n - data)
            return 0;
        last = k;
    }
    *pos = data + last - 1;
    return 1;
}
static TypeView type_index_item(TypeIndex idx, uint32_t i)
{
    uint32_t a, b;
    TypeView empty = {NULL, 0};
    if (i >= idx.count)
        return empty;
    a = type_at(idx.v, idx.base + (size_t)i * idx.offsize, idx.offsize);
    b = type_at(idx.v, idx.base + ((size_t)i + 1) * idx.offsize, idx.offsize);
    return type_slice(idx.v, idx.data + a - 1, b - a);
}
/* DICT numbers, including a bounded decimal real parser. */
static int type_number(TypeCursor *c, uint32_t b, double *out, int dict)
{
    int32_t v;
    if (b >= 32 && b <= 246)
        *out = (double)((int)b - 139);
    else if (b >= 247 && b <= 250)
        *out = (double)((b - 247) * 256 + type_read(c, 1) + 108);
    else if (b >= 251 && b <= 254)
        *out = -(double)((b - 251) * 256 + type_read(c, 1) + 108);
    else if (b == 28)
        *out = (double)type_signed(type_read(c, 2), 16);
    else if (b == 29 && dict) {
        uint32_t u = type_read(c, 4);
        *out = u > INT32_MAX ? -(double)(UINT32_MAX - u) - 1.0 : (double)u;
    } else if (b == 255 && !dict) {
        uint32_t u = type_read(c, 4);
        *out = (u > INT32_MAX ? -(double)(UINT32_MAX - u) - 1.0 : (double)u) / 65536.0;
    } else if (b == 30 && dict) {
        char text[64];
        unsigned n = 0, k;
        int done = 0;
        while (!done && !c->bad) {
            uint32_t u = type_read(c, 1);
            for (k = 0; k < 2; k++) {
                unsigned d = (u >> (k ? 0 : 4)) & 15u;
                if (d == 15) {
                    done = 1;
                    break;
                }
                if (d == 13)
                    return 0;
                if (n > 58)
                    return 0;
                if (d < 10)
                    text[n++] = (char)('0' + d);
                else if (d == 10)
                    text[n++] = '.';
                else if (d == 11 || d == 12) {
                    text[n++] = 'e';
                    if (d == 12)
                        text[n++] = '-';
                } else
                    text[n++] = '-';
            }
        }
        /* Locale-independent conversion, bounded exponent. */
        {
            unsigned j = 0;
            double r = 0, frac = 1;
            int sign = 1, exp = 0, es = 1, dot = 0, digits = 0;
            if (j < n && text[j] == '-') {
                sign = -1;
                j++;
            }
            for (; j < n && text[j] != 'e'; j++) {
                if (text[j] == '.') {
                    if (dot)
                        return 0;
                    dot = 1;
                    continue;
                }
                if (text[j] < '0' || text[j] > '9')
                    return 0;
                digits++;
                r = r * 10 + text[j] - '0';
                if (dot)
                    frac *= 10;
            }
            if (!digits)
                return 0;
            if (j < n) {
                j++;
                if (j < n && text[j] == '-') {
                    es = -1;
                    j++;
                }
                if (j == n)
                    return 0;
                for (; j < n; j++) {
                    if (text[j] < '0' || text[j] > '9' || exp > 100)
                        return 0;
                    exp = exp * 10 + text[j] - '0';
                }
            }
            if (exp > 30)
                return 0;
            *out = sign * r / frac;
            while (exp--)
                *out = es > 0 ? *out * 10 : *out / 10;
        }
    } else
        return 0;
    v = c->bad;
    return !v && isfinite(*out) && fabs(*out) <= 2147483648.0;
}
typedef struct TypeDict {
    uint32_t chars, private_at, private_n, subrs, fdarray, fdselect, vstore, charset, encoding,
        vsindex;
    double matrix[6];
    int has_matrix;
} TypeDict;
static int type_dict(TypeView view, TypeDict *d)
{
    TypeCursor c = type_cursor(view);
    double s[TYPE_MAX_CFF_STACK];
    unsigned n = 0, budget = TYPE_MAX_OPERATIONS;
    memset(d, 0, sizeof *d);
    while (c.at < c.v.n && !c.bad) {
        uint32_t b = type_read(&c, 1), op = b;
        if (!budget--)
            return 0;
        if (b >= 28 && b != 31) {
            double x;
            if (!type_number(&c, b, &x, 1) || n == TYPE_MAX_CFF_STACK)
                return 0;
            s[n++] = x;
            continue;
        }
        if (b == 12)
            op = 0x100u + type_read(&c, 1);
        if (op == 17 || op == 19 || op == 0x124 || op == 0x125 || op == 24 || op == 15 ||
            op == 16 || op == 22) {
            uint32_t x;
            if (n != 1 || s[0] < 0 || s[0] > UINT32_MAX || floor(s[0]) != s[0])
                return 0;
            x = (uint32_t)s[0];
            if (op == 17)
                d->chars = x;
            else if (op == 19)
                d->subrs = x;
            else if (op == 0x124)
                d->fdarray = x;
            else if (op == 0x125)
                d->fdselect = x;
            else if (op == 24)
                d->vstore = x;
            else if (op == 22)
                d->vsindex = x;
            else if (op == 15)
                d->charset = x;
            else
                d->encoding = x;
        } else if (op == 18) {
            if (n != 2 || s[0] < 0 || s[1] < 0 || s[0] > UINT32_MAX || s[1] > UINT32_MAX ||
                floor(s[0]) != s[0] || floor(s[1]) != s[1])
                return 0;
            d->private_n = (uint32_t)s[0];
            d->private_at = (uint32_t)s[1];
        } else if (op == 0x107) {
            unsigned i;
            if (n != 6)
                return 0;
            for (i = 0; i < 6; i++)
                d->matrix[i] = s[i];
            d->has_matrix = 1;
        }
        n = 0;
    }
    return !c.bad && n == 0;
}
static int type_private(const TypeTTF *f, TypeDict d, TypeIndex *sub, uint32_t *vs)
{
    TypeDict p;
    size_t at;
    TypeView v = f->table[T_CFF];
    memset(sub, 0, sizeof *sub);
    *vs = 0;
    if (!d.private_n)
        return 1;
    if (!type_slice(v, d.private_at, d.private_n).p ||
        !type_dict(type_slice(v, d.private_at, d.private_n), &p))
        return 0;
    *vs = p.vsindex;
    if (!p.subrs)
        return 1;
    if (d.private_at > v.n || p.subrs > v.n - d.private_at)
        return 0;
    at = (size_t)d.private_at + p.subrs;
    return type_index(v, &at, f->cff2, sub);
}
static int type_cff_init(TypeTTF *f)
{
    TypeView v = f->table[T_CFF];
    TypeCursor c = type_cursor(v);
    TypeIndex idx;
    TypeDict d;
    size_t at;
    unsigned i;
    uint32_t max_vs = 0, major = type_read(&c, 1);
    type_read(&c, 1);
    at = type_read(&c, 1);
    if (c.bad || at > v.n || at < (f->cff2 ? 5u : 4u) || major != (f->cff2 ? 2u : 1u))
        return 0;
    if (f->cff2) {
        uint32_t n = type_read(&c, 2);
        if (!type_slice(v, at, n).p || !type_dict(type_slice(v, at, n), &d))
            return 0;
        at += n;
    } else {
        if (type_at(v, 3, 1) < 1 || type_at(v, 3, 1) > 4)
            return 0;
        if (!type_index(v, &at, 0, &idx) || idx.count != 1 || !type_index(v, &at, 0, &idx) ||
            idx.count != 1 || !type_dict(type_index_item(idx, 0), &d) ||
            !type_index(v, &at, 0, &idx))
            return 0;
    }
    if (!type_index(v, &at, f->cff2, &f->gsubrs))
        return 0;
    at = d.chars;
    if (!d.chars || !type_index(v, &at, f->cff2, &f->chars) ||
        f->chars.count != (uint32_t)f->num_glyphs)
        return 0;
    f->charset = d.charset;
    if (!type_private(f, d, &f->subrs, &f->default_vs))
        return 0;
    max_vs = f->default_vs;
    for (i = 0; i < 6; i++)
        f->matrix[i] = d.has_matrix
                           ? d.matrix[i] * f->units_per_em
                           : (i == 0 || i == 3 ? (f->cff2 ? 1.0 : f->units_per_em / 1000.0) : 0.0);
    if (d.fdarray) {
        at = d.fdarray;
        if (!type_index(v, &at, f->cff2, &f->fds) || !f->fds.count || f->fds.count > 256)
            return 0;
        for (i = 0; i < f->fds.count; i++) {
            TypeDict fd;
            TypeIndex sub;
            uint32_t vs;
            if (!type_dict(type_index_item(f->fds, i), &fd) || !type_private(f, fd, &sub, &vs))
                return 0;
            if (vs > max_vs)
                max_vs = vs;
        }
        if (!d.fdselect && f->cff2 && f->fds.count == 1)
            goto fd_done;
        if (d.fdselect >= v.n)
            return 0;
        f->fdselect = type_slice(v, d.fdselect, v.n - d.fdselect);
        {
            TypeCursor s = type_cursor(f->fdselect);
            unsigned format = type_read(&s, 1);
            uint32_t n, j, last = 0;
            if (format == 0) {
                for (j = 0; j < (uint32_t)f->num_glyphs; j++)
                    if (type_read(&s, 1) >= f->fds.count)
                        return 0;
            } else if (format == 3 || (format == 4 && f->cff2)) {
                unsigned w = format == 3 ? 2 : 4, fw = format == 3 ? 1 : 2;
                n = type_read(&s, w);
                if (!n || n > (uint32_t)f->num_glyphs)
                    return 0;
                for (j = 0; j < n; j++) {
                    uint32_t start = type_read(&s, w), fd = type_read(&s, fw);
                    if ((!j && start) || (j && start <= last) || start >= (uint32_t)f->num_glyphs ||
                        fd >= f->fds.count)
                        return 0;
                    last = start;
                }
                if (type_read(&s, w) != (uint32_t)f->num_glyphs)
                    return 0;
            } else
                return 0;
            if (s.bad)
                return 0;
        }
    } else if (d.fdselect)
        return 0;
fd_done:;
    /* Validate charset/encoding, even though Unicode lookup uses cmap. */
    if (!f->cff2 && d.charset > 2) {
        TypeCursor s;
        uint32_t left = (uint32_t)f->num_glyphs - 1;
        unsigned fmt;
        if (d.charset >= v.n)
            return 0;
        s = type_cursor(type_slice(v, d.charset, v.n - d.charset));
        fmt = type_read(&s, 1);
        while (left && !s.bad) {
            uint32_t run = 1;
            type_read(&s, 2);
            if (fmt == 1 || fmt == 2)
                run += type_read(&s, fmt == 1 ? 1 : 2);
            else if (fmt != 0)
                return 0;
            if (run > left)
                return 0;
            left -= run;
        }
        if (s.bad)
            return 0;
    }
    if (!f->cff2 && d.encoding > 1) {
        TypeCursor s;
        unsigned fmt, n, j;
        if (d.encoding >= v.n)
            return 0;
        s = type_cursor(type_slice(v, d.encoding, v.n - d.encoding));
        fmt = type_read(&s, 1);
        n = type_read(&s, 1);
        if ((fmt & 127u) > 1)
            return 0;
        for (j = 0; j < n; j++) {
            type_read(&s, 1);
            if (fmt & 1u)
                type_read(&s, 1);
        }
        if (fmt & 128u) {
            n = type_read(&s, 1);
            for (j = 0; j < n; j++) {
                type_read(&s, 1);
                type_read(&s, 2);
            }
        }
        if (s.bad)
            return 0;
    }
    if (d.vstore) {
        TypeCursor s;
        uint32_t length, off, n, j;
        TypeView store;
        if (d.vstore >= v.n)
            return 0;
        s = type_cursor(type_slice(v, d.vstore, v.n - d.vstore));
        length = type_read(&s, 2);
        store = type_slice(s.v, s.at, length);
        s = type_cursor(store);
        if (type_read(&s, 2) != 1)
            return 0;
        off = type_read(&s, 4);
        n = type_read(&s, 2);
        if (n > 256 || s.bad || n > (s.v.n - s.at) / 4)
            return 0;
        for (j = 0; j < n; j++) {
            uint32_t data = type_read(&s, 4), regions;
            TypeCursor t;
            if (data >= store.n)
                return 0;
            t = type_cursor(type_slice(store, data, store.n - data));
            type_read(&t, 2);
            type_read(&t, 2);
            regions = type_read(&t, 2);
            if (t.bad || regions > 256 || regions > (t.v.n - t.at) / 2)
                return 0;
        }
        if (off >= store.n)
            return 0;
        {
            TypeCursor t = type_cursor(type_slice(store, off, store.n - off));
            uint32_t axes = type_read(&t, 2), regions = type_read(&t, 2);
            if (t.bad || axes > 64 || regions > 256 || (size_t)axes * regions > (t.v.n - t.at) / 6)
                return 0;
        }
        f->vstore = store;
    }
    if (f->cff2 && max_vs && (!f->vstore.p || max_vs >= type_at(f->vstore, 6, 2)))
        return 0;
    return !c.bad;
}
static int type_cmap_validate(TypeView v, int glyphs)
{
    TypeCursor c = type_cursor(v);
    uint32_t fmt = type_read(&c, 2), n, i, last = 0, len;
    if (fmt == 0) {
        len = type_read(&c, 2);
        type_read(&c, 2);
        if (len < 262 || len > v.n)
            return 0;
        for (i = 0; i < 256; i++)
            if (type_read(&c, 1) >= (uint32_t)glyphs)
                return 0;
    } else if (fmt == 6) {
        len = type_read(&c, 2);
        type_read(&c, 2);
        n = type_read(&c, 2);
        i = type_read(&c, 2);
        if (len > v.n || len < 10 || i > (len - 10) / 2 || n + i > 65536)
            return 0;
        for (n = 0; n < i; n++)
            if (type_read(&c, 2) >= (uint32_t)glyphs)
                return 0;
    } else if (fmt == 4) {
        uint32_t seg, j;
        len = type_read(&c, 2);
        type_read(&c, 2);
        seg = type_read(&c, 2);
        if ((seg & 1) || seg < 2 || len > v.n || len < 16 || seg / 2 > (len - 16) / 8)
            return 0;
        seg /= 2;
        for (j = 0; j < seg; j++) {
            uint32_t end = type_at(v, 14 + 2 * j, 2), start = type_at(v, 16 + 2 * seg + 2 * j, 2),
                     ro = type_at(v, 16 + 6 * seg + 2 * j, 2);
            size_t p = 16 + 6 * seg + 2 * j;
            if (start > end || (j && start <= last) || (ro & 1))
                return 0;
            last = end;
            if (ro && (ro > len - p || (size_t)(end - start) + 1 > (len - p - ro) / 2))
                return 0;
        }
        if (last != 65535)
            return 0;
    } else if (fmt == 12 || fmt == 13) {
        type_read(&c, 2);
        len = type_read(&c, 4);
        type_read(&c, 4);
        n = type_read(&c, 4);
        if (c.bad || len > v.n || len < 16 || n > (len - 16) / 12 || n > 65536)
            return 0;
        for (i = 0; i < n; i++) {
            uint32_t start = type_read(&c, 4), end = type_read(&c, 4), gid = type_read(&c, 4);
            if (start > end || end > 0x10ffff || (i && start <= last) || gid >= (uint32_t)glyphs ||
                (fmt == 12 && end - start >= (uint32_t)glyphs - gid))
                return 0;
            last = end;
        }
    } else if (fmt == 14) {
        len = type_read(&c, 4);
        n = type_read(&c, 4);
        if (c.bad || len > v.n || len < 10 || n > (len - 10) / 11 || n > 256)
            return 0;
        for (i = 0; i < n; i++) {
            uint32_t sel = type_read(&c, 3), a = type_read(&c, 4), b = type_read(&c, 4), j, count,
                     prev = 0;
            TypeCursor t;
            if (sel > 0x10ffff || (i && sel <= last))
                return 0;
            last = sel;
            if (a) {
                if (a > len || len - a < 4)
                    return 0;
                t = type_cursor(type_slice(v, a, len - a));
                count = type_read(&t, 4);
                if (count > 65536 || count > (len - a - 4) / 4)
                    return 0;
                for (j = 0; j < count; j++) {
                    uint32_t cp = type_read(&t, 3), run = type_read(&t, 1);
                    if (cp > 0x10ffff - run || (j && cp <= prev))
                        return 0;
                    prev = cp + run;
                }
            }
            if (b) {
                if (b > len || len - b < 4)
                    return 0;
                t = type_cursor(type_slice(v, b, len - b));
                count = type_read(&t, 4);
                if (count > 65536 || count > (len - b - 4) / 5)
                    return 0;
                prev = 0;
                for (j = 0; j < count; j++) {
                    uint32_t cp = type_read(&t, 3), g = type_read(&t, 2);
                    if (cp > 0x10ffff || (j && cp <= prev) || g >= (uint32_t)glyphs)
                        return 0;
                    prev = cp;
                }
            }
        }
    } else
        return 0;
    return !c.bad;
}
static TypeError type_parse(TypeTTF *f, const void *bytes, size_t len, uint32_t face)
{
    static const uint32_t tags[T_COUNT] = {
        TYPE_TAG('h', 'e', 'a', 'd'), TYPE_TAG('h', 'h', 'e', 'a'), TYPE_TAG('h', 'm', 't', 'x'),
        TYPE_TAG('m', 'a', 'x', 'p'), TYPE_TAG('c', 'm', 'a', 'p'), TYPE_TAG('l', 'o', 'c', 'a'),
        TYPE_TAG('g', 'l', 'y', 'f'), TYPE_TAG('C', 'F', 'F', ' '), TYPE_TAG('O', 'S', '/', '2'),
        TYPE_TAG('n', 'a', 'm', 'e'), TYPE_TAG('p', 'o', 's', 't'), TYPE_TAG('v', 'h', 'e', 'a'),
        TYPE_TAG('v', 'm', 't', 'x')};
    TypeView all;
    TypeCursor c;
    uint32_t signature, n, i, j;
    size_t start = 0;
    int best = -1;
    if (!bytes || len < 12 || len > TYPE_MAX_FONT_BYTES)
        return TYPE_ERR_INVALID_FONT;
    all.p = (const uint8_t *)bytes;
    all.n = len;
    c = type_cursor(all);
    signature = type_read(&c, 4);
    if (signature == TYPE_TAG('t', 't', 'c', 'f')) {
        uint32_t ver = type_read(&c, 4), count = type_read(&c, 4);
        if ((ver != 0x10000 && ver != 0x20000) || !count || count > 256 || count > (len - 12) / 4 ||
            face >= count)
            return TYPE_ERR_INVALID_FONT;
        start = type_at(all, 12 + (size_t)face * 4, 4);
        if (start > len || len - start < 12)
            return TYPE_ERR_INVALID_FONT;
        c = type_cursor(type_slice(all, start, len - start));
        signature = type_read(&c, 4);
    } else if (face)
        return TYPE_ERR_GLYPH_NOT_FOUND;
    if (signature != 0x10000 && signature != TYPE_TAG('t', 'r', 'u', 'e') &&
        signature != TYPE_TAG('O', 'T', 'T', 'O'))
        return TYPE_ERR_INVALID_FONT;
    n = type_read(&c, 2);
    if (!n || n > TYPE_MAX_TABLES || n > (c.v.n - 12) / 16)
        return TYPE_ERR_INVALID_FONT;
    type_skip(&c, 6);
    for (i = 0; i < n; i++) {
        uint32_t tag = type_read(&c, 4), off, size;
        type_read(&c, 4);
        off = type_read(&c, 4);
        size = type_read(&c, 4);
        if (c.bad || off > len || size > len - off)
            return TYPE_ERR_INVALID_FONT;
        for (j = 0; j < i; j++)
            if (type_at(c.v, 12 + (size_t)j * 16, 4) == tag)
                return TYPE_ERR_INVALID_FONT;
        for (j = 0; j < T_COUNT; j++)
            if (tag == tags[j] || (j == T_CFF && tag == TYPE_TAG('C', 'F', 'F', '2'))) {
                if (f->table[j].p)
                    return TYPE_ERR_INVALID_FONT;
                f->table[j] = type_slice(all, off, size);
                if (tag == TYPE_TAG('C', 'F', 'F', '2'))
                    f->cff2 = 1;
            }
    }
    if (f->table[T_HEAD].n < 54 || f->table[T_HHEA].n < 36 || f->table[T_MAXP].n < 6 ||
        f->table[T_CMAP].n < 4)
        return TYPE_ERR_INVALID_FONT;
    if (type_at(f->table[T_HEAD], 12, 4) != 0x5f0f3cf5 ||
        type_at(f->table[T_HHEA], 0, 4) != 0x10000)
        return TYPE_ERR_INVALID_FONT;
    f->units_per_em = (uint16_t)type_at(f->table[T_HEAD], 18, 2);
    f->num_glyphs = (int)type_at(f->table[T_MAXP], 4, 2);
    f->long_loca = (int)type_at(f->table[T_HEAD], 50, 2);
    if (f->units_per_em < 16 || f->units_per_em > 16384 || !f->num_glyphs || f->long_loca > 1)
        return TYPE_ERR_INVALID_FONT;
    f->hmetrics = (uint16_t)type_at(f->table[T_HHEA], 34, 2);
    if (!f->hmetrics || f->hmetrics > f->num_glyphs ||
        f->table[T_HMTX].n < (size_t)f->hmetrics * 4 + ((size_t)f->num_glyphs - f->hmetrics) * 2)
        return TYPE_ERR_INVALID_FONT;
    f->ascent = (int16_t)type_signed(type_at(f->table[T_HHEA], 4, 2), 16);
    f->descent = (int16_t)type_signed(type_at(f->table[T_HHEA], 6, 2), 16);
    f->line_gap = (int16_t)type_signed(type_at(f->table[T_HHEA], 8, 2), 16);
    if (f->table[T_VHEA].p || f->table[T_VMTX].p) {
        if (f->table[T_VHEA].n < 36 || type_at(f->table[T_VHEA], 0, 4) < 0x10000 ||
            type_at(f->table[T_VHEA], 0, 4) > 0x11000)
            return TYPE_ERR_INVALID_FONT;
        f->vmetrics = (uint16_t)type_at(f->table[T_VHEA], 34, 2);
        if (!f->vmetrics || f->vmetrics > f->num_glyphs ||
            f->table[T_VMTX].n <
                (size_t)f->vmetrics * 4 + ((size_t)f->num_glyphs - f->vmetrics) * 2)
            return TYPE_ERR_INVALID_FONT;
    }
    if (f->table[T_OS2].p) {
        unsigned version = type_at(f->table[T_OS2], 0, 2);
        size_t need = version == 0 ? 78 : (version == 1 ? 86 : (version < 5 ? 96 : 100));
        if (version > 5 || f->table[T_OS2].n < need)
            return TYPE_ERR_INVALID_FONT;
    }
    if (f->table[T_NAME].p) {
        TypeView v = f->table[T_NAME];
        uint32_t count, strings;
        unsigned format;
        TypeCursor t = type_cursor(v);
        format = type_read(&t, 2);
        count = type_read(&t, 2);
        strings = type_read(&t, 2);
        if (t.bad || format > 1 || count > (v.n - 6) / 12 || strings > v.n ||
            strings < 6 + (size_t)count * 12)
            return TYPE_ERR_INVALID_FONT;
        for (i = 0; i < count; i++) {
            uint32_t l, off;
            type_skip(&t, 8);
            l = type_read(&t, 2);
            off = type_read(&t, 2);
            if (off > v.n - strings || l > v.n - strings - off)
                return TYPE_ERR_INVALID_FONT;
        }
        if (format == 1) {
            uint32_t k = type_read(&t, 2);
            if (t.bad || k > (v.n - t.at) / 4)
                return TYPE_ERR_INVALID_FONT;
            for (i = 0; i < k; i++) {
                uint32_t l = type_read(&t, 2), off = type_read(&t, 2);
                if (off > v.n - strings || l > v.n - strings - off)
                    return TYPE_ERR_INVALID_FONT;
            }
        }
    }
    if (f->table[T_POST].p) {
        TypeView v = f->table[T_POST];
        uint32_t format = type_at(v, 0, 4);
        if (v.n < 32)
            return TYPE_ERR_INVALID_FONT;
        if (format == 0x20000) {
            TypeCursor t = type_cursor(v);
            uint32_t count, highest = 257;
            t.at = 32;
            count = type_read(&t, 2);
            if (t.bad || count != (uint32_t)f->num_glyphs || count > (v.n - t.at) / 2)
                return TYPE_ERR_INVALID_FONT;
            for (i = 0; i < count; i++) {
                uint32_t k = type_read(&t, 2);
                if (k > highest)
                    highest = k;
            }
            for (i = 258; i <= highest; i++) {
                uint32_t l = type_read(&t, 1);
                if (!type_skip(&t, l))
                    return TYPE_ERR_INVALID_FONT;
            }
            if (t.bad)
                return TYPE_ERR_INVALID_FONT;
        } else if (format == 0x25000) {
            if (v.n < 34 + (size_t)f->num_glyphs)
                return TYPE_ERR_INVALID_FONT;
        } else if (format != 0x10000 && format != 0x30000 && format != 0x40000)
            return TYPE_ERR_UNSUPPORTED_TABLE;
    }
    c = type_cursor(f->table[T_CMAP]);
    if (type_read(&c, 2) != 0)
        return TYPE_ERR_INVALID_FONT;
    n = type_read(&c, 2);
    if (!n || n > 256 || n > (c.v.n - 4) / 8)
        return TYPE_ERR_INVALID_FONT;
    for (i = 0; i < n; i++) {
        uint32_t pid = type_read(&c, 2), eid = type_read(&c, 2), off = type_read(&c, 4), fmt;
        TypeView v;
        int score;
        if (off > c.v.n || c.v.n - off < 2)
            return TYPE_ERR_INVALID_FONT;
        v = type_slice(c.v, off, c.v.n - off);
        fmt = type_at(v, 0, 2);
        if (fmt != 0 && fmt != 4 && fmt != 6 && fmt != 12 && fmt != 13 && fmt != 14)
            continue;
        if (!type_cmap_validate(v, f->num_glyphs))
            return TYPE_ERR_INVALID_FONT;
        if (fmt == 14) {
            f->variation = v;
            continue;
        }
        if (pid != 0 && !(pid == 3 && (eid == 1 || eid == 10)))
            continue;
        score = (fmt == 12 ? 100 : (fmt == 13 ? 90 : (fmt == 4 ? 80 : 20))) + (pid == 3 ? 1 : 0);
        if (score > best) {
            f->cmap = v;
            best = score;
        }
    }
    if (best < 0)
        return TYPE_ERR_UNSUPPORTED_TABLE;
    if (f->table[T_GLYF].p && f->table[T_LOCA].p) {
        uint32_t last = 0;
        unsigned w = f->long_loca ? 4 : 2;
        if (type_at(f->table[T_MAXP], 0, 4) != 0x10000 || f->table[T_MAXP].n < 32 ||
            f->table[T_LOCA].n < ((size_t)f->num_glyphs + 1) * w)
            return TYPE_ERR_INVALID_FONT;
        for (i = 0; i <= (uint32_t)f->num_glyphs; i++) {
            uint32_t off = type_at(f->table[T_LOCA], (size_t)i * w, w);
            if (w == 2)
                off *= 2;
            if (off < last || off > f->table[T_GLYF].n)
                return TYPE_ERR_INVALID_FONT;
            last = off;
        }
    } else if (f->table[T_CFF].p) {
        if (type_at(f->table[T_MAXP], 0, 4) != 0x5000 || !type_cff_init(f))
            return TYPE_ERR_INVALID_FONT;
    } else
        return TYPE_ERR_UNSUPPORTED_TABLE;
    f->bytes = bytes;
    f->len = len;
    f->outline = 1;
    return TYPE_OK;
}
size_t type_ttf_memory(void)
{
    return sizeof(TypeTTF);
}
size_t type_otf_memory(void)
{
    return sizeof(TypeOTF);
}
TypeTTF *type_ttf_place_index(void *buf, size_t cap, const void *bytes, size_t len, uint32_t face)
{
    TypeTTF *f;
    if (!buf || ((uintptr_t)buf & 15u) || cap < sizeof *f)
        return NULL;
    f = buf;
    memset(f, 0, sizeof *f);
    f->err = type_parse(f, bytes, len, face);
    return f->err == TYPE_OK ? f : NULL;
}
TypeTTF *type_ttf_place(void *buf, size_t cap, const void *bytes, size_t len)
{
    return type_ttf_place_index(buf, cap, bytes, len, 0);
}
TypeTTF *type_ttf_place_trusted(void *buf, size_t cap, const void *bytes, size_t len)
{
    return type_ttf_place(buf, cap, bytes, len);
}
TypeOTF *type_otf_place(void *buf, size_t cap, const void *bytes, size_t len)
{
    return (TypeOTF *)type_ttf_place(buf, cap, bytes, len);
}
TypeError type_ttf_error(const TypeTTF *f)
{
    return f ? f->err : TYPE_ERR_INVALID_FONT;
}
TypeError type_otf_error(const TypeOTF *f)
{
    return f ? f->font.err : TYPE_ERR_INVALID_FONT;
}
int type_ttf_outline(const TypeTTF *f)
{
    return f && f->err == TYPE_OK && f->outline;
}
int type_otf_outline(const TypeOTF *f)
{
    return f && type_ttf_outline(&f->font);
}
int type_ttf_color(const TypeTTF *f)
{
    (void)f;
    return 0;
}
int type_otf_color(const TypeOTF *f)
{
    (void)f;
    return 0;
}
size_t type_font_memory(void)
{
    return sizeof(TypeFont);
}
TypeFont *type_font_from_ttf(void *buf, size_t cap, const TypeTTF *f, const TypeFaceParams *params)
{
    TypeFont *font;
    TypeFaceParams d;
    if (!buf || ((uintptr_t)buf & 15u) || cap < sizeof *font || !f || f->err != TYPE_OK)
        return NULL;
    if (!params) {
        type_face_params_default(&d);
        params = &d;
    }
    font = buf;
    memset(font, 0, sizeof *font);
    font->ttf = f;
    font->style = params->style;
    font->weight = params->weight ? params->weight
                                  : (params->style.bold ? TYPE_WEIGHT_BOLD : TYPE_WEIGHT_REGULAR);
    return font;
}
TypeFont *type_font_from_otf(void *buf, size_t cap, const TypeOTF *f, const TypeFaceParams *params)
{
    return type_font_from_ttf(buf, cap, f ? &f->font : NULL, params);
}
uint16_t type_font_weight(const TypeFont *f)
{
    return f ? f->weight : 0;
}
TypeStyle type_font_style(const TypeFont *f)
{
    TypeStyle s = {0, 0};
    return f ? f->style : s;
}
TypeError type_font_metrics(const TypeFont *f, float px, TypeMetrics *out)
{
    float s;
    if (!f || !f->ttf || !out || !(px > 0 && px <= 65535))
        return TYPE_ERR_INVALID_FONT;
    s = px / f->ttf->units_per_em;
    out->ascender = f->ttf->ascent * s;
    out->descender = f->ttf->descent * s;
    out->line_gap = f->ttf->line_gap * s;
    out->units_per_em = f->ttf->units_per_em;
    return TYPE_OK;
}
TypeError type_font_glyph_index(const TypeFont *f, uint32_t cp, uint16_t *out)
{
    TypeView v;
    uint32_t fmt, g = 0;
    if (!f || !f->ttf || !out)
        return TYPE_ERR_INVALID_FONT;
    if (cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
        return TYPE_ERR_GLYPH_NOT_FOUND;
    v = f->ttf->cmap;
    fmt = type_at(v, 0, 2);
    if (fmt == 0 && cp < 256)
        g = type_at(v, 6 + cp, 1);
    else if (fmt == 6) {
        uint32_t first = type_at(v, 6, 2), n = type_at(v, 8, 2);
        if (cp >= first && cp - first < n)
            g = type_at(v, 10 + 2 * (cp - first), 2);
    } else if (fmt == 4 && cp <= 65535) {
        uint32_t n = type_at(v, 6, 2) / 2, lo = 0, hi = n;
        while (lo < hi) {
            uint32_t mid = lo + (hi - lo) / 2;
            if (type_at(v, 14 + 2 * mid, 2) < cp)
                lo = mid + 1;
            else
                hi = mid;
        }
        if (lo < n && cp >= type_at(v, 16 + 2 * n + 2 * lo, 2)) {
            uint32_t delta = type_at(v, 16 + 4 * n + 2 * lo, 2),
                     ro = type_at(v, 16 + 6 * n + 2 * lo, 2);
            if (!ro)
                g = (cp + delta) & 65535u;
            else {
                g = type_at(
                    v, 16 + 6 * n + 2 * lo + ro + 2 * (cp - type_at(v, 16 + 2 * n + 2 * lo, 2)), 2);
                if (g)
                    g = (g + delta) & 65535u;
            }
        }
    } else if (fmt == 12 || fmt == 13) {
        uint32_t lo = 0, hi = type_at(v, 12, 4);
        while (lo < hi) {
            uint32_t mid = lo + (hi - lo) / 2, end = type_at(v, 20 + (size_t)mid * 12, 4);
            if (end < cp)
                lo = mid + 1;
            else
                hi = mid;
        }
        if (lo < type_at(v, 12, 4)) {
            uint32_t first = type_at(v, 16 + (size_t)lo * 12, 4);
            if (cp >= first)
                g = type_at(v, 24 + (size_t)lo * 12, 4) + (fmt == 12 ? cp - first : 0);
        }
    }
    if (g >= (uint32_t)f->ttf->num_glyphs)
        return TYPE_ERR_INVALID_FONT;
    if (!g)
        return TYPE_ERR_GLYPH_NOT_FOUND;
    *out = (uint16_t)g;
    return TYPE_OK;
}
TypeError type_font_advance(const TypeFont *f, uint16_t gid, uint16_t *out)
{
    if (!f || !f->ttf || !out)
        return TYPE_ERR_INVALID_FONT;
    if (gid >= f->ttf->num_glyphs)
        return TYPE_ERR_GLYPH_NOT_FOUND;
    *out = (uint16_t)type_at(f->ttf->table[T_HMTX],
                             (size_t)(gid < f->ttf->hmetrics ? gid : f->ttf->hmetrics - 1) * 4, 2);
    return TYPE_OK;
}
TypeError type_font_vertical_advance(const TypeFont *f, uint16_t gid, uint16_t *advance,
                                     int16_t *bearing)
{
    uint16_t n;
    if (!f || !f->ttf || !advance || !bearing)
        return TYPE_ERR_INVALID_FONT;
    if (gid >= f->ttf->num_glyphs)
        return TYPE_ERR_GLYPH_NOT_FOUND;
    n = f->ttf->vmetrics;
    if (!n)
        return TYPE_ERR_UNSUPPORTED_TABLE;
    *advance = (uint16_t)type_at(f->ttf->table[T_VMTX], (size_t)(gid < n ? gid : n - 1) * 4, 2);
    *bearing = (int16_t)type_signed(
        type_at(f->ttf->table[T_VMTX],
                gid < n ? (size_t)gid * 4 + 2 : (size_t)n * 4 + ((size_t)gid - n) * 2, 2),
        16);
    return TYPE_OK;
}
TypeError type_font_name(const TypeFont *f, uint16_t id, const void **bytes, size_t *len,
                         uint16_t *platform)
{
    TypeView v;
    uint32_t n, strings, i;
    if (!f || !f->ttf || !bytes || !len || !platform)
        return TYPE_ERR_INVALID_FONT;
    v = f->ttf->table[T_NAME];
    if (!v.p)
        return TYPE_ERR_UNSUPPORTED_TABLE;
    n = type_at(v, 2, 2);
    strings = type_at(v, 4, 2);
    for (i = 0; i < n; i++) {
        size_t p = 6 + (size_t)i * 12;
        if (type_at(v, p + 6, 2) == id) {
            TypeView s = type_slice(v, strings + type_at(v, p + 10, 2), type_at(v, p + 8, 2));
            *bytes = s.p;
            *len = s.n;
            *platform = (uint16_t)type_at(v, p, 2);
            return TYPE_OK;
        }
    }
    return TYPE_ERR_GLYPH_NOT_FOUND;
}
typedef struct TypePoint {
    double x, y;
    uint8_t on, end;
} TypePoint;
typedef struct TypeWalk {
    TypePoint *points;
    uint32_t n, contours, components, budget;
    uint16_t path[TYPE_MAX_DEPTH];
} TypeWalk;
static TypeView type_glyf(const TypeTTF *f, uint16_t gid)
{
    unsigned w = f->long_loca ? 4 : 2;
    uint32_t a = type_at(f->table[T_LOCA], (size_t)gid * w, w),
             b = type_at(f->table[T_LOCA], ((size_t)gid + 1) * w, w);
    if (w == 2) {
        a *= 2;
        b *= 2;
    }
    return type_slice(f->table[T_GLYF], a, b - a);
}
/* Decode flags twice to locate the independent x/y streams without an array. */
static TypeError type_tt_walk(const TypeTTF *f, uint16_t gid, TypeWalk *w, unsigned depth)
{
    TypeView v;
    TypeCursor c, ends, flags, xc, yc;
    int contours;
    unsigned i;
    uint32_t base = w->n;
    if (gid >= f->num_glyphs)
        return TYPE_ERR_INVALID_FONT;
    if (depth >= TYPE_MAX_DEPTH)
        return TYPE_ERR_LIMIT;
    for (i = 0; i < depth; i++)
        if (w->path[i] == gid)
            return TYPE_ERR_INVALID_FONT;
    w->path[depth] = gid;
    v = type_glyf(f, gid);
    if (!v.n)
        return TYPE_OK;
    if (v.n < 10)
        return TYPE_ERR_INVALID_FONT;
    c = type_cursor(v);
    contours = type_signed(type_read(&c, 2), 16);
    type_skip(&c, 8);
    if (type_signed(type_at(v, 2, 2), 16) > type_signed(type_at(v, 6, 2), 16) ||
        type_signed(type_at(v, 4, 2), 16) > type_signed(type_at(v, 8, 2), 16))
        return TYPE_ERR_INVALID_FONT;
    if (contours >= 0) {
        uint32_t n = 0, previous = 0, xbytes = 0, ybytes = 0, repeat = 0, flag = 0, k, instructions,
                 end = 0;
        int32_t x = 0, y = 0;
        if ((unsigned)contours > TYPE_MAX_CONTOURS - w->contours)
            return TYPE_ERR_LIMIT;
        ends = c;
        for (i = 0; i < (unsigned)contours; i++) {
            uint32_t e = type_read(&c, 2);
            if ((i && e <= previous) || c.bad)
                return TYPE_ERR_INVALID_FONT;
            previous = e;
            n = e + 1;
        }
        if (n > TYPE_MAX_POINTS - w->n)
            return TYPE_ERR_LIMIT;
        instructions = type_read(&c, 2);
        if (c.bad)
            return TYPE_ERR_INVALID_FONT;
        if (instructions > TYPE_MAX_INSTRUCTIONS)
            return TYPE_ERR_LIMIT;
        if (!type_skip(&c, instructions))
            return TYPE_ERR_INVALID_FONT;
        flags = c;
        for (k = 0; k < n; k++) {
            if (!w->budget--)
                return TYPE_ERR_LIMIT;
            if (!repeat) {
                flag = type_read(&c, 1);
                if (flag & 128u)
                    return TYPE_ERR_INVALID_FONT;
                repeat = 1;
                if (flag & 8u)
                    repeat += type_read(&c, 1);
                if (repeat > n - k)
                    return TYPE_ERR_INVALID_FONT;
            }
            repeat--;
            xbytes += (flag & 2u) ? 1 : ((flag & 16u) ? 0 : 2);
            ybytes += (flag & 4u) ? 1 : ((flag & 32u) ? 0 : 2);
        }
        if (c.bad || xbytes > v.n - c.at || ybytes > v.n - c.at - xbytes)
            return TYPE_ERR_INVALID_FONT;
        xc = type_cursor(type_slice(v, c.at, xbytes));
        yc = type_cursor(type_slice(v, c.at + xbytes, ybytes));
        repeat = 0;
        if (contours)
            end = type_read(&ends, 2);
        for (k = 0; k < n; k++) {
            int32_t dx, dy;
            if (!repeat) {
                flag = type_read(&flags, 1);
                repeat = 1;
                if (flag & 8u)
                    repeat += type_read(&flags, 1);
            }
            repeat--;
            dx = (flag & 2u) ? (int32_t)type_read(&xc, 1)
                             : ((flag & 16u) ? 0 : type_signed(type_read(&xc, 2), 16));
            if ((flag & 2u) && !(flag & 16u))
                dx = -dx;
            dy = (flag & 4u) ? (int32_t)type_read(&yc, 1)
                             : ((flag & 32u) ? 0 : type_signed(type_read(&yc, 2), 16));
            if ((flag & 4u) && !(flag & 32u))
                dy = -dy;
            x += dx;
            y += dy;
            if (x < -32768 || x > 32767 || y < -32768 || y > 32767)
                return TYPE_ERR_INVALID_FONT;
            if (w->points) {
                TypePoint *p = &w->points[w->n + k];
                p->x = x;
                p->y = y;
                p->on = (uint8_t)(flag & 1u);
                p->end = (uint8_t)(k == end);
            }
            if (k == end && k + 1 < n)
                end = type_read(&ends, 2);
        }
        if (flags.bad || xc.bad || yc.bad)
            return TYPE_ERR_INVALID_FONT;
        w->n += n;
        w->contours += (uint32_t)contours;
        return TYPE_OK;
    }
    if (contours != -1)
        return TYPE_ERR_INVALID_FONT;
    {
        uint32_t bits = 0;
        int have_instructions = 0;
        do {
            uint32_t child, a, b, childbase = w->n;
            double xx = 1, xy = 0, yx = 0, yy = 1, tx = 0, ty = 0;
            TypeError e;
            if (++w->components > TYPE_MAX_COMPONENTS || !w->budget--)
                return TYPE_ERR_LIMIT;
            bits = type_read(&c, 2);
            child = type_read(&c, 2);
            a = type_read(&c, (bits & 1u) ? 2 : 1);
            b = type_read(&c, (bits & 1u) ? 2 : 1);
            if (c.bad || (bits & 0xe000u) || ((bits & 0x1800u) == 0x1800u))
                return TYPE_ERR_INVALID_FONT;
            if (((bits & 8u) != 0) + ((bits & 64u) != 0) + ((bits & 128u) != 0) > 1)
                return TYPE_ERR_INVALID_FONT;
            if (bits & 8u) {
                xx = yy = type_signed(type_read(&c, 2), 16) / 16384.0;
            } else if (bits & 64u) {
                xx = type_signed(type_read(&c, 2), 16) / 16384.0;
                yy = type_signed(type_read(&c, 2), 16) / 16384.0;
            } else if (bits & 128u) {
                xx = type_signed(type_read(&c, 2), 16) / 16384.0;
                yx = type_signed(type_read(&c, 2), 16) / 16384.0;
                xy = type_signed(type_read(&c, 2), 16) / 16384.0;
                yy = type_signed(type_read(&c, 2), 16) / 16384.0;
            }
            if (c.bad)
                return TYPE_ERR_INVALID_FONT;
            e = type_tt_walk(f, (uint16_t)child, w, depth + 1);
            if (e != TYPE_OK)
                return e;
            if (bits & 2u) {
                tx = type_signed(a, (bits & 1u) ? 16 : 8);
                ty = type_signed(b, (bits & 1u) ? 16 : 8);
                if (bits & 0x800u) {
                    double t = xx * tx + xy * ty;
                    ty = yx * tx + yy * ty;
                    tx = t;
                }
                if (bits & 4u) {
                    tx = floor(tx + 0.5);
                    ty = floor(ty + 0.5);
                }
            } else {
                if (a >= childbase - base || b >= w->n - childbase)
                    return TYPE_ERR_INVALID_FONT;
                if (w->points) {
                    TypePoint p = w->points[base + a], q = w->points[childbase + b];
                    tx = p.x - xx * q.x - xy * q.y;
                    ty = p.y - yx * q.x - yy * q.y;
                }
            }
            if (w->points)
                for (i = childbase; i < w->n; i++) {
                    TypePoint *p = &w->points[i];
                    double x = xx * p->x + xy * p->y + tx, y = yx * p->x + yy * p->y + ty;
                    if (!isfinite(x) || !isfinite(y) || fabs(x) > 1048576 || fabs(y) > 1048576)
                        return TYPE_ERR_LIMIT;
                    p->x = x;
                    p->y = y;
                }
            have_instructions |= (bits & 256u) != 0;
        } while (bits & 32u);
        if (have_instructions) {
            uint32_t n = type_read(&c, 2);
            if (n > TYPE_MAX_INSTRUCTIONS)
                return TYPE_ERR_LIMIT;
            if (!type_skip(&c, n))
                return TYPE_ERR_INVALID_FONT;
        }
    }
    return c.bad ? TYPE_ERR_INVALID_FONT : TYPE_OK;
}
typedef struct TypeOutline {
    TypeVertex *v;
    uint32_t n, cap;
    double box[4];
    int has;
    TypeError err;
} TypeOutline;
static void type_emit(TypeOutline *o, uint8_t op, double x, double y, double cx, double cy,
                      double cx1, double cy1)
{
    unsigned i;
    double p[6];
    p[0] = x;
    p[1] = y;
    p[2] = cx;
    p[3] = cy;
    p[4] = cx1;
    p[5] = cy1;
    if (o->err != TYPE_OK)
        return;
    if (o->n >= TYPE_MAX_VERTICES) {
        o->err = TYPE_ERR_LIMIT;
        return;
    }
    for (i = 0; i < 6; i++)
        if (!isfinite(p[i]) || fabs(p[i]) > 1048576) {
            o->err = TYPE_ERR_LIMIT;
            return;
        }
    /* Control bounds are deliberately conservative for CFF. */
    for (i = 0; i < (op == TYPE_VERTEX_CUBIC ? 6u : (op == TYPE_VERTEX_QUAD ? 4u : 2u)); i += 2) {
        if (!o->has) {
            o->box[0] = o->box[2] = p[i];
            o->box[1] = o->box[3] = p[i + 1];
            o->has = 1;
        } else {
            if (p[i] < o->box[0])
                o->box[0] = p[i];
            if (p[i] > o->box[2])
                o->box[2] = p[i];
            if (p[i + 1] < o->box[1])
                o->box[1] = p[i + 1];
            if (p[i + 1] > o->box[3])
                o->box[3] = p[i + 1];
        }
    }
    if (o->v) {
        TypeVertex *v;
        if (o->n >= o->cap) {
            o->err = TYPE_ERR_BUF_TOO_SMALL;
            return;
        }
        v = &o->v[o->n];
        v->type = op;
        v->x = x;
        v->y = y;
        v->cx = cx;
        v->cy = cy;
        v->cx1 = cx1;
        v->cy1 = cy1;
    }
    o->n++;
}
static void type_tt_vertices(TypeOutline *o, TypePoint *p, uint32_t n)
{
    uint32_t first = 0;
    while (first < n) {
        uint32_t last = first, i, count;
        TypePoint start, prev;
        int pending = 0;
        while (last < n && !p[last].end)
            last++;
        if (last == n) {
            o->err = TYPE_ERR_INVALID_FONT;
            return;
        }
        start = p[first];
        count = last - first + 1;
        if (!start.on) {
            if (p[last].on) {
                start = p[last];
                count--;
                i = first;
            } else {
                start.x = (start.x + p[last].x) * 0.5;
                start.y = (start.y + p[last].y) * 0.5;
                start.on = 1;
                i = first;
            }
        } else {
            i = first + 1;
            count--;
        }
        type_emit(o, TYPE_VERTEX_MOVE, start.x, start.y, 0, 0, 0, 0);
        prev = start;
        while (count--) {
            TypePoint q = p[i++];
            if (q.on) {
                if (pending)
                    type_emit(o, TYPE_VERTEX_QUAD, q.x, q.y, prev.x, prev.y, 0, 0);
                else
                    type_emit(o, TYPE_VERTEX_LINE, q.x, q.y, 0, 0, 0, 0);
                pending = 0;
            } else {
                if (pending)
                    type_emit(o, TYPE_VERTEX_QUAD, (prev.x + q.x) * 0.5, (prev.y + q.y) * 0.5,
                              prev.x, prev.y, 0, 0);
                pending = 1;
            }
            prev = q;
        }
        if (pending)
            type_emit(o, TYPE_VERTEX_QUAD, start.x, start.y, prev.x, prev.y, 0, 0);
        else
            type_emit(o, TYPE_VERTEX_LINE, start.x, start.y, 0, 0, 0, 0);
        first = last + 1;
    }
}
/* Type 2 interpreter: shared stack, shared byte/operation budget, bounded
 * calls. */
typedef struct TypeCS {
    const TypeTTF *f;
    TypeOutline *o;
    TypeIndex local;
    double s[TYPE_MAX_CFF_STACK], transient[32], x, y, startx, starty;
    uint32_t n, stems, budget, vs, components;
    double originx, originy, matrix[6];
    int width, open, ended;
} TypeCS;
static void type_cs_emit(TypeCS *c, uint8_t op, double x, double y, double cx, double cy,
                         double cx1, double cy1)
{
    const double *m = c->matrix;
    x += c->originx;
    y += c->originy;
    cx += c->originx;
    cy += c->originy;
    cx1 += c->originx;
    cy1 += c->originy;
    type_emit(c->o, op, m[0] * x + m[2] * y + m[4], m[1] * x + m[3] * y + m[5],
              m[0] * cx + m[2] * cy + m[4], m[1] * cx + m[3] * cy + m[5],
              m[0] * cx1 + m[2] * cy1 + m[4], m[1] * cx1 + m[3] * cy1 + m[5]);
}
static void type_cs_close(TypeCS *c)
{
    if (c->open) {
        type_cs_emit(c, TYPE_VERTEX_LINE, c->startx, c->starty, 0, 0, 0, 0);
        c->open = 0;
    }
}
static void type_cs_line(TypeCS *c, double dx, double dy)
{
    c->x += dx;
    c->y += dy;
    type_cs_emit(c, TYPE_VERTEX_LINE, c->x, c->y, 0, 0, 0, 0);
}
static void type_cs_curve(TypeCS *c, double dx1, double dy1, double dx2, double dy2, double dx3,
                          double dy3)
{
    double a = c->x + dx1, b = c->y + dy1, d = a + dx2, e = b + dy2;
    c->x = d + dx3;
    c->y = e + dy3;
    type_cs_emit(c, TYPE_VERTEX_CUBIC, c->x, c->y, a, b, d, e);
}
static int type_cs_int(double v, int32_t *out)
{
    if (!isfinite(v) || v < INT32_MIN || v > INT32_MAX || floor(v) != v)
        return 0;
    *out = (int32_t)v;
    return 1;
}
static TypeError type_cs_blend(TypeCS *c)
{
    TypeView v = c->f->vstore;
    TypeCursor t, r;
    uint32_t data, region_at, regions, axes, i, j;
    int32_t count;
    double weights[256];
    if (!c->n || !type_cs_int(c->s[c->n - 1], &count) || count < 1 || !v.p ||
        c->vs >= type_at(v, 6, 2))
        return TYPE_ERR_INVALID_FONT;
    data = type_at(v, 8 + (size_t)c->vs * 4, 4);
    region_at = type_at(v, 2, 4);
    t = type_cursor(type_slice(v, data, v.n - data));
    type_read(&t, 2);
    type_read(&t, 2);
    regions = type_read(&t, 2);
    r = type_cursor(type_slice(v, region_at, v.n - region_at));
    axes = type_read(&r, 2);
    if (regions > 256 || (uint32_t)count > (c->n - 1) / (regions + 1))
        return TYPE_ERR_INVALID_FONT;
    for (i = 0; i < regions; i++) {
        uint32_t index = type_read(&t, 2);
        double weight = 1;
        if (index >= type_at(r.v, 2, 2))
            return TYPE_ERR_INVALID_FONT;
        for (j = 0; j < axes; j++) {
            if (!c->budget--)
                return TYPE_ERR_LIMIT;
            size_t at = 4 + ((size_t)index * axes + j) * 6;
            double a = type_signed(type_at(r.v, at, 2), 16) / 16384.0,
                   b = type_signed(type_at(r.v, at + 2, 2), 16) / 16384.0,
                   d = type_signed(type_at(r.v, at + 4, 2), 16) / 16384.0;
            if (a > b || b > d || a < -1 || d > 1)
                return TYPE_ERR_INVALID_FONT;
            if (b == 0 || (a < 0 && d > 0))
                continue;
            if (0 < a || 0 > d)
                weight = 0;
            else if (0 < b)
                weight *= (-a) / (b - a);
            else
                weight *= d / (d - b);
        }
        weights[i] = weight;
    }
    {
        uint32_t base = c->n - 1 - (uint32_t)count * (regions + 1), k;
        for (k = 0; k < (uint32_t)count; k++)
            for (i = 0; i < regions; i++) {
                if (!c->budget--)
                    return TYPE_ERR_LIMIT;
                c->s[base + k] += c->s[base + (uint32_t)count + k * regions + i] * weights[i];
            }
        c->n = base + (uint32_t)count;
    }
    return t.bad ? TYPE_ERR_INVALID_FONT : TYPE_OK;
}
/* CFF standardized encoding and predefined charset numeric mappings. */
static const uint16_t type_standard_sids[] = {
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   2,   3,   4,   5,   6,
    7,   8,   9,   10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,
    26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,
    45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,
    64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,  80,  81,  82,
    83,  84,  85,  86,  87,  88,  89,  90,  91,  92,  93,  94,  95,  0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   96,  97,  98,  99,  100, 101, 102, 103, 104, 105,
    106, 107, 108, 109, 110, 0,   111, 112, 113, 114, 0,   115, 116, 117, 118, 119, 120, 121, 122,
    0,   123, 0,   124, 125, 126, 127, 128, 129, 130, 131, 0,   132, 133, 0,   134, 135, 136, 137,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   138, 0,   139,
    0,   0,   0,   0,   140, 141, 142, 143, 0,   0,   0,   0,   0,   144, 0,   0,   0,   145, 0,
    0,   146, 147, 148, 149, 0,   0,   0,   0};
static const uint16_t type_expert_sids[] = {
    0,   1,   229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 13,  14,  15,  99,  239, 240, 241,
    242, 243, 244, 245, 246, 247, 248, 27,  28,  249, 250, 251, 252, 253, 254, 255, 256, 257, 258,
    259, 260, 261, 262, 263, 264, 265, 266, 109, 110, 267, 268, 269, 270, 271, 272, 273, 274, 275,
    276, 277, 278, 279, 280, 281, 282, 283, 284, 285, 286, 287, 288, 289, 290, 291, 292, 293, 294,
    295, 296, 297, 298, 299, 300, 301, 302, 303, 304, 305, 306, 307, 308, 309, 310, 311, 312, 313,
    314, 315, 316, 317, 318, 158, 155, 163, 319, 320, 321, 322, 323, 324, 325, 326, 150, 164, 169,
    327, 328, 329, 330, 331, 332, 333, 334, 335, 336, 337, 338, 339, 340, 341, 342, 343, 344, 345,
    346, 347, 348, 349, 350, 351, 352, 353, 354, 355, 356, 357, 358, 359, 360, 361, 362, 363, 364,
    365, 366, 367, 368, 369, 370, 371, 372, 373, 374, 375, 376, 377, 378};
static const uint16_t type_expert_subset_sids[] = {
    0,   1,   231, 232, 235, 236, 237, 238, 13,  14,  15,  99,  239, 240, 241, 242, 243, 244,
    245, 246, 247, 248, 27,  28,  249, 250, 251, 253, 254, 255, 256, 257, 258, 259, 260, 261,
    262, 263, 264, 265, 266, 109, 110, 267, 268, 269, 270, 272, 300, 301, 302, 305, 314, 315,
    158, 155, 163, 320, 321, 322, 323, 324, 325, 326, 150, 164, 169, 327, 328, 329, 330, 331,
    332, 333, 334, 335, 336, 337, 338, 339, 340, 341, 342, 343, 344, 345, 346};
static TypeError type_cs_glyph(TypeCS *s, uint16_t gid, unsigned depth);
static TypeError type_cs_lookup(TypeCS *s, int32_t code, uint16_t *gid)
{
    const TypeTTF *f = s->f;
    uint32_t sid, i;
    if (code < 0 || code > 255 || f->fds.count)
        return TYPE_ERR_INVALID_FONT;
    sid = type_standard_sids[code];
    if (!sid)
        return TYPE_ERR_INVALID_FONT;
    if (!f->charset) {
        if (sid >= (uint32_t)f->num_glyphs || sid >= 229)
            return TYPE_ERR_INVALID_FONT;
        *gid = (uint16_t)sid;
        return TYPE_OK;
    }
    if (f->charset <= 2) {
        const uint16_t *map = f->charset == 1 ? type_expert_sids : type_expert_subset_sids;
        size_t count = f->charset == 1 ? sizeof(type_expert_sids) / sizeof(*map)
                                       : sizeof(type_expert_subset_sids) / sizeof(*map);
        for (i = 1; i < count && i < (uint32_t)f->num_glyphs; i++) {
            if (!s->budget--)
                return TYPE_ERR_LIMIT;
            if (map[i] == sid) {
                *gid = (uint16_t)i;
                return TYPE_OK;
            }
        }
    } else {
        TypeView v = f->table[T_CFF];
        TypeCursor c = type_cursor(type_slice(v, f->charset, v.n - f->charset));
        uint32_t fmt = type_read(&c, 1), base = 1;
        while (base < (uint32_t)f->num_glyphs && !c.bad) {
            uint32_t first = type_read(&c, 2), run = fmt == 0 ? 0 : type_read(&c, fmt == 1 ? 1 : 2);
            if (!s->budget--)
                return TYPE_ERR_LIMIT;
            if (sid >= first && sid - first <= run &&
                base + sid - first < (uint32_t)f->num_glyphs) {
                *gid = (uint16_t)(base + sid - first);
                return TYPE_OK;
            }
            base += run + 1;
        }
    }
    return TYPE_ERR_INVALID_FONT;
}
static TypeError type_cs_run(TypeCS *s, TypeView v, unsigned depth, int sub)
{
    TypeCursor c = type_cursor(v);
    if (depth >= TYPE_MAX_CFF_DEPTH)
        return TYPE_ERR_LIMIT;
    while (c.at < c.v.n && !c.bad) {
        uint32_t b = type_read(&c, 1), i = 0, n = s->n;
        double *a = s->s;
        TypeError err;
        if (!s->budget--)
            return TYPE_ERR_LIMIT;
        if (b >= 32 || b == 28) {
            double x;
            if (s->n >= TYPE_MAX_CFF_STACK)
                return TYPE_ERR_LIMIT;
            if (!type_number(&c, b, &x, 0))
                return TYPE_ERR_INVALID_FONT;
            s->s[s->n++] = x;
            continue;
        }
        if (b == 10 || b == 29) {
            TypeIndex idx = b == 10 ? s->local : s->f->gsubrs;
            int32_t k;
            uint32_t bias = idx.count < 1240 ? 107 : (idx.count < 33900 ? 1131 : 32768);
            if (!n || !type_cs_int(a[n - 1], &k))
                return TYPE_ERR_INVALID_FONT;
            s->n--;
            if (k < -(int32_t)bias || (int64_t)k + bias >= idx.count)
                return TYPE_ERR_INVALID_FONT;
            err = type_cs_run(s, type_index_item(idx, (uint32_t)((int64_t)k + bias)), depth + 1, 1);
            if (err != TYPE_OK)
                return err;
            if (s->ended)
                return TYPE_OK;
            continue;
        }
        if (b == 11)
            return sub ? TYPE_OK : TYPE_ERR_INVALID_FONT;
        if (b == 15 && s->f->cff2) {
            int32_t k;
            if (n != 1 || !type_cs_int(a[0], &k) || k < 0 || !s->f->vstore.p ||
                (uint32_t)k >= type_at(s->f->vstore, 6, 2))
                return TYPE_ERR_INVALID_FONT;
            s->vs = (uint32_t)k;
            s->n = 0;
            continue;
        }
        if (b == 16 && s->f->cff2) {
            err = type_cs_blend(s);
            if (err != TYPE_OK)
                return err;
            continue;
        }
        if (b == 1 || b == 3 || b == 18 || b == 23 || b == 19 || b == 20) {
            if (!s->width && !s->f->cff2 && (n & 1u)) {
                memmove(a, a + 1, (n - 1) * sizeof *a);
                n--;
            }
            s->width = 1;
            if (n & 1u)
                return TYPE_ERR_INVALID_FONT;
            s->stems += n / 2;
            if (s->stems > 96)
                return TYPE_ERR_LIMIT;
            s->n = 0;
            if (b == 19 || b == 20)
                if (!type_skip(&c, (s->stems + 7) / 8))
                    return TYPE_ERR_INVALID_FONT;
            continue;
        }
        if (b == 4 || b == 21 || b == 22) {
            unsigned need = b == 21 ? 2 : 1;
            if (!s->width && !s->f->cff2 && n == need + 1) {
                memmove(a, a + 1, need * sizeof *a);
                n--;
            }
            s->width = 1;
            if (n != need)
                return TYPE_ERR_INVALID_FONT;
            type_cs_close(s);
            s->x += b == 4 ? 0 : a[0];
            s->y += b == 22 ? 0 : a[need - 1];
            s->startx = s->x;
            s->starty = s->y;
            s->open = 1;
            type_cs_emit(s, TYPE_VERTEX_MOVE, s->x, s->y, 0, 0, 0, 0);
        } else if (b == 14) {
            if (s->f->cff2)
                return TYPE_ERR_INVALID_FONT;
            if (!s->width && n == 1)
                n = 0;
            if (n == 4 || (!s->width && n == 5)) {
                int32_t bcode, acode;
                uint16_t base, accent;
                double adx = a[n - 4], ady = a[n - 3], ox = s->originx, oy = s->originy;
                if (!type_cs_int(a[n - 2], &bcode) || !type_cs_int(a[n - 1], &acode))
                    return TYPE_ERR_INVALID_FONT;
                err = type_cs_lookup(s, bcode, &base);
                if (err != TYPE_OK)
                    return err;
                err = type_cs_lookup(s, acode, &accent);
                if (err != TYPE_OK)
                    return err;
                if (s->components > TYPE_MAX_COMPONENTS - 2)
                    return TYPE_ERR_LIMIT;
                s->components += 2;
                type_cs_close(s);
                err = type_cs_glyph(s, base, depth + 1);
                if (err != TYPE_OK)
                    return err;
                s->originx = ox + adx;
                s->originy = oy + ady;
                err = type_cs_glyph(s, accent, depth + 1);
                s->originx = ox;
                s->originy = oy;
                s->ended = 1;
                return err;
            }
            if (n)
                return TYPE_ERR_INVALID_FONT;
            type_cs_close(s);
            s->n = 0;
            s->ended = 1;
            return s->o->err;
        } else if (b == 12) {
            uint32_t op = type_read(&c, 1);
            int32_t k, j;
            double x, y;
            if (op == 34 || op == 35 || op == 36 || op == 37) {
                if (!s->open)
                    return TYPE_ERR_INVALID_FONT;
                if (op == 34 && n == 7) {
                    type_cs_curve(s, a[0], 0, a[1], a[2], a[3], 0);
                    type_cs_curve(s, a[4], 0, a[5], -a[2], a[6], 0);
                } else if (op == 35 && n == 13) {
                    type_cs_curve(s, a[0], a[1], a[2], a[3], a[4], a[5]);
                    type_cs_curve(s, a[6], a[7], a[8], a[9], a[10], a[11]);
                } else if (op == 36 && n == 9) {
                    type_cs_curve(s, a[0], a[1], a[2], a[3], a[4], 0);
                    type_cs_curve(s, a[5], 0, a[6], a[7], a[8], -a[1] - a[3] - a[7]);
                } else if (op == 37 && n == 11) {
                    x = a[0] + a[2] + a[4] + a[6] + a[8];
                    y = a[1] + a[3] + a[5] + a[7] + a[9];
                    type_cs_curve(s, a[0], a[1], a[2], a[3], a[4], a[5]);
                    type_cs_curve(s, a[6], a[7], a[8], a[9], fabs(x) > fabs(y) ? a[10] : -x,
                                  fabs(x) > fabs(y) ? -y : a[10]);
                } else
                    return TYPE_ERR_INVALID_FONT;
                s->n = 0;
                continue;
            }
            if (op == 3 || op == 4 || op == 10 || op == 11 || op == 12 || op == 15 || op == 24) {
                if (n < 2)
                    return TYPE_ERR_INVALID_FONT;
                x = a[n - 2];
                y = a[n - 1];
                s->n--;
                if (op == 3)
                    a[n - 2] = (x != 0 && y != 0);
                else if (op == 4)
                    a[n - 2] = (x != 0 || y != 0);
                else if (op == 10)
                    a[n - 2] = x + y;
                else if (op == 11)
                    a[n - 2] = x - y;
                else if (op == 12) {
                    if (y == 0)
                        return TYPE_ERR_INVALID_FONT;
                    a[n - 2] = x / y;
                } else if (op == 15)
                    a[n - 2] = (x == y);
                else
                    a[n - 2] = x * y;
            } else if (op == 5 || op == 9 || op == 14 || op == 26) {
                if (!n)
                    return TYPE_ERR_INVALID_FONT;
                x = a[n - 1];
                if (op == 5)
                    a[n - 1] = (x == 0);
                else if (op == 9)
                    a[n - 1] = fabs(x);
                else if (op == 14)
                    a[n - 1] = -x;
                else {
                    if (x < 0)
                        return TYPE_ERR_INVALID_FONT;
                    a[n - 1] = sqrt(x);
                }
            } else if (op == 18) {
                if (!n)
                    return TYPE_ERR_INVALID_FONT;
                s->n--;
            } else if (op == 20) {
                if (n < 2 || !type_cs_int(a[n - 1], &k) || k < 0 || k >= 32)
                    return TYPE_ERR_INVALID_FONT;
                s->transient[k] = a[n - 2];
                s->n -= 2;
            } else if (op == 21) {
                if (!n || !type_cs_int(a[n - 1], &k) || k < 0 || k >= 32)
                    return TYPE_ERR_INVALID_FONT;
                a[n - 1] = s->transient[k];
            } else if (op == 22) {
                if (n < 4)
                    return TYPE_ERR_INVALID_FONT;
                a[n - 4] = a[n - 2] <= a[n - 1] ? a[n - 4] : a[n - 3];
                s->n -= 3;
            } else if (op == 23) {
                if (n == TYPE_MAX_CFF_STACK)
                    return TYPE_ERR_LIMIT;
                a[s->n++] = 0.5;
            } else if (op == 27) {
                if (!n || n == TYPE_MAX_CFF_STACK)
                    return TYPE_ERR_INVALID_FONT;
                a[s->n++] = a[n - 1];
            } else if (op == 28) {
                if (n < 2)
                    return TYPE_ERR_INVALID_FONT;
                x = a[n - 1];
                a[n - 1] = a[n - 2];
                a[n - 2] = x;
            } else if (op == 29) {
                if (n < 2 || !type_cs_int(a[n - 1], &k))
                    return TYPE_ERR_INVALID_FONT;
                if (k < 0)
                    k = 0;
                if ((uint32_t)k >= n - 1)
                    k = (int32_t)n - 2;
                a[n - 1] = a[n - 2 - (uint32_t)k];
            } else if (op == 30) {
                if (n < 2 || !type_cs_int(a[n - 2], &k) || !type_cs_int(a[n - 1], &j) || k < 0 ||
                    (uint32_t)k > n - 2)
                    return TYPE_ERR_INVALID_FONT;
                s->n -= 2;
                if (k) {
                    j %= k;
                    if (j < 0)
                        j += k;
                    while (j--) {
                        x = a[s->n - 1];
                        for (i = s->n - 1; i > s->n - (uint32_t)k; i--) {
                            if (!s->budget--)
                                return TYPE_ERR_LIMIT;
                            a[i] = a[i - 1];
                        }
                        a[i] = x;
                    }
                }
            } else
                return TYPE_ERR_UNSUPPORTED_TABLE;
            for (i = 0; i < s->n; i++)
                if (!isfinite(a[i]) || fabs(a[i]) > 2147483648.0)
                    return TYPE_ERR_LIMIT;
            continue;
        } else {
            if (!s->open)
                return TYPE_ERR_INVALID_FONT;
            if (b == 5) {
                if (!n || (n & 1u))
                    return TYPE_ERR_INVALID_FONT;
                for (i = 0; i < n; i += 2)
                    type_cs_line(s, a[i], a[i + 1]);
            } else if (b == 6 || b == 7) {
                int horizontal = b == 6;
                if (!n)
                    return TYPE_ERR_INVALID_FONT;
                for (i = 0; i < n; i++) {
                    type_cs_line(s, horizontal ? a[i] : 0, horizontal ? 0 : a[i]);
                    horizontal = !horizontal;
                }
            } else if (b == 8) {
                if (!n || n % 6)
                    return TYPE_ERR_INVALID_FONT;
                for (i = 0; i < n; i += 6)
                    type_cs_curve(s, a[i], a[i + 1], a[i + 2], a[i + 3], a[i + 4], a[i + 5]);
            } else if (b == 24) {
                if (n < 8 || (n - 2) % 6)
                    return TYPE_ERR_INVALID_FONT;
                for (i = 0; i < n - 2; i += 6)
                    type_cs_curve(s, a[i], a[i + 1], a[i + 2], a[i + 3], a[i + 4], a[i + 5]);
                type_cs_line(s, a[n - 2], a[n - 1]);
            } else if (b == 25) {
                if (n < 8 || (n - 6) % 2)
                    return TYPE_ERR_INVALID_FONT;
                for (i = 0; i < n - 6; i += 2)
                    type_cs_line(s, a[i], a[i + 1]);
                type_cs_curve(s, a[i], a[i + 1], a[i + 2], a[i + 3], a[i + 4], a[i + 5]);
            } else if (b == 26 || b == 27) {
                double extra = 0;
                int vertical = b == 26;
                if (n & 1u)
                    extra = a[i++];
                if (n - i < 4 || (n - i) % 4)
                    return TYPE_ERR_INVALID_FONT;
                for (; i < n; i += 4) {
                    type_cs_curve(s, vertical ? extra : a[i], vertical ? a[i] : extra, a[i + 1],
                                  a[i + 2], vertical ? 0 : a[i + 3], vertical ? a[i + 3] : 0);
                    extra = 0;
                }
            } else if (b == 30 || b == 31) {
                int horizontal = b == 31;
                if (n < 4 || (n % 4 != 0 && n % 4 != 1))
                    return TYPE_ERR_INVALID_FONT;
                while (i + 4 <= n) {
                    double dx1 = horizontal ? a[i] : 0, dy1 = horizontal ? 0 : a[i], dx2 = a[i + 1],
                           dy2 = a[i + 2], dx3 = horizontal ? 0 : a[i + 3],
                           dy3 = horizontal ? a[i + 3] : 0;
                    i += 4;
                    if (n - i == 1) {
                        if (horizontal)
                            dx3 = a[i++];
                        else
                            dy3 = a[i++];
                    }
                    type_cs_curve(s, dx1, dy1, dx2, dy2, dx3, dy3);
                    horizontal = !horizontal;
                }
            } else
                return TYPE_ERR_UNSUPPORTED_TABLE;
        }
        s->n = 0;
        if (s->o->err != TYPE_OK)
            return s->o->err;
    }
    if (c.bad)
        return TYPE_ERR_INVALID_FONT;
    if (sub)
        return s->f->cff2 ? TYPE_OK : TYPE_ERR_INVALID_FONT;
    if (s->f->cff2) {
        if (s->n)
            return TYPE_ERR_INVALID_FONT;
        type_cs_close(s);
        return s->o->err;
    }
    return TYPE_ERR_INVALID_FONT;
}
static TypeError type_cs_glyph(TypeCS *s, uint16_t gid, unsigned depth)
{
    const TypeTTF *f = s->f;
    unsigned i;
    if (gid >= f->num_glyphs || depth >= TYPE_MAX_CFF_DEPTH)
        return TYPE_ERR_LIMIT;
    s->local = f->subrs;
    s->n = s->stems = 0;
    s->vs = f->default_vs;
    s->width = s->open = s->ended = 0;
    s->x = s->y = 0;
    memset(s->transient, 0, sizeof s->transient);
    memcpy(s->matrix, f->matrix, sizeof s->matrix);
    if (f->fds.count) {
        uint32_t fd = 0;
        TypeDict d;
        if (f->fdselect.p) {
            TypeCursor c = type_cursor(f->fdselect);
            unsigned fmt = type_read(&c, 1);
            if (fmt == 0) {
                c.at = 1 + gid;
                fd = type_read(&c, 1);
            } else {
                unsigned w = fmt == 3 ? 2 : 4, fw = fmt == 3 ? 1 : 2;
                uint32_t n = type_read(&c, w), j;
                for (j = 0; j < n; j++) {
                    uint32_t first = type_read(&c, w), k = type_read(&c, fw);
                    if (first > gid)
                        break;
                    fd = k;
                }
            }
        }
        if (!type_dict(type_index_item(f->fds, fd), &d) || !type_private(f, d, &s->local, &s->vs))
            return TYPE_ERR_INVALID_FONT;
        if (d.has_matrix) {
            const double *a = f->matrix, *b = d.matrix;
            s->matrix[0] = a[0] * b[0] + a[2] * b[1];
            s->matrix[1] = a[1] * b[0] + a[3] * b[1];
            s->matrix[2] = a[0] * b[2] + a[2] * b[3];
            s->matrix[3] = a[1] * b[2] + a[3] * b[3];
            s->matrix[4] = a[0] * b[4] + a[2] * b[5] + a[4];
            s->matrix[5] = a[1] * b[4] + a[3] * b[5] + a[5];
        }
    }
    for (i = 0; i < 6; i++)
        if (!isfinite(s->matrix[i]))
            return TYPE_ERR_LIMIT;
    return type_cs_run(s, type_index_item(f->chars, gid), depth, 0);
}
static TypeError type_cff_outline(const TypeTTF *f, uint16_t gid, TypeOutline *o)
{
    TypeCS s;
    memset(&s, 0, sizeof s);
    s.f = f;
    s.o = o;
    s.budget = TYPE_MAX_OPERATIONS;
    return type_cs_glyph(&s, gid, 0);
}
TypeError type_font_outline(const TypeFont *font, uint16_t gid, TypeScratch *scratch,
                            TypeVertex *vertices, uint32_t capacity, uint32_t *count)
{
    TypeOutline o;
    TypeError e;
    size_t mark = 0;
    if (count)
        *count = 0;
    if (!font || !font->ttf || !count)
        return TYPE_ERR_INVALID_FONT;
    if (gid >= font->ttf->num_glyphs)
        return TYPE_ERR_GLYPH_NOT_FOUND;
    memset(&o, 0, sizeof o);
    o.v = vertices;
    o.cap = capacity;
    if (font->ttf->table[T_CFF].p)
        e = type_cff_outline(font->ttf, gid, &o);
    else {
        TypeWalk w;
        memset(&w, 0, sizeof w);
        w.budget = TYPE_MAX_OPERATIONS;
        e = type_tt_walk(font->ttf, gid, &w, 0);
        if (e != TYPE_OK)
            return e;
        if (!w.n) {
            *count = 0;
            return TYPE_OK;
        }
        if (!scratch)
            return TYPE_ERR_SCRATCH_REQUIRED;
        mark = scratch->used;
        w.points = ttf_scratch_alloc((size_t)w.n * sizeof *w.points, scratch);
        if (!w.points)
            return TYPE_ERR_SCRATCH_FULL;
        w.n = w.contours = w.components = 0;
        w.budget = TYPE_MAX_OPERATIONS;
        e = type_tt_walk(font->ttf, gid, &w, 0);
        if (e == TYPE_OK)
            type_tt_vertices(&o, w.points, w.n);
        scratch->used = mark;
    }
    if (e != TYPE_OK)
        return e;
    if (o.err != TYPE_OK)
        return o.err;
    *count = o.n;
    return TYPE_OK;
}
static TypeError type_glyph_box(const TypeTTF *f, uint16_t gid, double box[4])
{
    if (gid >= f->num_glyphs)
        return TYPE_ERR_GLYPH_NOT_FOUND;
    if (f->table[T_CFF].p) {
        TypeOutline o;
        TypeError e;
        memset(&o, 0, sizeof o);
        e = type_cff_outline(f, gid, &o);
        if (e != TYPE_OK)
            return e;
        memcpy(box, o.box, sizeof o.box);
    } else {
        TypeView v = type_glyf(f, gid);
        TypeWalk w;
        TypeError e;
        unsigned i;
        memset(&w, 0, sizeof w);
        w.budget = TYPE_MAX_OPERATIONS;
        e = type_tt_walk(f, gid, &w, 0);
        if (e != TYPE_OK)
            return e;
        for (i = 0; i < 4; i++)
            box[i] = w.n ? type_signed(type_at(v, 2 + i * 2, 2), 16) : 0;
    }
    return TYPE_OK;
}
int type_font_has_drawable(const TypeFont *f, uint16_t gid)
{
    double b[4];
    return f && f->ttf && type_glyph_box(f->ttf, gid, b) == TYPE_OK && b[0] < b[2] && b[1] < b[3];
}
static int16_t ttf_clamp_i16(int v)
{
    return (int16_t)(v > 32767 ? 32767 : (v < -32768 ? -32768 : v));
}
TypeError type_font_bitmap_info(const TypeFont *f, uint16_t gid, float px, TypeBitmap *out)
{
    double b[4];
    volatile float scale, left, top, right, bottom;
    double x0, y0, x1, y1, adv;
    uint16_t fu;
    TypeError e;
    if (!f || !f->ttf || !out || !(px > 0 && px <= 65535))
        return TYPE_ERR_INVALID_FONT;
    e = type_glyph_box(f->ttf, gid, b);
    if (e != TYPE_OK)
        return e;
    e = type_font_advance(f, gid, &fu);
    if (e != TYPE_OK)
        return e;
    memset(out, 0, sizeof *out);
    scale = px / f->ttf->units_per_em;
    adv = fu * scale;
    out->advance = (uint16_t)(adv >= 65535 ? 65535 : adv + 0.5);
    /* Float products preserve existing bitmap origins for integer glyf boxes. */
    left = (float)b[0] * scale;
    top = -(float)b[3] * scale;
    right = (float)b[2] * scale;
    bottom = -(float)b[1] * scale;
    x0 = floor(left);
    y0 = floor(top);
    x1 = ceil(right);
    y1 = ceil(bottom);
    if (x1 <= x0 || y1 <= y0)
        return TYPE_OK;
    if (x1 - x0 > 65535 || y1 - y0 > 65535 || fabs(x0) > 32767 || fabs(y0) > 32767)
        return TYPE_ERR_LIMIT;
    out->width = (uint16_t)(x1 - x0);
    out->height = (uint16_t)(y1 - y0);
    out->bearing_x = ttf_clamp_i16((int)x0);
    out->bearing_y = ttf_clamp_i16((int)-y0);
    return TYPE_OK;
}
size_t type_bitmap_bytes(const TypeBitmap *b)
{
    size_t n;
    if (!b)
        return 0;
    n = (size_t)b->width * b->height;
    if (b->color) {
        if (n > SIZE_MAX / 4)
            return 0;
        n *= 4;
    }
    return n;
}
/* Coverage integrates nonzero-fill spans at 32 fixed y samples. Coordinates
 * are rounded once to 16.16; subdivision, intersections and coverage are
 * integer. Volatile stores force binary64 rounding at the floating/fixed
 * boundary, even on targets with excess precision. Compile with
 * -ffp-contract=off. */
typedef struct TypeEdge {
    int32_t x0, y0, x1, y1;
} TypeEdge;
typedef struct TypeCross {
    int32_t x, dir;
} TypeCross;
typedef struct TypeEdges {
    TypeEdge *v;
    uint32_t n, cap;
    TypeError err;
} TypeEdges;
static int32_t type_fixed(TypeEdges *e, double x, double scale, int offset)
{
    volatile double scaled = x * scale;
    volatile double shifted = scaled + offset;
    volatile double rounded;
    if (!isfinite(shifted) || fabs(shifted) > 16383) {
        e->err = TYPE_ERR_LIMIT;
        return 0;
    }
    rounded = floor(shifted * 65536.0 + 0.5);
    return (int32_t)rounded;
}
static int32_t type_mid(int32_t a, int32_t b)
{
    return (int32_t)(((int64_t)a + b) / 2);
}
static void type_edge(TypeEdges *e, int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    TypeEdge r;
    if (e->err != TYPE_OK || y0 == y1)
        return;
    r.x0 = x0;
    r.y0 = y0;
    r.x1 = x1;
    r.y1 = y1;
    if (e->n >= TYPE_MAX_EDGES || (e->v && e->n >= e->cap)) {
        e->err = TYPE_ERR_LIMIT;
        return;
    }
    if (e->v)
        e->v[e->n] = r;
    e->n++;
}
static void type_flatten(TypeEdges *e, int32_t x0, int32_t y0, int32_t a, int32_t b, int32_t c,
                         int32_t d, int32_t x1, int32_t y1, int cubic, unsigned depth)
{
    int64_t dx = a - (x0 + ((int64_t)x1 - x0) / (cubic ? 3 : 2)),
            dy = b - (y0 + ((int64_t)y1 - y0) / (cubic ? 3 : 2)), ex = 0, ey = 0;
    int flat;
    if (e->err != TYPE_OK)
        return;
    if (cubic) {
        ex = c - (x0 + 2 * ((int64_t)x1 - x0) / 3);
        ey = d - (y0 + 2 * ((int64_t)y1 - y0) / 3);
    }
    /* Check magnitudes before squaring: hostile coordinates cannot overflow. */
    flat = dx >= -1024 && dx <= 1024 && dy >= -1024 && dy <= 1024 && ex >= -1024 && ex <= 1024 &&
           ey >= -1024 && ey <= 1024;
    if (flat && dx * dx + dy * dy + ex * ex + ey * ey <= 1048576) {
        type_edge(e, x0, y0, x1, y1);
        return;
    }
    if (depth == 12) {
        e->err = TYPE_ERR_LIMIT;
        return;
    }
    if (cubic) {
        int32_t xa = type_mid(x0, a), ya = type_mid(y0, b), ab = type_mid(a, c),
                bd = type_mid(b, d), cx = type_mid(c, x1), cy = type_mid(d, y1),
                lx = type_mid(xa, ab), ly = type_mid(ya, bd), rx = type_mid(ab, cx),
                ry = type_mid(bd, cy), mx = type_mid(lx, rx), my = type_mid(ly, ry);
        type_flatten(e, x0, y0, xa, ya, lx, ly, mx, my, 1, depth + 1);
        type_flatten(e, mx, my, rx, ry, cx, cy, x1, y1, 1, depth + 1);
    } else {
        int32_t xa = type_mid(x0, a), ya = type_mid(y0, b), bx = type_mid(a, x1),
                by = type_mid(b, y1), mx = type_mid(xa, bx), my = type_mid(ya, by);
        type_flatten(e, x0, y0, xa, ya, 0, 0, mx, my, 0, depth + 1);
        type_flatten(e, mx, my, bx, by, 0, 0, x1, y1, 0, depth + 1);
    }
}
static void type_make_edges(TypeEdges *e, const TypeVertex *v, uint32_t n, double scale, int bx,
                            int by)
{
    uint32_t i;
    int32_t x = 0, y = 0;
    for (i = 0; i < n && e->err == TYPE_OK; i++) {
        int32_t nx = type_fixed(e, v[i].x, scale, -bx), ny = type_fixed(e, v[i].y, -scale, by);
        if (v[i].type == TYPE_VERTEX_LINE)
            type_edge(e, x, y, nx, ny);
        else if (v[i].type == TYPE_VERTEX_QUAD || v[i].type == TYPE_VERTEX_CUBIC) {
            int32_t a = type_fixed(e, v[i].cx, scale, -bx), b = type_fixed(e, v[i].cy, -scale, by),
                    c = 0, d = 0;
            if (v[i].type == TYPE_VERTEX_CUBIC) {
                c = type_fixed(e, v[i].cx1, scale, -bx);
                d = type_fixed(e, v[i].cy1, -scale, by);
            }
            type_flatten(e, x, y, a, b, c, d, nx, ny, v[i].type == TYPE_VERTEX_CUBIC, 0);
        }
        x = nx;
        y = ny;
    }
}
static int type_cross_less(TypeCross a, TypeCross b)
{
    return a.x < b.x || (a.x == b.x && a.dir < b.dir);
}
/* In-place heap sort: libc qsort may allocate hidden merge-sort storage. */
static void type_cross_sort(TypeCross *v, uint32_t n)
{
    uint32_t start = n / 2, end = n;
    while (end > 1) {
        uint32_t root;
        TypeCross value;
        if (start) {
            root = --start;
            value = v[root];
        } else {
            value = v[--end];
            v[end] = v[0];
            root = 0;
        }
        while (root < end / 2) {
            uint32_t child = root * 2 + 1;
            if (child + 1 < end && type_cross_less(v[child], v[child + 1]))
                child++;
            if (!type_cross_less(value, v[child]))
                break;
            v[root] = v[child];
            root = child;
        }
        v[root] = value;
    }
}
static void type_span(uint32_t *row, uint32_t width, int32_t a, int32_t b)
{
    uint32_t first, last, i;
    int32_t limit = (int32_t)width * 65536;
    if (a < 0)
        a = 0;
    if (b > limit)
        b = limit;
    if (a >= b)
        return;
    first = (uint32_t)a >> 16;
    last = ((uint32_t)b - 1) >> 16;
    for (i = first; i <= last; i++) {
        int32_t l = (int32_t)i * 65536, r = l + 65536;
        row[i] += (uint32_t)((b < r ? b : r) - (a > l ? a : l));
    }
}
TypeError type_font_render_bitmap_scratch(const TypeFont *font, uint16_t gid, float px,
                                          TypeAntiAlias aa, void *pixels, size_t len,
                                          TypeScratch *scratch, TypeBitmap *out)
{
    TypeBitmap info;
    TypeError err;
    TypeVertex *vertices;
    TypeEdges edges;
    TypeCross *cross;
    uint32_t *row, n = 0, count, i, y;
    size_t mark, need;
    double scale;
    volatile float pixel_scale;
    if (aa != TYPE_AA_NONE && aa != TYPE_AA_BOX)
        return TYPE_ERR_UNIMPLEMENTED;
    err = type_font_bitmap_info(font, gid, px, &info);
    if (err != TYPE_OK)
        return err;
    need = type_bitmap_bytes(&info);
    if (!need) {
        if (out)
            *out = info;
        return TYPE_OK;
    }
    if (!pixels || len < need)
        return TYPE_ERR_BUF_TOO_SMALL;
    if (!scratch)
        return TYPE_ERR_SCRATCH_REQUIRED;
    mark = scratch->used;
    scratch->failed = 0;
    memset(pixels, 0, need);
    /* Count first; glyf counting also requires bounded point scratch. */
    err = type_font_outline(font, gid, scratch, NULL, 0, &n);
    if (err != TYPE_OK)
        goto done;
    vertices = ttf_scratch_alloc((size_t)n * sizeof *vertices, scratch);
    if (!vertices) {
        err = TYPE_ERR_SCRATCH_FULL;
        goto done;
    }
    err = type_font_outline(font, gid, scratch, vertices, n, &count);
    if (err != TYPE_OK)
        goto done;
    pixel_scale = px / font->ttf->units_per_em;
    scale = pixel_scale;
    memset(&edges, 0, sizeof edges);
    type_make_edges(&edges, vertices, n, scale, info.bearing_x, info.bearing_y);
    if (edges.err != TYPE_OK) {
        err = edges.err;
        goto done;
    }
    if (info.width > 16383 || info.height > 16383 ||
        (uint64_t)(edges.n + info.width) * info.height * 32 > TYPE_MAX_RASTER_WORK) {
        err = TYPE_ERR_LIMIT;
        goto done;
    }
    edges.cap = edges.n;
    edges.v = ttf_scratch_alloc((size_t)edges.n * sizeof *edges.v, scratch);
    cross = ttf_scratch_alloc((size_t)edges.n * sizeof *cross, scratch);
    row = ttf_scratch_alloc((size_t)info.width * sizeof *row, scratch);
    if (!edges.v || !cross || !row) {
        err = TYPE_ERR_SCRATCH_FULL;
        goto done;
    }
    edges.n = 0;
    type_make_edges(&edges, vertices, n, scale, info.bearing_x, info.bearing_y);
    if (edges.err != TYPE_OK) {
        err = edges.err;
        goto done;
    }
    for (y = 0; y < info.height; y++) {
        unsigned sample;
        memset(row, 0, (size_t)info.width * sizeof *row);
        for (sample = 0; sample < 32; sample++) {
            int32_t sy = (int32_t)y * 65536 + (int32_t)(sample * 2048 + 1024), previous = 0,
                    winding = 0;
            uint32_t k = 0;
            for (i = 0; i < edges.n; i++) {
                TypeEdge e = edges.v[i];
                int32_t low = e.y0 < e.y1 ? e.y0 : e.y1, high = e.y0 < e.y1 ? e.y1 : e.y0;
                if (sy >= low && sy < high) {
                    int64_t dx = (int64_t)e.x1 - e.x0, dy = (int64_t)e.y1 - e.y0;
                    cross[k].x = e.x0 + (int32_t)(dx * ((int64_t)sy - e.y0) / dy);
                    cross[k++].dir = e.y1 > e.y0 ? 1 : -1;
                }
            }
            type_cross_sort(cross, k);
            for (i = 0; i < k; i++) {
                if (winding)
                    type_span(row, info.width, previous, cross[i].x);
                winding += cross[i].dir;
                previous = cross[i].x;
            }
        }
        for (i = 0; i < info.width; i++) {
            uint32_t coverage = (uint32_t)(((uint64_t)row[i] * 255 + 1048576) / 2097152);
            ((uint8_t *)pixels)[(size_t)y * info.width + i] =
                (uint8_t)(aa == TYPE_AA_NONE ? (coverage > 127 ? 255 : 0) : coverage);
        }
    }
    err = TYPE_OK;
done:
    scratch->used = mark;
    if (err != TYPE_OK) {
        memset(pixels, 0, need);
        return err;
    }
    info.pixels = pixels;
    if (out)
        *out = info;
    return TYPE_OK;
}
TypeError type_font_render_bitmap(const TypeFont *f, uint16_t gid, float px, TypeAntiAlias aa,
                                  void *pixels, size_t len, TypeBitmap *out)
{
    return type_font_render_bitmap_scratch(f, gid, px, aa, pixels, len, NULL, out);
}
TypeError type_font_render_sdf(const TypeFont *f, uint16_t gid, float px, void *pixels, size_t len,
                               TypeBitmap *out)
{
    (void)f;
    (void)gid;
    (void)px;
    (void)pixels;
    (void)len;
    (void)out;
    return TYPE_ERR_UNIMPLEMENTED;
}

/* Standalone deterministic shelves; the context uses the same packing policy.
 */
struct TypeAtlas {
    uint32_t w, h, x, y, shelf;
    uint8_t *pixels;
};
struct TypeAtlasRgba {
    uint32_t w, h, x, y, shelf;
    uint32_t *pixels;
};
static size_t type_atlas_size(uint32_t w, uint32_t h, size_t pixel, size_t header)
{
    size_t n;
    if (!w || !h || w > 65535 || h > 65535)
        return 0;
    if ((size_t)w > SIZE_MAX / h)
        return 0;
    n = (size_t)w * h;
    if (n > (SIZE_MAX - header) / pixel)
        return 0;
    return header + n * pixel;
}
size_t type_atlas_memory(uint32_t w, uint32_t h)
{
    return type_atlas_size(w, h, 1, sizeof(TypeAtlas));
}
size_t type_atlas_rgba_memory(uint32_t w, uint32_t h)
{
    return type_atlas_size(w, h, 4, sizeof(TypeAtlasRgba));
}
TypeAtlas *type_atlas_place(void *buf, size_t cap, uint32_t w, uint32_t h)
{
    TypeAtlas *a;
    size_t n = type_atlas_memory(w, h);
    if (!buf || ((uintptr_t)buf & 15u) || !n || cap < n)
        return NULL;
    a = buf;
    memset(buf, 0, n);
    a->w = w;
    a->h = h;
    a->pixels = (uint8_t *)buf + sizeof *a;
    return a;
}
TypeAtlasRgba *type_atlas_rgba_place(void *buf, size_t cap, uint32_t w, uint32_t h)
{
    TypeAtlasRgba *a;
    size_t n = type_atlas_rgba_memory(w, h);
    if (!buf || ((uintptr_t)buf & 15u) || !n || cap < n)
        return NULL;
    a = buf;
    memset(buf, 0, n);
    a->w = w;
    a->h = h;
    a->pixels = (uint32_t *)((uint8_t *)buf + sizeof *a);
    return a;
}
const uint8_t *type_atlas_pixels(const TypeAtlas *a, uint32_t *w, uint32_t *h)
{
    if (w)
        *w = a ? a->w : 0;
    if (h)
        *h = a ? a->h : 0;
    return a ? a->pixels : NULL;
}
const uint32_t *type_atlas_rgba_pixels(const TypeAtlasRgba *a, uint32_t *w, uint32_t *h)
{
    if (w)
        *w = a ? a->w : 0;
    if (h)
        *h = a ? a->h : 0;
    return a ? a->pixels : NULL;
}
static int type_shelf(uint32_t aw, uint32_t ah, uint32_t *sx, uint32_t *sy, uint32_t *sh,
                      uint32_t w, uint32_t h, TypeRect *out)
{
    uint32_t x = *sx, y = *sy, shelf = *sh;
    if (!out || w > aw || h > ah)
        return 0;
    if (w > aw - x) {
        x = 0;
        if (shelf > ah - y)
            return 0;
        y += shelf;
        shelf = 0;
    }
    if (h > ah - y)
        return 0;
    out->x = x;
    out->y = y;
    out->w = w;
    out->h = h;
    *sx = x + w;
    *sy = y;
    *sh = h > shelf ? h : shelf;
    return 1;
}
int type_atlas_pack(TypeAtlas *a, uint32_t w, uint32_t h, TypeRect *out)
{
    return a && type_shelf(a->w, a->h, &a->x, &a->y, &a->shelf, w, h, out);
}
int type_atlas_rgba_pack(TypeAtlasRgba *a, uint32_t w, uint32_t h, TypeRect *out)
{
    return a && type_shelf(a->w, a->h, &a->x, &a->y, &a->shelf, w, h, out);
}
void type_atlas_blit(TypeAtlas *a, const TypeRect *r, const uint8_t *src)
{
    uint32_t y;
    if (!a || !r || !src || r->x > a->w || r->y > a->h || r->w > a->w - r->x || r->h > a->h - r->y)
        return;
    for (y = 0; y < r->h; y++)
        memcpy(a->pixels + (size_t)(r->y + y) * a->w + r->x, src + (size_t)y * r->w, r->w);
}
void type_atlas_rgba_blit(TypeAtlasRgba *a, const TypeRect *r, const uint8_t *src)
{
    uint32_t y;
    if (!a || !r || !src || r->x > a->w || r->y > a->h || r->w > a->w - r->x || r->h > a->h - r->y)
        return;
    for (y = 0; y < r->h; y++)
        memcpy(a->pixels + (size_t)(r->y + y) * a->w + r->x, src + (size_t)y * r->w * 4,
               (size_t)r->w * 4);
}
void type_atlas_clear(TypeAtlas *a)
{
    if (a) {
        memset(a->pixels, 0, (size_t)a->w * a->h);
        a->x = a->y = a->shelf = 0;
    }
}
void type_atlas_rgba_clear(TypeAtlasRgba *a)
{
    if (a) {
        memset(a->pixels, 0, (size_t)a->w * a->h * 4);
        a->x = a->y = a->shelf = 0;
    }
}

/* type.c - Faces, atlas, and UTF-8 layout. Copyright (c) 2025-2026 Vasco Alves
 * Caller provides the buffer from type_memory. Font bytes are borrowed.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#define TYPE_NONE 0xFFFFFFFFu

typedef struct CacheSlot {
    TypeGlyphKey key;
    TypeGlyph glyph;
    uint32_t stamp;
    uint8_t used;
} CacheSlot;

typedef struct FaceRec {
    TypeTTF *ttf;
    TypeFont *font;
    uint32_t id;
} FaceRec;

struct TypeCtx {
    TypeParams params;
    uint32_t nfaces;
    uint32_t primary;
    uint32_t bold;
    uint32_t italic;
    uint32_t bold_italic;
    uint32_t fallbacks[TYPE_DEFAULT_MAX_FALLBACK];
    uint32_t nfallbacks;
    uint16_t ascii_size;
    uint8_t warming;
    uint8_t ascii_ok[TYPE_ASCII_N];
    uint8_t replacement_ok;
    TypeGlyph ascii[TYPE_ASCII_N];
    TypeGlyph replacement;
    TypeGlyphStats stats;
    uint64_t atlas_revision;
    uint32_t aw, ah;
    uint32_t shelf_x, shelf_y, shelf_h;
    uint8_t *cov;
    uint32_t *rgba;
    uint32_t cache_n;
    uint32_t stamp;
    CacheSlot *cache;
    TypeScratch *scratch;
    FaceRec *faces;
    unsigned char *bytes;
    size_t face_stride;
};

typedef struct TypeLayout {
    size_t total;
    size_t faces;
    size_t cov;
    size_t rgba;
    size_t cache;
    size_t face_stride;
    size_t face_bytes;
} TypeLayout;

static size_t type_align(size_t v, size_t a);
static int type_layout(const TypeParams *params, TypeLayout *lay);
static uint16_t type_size_px(float size_px);
static void type_atlas_reset(struct TypeCtx *ctx);
static int type_pack(struct TypeCtx *ctx, uint32_t w, uint32_t h, TypeRect *out);
static FaceRec *type_face(const struct TypeCtx *ctx, uint32_t id);
static TypeError type_raster(struct TypeCtx *ctx, FaceRec *face, uint16_t gid, uint16_t size_u,
                             TypeGlyph *out);
static TypeError type_resolve(struct TypeCtx *ctx, uint32_t codepoint, TypeStyle style,
                              FaceRec **face, uint16_t *gid);
static uint64_t type_now_ns(void);
static int type_cache_find(struct TypeCtx *ctx, const TypeGlyphKey *key, TypeGlyph *out, int touch);
static void type_cache_put(struct TypeCtx *ctx, const TypeGlyphKey *key, const TypeGlyph *glyph);
static void type_ascii_stash(struct TypeCtx *ctx);

static size_t type_align(size_t v, size_t a)
{
    return (v + a - 1u) & ~(a - 1u);
}

/* Add one aligned array without intermediate multiplication/add overflow. */
static int type_layout_array(size_t *o, size_t count, size_t stride)
{
    size_t n;
    if (stride && count > SIZE_MAX / stride)
        return 0;
    n = count * stride;
    if (*o > SIZE_MAX - 15u || n > SIZE_MAX - 15u - *o)
        return 0;
    *o = type_align(*o + n, 16);
    return 1;
}

static int type_layout(const TypeParams *params, TypeLayout *lay)
{
    size_t o, pixels;
    if (!params || !lay || !params->atlas_width || !params->atlas_height ||
        params->atlas_width > 65535u || params->atlas_height > 65535u || !params->cache_capacity ||
        !params->max_faces || params->antialias > TYPE_AA_SDF)
        return 0;
    memset(lay, 0, sizeof *lay);
    lay->face_stride = type_align(type_ttf_memory(), 16) + type_align(type_font_memory(), 16);
    o = type_align(sizeof(struct TypeCtx), 16);
    lay->faces = o;
    if (!type_layout_array(&o, params->max_faces, sizeof(FaceRec)))
        return 0;
    lay->face_bytes = o;
    if (!type_layout_array(&o, params->max_faces, lay->face_stride))
        return 0;
    pixels = (size_t)params->atlas_width * params->atlas_height;
    lay->cov = o;
    if (!type_layout_array(&o, pixels, 1))
        return 0;
    lay->rgba = o;
    if (!type_layout_array(&o, pixels, 4))
        return 0;
    lay->cache = o;
    if (!type_layout_array(&o, params->cache_capacity, sizeof(CacheSlot)))
        return 0;
    lay->total = o;
    return 1;
}

void type_params_default(TypeParams *params)
{
    if (!params)
        return;
    memset(params, 0, sizeof *params);
    params->atlas_width = TYPE_DEFAULT_ATLAS_WIDTH;
    params->atlas_height = TYPE_DEFAULT_ATLAS_HEIGHT;
    params->cache_capacity = TYPE_DEFAULT_CACHE;
    params->max_faces = TYPE_DEFAULT_MAX_FACES;
    params->max_fallbacks = TYPE_DEFAULT_MAX_FALLBACK;
    params->ligatures = 1;
    params->color_emoji = 1;
    params->antialias = TYPE_AA_BOX;
}

void type_face_params_default(TypeFaceParams *face)
{
    if (!face)
        return;
    memset(face, 0, sizeof *face);
}

size_t type_memory(const TypeParams *params)
{
    TypeLayout lay;

    if (!type_layout(params, &lay))
        return 0;
    return lay.total;
}

TypeCtx *type_place(void *buf, size_t bufsize, const TypeParams *params)
{
    struct TypeCtx *ctx;
    TypeLayout lay;
    unsigned char *base;

    if (!buf || ((uintptr_t)buf & 15u) || !type_layout(params, &lay) || bufsize < lay.total)
        return NULL;
    memset(buf, 0, lay.total);
    base = buf;
    ctx = buf;
    ctx->params = *params;
    if (ctx->params.max_fallbacks > TYPE_DEFAULT_MAX_FALLBACK)
        ctx->params.max_fallbacks = TYPE_DEFAULT_MAX_FALLBACK;
    ctx->primary = TYPE_NONE;
    ctx->bold = TYPE_NONE;
    ctx->italic = TYPE_NONE;
    ctx->bold_italic = TYPE_NONE;
    ctx->atlas_revision = 1;
    ctx->aw = params->atlas_width;
    ctx->ah = params->atlas_height;
    ctx->bytes = base;
    ctx->face_stride = lay.face_stride;
    ctx->faces = (FaceRec *)(base + lay.faces);
    ctx->cov = base + lay.cov;
    ctx->rgba = (uint32_t *)(base + lay.rgba);
    ctx->cache = (CacheSlot *)(base + lay.cache);
    ctx->cache_n = params->cache_capacity;
    ctx->scratch = NULL;
    return ctx;
}

void type_set_scratch(TypeCtx *ctx, TypeScratch *scratch)
{
    if (ctx)
        ctx->scratch = scratch;
}

static uint16_t type_size_px(float size_px)
{
    uint32_t s;

    if (!(size_px > 0.0f && size_px <= 65535.0f))
        return 0;
    s = (uint32_t)(size_px + 0.5f);
    if (s < 1)
        s = 1;
    if (s > 65535)
        s = 65535;
    return (uint16_t)s;
}

static void type_atlas_reset(struct TypeCtx *ctx)
{
    ctx->atlas_revision++;
    memset(ctx->cov, 0, (size_t)ctx->aw * (size_t)ctx->ah);
    memset(ctx->rgba, 0, (size_t)ctx->aw * (size_t)ctx->ah * 4u);
    memset(ctx->cache, 0, (size_t)ctx->cache_n * sizeof(CacheSlot));
    memset(ctx->ascii_ok, 0, sizeof ctx->ascii_ok);
    ctx->replacement_ok = 0;
    ctx->shelf_x = 0;
    ctx->shelf_y = 0;
    ctx->shelf_h = 0;
    ctx->stamp = 1;
}

static int type_pack(struct TypeCtx *ctx, uint32_t w, uint32_t h, TypeRect *out)
{
    uint32_t pw, ph;

    pw = w + 1;
    ph = h + 1;
    if (pw > ctx->aw || ph > ctx->ah)
        return 0;
    if (ctx->shelf_x + pw > ctx->aw) {
        ctx->shelf_y += ctx->shelf_h;
        ctx->shelf_x = 0;
        ctx->shelf_h = 0;
    }
    if (ctx->shelf_y + ph > ctx->ah)
        return 0;
    out->x = ctx->shelf_x;
    out->y = ctx->shelf_y;
    out->w = w;
    out->h = h;
    ctx->shelf_x += pw;
    if (ph > ctx->shelf_h)
        ctx->shelf_h = ph;
    return 1;
}

static FaceRec *type_face(const struct TypeCtx *ctx, uint32_t id)
{
    uint32_t i;

    if (!ctx || id == TYPE_NONE)
        return NULL;
    for (i = 0; i < ctx->nfaces; i++) {
        if (ctx->faces[i].id == id)
            return &ctx->faces[i];
    }
    return NULL;
}

static uint64_t type_now_ns(void)
{
#ifdef TYPE_NOW_NS
    return TYPE_NOW_NS(); /* optional platform monotonic timer */
#else
    clock_t ticks = clock(); /* C99 fallback: process CPU time */
    if (ticks == (clock_t)-1)
        return 0;
    return (uint64_t)((double)ticks * (1000000000.0 / (double)CLOCKS_PER_SEC));
#endif
}

static void type_ascii_stash(struct TypeCtx *ctx)
{
    TypeGlyphKey key;
    uint32_t i;

    if (!ctx || ctx->ascii_size == 0)
        return;
    memset(&key, 0, sizeof key);
    key.size_px = ctx->ascii_size;
    for (i = 0; i < TYPE_ASCII_N; i++) {
        if (!ctx->ascii_ok[i])
            continue;
        key.font_id = ctx->ascii[i].font_id;
        key.glyph_id = ctx->ascii[i].glyph_id;
        if (type_cache_find(ctx, &key, NULL, 0))
            continue;
        type_cache_put(ctx, &key, &ctx->ascii[i]);
    }
    if (!ctx->replacement_ok)
        return;
    key.font_id = ctx->replacement.font_id;
    key.glyph_id = ctx->replacement.glyph_id;
    if (!type_cache_find(ctx, &key, NULL, 0))
        type_cache_put(ctx, &key, &ctx->replacement);
}

static TypeError type_raster(struct TypeCtx *ctx, FaceRec *face, uint16_t gid, uint16_t size_u,
                             TypeGlyph *out)
{
    TypeBitmap info;
    TypeError err;
    TypeRect rect;
    uint16_t adv;
    float scale;
    size_t need, mark;
    uint8_t *bitmap;
    uint32_t row;
    uint64_t t0, t1;
    TypeAntiAlias aa;

    memset(out, 0, sizeof *out);
    out->font_id = face->id;
    out->glyph_id = gid;
    err = type_font_advance(face->font, gid, &adv);
    if (err != TYPE_OK)
        return err;
    scale = (float)size_u / (float)face->ttf->units_per_em;
    out->advance = (float)adv * scale;
    aa = (TypeAntiAlias)ctx->params.antialias;
    err = type_font_bitmap_info(face->font, gid, (float)size_u, &info);
    if (err != TYPE_OK)
        return err;
    out->bearing_x = (float)info.bearing_x;
    out->bearing_y = (float)info.bearing_y;
    if (info.width == 0 || info.height == 0)
        return TYPE_OK;
    need = type_bitmap_bytes(&info);
    if (!ctx->scratch)
        return TYPE_ERR_SCRATCH_REQUIRED;
    mark = ctx->scratch->used;
    ctx->scratch->failed = 0;
    bitmap = (uint8_t *)ttf_scratch_alloc(need, ctx->scratch);
    if (!bitmap)
        return TYPE_ERR_SCRATCH_FULL;
    t0 = type_now_ns();
    err = type_font_render_bitmap_scratch(face->font, gid, (float)size_u, aa, bitmap, need,
                                          ctx->scratch, &info);
    t1 = type_now_ns();
    if (t1 > t0)
        ctx->stats.raster_ns += t1 - t0;
    if (err != TYPE_OK) {
        ctx->scratch->used = mark;
        return err;
    }
    if (!type_pack(ctx, info.width, info.height, &rect)) {
        ctx->scratch->used = mark;
        return TYPE_ERR_ATLAS_FULL;
    }
    t0 = type_now_ns();
    for (row = 0; row < info.height; row++) {
        memcpy(ctx->cov + ((size_t)(rect.y + row) * ctx->aw + rect.x),
               bitmap + (size_t)row * info.width, info.width);
    }
    ctx->scratch->used = mark;
    t1 = type_now_ns();
    if (t1 > t0)
        ctx->stats.atlas_ns += t1 - t0;
    ctx->atlas_revision++;
    out->width = info.width;
    out->height = info.height;
    out->atlas_x = (uint16_t)rect.x;
    out->atlas_y = (uint16_t)rect.y;
    out->bearing_x = (float)info.bearing_x;
    out->bearing_y = (float)info.bearing_y;
    ctx->stats.misses++;
    return TYPE_OK;
}

static int type_cache_find(struct TypeCtx *ctx, const TypeGlyphKey *key, TypeGlyph *out, int touch)
{
    uint32_t i;

    for (i = 0; i < ctx->cache_n; i++) {
        CacheSlot *s;

        s = &ctx->cache[i];
        if (!s->used)
            continue;
        if (s->key.font_id != key->font_id || s->key.glyph_id != key->glyph_id)
            continue;
        if (s->key.size_px != key->size_px || s->key.flags != key->flags)
            continue;
        if (touch) {
            ctx->stamp++;
            s->stamp = ctx->stamp;
        }
        if (out)
            *out = s->glyph;
        return 1;
    }
    return 0;
}

static void type_cache_put(struct TypeCtx *ctx, const TypeGlyphKey *key, const TypeGlyph *glyph)
{
    uint32_t i;
    uint32_t victim;
    uint32_t best;

    for (i = 0; i < ctx->cache_n; i++) {
        if (!ctx->cache[i].used) {
            ctx->stamp++;
            ctx->cache[i].used = 1;
            ctx->cache[i].key = *key;
            ctx->cache[i].glyph = *glyph;
            ctx->cache[i].stamp = ctx->stamp;
            return;
        }
    }
    victim = 0;
    best = ctx->cache[0].stamp;
    for (i = 1; i < ctx->cache_n; i++) {
        if (ctx->cache[i].stamp < best) {
            best = ctx->cache[i].stamp;
            victim = i;
        }
    }
    ctx->stamp++;
    ctx->cache[victim].used = 1;
    ctx->cache[victim].key = *key;
    ctx->cache[victim].glyph = *glyph;
    ctx->cache[victim].stamp = ctx->stamp;
}

static TypeError type_resolve(struct TypeCtx *ctx, uint32_t codepoint, TypeStyle style,
                              FaceRec **face, uint16_t *gid)
{
    uint32_t order[8];
    uint32_t n, i;
    FaceRec *primary;

    n = 0;
    if (style.bold && style.italic && ctx->bold_italic != TYPE_NONE)
        order[n++] = ctx->bold_italic;
    else if (style.italic && ctx->italic != TYPE_NONE)
        order[n++] = ctx->italic;
    else if (style.bold && ctx->bold != TYPE_NONE)
        order[n++] = ctx->bold;
    if (ctx->primary != TYPE_NONE)
        order[n++] = ctx->primary;
    for (i = 0; i < ctx->nfallbacks && n < 8; i++)
        order[n++] = ctx->fallbacks[i];
    for (i = 0; i < n; i++) {
        FaceRec *f;
        uint16_t g;

        f = type_face(ctx, order[i]);
        if (!f)
            continue;
        if (type_font_glyph_index(f->font, codepoint, &g) == TYPE_OK) {
            *face = f;
            *gid = g;
            return TYPE_OK;
        }
    }
    primary = type_face(ctx, ctx->primary);
    if (!primary)
        return TYPE_ERR_INVALID_FONT;
    *face = primary;
    *gid = 0;
    return TYPE_OK;
}

TypeError type_add_font(TypeCtx *ctx, const void *bytes, size_t len, const TypeFaceParams *face,
                        uint32_t *out_id)
{
    return type_add_font_trusted(ctx, bytes, len, face, out_id);
}

TypeError type_add_font_trusted(TypeCtx *ctx, const void *bytes, size_t len,
                                const TypeFaceParams *face, uint32_t *out_id)
{
    TypeFaceParams def;
    FaceRec *rec;
    unsigned char *slot;
    TypeTTF *ttf;
    TypeFont *font;
    uint32_t id;

    if (!ctx || !bytes)
        return TYPE_ERR_INVALID_FONT;
    if (!face) {
        type_face_params_default(&def);
        face = &def;
    }
    if (ctx->nfaces >= ctx->params.max_faces)
        return TYPE_ERR_BUF_TOO_SMALL;
    id = ctx->nfaces;
    {
        TypeLayout lay;

        if (!type_layout(&ctx->params, &lay))
            return TYPE_ERR_INVALID_FONT;
        slot = ctx->bytes + lay.face_bytes + (size_t)id * lay.face_stride;
        ttf = type_ttf_place_trusted(slot, type_ttf_memory(), bytes, len);
        if (!ttf)
            return type_ttf_error((TypeTTF *)slot);
        if (!type_ttf_outline(ttf))
            return TYPE_ERR_INVALID_FONT;
        font = type_font_from_ttf(slot + type_align(type_ttf_memory(), 16), type_font_memory(), ttf,
                                  face);
        if (!font)
            return TYPE_ERR_INVALID_FONT;
    }
    rec = &ctx->faces[id];
    rec->ttf = ttf;
    rec->font = font;
    rec->id = id;
    ctx->nfaces++;
    if (ctx->primary == TYPE_NONE)
        ctx->primary = id;
    if (face->style.bold && face->style.italic)
        ctx->bold_italic = id;
    else if (face->style.bold)
        ctx->bold = id;
    else if (face->style.italic)
        ctx->italic = id;
    if (out_id)
        *out_id = id;
    return TYPE_OK;
}

void type_clear_fonts(TypeCtx *ctx)
{
    if (!ctx)
        return;
    ctx->nfaces = 0;
    ctx->primary = TYPE_NONE;
    ctx->bold = TYPE_NONE;
    ctx->italic = TYPE_NONE;
    ctx->bold_italic = TYPE_NONE;
    ctx->nfallbacks = 0;
    ctx->ascii_size = 0;
    type_atlas_reset(ctx);
}

void type_select(TypeCtx *ctx, uint32_t font_id)
{
    if (!ctx || !type_face(ctx, font_id))
        return;
    type_ascii_stash(ctx);
    ctx->primary = font_id;
    ctx->ascii_size = 0;
    memset(ctx->ascii_ok, 0, sizeof ctx->ascii_ok);
    ctx->replacement_ok = 0;
}

TypeError type_set_fallbacks(TypeCtx *ctx, const uint32_t *ids, uint32_t count)
{
    uint32_t i;

    if (!ctx || (!ids && count))
        return TYPE_ERR_INVALID_FONT;
    if (count > ctx->params.max_fallbacks)
        return TYPE_ERR_BUF_TOO_SMALL;
    for (i = 0; i < count; i++) {
        if (!type_face(ctx, ids[i]))
            return TYPE_ERR_INVALID_FONT;
    }
    ctx->nfallbacks = count;
    for (i = 0; i < count; i++)
        ctx->fallbacks[i] = ids[i];
    return TYPE_OK;
}

uint64_t type_ctx_atlas_revision(const TypeCtx *ctx)
{
    return ctx ? ctx->atlas_revision : 0;
}

const uint8_t *type_ctx_atlas(const TypeCtx *ctx, uint32_t *width, uint32_t *height)
{
    if (!ctx)
        return NULL;
    if (width)
        *width = ctx->aw;
    if (height)
        *height = ctx->ah;
    return ctx->cov;
}

const uint32_t *type_ctx_color_atlas(const TypeCtx *ctx, uint32_t *width, uint32_t *height)
{
    if (!ctx)
        return NULL;
    if (width)
        *width = ctx->aw;
    if (height)
        *height = ctx->ah;
    return ctx->rgba;
}

TypeError type_metrics(const TypeCtx *ctx, float size_px, TypeMetrics *out)
{
    FaceRec *face;

    if (!ctx || !out)
        return TYPE_ERR_INVALID_FONT;
    face = type_face(ctx, ctx->primary);
    if (!face)
        return TYPE_ERR_INVALID_FONT;
    return type_font_metrics(face->font, size_px, out);
}

static TypeError type_glyph_styled_inner(struct TypeCtx *ctx, uint32_t codepoint, float size_px,
                                         TypeStyle style, TypeGlyph *out)
{
    uint16_t size_u;
    uint16_t gid;
    FaceRec *face;
    TypeError err;
    TypeGlyphKey key;
    int styled;
    uint32_t slot;

    if (!ctx || !out || !(size_px > 0.0f))
        return TYPE_ERR_INVALID_FONT;
    size_u = type_size_px(size_px);
    if (!size_u)
        return TYPE_ERR_INVALID_FONT;
    styled = style.bold || style.italic;
    if (!ctx->warming && size_u != ctx->ascii_size) {
        ctx->warming = 1;
        err = type_warm_ascii(ctx, (float)size_u);
        ctx->warming = 0;
        if (err != TYPE_OK)
            return err;
    }
    if (!styled && codepoint >= TYPE_ASCII_LO && codepoint <= TYPE_ASCII_HI &&
        size_u == ctx->ascii_size) {
        slot = codepoint - TYPE_ASCII_LO;
        if (ctx->ascii_ok[slot]) {
            ctx->stats.hits++;
            *out = ctx->ascii[slot];
            return TYPE_OK;
        }
    }
    if (!styled && codepoint == TYPE_REPLACEMENT && ctx->replacement_ok &&
        size_u == ctx->ascii_size) {
        ctx->stats.hits++;
        *out = ctx->replacement;
        return TYPE_OK;
    }
    err = type_resolve(ctx, codepoint, style, &face, &gid);
    if (err != TYPE_OK)
        return err;
    key.font_id = face->id;
    key.glyph_id = gid;
    key.size_px = size_u;
    key.flags = 0;
    if (style.bold)
        key.flags |= TYPE_STYLE_BOLD;
    if (style.italic)
        key.flags |= TYPE_STYLE_ITALIC;
    if (type_cache_find(ctx, &key, out, 1)) {
        ctx->stats.hits++;
        if (!styled && codepoint >= TYPE_ASCII_LO && codepoint <= TYPE_ASCII_HI &&
            size_u == ctx->ascii_size) {
            slot = codepoint - TYPE_ASCII_LO;
            ctx->ascii[slot] = *out;
            ctx->ascii_ok[slot] = 1;
        } else if (!styled && codepoint == TYPE_REPLACEMENT && size_u == ctx->ascii_size) {
            ctx->replacement = *out;
            ctx->replacement_ok = 1;
        }
        return TYPE_OK;
    }
    err = type_raster(ctx, face, gid, size_u, out);
    if (err != TYPE_OK)
        return err;
    if (!styled && codepoint >= TYPE_ASCII_LO && codepoint <= TYPE_ASCII_HI) {
        slot = codepoint - TYPE_ASCII_LO;
        ctx->ascii[slot] = *out;
        ctx->ascii_ok[slot] = 1;
    } else if (!styled && codepoint == TYPE_REPLACEMENT) {
        ctx->replacement = *out;
        ctx->replacement_ok = 1;
    } else {
        type_cache_put(ctx, &key, out);
    }
    return TYPE_OK;
}

TypeError type_glyph_id(TypeCtx *ctx, uint32_t font_id, uint32_t glyph_id, float size_px,
                        TypeGlyph *out)
{
    FaceRec *face;
    TypeGlyphKey key;
    TypeError err;
    uint16_t size_u = type_size_px(size_px);
    if (!ctx || !out || !size_u)
        return TYPE_ERR_INVALID_FONT;
    face = type_face(ctx, font_id);
    if (!face)
        return TYPE_ERR_INVALID_FONT;
    if (glyph_id >= (uint32_t)face->ttf->num_glyphs)
        return TYPE_ERR_GLYPH_NOT_FOUND;
    memset(&key, 0, sizeof key);
    key.font_id = font_id;
    key.glyph_id = glyph_id;
    key.size_px = size_u;
    if (type_cache_find(ctx, &key, out, 1)) {
        ctx->stats.hits++;
        return TYPE_OK;
    }
    err = type_raster(ctx, face, (uint16_t)glyph_id, size_u, out);
    if (err == TYPE_OK)
        type_cache_put(ctx, &key, out);
    return err;
}

static TypeError type_layout_shaped_inner(TypeCtx *ctx, const TypeShapedGlyph *run, uint32_t count,
                                          float size_px, TypePositionedGlyph *out, uint32_t cap,
                                          uint32_t *written, TypeTextMetrics *metrics, int measure)
{
    TypeTextMetrics m;
    TypeError err = TYPE_OK;
    uint32_t i = 0;
    memset(&m, 0, sizeof m);
    if (written)
        *written = 0;
    if (metrics)
        *metrics = m;
    if (!ctx || (!run && count) || !type_size_px(size_px))
        return TYPE_ERR_INVALID_FONT;
    for (i = 0; i < count; i++) {
        const TypeShapedGlyph *r = &run[i];
        TypeGlyph g;
        float x, y, next_x, next_y;
        if (i >= cap) {
            err = TYPE_ERR_BUF_TOO_SMALL;
            break;
        }
        if (!isfinite(r->x_advance) || !isfinite(r->y_advance) || !isfinite(r->x_offset) ||
            !isfinite(r->y_offset)) {
            err = TYPE_ERR_INVALID_FONT;
            break;
        }
        x = m.advance_x + r->x_offset;
        y = m.advance_y + r->y_offset;
        next_x = m.advance_x + r->x_advance;
        next_y = m.advance_y + r->y_advance;
        /* Leave ample range for bitmap bearings and dimensions. */
        if (!isfinite(x) || !isfinite(y) || !isfinite(next_x) || !isfinite(next_y) ||
            fabsf(x) > FLT_MAX / 2 || fabsf(y) > FLT_MAX / 2) {
            err = TYPE_ERR_INVALID_FONT;
            break;
        }
        if (measure) {
            FaceRec *face = type_face(ctx, r->font_id);
            TypeBitmap b;
            if (!face) {
                err = TYPE_ERR_INVALID_FONT;
                break;
            }
            if (r->glyph_id >= (uint32_t)face->ttf->num_glyphs) {
                err = TYPE_ERR_GLYPH_NOT_FOUND;
                break;
            }
            err = type_font_bitmap_info(face->font, (uint16_t)r->glyph_id,
                                        (float)type_size_px(size_px), &b);
            if (err != TYPE_OK)
                break;
            memset(&g, 0, sizeof g);
            g.bearing_x = b.bearing_x;
            g.bearing_y = b.bearing_y;
            g.width = b.width;
            g.height = b.height;
        } else {
            err = type_glyph_id(ctx, r->font_id, r->glyph_id, size_px, &g);
            if (err != TYPE_OK)
                break;
        }
        if (out) {
            out[i].glyph = g;
            out[i].cluster = r->cluster;
            out[i].x = x;
            out[i].y = y;
        }
        if (g.width && g.height) {
            float left = x + g.bearing_x, top = y - g.bearing_y;
            float right = left + g.width, bottom = top + g.height;
            if (!m.has_ink) {
                m.min_x = left;
                m.min_y = top;
                m.max_x = right;
                m.max_y = bottom;
            } else {
                if (left < m.min_x)
                    m.min_x = left;
                if (top < m.min_y)
                    m.min_y = top;
                if (right > m.max_x)
                    m.max_x = right;
                if (bottom > m.max_y)
                    m.max_y = bottom;
            }
            m.has_ink = 1;
        }
        m.advance_x = next_x;
        m.advance_y = next_y;
    }
    if (written)
        *written = i;
    if (metrics)
        *metrics = m;
    return err;
}

TypeError type_layout_shaped(TypeCtx *ctx, const TypeShapedGlyph *run, uint32_t count,
                             float size_px, TypePositionedGlyph *out, uint32_t cap,
                             uint32_t *written, TypeTextMetrics *metrics)
{
    return type_layout_shaped_inner(ctx, run, count, size_px, out, cap, written, metrics, 0);
}

TypeError type_measure_shaped(const TypeCtx *ctx, const TypeShapedGlyph *run, uint32_t count,
                              float size_px, uint32_t *written, TypeTextMetrics *metrics)
{
    return type_layout_shaped_inner((TypeCtx *)ctx, run, count, size_px, NULL, count, written,
                                    metrics, 1);
}

TypeError type_glyph(TypeCtx *ctx, uint32_t codepoint, float size_px, TypeGlyph *out)
{
    TypeStyle style;

    memset(&style, 0, sizeof style);
    return type_glyph_styled_inner(ctx, codepoint, size_px, style, out);
}

TypeError type_glyph_styled(TypeCtx *ctx, uint32_t codepoint, float size_px, TypeStyle style,
                            TypeGlyph *out)
{
    return type_glyph_styled_inner(ctx, codepoint, size_px, style, out);
}

TypeError type_warm_ascii(TypeCtx *ctx, float size_px)
{
    uint16_t size_u;
    uint32_t cp;
    TypeGlyph g;
    TypeError err;
    int save;

    if (!ctx)
        return TYPE_ERR_INVALID_FONT;
    size_u = type_size_px(size_px);
    if (size_u == 0)
        return TYPE_ERR_INVALID_FONT;
    if (size_u != ctx->ascii_size) {
        type_ascii_stash(ctx);
        ctx->ascii_size = size_u;
        memset(ctx->ascii_ok, 0, sizeof ctx->ascii_ok);
        ctx->replacement_ok = 0;
    }
    save = ctx->warming;
    ctx->warming = 1;
    for (cp = TYPE_ASCII_LO; cp <= TYPE_ASCII_HI; cp++) {
        if (ctx->ascii_ok[cp - TYPE_ASCII_LO])
            continue;
        err = type_glyph(ctx, cp, (float)size_u, &g);
        if (err != TYPE_OK) {
            ctx->warming = save;
            return err;
        }
    }
    ctx->warming = save;
    return TYPE_OK;
}

TypeError type_peek_glyph_styled(const TypeCtx *ctx, uint32_t codepoint, float size_px,
                                 TypeStyle style, TypeGlyph *out)
{
    uint16_t size_u;
    int styled;

    if (!ctx || !out)
        return TYPE_ERR_GLYPH_NOT_FOUND;
    size_u = type_size_px(size_px);
    if (size_u == 0 || size_u != ctx->ascii_size)
        return TYPE_ERR_GLYPH_NOT_FOUND;
    styled = style.bold || style.italic;
    if (!styled && codepoint >= TYPE_ASCII_LO && codepoint <= TYPE_ASCII_HI) {
        uint32_t slot;

        slot = codepoint - TYPE_ASCII_LO;
        if (!ctx->ascii_ok[slot])
            return TYPE_ERR_GLYPH_NOT_FOUND;
        *out = ctx->ascii[slot];
        return TYPE_OK;
    }
    if (!styled && codepoint == TYPE_REPLACEMENT && ctx->replacement_ok) {
        *out = ctx->replacement;
        return TYPE_OK;
    }
    {
        struct TypeCtx *mut;
        FaceRec *face;
        uint16_t gid;
        TypeGlyphKey key;

        mut = (struct TypeCtx *)ctx;
        if (type_resolve(mut, codepoint, style, &face, &gid) != TYPE_OK)
            return TYPE_ERR_GLYPH_NOT_FOUND;
        memset(&key, 0, sizeof key);
        key.font_id = face->id;
        key.glyph_id = gid;
        key.size_px = size_u;
        if (style.bold)
            key.flags |= TYPE_STYLE_BOLD;
        if (style.italic)
            key.flags |= TYPE_STYLE_ITALIC;
        if (!type_cache_find(mut, &key, out, 0))
            return TYPE_ERR_GLYPH_NOT_FOUND;
        return TYPE_OK;
    }
}

TypeError type_peek_glyph(const TypeCtx *ctx, uint32_t codepoint, float size_px, TypeGlyph *out)
{
    TypeStyle style;

    memset(&style, 0, sizeof style);
    return type_peek_glyph_styled(ctx, codepoint, size_px, style, out);
}

void type_clear_atlas(TypeCtx *ctx)
{
    float size;

    if (!ctx)
        return;
    size = (float)ctx->ascii_size;
    type_atlas_reset(ctx);
    if (ctx->ascii_size == 0 && size == 0.0f)
        return;
    ctx->ascii_size = 0;
    if (size > 0.0f)
        type_warm_ascii(ctx, size);
}

void type_stats(const TypeCtx *ctx, TypeGlyphStats *out)
{
    if (!out)
        return;
    if (!ctx) {
        memset(out, 0, sizeof *out);
        return;
    }
    *out = ctx->stats;
}

uint32_t type_utf8_next(const char *s, size_t len, uint32_t *codepoint)
{
    const uint8_t *p;
    uint32_t cp;
    uint32_t need;
    uint32_t i;

    if (!s || !codepoint || len == 0)
        return 0;
    p = (const uint8_t *)s;
    if (p[0] < 0x80) {
        *codepoint = p[0];
        return 1;
    }
    if ((p[0] & 0xE0) == 0xC0) {
        need = 2;
        cp = p[0] & 0x1F;
    } else if ((p[0] & 0xF0) == 0xE0) {
        need = 3;
        cp = p[0] & 0x0F;
    } else if ((p[0] & 0xF8) == 0xF0) {
        need = 4;
        cp = p[0] & 0x07;
    } else {
        return 0;
    }
    if (len < need)
        return 0;
    for (i = 1; i < need; i++) {
        if ((p[i] & 0xC0) != 0x80)
            return 0;
        cp = (cp << 6) | (p[i] & 0x3F);
    }
    if (need == 2 && cp < 0x80)
        return 0;
    if (need == 3 && cp < 0x800)
        return 0;
    if (need == 4 && cp < 0x10000)
        return 0;
    if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
        return 0;
    *codepoint = cp;
    return need;
}

int type_utf8_validate(const char *s, size_t len)
{
    size_t i;

    if (!s && len)
        return 0;
    i = 0;
    while (i < len) {
        uint32_t cp;
        uint32_t n;

        cp = 0;
        n = type_utf8_next(s + i, len - i, &cp);
        if (n == 0)
            return 0;
        i += n;
    }
    return 1;
}

TypeError type_layout_utf8(TypeCtx *ctx, const char *text, size_t len, float size_px, TypeCell *out,
                           uint32_t cap, uint32_t *written, size_t *bytes_used)
{
    size_t i;
    uint32_t n;
    float x;

    if (written)
        *written = 0;
    if (bytes_used)
        *bytes_used = 0;
    if (!ctx || (!text && len) || !(size_px > 0.0f))
        return TYPE_ERR_INVALID_FONT;
    if (cap && !out)
        return TYPE_ERR_BUF_TOO_SMALL;
    i = 0;
    n = 0;
    x = 0.0f;
    while (i < len) {
        uint32_t cp;
        uint32_t adv;
        TypeGlyph g;
        TypeError err;

        cp = 0;
        adv = type_utf8_next(text + i, len - i, &cp);
        if (adv == 0) {
            if (written)
                *written = n;
            if (bytes_used)
                *bytes_used = i;
            return TYPE_ERR_INVALID_UTF8;
        }
        if (n >= cap) {
            if (written)
                *written = n;
            if (bytes_used)
                *bytes_used = i;
            return TYPE_ERR_BUF_TOO_SMALL;
        }
        err = type_glyph(ctx, cp, size_px, &g);
        if (err != TYPE_OK) {
            if (written)
                *written = n;
            if (bytes_used)
                *bytes_used = i;
            return err;
        }
        out[n].glyph = g;
        out[n].x = x;
        out[n].y = 0.0f;
        out[n].codepoint = cp;
        x += g.advance;
        n++;
        i += adv;
    }
    if (written)
        *written = n;
    if (bytes_used)
        *bytes_used = i;
    return TYPE_OK;
}

TypeWidth type_width(uint32_t codepoint)
{
    (void)codepoint;
    return TYPE_WIDTH_NEUTRAL;
}

int type_is_cjk(uint32_t codepoint)
{
    (void)codepoint;
    return 0;
}

int type_is_emoji(uint32_t codepoint)
{
    (void)codepoint;
    return 0;
}

int type_cell_width(uint32_t codepoint)
{
    (void)codepoint;
    return 1;
}

int type_is_combining(uint32_t codepoint)
{
    (void)codepoint;
    return 0;
}

#endif /* TYPE_IMPLEMENTATION && !TYPE_IMPLEMENTATION_ONCE */

/*
------------------------------------------------------------------------------
Copyright (c) 2025-2026 Vasco Alves

Project-owned portions: GNU General Public License version 3 only
(GPL-3.0-only) OR a separately negotiated paid commercial license.
Free under GPLv3. Commercial licenses available for proprietary applications.

You may redistribute and/or modify the project-owned portions under GPLv3.
They are provided WITHOUT ANY WARRANTY; without even the implied warranty
of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See LICENSE-GPL-3.0.txt for the full GPLv3 text, or
https://www.gnu.org/licenses/gpl-3.0.html if it was not supplied.
See LICENSE and COMMERCIAL-LICENSE.md in https://github.com/godxgun/godstack
for scope and commercial inquiries. This notice is not a commercial agreement.
Third-party portions retain their own notices and licenses.
Previously released versions retain their existing licenses.
------------------------------------------------------------------------------
*/
