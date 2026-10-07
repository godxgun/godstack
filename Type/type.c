/* type.c - Faces, atlas, and UTF-8 layout. Copyright (c) 2025-2026 Vasco Alves
 * Caller provides the buffer from type_memory. Font bytes are borrowed.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "type.h"
#include "t_ttf.c"
#include "t_otf.c"

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
