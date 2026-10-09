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
 *     #include "Type.h"
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
 * Type.h implementation
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

#if defined(TYPE_IMPLEMENTATION) && !defined(TYPE_IMPLEMENTATION_ONCE)
#define TYPE_IMPLEMENTATION_ONCE
/* t_ttf.c - TrueType glyf faces. Copyright (c) 2025-2026 Vasco Alves
 * Raster is stb_truetype. Shape scratch is a fixed buffer, reset each call.
 * Font bytes are borrowed for the life of the placed face.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>


static unsigned char ttf_scratch[1 << 20];
static size_t ttf_scratch_used;

static void *ttf_scratch_alloc(size_t size, void *user);
static void ttf_scratch_free(void *p, void *user);
static void ttf_scratch_reset(void);
static int16_t ttf_clamp_i16(int v);
static uint16_t ttf_upem(const struct TypeTTF *font);

static void *
ttf_scratch_alloc(size_t size, void *user)
{
	size_t n;
	void *p;

	(void)user;
	n = (size + 15u) & ~(size_t)15u;
	if (n < size || ttf_scratch_used + n > sizeof ttf_scratch)
		return NULL;
	p = ttf_scratch + ttf_scratch_used;
	ttf_scratch_used += n;
	return p;
}

static void
ttf_scratch_free(void *p, void *user)
{
	(void)p;
	(void)user;
}

static void
ttf_scratch_reset(void)
{
	ttf_scratch_used = 0;
}

#ifndef STBTT_malloc
#define STBTT_malloc(size, user) ttf_scratch_alloc(size, user)
#define TYPE_DEFINED_STBTT_MALLOC
#endif
#ifndef STBTT_free
#define STBTT_free(p, user) ttf_scratch_free(p, user)
#define TYPE_DEFINED_STBTT_FREE
#endif
#ifndef STBTT_STATIC
#define STBTT_STATIC
#define TYPE_DEFINED_STBTT_STATIC
#endif
#ifndef STB_TRUETYPE_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION
#define TYPE_DEFINED_STB_TRUETYPE_IMPLEMENTATION
#endif
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
// stb_truetype.h v1.26 public domain. Sean Barrett / RAD Game Tools 2009-2021.
//
// =======================================================================
//    NO SECURITY GUARANTEE -- DO NOT USE THIS ON UNTRUSTED FONT FILES
// This library does no range checking of the offsets found in the file,
// meaning an attacker can use it to read arbitrary memory.
// =======================================================================
//
// Parse TrueType. Extract glyph metrics + shapes. Render 1-channel AA bitmap
// (box filter) or 1-channel SDF. No range check on file offsets.
//
// Todo: non-MS cmap, crashproof bad data, hinting, ClearType AA, temp-pool
// alloc, edge list from curves, rasterize from curves.
//
// Contributors: Mikko Mononen (compound shape, more cmap). Tor Andersson
// (kerning, subpixel). Dougall Johnson (OpenType / Type 2). Daniel Ribeiro
// Maciel (GPOS kerning). Also Ryan Gordon, Simon Glass, IntellectualKitty,
// Imanol Celaya. Many bug reports.
//
// 1.26 fix rasterizer. 1.25 many fixes. 1.23 SVG + full kern table (kern not
// GPOS). 1.22 less missing-glyph dup; GPOS+kern both present. 1.20 PackFontRange
// skip missing; GetScaleFontVMetrics. 1.19 GPOS kern, STBTT_fmod. 1.16 SDF.
// 1.14 TTC font count. 1.13 OpenType + some Apple. 1.07 sparse PackFontRanges +
// pack/render split; GetFontOffsetForIndex non-0. 1.06 new rasterizer ~35%
// faster, better AA except overlap overestimate. 1.00 PackBegin + oversample.
// 0.4 kerning + subpixel. 0.3 cmap 12 + compound. 0.1 first release 2009-03-09.
//
// USAGE
// One translation unit: #define STB_TRUETYPE_IMPLEMENTATION before include.
// Private impl: #define STBTT_STATIC.
//
// Bake (tools only): stbtt_BakeFontBitmap, stbtt_GetBakedQuad.
// Pack (ship): include stb_rect_pack.h; PackBegin, PackSetOversampling,
// PackFontRanges, PackEnd, GetPackedQuad.
// Load from memory (keep buffer live): InitFont, GetFontOffsetForIndex,
// GetNumberOfFonts.
// Bitmap: GetCodepointBitmap (alloc), MakeCodepointBitmap (caller buf),
// GetCodepointBitmapBox.
// Metrics: GetCodepointHMetrics, GetFontVMetrics, GetFontVMetricsOS2,
// GetCodepointKernAdvance.
//
// Rasterizer since 1.06: faster, better coverage AA. Overlap shapes
// overestimate AA. Old rasterizer: #define STBTT_RASTERIZER_VERSION 1 (~15%
// slower).
//
// Terms: codepoint = Unicode scalar. Glyph = shape. Glyph index = font-local
// id. Baseline = bottom of caps; shapes go above + below. Current point =
// glyph origin; y = baseline (baked fonts too). V metrics: GetFontVMetrics.
// Prefer pixel height of vertical extent. Point API exists (72 pt/in). Screen
// DPI not inches; Win 96 px/in so 1 pt = 1.333 px. TTF point scale often
// wrong on non-commercial fonts.
//
// Scale: ScaleForPixelHeight or ScaleForMappingEmToPixels -> SF.
// Baseline: GetFontBoundingBox; SF*-y0 = worst rise. Top of screen y=0 =>
// baseline = SF*-y0.
// Current point: first glyph may extend left. Pad, or use bbox / LSB.
// Draw: bbox relative to <current_point, baseline>. Rect
// <cp+SF*x0, base+SF*y0> .. <cp+SF*x1, base+SF*y1>.
// Advance: GlyphHMetrics; current_point += SF * advance.
//
// Quality: *Subpixel for AA unhinted text. Not on baked. Kerning worth it
// with subpixel.
// Speed: convert codepoint -> glyph index once. Many mallocs; no temp-pool
// yet.
//
// Raw big-endian TTF, no aux tables. LE pay swap. Cache bitmaps/shapes.
// Font-name sniff hard; API exists, do not trust.
//
// 1.06 perf (32/64): old 8.83/7.68s, pool 7.72/6.34, inline sort 6.54/5.65,
// new rasterizer 5.63/5.00.
//
// SAMPLES (below, #if 0)
// Bake 32..126 into 512^2, GetBakedQuad, OpenGL quads. y-down, origin top-left.
// GetBakedQuad last arg 1=GL/D3D10+, 0=D3D9. Bake may not fit. Complete
// truetype_demo_win32.c.
// GetCodepointBitmap ASCII art of one glyph.
// "Hello World!" banner: pad left; MakeCodepointBitmap stomps overlap (lj
// wrong). Bake API not for sequences; blit + alpha blend from temp.
//
// INTEGRATION (define before include)
// stbtt_uint8/16/32, stbtt_int8/16/32. STBTT_ifloor / STBTT_iceil (else
// math.h). STBTT_malloc / STBTT_free (else malloc.h).
//
// BAKE API
// stbtt_bakedchar: bbox in bitmap. BakeFontBitmap(data, offset=0 for .ttf,
// pixel_height, bitmap, first_char, num_chars, chardata you alloc). Return
// >0 first unused row; <0 = -chars that fit; 0 nothing. Crappy packer.
// GetBakedQuad(chardata, pw, ph, char_index='c'-first, &x,&y, &q, opengl_fill).
// y down. Glyphs above + below current. Inefficient; copy if hot.
// GetScaledFontVMetrics: v metrics, no font object.
//
// PACK API
// Multi-font atlas, better pack. PackBegin(spc, pixels, w, h, stride_or_0,
// padding, alloc_ctx): 1-channel w*h. padding 1 for bilinear. 0 fail, 1 ok.
// PackEnd frees. PackFontRange: font_index (0 if unknown), num chars from
// first_unicode, write chardata_for_range for GetPackedQuad. font_size =
// ascender-descender = ScaleForPixelHeight. Point size:
// STBTT_POINT_SIZE(20) = 'M' 20 px; bare 20 = max-min y 20 px.
// pack_range: continuous first codepoint or sparse array; output chardata;
// rest internal.
// PackFontRanges better than many PackFontRange. Repeat inside one
// PackBegin/End.
// PackSetOversampling(h,v) for later PackFontRange(s) / GatherRects. Default
// 1x1. Cost h*v pixels (2x2 = 4x). Bilinear. Set before GatherRects.
// PackSetSkipMissingCodepoints(skip): skip!=0 drop missing; skip=0 (default)
// use missing glyph (empty box).
// GatherRects + PackRects + RenderIntoRects ~= PackFontRanges. Custom: gather
// many, pack once, render many. May pack better. stbtt_pack_context opaque
// PackBegin..PackEnd.
//
// LOAD
// GetNumberOfFonts: TTC many, TTF one. -1 error. Index 0..n-1.
// GetFontOffsetForIndex: -1 OOR. Plain TTF: 0 at index 0 else -1.
// stbtt_fontinfo: declare stack/global, treat opaque. Holds .ttf ptr, font
// start, glyph count, table offs, cmap, glyph-map format, CFF / charstring /
// subr / fontdict. InitFont(info, data, offset) fills value data, no free.
// 0 fail.
//
// CODEPOINT -> GLYPH
// FindGlyphIndex then glyph APIs. 0 = undefined.
//
// PROPERTIES
// ScaleForPixelHeight: scale = pixels / (ascent - descent). Or ascent-only
// same idea. ScaleForMappingEmToPixels: EM -> pixels (classic API, maybe).
// GetFontVMetrics: ascent above baseline, descent below (usually neg),
// lineGap between rows. Advance y by ascent - descent + lineGap. Unscaled;
// * scale. GetFontVMetricsOS2: OS/2 typographic (MS TTF). 1 table present,
// 0 fail. GetFontBoundingBox: all glyphs. GetCodepointHMetrics: LSB = x to
// left edge; advanceWidth = x to next. Unscaled. GetCodepointKernAdvance:
// extra between ch1,ch2. GetCodepointBox: visible bbox unscaled.
// Glyph* twins take glyph index. GetKerningTable: write <= table_length,
// return count. Sort (g1,g2).
//
// SHAPES (before bitmaps for C order)
// Override STBTT_vmove/vline/vcurve values if you must. Can't use stbtt_int16
// in header. IsGlyphEmpty !=0 if nothing drawn. GetCodepointShape /
// GetGlyphShape: vertex count + *vertices, unscaled. Contours: STBTT_moveto
// then lineto / curveto. lineto prev->(x,y). curveto quadratic, control
// (cx,cy). FreeShape. GetCodepointSVG / GetGlyphSVG: size or 0.
//
// BITMAP
// FreeBitmap. GetCodepointBitmap: alloc 8bpp 1-ch AA. 0 clear, 255 cover.
// L->R, T->B. xoff/yoff = origin to bitmap top-left. *Subpixel adds shift.
// MakeCodepointBitmap: caller output, out_stride, clip out_w/out_h. Box
// first. Make*Subpixel. Make*SubpixelPrefilter (PackSetOversampling).
// GetCodepointBitmapBox: ix1-ix0 w, iy1-iy0 h; place top-left
// (LSB*scale, iy0). Bitmap y-down, shape y-up: Box inverted vs CodepointBox.
// *Subpixel twin. Glyph* twins for index.
// stbtt_Rasterize: quadratic shape into 1-ch bitmap. flatness_in_pixels,
// verts, count, scale, translate, extra translate, invert y, malloc ctx.
//
// SDF
// FreeSDF. Discrete SDF, 1-ch tex, bilinear, threshold = scalable font.
// scale same as bitmap. padding = extra dist pixels (outlines).
// onedge_value 0-255 isocontour. pixel_dist_scale = SDF step per pixel;
// +ve: > onedge inside; -ve: < onedge inside. Out w/h include pad. xoff/yoff
// origin. Return 0..255 w*h. Clamp outside 0..255.
// Ex: ScaleForPixelHeight(22), pad 5, onedge 180, dist 180/5=36. Glyph ~22,
// bitmap ~32. Fill if sample >= 180/255. Outside 3 px: (180-36*3)/255=72/255.
// Maps 5 px out .. 2 px in to 0..255. Analytic per pixel, not hi-res approx.
// Slow. Unoptimized.
//
// FONT NAME (avoid; do offline tables)
// Names many encodings + langs. Case-fold underspecified + huge.
// FindMatchingFont: case-sensitive Unicode names, before InitFont. Return
// offset or -1. STBTT_MACSTYLE_DONTCARE + "Arial Bold"; else "Arial" +
// macStyle bitfield (inconsistent). macStyle 0 != DONTCARE; checks bits==0.
// GetFontNameString after InitFont. CompareUTF8toUTF16_bigendian: utf8 vs
// BE utf16. Name string may be BE double-byte. IDs: Apple TTRefMan Ch6 name,
// MS otspec/name. platformID / encodingID (Unicode, MS, Mac=Script Manager)
// / languageID (MS=LCID, many EN/AR; Mac langs).
//
// IMPLEMENTATION notes (code below)
// stbtt__buf parse helpers. ALLOW_UNALIGNED_TRUETYPE if unpadded TTF on
// strict-align. Versions: TrueType 1, TT+Type1 unsupported, OpenType CFF,
// OT 1.0, Apple TT. TTC v1 multi-font. Required tables vs optional. CFF/Type2
// only (no Type1). First name INDEX. CID fonts. cmap: MS/Unicode, Mac/iOS
// Unicode. fmt 0 Apple byte; high-byte CJK TODO; fmt 4 Win binary range;
// fmt 12 group binary. Unknown glyph map = fail. Shape: two-pass flags/x/y,
// off-curve start interpolates on-curve. Compound: XY, scale, xy-scale,
// 2x2; match-point TODO. CFF charstring: ignore init width if hmtx. Hinting
// TODO. hintmask/cntrmask, h/vstem(+hm), r/h/vmoveto, r/h/vlineto,
// hv/vh/rr/vv/hhcurveto, rcurveline/rlinecurve, callsubr/callgsubr, return,
// endchar, flex/hflex/flex1/hflex1 (ignore flex-depth/res, always bezier).
// Charstring run twice (count then emit). kern/GPOS: first table, horizontal
// fmt 0. GPOS pair adjust major1 minor0. Class 0 = unassigned. Unimplemented
// GPOS format: define STBTT_assert. No kern table: skip both cmap lookups.
//
// Rasterizer: integral bbox, pixel = square. v1: subsample, active edges,
// non-zero winding, AA at x0/x1, clip off-edge (bad bbox / small dest).
// v2: direct AA, no supersample. Edge clip brute if out of dest (x_top /
// x_bottom extrapolate). Up to 2 intersections / pixel; split on L/R not
// T/B. Sort: median-of-3, equality for sentinels. vsubsample | 255 for
// full opacity. Horizontal edges skipped. Tessellate to flatness; 65536
// segs cap. y-up shape, y-down bitmap invert. XOR winding.
//
// Bake packer SUPER-CRAPPY (small). No stb_rect_pack.h: replacement +
// COMPILER WARNING if symbols twice -- include stb_rect_pack.h first.
// With stb_rect_pack: SUPER-AWESOME (Ryan Gordon). Prefilter = box width
// oversample, phase (oversample-1)/2; shift opposite. Restore oversample
// after range. Flag unpacked chars.
//
// SDF: cubic/quad/line dist. Ray (-inf,y)->(x,y) winding. Nudge y off
// vertices. Coarse bbox cull. Outside = negative. a=0 linear, a_inv=0 quad.
//
// Name match: utf8 prefix vs utf16; Unicode encodings only; family then
// subfamily same enc/lang; macStyle italic/bold/underline; if macStyle
// checked, family only.
//
// If #include "stb_rect_pack.h" after this file, duplicate-symbol warning:
// include packer first.
//
// ------------------------------------------------------------------------------
// This software is available under 2 licenses -- choose whichever you prefer.
// ------------------------------------------------------------------------------
// ALTERNATIVE A - MIT License
// Copyright (c) 2017 Sean Barrett
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
// of the Software, and to permit persons to whom the Software is furnished to do
// so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
// ------------------------------------------------------------------------------
// ALTERNATIVE B - Public Domain (www.unlicense.org)
// This is free and unencumbered software released into the public domain.
// Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
// software, either in source code form or as a compiled binary, for any purpose,
// commercial or non-commercial, and by any means.
// In jurisdictions that recognize copyright laws, the author or authors of this
// software dedicate any and all copyright interest in the software to the public
// domain. We make this dedication for the benefit of the public at large and to
// the detriment of our heirs and successors. We intend this dedication to be an
// overt act of relinquishment in perpetuity of all present and future rights to
// this software under copyright law.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
// ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// ------------------------------------------------------------------------------

#if 0
#define STB_TRUETYPE_IMPLEMENTATION  
#include "stb_truetype.h"

unsigned char ttf_buffer[1<<20];
unsigned char temp_bitmap[512*512];

stbtt_bakedchar cdata[96]; 
GLuint ftex;

void my_stbtt_initfont(void)
{
   fread(ttf_buffer, 1, 1<<20, fopen("c:/windows/fonts/times.ttf", "rb"));
   stbtt_BakeFontBitmap(ttf_buffer,0, 32.0, temp_bitmap,512,512, 32,96, cdata); 
   
   glGenTextures(1, &ftex);
   glBindTexture(GL_TEXTURE_2D, ftex);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, 512,512, 0, GL_ALPHA, GL_UNSIGNED_BYTE, temp_bitmap);
   
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
}

void my_stbtt_print(float x, float y, char *text)
{
   
   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glEnable(GL_TEXTURE_2D);
   glBindTexture(GL_TEXTURE_2D, ftex);
   glBegin(GL_QUADS);
   while (*text) {
      if (*text >= 32 && *text < 128) {
         stbtt_aligned_quad q;
         stbtt_GetBakedQuad(cdata, 512,512, *text-32, &x,&y,&q,1);
         glTexCoord2f(q.s0,q.t0); glVertex2f(q.x0,q.y0);
         glTexCoord2f(q.s1,q.t0); glVertex2f(q.x1,q.y0);
         glTexCoord2f(q.s1,q.t1); glVertex2f(q.x1,q.y1);
         glTexCoord2f(q.s0,q.t1); glVertex2f(q.x0,q.y1);
      }
      ++text;
   }
   glEnd();
}
#endif

#if 0
#include <stdio.h>
#define STB_TRUETYPE_IMPLEMENTATION  
#include "stb_truetype.h"

char ttf_buffer[1<<25];

int main(int argc, char **argv)
{
   stbtt_fontinfo font;
   unsigned char *bitmap;
   int w,h,i,j,c = (argc > 1 ? atoi(argv[1]) : 'a'), s = (argc > 2 ? atoi(argv[2]) : 20);

   fread(ttf_buffer, 1, 1<<25, fopen(argc > 3 ? argv[3] : "c:/windows/fonts/arialbd.ttf", "rb"));

   stbtt_InitFont(&font, ttf_buffer, stbtt_GetFontOffsetForIndex(ttf_buffer,0));
   bitmap = stbtt_GetCodepointBitmap(&font, 0,stbtt_ScaleForPixelHeight(&font, s), c, &w, &h, 0,0);

   for (j=0; j < h; ++j) {
      for (i=0; i < w; ++i)
         putchar(" .:ioVM@"[bitmap[j*w+i]>>5]);
      putchar('\n');
   }
   return 0;
}
#endif

#if 0
char buffer[24<<20];
unsigned char screen[20][79];

int main(int arg, char **argv)
{
   stbtt_fontinfo font;
   int i,j,ascent,baseline,ch=0;
   float scale, xpos=2; 
   char *text = "Heljo World!"; 

   fread(buffer, 1, 1000000, fopen("c:/windows/fonts/arialbd.ttf", "rb"));
   stbtt_InitFont(&font, buffer, 0);

   scale = stbtt_ScaleForPixelHeight(&font, 15);
   stbtt_GetFontVMetrics(&font, &ascent,0,0);
   baseline = (int) (ascent*scale);

   while (text[ch]) {
      int advance,lsb,x0,y0,x1,y1;
      float x_shift = xpos - (float) floor(xpos);
      stbtt_GetCodepointHMetrics(&font, text[ch], &advance, &lsb);
      stbtt_GetCodepointBitmapBoxSubpixel(&font, text[ch], scale,scale,x_shift,0, &x0,&y0,&x1,&y1);
      stbtt_MakeCodepointBitmapSubpixel(&font, &screen[baseline + y0][(int) xpos + x0], x1-x0,y1-y0, 79, scale,scale,x_shift,0, text[ch]);
      
      
      
      
      xpos += (advance * scale);
      if (text[ch+1])
         xpos += scale*stbtt_GetCodepointKernAdvance(&font, text[ch],text[ch+1]);
      ++ch;
   }

   for (j=0; j < 20; ++j) {
      for (i=0; i < 78; ++i)
         putchar(" .:ioVM@"[screen[j][i]>>5]);
      putchar('\n');
   }

   return 0;
}
#endif

#ifdef STB_TRUETYPE_IMPLEMENTATION
   
   #ifndef stbtt_uint8
   typedef unsigned char   stbtt_uint8;
   typedef signed   char   stbtt_int8;
   typedef unsigned short  stbtt_uint16;
   typedef signed   short  stbtt_int16;
   typedef unsigned int    stbtt_uint32;
   typedef signed   int    stbtt_int32;
   #endif

   typedef char stbtt__check_size32[sizeof(stbtt_int32)==4 ? 1 : -1];
   typedef char stbtt__check_size16[sizeof(stbtt_int16)==2 ? 1 : -1];

   
   #ifndef STBTT_ifloor
   #include <math.h>
   #define STBTT_ifloor(x)   ((int) floor(x))
   #define STBTT_iceil(x)    ((int) ceil(x))
   #endif

   #ifndef STBTT_sqrt
   #include <math.h>
   #define STBTT_sqrt(x)      sqrt(x)
   #define STBTT_pow(x,y)     pow(x,y)
   #endif

   #ifndef STBTT_fmod
   #include <math.h>
   #define STBTT_fmod(x,y)    fmod(x,y)
   #endif

   #ifndef STBTT_cos
   #include <math.h>
   #define STBTT_cos(x)       cos(x)
   #define STBTT_acos(x)      acos(x)
   #endif

   #ifndef STBTT_fabs
   #include <math.h>
   #define STBTT_fabs(x)      fabs(x)
   #endif

   
   #ifndef STBTT_malloc
   #include <stdlib.h>
   #define STBTT_malloc(x,u)  ((void)(u),malloc(x))
   #define STBTT_free(x,u)    ((void)(u),free(x))
   #endif

   #ifndef STBTT_assert
   #include <assert.h>
   #define STBTT_assert(x)    assert(x)
   #endif

   #ifndef STBTT_strlen
   #include <string.h>
   #define STBTT_strlen(x)    strlen(x)
   #endif

   #ifndef STBTT_memcpy
   #include <string.h>
   #define STBTT_memcpy       memcpy
   #define STBTT_memset       memset
   #endif
#endif

#ifndef __STB_INCLUDE_STB_TRUETYPE_H__
#define __STB_INCLUDE_STB_TRUETYPE_H__

#ifdef STBTT_STATIC
#define STBTT_DEF static
#else
#define STBTT_DEF extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
   unsigned char *data;
   int cursor;
   int size;
} stbtt__buf;

typedef struct
{
   unsigned short x0,y0,x1,y1; 
   float xoff,yoff,xadvance;
} stbtt_bakedchar;

STBTT_DEF int stbtt_BakeFontBitmap(const unsigned char *data, int offset,  
                                float pixel_height,                     
                                unsigned char *pixels, int pw, int ph,  
                                int first_char, int num_chars,          
                                stbtt_bakedchar *chardata);             

typedef struct
{
   float x0,y0,s0,t0; 
   float x1,y1,s1,t1; 
} stbtt_aligned_quad;

STBTT_DEF void stbtt_GetBakedQuad(const stbtt_bakedchar *chardata, int pw, int ph,  
                               int char_index,             
                               float *xpos, float *ypos,   
                               stbtt_aligned_quad *q,      
                               int opengl_fillrule);       

STBTT_DEF void stbtt_GetScaledFontVMetrics(const unsigned char *fontdata, int index, float size, float *ascent, float *descent, float *lineGap);

typedef struct
{
   unsigned short x0,y0,x1,y1; 
   float xoff,yoff,xadvance;
   float xoff2,yoff2;
} stbtt_packedchar;

typedef struct stbtt_pack_context stbtt_pack_context;
typedef struct stbtt_fontinfo stbtt_fontinfo;
#ifndef STB_RECT_PACK_VERSION
typedef struct stbrp_rect stbrp_rect;
#endif

STBTT_DEF int  stbtt_PackBegin(stbtt_pack_context *spc, unsigned char *pixels, int width, int height, int stride_in_bytes, int padding, void *alloc_context);

STBTT_DEF void stbtt_PackEnd  (stbtt_pack_context *spc);

#define STBTT_POINT_SIZE(x)   (-(x))

STBTT_DEF int  stbtt_PackFontRange(stbtt_pack_context *spc, const unsigned char *fontdata, int font_index, float font_size,
                                int first_unicode_char_in_range, int num_chars_in_range, stbtt_packedchar *chardata_for_range);

typedef struct
{
   float font_size;
   int first_unicode_codepoint_in_range;  
   int *array_of_unicode_codepoints;       
   int num_chars;
   stbtt_packedchar *chardata_for_range; 
   unsigned char h_oversample, v_oversample; 
} stbtt_pack_range;

STBTT_DEF int  stbtt_PackFontRanges(stbtt_pack_context *spc, const unsigned char *fontdata, int font_index, stbtt_pack_range *ranges, int num_ranges);

STBTT_DEF void stbtt_PackSetOversampling(stbtt_pack_context *spc, unsigned int h_oversample, unsigned int v_oversample);

STBTT_DEF void stbtt_PackSetSkipMissingCodepoints(stbtt_pack_context *spc, int skip);

STBTT_DEF void stbtt_GetPackedQuad(const stbtt_packedchar *chardata, int pw, int ph,  
                               int char_index,             
                               float *xpos, float *ypos,   
                               stbtt_aligned_quad *q,      
                               int align_to_integer);

STBTT_DEF int  stbtt_PackFontRangesGatherRects(stbtt_pack_context *spc, const stbtt_fontinfo *info, stbtt_pack_range *ranges, int num_ranges, stbrp_rect *rects);
STBTT_DEF void stbtt_PackFontRangesPackRects(stbtt_pack_context *spc, stbrp_rect *rects, int num_rects);
STBTT_DEF int  stbtt_PackFontRangesRenderIntoRects(stbtt_pack_context *spc, const stbtt_fontinfo *info, stbtt_pack_range *ranges, int num_ranges, stbrp_rect *rects);

struct stbtt_pack_context {
   void *user_allocator_context;
   void *pack_info;
   int   width;
   int   height;
   int   stride_in_bytes;
   int   padding;
   int   skip_missing;
   unsigned int   h_oversample, v_oversample;
   unsigned char *pixels;
   void  *nodes;
};

STBTT_DEF int stbtt_GetNumberOfFonts(const unsigned char *data);

STBTT_DEF int stbtt_GetFontOffsetForIndex(const unsigned char *data, int index);

struct stbtt_fontinfo
{
   void           * userdata;
   unsigned char  * data;              
   int              fontstart;         

   int numGlyphs;                     

   int loca,head,glyf,hhea,hmtx,kern,gpos,svg; 
   int index_map;                     
   int indexToLocFormat;              

   stbtt__buf cff;                    
   stbtt__buf charstrings;            
   stbtt__buf gsubrs;                 
   stbtt__buf subrs;                  
   stbtt__buf fontdicts;              
   stbtt__buf fdselect;               
};

STBTT_DEF int stbtt_InitFont(stbtt_fontinfo *info, const unsigned char *data, int offset);

STBTT_DEF int stbtt_FindGlyphIndex(const stbtt_fontinfo *info, int unicode_codepoint);

STBTT_DEF float stbtt_ScaleForPixelHeight(const stbtt_fontinfo *info, float pixels);

STBTT_DEF float stbtt_ScaleForMappingEmToPixels(const stbtt_fontinfo *info, float pixels);

STBTT_DEF void stbtt_GetFontVMetrics(const stbtt_fontinfo *info, int *ascent, int *descent, int *lineGap);

STBTT_DEF int  stbtt_GetFontVMetricsOS2(const stbtt_fontinfo *info, int *typoAscent, int *typoDescent, int *typoLineGap);

STBTT_DEF void stbtt_GetFontBoundingBox(const stbtt_fontinfo *info, int *x0, int *y0, int *x1, int *y1);

STBTT_DEF void stbtt_GetCodepointHMetrics(const stbtt_fontinfo *info, int codepoint, int *advanceWidth, int *leftSideBearing);

STBTT_DEF int  stbtt_GetCodepointKernAdvance(const stbtt_fontinfo *info, int ch1, int ch2);

STBTT_DEF int stbtt_GetCodepointBox(const stbtt_fontinfo *info, int codepoint, int *x0, int *y0, int *x1, int *y1);

STBTT_DEF void stbtt_GetGlyphHMetrics(const stbtt_fontinfo *info, int glyph_index, int *advanceWidth, int *leftSideBearing);
STBTT_DEF int  stbtt_GetGlyphKernAdvance(const stbtt_fontinfo *info, int glyph1, int glyph2);
STBTT_DEF int  stbtt_GetGlyphBox(const stbtt_fontinfo *info, int glyph_index, int *x0, int *y0, int *x1, int *y1);

typedef struct stbtt_kerningentry
{
   int glyph1; 
   int glyph2;
   int advance;
} stbtt_kerningentry;

STBTT_DEF int  stbtt_GetKerningTableLength(const stbtt_fontinfo *info);
STBTT_DEF int  stbtt_GetKerningTable(const stbtt_fontinfo *info, stbtt_kerningentry* table, int table_length);

#ifndef STBTT_vmove 
   enum {
      STBTT_vmove=1,
      STBTT_vline,
      STBTT_vcurve,
      STBTT_vcubic
   };
#endif

#ifndef stbtt_vertex 
                   
   #define stbtt_vertex_type short 
   typedef struct
   {
      stbtt_vertex_type x,y,cx,cy,cx1,cy1;
      unsigned char type,padding;
   } stbtt_vertex;
#endif

STBTT_DEF int stbtt_IsGlyphEmpty(const stbtt_fontinfo *info, int glyph_index);

STBTT_DEF int stbtt_GetCodepointShape(const stbtt_fontinfo *info, int unicode_codepoint, stbtt_vertex **vertices);
STBTT_DEF int stbtt_GetGlyphShape(const stbtt_fontinfo *info, int glyph_index, stbtt_vertex **vertices);

STBTT_DEF void stbtt_FreeShape(const stbtt_fontinfo *info, stbtt_vertex *vertices);

STBTT_DEF unsigned char *stbtt_FindSVGDoc(const stbtt_fontinfo *info, int gl);
STBTT_DEF int stbtt_GetCodepointSVG(const stbtt_fontinfo *info, int unicode_codepoint, const char **svg);
STBTT_DEF int stbtt_GetGlyphSVG(const stbtt_fontinfo *info, int gl, const char **svg);

STBTT_DEF void stbtt_FreeBitmap(unsigned char *bitmap, void *userdata);

STBTT_DEF unsigned char *stbtt_GetCodepointBitmap(const stbtt_fontinfo *info, float scale_x, float scale_y, int codepoint, int *width, int *height, int *xoff, int *yoff);

STBTT_DEF unsigned char *stbtt_GetCodepointBitmapSubpixel(const stbtt_fontinfo *info, float scale_x, float scale_y, float shift_x, float shift_y, int codepoint, int *width, int *height, int *xoff, int *yoff);

STBTT_DEF void stbtt_MakeCodepointBitmap(const stbtt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, int codepoint);

STBTT_DEF void stbtt_MakeCodepointBitmapSubpixel(const stbtt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, float shift_x, float shift_y, int codepoint);

STBTT_DEF void stbtt_MakeCodepointBitmapSubpixelPrefilter(const stbtt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, float shift_x, float shift_y, int oversample_x, int oversample_y, float *sub_x, float *sub_y, int codepoint);

STBTT_DEF void stbtt_GetCodepointBitmapBox(const stbtt_fontinfo *font, int codepoint, float scale_x, float scale_y, int *ix0, int *iy0, int *ix1, int *iy1);

STBTT_DEF void stbtt_GetCodepointBitmapBoxSubpixel(const stbtt_fontinfo *font, int codepoint, float scale_x, float scale_y, float shift_x, float shift_y, int *ix0, int *iy0, int *ix1, int *iy1);

STBTT_DEF unsigned char *stbtt_GetGlyphBitmap(const stbtt_fontinfo *info, float scale_x, float scale_y, int glyph, int *width, int *height, int *xoff, int *yoff);
STBTT_DEF unsigned char *stbtt_GetGlyphBitmapSubpixel(const stbtt_fontinfo *info, float scale_x, float scale_y, float shift_x, float shift_y, int glyph, int *width, int *height, int *xoff, int *yoff);
STBTT_DEF void stbtt_MakeGlyphBitmap(const stbtt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, int glyph);
STBTT_DEF void stbtt_MakeGlyphBitmapSubpixel(const stbtt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, float shift_x, float shift_y, int glyph);
STBTT_DEF void stbtt_MakeGlyphBitmapSubpixelPrefilter(const stbtt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, float shift_x, float shift_y, int oversample_x, int oversample_y, float *sub_x, float *sub_y, int glyph);
STBTT_DEF void stbtt_GetGlyphBitmapBox(const stbtt_fontinfo *font, int glyph, float scale_x, float scale_y, int *ix0, int *iy0, int *ix1, int *iy1);
STBTT_DEF void stbtt_GetGlyphBitmapBoxSubpixel(const stbtt_fontinfo *font, int glyph, float scale_x, float scale_y,float shift_x, float shift_y, int *ix0, int *iy0, int *ix1, int *iy1);

typedef struct
{
   int w,h,stride;
   unsigned char *pixels;
} stbtt__bitmap;

STBTT_DEF void stbtt_Rasterize(stbtt__bitmap *result,        
                               float flatness_in_pixels,     
                               stbtt_vertex *vertices,       
                               int num_verts,                
                               float scale_x, float scale_y, 
                               float shift_x, float shift_y, 
                               int x_off, int y_off,         
                               int invert,                   
                               void *userdata);              

STBTT_DEF void stbtt_FreeSDF(unsigned char *bitmap, void *userdata);

STBTT_DEF unsigned char * stbtt_GetGlyphSDF(const stbtt_fontinfo *info, float scale, int glyph, int padding, unsigned char onedge_value, float pixel_dist_scale, int *width, int *height, int *xoff, int *yoff);
STBTT_DEF unsigned char * stbtt_GetCodepointSDF(const stbtt_fontinfo *info, float scale, int codepoint, int padding, unsigned char onedge_value, float pixel_dist_scale, int *width, int *height, int *xoff, int *yoff);

STBTT_DEF int stbtt_FindMatchingFont(const unsigned char *fontdata, const char *name, int flags);

#define STBTT_MACSTYLE_DONTCARE     0
#define STBTT_MACSTYLE_BOLD         1
#define STBTT_MACSTYLE_ITALIC       2
#define STBTT_MACSTYLE_UNDERSCORE   4
#define STBTT_MACSTYLE_NONE         8   

STBTT_DEF int stbtt_CompareUTF8toUTF16_bigendian(const char *s1, int len1, const char *s2, int len2);

STBTT_DEF const char *stbtt_GetFontNameString(const stbtt_fontinfo *font, int *length, int platformID, int encodingID, int languageID, int nameID);

enum { 
   STBTT_PLATFORM_ID_UNICODE   =0,
   STBTT_PLATFORM_ID_MAC       =1,
   STBTT_PLATFORM_ID_ISO       =2,
   STBTT_PLATFORM_ID_MICROSOFT =3
};

enum { 
   STBTT_UNICODE_EID_UNICODE_1_0    =0,
   STBTT_UNICODE_EID_UNICODE_1_1    =1,
   STBTT_UNICODE_EID_ISO_10646      =2,
   STBTT_UNICODE_EID_UNICODE_2_0_BMP=3,
   STBTT_UNICODE_EID_UNICODE_2_0_FULL=4
};

enum { 
   STBTT_MS_EID_SYMBOL        =0,
   STBTT_MS_EID_UNICODE_BMP   =1,
   STBTT_MS_EID_SHIFTJIS      =2,
   STBTT_MS_EID_UNICODE_FULL  =10
};

enum { 
   STBTT_MAC_EID_ROMAN        =0,   STBTT_MAC_EID_ARABIC       =4,
   STBTT_MAC_EID_JAPANESE     =1,   STBTT_MAC_EID_HEBREW       =5,
   STBTT_MAC_EID_CHINESE_TRAD =2,   STBTT_MAC_EID_GREEK        =6,
   STBTT_MAC_EID_KOREAN       =3,   STBTT_MAC_EID_RUSSIAN      =7
};

enum { 
       
   STBTT_MS_LANG_ENGLISH     =0x0409,   STBTT_MS_LANG_ITALIAN     =0x0410,
   STBTT_MS_LANG_CHINESE     =0x0804,   STBTT_MS_LANG_JAPANESE    =0x0411,
   STBTT_MS_LANG_DUTCH       =0x0413,   STBTT_MS_LANG_KOREAN      =0x0412,
   STBTT_MS_LANG_FRENCH      =0x040c,   STBTT_MS_LANG_RUSSIAN     =0x0419,
   STBTT_MS_LANG_GERMAN      =0x0407,   STBTT_MS_LANG_SPANISH     =0x0409,
   STBTT_MS_LANG_HEBREW      =0x040d,   STBTT_MS_LANG_SWEDISH     =0x041D
};

enum { 
   STBTT_MAC_LANG_ENGLISH      =0 ,   STBTT_MAC_LANG_JAPANESE     =11,
   STBTT_MAC_LANG_ARABIC       =12,   STBTT_MAC_LANG_KOREAN       =23,
   STBTT_MAC_LANG_DUTCH        =4 ,   STBTT_MAC_LANG_RUSSIAN      =32,
   STBTT_MAC_LANG_FRENCH       =1 ,   STBTT_MAC_LANG_SPANISH      =6 ,
   STBTT_MAC_LANG_GERMAN       =2 ,   STBTT_MAC_LANG_SWEDISH      =5 ,
   STBTT_MAC_LANG_HEBREW       =10,   STBTT_MAC_LANG_CHINESE_SIMPLIFIED =33,
   STBTT_MAC_LANG_ITALIAN      =3 ,   STBTT_MAC_LANG_CHINESE_TRAD =19
};

#ifdef __cplusplus
}
#endif

#endif 

#ifdef STB_TRUETYPE_IMPLEMENTATION

#ifndef STBTT_MAX_OVERSAMPLE
#define STBTT_MAX_OVERSAMPLE   8
#endif

#if STBTT_MAX_OVERSAMPLE > 255
#error "STBTT_MAX_OVERSAMPLE cannot be > 255"
#endif

typedef int stbtt__test_oversample_pow2[(STBTT_MAX_OVERSAMPLE & (STBTT_MAX_OVERSAMPLE-1)) == 0 ? 1 : -1];

#ifndef STBTT_RASTERIZER_VERSION
#define STBTT_RASTERIZER_VERSION 2
#endif

#ifdef _MSC_VER
#define STBTT__NOTUSED(v)  (void)(v)
#else
#define STBTT__NOTUSED(v)  (void)sizeof(v)
#endif

static stbtt_uint8 stbtt__buf_get8(stbtt__buf *b)
{
   if (b->cursor >= b->size)
      return 0;
   return b->data[b->cursor++];
}

static stbtt_uint8 stbtt__buf_peek8(stbtt__buf *b)
{
   if (b->cursor >= b->size)
      return 0;
   return b->data[b->cursor];
}

static void stbtt__buf_seek(stbtt__buf *b, int o)
{
   STBTT_assert(!(o > b->size || o < 0));
   b->cursor = (o > b->size || o < 0) ? b->size : o;
}

static void stbtt__buf_skip(stbtt__buf *b, int o)
{
   stbtt__buf_seek(b, b->cursor + o);
}

static stbtt_uint32 stbtt__buf_get(stbtt__buf *b, int n)
{
   stbtt_uint32 v = 0;
   int i;
   STBTT_assert(n >= 1 && n <= 4);
   for (i = 0; i < n; i++)
      v = (v << 8) | stbtt__buf_get8(b);
   return v;
}

static stbtt__buf stbtt__new_buf(const void *p, size_t size)
{
   stbtt__buf r;
   STBTT_assert(size < 0x40000000);
   r.data = (stbtt_uint8*) p;
   r.size = (int) size;
   r.cursor = 0;
   return r;
}

#define stbtt__buf_get16(b)  stbtt__buf_get((b), 2)
#define stbtt__buf_get32(b)  stbtt__buf_get((b), 4)

static stbtt__buf stbtt__buf_range(const stbtt__buf *b, int o, int s)
{
   stbtt__buf r = stbtt__new_buf(NULL, 0);
   if (o < 0 || s < 0 || o > b->size || s > b->size - o) return r;
   r.data = b->data + o;
   r.size = s;
   return r;
}

static stbtt__buf stbtt__cff_get_index(stbtt__buf *b)
{
   int count, start, offsize;
   start = b->cursor;
   count = stbtt__buf_get16(b);
   if (count) {
      offsize = stbtt__buf_get8(b);
      STBTT_assert(offsize >= 1 && offsize <= 4);
      stbtt__buf_skip(b, offsize * count);
      stbtt__buf_skip(b, stbtt__buf_get(b, offsize) - 1);
   }
   return stbtt__buf_range(b, start, b->cursor - start);
}

static stbtt_uint32 stbtt__cff_int(stbtt__buf *b)
{
   int b0 = stbtt__buf_get8(b);
   if (b0 >= 32 && b0 <= 246)       return b0 - 139;
   else if (b0 >= 247 && b0 <= 250) return (b0 - 247)*256 + stbtt__buf_get8(b) + 108;
   else if (b0 >= 251 && b0 <= 254) return -(b0 - 251)*256 - stbtt__buf_get8(b) - 108;
   else if (b0 == 28)               return stbtt__buf_get16(b);
   else if (b0 == 29)               return stbtt__buf_get32(b);
   STBTT_assert(0);
   return 0;
}

static void stbtt__cff_skip_operand(stbtt__buf *b) {
   int v, b0 = stbtt__buf_peek8(b);
   STBTT_assert(b0 >= 28);
   if (b0 == 30) {
      stbtt__buf_skip(b, 1);
      while (b->cursor < b->size) {
         v = stbtt__buf_get8(b);
         if ((v & 0xF) == 0xF || (v >> 4) == 0xF)
            break;
      }
   } else {
      stbtt__cff_int(b);
   }
}

static stbtt__buf stbtt__dict_get(stbtt__buf *b, int key)
{
   stbtt__buf_seek(b, 0);
   while (b->cursor < b->size) {
      int start = b->cursor, end, op;
      while (stbtt__buf_peek8(b) >= 28)
         stbtt__cff_skip_operand(b);
      end = b->cursor;
      op = stbtt__buf_get8(b);
      if (op == 12)  op = stbtt__buf_get8(b) | 0x100;
      if (op == key) return stbtt__buf_range(b, start, end-start);
   }
   return stbtt__buf_range(b, 0, 0);
}

static void stbtt__dict_get_ints(stbtt__buf *b, int key, int outcount, stbtt_uint32 *out)
{
   int i;
   stbtt__buf operands = stbtt__dict_get(b, key);
   for (i = 0; i < outcount && operands.cursor < operands.size; i++)
      out[i] = stbtt__cff_int(&operands);
}

static int stbtt__cff_index_count(stbtt__buf *b)
{
   stbtt__buf_seek(b, 0);
   return stbtt__buf_get16(b);
}

static stbtt__buf stbtt__cff_index_get(stbtt__buf b, int i)
{
   int count, offsize, start, end;
   stbtt__buf_seek(&b, 0);
   count = stbtt__buf_get16(&b);
   offsize = stbtt__buf_get8(&b);
   STBTT_assert(i >= 0 && i < count);
   STBTT_assert(offsize >= 1 && offsize <= 4);
   stbtt__buf_skip(&b, i*offsize);
   start = stbtt__buf_get(&b, offsize);
   end = stbtt__buf_get(&b, offsize);
   return stbtt__buf_range(&b, 2+(count+1)*offsize+start, end - start);
}

#define ttBYTE(p)     (* (stbtt_uint8 *) (p))
#define ttCHAR(p)     (* (stbtt_int8 *) (p))
#define ttFixed(p)    ttLONG(p)

static stbtt_uint16 ttUSHORT(stbtt_uint8 *p) { return p[0]*256 + p[1]; }
static stbtt_int16 ttSHORT(stbtt_uint8 *p)   { return p[0]*256 + p[1]; }
static stbtt_uint32 ttULONG(stbtt_uint8 *p)  { return (p[0]<<24) + (p[1]<<16) + (p[2]<<8) + p[3]; }
static stbtt_int32 ttLONG(stbtt_uint8 *p)    { return (p[0]<<24) + (p[1]<<16) + (p[2]<<8) + p[3]; }

#define stbtt_tag4(p,c0,c1,c2,c3) ((p)[0] == (c0) && (p)[1] == (c1) && (p)[2] == (c2) && (p)[3] == (c3))
#define stbtt_tag(p,str)           stbtt_tag4(p,str[0],str[1],str[2],str[3])

static int stbtt__isfont(stbtt_uint8 *font)
{
   
   if (stbtt_tag4(font, '1',0,0,0))  return 1; 
   if (stbtt_tag(font, "typ1"))   return 1; 
   if (stbtt_tag(font, "OTTO"))   return 1; 
   if (stbtt_tag4(font, 0,1,0,0)) return 1; 
   if (stbtt_tag(font, "true"))   return 1; 
   return 0;
}

static stbtt_uint32 stbtt__find_table(stbtt_uint8 *data, stbtt_uint32 fontstart, const char *tag)
{
   stbtt_int32 num_tables = ttUSHORT(data+fontstart+4);
   stbtt_uint32 tabledir = fontstart + 12;
   stbtt_int32 i;
   for (i=0; i < num_tables; ++i) {
      stbtt_uint32 loc = tabledir + 16*i;
      if (stbtt_tag(data+loc+0, tag))
         return ttULONG(data+loc+8);
   }
   return 0;
}

static int stbtt_GetFontOffsetForIndex_internal(unsigned char *font_collection, int index)
{
   
   if (stbtt__isfont(font_collection))
      return index == 0 ? 0 : -1;

   
   if (stbtt_tag(font_collection, "ttcf")) {
      
      if (ttULONG(font_collection+4) == 0x00010000 || ttULONG(font_collection+4) == 0x00020000) {
         stbtt_int32 n = ttLONG(font_collection+8);
         if (index >= n)
            return -1;
         return ttULONG(font_collection+12+index*4);
      }
   }
   return -1;
}

static int stbtt_GetNumberOfFonts_internal(unsigned char *font_collection)
{
   
   if (stbtt__isfont(font_collection))
      return 1;

   
   if (stbtt_tag(font_collection, "ttcf")) {
      
      if (ttULONG(font_collection+4) == 0x00010000 || ttULONG(font_collection+4) == 0x00020000) {
         return ttLONG(font_collection+8);
      }
   }
   return 0;
}

static stbtt__buf stbtt__get_subrs(stbtt__buf cff, stbtt__buf fontdict)
{
   stbtt_uint32 subrsoff = 0, private_loc[2] = { 0, 0 };
   stbtt__buf pdict;
   stbtt__dict_get_ints(&fontdict, 18, 2, private_loc);
   if (!private_loc[1] || !private_loc[0]) return stbtt__new_buf(NULL, 0);
   pdict = stbtt__buf_range(&cff, private_loc[1], private_loc[0]);
   stbtt__dict_get_ints(&pdict, 19, 1, &subrsoff);
   if (!subrsoff) return stbtt__new_buf(NULL, 0);
   stbtt__buf_seek(&cff, private_loc[1]+subrsoff);
   return stbtt__cff_get_index(&cff);
}

static int stbtt__get_svg(stbtt_fontinfo *info)
{
   stbtt_uint32 t;
   if (info->svg < 0) {
      t = stbtt__find_table(info->data, info->fontstart, "SVG ");
      if (t) {
         stbtt_uint32 offset = ttULONG(info->data + t + 2);
         info->svg = t + offset;
      } else {
         info->svg = 0;
      }
   }
   return info->svg;
}

static int stbtt_InitFont_internal(stbtt_fontinfo *info, unsigned char *data, int fontstart)
{
   stbtt_uint32 cmap, t;
   stbtt_int32 i,numTables;

   info->data = data;
   info->fontstart = fontstart;
   info->cff = stbtt__new_buf(NULL, 0);

   cmap = stbtt__find_table(data, fontstart, "cmap");       
   info->loca = stbtt__find_table(data, fontstart, "loca"); 
   info->head = stbtt__find_table(data, fontstart, "head"); 
   info->glyf = stbtt__find_table(data, fontstart, "glyf"); 
   info->hhea = stbtt__find_table(data, fontstart, "hhea"); 
   info->hmtx = stbtt__find_table(data, fontstart, "hmtx"); 
   info->kern = stbtt__find_table(data, fontstart, "kern"); 
   info->gpos = stbtt__find_table(data, fontstart, "GPOS"); 

   if (!cmap || !info->head || !info->hhea || !info->hmtx)
      return 0;
   if (info->glyf) {
      
      if (!info->loca) return 0;
   } else {
      
      stbtt__buf b, topdict, topdictidx;
      stbtt_uint32 cstype = 2, charstrings = 0, fdarrayoff = 0, fdselectoff = 0;
      stbtt_uint32 cff;

      cff = stbtt__find_table(data, fontstart, "CFF ");
      if (!cff) return 0;

      info->fontdicts = stbtt__new_buf(NULL, 0);
      info->fdselect = stbtt__new_buf(NULL, 0);

      
      info->cff = stbtt__new_buf(data+cff, 512*1024*1024);
      b = info->cff;

      
      stbtt__buf_skip(&b, 2);
      stbtt__buf_seek(&b, stbtt__buf_get8(&b)); 

      
      
      stbtt__cff_get_index(&b);  
      topdictidx = stbtt__cff_get_index(&b);
      topdict = stbtt__cff_index_get(topdictidx, 0);
      stbtt__cff_get_index(&b);  
      info->gsubrs = stbtt__cff_get_index(&b);

      stbtt__dict_get_ints(&topdict, 17, 1, &charstrings);
      stbtt__dict_get_ints(&topdict, 0x100 | 6, 1, &cstype);
      stbtt__dict_get_ints(&topdict, 0x100 | 36, 1, &fdarrayoff);
      stbtt__dict_get_ints(&topdict, 0x100 | 37, 1, &fdselectoff);
      info->subrs = stbtt__get_subrs(b, topdict);

      
      if (cstype != 2) return 0;
      if (charstrings == 0) return 0;

      if (fdarrayoff) {
         
         if (!fdselectoff) return 0;
         stbtt__buf_seek(&b, fdarrayoff);
         info->fontdicts = stbtt__cff_get_index(&b);
         info->fdselect = stbtt__buf_range(&b, fdselectoff, b.size-fdselectoff);
      }

      stbtt__buf_seek(&b, charstrings);
      info->charstrings = stbtt__cff_get_index(&b);
   }

   t = stbtt__find_table(data, fontstart, "maxp");
   if (t)
      info->numGlyphs = ttUSHORT(data+t+4);
   else
      info->numGlyphs = 0xffff;

   info->svg = -1;

   
   
   
   numTables = ttUSHORT(data + cmap + 2);
   info->index_map = 0;
   for (i=0; i < numTables; ++i) {
      stbtt_uint32 encoding_record = cmap + 4 + 8 * i;
      
      switch(ttUSHORT(data+encoding_record)) {
         case STBTT_PLATFORM_ID_MICROSOFT:
            switch (ttUSHORT(data+encoding_record+2)) {
               case STBTT_MS_EID_UNICODE_BMP:
               case STBTT_MS_EID_UNICODE_FULL:
                  
                  info->index_map = cmap + ttULONG(data+encoding_record+4);
                  break;
            }
            break;
        case STBTT_PLATFORM_ID_UNICODE:
            
            
            info->index_map = cmap + ttULONG(data+encoding_record+4);
            break;
      }
   }
   if (info->index_map == 0)
      return 0;

   info->indexToLocFormat = ttUSHORT(data+info->head + 50);
   return 1;
}

STBTT_DEF int stbtt_FindGlyphIndex(const stbtt_fontinfo *info, int unicode_codepoint)
{
   stbtt_uint8 *data = info->data;
   stbtt_uint32 index_map = info->index_map;

   stbtt_uint16 format = ttUSHORT(data + index_map + 0);
   if (format == 0) { 
      stbtt_int32 bytes = ttUSHORT(data + index_map + 2);
      if (unicode_codepoint < bytes-6)
         return ttBYTE(data + index_map + 6 + unicode_codepoint);
      return 0;
   } else if (format == 6) {
      stbtt_uint32 first = ttUSHORT(data + index_map + 6);
      stbtt_uint32 count = ttUSHORT(data + index_map + 8);
      if ((stbtt_uint32) unicode_codepoint >= first && (stbtt_uint32) unicode_codepoint < first+count)
         return ttUSHORT(data + index_map + 10 + (unicode_codepoint - first)*2);
      return 0;
   } else if (format == 2) {
      STBTT_assert(0); 
      return 0;
   } else if (format == 4) { 
      stbtt_uint16 segcount = ttUSHORT(data+index_map+6) >> 1;
      stbtt_uint16 searchRange = ttUSHORT(data+index_map+8) >> 1;
      stbtt_uint16 entrySelector = ttUSHORT(data+index_map+10);
      stbtt_uint16 rangeShift = ttUSHORT(data+index_map+12) >> 1;

      
      stbtt_uint32 endCount = index_map + 14;
      stbtt_uint32 search = endCount;

      if (unicode_codepoint > 0xffff)
         return 0;

      
      
      if (unicode_codepoint >= ttUSHORT(data + search + rangeShift*2))
         search += rangeShift*2;

      
      search -= 2;
      while (entrySelector) {
         stbtt_uint16 end;
         searchRange >>= 1;
         end = ttUSHORT(data + search + searchRange*2);
         if (unicode_codepoint > end)
            search += searchRange*2;
         --entrySelector;
      }
      search += 2;

      {
         stbtt_uint16 offset, start, last;
         stbtt_uint16 item = (stbtt_uint16) ((search - endCount) >> 1);

         start = ttUSHORT(data + index_map + 14 + segcount*2 + 2 + 2*item);
         last = ttUSHORT(data + endCount + 2*item);
         if (unicode_codepoint < start || unicode_codepoint > last)
            return 0;

         offset = ttUSHORT(data + index_map + 14 + segcount*6 + 2 + 2*item);
         if (offset == 0)
            return (stbtt_uint16) (unicode_codepoint + ttSHORT(data + index_map + 14 + segcount*4 + 2 + 2*item));

         return ttUSHORT(data + offset + (unicode_codepoint-start)*2 + index_map + 14 + segcount*6 + 2 + 2*item);
      }
   } else if (format == 12 || format == 13) {
      stbtt_uint32 ngroups = ttULONG(data+index_map+12);
      stbtt_int32 low,high;
      low = 0; high = (stbtt_int32)ngroups;
      
      while (low < high) {
         stbtt_int32 mid = low + ((high-low) >> 1); 
         stbtt_uint32 start_char = ttULONG(data+index_map+16+mid*12);
         stbtt_uint32 end_char = ttULONG(data+index_map+16+mid*12+4);
         if ((stbtt_uint32) unicode_codepoint < start_char)
            high = mid;
         else if ((stbtt_uint32) unicode_codepoint > end_char)
            low = mid+1;
         else {
            stbtt_uint32 start_glyph = ttULONG(data+index_map+16+mid*12+8);
            if (format == 12)
               return start_glyph + unicode_codepoint-start_char;
            else 
               return start_glyph;
         }
      }
      return 0; 
   }
   
   STBTT_assert(0);
   return 0;
}

STBTT_DEF int stbtt_GetCodepointShape(const stbtt_fontinfo *info, int unicode_codepoint, stbtt_vertex **vertices)
{
   return stbtt_GetGlyphShape(info, stbtt_FindGlyphIndex(info, unicode_codepoint), vertices);
}

static void stbtt_setvertex(stbtt_vertex *v, stbtt_uint8 type, stbtt_int32 x, stbtt_int32 y, stbtt_int32 cx, stbtt_int32 cy)
{
   v->type = type;
   v->x = (stbtt_int16) x;
   v->y = (stbtt_int16) y;
   v->cx = (stbtt_int16) cx;
   v->cy = (stbtt_int16) cy;
}

static int stbtt__GetGlyfOffset(const stbtt_fontinfo *info, int glyph_index)
{
   int g1,g2;

   STBTT_assert(!info->cff.size);

   if (glyph_index >= info->numGlyphs) return -1; 
   if (info->indexToLocFormat >= 2)    return -1; 

   if (info->indexToLocFormat == 0) {
      g1 = info->glyf + ttUSHORT(info->data + info->loca + glyph_index * 2) * 2;
      g2 = info->glyf + ttUSHORT(info->data + info->loca + glyph_index * 2 + 2) * 2;
   } else {
      g1 = info->glyf + ttULONG (info->data + info->loca + glyph_index * 4);
      g2 = info->glyf + ttULONG (info->data + info->loca + glyph_index * 4 + 4);
   }

   return g1==g2 ? -1 : g1; 
}

static int stbtt__GetGlyphInfoT2(const stbtt_fontinfo *info, int glyph_index, int *x0, int *y0, int *x1, int *y1);

STBTT_DEF int stbtt_GetGlyphBox(const stbtt_fontinfo *info, int glyph_index, int *x0, int *y0, int *x1, int *y1)
{
   if (info->cff.size) {
      stbtt__GetGlyphInfoT2(info, glyph_index, x0, y0, x1, y1);
   } else {
      int g = stbtt__GetGlyfOffset(info, glyph_index);
      if (g < 0) return 0;

      if (x0) *x0 = ttSHORT(info->data + g + 2);
      if (y0) *y0 = ttSHORT(info->data + g + 4);
      if (x1) *x1 = ttSHORT(info->data + g + 6);
      if (y1) *y1 = ttSHORT(info->data + g + 8);
   }
   return 1;
}

STBTT_DEF int stbtt_GetCodepointBox(const stbtt_fontinfo *info, int codepoint, int *x0, int *y0, int *x1, int *y1)
{
   return stbtt_GetGlyphBox(info, stbtt_FindGlyphIndex(info,codepoint), x0,y0,x1,y1);
}

STBTT_DEF int stbtt_IsGlyphEmpty(const stbtt_fontinfo *info, int glyph_index)
{
   stbtt_int16 numberOfContours;
   int g;
   if (info->cff.size)
      return stbtt__GetGlyphInfoT2(info, glyph_index, NULL, NULL, NULL, NULL) == 0;
   g = stbtt__GetGlyfOffset(info, glyph_index);
   if (g < 0) return 1;
   numberOfContours = ttSHORT(info->data + g);
   return numberOfContours == 0;
}

static int stbtt__close_shape(stbtt_vertex *vertices, int num_vertices, int was_off, int start_off,
    stbtt_int32 sx, stbtt_int32 sy, stbtt_int32 scx, stbtt_int32 scy, stbtt_int32 cx, stbtt_int32 cy)
{
   if (start_off) {
      if (was_off)
         stbtt_setvertex(&vertices[num_vertices++], STBTT_vcurve, (cx+scx)>>1, (cy+scy)>>1, cx,cy);
      stbtt_setvertex(&vertices[num_vertices++], STBTT_vcurve, sx,sy,scx,scy);
   } else {
      if (was_off)
         stbtt_setvertex(&vertices[num_vertices++], STBTT_vcurve,sx,sy,cx,cy);
      else
         stbtt_setvertex(&vertices[num_vertices++], STBTT_vline,sx,sy,0,0);
   }
   return num_vertices;
}

static int stbtt__GetGlyphShapeTT(const stbtt_fontinfo *info, int glyph_index, stbtt_vertex **pvertices)
{
   stbtt_int16 numberOfContours;
   stbtt_uint8 *endPtsOfContours;
   stbtt_uint8 *data = info->data;
   stbtt_vertex *vertices=0;
   int num_vertices=0;
   int g = stbtt__GetGlyfOffset(info, glyph_index);

   *pvertices = NULL;

   if (g < 0) return 0;

   numberOfContours = ttSHORT(data + g);

   if (numberOfContours > 0) {
      stbtt_uint8 flags=0,flagcount;
      stbtt_int32 ins, i,j=0,m,n, next_move, was_off=0, off, start_off=0;
      stbtt_int32 x,y,cx,cy,sx,sy, scx,scy;
      stbtt_uint8 *points;
      endPtsOfContours = (data + g + 10);
      ins = ttUSHORT(data + g + 10 + numberOfContours * 2);
      points = data + g + 10 + numberOfContours * 2 + 2 + ins;

      n = 1+ttUSHORT(endPtsOfContours + numberOfContours*2-2);

      m = n + 2*numberOfContours;  
      vertices = (stbtt_vertex *) STBTT_malloc(m * sizeof(vertices[0]), info->userdata);
      if (vertices == 0)
         return 0;

      next_move = 0;
      flagcount=0;

      
      
      

      off = m - n; 

      

      for (i=0; i < n; ++i) {
         if (flagcount == 0) {
            flags = *points++;
            if (flags & 8)
               flagcount = *points++;
         } else
            --flagcount;
         vertices[off+i].type = flags;
      }

      
      x=0;
      for (i=0; i < n; ++i) {
         flags = vertices[off+i].type;
         if (flags & 2) {
            stbtt_int16 dx = *points++;
            x += (flags & 16) ? dx : -dx; 
         } else {
            if (!(flags & 16)) {
               x = x + (stbtt_int16) (points[0]*256 + points[1]);
               points += 2;
            }
         }
         vertices[off+i].x = (stbtt_int16) x;
      }

      
      y=0;
      for (i=0; i < n; ++i) {
         flags = vertices[off+i].type;
         if (flags & 4) {
            stbtt_int16 dy = *points++;
            y += (flags & 32) ? dy : -dy; 
         } else {
            if (!(flags & 32)) {
               y = y + (stbtt_int16) (points[0]*256 + points[1]);
               points += 2;
            }
         }
         vertices[off+i].y = (stbtt_int16) y;
      }

      
      num_vertices=0;
      sx = sy = cx = cy = scx = scy = 0;
      for (i=0; i < n; ++i) {
         flags = vertices[off+i].type;
         x     = (stbtt_int16) vertices[off+i].x;
         y     = (stbtt_int16) vertices[off+i].y;

         if (next_move == i) {
            if (i != 0)
               num_vertices = stbtt__close_shape(vertices, num_vertices, was_off, start_off, sx,sy,scx,scy,cx,cy);

            
            start_off = !(flags & 1);
            if (start_off) {
               
               
               scx = x;
               scy = y;
               if (!(vertices[off+i+1].type & 1)) {
                  
                  sx = (x + (stbtt_int32) vertices[off+i+1].x) >> 1;
                  sy = (y + (stbtt_int32) vertices[off+i+1].y) >> 1;
               } else {
                  
                  sx = (stbtt_int32) vertices[off+i+1].x;
                  sy = (stbtt_int32) vertices[off+i+1].y;
                  ++i; 
               }
            } else {
               sx = x;
               sy = y;
            }
            stbtt_setvertex(&vertices[num_vertices++], STBTT_vmove,sx,sy,0,0);
            was_off = 0;
            next_move = 1 + ttUSHORT(endPtsOfContours+j*2);
            ++j;
         } else {
            if (!(flags & 1)) { 
               if (was_off) 
                  stbtt_setvertex(&vertices[num_vertices++], STBTT_vcurve, (cx+x)>>1, (cy+y)>>1, cx, cy);
               cx = x;
               cy = y;
               was_off = 1;
            } else {
               if (was_off)
                  stbtt_setvertex(&vertices[num_vertices++], STBTT_vcurve, x,y, cx, cy);
               else
                  stbtt_setvertex(&vertices[num_vertices++], STBTT_vline, x,y,0,0);
               was_off = 0;
            }
         }
      }
      num_vertices = stbtt__close_shape(vertices, num_vertices, was_off, start_off, sx,sy,scx,scy,cx,cy);
   } else if (numberOfContours < 0) {
      
      int more = 1;
      stbtt_uint8 *comp = data + g + 10;
      num_vertices = 0;
      vertices = 0;
      while (more) {
         stbtt_uint16 flags, gidx;
         int comp_num_verts = 0, i;
         stbtt_vertex *comp_verts = 0, *tmp = 0;
         float mtx[6] = {1,0,0,1,0,0}, m, n;

         flags = ttSHORT(comp); comp+=2;
         gidx = ttSHORT(comp); comp+=2;

         if (flags & 2) { 
            if (flags & 1) { 
               mtx[4] = ttSHORT(comp); comp+=2;
               mtx[5] = ttSHORT(comp); comp+=2;
            } else {
               mtx[4] = ttCHAR(comp); comp+=1;
               mtx[5] = ttCHAR(comp); comp+=1;
            }
         }
         else {
            
            STBTT_assert(0);
         }
         if (flags & (1<<3)) { 
            mtx[0] = mtx[3] = ttSHORT(comp)/16384.0f; comp+=2;
            mtx[1] = mtx[2] = 0;
         } else if (flags & (1<<6)) { 
            mtx[0] = ttSHORT(comp)/16384.0f; comp+=2;
            mtx[1] = mtx[2] = 0;
            mtx[3] = ttSHORT(comp)/16384.0f; comp+=2;
         } else if (flags & (1<<7)) { 
            mtx[0] = ttSHORT(comp)/16384.0f; comp+=2;
            mtx[1] = ttSHORT(comp)/16384.0f; comp+=2;
            mtx[2] = ttSHORT(comp)/16384.0f; comp+=2;
            mtx[3] = ttSHORT(comp)/16384.0f; comp+=2;
         }

         
         m = (float) STBTT_sqrt(mtx[0]*mtx[0] + mtx[1]*mtx[1]);
         n = (float) STBTT_sqrt(mtx[2]*mtx[2] + mtx[3]*mtx[3]);

         
         comp_num_verts = stbtt_GetGlyphShape(info, gidx, &comp_verts);
         if (comp_num_verts > 0) {
            
            for (i = 0; i < comp_num_verts; ++i) {
               stbtt_vertex* v = &comp_verts[i];
               stbtt_vertex_type x,y;
               x=v->x; y=v->y;
               v->x = (stbtt_vertex_type)(m * (mtx[0]*x + mtx[2]*y + mtx[4]));
               v->y = (stbtt_vertex_type)(n * (mtx[1]*x + mtx[3]*y + mtx[5]));
               x=v->cx; y=v->cy;
               v->cx = (stbtt_vertex_type)(m * (mtx[0]*x + mtx[2]*y + mtx[4]));
               v->cy = (stbtt_vertex_type)(n * (mtx[1]*x + mtx[3]*y + mtx[5]));
            }
            
            tmp = (stbtt_vertex*)STBTT_malloc((num_vertices+comp_num_verts)*sizeof(stbtt_vertex), info->userdata);
            if (!tmp) {
               if (vertices) STBTT_free(vertices, info->userdata);
               if (comp_verts) STBTT_free(comp_verts, info->userdata);
               return 0;
            }
            if (num_vertices > 0 && vertices) STBTT_memcpy(tmp, vertices, num_vertices*sizeof(stbtt_vertex));
            STBTT_memcpy(tmp+num_vertices, comp_verts, comp_num_verts*sizeof(stbtt_vertex));
            if (vertices) STBTT_free(vertices, info->userdata);
            vertices = tmp;
            STBTT_free(comp_verts, info->userdata);
            num_vertices += comp_num_verts;
         }
         
         more = flags & (1<<5);
      }
   } else {
      
   }

   *pvertices = vertices;
   return num_vertices;
}

typedef struct
{
   int bounds;
   int started;
   float first_x, first_y;
   float x, y;
   stbtt_int32 min_x, max_x, min_y, max_y;

   stbtt_vertex *pvertices;
   int num_vertices;
} stbtt__csctx;

#define STBTT__CSCTX_INIT(bounds) {bounds,0, 0,0, 0,0, 0,0,0,0, NULL, 0}

static void stbtt__track_vertex(stbtt__csctx *c, stbtt_int32 x, stbtt_int32 y)
{
   if (x > c->max_x || !c->started) c->max_x = x;
   if (y > c->max_y || !c->started) c->max_y = y;
   if (x < c->min_x || !c->started) c->min_x = x;
   if (y < c->min_y || !c->started) c->min_y = y;
   c->started = 1;
}

static void stbtt__csctx_v(stbtt__csctx *c, stbtt_uint8 type, stbtt_int32 x, stbtt_int32 y, stbtt_int32 cx, stbtt_int32 cy, stbtt_int32 cx1, stbtt_int32 cy1)
{
   if (c->bounds) {
      stbtt__track_vertex(c, x, y);
      if (type == STBTT_vcubic) {
         stbtt__track_vertex(c, cx, cy);
         stbtt__track_vertex(c, cx1, cy1);
      }
   } else {
      stbtt_setvertex(&c->pvertices[c->num_vertices], type, x, y, cx, cy);
      c->pvertices[c->num_vertices].cx1 = (stbtt_int16) cx1;
      c->pvertices[c->num_vertices].cy1 = (stbtt_int16) cy1;
   }
   c->num_vertices++;
}

static void stbtt__csctx_close_shape(stbtt__csctx *ctx)
{
   if (ctx->first_x != ctx->x || ctx->first_y != ctx->y)
      stbtt__csctx_v(ctx, STBTT_vline, (int)ctx->first_x, (int)ctx->first_y, 0, 0, 0, 0);
}

static void stbtt__csctx_rmove_to(stbtt__csctx *ctx, float dx, float dy)
{
   stbtt__csctx_close_shape(ctx);
   ctx->first_x = ctx->x = ctx->x + dx;
   ctx->first_y = ctx->y = ctx->y + dy;
   stbtt__csctx_v(ctx, STBTT_vmove, (int)ctx->x, (int)ctx->y, 0, 0, 0, 0);
}

static void stbtt__csctx_rline_to(stbtt__csctx *ctx, float dx, float dy)
{
   ctx->x += dx;
   ctx->y += dy;
   stbtt__csctx_v(ctx, STBTT_vline, (int)ctx->x, (int)ctx->y, 0, 0, 0, 0);
}

static void stbtt__csctx_rccurve_to(stbtt__csctx *ctx, float dx1, float dy1, float dx2, float dy2, float dx3, float dy3)
{
   float cx1 = ctx->x + dx1;
   float cy1 = ctx->y + dy1;
   float cx2 = cx1 + dx2;
   float cy2 = cy1 + dy2;
   ctx->x = cx2 + dx3;
   ctx->y = cy2 + dy3;
   stbtt__csctx_v(ctx, STBTT_vcubic, (int)ctx->x, (int)ctx->y, (int)cx1, (int)cy1, (int)cx2, (int)cy2);
}

static stbtt__buf stbtt__get_subr(stbtt__buf idx, int n)
{
   int count = stbtt__cff_index_count(&idx);
   int bias = 107;
   if (count >= 33900)
      bias = 32768;
   else if (count >= 1240)
      bias = 1131;
   n += bias;
   if (n < 0 || n >= count)
      return stbtt__new_buf(NULL, 0);
   return stbtt__cff_index_get(idx, n);
}

static stbtt__buf stbtt__cid_get_glyph_subrs(const stbtt_fontinfo *info, int glyph_index)
{
   stbtt__buf fdselect = info->fdselect;
   int nranges, start, end, v, fmt, fdselector = -1, i;

   stbtt__buf_seek(&fdselect, 0);
   fmt = stbtt__buf_get8(&fdselect);
   if (fmt == 0) {
      
      stbtt__buf_skip(&fdselect, glyph_index);
      fdselector = stbtt__buf_get8(&fdselect);
   } else if (fmt == 3) {
      nranges = stbtt__buf_get16(&fdselect);
      start = stbtt__buf_get16(&fdselect);
      for (i = 0; i < nranges; i++) {
         v = stbtt__buf_get8(&fdselect);
         end = stbtt__buf_get16(&fdselect);
         if (glyph_index >= start && glyph_index < end) {
            fdselector = v;
            break;
         }
         start = end;
      }
   }
   if (fdselector == -1) stbtt__new_buf(NULL, 0);
   return stbtt__get_subrs(info->cff, stbtt__cff_index_get(info->fontdicts, fdselector));
}

static int stbtt__run_charstring(const stbtt_fontinfo *info, int glyph_index, stbtt__csctx *c)
{
   int in_header = 1, maskbits = 0, subr_stack_height = 0, sp = 0, v, i, b0;
   int has_subrs = 0, clear_stack;
   float s[48];
   stbtt__buf subr_stack[10], subrs = info->subrs, b;
   float f;

#define STBTT__CSERR(s) (0)

   
   b = stbtt__cff_index_get(info->charstrings, glyph_index);
   while (b.cursor < b.size) {
      i = 0;
      clear_stack = 1;
      b0 = stbtt__buf_get8(&b);
      switch (b0) {
      
      case 0x13: 
      case 0x14: 
         if (in_header)
            maskbits += (sp / 2); 
         in_header = 0;
         stbtt__buf_skip(&b, (maskbits + 7) / 8);
         break;

      case 0x01: 
      case 0x03: 
      case 0x12: 
      case 0x17: 
         maskbits += (sp / 2);
         break;

      case 0x15: 
         in_header = 0;
         if (sp < 2) return STBTT__CSERR("rmoveto stack");
         stbtt__csctx_rmove_to(c, s[sp-2], s[sp-1]);
         break;
      case 0x04: 
         in_header = 0;
         if (sp < 1) return STBTT__CSERR("vmoveto stack");
         stbtt__csctx_rmove_to(c, 0, s[sp-1]);
         break;
      case 0x16: 
         in_header = 0;
         if (sp < 1) return STBTT__CSERR("hmoveto stack");
         stbtt__csctx_rmove_to(c, s[sp-1], 0);
         break;

      case 0x05: 
         if (sp < 2) return STBTT__CSERR("rlineto stack");
         for (; i + 1 < sp; i += 2)
            stbtt__csctx_rline_to(c, s[i], s[i+1]);
         break;

      
      

      case 0x07: 
         if (sp < 1) return STBTT__CSERR("vlineto stack");
         goto vlineto;
      case 0x06: 
         if (sp < 1) return STBTT__CSERR("hlineto stack");
         for (;;) {
            if (i >= sp) break;
            stbtt__csctx_rline_to(c, s[i], 0);
            i++;
      vlineto:
            if (i >= sp) break;
            stbtt__csctx_rline_to(c, 0, s[i]);
            i++;
         }
         break;

      case 0x1F: 
         if (sp < 4) return STBTT__CSERR("hvcurveto stack");
         goto hvcurveto;
      case 0x1E: 
         if (sp < 4) return STBTT__CSERR("vhcurveto stack");
         for (;;) {
            if (i + 3 >= sp) break;
            stbtt__csctx_rccurve_to(c, 0, s[i], s[i+1], s[i+2], s[i+3], (sp - i == 5) ? s[i + 4] : 0.0f);
            i += 4;
      hvcurveto:
            if (i + 3 >= sp) break;
            stbtt__csctx_rccurve_to(c, s[i], 0, s[i+1], s[i+2], (sp - i == 5) ? s[i+4] : 0.0f, s[i+3]);
            i += 4;
         }
         break;

      case 0x08: 
         if (sp < 6) return STBTT__CSERR("rcurveline stack");
         for (; i + 5 < sp; i += 6)
            stbtt__csctx_rccurve_to(c, s[i], s[i+1], s[i+2], s[i+3], s[i+4], s[i+5]);
         break;

      case 0x18: 
         if (sp < 8) return STBTT__CSERR("rcurveline stack");
         for (; i + 5 < sp - 2; i += 6)
            stbtt__csctx_rccurve_to(c, s[i], s[i+1], s[i+2], s[i+3], s[i+4], s[i+5]);
         if (i + 1 >= sp) return STBTT__CSERR("rcurveline stack");
         stbtt__csctx_rline_to(c, s[i], s[i+1]);
         break;

      case 0x19: 
         if (sp < 8) return STBTT__CSERR("rlinecurve stack");
         for (; i + 1 < sp - 6; i += 2)
            stbtt__csctx_rline_to(c, s[i], s[i+1]);
         if (i + 5 >= sp) return STBTT__CSERR("rlinecurve stack");
         stbtt__csctx_rccurve_to(c, s[i], s[i+1], s[i+2], s[i+3], s[i+4], s[i+5]);
         break;

      case 0x1A: 
      case 0x1B: 
         if (sp < 4) return STBTT__CSERR("(vv|hh)curveto stack");
         f = 0.0;
         if (sp & 1) { f = s[i]; i++; }
         for (; i + 3 < sp; i += 4) {
            if (b0 == 0x1B)
               stbtt__csctx_rccurve_to(c, s[i], f, s[i+1], s[i+2], s[i+3], 0.0);
            else
               stbtt__csctx_rccurve_to(c, f, s[i], s[i+1], s[i+2], 0.0, s[i+3]);
            f = 0.0;
         }
         break;

      case 0x0A: 
         if (!has_subrs) {
            if (info->fdselect.size)
               subrs = stbtt__cid_get_glyph_subrs(info, glyph_index);
            has_subrs = 1;
         }
         
      case 0x1D: 
         if (sp < 1) return STBTT__CSERR("call(g|)subr stack");
         v = (int) s[--sp];
         if (subr_stack_height >= 10) return STBTT__CSERR("recursion limit");
         subr_stack[subr_stack_height++] = b;
         b = stbtt__get_subr(b0 == 0x0A ? subrs : info->gsubrs, v);
         if (b.size == 0) return STBTT__CSERR("subr not found");
         b.cursor = 0;
         clear_stack = 0;
         break;

      case 0x0B: 
         if (subr_stack_height <= 0) return STBTT__CSERR("return outside subr");
         b = subr_stack[--subr_stack_height];
         clear_stack = 0;
         break;

      case 0x0E: 
         stbtt__csctx_close_shape(c);
         return 1;

      case 0x0C: { 
         float dx1, dx2, dx3, dx4, dx5, dx6, dy1, dy2, dy3, dy4, dy5, dy6;
         float dx, dy;
         int b1 = stbtt__buf_get8(&b);
         switch (b1) {
         
         
         case 0x22: 
            if (sp < 7) return STBTT__CSERR("hflex stack");
            dx1 = s[0];
            dx2 = s[1];
            dy2 = s[2];
            dx3 = s[3];
            dx4 = s[4];
            dx5 = s[5];
            dx6 = s[6];
            stbtt__csctx_rccurve_to(c, dx1, 0, dx2, dy2, dx3, 0);
            stbtt__csctx_rccurve_to(c, dx4, 0, dx5, -dy2, dx6, 0);
            break;

         case 0x23: 
            if (sp < 13) return STBTT__CSERR("flex stack");
            dx1 = s[0];
            dy1 = s[1];
            dx2 = s[2];
            dy2 = s[3];
            dx3 = s[4];
            dy3 = s[5];
            dx4 = s[6];
            dy4 = s[7];
            dx5 = s[8];
            dy5 = s[9];
            dx6 = s[10];
            dy6 = s[11];
            
            stbtt__csctx_rccurve_to(c, dx1, dy1, dx2, dy2, dx3, dy3);
            stbtt__csctx_rccurve_to(c, dx4, dy4, dx5, dy5, dx6, dy6);
            break;

         case 0x24: 
            if (sp < 9) return STBTT__CSERR("hflex1 stack");
            dx1 = s[0];
            dy1 = s[1];
            dx2 = s[2];
            dy2 = s[3];
            dx3 = s[4];
            dx4 = s[5];
            dx5 = s[6];
            dy5 = s[7];
            dx6 = s[8];
            stbtt__csctx_rccurve_to(c, dx1, dy1, dx2, dy2, dx3, 0);
            stbtt__csctx_rccurve_to(c, dx4, 0, dx5, dy5, dx6, -(dy1+dy2+dy5));
            break;

         case 0x25: 
            if (sp < 11) return STBTT__CSERR("flex1 stack");
            dx1 = s[0];
            dy1 = s[1];
            dx2 = s[2];
            dy2 = s[3];
            dx3 = s[4];
            dy3 = s[5];
            dx4 = s[6];
            dy4 = s[7];
            dx5 = s[8];
            dy5 = s[9];
            dx6 = dy6 = s[10];
            dx = dx1+dx2+dx3+dx4+dx5;
            dy = dy1+dy2+dy3+dy4+dy5;
            if (STBTT_fabs(dx) > STBTT_fabs(dy))
               dy6 = -dy;
            else
               dx6 = -dx;
            stbtt__csctx_rccurve_to(c, dx1, dy1, dx2, dy2, dx3, dy3);
            stbtt__csctx_rccurve_to(c, dx4, dy4, dx5, dy5, dx6, dy6);
            break;

         default:
            return STBTT__CSERR("unimplemented");
         }
      } break;

      default:
         if (b0 != 255 && b0 != 28 && b0 < 32)
            return STBTT__CSERR("reserved operator");

         
         if (b0 == 255) {
            f = (float)(stbtt_int32)stbtt__buf_get32(&b) / 0x10000;
         } else {
            stbtt__buf_skip(&b, -1);
            f = (float)(stbtt_int16)stbtt__cff_int(&b);
         }
         if (sp >= 48) return STBTT__CSERR("push stack overflow");
         s[sp++] = f;
         clear_stack = 0;
         break;
      }
      if (clear_stack) sp = 0;
   }
   return STBTT__CSERR("no endchar");

#undef STBTT__CSERR
}

static int stbtt__GetGlyphShapeT2(const stbtt_fontinfo *info, int glyph_index, stbtt_vertex **pvertices)
{
   
   stbtt__csctx count_ctx = STBTT__CSCTX_INIT(1);
   stbtt__csctx output_ctx = STBTT__CSCTX_INIT(0);
   if (stbtt__run_charstring(info, glyph_index, &count_ctx)) {
      *pvertices = (stbtt_vertex*)STBTT_malloc(count_ctx.num_vertices*sizeof(stbtt_vertex), info->userdata);
      output_ctx.pvertices = *pvertices;
      if (stbtt__run_charstring(info, glyph_index, &output_ctx)) {
         STBTT_assert(output_ctx.num_vertices == count_ctx.num_vertices);
         return output_ctx.num_vertices;
      }
   }
   *pvertices = NULL;
   return 0;
}

static int stbtt__GetGlyphInfoT2(const stbtt_fontinfo *info, int glyph_index, int *x0, int *y0, int *x1, int *y1)
{
   stbtt__csctx c = STBTT__CSCTX_INIT(1);
   int r = stbtt__run_charstring(info, glyph_index, &c);
   if (x0)  *x0 = r ? c.min_x : 0;
   if (y0)  *y0 = r ? c.min_y : 0;
   if (x1)  *x1 = r ? c.max_x : 0;
   if (y1)  *y1 = r ? c.max_y : 0;
   return r ? c.num_vertices : 0;
}

STBTT_DEF int stbtt_GetGlyphShape(const stbtt_fontinfo *info, int glyph_index, stbtt_vertex **pvertices)
{
   if (!info->cff.size)
      return stbtt__GetGlyphShapeTT(info, glyph_index, pvertices);
   else
      return stbtt__GetGlyphShapeT2(info, glyph_index, pvertices);
}

STBTT_DEF void stbtt_GetGlyphHMetrics(const stbtt_fontinfo *info, int glyph_index, int *advanceWidth, int *leftSideBearing)
{
   stbtt_uint16 numOfLongHorMetrics = ttUSHORT(info->data+info->hhea + 34);
   if (glyph_index < numOfLongHorMetrics) {
      if (advanceWidth)     *advanceWidth    = ttSHORT(info->data + info->hmtx + 4*glyph_index);
      if (leftSideBearing)  *leftSideBearing = ttSHORT(info->data + info->hmtx + 4*glyph_index + 2);
   } else {
      if (advanceWidth)     *advanceWidth    = ttSHORT(info->data + info->hmtx + 4*(numOfLongHorMetrics-1));
      if (leftSideBearing)  *leftSideBearing = ttSHORT(info->data + info->hmtx + 4*numOfLongHorMetrics + 2*(glyph_index - numOfLongHorMetrics));
   }
}

STBTT_DEF int  stbtt_GetKerningTableLength(const stbtt_fontinfo *info)
{
   stbtt_uint8 *data = info->data + info->kern;

   
   if (!info->kern)
      return 0;
   if (ttUSHORT(data+2) < 1) 
      return 0;
   if (ttUSHORT(data+8) != 1) 
      return 0;

   return ttUSHORT(data+10);
}

STBTT_DEF int stbtt_GetKerningTable(const stbtt_fontinfo *info, stbtt_kerningentry* table, int table_length)
{
   stbtt_uint8 *data = info->data + info->kern;
   int k, length;

   
   if (!info->kern)
      return 0;
   if (ttUSHORT(data+2) < 1) 
      return 0;
   if (ttUSHORT(data+8) != 1) 
      return 0;

   length = ttUSHORT(data+10);
   if (table_length < length)
      length = table_length;

   for (k = 0; k < length; k++)
   {
      table[k].glyph1 = ttUSHORT(data+18+(k*6));
      table[k].glyph2 = ttUSHORT(data+20+(k*6));
      table[k].advance = ttSHORT(data+22+(k*6));
   }

   return length;
}

static int stbtt__GetGlyphKernInfoAdvance(const stbtt_fontinfo *info, int glyph1, int glyph2)
{
   stbtt_uint8 *data = info->data + info->kern;
   stbtt_uint32 needle, straw;
   int l, r, m;

   
   if (!info->kern)
      return 0;
   if (ttUSHORT(data+2) < 1) 
      return 0;
   if (ttUSHORT(data+8) != 1) 
      return 0;

   l = 0;
   r = ttUSHORT(data+10) - 1;
   needle = glyph1 << 16 | glyph2;
   while (l <= r) {
      m = (l + r) >> 1;
      straw = ttULONG(data+18+(m*6)); 
      if (needle < straw)
         r = m - 1;
      else if (needle > straw)
         l = m + 1;
      else
         return ttSHORT(data+22+(m*6));
   }
   return 0;
}

static stbtt_int32 stbtt__GetCoverageIndex(stbtt_uint8 *coverageTable, int glyph)
{
   stbtt_uint16 coverageFormat = ttUSHORT(coverageTable);
   switch (coverageFormat) {
      case 1: {
         stbtt_uint16 glyphCount = ttUSHORT(coverageTable + 2);

         
         stbtt_int32 l=0, r=glyphCount-1, m;
         int straw, needle=glyph;
         while (l <= r) {
            stbtt_uint8 *glyphArray = coverageTable + 4;
            stbtt_uint16 glyphID;
            m = (l + r) >> 1;
            glyphID = ttUSHORT(glyphArray + 2 * m);
            straw = glyphID;
            if (needle < straw)
               r = m - 1;
            else if (needle > straw)
               l = m + 1;
            else {
               return m;
            }
         }
         break;
      }

      case 2: {
         stbtt_uint16 rangeCount = ttUSHORT(coverageTable + 2);
         stbtt_uint8 *rangeArray = coverageTable + 4;

         
         stbtt_int32 l=0, r=rangeCount-1, m;
         int strawStart, strawEnd, needle=glyph;
         while (l <= r) {
            stbtt_uint8 *rangeRecord;
            m = (l + r) >> 1;
            rangeRecord = rangeArray + 6 * m;
            strawStart = ttUSHORT(rangeRecord);
            strawEnd = ttUSHORT(rangeRecord + 2);
            if (needle < strawStart)
               r = m - 1;
            else if (needle > strawEnd)
               l = m + 1;
            else {
               stbtt_uint16 startCoverageIndex = ttUSHORT(rangeRecord + 4);
               return startCoverageIndex + glyph - strawStart;
            }
         }
         break;
      }

      default: return -1; 
   }

   return -1;
}

static stbtt_int32  stbtt__GetGlyphClass(stbtt_uint8 *classDefTable, int glyph)
{
   stbtt_uint16 classDefFormat = ttUSHORT(classDefTable);
   switch (classDefFormat)
   {
      case 1: {
         stbtt_uint16 startGlyphID = ttUSHORT(classDefTable + 2);
         stbtt_uint16 glyphCount = ttUSHORT(classDefTable + 4);
         stbtt_uint8 *classDef1ValueArray = classDefTable + 6;

         if (glyph >= startGlyphID && glyph < startGlyphID + glyphCount)
            return (stbtt_int32)ttUSHORT(classDef1ValueArray + 2 * (glyph - startGlyphID));
         break;
      }

      case 2: {
         stbtt_uint16 classRangeCount = ttUSHORT(classDefTable + 2);
         stbtt_uint8 *classRangeRecords = classDefTable + 4;

         
         stbtt_int32 l=0, r=classRangeCount-1, m;
         int strawStart, strawEnd, needle=glyph;
         while (l <= r) {
            stbtt_uint8 *classRangeRecord;
            m = (l + r) >> 1;
            classRangeRecord = classRangeRecords + 6 * m;
            strawStart = ttUSHORT(classRangeRecord);
            strawEnd = ttUSHORT(classRangeRecord + 2);
            if (needle < strawStart)
               r = m - 1;
            else if (needle > strawEnd)
               l = m + 1;
            else
               return (stbtt_int32)ttUSHORT(classRangeRecord + 4);
         }
         break;
      }

      default:
         return -1; 
   }

   
   return 0;
}

#define STBTT_GPOS_TODO_assert(x)

static stbtt_int32 stbtt__GetGlyphGPOSInfoAdvance(const stbtt_fontinfo *info, int glyph1, int glyph2)
{
   stbtt_uint16 lookupListOffset;
   stbtt_uint8 *lookupList;
   stbtt_uint16 lookupCount;
   stbtt_uint8 *data;
   stbtt_int32 i, sti;

   if (!info->gpos) return 0;

   data = info->data + info->gpos;

   if (ttUSHORT(data+0) != 1) return 0; 
   if (ttUSHORT(data+2) != 0) return 0; 

   lookupListOffset = ttUSHORT(data+8);
   lookupList = data + lookupListOffset;
   lookupCount = ttUSHORT(lookupList);

   for (i=0; i<lookupCount; ++i) {
      stbtt_uint16 lookupOffset = ttUSHORT(lookupList + 2 + 2 * i);
      stbtt_uint8 *lookupTable = lookupList + lookupOffset;

      stbtt_uint16 lookupType = ttUSHORT(lookupTable);
      stbtt_uint16 subTableCount = ttUSHORT(lookupTable + 4);
      stbtt_uint8 *subTableOffsets = lookupTable + 6;
      if (lookupType != 2) 
         continue;

      for (sti=0; sti<subTableCount; sti++) {
         stbtt_uint16 subtableOffset = ttUSHORT(subTableOffsets + 2 * sti);
         stbtt_uint8 *table = lookupTable + subtableOffset;
         stbtt_uint16 posFormat = ttUSHORT(table);
         stbtt_uint16 coverageOffset = ttUSHORT(table + 2);
         stbtt_int32 coverageIndex = stbtt__GetCoverageIndex(table + coverageOffset, glyph1);
         if (coverageIndex == -1) continue;

         switch (posFormat) {
            case 1: {
               stbtt_int32 l, r, m;
               int straw, needle;
               stbtt_uint16 valueFormat1 = ttUSHORT(table + 4);
               stbtt_uint16 valueFormat2 = ttUSHORT(table + 6);
               if (valueFormat1 == 4 && valueFormat2 == 0) { 
                  stbtt_int32 valueRecordPairSizeInBytes = 2;
                  stbtt_uint16 pairSetCount = ttUSHORT(table + 8);
                  stbtt_uint16 pairPosOffset = ttUSHORT(table + 10 + 2 * coverageIndex);
                  stbtt_uint8 *pairValueTable = table + pairPosOffset;
                  stbtt_uint16 pairValueCount = ttUSHORT(pairValueTable);
                  stbtt_uint8 *pairValueArray = pairValueTable + 2;

                  if (coverageIndex >= pairSetCount) return 0;

                  needle=glyph2;
                  r=pairValueCount-1;
                  l=0;

                  
                  while (l <= r) {
                     stbtt_uint16 secondGlyph;
                     stbtt_uint8 *pairValue;
                     m = (l + r) >> 1;
                     pairValue = pairValueArray + (2 + valueRecordPairSizeInBytes) * m;
                     secondGlyph = ttUSHORT(pairValue);
                     straw = secondGlyph;
                     if (needle < straw)
                        r = m - 1;
                     else if (needle > straw)
                        l = m + 1;
                     else {
                        stbtt_int16 xAdvance = ttSHORT(pairValue + 2);
                        return xAdvance;
                     }
                  }
               } else
                  return 0;
               break;
            }

            case 2: {
               stbtt_uint16 valueFormat1 = ttUSHORT(table + 4);
               stbtt_uint16 valueFormat2 = ttUSHORT(table + 6);
               if (valueFormat1 == 4 && valueFormat2 == 0) { 
                  stbtt_uint16 classDef1Offset = ttUSHORT(table + 8);
                  stbtt_uint16 classDef2Offset = ttUSHORT(table + 10);
                  int glyph1class = stbtt__GetGlyphClass(table + classDef1Offset, glyph1);
                  int glyph2class = stbtt__GetGlyphClass(table + classDef2Offset, glyph2);

                  stbtt_uint16 class1Count = ttUSHORT(table + 12);
                  stbtt_uint16 class2Count = ttUSHORT(table + 14);
                  stbtt_uint8 *class1Records, *class2Records;
                  stbtt_int16 xAdvance;

                  if (glyph1class < 0 || glyph1class >= class1Count) return 0; 
                  if (glyph2class < 0 || glyph2class >= class2Count) return 0; 

                  class1Records = table + 16;
                  class2Records = class1Records + 2 * (glyph1class * class2Count);
                  xAdvance = ttSHORT(class2Records + 2 * glyph2class);
                  return xAdvance;
               } else
                  return 0;
               break;
            }

            default:
               return 0; 
         }
      }
   }

   return 0;
}

STBTT_DEF int  stbtt_GetGlyphKernAdvance(const stbtt_fontinfo *info, int g1, int g2)
{
   int xAdvance = 0;

   if (info->gpos)
      xAdvance += stbtt__GetGlyphGPOSInfoAdvance(info, g1, g2);
   else if (info->kern)
      xAdvance += stbtt__GetGlyphKernInfoAdvance(info, g1, g2);

   return xAdvance;
}

STBTT_DEF int  stbtt_GetCodepointKernAdvance(const stbtt_fontinfo *info, int ch1, int ch2)
{
   if (!info->kern && !info->gpos) 
      return 0;
   return stbtt_GetGlyphKernAdvance(info, stbtt_FindGlyphIndex(info,ch1), stbtt_FindGlyphIndex(info,ch2));
}

STBTT_DEF void stbtt_GetCodepointHMetrics(const stbtt_fontinfo *info, int codepoint, int *advanceWidth, int *leftSideBearing)
{
   stbtt_GetGlyphHMetrics(info, stbtt_FindGlyphIndex(info,codepoint), advanceWidth, leftSideBearing);
}

STBTT_DEF void stbtt_GetFontVMetrics(const stbtt_fontinfo *info, int *ascent, int *descent, int *lineGap)
{
   if (ascent ) *ascent  = ttSHORT(info->data+info->hhea + 4);
   if (descent) *descent = ttSHORT(info->data+info->hhea + 6);
   if (lineGap) *lineGap = ttSHORT(info->data+info->hhea + 8);
}

STBTT_DEF int  stbtt_GetFontVMetricsOS2(const stbtt_fontinfo *info, int *typoAscent, int *typoDescent, int *typoLineGap)
{
   int tab = stbtt__find_table(info->data, info->fontstart, "OS/2");
   if (!tab)
      return 0;
   if (typoAscent ) *typoAscent  = ttSHORT(info->data+tab + 68);
   if (typoDescent) *typoDescent = ttSHORT(info->data+tab + 70);
   if (typoLineGap) *typoLineGap = ttSHORT(info->data+tab + 72);
   return 1;
}

STBTT_DEF void stbtt_GetFontBoundingBox(const stbtt_fontinfo *info, int *x0, int *y0, int *x1, int *y1)
{
   *x0 = ttSHORT(info->data + info->head + 36);
   *y0 = ttSHORT(info->data + info->head + 38);
   *x1 = ttSHORT(info->data + info->head + 40);
   *y1 = ttSHORT(info->data + info->head + 42);
}

STBTT_DEF float stbtt_ScaleForPixelHeight(const stbtt_fontinfo *info, float height)
{
   int fheight = ttSHORT(info->data + info->hhea + 4) - ttSHORT(info->data + info->hhea + 6);
   return (float) height / fheight;
}

STBTT_DEF float stbtt_ScaleForMappingEmToPixels(const stbtt_fontinfo *info, float pixels)
{
   int unitsPerEm = ttUSHORT(info->data + info->head + 18);
   return pixels / unitsPerEm;
}

STBTT_DEF void stbtt_FreeShape(const stbtt_fontinfo *info, stbtt_vertex *v)
{
   STBTT_free(v, info->userdata);
}

STBTT_DEF stbtt_uint8 *stbtt_FindSVGDoc(const stbtt_fontinfo *info, int gl)
{
   int i;
   stbtt_uint8 *data = info->data;
   stbtt_uint8 *svg_doc_list = data + stbtt__get_svg((stbtt_fontinfo *) info);

   int numEntries = ttUSHORT(svg_doc_list);
   stbtt_uint8 *svg_docs = svg_doc_list + 2;

   for(i=0; i<numEntries; i++) {
      stbtt_uint8 *svg_doc = svg_docs + (12 * i);
      if ((gl >= ttUSHORT(svg_doc)) && (gl <= ttUSHORT(svg_doc + 2)))
         return svg_doc;
   }
   return 0;
}

STBTT_DEF int stbtt_GetGlyphSVG(const stbtt_fontinfo *info, int gl, const char **svg)
{
   stbtt_uint8 *data = info->data;
   stbtt_uint8 *svg_doc;

   if (info->svg == 0)
      return 0;

   svg_doc = stbtt_FindSVGDoc(info, gl);
   if (svg_doc != NULL) {
      *svg = (char *) data + info->svg + ttULONG(svg_doc + 4);
      return ttULONG(svg_doc + 8);
   } else {
      return 0;
   }
}

STBTT_DEF int stbtt_GetCodepointSVG(const stbtt_fontinfo *info, int unicode_codepoint, const char **svg)
{
   return stbtt_GetGlyphSVG(info, stbtt_FindGlyphIndex(info, unicode_codepoint), svg);
}

STBTT_DEF void stbtt_GetGlyphBitmapBoxSubpixel(const stbtt_fontinfo *font, int glyph, float scale_x, float scale_y,float shift_x, float shift_y, int *ix0, int *iy0, int *ix1, int *iy1)
{
   int x0=0,y0=0,x1,y1; 
   if (!stbtt_GetGlyphBox(font, glyph, &x0,&y0,&x1,&y1)) {
      
      if (ix0) *ix0 = 0;
      if (iy0) *iy0 = 0;
      if (ix1) *ix1 = 0;
      if (iy1) *iy1 = 0;
   } else {
      
      if (ix0) *ix0 = STBTT_ifloor( x0 * scale_x + shift_x);
      if (iy0) *iy0 = STBTT_ifloor(-y1 * scale_y + shift_y);
      if (ix1) *ix1 = STBTT_iceil ( x1 * scale_x + shift_x);
      if (iy1) *iy1 = STBTT_iceil (-y0 * scale_y + shift_y);
   }
}

STBTT_DEF void stbtt_GetGlyphBitmapBox(const stbtt_fontinfo *font, int glyph, float scale_x, float scale_y, int *ix0, int *iy0, int *ix1, int *iy1)
{
   stbtt_GetGlyphBitmapBoxSubpixel(font, glyph, scale_x, scale_y,0.0f,0.0f, ix0, iy0, ix1, iy1);
}

STBTT_DEF void stbtt_GetCodepointBitmapBoxSubpixel(const stbtt_fontinfo *font, int codepoint, float scale_x, float scale_y, float shift_x, float shift_y, int *ix0, int *iy0, int *ix1, int *iy1)
{
   stbtt_GetGlyphBitmapBoxSubpixel(font, stbtt_FindGlyphIndex(font,codepoint), scale_x, scale_y,shift_x,shift_y, ix0,iy0,ix1,iy1);
}

STBTT_DEF void stbtt_GetCodepointBitmapBox(const stbtt_fontinfo *font, int codepoint, float scale_x, float scale_y, int *ix0, int *iy0, int *ix1, int *iy1)
{
   stbtt_GetCodepointBitmapBoxSubpixel(font, codepoint, scale_x, scale_y,0.0f,0.0f, ix0,iy0,ix1,iy1);
}

typedef struct stbtt__hheap_chunk
{
   struct stbtt__hheap_chunk *next;
} stbtt__hheap_chunk;

typedef struct stbtt__hheap
{
   struct stbtt__hheap_chunk *head;
   void   *first_free;
   int    num_remaining_in_head_chunk;
} stbtt__hheap;

static void *stbtt__hheap_alloc(stbtt__hheap *hh, size_t size, void *userdata)
{
   if (hh->first_free) {
      void *p = hh->first_free;
      hh->first_free = * (void **) p;
      return p;
   } else {
      if (hh->num_remaining_in_head_chunk == 0) {
         int count = (size < 32 ? 2000 : size < 128 ? 800 : 100);
         stbtt__hheap_chunk *c = (stbtt__hheap_chunk *) STBTT_malloc(sizeof(stbtt__hheap_chunk) + size * count, userdata);
         if (c == NULL)
            return NULL;
         c->next = hh->head;
         hh->head = c;
         hh->num_remaining_in_head_chunk = count;
      }
      --hh->num_remaining_in_head_chunk;
      return (char *) (hh->head) + sizeof(stbtt__hheap_chunk) + size * hh->num_remaining_in_head_chunk;
   }
}

static void stbtt__hheap_free(stbtt__hheap *hh, void *p)
{
   *(void **) p = hh->first_free;
   hh->first_free = p;
}

static void stbtt__hheap_cleanup(stbtt__hheap *hh, void *userdata)
{
   stbtt__hheap_chunk *c = hh->head;
   while (c) {
      stbtt__hheap_chunk *n = c->next;
      STBTT_free(c, userdata);
      c = n;
   }
}

typedef struct stbtt__edge {
   float x0,y0, x1,y1;
   int invert;
} stbtt__edge;

typedef struct stbtt__active_edge
{
   struct stbtt__active_edge *next;
   #if STBTT_RASTERIZER_VERSION==1
   int x,dx;
   float ey;
   int direction;
   #elif STBTT_RASTERIZER_VERSION==2
   float fx,fdx,fdy;
   float direction;
   float sy;
   float ey;
   #else
   #error "Unrecognized value of STBTT_RASTERIZER_VERSION"
   #endif
} stbtt__active_edge;

#if STBTT_RASTERIZER_VERSION == 1
#define STBTT_FIXSHIFT   10
#define STBTT_FIX        (1 << STBTT_FIXSHIFT)
#define STBTT_FIXMASK    (STBTT_FIX-1)

static stbtt__active_edge *stbtt__new_active(stbtt__hheap *hh, stbtt__edge *e, int off_x, float start_point, void *userdata)
{
   stbtt__active_edge *z = (stbtt__active_edge *) stbtt__hheap_alloc(hh, sizeof(*z), userdata);
   float dxdy = (e->x1 - e->x0) / (e->y1 - e->y0);
   STBTT_assert(z != NULL);
   if (!z) return z;

   
   if (dxdy < 0)
      z->dx = -STBTT_ifloor(STBTT_FIX * -dxdy);
   else
      z->dx = STBTT_ifloor(STBTT_FIX * dxdy);

   z->x = STBTT_ifloor(STBTT_FIX * e->x0 + z->dx * (start_point - e->y0)); 
   z->x -= off_x * STBTT_FIX;

   z->ey = e->y1;
   z->next = 0;
   z->direction = e->invert ? 1 : -1;
   return z;
}
#elif STBTT_RASTERIZER_VERSION == 2
static stbtt__active_edge *stbtt__new_active(stbtt__hheap *hh, stbtt__edge *e, int off_x, float start_point, void *userdata)
{
   stbtt__active_edge *z = (stbtt__active_edge *) stbtt__hheap_alloc(hh, sizeof(*z), userdata);
   float dxdy = (e->x1 - e->x0) / (e->y1 - e->y0);
   STBTT_assert(z != NULL);
   
   if (!z) return z;
   z->fdx = dxdy;
   z->fdy = dxdy != 0.0f ? (1.0f/dxdy) : 0.0f;
   z->fx = e->x0 + dxdy * (start_point - e->y0);
   z->fx -= off_x;
   z->direction = e->invert ? 1.0f : -1.0f;
   z->sy = e->y0;
   z->ey = e->y1;
   z->next = 0;
   return z;
}
#else
#error "Unrecognized value of STBTT_RASTERIZER_VERSION"
#endif

#if STBTT_RASTERIZER_VERSION == 1

static void stbtt__fill_active_edges(unsigned char *scanline, int len, stbtt__active_edge *e, int max_weight)
{
   
   int x0=0, w=0;

   while (e) {
      if (w == 0) {
         
         x0 = e->x; w += e->direction;
      } else {
         int x1 = e->x; w += e->direction;
         
         if (w == 0) {
            int i = x0 >> STBTT_FIXSHIFT;
            int j = x1 >> STBTT_FIXSHIFT;

            if (i < len && j >= 0) {
               if (i == j) {
                  
                  scanline[i] = scanline[i] + (stbtt_uint8) ((x1 - x0) * max_weight >> STBTT_FIXSHIFT);
               } else {
                  if (i >= 0) 
                     scanline[i] = scanline[i] + (stbtt_uint8) (((STBTT_FIX - (x0 & STBTT_FIXMASK)) * max_weight) >> STBTT_FIXSHIFT);
                  else
                     i = -1; 

                  if (j < len) 
                     scanline[j] = scanline[j] + (stbtt_uint8) (((x1 & STBTT_FIXMASK) * max_weight) >> STBTT_FIXSHIFT);
                  else
                     j = len; 

                  for (++i; i < j; ++i) 
                     scanline[i] = scanline[i] + (stbtt_uint8) max_weight;
               }
            }
         }
      }

      e = e->next;
   }
}

static void stbtt__rasterize_sorted_edges(stbtt__bitmap *result, stbtt__edge *e, int n, int vsubsample, int off_x, int off_y, void *userdata)
{
   stbtt__hheap hh = { 0, 0, 0 };
   stbtt__active_edge *active = NULL;
   int y,j=0;
   int max_weight = (255 / vsubsample);  
   int s; 
   unsigned char scanline_data[512], *scanline;

   if (result->w > 512)
      scanline = (unsigned char *) STBTT_malloc(result->w, userdata);
   else
      scanline = scanline_data;

   y = off_y * vsubsample;
   e[n].y0 = (off_y + result->h) * (float) vsubsample + 1;

   while (j < result->h) {
      STBTT_memset(scanline, 0, result->w);
      for (s=0; s < vsubsample; ++s) {
         
         float scan_y = y + 0.5f;
         stbtt__active_edge **step = &active;

         
         
         while (*step) {
            stbtt__active_edge * z = *step;
            if (z->ey <= scan_y) {
               *step = z->next; 
               STBTT_assert(z->direction);
               z->direction = 0;
               stbtt__hheap_free(&hh, z);
            } else {
               z->x += z->dx; 
               step = &((*step)->next); 
            }
         }

         
         for(;;) {
            int changed=0;
            step = &active;
            while (*step && (*step)->next) {
               if ((*step)->x > (*step)->next->x) {
                  stbtt__active_edge *t = *step;
                  stbtt__active_edge *q = t->next;

                  t->next = q->next;
                  q->next = t;
                  *step = q;
                  changed = 1;
               }
               step = &(*step)->next;
            }
            if (!changed) break;
         }

         
         while (e->y0 <= scan_y) {
            if (e->y1 > scan_y) {
               stbtt__active_edge *z = stbtt__new_active(&hh, e, off_x, scan_y, userdata);
               if (z != NULL) {
                  
                  if (active == NULL)
                     active = z;
                  else if (z->x < active->x) {
                     
                     z->next = active;
                     active = z;
                  } else {
                     
                     stbtt__active_edge *p = active;
                     while (p->next && p->next->x < z->x)
                        p = p->next;
                     
                     z->next = p->next;
                     p->next = z;
                  }
               }
            }
            ++e;
         }

         
         if (active)
            stbtt__fill_active_edges(scanline, result->w, active, max_weight);

         ++y;
      }
      STBTT_memcpy(result->pixels + j * result->stride, scanline, result->w);
      ++j;
   }

   stbtt__hheap_cleanup(&hh, userdata);

   if (scanline != scanline_data)
      STBTT_free(scanline, userdata);
}

#elif STBTT_RASTERIZER_VERSION == 2

static void stbtt__handle_clipped_edge(float *scanline, int x, stbtt__active_edge *e, float x0, float y0, float x1, float y1)
{
   if (y0 == y1) return;
   STBTT_assert(y0 < y1);
   STBTT_assert(e->sy <= e->ey);
   if (y0 > e->ey) return;
   if (y1 < e->sy) return;
   if (y0 < e->sy) {
      x0 += (x1-x0) * (e->sy - y0) / (y1-y0);
      y0 = e->sy;
   }
   if (y1 > e->ey) {
      x1 += (x1-x0) * (e->ey - y1) / (y1-y0);
      y1 = e->ey;
   }

   if (x0 == x)
      STBTT_assert(x1 <= x+1);
   else if (x0 == x+1)
      STBTT_assert(x1 >= x);
   else if (x0 <= x)
      STBTT_assert(x1 <= x);
   else if (x0 >= x+1)
      STBTT_assert(x1 >= x+1);
   else
      STBTT_assert(x1 >= x && x1 <= x+1);

   if (x0 <= x && x1 <= x)
      scanline[x] += e->direction * (y1-y0);
   else if (x0 >= x+1 && x1 >= x+1)
      ;
   else {
      STBTT_assert(x0 >= x && x0 <= x+1 && x1 >= x && x1 <= x+1);
      scanline[x] += e->direction * (y1-y0) * (1-((x0-x)+(x1-x))/2); 
   }
}

static float stbtt__sized_trapezoid_area(float height, float top_width, float bottom_width)
{
   STBTT_assert(top_width >= 0);
   STBTT_assert(bottom_width >= 0);
   return (top_width + bottom_width) / 2.0f * height;
}

static float stbtt__position_trapezoid_area(float height, float tx0, float tx1, float bx0, float bx1)
{
   return stbtt__sized_trapezoid_area(height, tx1 - tx0, bx1 - bx0);
}

static float stbtt__sized_triangle_area(float height, float width)
{
   return height * width / 2;
}

static void stbtt__fill_active_edges_new(float *scanline, float *scanline_fill, int len, stbtt__active_edge *e, float y_top)
{
   float y_bottom = y_top+1;

   while (e) {
      

      
      STBTT_assert(e->ey >= y_top);

      if (e->fdx == 0) {
         float x0 = e->fx;
         if (x0 < len) {
            if (x0 >= 0) {
               stbtt__handle_clipped_edge(scanline,(int) x0,e, x0,y_top, x0,y_bottom);
               stbtt__handle_clipped_edge(scanline_fill-1,(int) x0+1,e, x0,y_top, x0,y_bottom);
            } else {
               stbtt__handle_clipped_edge(scanline_fill-1,0,e, x0,y_top, x0,y_bottom);
            }
         }
      } else {
         float x0 = e->fx;
         float dx = e->fdx;
         float xb = x0 + dx;
         float x_top, x_bottom;
         float sy0,sy1;
         float dy = e->fdy;
         STBTT_assert(e->sy <= y_bottom && e->ey >= y_top);

         
         
         
         if (e->sy > y_top) {
            x_top = x0 + dx * (e->sy - y_top);
            sy0 = e->sy;
         } else {
            x_top = x0;
            sy0 = y_top;
         }
         if (e->ey < y_bottom) {
            x_bottom = x0 + dx * (e->ey - y_top);
            sy1 = e->ey;
         } else {
            x_bottom = xb;
            sy1 = y_bottom;
         }

         if (x_top >= 0 && x_bottom >= 0 && x_top < len && x_bottom < len) {
            

            if ((int) x_top == (int) x_bottom) {
               float height;
               
               int x = (int) x_top;
               height = (sy1 - sy0) * e->direction;
               STBTT_assert(x >= 0 && x < len);
               scanline[x]      += stbtt__position_trapezoid_area(height, x_top, x+1.0f, x_bottom, x+1.0f);
               scanline_fill[x] += height; 
            } else {
               int x,x1,x2;
               float y_crossing, y_final, step, sign, area;
               
               if (x_top > x_bottom) {
                  
                  float t;
                  sy0 = y_bottom - (sy0 - y_top);
                  sy1 = y_bottom - (sy1 - y_top);
                  t = sy0, sy0 = sy1, sy1 = t;
                  t = x_bottom, x_bottom = x_top, x_top = t;
                  dx = -dx;
                  dy = -dy;
                  t = x0, x0 = xb, xb = t;
               }
               STBTT_assert(dy >= 0);
               STBTT_assert(dx >= 0);

               x1 = (int) x_top;
               x2 = (int) x_bottom;
               
               y_crossing = y_top + dy * (x1+1 - x0);

               
               y_final = y_top + dy * (x2 - x0);

               
               
               
               
               
               
               
               
               
               
               
               
               
               
               
               

               
               
               if (y_crossing > y_bottom)
                  y_crossing = y_bottom;

               sign = e->direction;

               
               area = sign * (y_crossing-sy0);

               
               scanline[x1] += stbtt__sized_triangle_area(area, x1+1 - x_top);

               
               if (y_final > y_bottom) {
                  y_final = y_bottom;
                  dy = (y_final - y_crossing ) / (x2 - (x1+1)); 
               }

               
               
               
               
               
               
               
               
               

               step = sign * dy * 1; 
               
               

               for (x = x1+1; x < x2; ++x) {
                  scanline[x] += area + step/2; 
                  area += step;
               }
               STBTT_assert(STBTT_fabs(area) <= 1.01f); 
               STBTT_assert(sy1 > y_final-0.01f);

               
               
               scanline[x2] += area + sign * stbtt__position_trapezoid_area(sy1-y_final, (float) x2, x2+1.0f, x_bottom, x2+1.0f);

               
               scanline_fill[x2] += sign * (sy1-sy0);
            }
         } else {
            
            
            
            
            
            
            
            int x;
            for (x=0; x < len; ++x) {
               
               
               
               
               
               
               
               
               
               
               
               

               
               float y0 = y_top;
               float x1 = (float) (x);
               float x2 = (float) (x+1);
               float x3 = xb;
               float y3 = y_bottom;

               
               
               
               float y1 = (x - x0) / dx + y_top;
               float y2 = (x+1 - x0) / dx + y_top;

               if (x0 < x1 && x3 > x2) {         
                  stbtt__handle_clipped_edge(scanline,x,e, x0,y0, x1,y1);
                  stbtt__handle_clipped_edge(scanline,x,e, x1,y1, x2,y2);
                  stbtt__handle_clipped_edge(scanline,x,e, x2,y2, x3,y3);
               } else if (x3 < x1 && x0 > x2) {  
                  stbtt__handle_clipped_edge(scanline,x,e, x0,y0, x2,y2);
                  stbtt__handle_clipped_edge(scanline,x,e, x2,y2, x1,y1);
                  stbtt__handle_clipped_edge(scanline,x,e, x1,y1, x3,y3);
               } else if (x0 < x1 && x3 > x1) {  
                  stbtt__handle_clipped_edge(scanline,x,e, x0,y0, x1,y1);
                  stbtt__handle_clipped_edge(scanline,x,e, x1,y1, x3,y3);
               } else if (x3 < x1 && x0 > x1) {  
                  stbtt__handle_clipped_edge(scanline,x,e, x0,y0, x1,y1);
                  stbtt__handle_clipped_edge(scanline,x,e, x1,y1, x3,y3);
               } else if (x0 < x2 && x3 > x2) {  
                  stbtt__handle_clipped_edge(scanline,x,e, x0,y0, x2,y2);
                  stbtt__handle_clipped_edge(scanline,x,e, x2,y2, x3,y3);
               } else if (x3 < x2 && x0 > x2) {  
                  stbtt__handle_clipped_edge(scanline,x,e, x0,y0, x2,y2);
                  stbtt__handle_clipped_edge(scanline,x,e, x2,y2, x3,y3);
               } else {  
                  stbtt__handle_clipped_edge(scanline,x,e, x0,y0, x3,y3);
               }
            }
         }
      }
      e = e->next;
   }
}

static void stbtt__rasterize_sorted_edges(stbtt__bitmap *result, stbtt__edge *e, int n, int vsubsample, int off_x, int off_y, void *userdata)
{
   stbtt__hheap hh = { 0, 0, 0 };
   stbtt__active_edge *active = NULL;
   int y,j=0, i;
   float scanline_data[129], *scanline, *scanline2;

   STBTT__NOTUSED(vsubsample);

   if (result->w > 64)
      scanline = (float *) STBTT_malloc((result->w*2+1) * sizeof(float), userdata);
   else
      scanline = scanline_data;

   scanline2 = scanline + result->w;

   y = off_y;
   e[n].y0 = (float) (off_y + result->h) + 1;

   while (j < result->h) {
      
      float scan_y_top    = y + 0.0f;
      float scan_y_bottom = y + 1.0f;
      stbtt__active_edge **step = &active;

      STBTT_memset(scanline , 0, result->w*sizeof(scanline[0]));
      STBTT_memset(scanline2, 0, (result->w+1)*sizeof(scanline[0]));

      
      
      while (*step) {
         stbtt__active_edge * z = *step;
         if (z->ey <= scan_y_top) {
            *step = z->next; 
            STBTT_assert(z->direction);
            z->direction = 0;
            stbtt__hheap_free(&hh, z);
         } else {
            step = &((*step)->next); 
         }
      }

      
      while (e->y0 <= scan_y_bottom) {
         if (e->y0 != e->y1) {
            stbtt__active_edge *z = stbtt__new_active(&hh, e, off_x, scan_y_top, userdata);
            if (z != NULL) {
               if (j == 0 && off_y != 0) {
                  if (z->ey < scan_y_top) {
                     
                     z->ey = scan_y_top;
                  }
               }
               STBTT_assert(z->ey >= scan_y_top); 
               
               z->next = active;
               active = z;
            }
         }
         ++e;
      }

      
      if (active)
         stbtt__fill_active_edges_new(scanline, scanline2+1, result->w, active, scan_y_top);

      {
         float sum = 0;
         for (i=0; i < result->w; ++i) {
            float k;
            int m;
            sum += scanline2[i];
            k = scanline[i] + sum;
            k = (float) STBTT_fabs(k)*255 + 0.5f;
            m = (int) k;
            if (m > 255) m = 255;
            result->pixels[j*result->stride + i] = (unsigned char) m;
         }
      }
      
      step = &active;
      while (*step) {
         stbtt__active_edge *z = *step;
         z->fx += z->fdx; 
         step = &((*step)->next); 
      }

      ++y;
      ++j;
   }

   stbtt__hheap_cleanup(&hh, userdata);

   if (scanline != scanline_data)
      STBTT_free(scanline, userdata);
}
#else
#error "Unrecognized value of STBTT_RASTERIZER_VERSION"
#endif

#define STBTT__COMPARE(a,b)  ((a)->y0 < (b)->y0)

static void stbtt__sort_edges_ins_sort(stbtt__edge *p, int n)
{
   int i,j;
   for (i=1; i < n; ++i) {
      stbtt__edge t = p[i], *a = &t;
      j = i;
      while (j > 0) {
         stbtt__edge *b = &p[j-1];
         int c = STBTT__COMPARE(a,b);
         if (!c) break;
         p[j] = p[j-1];
         --j;
      }
      if (i != j)
         p[j] = t;
   }
}

static void stbtt__sort_edges_quicksort(stbtt__edge *p, int n)
{
   
   while (n > 12) {
      stbtt__edge t;
      int c01,c12,c,m,i,j;

      
      m = n >> 1;
      c01 = STBTT__COMPARE(&p[0],&p[m]);
      c12 = STBTT__COMPARE(&p[m],&p[n-1]);
      
      if (c01 != c12) {
         
         int z;
         c = STBTT__COMPARE(&p[0],&p[n-1]);
         
         
         z = (c == c12) ? 0 : n-1;
         t = p[z];
         p[z] = p[m];
         p[m] = t;
      }
      
      
      t = p[0];
      p[0] = p[m];
      p[m] = t;

      
      i=1;
      j=n-1;
      for(;;) {
         
         
         for (;;++i) {
            if (!STBTT__COMPARE(&p[i], &p[0])) break;
         }
         for (;;--j) {
            if (!STBTT__COMPARE(&p[0], &p[j])) break;
         }
         
         if (i >= j) break;
         t = p[i];
         p[i] = p[j];
         p[j] = t;

         ++i;
         --j;
      }
      
      if (j < (n-i)) {
         stbtt__sort_edges_quicksort(p,j);
         p = p+i;
         n = n-i;
      } else {
         stbtt__sort_edges_quicksort(p+i, n-i);
         n = j;
      }
   }
}

static void stbtt__sort_edges(stbtt__edge *p, int n)
{
   stbtt__sort_edges_quicksort(p, n);
   stbtt__sort_edges_ins_sort(p, n);
}

typedef struct
{
   float x,y;
} stbtt__point;

static void stbtt__rasterize(stbtt__bitmap *result, stbtt__point *pts, int *wcount, int windings, float scale_x, float scale_y, float shift_x, float shift_y, int off_x, int off_y, int invert, void *userdata)
{
   float y_scale_inv = invert ? -scale_y : scale_y;
   stbtt__edge *e;
   int n,i,j,k,m;
#if STBTT_RASTERIZER_VERSION == 1
   int vsubsample = result->h < 8 ? 15 : 5;
#elif STBTT_RASTERIZER_VERSION == 2
   int vsubsample = 1;
#else
   #error "Unrecognized value of STBTT_RASTERIZER_VERSION"
#endif
   

   
   n = 0;
   for (i=0; i < windings; ++i)
      n += wcount[i];

   e = (stbtt__edge *) STBTT_malloc(sizeof(*e) * (n+1), userdata); 
   if (e == 0) return;
   n = 0;

   m=0;
   for (i=0; i < windings; ++i) {
      stbtt__point *p = pts + m;
      m += wcount[i];
      j = wcount[i]-1;
      for (k=0; k < wcount[i]; j=k++) {
         int a=k,b=j;
         
         if (p[j].y == p[k].y)
            continue;
         
         e[n].invert = 0;
         if (invert ? p[j].y > p[k].y : p[j].y < p[k].y) {
            e[n].invert = 1;
            a=j,b=k;
         }
         e[n].x0 = p[a].x * scale_x + shift_x;
         e[n].y0 = (p[a].y * y_scale_inv + shift_y) * vsubsample;
         e[n].x1 = p[b].x * scale_x + shift_x;
         e[n].y1 = (p[b].y * y_scale_inv + shift_y) * vsubsample;
         ++n;
      }
   }

   
   
   stbtt__sort_edges(e, n);

   
   stbtt__rasterize_sorted_edges(result, e, n, vsubsample, off_x, off_y, userdata);

   STBTT_free(e, userdata);
}

static void stbtt__add_point(stbtt__point *points, int n, float x, float y)
{
   if (!points) return; 
   points[n].x = x;
   points[n].y = y;
}

static int stbtt__tesselate_curve(stbtt__point *points, int *num_points, float x0, float y0, float x1, float y1, float x2, float y2, float objspace_flatness_squared, int n)
{
   
   float mx = (x0 + 2*x1 + x2)/4;
   float my = (y0 + 2*y1 + y2)/4;
   
   float dx = (x0+x2)/2 - mx;
   float dy = (y0+y2)/2 - my;
   if (n > 16) 
      return 1;
   if (dx*dx+dy*dy > objspace_flatness_squared) { 
      stbtt__tesselate_curve(points, num_points, x0,y0, (x0+x1)/2.0f,(y0+y1)/2.0f, mx,my, objspace_flatness_squared,n+1);
      stbtt__tesselate_curve(points, num_points, mx,my, (x1+x2)/2.0f,(y1+y2)/2.0f, x2,y2, objspace_flatness_squared,n+1);
   } else {
      stbtt__add_point(points, *num_points,x2,y2);
      *num_points = *num_points+1;
   }
   return 1;
}

static void stbtt__tesselate_cubic(stbtt__point *points, int *num_points, float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, float objspace_flatness_squared, int n)
{
   
   float dx0 = x1-x0;
   float dy0 = y1-y0;
   float dx1 = x2-x1;
   float dy1 = y2-y1;
   float dx2 = x3-x2;
   float dy2 = y3-y2;
   float dx = x3-x0;
   float dy = y3-y0;
   float longlen = (float) (STBTT_sqrt(dx0*dx0+dy0*dy0)+STBTT_sqrt(dx1*dx1+dy1*dy1)+STBTT_sqrt(dx2*dx2+dy2*dy2));
   float shortlen = (float) STBTT_sqrt(dx*dx+dy*dy);
   float flatness_squared = longlen*longlen-shortlen*shortlen;

   if (n > 16) 
      return;

   if (flatness_squared > objspace_flatness_squared) {
      float x01 = (x0+x1)/2;
      float y01 = (y0+y1)/2;
      float x12 = (x1+x2)/2;
      float y12 = (y1+y2)/2;
      float x23 = (x2+x3)/2;
      float y23 = (y2+y3)/2;

      float xa = (x01+x12)/2;
      float ya = (y01+y12)/2;
      float xb = (x12+x23)/2;
      float yb = (y12+y23)/2;

      float mx = (xa+xb)/2;
      float my = (ya+yb)/2;

      stbtt__tesselate_cubic(points, num_points, x0,y0, x01,y01, xa,ya, mx,my, objspace_flatness_squared,n+1);
      stbtt__tesselate_cubic(points, num_points, mx,my, xb,yb, x23,y23, x3,y3, objspace_flatness_squared,n+1);
   } else {
      stbtt__add_point(points, *num_points,x3,y3);
      *num_points = *num_points+1;
   }
}

static stbtt__point *stbtt_FlattenCurves(stbtt_vertex *vertices, int num_verts, float objspace_flatness, int **contour_lengths, int *num_contours, void *userdata)
{
   stbtt__point *points=0;
   int num_points=0;

   float objspace_flatness_squared = objspace_flatness * objspace_flatness;
   int i,n=0,start=0, pass;

   
   for (i=0; i < num_verts; ++i)
      if (vertices[i].type == STBTT_vmove)
         ++n;

   *num_contours = n;
   if (n == 0) return 0;

   *contour_lengths = (int *) STBTT_malloc(sizeof(**contour_lengths) * n, userdata);

   if (*contour_lengths == 0) {
      *num_contours = 0;
      return 0;
   }

   
   for (pass=0; pass < 2; ++pass) {
      float x=0,y=0;
      if (pass == 1) {
         points = (stbtt__point *) STBTT_malloc(num_points * sizeof(points[0]), userdata);
         if (points == NULL) goto error;
      }
      num_points = 0;
      n= -1;
      for (i=0; i < num_verts; ++i) {
         switch (vertices[i].type) {
            case STBTT_vmove:
               
               if (n >= 0)
                  (*contour_lengths)[n] = num_points - start;
               ++n;
               start = num_points;

               x = vertices[i].x, y = vertices[i].y;
               stbtt__add_point(points, num_points++, x,y);
               break;
            case STBTT_vline:
               x = vertices[i].x, y = vertices[i].y;
               stbtt__add_point(points, num_points++, x, y);
               break;
            case STBTT_vcurve:
               stbtt__tesselate_curve(points, &num_points, x,y,
                                        vertices[i].cx, vertices[i].cy,
                                        vertices[i].x,  vertices[i].y,
                                        objspace_flatness_squared, 0);
               x = vertices[i].x, y = vertices[i].y;
               break;
            case STBTT_vcubic:
               stbtt__tesselate_cubic(points, &num_points, x,y,
                                        vertices[i].cx, vertices[i].cy,
                                        vertices[i].cx1, vertices[i].cy1,
                                        vertices[i].x,  vertices[i].y,
                                        objspace_flatness_squared, 0);
               x = vertices[i].x, y = vertices[i].y;
               break;
         }
      }
      (*contour_lengths)[n] = num_points - start;
   }

   return points;
error:
   STBTT_free(points, userdata);
   STBTT_free(*contour_lengths, userdata);
   *contour_lengths = 0;
   *num_contours = 0;
   return NULL;
}

STBTT_DEF void stbtt_Rasterize(stbtt__bitmap *result, float flatness_in_pixels, stbtt_vertex *vertices, int num_verts, float scale_x, float scale_y, float shift_x, float shift_y, int x_off, int y_off, int invert, void *userdata)
{
   float scale            = scale_x > scale_y ? scale_y : scale_x;
   int winding_count      = 0;
   int *winding_lengths   = NULL;
   stbtt__point *windings = stbtt_FlattenCurves(vertices, num_verts, flatness_in_pixels / scale, &winding_lengths, &winding_count, userdata);
   if (windings) {
      stbtt__rasterize(result, windings, winding_lengths, winding_count, scale_x, scale_y, shift_x, shift_y, x_off, y_off, invert, userdata);
      STBTT_free(winding_lengths, userdata);
      STBTT_free(windings, userdata);
   }
}

STBTT_DEF void stbtt_FreeBitmap(unsigned char *bitmap, void *userdata)
{
   STBTT_free(bitmap, userdata);
}

STBTT_DEF unsigned char *stbtt_GetGlyphBitmapSubpixel(const stbtt_fontinfo *info, float scale_x, float scale_y, float shift_x, float shift_y, int glyph, int *width, int *height, int *xoff, int *yoff)
{
   int ix0,iy0,ix1,iy1;
   stbtt__bitmap gbm;
   stbtt_vertex *vertices;
   int num_verts = stbtt_GetGlyphShape(info, glyph, &vertices);

   if (scale_x == 0) scale_x = scale_y;
   if (scale_y == 0) {
      if (scale_x == 0) {
         STBTT_free(vertices, info->userdata);
         return NULL;
      }
      scale_y = scale_x;
   }

   stbtt_GetGlyphBitmapBoxSubpixel(info, glyph, scale_x, scale_y, shift_x, shift_y, &ix0,&iy0,&ix1,&iy1);

   
   gbm.w = (ix1 - ix0);
   gbm.h = (iy1 - iy0);
   gbm.pixels = NULL; 

   if (width ) *width  = gbm.w;
   if (height) *height = gbm.h;
   if (xoff  ) *xoff   = ix0;
   if (yoff  ) *yoff   = iy0;

   if (gbm.w && gbm.h) {
      gbm.pixels = (unsigned char *) STBTT_malloc(gbm.w * gbm.h, info->userdata);
      if (gbm.pixels) {
         gbm.stride = gbm.w;

         stbtt_Rasterize(&gbm, 0.35f, vertices, num_verts, scale_x, scale_y, shift_x, shift_y, ix0, iy0, 1, info->userdata);
      }
   }
   STBTT_free(vertices, info->userdata);
   return gbm.pixels;
}

STBTT_DEF unsigned char *stbtt_GetGlyphBitmap(const stbtt_fontinfo *info, float scale_x, float scale_y, int glyph, int *width, int *height, int *xoff, int *yoff)
{
   return stbtt_GetGlyphBitmapSubpixel(info, scale_x, scale_y, 0.0f, 0.0f, glyph, width, height, xoff, yoff);
}

STBTT_DEF void stbtt_MakeGlyphBitmapSubpixel(const stbtt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, float shift_x, float shift_y, int glyph)
{
   int ix0,iy0;
   stbtt_vertex *vertices;
   int num_verts = stbtt_GetGlyphShape(info, glyph, &vertices);
   stbtt__bitmap gbm;

   stbtt_GetGlyphBitmapBoxSubpixel(info, glyph, scale_x, scale_y, shift_x, shift_y, &ix0,&iy0,0,0);
   gbm.pixels = output;
   gbm.w = out_w;
   gbm.h = out_h;
   gbm.stride = out_stride;

   if (gbm.w && gbm.h)
      stbtt_Rasterize(&gbm, 0.35f, vertices, num_verts, scale_x, scale_y, shift_x, shift_y, ix0,iy0, 1, info->userdata);

   STBTT_free(vertices, info->userdata);
}

STBTT_DEF void stbtt_MakeGlyphBitmap(const stbtt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, int glyph)
{
   stbtt_MakeGlyphBitmapSubpixel(info, output, out_w, out_h, out_stride, scale_x, scale_y, 0.0f,0.0f, glyph);
}

STBTT_DEF unsigned char *stbtt_GetCodepointBitmapSubpixel(const stbtt_fontinfo *info, float scale_x, float scale_y, float shift_x, float shift_y, int codepoint, int *width, int *height, int *xoff, int *yoff)
{
   return stbtt_GetGlyphBitmapSubpixel(info, scale_x, scale_y,shift_x,shift_y, stbtt_FindGlyphIndex(info,codepoint), width,height,xoff,yoff);
}

STBTT_DEF void stbtt_MakeCodepointBitmapSubpixelPrefilter(const stbtt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, float shift_x, float shift_y, int oversample_x, int oversample_y, float *sub_x, float *sub_y, int codepoint)
{
   stbtt_MakeGlyphBitmapSubpixelPrefilter(info, output, out_w, out_h, out_stride, scale_x, scale_y, shift_x, shift_y, oversample_x, oversample_y, sub_x, sub_y, stbtt_FindGlyphIndex(info,codepoint));
}

STBTT_DEF void stbtt_MakeCodepointBitmapSubpixel(const stbtt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, float shift_x, float shift_y, int codepoint)
{
   stbtt_MakeGlyphBitmapSubpixel(info, output, out_w, out_h, out_stride, scale_x, scale_y, shift_x, shift_y, stbtt_FindGlyphIndex(info,codepoint));
}

STBTT_DEF unsigned char *stbtt_GetCodepointBitmap(const stbtt_fontinfo *info, float scale_x, float scale_y, int codepoint, int *width, int *height, int *xoff, int *yoff)
{
   return stbtt_GetCodepointBitmapSubpixel(info, scale_x, scale_y, 0.0f,0.0f, codepoint, width,height,xoff,yoff);
}

STBTT_DEF void stbtt_MakeCodepointBitmap(const stbtt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, int codepoint)
{
   stbtt_MakeCodepointBitmapSubpixel(info, output, out_w, out_h, out_stride, scale_x, scale_y, 0.0f,0.0f, codepoint);
}

static int stbtt_BakeFontBitmap_internal(unsigned char *data, int offset,  
                                float pixel_height,                     
                                unsigned char *pixels, int pw, int ph,  
                                int first_char, int num_chars,          
                                stbtt_bakedchar *chardata)
{
   float scale;
   int x,y,bottom_y, i;
   stbtt_fontinfo f;
   f.userdata = NULL;
   if (!stbtt_InitFont(&f, data, offset))
      return -1;
   STBTT_memset(pixels, 0, pw*ph); 
   x=y=1;
   bottom_y = 1;

   scale = stbtt_ScaleForPixelHeight(&f, pixel_height);

   for (i=0; i < num_chars; ++i) {
      int advance, lsb, x0,y0,x1,y1,gw,gh;
      int g = stbtt_FindGlyphIndex(&f, first_char + i);
      stbtt_GetGlyphHMetrics(&f, g, &advance, &lsb);
      stbtt_GetGlyphBitmapBox(&f, g, scale,scale, &x0,&y0,&x1,&y1);
      gw = x1-x0;
      gh = y1-y0;
      if (x + gw + 1 >= pw)
         y = bottom_y, x = 1; 
      if (y + gh + 1 >= ph) 
         return -i;
      STBTT_assert(x+gw < pw);
      STBTT_assert(y+gh < ph);
      stbtt_MakeGlyphBitmap(&f, pixels+x+y*pw, gw,gh,pw, scale,scale, g);
      chardata[i].x0 = (stbtt_int16) x;
      chardata[i].y0 = (stbtt_int16) y;
      chardata[i].x1 = (stbtt_int16) (x + gw);
      chardata[i].y1 = (stbtt_int16) (y + gh);
      chardata[i].xadvance = scale * advance;
      chardata[i].xoff     = (float) x0;
      chardata[i].yoff     = (float) y0;
      x = x + gw + 1;
      if (y+gh+1 > bottom_y)
         bottom_y = y+gh+1;
   }
   return bottom_y;
}

STBTT_DEF void stbtt_GetBakedQuad(const stbtt_bakedchar *chardata, int pw, int ph, int char_index, float *xpos, float *ypos, stbtt_aligned_quad *q, int opengl_fillrule)
{
   float d3d_bias = opengl_fillrule ? 0 : -0.5f;
   float ipw = 1.0f / pw, iph = 1.0f / ph;
   const stbtt_bakedchar *b = chardata + char_index;
   int round_x = STBTT_ifloor((*xpos + b->xoff) + 0.5f);
   int round_y = STBTT_ifloor((*ypos + b->yoff) + 0.5f);

   q->x0 = round_x + d3d_bias;
   q->y0 = round_y + d3d_bias;
   q->x1 = round_x + b->x1 - b->x0 + d3d_bias;
   q->y1 = round_y + b->y1 - b->y0 + d3d_bias;

   q->s0 = b->x0 * ipw;
   q->t0 = b->y0 * iph;
   q->s1 = b->x1 * ipw;
   q->t1 = b->y1 * iph;

   *xpos += b->xadvance;
}

#ifndef STB_RECT_PACK_VERSION

typedef int stbrp_coord;

typedef struct
{
   int width,height;
   int x,y,bottom_y;
} stbrp_context;

typedef struct
{
   unsigned char x;
} stbrp_node;

struct stbrp_rect
{
   stbrp_coord x,y;
   int id,w,h,was_packed;
};

static void stbrp_init_target(stbrp_context *con, int pw, int ph, stbrp_node *nodes, int num_nodes)
{
   con->width  = pw;
   con->height = ph;
   con->x = 0;
   con->y = 0;
   con->bottom_y = 0;
   STBTT__NOTUSED(nodes);
   STBTT__NOTUSED(num_nodes);
}

static void stbrp_pack_rects(stbrp_context *con, stbrp_rect *rects, int num_rects)
{
   int i;
   for (i=0; i < num_rects; ++i) {
      if (con->x + rects[i].w > con->width) {
         con->x = 0;
         con->y = con->bottom_y;
      }
      if (con->y + rects[i].h > con->height)
         break;
      rects[i].x = con->x;
      rects[i].y = con->y;
      rects[i].was_packed = 1;
      con->x += rects[i].w;
      if (con->y + rects[i].h > con->bottom_y)
         con->bottom_y = con->y + rects[i].h;
   }
   for (   ; i < num_rects; ++i)
      rects[i].was_packed = 0;
}
#endif

STBTT_DEF int stbtt_PackBegin(stbtt_pack_context *spc, unsigned char *pixels, int pw, int ph, int stride_in_bytes, int padding, void *alloc_context)
{
   stbrp_context *context = (stbrp_context *) STBTT_malloc(sizeof(*context)            ,alloc_context);
   int            num_nodes = pw - padding;
   stbrp_node    *nodes   = (stbrp_node    *) STBTT_malloc(sizeof(*nodes  ) * num_nodes,alloc_context);

   if (context == NULL || nodes == NULL) {
      if (context != NULL) STBTT_free(context, alloc_context);
      if (nodes   != NULL) STBTT_free(nodes  , alloc_context);
      return 0;
   }

   spc->user_allocator_context = alloc_context;
   spc->width = pw;
   spc->height = ph;
   spc->pixels = pixels;
   spc->pack_info = context;
   spc->nodes = nodes;
   spc->padding = padding;
   spc->stride_in_bytes = stride_in_bytes != 0 ? stride_in_bytes : pw;
   spc->h_oversample = 1;
   spc->v_oversample = 1;
   spc->skip_missing = 0;

   stbrp_init_target(context, pw-padding, ph-padding, nodes, num_nodes);

   if (pixels)
      STBTT_memset(pixels, 0, pw*ph); 

   return 1;
}

STBTT_DEF void stbtt_PackEnd  (stbtt_pack_context *spc)
{
   STBTT_free(spc->nodes    , spc->user_allocator_context);
   STBTT_free(spc->pack_info, spc->user_allocator_context);
}

STBTT_DEF void stbtt_PackSetOversampling(stbtt_pack_context *spc, unsigned int h_oversample, unsigned int v_oversample)
{
   STBTT_assert(h_oversample <= STBTT_MAX_OVERSAMPLE);
   STBTT_assert(v_oversample <= STBTT_MAX_OVERSAMPLE);
   if (h_oversample <= STBTT_MAX_OVERSAMPLE)
      spc->h_oversample = h_oversample;
   if (v_oversample <= STBTT_MAX_OVERSAMPLE)
      spc->v_oversample = v_oversample;
}

STBTT_DEF void stbtt_PackSetSkipMissingCodepoints(stbtt_pack_context *spc, int skip)
{
   spc->skip_missing = skip;
}

#define STBTT__OVER_MASK  (STBTT_MAX_OVERSAMPLE-1)

static void stbtt__h_prefilter(unsigned char *pixels, int w, int h, int stride_in_bytes, unsigned int kernel_width)
{
   unsigned char buffer[STBTT_MAX_OVERSAMPLE];
   int safe_w = w - kernel_width;
   int j;
   STBTT_memset(buffer, 0, STBTT_MAX_OVERSAMPLE); 
   for (j=0; j < h; ++j) {
      int i;
      unsigned int total;
      STBTT_memset(buffer, 0, kernel_width);

      total = 0;

      
      switch (kernel_width) {
         case 2:
            for (i=0; i <= safe_w; ++i) {
               total += pixels[i] - buffer[i & STBTT__OVER_MASK];
               buffer[(i+kernel_width) & STBTT__OVER_MASK] = pixels[i];
               pixels[i] = (unsigned char) (total / 2);
            }
            break;
         case 3:
            for (i=0; i <= safe_w; ++i) {
               total += pixels[i] - buffer[i & STBTT__OVER_MASK];
               buffer[(i+kernel_width) & STBTT__OVER_MASK] = pixels[i];
               pixels[i] = (unsigned char) (total / 3);
            }
            break;
         case 4:
            for (i=0; i <= safe_w; ++i) {
               total += pixels[i] - buffer[i & STBTT__OVER_MASK];
               buffer[(i+kernel_width) & STBTT__OVER_MASK] = pixels[i];
               pixels[i] = (unsigned char) (total / 4);
            }
            break;
         case 5:
            for (i=0; i <= safe_w; ++i) {
               total += pixels[i] - buffer[i & STBTT__OVER_MASK];
               buffer[(i+kernel_width) & STBTT__OVER_MASK] = pixels[i];
               pixels[i] = (unsigned char) (total / 5);
            }
            break;
         default:
            for (i=0; i <= safe_w; ++i) {
               total += pixels[i] - buffer[i & STBTT__OVER_MASK];
               buffer[(i+kernel_width) & STBTT__OVER_MASK] = pixels[i];
               pixels[i] = (unsigned char) (total / kernel_width);
            }
            break;
      }

      for (; i < w; ++i) {
         STBTT_assert(pixels[i] == 0);
         total -= buffer[i & STBTT__OVER_MASK];
         pixels[i] = (unsigned char) (total / kernel_width);
      }

      pixels += stride_in_bytes;
   }
}

static void stbtt__v_prefilter(unsigned char *pixels, int w, int h, int stride_in_bytes, unsigned int kernel_width)
{
   unsigned char buffer[STBTT_MAX_OVERSAMPLE];
   int safe_h = h - kernel_width;
   int j;
   STBTT_memset(buffer, 0, STBTT_MAX_OVERSAMPLE); 
   for (j=0; j < w; ++j) {
      int i;
      unsigned int total;
      STBTT_memset(buffer, 0, kernel_width);

      total = 0;

      
      switch (kernel_width) {
         case 2:
            for (i=0; i <= safe_h; ++i) {
               total += pixels[i*stride_in_bytes] - buffer[i & STBTT__OVER_MASK];
               buffer[(i+kernel_width) & STBTT__OVER_MASK] = pixels[i*stride_in_bytes];
               pixels[i*stride_in_bytes] = (unsigned char) (total / 2);
            }
            break;
         case 3:
            for (i=0; i <= safe_h; ++i) {
               total += pixels[i*stride_in_bytes] - buffer[i & STBTT__OVER_MASK];
               buffer[(i+kernel_width) & STBTT__OVER_MASK] = pixels[i*stride_in_bytes];
               pixels[i*stride_in_bytes] = (unsigned char) (total / 3);
            }
            break;
         case 4:
            for (i=0; i <= safe_h; ++i) {
               total += pixels[i*stride_in_bytes] - buffer[i & STBTT__OVER_MASK];
               buffer[(i+kernel_width) & STBTT__OVER_MASK] = pixels[i*stride_in_bytes];
               pixels[i*stride_in_bytes] = (unsigned char) (total / 4);
            }
            break;
         case 5:
            for (i=0; i <= safe_h; ++i) {
               total += pixels[i*stride_in_bytes] - buffer[i & STBTT__OVER_MASK];
               buffer[(i+kernel_width) & STBTT__OVER_MASK] = pixels[i*stride_in_bytes];
               pixels[i*stride_in_bytes] = (unsigned char) (total / 5);
            }
            break;
         default:
            for (i=0; i <= safe_h; ++i) {
               total += pixels[i*stride_in_bytes] - buffer[i & STBTT__OVER_MASK];
               buffer[(i+kernel_width) & STBTT__OVER_MASK] = pixels[i*stride_in_bytes];
               pixels[i*stride_in_bytes] = (unsigned char) (total / kernel_width);
            }
            break;
      }

      for (; i < h; ++i) {
         STBTT_assert(pixels[i*stride_in_bytes] == 0);
         total -= buffer[i & STBTT__OVER_MASK];
         pixels[i*stride_in_bytes] = (unsigned char) (total / kernel_width);
      }

      pixels += 1;
   }
}

static float stbtt__oversample_shift(int oversample)
{
   if (!oversample)
      return 0.0f;

   
   
   
   
   return (float)-(oversample - 1) / (2.0f * (float)oversample);
}

STBTT_DEF int stbtt_PackFontRangesGatherRects(stbtt_pack_context *spc, const stbtt_fontinfo *info, stbtt_pack_range *ranges, int num_ranges, stbrp_rect *rects)
{
   int i,j,k;
   int missing_glyph_added = 0;

   k=0;
   for (i=0; i < num_ranges; ++i) {
      float fh = ranges[i].font_size;
      float scale = fh > 0 ? stbtt_ScaleForPixelHeight(info, fh) : stbtt_ScaleForMappingEmToPixels(info, -fh);
      ranges[i].h_oversample = (unsigned char) spc->h_oversample;
      ranges[i].v_oversample = (unsigned char) spc->v_oversample;
      for (j=0; j < ranges[i].num_chars; ++j) {
         int x0,y0,x1,y1;
         int codepoint = ranges[i].array_of_unicode_codepoints == NULL ? ranges[i].first_unicode_codepoint_in_range + j : ranges[i].array_of_unicode_codepoints[j];
         int glyph = stbtt_FindGlyphIndex(info, codepoint);
         if (glyph == 0 && (spc->skip_missing || missing_glyph_added)) {
            rects[k].w = rects[k].h = 0;
         } else {
            stbtt_GetGlyphBitmapBoxSubpixel(info,glyph,
                                            scale * spc->h_oversample,
                                            scale * spc->v_oversample,
                                            0,0,
                                            &x0,&y0,&x1,&y1);
            rects[k].w = (stbrp_coord) (x1-x0 + spc->padding + spc->h_oversample-1);
            rects[k].h = (stbrp_coord) (y1-y0 + spc->padding + spc->v_oversample-1);
            if (glyph == 0)
               missing_glyph_added = 1;
         }
         ++k;
      }
   }

   return k;
}

STBTT_DEF void stbtt_MakeGlyphBitmapSubpixelPrefilter(const stbtt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, float shift_x, float shift_y, int prefilter_x, int prefilter_y, float *sub_x, float *sub_y, int glyph)
{
   stbtt_MakeGlyphBitmapSubpixel(info,
                                 output,
                                 out_w - (prefilter_x - 1),
                                 out_h - (prefilter_y - 1),
                                 out_stride,
                                 scale_x,
                                 scale_y,
                                 shift_x,
                                 shift_y,
                                 glyph);

   if (prefilter_x > 1)
      stbtt__h_prefilter(output, out_w, out_h, out_stride, prefilter_x);

   if (prefilter_y > 1)
      stbtt__v_prefilter(output, out_w, out_h, out_stride, prefilter_y);

   *sub_x = stbtt__oversample_shift(prefilter_x);
   *sub_y = stbtt__oversample_shift(prefilter_y);
}

STBTT_DEF int stbtt_PackFontRangesRenderIntoRects(stbtt_pack_context *spc, const stbtt_fontinfo *info, stbtt_pack_range *ranges, int num_ranges, stbrp_rect *rects)
{
   int i,j,k, missing_glyph = -1, return_value = 1;

   
   int old_h_over = spc->h_oversample;
   int old_v_over = spc->v_oversample;

   k = 0;
   for (i=0; i < num_ranges; ++i) {
      float fh = ranges[i].font_size;
      float scale = fh > 0 ? stbtt_ScaleForPixelHeight(info, fh) : stbtt_ScaleForMappingEmToPixels(info, -fh);
      float recip_h,recip_v,sub_x,sub_y;
      spc->h_oversample = ranges[i].h_oversample;
      spc->v_oversample = ranges[i].v_oversample;
      recip_h = 1.0f / spc->h_oversample;
      recip_v = 1.0f / spc->v_oversample;
      sub_x = stbtt__oversample_shift(spc->h_oversample);
      sub_y = stbtt__oversample_shift(spc->v_oversample);
      for (j=0; j < ranges[i].num_chars; ++j) {
         stbrp_rect *r = &rects[k];
         if (r->was_packed && r->w != 0 && r->h != 0) {
            stbtt_packedchar *bc = &ranges[i].chardata_for_range[j];
            int advance, lsb, x0,y0,x1,y1;
            int codepoint = ranges[i].array_of_unicode_codepoints == NULL ? ranges[i].first_unicode_codepoint_in_range + j : ranges[i].array_of_unicode_codepoints[j];
            int glyph = stbtt_FindGlyphIndex(info, codepoint);
            stbrp_coord pad = (stbrp_coord) spc->padding;

            
            r->x += pad;
            r->y += pad;
            r->w -= pad;
            r->h -= pad;
            stbtt_GetGlyphHMetrics(info, glyph, &advance, &lsb);
            stbtt_GetGlyphBitmapBox(info, glyph,
                                    scale * spc->h_oversample,
                                    scale * spc->v_oversample,
                                    &x0,&y0,&x1,&y1);
            stbtt_MakeGlyphBitmapSubpixel(info,
                                          spc->pixels + r->x + r->y*spc->stride_in_bytes,
                                          r->w - spc->h_oversample+1,
                                          r->h - spc->v_oversample+1,
                                          spc->stride_in_bytes,
                                          scale * spc->h_oversample,
                                          scale * spc->v_oversample,
                                          0,0,
                                          glyph);

            if (spc->h_oversample > 1)
               stbtt__h_prefilter(spc->pixels + r->x + r->y*spc->stride_in_bytes,
                                  r->w, r->h, spc->stride_in_bytes,
                                  spc->h_oversample);

            if (spc->v_oversample > 1)
               stbtt__v_prefilter(spc->pixels + r->x + r->y*spc->stride_in_bytes,
                                  r->w, r->h, spc->stride_in_bytes,
                                  spc->v_oversample);

            bc->x0       = (stbtt_int16)  r->x;
            bc->y0       = (stbtt_int16)  r->y;
            bc->x1       = (stbtt_int16) (r->x + r->w);
            bc->y1       = (stbtt_int16) (r->y + r->h);
            bc->xadvance =                scale * advance;
            bc->xoff     =       (float)  x0 * recip_h + sub_x;
            bc->yoff     =       (float)  y0 * recip_v + sub_y;
            bc->xoff2    =                (x0 + r->w) * recip_h + sub_x;
            bc->yoff2    =                (y0 + r->h) * recip_v + sub_y;

            if (glyph == 0)
               missing_glyph = j;
         } else if (spc->skip_missing) {
            return_value = 0;
         } else if (r->was_packed && r->w == 0 && r->h == 0 && missing_glyph >= 0) {
            ranges[i].chardata_for_range[j] = ranges[i].chardata_for_range[missing_glyph];
         } else {
            return_value = 0; 
         }

         ++k;
      }
   }

   
   spc->h_oversample = old_h_over;
   spc->v_oversample = old_v_over;

   return return_value;
}

STBTT_DEF void stbtt_PackFontRangesPackRects(stbtt_pack_context *spc, stbrp_rect *rects, int num_rects)
{
   stbrp_pack_rects((stbrp_context *) spc->pack_info, rects, num_rects);
}

STBTT_DEF int stbtt_PackFontRanges(stbtt_pack_context *spc, const unsigned char *fontdata, int font_index, stbtt_pack_range *ranges, int num_ranges)
{
   stbtt_fontinfo info;
   int i,j,n, return_value = 1;
   
   stbrp_rect    *rects;

   
   for (i=0; i < num_ranges; ++i)
      for (j=0; j < ranges[i].num_chars; ++j)
         ranges[i].chardata_for_range[j].x0 =
         ranges[i].chardata_for_range[j].y0 =
         ranges[i].chardata_for_range[j].x1 =
         ranges[i].chardata_for_range[j].y1 = 0;

   n = 0;
   for (i=0; i < num_ranges; ++i)
      n += ranges[i].num_chars;

   rects = (stbrp_rect *) STBTT_malloc(sizeof(*rects) * n, spc->user_allocator_context);
   if (rects == NULL)
      return 0;

   info.userdata = spc->user_allocator_context;
   stbtt_InitFont(&info, fontdata, stbtt_GetFontOffsetForIndex(fontdata,font_index));

   n = stbtt_PackFontRangesGatherRects(spc, &info, ranges, num_ranges, rects);

   stbtt_PackFontRangesPackRects(spc, rects, n);

   return_value = stbtt_PackFontRangesRenderIntoRects(spc, &info, ranges, num_ranges, rects);

   STBTT_free(rects, spc->user_allocator_context);
   return return_value;
}

STBTT_DEF int stbtt_PackFontRange(stbtt_pack_context *spc, const unsigned char *fontdata, int font_index, float font_size,
            int first_unicode_codepoint_in_range, int num_chars_in_range, stbtt_packedchar *chardata_for_range)
{
   stbtt_pack_range range;
   range.first_unicode_codepoint_in_range = first_unicode_codepoint_in_range;
   range.array_of_unicode_codepoints = NULL;
   range.num_chars                   = num_chars_in_range;
   range.chardata_for_range          = chardata_for_range;
   range.font_size                   = font_size;
   return stbtt_PackFontRanges(spc, fontdata, font_index, &range, 1);
}

STBTT_DEF void stbtt_GetScaledFontVMetrics(const unsigned char *fontdata, int index, float size, float *ascent, float *descent, float *lineGap)
{
   int i_ascent, i_descent, i_lineGap;
   float scale;
   stbtt_fontinfo info;
   stbtt_InitFont(&info, fontdata, stbtt_GetFontOffsetForIndex(fontdata, index));
   scale = size > 0 ? stbtt_ScaleForPixelHeight(&info, size) : stbtt_ScaleForMappingEmToPixels(&info, -size);
   stbtt_GetFontVMetrics(&info, &i_ascent, &i_descent, &i_lineGap);
   *ascent  = (float) i_ascent  * scale;
   *descent = (float) i_descent * scale;
   *lineGap = (float) i_lineGap * scale;
}

STBTT_DEF void stbtt_GetPackedQuad(const stbtt_packedchar *chardata, int pw, int ph, int char_index, float *xpos, float *ypos, stbtt_aligned_quad *q, int align_to_integer)
{
   float ipw = 1.0f / pw, iph = 1.0f / ph;
   const stbtt_packedchar *b = chardata + char_index;

   if (align_to_integer) {
      float x = (float) STBTT_ifloor((*xpos + b->xoff) + 0.5f);
      float y = (float) STBTT_ifloor((*ypos + b->yoff) + 0.5f);
      q->x0 = x;
      q->y0 = y;
      q->x1 = x + b->xoff2 - b->xoff;
      q->y1 = y + b->yoff2 - b->yoff;
   } else {
      q->x0 = *xpos + b->xoff;
      q->y0 = *ypos + b->yoff;
      q->x1 = *xpos + b->xoff2;
      q->y1 = *ypos + b->yoff2;
   }

   q->s0 = b->x0 * ipw;
   q->t0 = b->y0 * iph;
   q->s1 = b->x1 * ipw;
   q->t1 = b->y1 * iph;

   *xpos += b->xadvance;
}

#define STBTT_min(a,b)  ((a) < (b) ? (a) : (b))
#define STBTT_max(a,b)  ((a) < (b) ? (b) : (a))

static int stbtt__ray_intersect_bezier(float orig[2], float ray[2], float q0[2], float q1[2], float q2[2], float hits[2][2])
{
   float q0perp = q0[1]*ray[0] - q0[0]*ray[1];
   float q1perp = q1[1]*ray[0] - q1[0]*ray[1];
   float q2perp = q2[1]*ray[0] - q2[0]*ray[1];
   float roperp = orig[1]*ray[0] - orig[0]*ray[1];

   float a = q0perp - 2*q1perp + q2perp;
   float b = q1perp - q0perp;
   float c = q0perp - roperp;

   float s0 = 0., s1 = 0.;
   int num_s = 0;

   if (a != 0.0) {
      float discr = b*b - a*c;
      if (discr > 0.0) {
         float rcpna = -1 / a;
         float d = (float) STBTT_sqrt(discr);
         s0 = (b+d) * rcpna;
         s1 = (b-d) * rcpna;
         if (s0 >= 0.0 && s0 <= 1.0)
            num_s = 1;
         if (d > 0.0 && s1 >= 0.0 && s1 <= 1.0) {
            if (num_s == 0) s0 = s1;
            ++num_s;
         }
      }
   } else {
      
      
      s0 = c / (-2 * b);
      if (s0 >= 0.0 && s0 <= 1.0)
         num_s = 1;
   }

   if (num_s == 0)
      return 0;
   else {
      float rcp_len2 = 1 / (ray[0]*ray[0] + ray[1]*ray[1]);
      float rayn_x = ray[0] * rcp_len2, rayn_y = ray[1] * rcp_len2;

      float q0d =   q0[0]*rayn_x +   q0[1]*rayn_y;
      float q1d =   q1[0]*rayn_x +   q1[1]*rayn_y;
      float q2d =   q2[0]*rayn_x +   q2[1]*rayn_y;
      float rod = orig[0]*rayn_x + orig[1]*rayn_y;

      float q10d = q1d - q0d;
      float q20d = q2d - q0d;
      float q0rd = q0d - rod;

      hits[0][0] = q0rd + s0*(2.0f - 2.0f*s0)*q10d + s0*s0*q20d;
      hits[0][1] = a*s0+b;

      if (num_s > 1) {
         hits[1][0] = q0rd + s1*(2.0f - 2.0f*s1)*q10d + s1*s1*q20d;
         hits[1][1] = a*s1+b;
         return 2;
      } else {
         return 1;
      }
   }
}

static int equal(float *a, float *b)
{
   return (a[0] == b[0] && a[1] == b[1]);
}

static int stbtt__compute_crossings_x(float x, float y, int nverts, stbtt_vertex *verts)
{
   int i;
   float orig[2], ray[2] = { 1, 0 };
   float y_frac;
   int winding = 0;

   
   y_frac = (float) STBTT_fmod(y, 1.0f);
   if (y_frac < 0.01f)
      y += 0.01f;
   else if (y_frac > 0.99f)
      y -= 0.01f;

   orig[0] = x;
   orig[1] = y;

   
   for (i=0; i < nverts; ++i) {
      if (verts[i].type == STBTT_vline) {
         int x0 = (int) verts[i-1].x, y0 = (int) verts[i-1].y;
         int x1 = (int) verts[i  ].x, y1 = (int) verts[i  ].y;
         if (y > STBTT_min(y0,y1) && y < STBTT_max(y0,y1) && x > STBTT_min(x0,x1)) {
            float x_inter = (y - y0) / (y1 - y0) * (x1-x0) + x0;
            if (x_inter < x)
               winding += (y0 < y1) ? 1 : -1;
         }
      }
      if (verts[i].type == STBTT_vcurve) {
         int x0 = (int) verts[i-1].x , y0 = (int) verts[i-1].y ;
         int x1 = (int) verts[i  ].cx, y1 = (int) verts[i  ].cy;
         int x2 = (int) verts[i  ].x , y2 = (int) verts[i  ].y ;
         int ax = STBTT_min(x0,STBTT_min(x1,x2)), ay = STBTT_min(y0,STBTT_min(y1,y2));
         int by = STBTT_max(y0,STBTT_max(y1,y2));
         if (y > ay && y < by && x > ax) {
            float q0[2],q1[2],q2[2];
            float hits[2][2];
            q0[0] = (float)x0;
            q0[1] = (float)y0;
            q1[0] = (float)x1;
            q1[1] = (float)y1;
            q2[0] = (float)x2;
            q2[1] = (float)y2;
            if (equal(q0,q1) || equal(q1,q2)) {
               x0 = (int)verts[i-1].x;
               y0 = (int)verts[i-1].y;
               x1 = (int)verts[i  ].x;
               y1 = (int)verts[i  ].y;
               if (y > STBTT_min(y0,y1) && y < STBTT_max(y0,y1) && x > STBTT_min(x0,x1)) {
                  float x_inter = (y - y0) / (y1 - y0) * (x1-x0) + x0;
                  if (x_inter < x)
                     winding += (y0 < y1) ? 1 : -1;
               }
            } else {
               int num_hits = stbtt__ray_intersect_bezier(orig, ray, q0, q1, q2, hits);
               if (num_hits >= 1)
                  if (hits[0][0] < 0)
                     winding += (hits[0][1] < 0 ? -1 : 1);
               if (num_hits >= 2)
                  if (hits[1][0] < 0)
                     winding += (hits[1][1] < 0 ? -1 : 1);
            }
         }
      }
   }
   return winding;
}

static float stbtt__cuberoot( float x )
{
   if (x<0)
      return -(float) STBTT_pow(-x,1.0f/3.0f);
   else
      return  (float) STBTT_pow( x,1.0f/3.0f);
}

static int stbtt__solve_cubic(float a, float b, float c, float* r)
{
   float s = -a / 3;
   float p = b - a*a / 3;
   float q = a * (2*a*a - 9*b) / 27 + c;
   float p3 = p*p*p;
   float d = q*q + 4*p3 / 27;
   if (d >= 0) {
      float z = (float) STBTT_sqrt(d);
      float u = (-q + z) / 2;
      float v = (-q - z) / 2;
      u = stbtt__cuberoot(u);
      v = stbtt__cuberoot(v);
      r[0] = s + u + v;
      return 1;
   } else {
      float u = (float) STBTT_sqrt(-p/3);
      float v = (float) STBTT_acos(-STBTT_sqrt(-27/p3) * q / 2) / 3; 
      float m = (float) STBTT_cos(v);
      float n = (float) STBTT_cos(v-3.141592/2)*1.732050808f;
      r[0] = s + u * 2 * m;
      r[1] = s - u * (m + n);
      r[2] = s - u * (m - n);

      
      
      
      return 3;
   }
}

STBTT_DEF unsigned char * stbtt_GetGlyphSDF(const stbtt_fontinfo *info, float scale, int glyph, int padding, unsigned char onedge_value, float pixel_dist_scale, int *width, int *height, int *xoff, int *yoff)
{
   float scale_x = scale, scale_y = scale;
   int ix0,iy0,ix1,iy1;
   int w,h;
   unsigned char *data;

   if (scale == 0) return NULL;

   stbtt_GetGlyphBitmapBoxSubpixel(info, glyph, scale, scale, 0.0f,0.0f, &ix0,&iy0,&ix1,&iy1);

   
   if (ix0 == ix1 || iy0 == iy1)
      return NULL;

   ix0 -= padding;
   iy0 -= padding;
   ix1 += padding;
   iy1 += padding;

   w = (ix1 - ix0);
   h = (iy1 - iy0);

   if (width ) *width  = w;
   if (height) *height = h;
   if (xoff  ) *xoff   = ix0;
   if (yoff  ) *yoff   = iy0;

   
   scale_y = -scale_y;

   {
      
      const float eps = 1./1024, eps2 = eps*eps;
      int x,y,i,j;
      float *precompute;
      stbtt_vertex *verts;
      int num_verts = stbtt_GetGlyphShape(info, glyph, &verts);
      data = (unsigned char *) STBTT_malloc(w * h, info->userdata);
      precompute = (float *) STBTT_malloc(num_verts * sizeof(float), info->userdata);

      for (i=0,j=num_verts-1; i < num_verts; j=i++) {
         if (verts[i].type == STBTT_vline) {
            float x0 = verts[i].x*scale_x, y0 = verts[i].y*scale_y;
            float x1 = verts[j].x*scale_x, y1 = verts[j].y*scale_y;
            float dist = (float) STBTT_sqrt((x1-x0)*(x1-x0) + (y1-y0)*(y1-y0));
            precompute[i] = (dist < eps) ? 0.0f : 1.0f / dist;
         } else if (verts[i].type == STBTT_vcurve) {
            float x2 = verts[j].x *scale_x, y2 = verts[j].y *scale_y;
            float x1 = verts[i].cx*scale_x, y1 = verts[i].cy*scale_y;
            float x0 = verts[i].x *scale_x, y0 = verts[i].y *scale_y;
            float bx = x0 - 2*x1 + x2, by = y0 - 2*y1 + y2;
            float len2 = bx*bx + by*by;
            if (len2 >= eps2)
               precompute[i] = 1.0f / len2;
            else
               precompute[i] = 0.0f;
         } else
            precompute[i] = 0.0f;
      }

      for (y=iy0; y < iy1; ++y) {
         for (x=ix0; x < ix1; ++x) {
            float val;
            float min_dist = 999999.0f;
            float sx = (float) x + 0.5f;
            float sy = (float) y + 0.5f;
            float x_gspace = (sx / scale_x);
            float y_gspace = (sy / scale_y);

            int winding = stbtt__compute_crossings_x(x_gspace, y_gspace, num_verts, verts); 

            for (i=0; i < num_verts; ++i) {
               float x0 = verts[i].x*scale_x, y0 = verts[i].y*scale_y;

               if (verts[i].type == STBTT_vline && precompute[i] != 0.0f) {
                  float x1 = verts[i-1].x*scale_x, y1 = verts[i-1].y*scale_y;

                  float dist,dist2 = (x0-sx)*(x0-sx) + (y0-sy)*(y0-sy);
                  if (dist2 < min_dist*min_dist)
                     min_dist = (float) STBTT_sqrt(dist2);

                  
                  
                  
                  dist = (float) STBTT_fabs((x1-x0)*(y0-sy) - (y1-y0)*(x0-sx)) * precompute[i];
                  STBTT_assert(i != 0);
                  if (dist < min_dist) {
                     
                     
                     
                     float dx = x1-x0, dy = y1-y0;
                     float px = x0-sx, py = y0-sy;
                     
                     
                     float t = -(px*dx + py*dy) / (dx*dx + dy*dy);
                     if (t >= 0.0f && t <= 1.0f)
                        min_dist = dist;
                  }
               } else if (verts[i].type == STBTT_vcurve) {
                  float x2 = verts[i-1].x *scale_x, y2 = verts[i-1].y *scale_y;
                  float x1 = verts[i  ].cx*scale_x, y1 = verts[i  ].cy*scale_y;
                  float box_x0 = STBTT_min(STBTT_min(x0,x1),x2);
                  float box_y0 = STBTT_min(STBTT_min(y0,y1),y2);
                  float box_x1 = STBTT_max(STBTT_max(x0,x1),x2);
                  float box_y1 = STBTT_max(STBTT_max(y0,y1),y2);
                  
                  if (sx > box_x0-min_dist && sx < box_x1+min_dist && sy > box_y0-min_dist && sy < box_y1+min_dist) {
                     int num=0;
                     float ax = x1-x0, ay = y1-y0;
                     float bx = x0 - 2*x1 + x2, by = y0 - 2*y1 + y2;
                     float mx = x0 - sx, my = y0 - sy;
                     float res[3] = {0.f,0.f,0.f};
                     float px,py,t,it,dist2;
                     float a_inv = precompute[i];
                     if (a_inv == 0.0) { 
                        float a = 3*(ax*bx + ay*by);
                        float b = 2*(ax*ax + ay*ay) + (mx*bx+my*by);
                        float c = mx*ax+my*ay;
                        if (STBTT_fabs(a) < eps2) { 
                           if (STBTT_fabs(b) >= eps2) {
                              res[num++] = -c/b;
                           }
                        } else {
                           float discriminant = b*b - 4*a*c;
                           if (discriminant < 0)
                              num = 0;
                           else {
                              float root = (float) STBTT_sqrt(discriminant);
                              res[0] = (-b - root)/(2*a);
                              res[1] = (-b + root)/(2*a);
                              num = 2; 
                           }
                        }
                     } else {
                        float b = 3*(ax*bx + ay*by) * a_inv; 
                        float c = (2*(ax*ax + ay*ay) + (mx*bx+my*by)) * a_inv;
                        float d = (mx*ax+my*ay) * a_inv;
                        num = stbtt__solve_cubic(b, c, d, res);
                     }
                     dist2 = (x0-sx)*(x0-sx) + (y0-sy)*(y0-sy);
                     if (dist2 < min_dist*min_dist)
                        min_dist = (float) STBTT_sqrt(dist2);

                     if (num >= 1 && res[0] >= 0.0f && res[0] <= 1.0f) {
                        t = res[0], it = 1.0f - t;
                        px = it*it*x0 + 2*t*it*x1 + t*t*x2;
                        py = it*it*y0 + 2*t*it*y1 + t*t*y2;
                        dist2 = (px-sx)*(px-sx) + (py-sy)*(py-sy);
                        if (dist2 < min_dist * min_dist)
                           min_dist = (float) STBTT_sqrt(dist2);
                     }
                     if (num >= 2 && res[1] >= 0.0f && res[1] <= 1.0f) {
                        t = res[1], it = 1.0f - t;
                        px = it*it*x0 + 2*t*it*x1 + t*t*x2;
                        py = it*it*y0 + 2*t*it*y1 + t*t*y2;
                        dist2 = (px-sx)*(px-sx) + (py-sy)*(py-sy);
                        if (dist2 < min_dist * min_dist)
                           min_dist = (float) STBTT_sqrt(dist2);
                     }
                     if (num >= 3 && res[2] >= 0.0f && res[2] <= 1.0f) {
                        t = res[2], it = 1.0f - t;
                        px = it*it*x0 + 2*t*it*x1 + t*t*x2;
                        py = it*it*y0 + 2*t*it*y1 + t*t*y2;
                        dist2 = (px-sx)*(px-sx) + (py-sy)*(py-sy);
                        if (dist2 < min_dist * min_dist)
                           min_dist = (float) STBTT_sqrt(dist2);
                     }
                  }
               }
            }
            if (winding == 0)
               min_dist = -min_dist;  
            val = onedge_value + pixel_dist_scale * min_dist;
            if (val < 0)
               val = 0;
            else if (val > 255)
               val = 255;
            data[(y-iy0)*w+(x-ix0)] = (unsigned char) val;
         }
      }
      STBTT_free(precompute, info->userdata);
      STBTT_free(verts, info->userdata);
   }
   return data;
}

STBTT_DEF unsigned char * stbtt_GetCodepointSDF(const stbtt_fontinfo *info, float scale, int codepoint, int padding, unsigned char onedge_value, float pixel_dist_scale, int *width, int *height, int *xoff, int *yoff)
{
   return stbtt_GetGlyphSDF(info, scale, stbtt_FindGlyphIndex(info, codepoint), padding, onedge_value, pixel_dist_scale, width, height, xoff, yoff);
}

STBTT_DEF void stbtt_FreeSDF(unsigned char *bitmap, void *userdata)
{
   STBTT_free(bitmap, userdata);
}

static stbtt_int32 stbtt__CompareUTF8toUTF16_bigendian_prefix(stbtt_uint8 *s1, stbtt_int32 len1, stbtt_uint8 *s2, stbtt_int32 len2)
{
   stbtt_int32 i=0;

   
   while (len2) {
      stbtt_uint16 ch = s2[0]*256 + s2[1];
      if (ch < 0x80) {
         if (i >= len1) return -1;
         if (s1[i++] != ch) return -1;
      } else if (ch < 0x800) {
         if (i+1 >= len1) return -1;
         if (s1[i++] != 0xc0 + (ch >> 6)) return -1;
         if (s1[i++] != 0x80 + (ch & 0x3f)) return -1;
      } else if (ch >= 0xd800 && ch < 0xdc00) {
         stbtt_uint32 c;
         stbtt_uint16 ch2 = s2[2]*256 + s2[3];
         if (i+3 >= len1) return -1;
         c = ((ch - 0xd800) << 10) + (ch2 - 0xdc00) + 0x10000;
         if (s1[i++] != 0xf0 + (c >> 18)) return -1;
         if (s1[i++] != 0x80 + ((c >> 12) & 0x3f)) return -1;
         if (s1[i++] != 0x80 + ((c >>  6) & 0x3f)) return -1;
         if (s1[i++] != 0x80 + ((c      ) & 0x3f)) return -1;
         s2 += 2; 
         len2 -= 2;
      } else if (ch >= 0xdc00 && ch < 0xe000) {
         return -1;
      } else {
         if (i+2 >= len1) return -1;
         if (s1[i++] != 0xe0 + (ch >> 12)) return -1;
         if (s1[i++] != 0x80 + ((ch >> 6) & 0x3f)) return -1;
         if (s1[i++] != 0x80 + ((ch     ) & 0x3f)) return -1;
      }
      s2 += 2;
      len2 -= 2;
   }
   return i;
}

static int stbtt_CompareUTF8toUTF16_bigendian_internal(char *s1, int len1, char *s2, int len2)
{
   return len1 == stbtt__CompareUTF8toUTF16_bigendian_prefix((stbtt_uint8*) s1, len1, (stbtt_uint8*) s2, len2);
}

STBTT_DEF const char *stbtt_GetFontNameString(const stbtt_fontinfo *font, int *length, int platformID, int encodingID, int languageID, int nameID)
{
   stbtt_int32 i,count,stringOffset;
   stbtt_uint8 *fc = font->data;
   stbtt_uint32 offset = font->fontstart;
   stbtt_uint32 nm = stbtt__find_table(fc, offset, "name");
   if (!nm) return NULL;

   count = ttUSHORT(fc+nm+2);
   stringOffset = nm + ttUSHORT(fc+nm+4);
   for (i=0; i < count; ++i) {
      stbtt_uint32 loc = nm + 6 + 12 * i;
      if (platformID == ttUSHORT(fc+loc+0) && encodingID == ttUSHORT(fc+loc+2)
          && languageID == ttUSHORT(fc+loc+4) && nameID == ttUSHORT(fc+loc+6)) {
         *length = ttUSHORT(fc+loc+8);
         return (const char *) (fc+stringOffset+ttUSHORT(fc+loc+10));
      }
   }
   return NULL;
}

static int stbtt__matchpair(stbtt_uint8 *fc, stbtt_uint32 nm, stbtt_uint8 *name, stbtt_int32 nlen, stbtt_int32 target_id, stbtt_int32 next_id)
{
   stbtt_int32 i;
   stbtt_int32 count = ttUSHORT(fc+nm+2);
   stbtt_int32 stringOffset = nm + ttUSHORT(fc+nm+4);

   for (i=0; i < count; ++i) {
      stbtt_uint32 loc = nm + 6 + 12 * i;
      stbtt_int32 id = ttUSHORT(fc+loc+6);
      if (id == target_id) {
         
         stbtt_int32 platform = ttUSHORT(fc+loc+0), encoding = ttUSHORT(fc+loc+2), language = ttUSHORT(fc+loc+4);

         
         if (platform == 0 || (platform == 3 && encoding == 1) || (platform == 3 && encoding == 10)) {
            stbtt_int32 slen = ttUSHORT(fc+loc+8);
            stbtt_int32 off = ttUSHORT(fc+loc+10);

            
            stbtt_int32 matchlen = stbtt__CompareUTF8toUTF16_bigendian_prefix(name, nlen, fc+stringOffset+off,slen);
            if (matchlen >= 0) {
               
               if (i+1 < count && ttUSHORT(fc+loc+12+6) == next_id && ttUSHORT(fc+loc+12) == platform && ttUSHORT(fc+loc+12+2) == encoding && ttUSHORT(fc+loc+12+4) == language) {
                  slen = ttUSHORT(fc+loc+12+8);
                  off = ttUSHORT(fc+loc+12+10);
                  if (slen == 0) {
                     if (matchlen == nlen)
                        return 1;
                  } else if (matchlen < nlen && name[matchlen] == ' ') {
                     ++matchlen;
                     if (stbtt_CompareUTF8toUTF16_bigendian_internal((char*) (name+matchlen), nlen-matchlen, (char*)(fc+stringOffset+off),slen))
                        return 1;
                  }
               } else {
                  
                  if (matchlen == nlen)
                     return 1;
               }
            }
         }

         
      }
   }
   return 0;
}

static int stbtt__matches(stbtt_uint8 *fc, stbtt_uint32 offset, stbtt_uint8 *name, stbtt_int32 flags)
{
   stbtt_int32 nlen = (stbtt_int32) STBTT_strlen((char *) name);
   stbtt_uint32 nm,hd;
   if (!stbtt__isfont(fc+offset)) return 0;

   
   if (flags) {
      hd = stbtt__find_table(fc, offset, "head");
      if ((ttUSHORT(fc+hd+44) & 7) != (flags & 7)) return 0;
   }

   nm = stbtt__find_table(fc, offset, "name");
   if (!nm) return 0;

   if (flags) {
      
      if (stbtt__matchpair(fc, nm, name, nlen, 16, -1))  return 1;
      if (stbtt__matchpair(fc, nm, name, nlen,  1, -1))  return 1;
      if (stbtt__matchpair(fc, nm, name, nlen,  3, -1))  return 1;
   } else {
      if (stbtt__matchpair(fc, nm, name, nlen, 16, 17))  return 1;
      if (stbtt__matchpair(fc, nm, name, nlen,  1,  2))  return 1;
      if (stbtt__matchpair(fc, nm, name, nlen,  3, -1))  return 1;
   }

   return 0;
}

static int stbtt_FindMatchingFont_internal(unsigned char *font_collection, char *name_utf8, stbtt_int32 flags)
{
   stbtt_int32 i;
   for (i=0;;++i) {
      stbtt_int32 off = stbtt_GetFontOffsetForIndex(font_collection, i);
      if (off < 0) return off;
      if (stbtt__matches((stbtt_uint8 *) font_collection, off, (stbtt_uint8*) name_utf8, flags))
         return off;
   }
}

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

STBTT_DEF int stbtt_BakeFontBitmap(const unsigned char *data, int offset,
                                float pixel_height, unsigned char *pixels, int pw, int ph,
                                int first_char, int num_chars, stbtt_bakedchar *chardata)
{
   return stbtt_BakeFontBitmap_internal((unsigned char *) data, offset, pixel_height, pixels, pw, ph, first_char, num_chars, chardata);
}

STBTT_DEF int stbtt_GetFontOffsetForIndex(const unsigned char *data, int index)
{
   return stbtt_GetFontOffsetForIndex_internal((unsigned char *) data, index);
}

STBTT_DEF int stbtt_GetNumberOfFonts(const unsigned char *data)
{
   return stbtt_GetNumberOfFonts_internal((unsigned char *) data);
}

STBTT_DEF int stbtt_InitFont(stbtt_fontinfo *info, const unsigned char *data, int offset)
{
   return stbtt_InitFont_internal(info, (unsigned char *) data, offset);
}

STBTT_DEF int stbtt_FindMatchingFont(const unsigned char *fontdata, const char *name, int flags)
{
   return stbtt_FindMatchingFont_internal((unsigned char *) fontdata, (char *) name, flags);
}

STBTT_DEF int stbtt_CompareUTF8toUTF16_bigendian(const char *s1, int len1, const char *s2, int len2)
{
   return stbtt_CompareUTF8toUTF16_bigendian_internal((char *) s1, len1, (char *) s2, len2);
}

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#endif 


#pragma GCC diagnostic pop

#ifdef TYPE_DEFINED_STBTT_MALLOC
#undef STBTT_malloc
#undef TYPE_DEFINED_STBTT_MALLOC
#endif
#ifdef TYPE_DEFINED_STBTT_FREE
#undef STBTT_free
#undef TYPE_DEFINED_STBTT_FREE
#endif
#ifdef TYPE_DEFINED_STBTT_STATIC
#undef STBTT_STATIC
#undef TYPE_DEFINED_STBTT_STATIC
#endif
#ifdef TYPE_DEFINED_STB_TRUETYPE_IMPLEMENTATION
#undef STB_TRUETYPE_IMPLEMENTATION
#undef TYPE_DEFINED_STB_TRUETYPE_IMPLEMENTATION
#endif

struct TypeTTF {
	const uint8_t *bytes;
	size_t len;
	TypeError err;
	int outline;
	uint16_t units_per_em;
	int16_t ascent;
	int16_t descent;
	int16_t line_gap;
	int num_glyphs;
	stbtt_fontinfo info;
};

struct TypeFont {
	const struct TypeTTF *ttf;
	uint16_t weight;
	TypeStyle style;
};

static int16_t
ttf_clamp_i16(int v)
{
	if (v > 32767)
		return 32767;
	if (v < -32768)
		return (int16_t)-32768;
	return (int16_t)v;
}

static uint16_t
ttf_upem(const struct TypeTTF *font)
{
	const uint8_t *p;

	if (!font || !font->info.data || font->info.head <= 0)
		return 0;
	p = font->info.data + font->info.head + 18;
	return (uint16_t)((p[0] << 8) | p[1]);
}

size_t
type_ttf_memory(void)
{
	return sizeof (struct TypeTTF);
}

TypeTTF *
type_ttf_place(void *buf, size_t bufsize, const void *bytes, size_t len)
{
	struct TypeTTF *font;
	int ascent, descent, gap;
	int offset;

	if (!buf || bufsize < sizeof (struct TypeTTF) || !bytes || len < 12)
		return NULL;
	font = buf;
	memset(font, 0, sizeof *font);
	font->bytes = bytes;
	font->len = len;
	font->err = TYPE_ERR_INVALID_FONT;
	offset = stbtt_GetFontOffsetForIndex(bytes, 0);
	if (offset < 0)
		return NULL;
	ttf_scratch_reset();
	if (!stbtt_InitFont(&font->info, (const unsigned char *)bytes, offset))
		return NULL;
	if (!font->info.glyf)
		return NULL;
	font->units_per_em = ttf_upem(font);
	if (font->units_per_em == 0)
		return NULL;
	stbtt_GetFontVMetrics(&font->info, &ascent, &descent, &gap);
	font->ascent = ttf_clamp_i16(ascent);
	font->descent = ttf_clamp_i16(descent);
	font->line_gap = ttf_clamp_i16(gap);
	font->num_glyphs = font->info.numGlyphs;
	font->outline = 1;
	font->err = TYPE_OK;
	return font;
}

TypeError
type_ttf_error(const TypeTTF *font)
{
	if (!font)
		return TYPE_ERR_INVALID_FONT;
	return font->err;
}

int
type_ttf_outline(const TypeTTF *font)
{
	return font && font->outline;
}

int
type_ttf_color(const TypeTTF *font)
{
	(void)font;
	return 0;
}

size_t
type_font_memory(void)
{
	return sizeof (struct TypeFont);
}

TypeFont *
type_font_from_ttf(void *buf, size_t bufsize, const TypeTTF *ttf, const TypeFaceParams *face)
{
	struct TypeFont *font;
	TypeFaceParams def;

	if (!buf || bufsize < sizeof (struct TypeFont) || !ttf || ttf->err != TYPE_OK)
		return NULL;
	if (!face) {
		type_face_params_default(&def);
		face = &def;
	}
	font = buf;
	memset(font, 0, sizeof *font);
	font->ttf = ttf;
	font->style = face->style;
	if (face->weight)
		font->weight = face->weight;
	else if (face->style.bold)
		font->weight = TYPE_WEIGHT_BOLD;
	else
		font->weight = TYPE_WEIGHT_REGULAR;
	return font;
}

uint16_t
type_font_weight(const TypeFont *font)
{
	if (!font)
		return 0;
	return font->weight;
}

TypeStyle
type_font_style(const TypeFont *font)
{
	TypeStyle style;

	memset(&style, 0, sizeof style);
	if (!font)
		return style;
	return font->style;
}

TypeError
type_font_metrics(const TypeFont *font, float size_px, TypeMetrics *out)
{
	float scale;

	if (!font || !font->ttf || !out || !(size_px > 0.0f))
		return TYPE_ERR_INVALID_FONT;
	scale = size_px / (float)font->ttf->units_per_em;
	out->ascender = (float)font->ttf->ascent * scale;
	out->descender = (float)font->ttf->descent * scale;
	out->line_gap = (float)font->ttf->line_gap * scale;
	out->units_per_em = font->ttf->units_per_em;
	return TYPE_OK;
}

TypeError
type_font_glyph_index(const TypeFont *font, uint32_t codepoint, uint16_t *out_gid)
{
	int g;

	if (!font || !font->ttf || !out_gid)
		return TYPE_ERR_INVALID_FONT;
	if (codepoint > 0x10FFFFu)
		return TYPE_ERR_GLYPH_NOT_FOUND;
	g = stbtt_FindGlyphIndex(&font->ttf->info, (int)codepoint);
	if (g <= 0)
		return TYPE_ERR_GLYPH_NOT_FOUND;
	*out_gid = (uint16_t)g;
	return TYPE_OK;
}

TypeError
type_font_advance(const TypeFont *font, uint16_t glyph_id, uint16_t *out_fu)
{
	int adv, lsb;

	if (!font || !font->ttf || !out_fu)
		return TYPE_ERR_INVALID_FONT;
	if (glyph_id >= font->ttf->num_glyphs)
		return TYPE_ERR_GLYPH_NOT_FOUND;
	stbtt_GetGlyphHMetrics(&font->ttf->info, glyph_id, &adv, &lsb);
	if (adv < 0)
		adv = 0;
	if (adv > 65535)
		adv = 65535;
	*out_fu = (uint16_t)adv;
	(void)lsb;
	return TYPE_OK;
}

int
type_font_has_drawable(const TypeFont *font, uint16_t glyph_id)
{
	int x0, y0, x1, y1;

	if (!font || !font->ttf || glyph_id >= font->ttf->num_glyphs)
		return 0;
	if (!stbtt_GetGlyphBox(&font->ttf->info, glyph_id, &x0, &y0, &x1, &y1))
		return 0;
	return x0 < x1 && y0 < y1;
}

TypeError
type_font_bitmap_info(const TypeFont *font, uint16_t glyph_id, float size_px, TypeBitmap *out)
{
	float scale;
	int x0, y0, x1, y1;
	int adv, lsb;
	int w, h;
	uint16_t fu;

	if (!font || !font->ttf || !out || !(size_px > 0.0f))
		return TYPE_ERR_INVALID_FONT;
	if (glyph_id >= font->ttf->num_glyphs)
		return TYPE_ERR_GLYPH_NOT_FOUND;
	memset(out, 0, sizeof *out);
	scale = stbtt_ScaleForMappingEmToPixels(&font->ttf->info, size_px);
	stbtt_GetGlyphHMetrics(&font->ttf->info, glyph_id, &adv, &lsb);
	if (adv < 0)
		adv = 0;
	fu = (uint16_t)(adv > 65535 ? 65535 : adv);
	out->advance = (uint16_t)(fu * scale + 0.5f);
	stbtt_GetGlyphBitmapBox(&font->ttf->info, glyph_id, scale, scale, &x0, &y0, &x1, &y1);
	w = x1 - x0;
	h = y1 - y0;
	if (w <= 0 || h <= 0)
		return TYPE_OK;
	if (w > 65535 || h > 65535)
		return TYPE_ERR_INVALID_FONT;
	out->width = (uint16_t)w;
	out->height = (uint16_t)h;
	out->bearing_x = ttf_clamp_i16(x0);
	out->bearing_y = ttf_clamp_i16(-y0);
	return TYPE_OK;
}

size_t
type_bitmap_bytes(const TypeBitmap *info)
{
	size_t n;

	if (!info)
		return 0;
	n = (size_t)info->width * (size_t)info->height;
	if (info->color)
		n *= 4;
	return n;
}

TypeError
type_font_render_bitmap(const TypeFont *font, uint16_t glyph_id, float size_px, TypeAntiAlias aa, void *pixels, size_t len, TypeBitmap *out)
{
	TypeBitmap info;
	TypeError err;
	float scale;
	size_t need;
	size_t i;
	uint8_t *dst;

	if (aa == TYPE_AA_SDF)
		return TYPE_ERR_UNIMPLEMENTED;
	err = type_font_bitmap_info(font, glyph_id, size_px, &info);
	if (err != TYPE_OK)
		return err;
	need = type_bitmap_bytes(&info);
	if (need == 0) {
		if (out)
			*out = info;
		return TYPE_OK;
	}
	if (!pixels || len < need)
		return TYPE_ERR_BUF_TOO_SMALL;
	scale = stbtt_ScaleForMappingEmToPixels(&font->ttf->info, size_px);
	ttf_scratch_reset();
	dst = pixels;
	stbtt_MakeGlyphBitmap(&font->ttf->info, dst, info.width, info.height, info.width, scale, scale, glyph_id);
	if (aa == TYPE_AA_NONE) {
		for (i = 0; i < need; i++)
			dst[i] = dst[i] > 127 ? 255 : 0;
	}
	info.pixels = dst;
	if (out)
		*out = info;
	return TYPE_OK;
}

TypeError
type_font_render_sdf(const TypeFont *font, uint16_t glyph_id, float size_px, void *pixels, size_t len, TypeBitmap *out)
{
	(void)font;
	(void)glyph_id;
	(void)size_px;
	(void)pixels;
	(void)len;
	(void)out;
	return TYPE_ERR_UNIMPLEMENTED;
}

/* t_otf.c - OpenType / CFF faces. Copyright (c) 2025-2026 Vasco Alves
 * CFF outlines are not implemented yet.
 */


