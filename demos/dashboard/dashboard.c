/* Fuse dashboard that stress-tests Type: many faces, sizes, styles,
 * UTF-8, fallbacks, atlas fill, and the glyph LRU. Coverage is drawn
 * 1:1 with the framebuffer (pixel-snapped, one texel per sample).
 */

#define PEAK_IMPLEMENTATION
#include "Peak.h"
#define REND_IMPLEMENTATION
#include "Rend.h"
#define FUSE_IMPLEMENTATION
#include "Fuse.h"
#define FUSE_REND_IMPLEMENTATION
#include "FuseRend.h"
#define TYPE_IMPLEMENTATION
#include "Type.h"
#include "demos/headless.h"

#include <math.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#define COL_BG       0xFF0E1116u
#define COL_SIDE     0xFF161B22u
#define COL_TOP      0xFF1C2330u
#define COL_CARD     0xFF212833u
#define COL_BTN      0xFF2D3644u
#define COL_BTN_H    0xFF3D4A5Cu
#define COL_ACCENT   0xFF3B82F6u
#define COL_ACCENT_H 0xFF60A5FAu
#define COL_OK       0xFF22C55Eu
#define COL_WARN     0xFFF59E0Bu
#define COL_BAD      0xFFEF4444u
#define COL_TRACK    0xFF111827u
#define COL_NOB      0xFFF8FAFCu
#define COL_TEXT     0xFFE6EDF3u
#define COL_MUTED    0xFF93A1B5u

#define MAX_UI 256
#define MAX_VERTS 65536
#define DASH_TEXT_VERTS 16384
#define TEXT_UI 22.0f
#define TEXT_SMALL 15.0f
#define MAX_KEEP 8

enum {
	PAGE_SPECIMEN = 0,
	PAGE_STYLES,
	PAGE_SCRIPTS,
	PAGE_CACHE,
	PAGE_COUNT
};

typedef struct DashVert {
	float x, y, u, v;
	float r, g, b, a;
} DashVert;

typedef struct DashPC {
	float screen_w, screen_h, atlas_w, atlas_h;
} DashPC;

typedef struct DashText {
	RendPipeline pipeline;
	RendBuffer vbo;
	DashVert *verts;
	size_t cap;
	size_t count;
	float atlas_w, atlas_h;
} DashText;

typedef struct DashFonts {
	uint32_t regular;
	uint32_t blue;
	uint32_t cjk;
	int have_regular;
	int have_blue;
	int have_bold;
	int have_italic;
	int have_bi;
	int have_cjk;
	int nfaces;
	char note[192];
	char probe[192];
} DashFonts;

static PeakCtx *peak_demo_ctx;

static uint8_t *zip_ttf(const uint8_t *zip, size_t n, size_t *out_len);
static uint8_t *dash_file(const char *a, const char *b, size_t *out_n);
static int dash_text_init(DashText *dt, DashVert *verts, size_t cap, RendRenderer renderer, uint8_t *vert_spv, size_t vert_n, uint8_t *frag_spv, size_t frag_n);
static void dash_text_begin(DashText *dt, float atlas_w, float atlas_h);
static void dash_text_flush(DashText *dt, RendRenderer renderer, float screen_w, float screen_h);
static void dash_emit(DashText *dt, float x, float y, float w, float h, float u0, float v0, float u1, float v1, uint32_t color);
static void dash_glyph(DashText *dt, float x, float y, const TypeGlyph *g, uint32_t color);
static float dash_styled(DashText *dt, TypeCtx *type, float x, float y, float size, const char *s, TypeStyle style, uint32_t color);
static float dash_string(DashText *dt, TypeCtx *type, float x, float y, float size, const char *s, uint32_t color);
static void dash_string_box(DashText *dt, TypeCtx *type, float x, float y, float w, float h, float size, const char *s, uint32_t color, int center);
static int dash_add_face(TypeCtx *type, uint8_t **keep, int *nkeep, const char *path, int bold, int italic, uint32_t *id);
static void dash_load_faces(TypeCtx *type, uint8_t **keep, int *nkeep, DashFonts *fonts);
static void dash_use(TypeCtx *type, DashFonts *fonts, int blue);
static void dash_probe(TypeCtx *type, DashFonts *fonts);
static void dash_churn(TypeCtx *type, int count, float size, int *resets);
static void dash_pages(DashText *dt, TypeCtx *type, DashFonts *fonts, int page, float W, float H, float churn, int *resets);
static const char *dash_err(TypeError e);

static int atlas_resets;
static uint32_t churn_cp = 0x00A1u;

