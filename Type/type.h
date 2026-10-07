/* ===========================================================================
 * TYPE - Fast font rendering and utf8 parsing - Copyright (c) 2025-2026 Vasco Alves
 * See LICENSE file for license info.
 *
 * UTF-8 in, glyphs and metrics out. Multiple faces, fallbacks, weight,
 * style, ligatures, CJK, and emoji are in scope. Port of velocitty src/type.zig.
 *
 * FEATURES:
 * - Take either codepoints or UTF-8 bytes. Layout validates UTF-8, then
 *   walks codepoints.
 * - Load sfnt fonts (TrueType glyf, or color CBDT/CBLC) and rasterize into
 *   an atlas:
 *      - Coverage bitmaps with box-filter antialiasing.
 *      - Color emoji into a separate RGBA atlas.
 *      - Glyph metrics, advance, and bearings.
 *      - Signed-distance bitmaps are declared and return TYPE_ERR_UNIMPLEMENTED.
 * - East Asian Width (UAX #11) and emoji (UAX #51) for layout.
 *      - BMP: property table painted from the UAX ranges.
 *      - Plane 1: compact page map (emoji / kana).
 *      - Planes 2+: binary search on the same ranges (CJK unified).
 *
 * ANTI-FEATURES:
 * - Does not allocate. Call the memory function, then place into your buffer.
 * - Does not read files. Font bytes must outlive every face that points at them.
 * - Does not grow the face list, the fallback list, or the glyph cache.
 *   Sizes are fixed when the context is placed.
 *
 * PREFIX: TYPE_ (macros)  Type (types)  type_ (functions)
 *
 * USAGE:
 *     #include "type.h"
 *
 *     TypeParams params;
 *     type_params_default(&params);
 *     unsigned char *buf = ...; // type_memory(&params) bytes
 *     TypeCtx *type = type_place(buf, type_memory(&params), &params);
 *     uint32_t id;
 *     type_add_font(type, bytes, len, NULL, &id);
 *     TypeGlyph g;
 *     type_glyph(type, 'A', 16.0f, &g);
 *
 * =========================================================================== */

#ifndef TYPE_H
#define TYPE_H

#define TYPE_MAJOR 0
#define TYPE_MINOR 1
#define TYPE_PATCH 0

/* CHANGE LOG
 * 0.1.0 - @vasco - public API from velocitty type.zig; caller-provided memory
 */

#include <stddef.h>
#include <stdint.h>

#define TYPE_DEFAULT_ATLAS_WIDTH  1024u
#define TYPE_DEFAULT_ATLAS_HEIGHT 1024u
#define TYPE_DEFAULT_CACHE        2048u
#define TYPE_DEFAULT_MAX_FACES    8u
#define TYPE_DEFAULT_MAX_FALLBACK 8u

#define TYPE_ASCII_LO    32u
#define TYPE_ASCII_HI    126u
#define TYPE_ASCII_N     (TYPE_ASCII_HI - TYPE_ASCII_LO + 1u)
#define TYPE_REPLACEMENT 0xFFFDu

/* GlyphKey.flags. Bold is bit 0, italic is bit 1. */
#define TYPE_STYLE_BOLD    1u
#define TYPE_STYLE_ITALIC  2u

typedef struct TypeTTF TypeTTF;     /* Parsed TrueType (glyf) file. t_ttf.c */
typedef struct TypeOTF TypeOTF;     /* Parsed OpenType / CFF file. t_otf.c */
typedef struct TypeFont TypeFont;   /* Face view: parsed file + weight + style. Borrows the file. */
typedef struct TypeAtlas TypeAtlas; /* Shelf-packed coverage atlas. */
typedef struct TypeAtlasRgba TypeAtlasRgba;
typedef struct TypeCtx TypeCtx;     /* Faces, fallbacks, atlases, glyph LRU. */

typedef enum {
	TYPE_OK = 0,
	TYPE_ERR_INVALID_FONT,
	TYPE_ERR_UNSUPPORTED_TABLE,
	TYPE_ERR_GLYPH_NOT_FOUND,
	TYPE_ERR_ATLAS_FULL,
	TYPE_ERR_INVALID_UTF8,
	TYPE_ERR_UNIMPLEMENTED,
	TYPE_ERR_BUF_TOO_SMALL,
} TypeError;

typedef enum {
	TYPE_AA_NONE = 0, /* binary coverage */
	TYPE_AA_BOX = 1,  /* box-filter antialiasing */
	TYPE_AA_SDF = 2,  /* signed distance; raster returns TYPE_ERR_UNIMPLEMENTED */
} TypeAntiAlias;