size_t
type_otf_memory(void)
{
	return 0;
}

TypeOTF *
type_otf_place(void *buf, size_t bufsize, const void *bytes, size_t len)
{
	(void)buf;
	(void)bufsize;
	(void)bytes;
	(void)len;
	return NULL;
}

TypeError
type_otf_error(const TypeOTF *font)
{
	(void)font;
	return TYPE_ERR_UNIMPLEMENTED;
}

int
type_otf_outline(const TypeOTF *font)
{
	(void)font;
	return 0;
}

int
type_otf_color(const TypeOTF *font)
{
	(void)font;
	return 0;
}

TypeFont *
type_font_from_otf(void *buf, size_t bufsize, const TypeOTF *otf, const TypeFaceParams *face)
{
	(void)buf;
	(void)bufsize;
	(void)otf;
	(void)face;
	return NULL;
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
	uint8_t *scratch;
	size_t scratch_n;
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
	size_t scratch;
	size_t scratch_n;
	size_t face_stride;
	size_t face_bytes;
} TypeLayout;

static size_t type_align(size_t v, size_t a);
static int type_layout(const TypeParams *params, TypeLayout *lay);
static uint16_t type_size_px(float size_px);
static void type_atlas_reset(struct TypeCtx *ctx);
static int type_pack(struct TypeCtx *ctx, uint32_t w, uint32_t h, TypeRect *out);
static FaceRec *type_face(const struct TypeCtx *ctx, uint32_t id);
static TypeError type_raster(struct TypeCtx *ctx, FaceRec *face, uint16_t gid, uint16_t size_u, TypeGlyph *out);
static TypeError type_resolve(struct TypeCtx *ctx, uint32_t codepoint, TypeStyle style, FaceRec **face, uint16_t *gid);
static uint64_t type_now_ns(void);
static int type_cache_find(struct TypeCtx *ctx, const TypeGlyphKey *key, TypeGlyph *out, int touch);
static void type_cache_put(struct TypeCtx *ctx, const TypeGlyphKey *key, const TypeGlyph *glyph);
static void type_ascii_stash(struct TypeCtx *ctx);

