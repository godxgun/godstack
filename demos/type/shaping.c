/* Experimental system HarfBuzz/FriBidi integration. Not linked into Sigma.
 * Bounded fixture strings only; NOT general script/fallback/line itemization.
 * Dependency objects use their own allocators and remain unqualified for §5. */
#define _POSIX_C_SOURCE 200809L
#define TYPE_IMPLEMENTATION
#include "../../Type.h"
#include "fixture_support.h"
#include <hb.h>
#include <hb-ot.h>
#include <fribidi.h>
#include "harfbuzz_bridge.h"

#define CAP 64u
static unsigned char permanent[2u << 20], temporary[(2u << 20) + 15];
static unsigned char bytes[3][2u << 20];

typedef struct FixtureFace {
	hb_blob_t *blob;
	hb_face_t *face;
	hb_font_t *font;
	uint32_t id;
} FixtureFace;

static FixtureFace load_face(TypeCtx *ctx, const char *path, unsigned int index)
{
	FixtureFace f;
	size_t len = fixture_font(path, bytes[index], sizeof bytes[index]);
	assert(type_add_font_trusted(ctx, bytes[index], len, NULL, &f.id) == TYPE_OK);
	f.blob = hb_blob_create((const char *)bytes[index], (unsigned int)len, HB_MEMORY_MODE_READONLY,
	                        NULL, NULL);
	f.face = hb_face_create(f.blob, 0);
	f.font = hb_font_create(f.face);
	assert(hb_face_get_glyph_count(f.face) > 0);
	hb_ot_font_set_funcs(f.font);
	/* Type uses EM mapping and rounds size to integer pixels. */
	hb_font_set_scale(f.font, 32 * 64, 32 * 64);
	return f;
}

static void check_layout(TypeCtx *ctx, TypeShapedGlyph *run, uint32_t n)
{
	TypePositionedGlyph out[CAP];
	TypeTextMetrics measured, drawn;
	uint32_t done, i;
	float x = 0, y = 0;
	uint64_t before = type_ctx_atlas_revision(ctx);
	assert(type_measure_shaped(ctx, run, n, 32, &done, &measured) == TYPE_OK && done == n);
	assert(type_ctx_atlas_revision(ctx) == before);
	assert(type_layout_shaped(ctx, run, n, 32, out, CAP, &done, &drawn) == TYPE_OK && done == n);
	for (i = 0; i < n; i++) {
		assert(out[i].glyph.font_id == run[i].font_id);
		assert(out[i].glyph.glyph_id == run[i].glyph_id);
		assert(out[i].cluster == run[i].cluster);
		assert(fabsf(out[i].x - (x + run[i].x_offset)) < 0.0001f);
		assert(fabsf(out[i].y - (y + run[i].y_offset)) < 0.0001f);
		x += run[i].x_advance;
		y += run[i].y_advance;
	}
	assert(drawn.advance_x == x && drawn.advance_y == y);
	assert(measured.advance_x == drawn.advance_x && measured.advance_y == drawn.advance_y);
	assert(measured.has_ink == drawn.has_ink);
	assert(measured.min_x == drawn.min_x && measured.min_y == drawn.min_y);
	assert(measured.max_x == drawn.max_x && measured.max_y == drawn.max_y);
}

/* Resolve embedding levels on logical Unicode, reorder the RUNS, then shape
 * each original logical slice. Never shape FriBidi's visually reversed text.
 * This fixture has only Latin level-0 and Hebrew level-1 runs. Production must
 * additionally itemize script, language, font fallback, controls and lines. */
static uint32_t mixed_fixture(FixtureFace *latin, FixtureFace *hebrew, hb_buffer_t *buffer,
                              TypeShapedGlyph *out)
{
	static const char text[] = "abc \xD7\x90\xD7\x91\xD7\x92 xyz";
	FriBidiChar logical[CAP];
	FriBidiCharType types[CAP];
	FriBidiBracketType brackets[CAP];
	FriBidiLevel levels[CAP];
	FriBidiStrIndex map[CAP];
	FriBidiParType base = FRIBIDI_PAR_ON;
	size_t offsets[CAP + 1], at = 0;
	uint32_t n = 0, nr = 0, i, total = 0;
	uint32_t run_start[CAP], run_end[CAP], run_for_char[CAP];
	unsigned char emitted[CAP] = {0};
	while (at < sizeof text - 1) {
		uint32_t cp = 0, step;
		assert(n < CAP);
		offsets[n] = at;
		step = type_utf8_next(text + at, sizeof text - 1 - at, &cp);
		assert(step);
		logical[n] = cp;
		map[n] = (FriBidiStrIndex)n;
		n++;
		at += step;
	}
	offsets[n] = at;
	fribidi_get_bidi_types(logical, (FriBidiStrIndex)n, types);
	fribidi_get_bracket_types(logical, (FriBidiStrIndex)n, types, brackets);
	assert(fribidi_get_par_embedding_levels_ex(types, brackets, (FriBidiStrIndex)n, &base, levels));
	assert(base == FRIBIDI_PAR_LTR);
	assert(fribidi_reorder_line(0, types, (FriBidiStrIndex)n, 0, base, levels, NULL, map));
	for (i = 0; i < n; i++) {
		if (i == 0 || levels[i] != levels[i - 1]) {
			if (nr)
				run_end[nr - 1] = i;
			run_start[nr++] = i;
		}
		run_for_char[i] = nr - 1;
	}
	run_end[nr - 1] = n;
	assert(nr == 3);
	for (i = 0; i < n; i++) {
		uint32_t r = run_for_char[map[i]], start = run_start[r], count;
		FixtureFace *f;
		if (emitted[r])
			continue;
		emitted[r] = 1;
		assert(levels[start] == 0 || levels[start] == 1);
		f = (levels[start] & 1) ? hebrew : latin;
		assert(type_demo_hb_run(
		           f->font, buffer, f->id, text, sizeof text - 1, offsets[start],
		           offsets[run_end[r]], (levels[start] & 1) ? HB_DIRECTION_RTL : HB_DIRECTION_LTR,
		           (levels[start] & 1) ? HB_SCRIPT_HEBREW : HB_SCRIPT_LATIN,
		           (levels[start] & 1) ? "he" : "en", out + total, CAP - total, &count) == TYPE_OK);
		total += count;
	}
	assert(total == 11);
	assert(out[4].cluster == 8 && out[5].cluster == 6 && out[6].cluster == 4);
	assert(out[4].font_id == hebrew->id && out[0].font_id == latin->id);
	return total;
}