static uint32_t
zip_u32(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint8_t *
zip_ttf(const uint8_t *zip, size_t n, size_t *out_len)
{
	const uint8_t *p;
	const uint8_t *end;

	if (!zip || !out_len)
		return NULL;
	p = zip;
	end = zip + n;
	while (p + 30 < end) {
		uint32_t method, nl, el, cs, us;
		const uint8_t *name;
		const uint8_t *data;

		if (!(p[0] == 0x50 && p[1] == 0x4b && p[2] == 0x03 && p[3] == 0x04)) {
			p++;
			continue;
		}
		method = (uint32_t)p[8] | ((uint32_t)p[9] << 8);
		cs = zip_u32(p + 18);
		us = zip_u32(p + 22);
		nl = (uint32_t)p[26] | ((uint32_t)p[27] << 8);
		el = (uint32_t)p[28] | ((uint32_t)p[29] << 8);
		if (p + 30 + nl + el + cs > end)
			return NULL;
		name = p + 30;
		data = name + nl + el;
		if (nl >= 4 && memcmp(name + nl - 4, ".ttf", 4) == 0) {
			uint8_t *font;
			z_stream strm;
			int z;

			font = malloc(us ? us : 1);
			if (!font)
				return NULL;
			if (method == 0) {
				memcpy(font, data, us);
			} else if (method == 8) {
				memset(&strm, 0, sizeof strm);
				if (inflateInit2(&strm, -15) != Z_OK) {
					free(font);
					return NULL;
				}
				strm.next_in = (Bytef *)data;
				strm.avail_in = cs;
				strm.next_out = font;
				strm.avail_out = us;
				z = inflate(&strm, Z_FINISH);
				inflateEnd(&strm);
				if (z != Z_STREAM_END) {
					free(font);
					return NULL;
				}
			} else {
				free(font);
				return NULL;
			}
			*out_len = us;
			return font;
		}
		p = data + cs;
	}
	return NULL;
}

static uint8_t *
dash_file(const char *a, const char *b, size_t *out_n)
{
	unsigned long n;
	uint8_t *p;

	n = 0;
	p = peak_file_alloc(a, &n);
	if (!p && b)
		p = peak_file_alloc(b, &n);
	if (out_n)
		*out_n = p ? (size_t)n : 0;
	return p;
}

static const char *
dash_err(TypeError e)
{
	switch (e) {
	case TYPE_OK: return "ok";
	case TYPE_ERR_INVALID_FONT: return "invalid font";
	case TYPE_ERR_UNSUPPORTED_TABLE: return "unsupported table";
	case TYPE_ERR_GLYPH_NOT_FOUND: return "missing glyph";
	case TYPE_ERR_ATLAS_FULL: return "atlas full";
	case TYPE_ERR_INVALID_UTF8: return "bad utf8";
	case TYPE_ERR_UNIMPLEMENTED: return "unimplemented";
	case TYPE_ERR_BUF_TOO_SMALL: return "buf too small";
	default: return "error";
	}
}

static void
dash_color(uint32_t c, float *r, float *g, float *b, float *a)
{
	*a = ((c >> 24) & 0xFFu) / 255.0f;
	*r = ((c >> 16) & 0xFFu) / 255.0f;
	*g = ((c >> 8) & 0xFFu) / 255.0f;
	*b = (c & 0xFFu) / 255.0f;
}

static int
dash_text_init(DashText *dt, DashVert *verts, size_t cap, RendRenderer renderer,
               uint8_t *vert_spv, size_t vert_n, uint8_t *frag_spv, size_t frag_n)
{
	RendVertexBinding bind;
	RendVertexAttributes attrs[3];
	RendPushConstantInfo pc;

	if (!dt || !verts || cap < 6 || !renderer || !vert_spv || !frag_spv)
		return 0;
	memset(dt, 0, sizeof *dt);
	dt->verts = verts;
	dt->cap = cap;
	bind.binding = 0;
	bind.stride = sizeof (DashVert);
	bind.input_rate = REND_INPUT_RATE_VERTEX;
	attrs[0].location = 0;
	attrs[0].binding = 0;
	attrs[0].offset = offsetof(DashVert, x);
	attrs[0].format = REND_FORMAT_2_SFLOAT32;
	attrs[1].location = 1;
	attrs[1].binding = 0;
	attrs[1].offset = offsetof(DashVert, u);
	attrs[1].format = REND_FORMAT_2_SFLOAT32;
	attrs[2].location = 2;
	attrs[2].binding = 0;
	attrs[2].offset = offsetof(DashVert, r);
	attrs[2].format = REND_FORMAT_4_SFLOAT32;
	pc.offset = 0;
	pc.size = sizeof (DashPC);
	dt->pipeline = rend_pipeline_create_graphics_spirv(
		renderer,
		vert_spv, vert_n,
		frag_spv, frag_n,
		&bind, 1,
		attrs, 3,
		&pc, 1,
		REND_POLYGON_MODE_FILL,
		REND_CULL_MODE_NONE,
		REND_TOPOLOGY_TRIANGLE_LIST,
		REND_FORMAT_UNDEFINED,
		false);
	if (!dt->pipeline)
		return 0;
	rend_pipeline_set_blend(dt->pipeline, true);
	dt->vbo = rend_buffer_create(renderer, cap * sizeof (DashVert), REND_BUFFER_VERTEX, false);
	return 1;
}

static void
dash_text_begin(DashText *dt, float atlas_w, float atlas_h)
{
	dt->count = 0;
	dt->atlas_w = atlas_w;
	dt->atlas_h = atlas_h;
}

static void
dash_emit(DashText *dt, float x, float y, float w, float h, float u0, float v0, float u1, float v1, uint32_t color)
{
	DashVert *v;
	float r, g, b, a;
	size_t i;

	if (dt->count + 6 > dt->cap)
		return;
	dash_color(color, &r, &g, &b, &a);
	v = dt->verts + dt->count;
	v[0].x = x;     v[0].y = y;     v[0].u = u0; v[0].v = v0;
	v[1].x = x + w; v[1].y = y;     v[1].u = u1; v[1].v = v0;
	v[2].x = x;     v[2].y = y + h; v[2].u = u0; v[2].v = v1;
	v[3].x = x;     v[3].y = y + h; v[3].u = u0; v[3].v = v1;
	v[4].x = x + w; v[4].y = y;     v[4].u = u1; v[4].v = v0;
	v[5].x = x + w; v[5].y = y + h; v[5].u = u1; v[5].v = v1;
	for (i = 0; i < 6; i++) {
		v[i].r = r;
		v[i].g = g;
		v[i].b = b;
		v[i].a = a;
	}
	dt->count += 6;
}

static void
dash_glyph(DashText *dt, float x, float y, const TypeGlyph *g, uint32_t color)
{
	float gx, gy, u0, v0, u1, v1;

	if (!g || g->width == 0 || g->height == 0 || dt->atlas_w <= 0.0f || dt->atlas_h <= 0.0f)
		return;
	gx = floorf(x + g->bearing_x + 0.5f);
	gy = floorf(y - g->bearing_y + 0.5f);
	u0 = (float)g->atlas_x / dt->atlas_w;
	v0 = (float)g->atlas_y / dt->atlas_h;
	u1 = (float)(g->atlas_x + g->width) / dt->atlas_w;
	v1 = (float)(g->atlas_y + g->height) / dt->atlas_h;
	dash_emit(dt, gx, gy, (float)g->width, (float)g->height, u0, v0, u1, v1, color);
}

static float
dash_styled(DashText *dt, TypeCtx *type, float x, float y, float size, const char *s, TypeStyle style, uint32_t color)
{
	const char *p;
	float pen;
	size_t left;

	if (!s || !s[0])
		return 0.0f;
	pen = floorf(x + 0.5f);
	y = floorf(y + 0.5f);
	p = s;
	left = strlen(s);
	while (left) {
		TypeGlyph g;
		uint32_t cp;
		uint32_t n;
		TypeError err;

		cp = 0;
		n = type_utf8_next(p, left, &cp);
		if (n == 0)
			break;
		err = type_glyph_styled(type, cp, size, style, &g);
		if (err == TYPE_ERR_ATLAS_FULL) {
			type_clear_atlas(type);
			atlas_resets++;
			err = type_glyph_styled(type, cp, size, style, &g);
		}
		if (err != TYPE_OK)
			break;
		dash_glyph(dt, pen, y, &g, color);
		pen += g.advance;
		p += n;
		left -= n;
	}
	return pen - x;
}

static float
dash_string(DashText *dt, TypeCtx *type, float x, float y, float size, const char *s, uint32_t color)
{
	TypeStyle style;

	memset(&style, 0, sizeof style);
	return dash_styled(dt, type, x, y, size, s, style, color);
}

static float
dash_measure(TypeCtx *type, float size, const char *s)
{
	TypeCell cells[96];
	uint32_t n;

	if (!s || type_layout_utf8(type, s, strlen(s), size, cells, 96, &n, NULL) != TYPE_OK || !n)
		return 0.0f;
	return cells[n - 1].x + cells[n - 1].glyph.advance;
}

static void
dash_string_box(DashText *dt, TypeCtx *type, float x, float y, float w, float h, float size, const char *s, uint32_t color, int center)
{
	TypeMetrics metrics;
	float tw, pen, baseline;

	if (!s || !s[0])
		return;
	if (type_metrics(type, size, &metrics) != TYPE_OK)
		return;
	tw = dash_measure(type, size, s);
	if (center)
		pen = x + (w - tw) * 0.5f;
	else
		pen = x;
	baseline = y + (h - (metrics.ascender - metrics.descender)) * 0.5f + metrics.ascender;
	dash_string(dt, type, pen, baseline, size, s, color);
}

static void
dash_text_flush(DashText *dt, RendRenderer renderer, float screen_w, float screen_h)
{
	DashPC pc;

	if (!dt || !renderer || !dt->pipeline || dt->count == 0)
		return;
	pc.screen_w = screen_w;
	pc.screen_h = screen_h;
	pc.atlas_w = dt->atlas_w;
	pc.atlas_h = dt->atlas_h;
	rend_buffer_write(renderer, &dt->vbo, dt->verts, dt->count * sizeof (DashVert), 0);
	rend_cmd_bind_pipeline(dt->pipeline);
	rend_cmd_push_constants(dt->pipeline, &pc, sizeof pc);
	rend_cmd_bind_vertex_buffer(dt->pipeline, 0, dt->vbo, 0);
	rend_cmd_draw(dt->pipeline, dt->count, 1);
}

static int
dash_add_face(TypeCtx *type, uint8_t **keep, int *nkeep, const char *path, int bold, int italic, uint32_t *id)
{
	unsigned long n;
	uint8_t *bytes;
	TypeFaceParams face;
	TypeError err;

	if (!path)
		return 0;
	bytes = peak_file_alloc(path, &n);
	if (!bytes)
		return 0;
	type_face_params_default(&face);
	face.style.bold = bold ? 1 : 0;
	face.style.italic = italic ? 1 : 0;
	if (bold)
		face.weight = TYPE_WEIGHT_BOLD;
	else
		face.weight = TYPE_WEIGHT_REGULAR;
	err = type_add_font(type, bytes, (size_t)n, &face, id);
	if (err != TYPE_OK) {
		free(bytes);
		return 0;
	}
	if (*nkeep < MAX_KEEP)
		keep[(*nkeep)++] = bytes;
	return 1;
}

static int
dash_add_first(TypeCtx *type, uint8_t **keep, int *nkeep, const char **paths, int npaths, int bold, int italic, uint32_t *id)
{
	int i;

	for (i = 0; i < npaths; i++) {
		if (dash_add_face(type, keep, nkeep, paths[i], bold, italic, id))
			return 1;
	}
	return 0;
}

static void
dash_load_faces(TypeCtx *type, uint8_t **keep, int *nkeep, DashFonts *fonts)
{
	static const char *regular[] = {
		"/usr/share/fonts/noto/NotoSans-Regular.ttf",
		"/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
		"/usr/share/fonts/TTF/DejaVuSans.ttf",
		"/usr/share/fonts/Adwaita/AdwaitaSans-Regular.ttf"
	};
	static const char *bold[] = {
		"/usr/share/fonts/noto/NotoSans-Bold.ttf",
		"/usr/share/fonts/liberation/LiberationSans-Bold.ttf",
		"/usr/share/fonts/TTF/DejaVuSans-Bold.ttf"
	};
	static const char *italic[] = {
		"/usr/share/fonts/noto/NotoSans-Italic.ttf",
		"/usr/share/fonts/liberation/LiberationSans-Italic.ttf",
		"/usr/share/fonts/TTF/DejaVuSans-Oblique.ttf"
	};
	static const char *bi[] = {
		"/usr/share/fonts/noto/NotoSans-BoldItalic.ttf",
		"/usr/share/fonts/liberation/LiberationSans-BoldItalic.ttf",
		"/usr/share/fonts/TTF/DejaVuSans-BoldOblique.ttf"
	};
	uint32_t id;
	uint32_t fb[4];
	uint32_t nfb;
	uint8_t *bytes;
	size_t n;
	TypeError err;

	memset(fonts, 0, sizeof *fonts);
	fonts->regular = TYPE_NONE;
	fonts->blue = TYPE_NONE;
	fonts->cjk = TYPE_NONE;
	fonts->have_regular = dash_add_first(type, keep, nkeep, regular, 4, 0, 0, &fonts->regular);
	fonts->have_bold = dash_add_first(type, keep, nkeep, bold, 3, 1, 0, &id);
	fonts->have_italic = dash_add_first(type, keep, nkeep, italic, 3, 0, 1, &id);
	fonts->have_bi = dash_add_first(type, keep, nkeep, bi, 3, 1, 1, &id);

	bytes = dash_file("Blue Screen Personal Use.ttf", "demos/dashboard/Blue Screen Personal Use.ttf", &n);
	if (!bytes) {
		uint8_t *zip;
		unsigned long zn;

		zip = peak_file_alloc("demos/blue_screen.zip", &zn);
		if (!zip)
			zip = peak_file_alloc("blue_screen.zip", &zn);
		if (zip) {
			bytes = zip_ttf(zip, zn, &n);
			free(zip);
		}
	}
	if (bytes) {
		err = type_add_font(type, bytes, n, NULL, &fonts->blue);
		if (err == TYPE_OK) {
			fonts->have_blue = 1;
			if (*nkeep < MAX_KEEP)
				keep[(*nkeep)++] = bytes;
		} else {
			free(bytes);
		}
	}

	err = TYPE_ERR_INVALID_FONT;
	if (dash_add_face(type, keep, nkeep, "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc", 0, 0, &fonts->cjk)) {
		fonts->have_cjk = 1;
		err = TYPE_OK;
	}
	nfb = 0;
	if (fonts->have_blue)
		fb[nfb++] = fonts->blue;
	if (fonts->have_cjk)
		fb[nfb++] = fonts->cjk;
	if (nfb)
		type_set_fallbacks(type, fb, nfb);
	fonts->nfaces = (int)(fonts->have_regular + fonts->have_bold + fonts->have_italic + fonts->have_bi + fonts->have_blue + fonts->have_cjk);
	snprintf(fonts->note, sizeof fonts->note,
		"faces %d  bold %d  italic %d  bi %d  blue %d  cjk %s",
		fonts->nfaces, fonts->have_bold, fonts->have_italic, fonts->have_bi, fonts->have_blue,
		fonts->have_cjk ? "yes" : dash_err(err));
}

static void
dash_use(TypeCtx *type, DashFonts *fonts, int blue)
{
	if (blue && fonts->have_blue)
		type_select(type, fonts->blue);
	else if (fonts->have_regular)
		type_select(type, fonts->regular);
}

static void
dash_probe(TypeCtx *type, DashFonts *fonts)
{
	TypeCell cells[8];
	TypeGlyph g;
	TypeError e, huge;
	uint32_t written, aw, ah, x, y;
	size_t used, gray, cover;
	const uint8_t *px;
	int peek_cold;
	const char *bad = "ok\xff!";

	peek_cold = type_peek_glyph(type, 0x4E00, 16.0f, &g) == TYPE_OK;
	e = type_layout_utf8(type, bad, 4, TEXT_UI, cells, 8, &written, &used);
	huge = type_glyph(type, 'M', 480.0f, &g);
	gray = 0;
	cover = 0;
	if (type_glyph(type, 'A', TEXT_UI, &g) == TYPE_OK) {
		px = type_ctx_atlas(type, &aw, &ah);
		cover = (size_t)g.width * (size_t)g.height;
		for (y = 0; y < g.height; y++) {
			for (x = 0; x < g.width; x++) {
				uint8_t c;

				c = px[((size_t)g.atlas_y + y) * aw + g.atlas_x + x];
				if (c && c != 255)
					gray++;
			}
		}
	}
	snprintf(fonts->probe, sizeof fonts->probe,
		"utf8 %s at %u  peek CJK %s  width %d cjk %d emoji %d  A gray %u/%u  huge %s",
		dash_err(e), (unsigned)used,
		peek_cold ? "hit" : "cold",
		type_width(0x4E00), type_is_cjk(0x4E00), type_is_emoji(0x1F600),
		(unsigned)gray, (unsigned)cover, dash_err(huge));
}

static void
dash_churn(TypeCtx *type, int count, float size, int *resets)
{
	int i;

	for (i = 0; i < count; i++) {
		TypeGlyph g;
		TypeError err;
		uint32_t cp;

		cp = 0x00A1u + (churn_cp % 5000u);
		churn_cp++;
		err = type_glyph(type, cp, size, &g);
		if (err == TYPE_ERR_ATLAS_FULL) {
			type_clear_atlas(type);
			atlas_resets++;
			if (resets)
				(*resets)++;
			break;
		}
	}
}

static float
dash_line(DashText *dt, TypeCtx *type, float x, float y, float size, const char *s, uint32_t color)
{
	TypeMetrics m;

	if (type_metrics(type, size, &m) != TYPE_OK)
		return y + size;
	dash_string(dt, type, x, y + m.ascender, size, s, color);
	return y + m.ascender - m.descender + m.line_gap + 6.0f;
}

static void
dash_stats_line(DashText *dt, TypeCtx *type, float x, float y, float W)
{
	TypeGlyphStats st;
	char buf[160];
	uint32_t aw, ah;
	const uint8_t *px;
	uint32_t n, i, fill;

	type_stats(type, &st);
	px = type_ctx_atlas(type, &aw, &ah);
	n = aw * ah;
	fill = 0;
	if (px && n) {
		for (i = 0; i < n; i += 32)
			if (px[i])
				fill++;
	}
	snprintf(buf, sizeof buf, "hits %llu  misses %llu  raster %.2f ms  atlas %.2f ms  fill %u  resets %d",
		(unsigned long long)st.hits, (unsigned long long)st.misses,
		(double)st.raster_ns / 1.0e6, (double)st.atlas_ns / 1.0e6,
		fill, atlas_resets);
	(void)W;
	dash_string(dt, type, x, y, TEXT_SMALL, buf, COL_MUTED);
}

static void
dash_pages(DashText *dt, TypeCtx *type, DashFonts *fonts, int page, float W, float H, float churn, int *resets)
{
	static const char *nav_name[] = { "Specimen", "Styles", "Scripts", "Cache" };
	static const float sizes[] = { 14.0f, 22.0f, 32.0f, 48.0f, 72.0f };
	static const char *scripts[] = {
		"Latin  na\xc3\xafve caf\xc3\xa9",
		"Cyrillic  \xd0\x9f\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82",
		"Greek  \xce\xb3\xce\xb5\xce\xb9\xce\xac",
		"CJK  \xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e",
		"Hangul  \xed\x95\x9c\xea\xb8\x80",
		"Hiragana  \xe3\x81\x82\xe3\x81\x84\xe3\x81\x86",
		"Symbols  \xe2\x86\x90 \xe2\x98\x85 \xe2\x99\xaa",
		"Replacement  \xef\xbf\xbd"
	};
	char buf[128];
	float x, y, line;
	int i;
	TypeStyle style;

	x = 216.0f;
	dash_use(type, fonts, 0);
	for (i = 0; i < PAGE_COUNT; i++) {
		dash_string_box(dt, type, 12, 64.0f + (float)i * 44.0f, 176, 36, TEXT_UI,
			nav_name[i], page == i ? 0xFFFFFFFFu : COL_TEXT, 1);
	}
	dash_string(dt, type, 16, 28, TEXT_UI, "Type", COL_TEXT);
	dash_string(dt, type, 220, 28, TEXT_UI, nav_name[page], COL_TEXT);
	dash_string(dt, type, x, 64, TEXT_SMALL, fonts->note, COL_MUTED);
	dash_stats_line(dt, type, x, 86, W);

	y = 112.0f;
	if (page == PAGE_SPECIMEN) {
		dash_string(dt, type, x, y, TEXT_SMALL, fonts->probe, COL_MUTED);
		y += 28.0f;
		for (i = 0; i < 5; i++) {
			TypeMetrics m;
			float sz;

			sz = sizes[i];
			if (y + sz > H - 8.0f)
				break;
			dash_use(type, fonts, 0);
			if (type_metrics(type, sz, &m) != TYPE_OK)
				continue;
			snprintf(buf, sizeof buf, "%g", sz);
			dash_string(dt, type, x, y + m.ascender, TEXT_SMALL, buf, COL_MUTED);
			dash_string(dt, type, x + 48.0f, y + m.ascender, sz, "The quick brown fox", COL_TEXT);
			line = y + m.ascender - m.descender + 4.0f;
			if (fonts->have_blue && line + sz < H - 4.0f) {
				dash_use(type, fonts, 1);
				if (type_metrics(type, sz, &m) == TYPE_OK) {
					dash_string(dt, type, x + 48.0f, line + m.ascender, sz, "Blue Screen", COL_ACCENT_H);
					line = line + m.ascender - m.descender + 8.0f;
				}
			} else {
				line += 8.0f;
			}
			y = line;
		}
		dash_use(type, fonts, 0);
	}

	if (page == PAGE_STYLES) {
		memset(&style, 0, sizeof style);
		y = dash_line(dt, type, x, y, 36.0f, "Regular  Type renders coverage", COL_TEXT);
		style.bold = 1;
		dash_styled(dt, type, x, y + 28.0f, 36.0f, "Bold  weight 700 face", style, COL_TEXT);
		y += 52.0f;
		memset(&style, 0, sizeof style);
		style.italic = 1;
		dash_styled(dt, type, x, y + 28.0f, 36.0f, "Italic  oblique face", style, COL_TEXT);
		y += 52.0f;
		style.bold = 1;
		style.italic = 1;
		dash_styled(dt, type, x, y + 28.0f, 36.0f, "Bold italic face", style, COL_TEXT);
		y += 64.0f;
		memset(&style, 0, sizeof style);
		style.bold = fonts->have_bold ? 1 : 0;
		dash_styled(dt, type, x, y + 18.0f, 18.0f, "18 px styled", style, COL_MUTED);
		dash_styled(dt, type, x + 180.0f, y + 22.0f, 28.0f, "28 px", style, COL_TEXT);
		dash_styled(dt, type, x + 280.0f, y + 28.0f, 42.0f, "42 px", style, COL_TEXT);
		if (!fonts->have_bold)
			dash_string(dt, type, x, y + 70.0f, TEXT_SMALL, "no bold face loaded; style bit still caches a key", COL_WARN);
	}

	if (page == PAGE_SCRIPTS) {
		for (i = 0; i < 8; i++) {
			if (y > H - 36.0f)
				break;
			y = dash_line(dt, type, x, y, 26.0f, scripts[i], COL_TEXT);
		}
		y += 8.0f;
		dash_string(dt, type, x, y, TEXT_SMALL, fonts->probe, COL_MUTED);
		y += 24.0f;
		snprintf(buf, sizeof buf, "cell %d  combining %d  validate %d  layout cap",
			type_cell_width(0x4E00), type_is_combining(0x0301),
			type_utf8_validate("\xE6\x97\xA5", 3));
		dash_string(dt, type, x, y, TEXT_SMALL, buf, COL_MUTED);
	}

	if (page == PAGE_CACHE) {
		uint32_t aw, ah;
		float vw, vh;
		TypeGlyph peek;
		int hot;

		type_ctx_atlas(type, &aw, &ah);
		vw = 360.0f;
		vh = 180.0f;
		if (vw > W - x - 16.0f)
			vw = W - x - 16.0f;
		dash_emit(dt, x, y, vw, vh, 0.0f, 0.0f, vw / (float)aw, vh / (float)ah, 0xFFFFFFFFu);
		y += vh + 16.0f;
		hot = type_peek_glyph(type, 'A', TEXT_UI, &peek) == TYPE_OK;
		snprintf(buf, sizeof buf, "peek ASCII A %s  churn %g  codepoint U+%04X",
			hot ? "hot" : "cold", churn, churn_cp);
		dash_string(dt, type, x, y, TEXT_SMALL, buf, COL_MUTED);
		y += 28.0f;
		dash_string(dt, type, x, y, 20.0f, "Atlas is shelf-packed. LRU eviction does not free texels.", COL_TEXT);
		y += 32.0f;
		dash_string(dt, type, x, y, 20.0f, "Clear resets both atlases and rewarms ASCII.", COL_TEXT);
		dash_churn(type, (int)(churn * 48.0f), 18.0f, resets);
	}
}

static uint8_t *
load_spv(const char *a, const char *b, unsigned long *size)
{
	uint8_t *p;

	p = peak_file_alloc(a, size);
	if (p)
		return p;
	return peak_file_alloc(b, size);
}

int
main(int argc, char **argv)
{
	PeakWindow win;
	RendBindingInfo bind_info;
	RendRenderer renderer;
	FuseRend fr;
	FuseCanvas canvas;
	static unsigned char ui_buf[1 << 18];
	static unsigned char vert_buf[MAX_VERTS * sizeof (FuseRendVertex)];
	static DashVert text_verts[DASH_TEXT_VERTS];
	unsigned long vert_spv_n, frag_spv_n, tvert_n, tfrag_n;
	uint8_t *vert_spv, *frag_spv, *tvert, *tfrag;
	uint8_t *keep[MAX_KEEP];
	unsigned char *type_buf;
	size_t type_n;
	TypeParams type_params;
	TypeCtx *type;
	DashText text;
	DashFonts fonts;
	RendTexture atlas_tex;
	uint32_t atlas_w, atlas_h;
	const uint8_t *atlas_px;
	int running;
	int page;
	int pointer_state;
	int headless;
	int frames;
	int frame_i;
	int nkeep;
	int resets;
	int probed;
	float mx, my;
	float churn;
	const char *ppm;
	uint32_t width;
	uint32_t height;
	int i;

	width = 1280;
	height = 720;
	memset(&win, 0, sizeof win);
	headless = headless_parse(argc, argv, &frames, &ppm);
	nkeep = 0;
	memset(keep, 0, sizeof keep);
	probed = 0;
	churn = 0.35f;
	page = PAGE_SPECIMEN;

	if (!headless) {
		if (!(peak_demo_ctx = peak_init_legacy())) {
			PFATAL("Failed to init Peak!");
			return 1;
		}
		win = peak_window_open(peak_demo_ctx, "Type Dashboard", width, height, 0);
		if (!win.running) {
			PFATAL("Failed to open a window!");
			return 1;
		}
		width = win.width;
		height = win.height;
	}

	memset(&bind_info, 0, sizeof bind_info);
	bind_info.texture_bindings[0] = 0;
	bind_info.texture_array_sizes[0] = 1;
	bind_info.texture_binding_count = 1;
	if (headless)
		renderer = rend_renderer_create_offscreen(width, height, REND_FORMAT_R8G8B8A8_UNORM, REND_BACKEND_AUTO, &bind_info);
	else
		renderer = rend_renderer_create(&win, REND_BACKEND_AUTO, NULL, true, &bind_info);
	if (!renderer) {
		PFATAL("Failed to create renderer!");
		if (!headless) {
			peak_window_close(&win);
			peak_quit(peak_demo_ctx);
		}
		return 1;
	}

	vert_spv = load_spv("fuse_ui.vert.spv", "demos/dashboard/fuse_ui.vert.spv", &vert_spv_n);
	frag_spv = load_spv("fuse_ui.frag.spv", "demos/dashboard/fuse_ui.frag.spv", &frag_spv_n);
	tvert = load_spv("fuse_text.vert.spv", "demos/dashboard/fuse_text.vert.spv", &tvert_n);
	tfrag = load_spv("fuse_text.frag.spv", "demos/dashboard/fuse_text.frag.spv", &tfrag_n);
	if (!vert_spv || !frag_spv || !tvert || !tfrag) {
		PFATAL("Failed to load dashboard shaders!");
		free(vert_spv);
		free(frag_spv);
		free(tvert);
		free(tfrag);
		return 1;
	}
	if (!fuse_rend_init(&fr, vert_buf, sizeof vert_buf, renderer, vert_spv, vert_spv_n, frag_spv, frag_spv_n)) {
		PFATAL("Failed to init Fuse Rend backend!");
		return 1;
	}
	free(vert_spv);
	free(frag_spv);
	if (!dash_text_init(&text, text_verts, DASH_TEXT_VERTS, renderer, tvert, tvert_n, tfrag, tfrag_n)) {
		PFATAL("Failed to init text pipeline!");
		free(tvert);
		free(tfrag);
		return 1;
	}
	free(tvert);
	free(tfrag);

	if (fuse_canvas_memory(MAX_UI) > sizeof ui_buf) {
		PFATAL("UI buffer too small!");
		return 1;
	}
	canvas = fuse_canvas_create(ui_buf, sizeof ui_buf);
	if (!canvas) {
		PFATAL("Failed to create Fuse canvas!");
		return 1;
	}

	pointer_state = FUSE_POINTER_RELEASED;
	mx = 0.0f;
	my = 0.0f;
	running = 1;
	frame_i = 0;
	type_buf = NULL;
	memset(&atlas_tex, 0, sizeof atlas_tex);
	memset(&fonts, 0, sizeof fonts);

	type_params_default(&type_params);
	type_params.atlas_width = 2048;
	type_params.atlas_height = 2048;
	type_params.cache_capacity = 1024;
	type_params.max_faces = 8;
	type_params.max_fallbacks = 4;
	type_params.color_emoji = 1;
	type_params.ligatures = 1;
	type_params.antialias = TYPE_AA_BOX;
	type_n = type_memory(&type_params);
	type_buf = malloc(type_n);
	type = type_place(type_buf, type_n, &type_params);
	if (!type) {
		PFATAL("Failed to place Type!");
		return 1;
	}
	dash_load_faces(type, keep, &nkeep, &fonts);
	if (!fonts.nfaces) {
		PFATAL("Failed to load a font for Type!");
		free(type_buf);
		return 1;
	}
	if (type_warm_ascii(type, TEXT_UI) != TYPE_OK) {
		PFATAL("Failed to rasterize ASCII!");
		free(type_buf);
		return 1;
	}
	atlas_px = type_ctx_atlas(type, &atlas_w, &atlas_h);
	atlas_tex = rend_texture_create(renderer, atlas_w, atlas_h, 1, 0, 1, REND_FORMAT_R8_UNORM);
	rend_pipeline_bind_texture(text.pipeline, &atlas_tex, 0, 0);

	while (running) {
		PeakEvent ev;
		float W, H;
		size_t ncmds;
		FuseCmd *cmds;
		static FuseClass sidebar;
		static FuseClass nav;

		if (headless && frame_i >= frames)
			break;

		while (!headless && peak_window_epoll(&win, &ev)) {
			if (ev.type == PEAK_EVENT_WINDOW_CLOSE)
				running = 0;
			if (ev.type == PEAK_EVENT_KEY_DOWN && ev.key.key == PEAK_KEY_ESCAPE)
				running = 0;
			if (ev.type == PEAK_EVENT_WINDOW_RESIZE) {
				width = ev.resize.width;
				height = ev.resize.height;
				win.width = width;
				win.height = height;
			}
			if (ev.type == PEAK_EVENT_POINTER) {
				mx = ev.pointer.x;
				my = ev.pointer.y;
				if (ev.pointer.type == PEAK_POINTER_LEFT) {
					if (ev.pointer.state == PEAK_POINTER_PRESSED)
						pointer_state = FUSE_POINTER_PRESSED;
					else if (ev.pointer.state == PEAK_POINTER_RELEASED)
						pointer_state = FUSE_POINTER_RELEASED;
				}
			}
			if (ev.type == PEAK_EVENT_KEY_DOWN) {
				if (ev.key.key == PEAK_KEY_1) page = PAGE_SPECIMEN;
				if (ev.key.key == PEAK_KEY_2) page = PAGE_STYLES;
				if (ev.key.key == PEAK_KEY_3) page = PAGE_SCRIPTS;
				if (ev.key.key == PEAK_KEY_4) page = PAGE_CACHE;
			}
		}

		W = (float)width;
		H = (float)height;
		if (W < 640.0f)
			W = 640.0f;
		if (H < 400.0f)
			H = 400.0f;

		memset(&sidebar, 0, sizeof sidebar);
		sidebar.color = COL_SIDE;
		memset(&nav, 0, sizeof nav);
		nav.direction = FUSE_DIRECTION_COLUMN;
		nav.gap = 8.0f;
		nav.pad_l = 12.0f;
		nav.pad_r = 12.0f;
		nav.pad_t = 64.0f;
		nav.pad_b = 12.0f;
		nav.width_sizing = FUSE_SIZING_FIT;
		nav.height_sizing = FUSE_SIZING_FIT;

		fuse_canvas_resize(canvas, W, H);
		fuse_canvas_pointer(canvas, pointer_state, mx, my);
		fuse_canvas_clear(canvas);

		fuse_div_begin(canvas, 0, 0, W, H, NULL); {
			fuse_div_begin(canvas, 0, 0, 200, H, &sidebar); {
				fuse_div_begin(canvas, 0, 0, 200, H, &nav); {
					for (i = 0; i < PAGE_COUNT; i++) {
						uint32_t idle, hover;
						idle = (page == i) ? COL_ACCENT : COL_BTN;
						hover = (page == i) ? COL_ACCENT_H : COL_BTN_H;
						fuse_idi(canvas, "nav", i);
						if (fuse_button(canvas, 12, 0, 176, 36, idle, hover))
							page = i;
					}
				} fuse_div_end(canvas);
			} fuse_div_end(canvas);

			fuse_div_begin(canvas, 200, 0, W - 200, H, NULL); {
				if (page == PAGE_CACHE) {
					fuse_id(canvas, "churn");
					fuse_slider(canvas, 16, H - 248, W - 232, 18, COL_TRACK, COL_ACCENT, &churn);
					fuse_id(canvas, "clear");
					if (fuse_button(canvas, 16, H - 210, 160, 36, COL_BAD, COL_BTN_H)) {
						type_clear_atlas(type);
						atlas_resets++;
					}
				}
			} fuse_div_end(canvas);
		} fuse_div_end(canvas);

		cmds = fuse_canvas_draw(canvas, &ncmds);
		if (!probed) {
			dash_probe(type, &fonts);
			probed = 1;
		}
		resets = 0;
		if (headless)
			dash_churn(type, 64, 16.0f, &resets);

		dash_text_begin(&text, (float)atlas_w, (float)atlas_h);
		dash_pages(&text, type, &fonts, page, W, H, churn, &resets);
		atlas_px = type_ctx_atlas(type, &atlas_w, &atlas_h);
		rend_texture_copy_data(renderer, &atlas_tex, atlas_px, (size_t)atlas_w * (size_t)atlas_h);

		if (rend_renderer_frame_begin(renderer)) {
			rend_cmd_render_begin(renderer, 0.055f, 0.067f, 0.086f, 1.0f);
			fuse_rend_begin(&fr, W, H);
			if (cmds)
				fuse_rend_cmds(&fr, cmds, ncmds);
			fuse_rend_quad(&fr, 200, 0, W - 200, 52, COL_TOP);
			if (page == PAGE_CACHE) {
				fuse_rend_quad(&fr, 216, 112, 360, 180, COL_CARD);
				fuse_rend_quad(&fr, 216, H - 210, 160, 36, COL_BAD);
			}
			fuse_rend_flush(&fr, renderer);
			dash_text_flush(&text, renderer, W, H);
			rend_cmd_render_end(renderer);
			rend_renderer_frame_end(renderer, NULL);
		}
		frame_i++;
	}

	if (headless && !headless_finish(renderer, width, height, REND_FORMAT_R8G8B8A8_UNORM, ppm)) {
		for (i = 0; i < nkeep; i++)
			free(keep[i]);
		free(type_buf);
		fuse_rend_shutdown(&fr);
		rend_renderer_destroy(renderer);
		rend_quit();
		return 1;
	}

	for (i = 0; i < nkeep; i++)
		free(keep[i]);
	free(type_buf);
	fuse_rend_shutdown(&fr);
	rend_buffer_destroy(&text.vbo);
	rend_texture_destroy(renderer, &atlas_tex);
	rend_renderer_destroy(renderer);
	if (!headless) {
		peak_window_close(&win);
		peak_quit(peak_demo_ctx);
	}
	rend_quit();
	return 0;
}