static size_t
type_align(size_t v, size_t a)
{
	return (v + a - 1u) & ~(a - 1u);
}

static int
type_layout(const TypeParams *params, TypeLayout *lay)
{
	size_t o;
	if (!params || !lay)
		return 0;
	if (params->atlas_width == 0 || params->atlas_height == 0 || params->cache_capacity == 0)
		return 0;
	if (params->max_faces == 0)
		return 0;
	memset(lay, 0, sizeof *lay);
	lay->face_stride = type_align(type_ttf_memory(), 16) + type_align(type_font_memory(), 16);
	o = type_align(sizeof (struct TypeCtx), 16);
	lay->faces = o;
	o = type_align(o + (size_t)params->max_faces * sizeof (FaceRec), 16);
	lay->face_bytes = o;
	o = type_align(o + (size_t)params->max_faces * lay->face_stride, 16);
	lay->cov = o;
	o = type_align(o + (size_t)params->atlas_width * (size_t)params->atlas_height, 16);
	lay->rgba = o;
	o = type_align(o + (size_t)params->atlas_width * (size_t)params->atlas_height * 4u, 16);
	lay->cache = o;
	o = type_align(o + (size_t)params->cache_capacity * sizeof (CacheSlot), 16);
	lay->scratch = o;
	lay->scratch_n = 256u * 256u;
	o += lay->scratch_n;
	lay->total = o;
	return 1;
}