int main(void)
{
	TypeCtx *ctx = fixture_ctx(permanent, sizeof permanent);
	TypeScratch scratch;
	FixtureFace faces[3];
	hb_buffer_t *buffer = hb_buffer_create();
	TypeShapedGlyph run[CAP];
	uint32_t n, i;
	assert(ctx && hb_buffer_pre_allocate(buffer, CAP));
	type_scratch_init(&scratch, temporary, sizeof temporary);
	type_set_scratch(ctx, &scratch);
	faces[0] = load_face(ctx, "assets/NotoSans-Regular.ttf", 0);
	faces[1] = load_face(ctx, "assets/NotoSansArabic-Regular.ttf", 1);
	faces[2] = load_face(ctx, "assets/NotoSansHebrew-Regular.ttf", 2);
	assert(type_demo_hb_run(faces[0].font, buffer, faces[0].id, "ffi", 3, 0, 3, HB_DIRECTION_LTR,
	                        HB_SCRIPT_LATIN, "en", run, CAP, &n) == TYPE_OK);
	assert(n > 0 && n < 3 && run[0].cluster == 0); /* actual GSUB ligature */
	check_layout(ctx, run, n);
	assert(type_demo_hb_run(faces[0].font, buffer, faces[0].id, "x\xCC\x81", 3, 0, 3,
	                        HB_DIRECTION_LTR, HB_SCRIPT_LATIN, "en", run, CAP, &n) == TYPE_OK);
	assert(n == 2 && run[1].x_advance == 0); /* actual combining mark */
	assert(run[1].x_offset != 0 || run[1].y_offset != 0);
	check_layout(ctx, run, n);
	assert(type_demo_hb_run(faces[1].font, buffer, faces[1].id, "\xD9\x84\xD8\xA7", 4, 0, 4,
	                        HB_DIRECTION_RTL, HB_SCRIPT_ARABIC, "ar", run, CAP, &n) == TYPE_OK);
	/* The bundled face retains two glyphs (no ligature claim). Prove joining
	 * context affected substitution by comparing against each isolated scalar. */
	assert(n == 2 && run[0].glyph_id != 0 && run[1].glyph_id != 0);
	{
		TypeShapedGlyph contextual[2], isolated_lam[CAP], isolated_alef[CAP];
		uint32_t nl, na, j;
		int differs;
		memcpy(contextual, run, sizeof contextual);
		assert(type_demo_hb_run(faces[1].font, buffer, faces[1].id, "\xD9\x84\xD8\xA7", 4, 0, 2,
		                        HB_DIRECTION_RTL, HB_SCRIPT_ARABIC, "ar", isolated_lam, CAP,
		                        &nl) == TYPE_OK);
		assert(type_demo_hb_run(faces[1].font, buffer, faces[1].id, "\xD9\x84\xD8\xA7", 4, 2, 4,
		                        HB_DIRECTION_RTL, HB_SCRIPT_ARABIC, "ar", isolated_alef, CAP,
		                        &na) == TYPE_OK);
		assert(nl == 1 && na == 1);
		differs = contextual[0].glyph_id != isolated_alef[0].glyph_id ||
		          contextual[1].glyph_id != isolated_lam[0].glyph_id;
		assert(differs);
		for (j = 0; j < n; j++)
			assert(contextual[j].glyph_id != 0x0644 && contextual[j].glyph_id != 0x0627);
	}
	check_layout(ctx, run, n);
	assert(type_demo_hb_run(faces[0].font, buffer, faces[0].id, "ffi", 3, 0, 3, HB_DIRECTION_LTR,
	                        HB_SCRIPT_LATIN, "en", run, 0, &n) == TYPE_ERR_BUF_TOO_SMALL &&
	       n == 0);
	assert(type_demo_hb_run(faces[0].font, buffer, faces[0].id, "\xC0\xAF", 2, 0, 2,
	                        HB_DIRECTION_LTR, HB_SCRIPT_LATIN, "en", run, CAP,
	                        &n) == TYPE_ERR_INVALID_UTF8);
	n = mixed_fixture(&faces[0], &faces[2], buffer, run);
	check_layout(ctx, run, n);
	assert(scratch.used == 0 && scratch.peak <= scratch.capacity);
	type_set_scratch(ctx, NULL);
	type_clear_fonts(ctx);
	hb_buffer_destroy(buffer);
	for (i = 0; i < 3; i++) {
		hb_font_destroy(faces[i].font);
		hb_face_destroy(faces[i].face);
		hb_blob_destroy(faces[i].blob);
	}
	printf("Type shaping fixture: passed (HarfBuzz %s; FriBidi %s)\n", hb_version_string(),
	       FRIBIDI_VERSION);
	return 0;
}