/* CSS-ish usWeightClass. Any u16 is legal; these are the named stops. */
typedef enum {
	TYPE_WEIGHT_THIN        = 100,
	TYPE_WEIGHT_EXTRA_LIGHT = 200,
	TYPE_WEIGHT_LIGHT       = 300,
	TYPE_WEIGHT_REGULAR     = 400,
	TYPE_WEIGHT_MEDIUM      = 500,
	TYPE_WEIGHT_SEMI_BOLD   = 600,
	TYPE_WEIGHT_BOLD        = 700,
	TYPE_WEIGHT_EXTRA_BOLD  = 800,
	TYPE_WEIGHT_BLACK       = 900,
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

/* weight 0 means "unset": bold style selects TYPE_WEIGHT_BOLD, otherwise REGULAR. */
typedef struct TypeFaceParams {
	uint16_t weight;
	TypeStyle style;
} TypeFaceParams;

/*
 * atlas_* and cache_capacity must be > 0 (type_params_default fills the
 * velocitty defaults). max_faces and max_fallbacks bound storage inside the
 * context buffer. ligatures is stored for shaping; this API does not apply
 * GSUB yet. color_emoji 0 drops CBDT/CBLC on add. antialias is TypeAntiAlias.
 */
typedef struct TypeParams {
	uint32_t atlas_width;
	uint32_t atlas_height;
	uint32_t cache_capacity;
	uint32_t max_faces;
	uint32_t max_fallbacks;
	uint8_t  ligatures;
	uint8_t  color_emoji;
	uint8_t  antialias;
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
	int16_t  bearing_x;
	int16_t  bearing_y;
	uint16_t advance;
	uint8_t *pixels;
	uint8_t  color;
} TypeBitmap;

typedef struct TypeGlyph {
	uint32_t font_id;
	uint16_t glyph_id;
	float    advance;
	float    bearing_x;
	float    bearing_y;
	uint16_t width;
	uint16_t height;
	uint16_t atlas_x;
	uint16_t atlas_y;
	uint8_t  color; /* 0 coverage atlas, 1 color atlas */
} TypeGlyph;

typedef struct TypeCell {
	TypeGlyph glyph;
	float x;
	float y;
	uint32_t codepoint;
} TypeCell;

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


void type_params_default(TypeParams *params);           /* 1024^2 atlas, 2048 cache, 8 faces, ligatures, color emoji, box AA. */
void type_face_params_default(TypeFaceParams *face);    /* weight unset, roman. */

/* ---------------------------------------------------------------------------
 * Parsed files. Bytes are borrowed for the life of the placed object.
 * t_ttf.c / t_otf.c
 * ------------------------------------------------------------------------- */

size_t type_ttf_memory(void);
size_t type_otf_memory(void);

TypeTTF *type_ttf_place(void *buf, size_t bufsize, const void *bytes, size_t len); /* NULL on short buf or invalid font. */
TypeOTF *type_otf_place(void *buf, size_t bufsize, const void *bytes, size_t len);

TypeError type_ttf_error(const TypeTTF *font); /* TYPE_OK, or why place failed. place(NULL buf) is not required. */
TypeError type_otf_error(const TypeOTF *font);

int type_ttf_outline(const TypeTTF *font);     /* 1 when glyf outlines are usable. */
int type_otf_outline(const TypeOTF *font);
int type_ttf_color(const TypeTTF *font);       /* 1 when a CBDT/CBLC face was parsed. */
int type_otf_color(const TypeOTF *font);

/* ---------------------------------------------------------------------------
 * Face. Borrows the placed file; does not copy glyph bytes.
 * ------------------------------------------------------------------------- */

size_t type_font_memory(void);

TypeFont *type_font_from_ttf(void *buf, size_t bufsize, const TypeTTF *ttf, const TypeFaceParams *face);
TypeFont *type_font_from_otf(void *buf, size_t bufsize, const TypeOTF *otf, const TypeFaceParams *face);

uint16_t type_font_weight(const TypeFont *font);
TypeStyle type_font_style(const TypeFont *font);

TypeError type_font_metrics(const TypeFont *font, float size_px, TypeMetrics *out);
TypeError type_font_glyph_index(const TypeFont *font, uint32_t codepoint, uint16_t *out_gid); /* TYPE_ERR_GLYPH_NOT_FOUND when unmapped. .notdef is not a hit. */
TypeError type_font_advance(const TypeFont *font, uint16_t glyph_id, uint16_t *out_fu);       /* font units */
int type_font_has_drawable(const TypeFont *font, uint16_t glyph_id);                          /* 1 outline or color bitmap */

/* Fill everything except pixels. Use type_bitmap_bytes to size the caller buffer. */
TypeError type_font_bitmap_info(const TypeFont *font, uint16_t glyph_id, float size_px, TypeBitmap *out);
size_t type_bitmap_bytes(const TypeBitmap *info); /* width*height, or *4 when color */

/* pixels must hold type_bitmap_bytes of the info at this size. Empty glyphs
 * (width or height 0) succeed and leave pixels untouched. */
TypeError type_font_render_bitmap(const TypeFont *font, uint16_t glyph_id, float size_px, TypeAntiAlias aa, void *pixels, size_t len, TypeBitmap *out);
TypeError type_font_render_sdf(const TypeFont *font, uint16_t glyph_id, float size_px, void *pixels, size_t len, TypeBitmap *out);

/* ---------------------------------------------------------------------------
 * Shelf atlas. One byte per pixel, or one u32 (straight RGBA8) for color.
 * Storage is width*height samples inside the same buffer as the header.
 * ------------------------------------------------------------------------- */

size_t type_atlas_memory(uint32_t width, uint32_t height);
size_t type_atlas_rgba_memory(uint32_t width, uint32_t height);

TypeAtlas *type_atlas_place(void *buf, size_t bufsize, uint32_t width, uint32_t height);
TypeAtlasRgba *type_atlas_rgba_place(void *buf, size_t bufsize, uint32_t width, uint32_t height);

const uint8_t *type_atlas_pixels(const TypeAtlas *atlas, uint32_t *width, uint32_t *height);
const uint32_t *type_atlas_rgba_pixels(const TypeAtlasRgba *atlas, uint32_t *width, uint32_t *height);

int  type_atlas_pack(TypeAtlas *atlas, uint32_t w, uint32_t h, TypeRect *out);           /* 1 placed, 0 does not fit */
int  type_atlas_rgba_pack(TypeAtlasRgba *atlas, uint32_t w, uint32_t h, TypeRect *out);
void type_atlas_blit(TypeAtlas *atlas, const TypeRect *rect, const uint8_t *src);       /* src is w*h coverage */
void type_atlas_rgba_blit(TypeAtlasRgba *atlas, const TypeRect *rect, const uint8_t *src); /* src is w*h*4 */
void type_atlas_clear(TypeAtlas *atlas);
void type_atlas_rgba_clear(TypeAtlasRgba *atlas);

/*
 * Context reserves memory for all our needs,
 * based on the parameters we pass to it.
 *
 * NOTE(vasco): First added face becomes primary. A bold+italic face is remembered
 * as bold-italic; a bold face as bold; an italic face as italic. Later adds
 * of the same kind replace that slot. Fallback order is the set_fallbacks list.
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
TypeCtx *type_place(void *buf, size_t bufsize, const TypeParams *params);

TypeError type_add_font(TypeCtx *ctx, const void *bytes, size_t len, const TypeFaceParams *face, uint32_t *out_id);
void type_clear_fonts(TypeCtx *ctx); /* Drops faces. Font bytes may be freed after this returns. */
void type_select(TypeCtx *ctx, uint32_t font_id);
TypeError type_set_fallbacks(TypeCtx *ctx, const uint32_t *ids, uint32_t count);

/* Coverage pixel revision; cache hits/packing alone do not change it.
 * Non-consuming: each GPU consumer keeps its own last uploaded revision. */
uint64_t type_ctx_atlas_revision(const TypeCtx *ctx);
const uint8_t *type_ctx_atlas(const TypeCtx *ctx, uint32_t *width, uint32_t *height);
const uint32_t *type_ctx_color_atlas(const TypeCtx *ctx, uint32_t *width, uint32_t *height);

TypeError type_metrics(const TypeCtx *ctx, float size_px, TypeMetrics *out);

/* Cache probe only. Does not rasterize, does not move the LRU, does not bump stats.
 * TYPE_ERR_GLYPH_NOT_FOUND when the size does not match the ASCII cache or the key is cold. */
TypeError type_peek_glyph(const TypeCtx *ctx, uint32_t codepoint, float size_px, TypeGlyph *out);
TypeError type_peek_glyph_styled(const TypeCtx *ctx, uint32_t codepoint, float size_px, TypeStyle style, TypeGlyph *out);
TypeError type_glyph(TypeCtx *ctx, uint32_t codepoint, float size_px, TypeGlyph *out);
TypeError type_glyph_styled(TypeCtx *ctx, uint32_t codepoint, float size_px, TypeStyle style, TypeGlyph *out);
TypeError type_warm_ascii(TypeCtx *ctx, float size_px);

/* Clear both atlases and the LRU, then warm ASCII at the last size. */
void type_clear_atlas(TypeCtx *ctx);

/* One line at baseline y = 0. x advances by glyph.advance. No wrap, no bidi.
 * On TYPE_ERR_BUF_TOO_SMALL, *written is cap and *bytes_used is the next
 * unconsumed byte. On TYPE_ERR_INVALID_UTF8, *bytes_used is the bad byte and
 * cells before it are kept. Both out-counts may be NULL. */
TypeError type_layout_utf8(TypeCtx *ctx, const char *text, size_t len, float size_px, TypeCell *out, uint32_t cap, uint32_t *written, size_t *bytes_used);

void type_stats(const TypeCtx *ctx, TypeGlyphStats *out);

/* ---------------------------------------------------------------------------
 * UTF-8. type_utf8_next returns the byte length, or 0 when the sequence is
 * short or illegal (*codepoint is unchanged).
 * ------------------------------------------------------------------------- */

uint32_t type_utf8_next(const char *s, size_t len, uint32_t *codepoint);
int      type_utf8_validate(const char *s, size_t len); /* 1 valid, 0 not */

/* ---------------------------------------------------------------------------
 * East Asian width and emoji class. cell width is 1 or 2.
 * ------------------------------------------------------------------------- */

TypeWidth type_width(uint32_t codepoint);
int type_is_cjk(uint32_t codepoint);
int type_is_emoji(uint32_t codepoint);
int type_cell_width(uint32_t codepoint);
int type_is_combining(uint32_t codepoint);

#endif /* TYPE_H */
