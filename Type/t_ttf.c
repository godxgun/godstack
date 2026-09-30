/* t_ttf.c - TrueType glyf faces. Copyright (c) 2025-2026 Vasco Alves
 * Raster is stb_truetype. Shape scratch is a fixed buffer, reset each call.
 * Font bytes are borrowed for the life of the placed face.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "type.h"

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

#define STBTT_malloc(size, user) ttf_scratch_alloc(size, user)
#define STBTT_free(p, user) ttf_scratch_free(p, user)
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#include "stb_truetype.h"
#pragma GCC diagnostic pop

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