void
type_params_default(TypeParams *params)
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

void
type_face_params_default(TypeFaceParams *face)
{
	if (!face)
		return;
	memset(face, 0, sizeof *face);
}

size_t
type_memory(const TypeParams *params)
{
	TypeLayout lay;

	if (!type_layout(params, &lay))
		return 0;
	return lay.total;
}

TypeCtx *
type_place(void *buf, size_t bufsize, const TypeParams *params)
{
	struct TypeCtx *ctx;
	TypeLayout lay;
	unsigned char *base;

	if (!buf || !type_layout(params, &lay) || bufsize < lay.total)
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
	ctx->scratch = base + lay.scratch;
	ctx->scratch_n = lay.scratch_n;
	return ctx;
}

static uint16_t
type_size_px(float size_px)
{
	uint32_t s;

	if (!(size_px > 0.0f))
		return 0;
	s = (uint32_t)(size_px + 0.5f);
	if (s < 1)
		s = 1;
	if (s > 65535)
		s = 65535;
	return (uint16_t)s;
}

static void
type_atlas_reset(struct TypeCtx *ctx)
{
	ctx->atlas_revision++;
	memset(ctx->cov, 0, (size_t)ctx->aw * (size_t)ctx->ah);
	memset(ctx->rgba, 0, (size_t)ctx->aw * (size_t)ctx->ah * 4u);
	memset(ctx->cache, 0, (size_t)ctx->cache_n * sizeof (CacheSlot));
	memset(ctx->ascii_ok, 0, sizeof ctx->ascii_ok);
	ctx->replacement_ok = 0;
	ctx->shelf_x = 0;
	ctx->shelf_y = 0;
	ctx->shelf_h = 0;
	ctx->stamp = 1;
}

static int
type_pack(struct TypeCtx *ctx, uint32_t w, uint32_t h, TypeRect *out)
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

static FaceRec *
type_face(const struct TypeCtx *ctx, uint32_t id)
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

static uint64_t
type_now_ns(void)
{
	struct timespec ts;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
		return 0;
	return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static void
type_ascii_stash(struct TypeCtx *ctx)
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

static TypeError
type_raster(struct TypeCtx *ctx, FaceRec *face, uint16_t gid, uint16_t size_u, TypeGlyph *out)
{
	TypeBitmap info;
	TypeError err;
	TypeRect rect;
	uint16_t adv;
	float scale;
	size_t need;
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
	if (need > ctx->scratch_n)
		return TYPE_ERR_BUF_TOO_SMALL;
	t0 = type_now_ns();
	err = type_font_render_bitmap(face->font, gid, (float)size_u, aa, ctx->scratch, ctx->scratch_n, &info);
	t1 = type_now_ns();
	if (t1 > t0)
		ctx->stats.raster_ns += t1 - t0;
	if (err != TYPE_OK)
		return err;
	if (!type_pack(ctx, info.width, info.height, &rect))
		return TYPE_ERR_ATLAS_FULL;
	t0 = type_now_ns();
	for (row = 0; row < info.height; row++) {
		memcpy(ctx->cov + ((size_t)(rect.y + row) * ctx->aw + rect.x),
		       ctx->scratch + (size_t)row * info.width,
		       info.width);
	}
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

static int
type_cache_find(struct TypeCtx *ctx, const TypeGlyphKey *key, TypeGlyph *out, int touch)
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

static void
type_cache_put(struct TypeCtx *ctx, const TypeGlyphKey *key, const TypeGlyph *glyph)
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

static TypeError
type_resolve(struct TypeCtx *ctx, uint32_t codepoint, TypeStyle style, FaceRec **face, uint16_t *gid)
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

TypeError
type_add_font(TypeCtx *ctx, const void *bytes, size_t len, const TypeFaceParams *face, uint32_t *out_id)
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
		ttf = type_ttf_place(slot, type_ttf_memory(), bytes, len);
		if (!ttf || !type_ttf_outline(ttf))
			return TYPE_ERR_INVALID_FONT;
		font = type_font_from_ttf(slot + type_align(type_ttf_memory(), 16), type_font_memory(), ttf, face);
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

void
type_clear_fonts(TypeCtx *ctx)
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

void
type_select(TypeCtx *ctx, uint32_t font_id)
{
	if (!ctx || !type_face(ctx, font_id))
		return;
	type_ascii_stash(ctx);
	ctx->primary = font_id;
	ctx->ascii_size = 0;
	memset(ctx->ascii_ok, 0, sizeof ctx->ascii_ok);
	ctx->replacement_ok = 0;
}

TypeError
type_set_fallbacks(TypeCtx *ctx, const uint32_t *ids, uint32_t count)
{
	uint32_t i;

	if (!ctx)
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

uint64_t
type_ctx_atlas_revision(const TypeCtx *ctx)
{
	return ctx ? ctx->atlas_revision : 0;
}

const uint8_t *
type_ctx_atlas(const TypeCtx *ctx, uint32_t *width, uint32_t *height)
{
	if (!ctx)
		return NULL;
	if (width)
		*width = ctx->aw;
	if (height)
		*height = ctx->ah;
	return ctx->cov;
}

const uint32_t *
type_ctx_color_atlas(const TypeCtx *ctx, uint32_t *width, uint32_t *height)
{
	if (!ctx)
		return NULL;
	if (width)
		*width = ctx->aw;
	if (height)
		*height = ctx->ah;
	return ctx->rgba;
}

TypeError
type_metrics(const TypeCtx *ctx, float size_px, TypeMetrics *out)
{
	FaceRec *face;

	if (!ctx || !out)
		return TYPE_ERR_INVALID_FONT;
	face = type_face(ctx, ctx->primary);
	if (!face)
		return TYPE_ERR_INVALID_FONT;
	return type_font_metrics(face->font, size_px, out);
}

static TypeError
type_glyph_styled_inner(struct TypeCtx *ctx, uint32_t codepoint, float size_px, TypeStyle style, TypeGlyph *out)
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
	styled = style.bold || style.italic;
	if (!ctx->warming && size_u != ctx->ascii_size) {
		ctx->warming = 1;
		err = type_warm_ascii(ctx, (float)size_u);
		ctx->warming = 0;
		if (err != TYPE_OK)
			return err;
	}
	if (!styled && codepoint >= TYPE_ASCII_LO && codepoint <= TYPE_ASCII_HI && size_u == ctx->ascii_size) {
		slot = codepoint - TYPE_ASCII_LO;
		if (ctx->ascii_ok[slot]) {
			ctx->stats.hits++;
			*out = ctx->ascii[slot];
			return TYPE_OK;
		}
	}
	if (!styled && codepoint == TYPE_REPLACEMENT && ctx->replacement_ok && size_u == ctx->ascii_size) {
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
		if (!styled && codepoint >= TYPE_ASCII_LO && codepoint <= TYPE_ASCII_HI && size_u == ctx->ascii_size) {
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

TypeError
type_glyph(TypeCtx *ctx, uint32_t codepoint, float size_px, TypeGlyph *out)
{
	TypeStyle style;

	memset(&style, 0, sizeof style);
	return type_glyph_styled_inner(ctx, codepoint, size_px, style, out);
}

TypeError
type_glyph_styled(TypeCtx *ctx, uint32_t codepoint, float size_px, TypeStyle style, TypeGlyph *out)
{
	return type_glyph_styled_inner(ctx, codepoint, size_px, style, out);
}

TypeError
type_warm_ascii(TypeCtx *ctx, float size_px)
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

TypeError
type_peek_glyph_styled(const TypeCtx *ctx, uint32_t codepoint, float size_px, TypeStyle style, TypeGlyph *out)
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

TypeError
type_peek_glyph(const TypeCtx *ctx, uint32_t codepoint, float size_px, TypeGlyph *out)
{
	TypeStyle style;

	memset(&style, 0, sizeof style);
	return type_peek_glyph_styled(ctx, codepoint, size_px, style, out);
}

void
type_clear_atlas(TypeCtx *ctx)
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

void
type_stats(const TypeCtx *ctx, TypeGlyphStats *out)
{
	if (!out)
		return;
	if (!ctx) {
		memset(out, 0, sizeof *out);
		return;
	}
	*out = ctx->stats;
}

uint32_t
type_utf8_next(const char *s, size_t len, uint32_t *codepoint)
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

int
type_utf8_validate(const char *s, size_t len)
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

TypeError
type_layout_utf8(TypeCtx *ctx, const char *text, size_t len, float size_px, TypeCell *out, uint32_t cap, uint32_t *written, size_t *bytes_used)
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

TypeWidth
type_width(uint32_t codepoint)
{
	(void)codepoint;
	return TYPE_WIDTH_NEUTRAL;
}

int
type_is_cjk(uint32_t codepoint)
{
	(void)codepoint;
	return 0;
}

int
type_is_emoji(uint32_t codepoint)
{
	(void)codepoint;
	return 0;
}

int
type_cell_width(uint32_t codepoint)
{
	(void)codepoint;
	return 1;
}

int
type_is_combining(uint32_t codepoint)
{
	(void)codepoint;
	return 0;
}

#endif /* TYPE_IMPLEMENTATION && !TYPE_IMPLEMENTATION_ONCE */
