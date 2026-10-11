/* C99 CPU-only contracts. Run from the repository root. No GPU/framework.
 * Malformed nested font structures must NEVER reach the trusted parser. */
#define _POSIX_C_SOURCE 200809L
#define TYPE_IMPLEMENTATION
#include "../../Type.h"
#include "fixture_support.h"

static unsigned char permanent_a[2u << 20], permanent_b[2u << 20];
static unsigned char temporary_a[(2u << 20) + 15], temporary_b[(2u << 20) + 15];
static unsigned char font_bytes[2u << 20];
static unsigned char parsed[4096], face_storage[256], pixels[1u << 20];

static void admission(TypeCtx *ctx)
{
	unsigned char bad[64] = {0};
	unsigned char *placed = fixture_align(parsed);
	uint32_t id = 987;
	size_t i;
	/* No read of even the first byte at the default trust boundary. */
	assert(type_add_font(ctx, (const void *)(uintptr_t)1, SIZE_MAX, NULL, &id) ==
	       TYPE_ERR_UNTRUSTED_FONT);
	assert(id == 987);
	assert(!type_ttf_place(placed, sizeof parsed - 15, (const void *)(uintptr_t)1, SIZE_MAX));
	assert(type_ttf_error((TypeTTF *)placed) == TYPE_ERR_UNTRUSTED_FONT);
	for (i = 0; i <= sizeof bad; i++) {
		assert(type_add_font(ctx, bad, i, NULL, NULL) == TYPE_ERR_UNTRUSTED_FONT);
		assert(!type_ttf_place_trusted(placed, sizeof parsed - 15, bad, i));
		assert(type_ttf_error((TypeTTF *)placed) == TYPE_ERR_INVALID_FONT);
	}
	/* Valid signature, truncated directory, then out-of-range table offset. */
	bad[1] = 1;
	bad[5] = 1;
	assert(type_add_font_trusted(ctx, bad, 12, NULL, NULL) == TYPE_ERR_INVALID_FONT);
	memcpy(bad + 12, "head", 4);
	memset(bad + 20, 255, 4);
	bad[27] = 54;
	assert(type_add_font_trusted(ctx, bad, sizeof bad, NULL, NULL) == TYPE_ERR_INVALID_FONT);
	/* Unsupported formats rejected before stb_InitFont. */
	memcpy(bad, "OTTO", 4);
	assert(type_add_font_trusted(ctx, bad, sizeof bad, NULL, NULL) == TYPE_ERR_UNSUPPORTED_TABLE);
	memcpy(bad, "ttcf", 4);
	assert(type_add_font_trusted(ctx, bad, sizeof bad, NULL, NULL) == TYPE_ERR_UNSUPPORTED_TABLE);
	memset(bad, 0, sizeof bad);
	bad[1] = 1;
	bad[5] = 1;
	memcpy(bad + 12, "CFF ", 4);
	bad[23] = 28;
	assert(type_add_font_trusted(ctx, bad, sizeof bad, NULL, NULL) == TYPE_ERR_UNSUPPORTED_TABLE);
	assert(type_otf_memory() == 0 && type_otf_error(NULL) == TYPE_ERR_UNIMPLEMENTED);
}

static const size_t budgets[] = {0, 16, 512, 2048, 8192, 16384, 32768, 65536};
typedef struct TypeFixture {
	TypeCtx *a, *b;

	TypeTTF *ttf;
	TypeFont *font;
	TypeScratch sa, sb;
	TypeGlyph ga, gb;
	TypeBitmap info;
	TypeShapedGlyph run[2];
	TypePositionedGlyph cells[2];
	TypeTextMetrics measured, rendered, prefix;
	TypeParams p;
	TypeError err;
	uint32_t id_a, id_b, written;
	uint16_t gid, space;
	uint64_t revision;
	size_t len, budget, failures;
} TypeFixture;

static void check_setup(TypeFixture *d, const char *path)
{

	assert(d->a && d->b);
	admission(d->a);
	type_params_default(&d->p);
	d->p.atlas_width = UINT32_MAX;
	assert(type_memory(&d->p) == 0);
	type_params_default(&d->p);
	assert(!type_place(permanent_a + 1, sizeof permanent_a - 1, &d->p));
	d->len = fixture_font(path, font_bytes, sizeof font_bytes);
	assert(type_ttf_memory() <= sizeof parsed - 15);
	assert(type_font_memory() <= sizeof face_storage - 15);
	d->ttf = type_ttf_place_trusted(fixture_align(parsed), sizeof parsed - 15, font_bytes, d->len);
	assert(d->ttf && type_ttf_outline(d->ttf));
	d->font =
	    type_font_from_ttf(fixture_align(face_storage), sizeof face_storage - 15, d->ttf, NULL);
	assert(d->font && type_font_glyph_index(d->font, 'A', &d->gid) == TYPE_OK);
	assert(type_add_font_trusted(d->a, font_bytes, d->len, NULL, &d->id_a) == TYPE_OK);
	assert(type_add_font_trusted(d->b, font_bytes, d->len, NULL, &d->id_b) == TYPE_OK);
	assert(type_glyph_id(d->a, d->id_a, d->gid, 32, &d->ga) == TYPE_ERR_SCRATCH_REQUIRED);
	assert(type_glyph_id(d->a, d->id_a, UINT32_MAX, 32, &d->ga) == TYPE_ERR_GLYPH_NOT_FOUND);
	assert(type_glyph_id(d->a, d->id_a, d->gid, INFINITY, &d->ga) == TYPE_ERR_INVALID_FONT);
}

static void check_glyph_cache(TypeFixture *d)
{
	type_scratch_init(&d->sa, temporary_a + 1,
	                  sizeof temporary_a - 1); /* misaligned view is supported */
	type_scratch_init(&d->sb, temporary_b, sizeof temporary_b);
	type_set_scratch(d->a, &d->sa);
	type_set_scratch(d->b, &d->sb);
	d->sa.used = 7;
	d->sb.used = 11; /* outer operation's prefix must survive */
	d->revision = type_ctx_atlas_revision(d->a);
	assert(type_glyph_id(d->a, d->id_a, d->gid, 32, &d->ga) == TYPE_OK);
	assert(d->sa.used == 7 && d->sa.peak > d->sa.used &&
	       type_ctx_atlas_revision(d->a) > d->revision);
	assert(type_glyph_id(d->a, d->id_a, 0, 32, &d->ga) == TYPE_OK); /* .notdef is a valid ID */
	assert(type_glyph_id(d->a, d->id_a, d->gid, 32, &d->ga) == TYPE_OK);
	assert(type_glyph_id(d->b, d->id_b, d->gid, 32, &d->gb) == TYPE_OK);
	assert(d->sb.used == 11 && d->sb.peak > d->sb.used && d->sa.used == 7);
	assert(d->ga.width == d->gb.width && d->ga.height == d->gb.height);
	d->revision = type_ctx_atlas_revision(d->a);
	assert(type_glyph_id(d->a, d->id_a, d->gid, 32, &d->ga) == TYPE_OK);
	assert(type_ctx_atlas_revision(d->a) == d->revision);
}

static void check_cold_glyphs(TypeFixture *d)
{
	/* Force independent cold-glyph failures: atlas must not publish partials. */
	for (d->budget = 0; d->budget < sizeof budgets / sizeof budgets[0]; d->budget++) {
		TypeCtx *cold = fixture_ctx(permanent_b, sizeof permanent_b);
		assert(type_add_font_trusted(cold, font_bytes, d->len, NULL, &d->id_b) == TYPE_OK);
		type_scratch_init(&d->sb, temporary_b, budgets[d->budget]);
		type_set_scratch(cold, &d->sb);
		d->revision = type_ctx_atlas_revision(cold);
		d->err = type_glyph_id(cold, d->id_b, d->gid, 128, &d->gb);
		if (d->err == TYPE_ERR_SCRATCH_FULL) {
			d->failures++;
			assert(d->sb.failed && d->sb.used == 0 && d->sb.peak <= d->sb.capacity);
			assert(type_ctx_atlas_revision(cold) == d->revision);
		} else {
			assert(d->err == TYPE_OK);
			assert(d->sb.used == 0);
		}
	}
	assert(d->failures > 0);
}

static void check_bitmaps(TypeFixture *d)
{
	/* Low-level rendering includes width > 64 (heap scanline in stb v2). */
	assert(type_font_bitmap_info(d->font, d->gid, 256, &d->info) == TYPE_OK);
	assert(d->info.width > 64 && type_bitmap_bytes(&d->info) <= sizeof pixels);
	assert(type_font_render_bitmap(d->font, d->gid, 256, TYPE_AA_BOX, pixels, sizeof pixels,
	                               NULL) == TYPE_ERR_SCRATCH_REQUIRED);
	for (d->budget = 0; d->budget < sizeof budgets / sizeof budgets[0]; d->budget++) {
		type_scratch_init(&d->sb, temporary_b, budgets[d->budget]);
		d->err = type_font_render_bitmap_scratch(d->font, d->gid, 256, TYPE_AA_BOX, pixels,
		                                         sizeof pixels, &d->sb, NULL);
		assert(d->err == TYPE_OK || d->err == TYPE_ERR_SCRATCH_FULL);
		assert(d->sb.used == 0 && d->sb.peak <= d->sb.capacity);
		if (d->err == TYPE_ERR_SCRATCH_FULL) {
			size_t j;
			for (j = 0; j < type_bitmap_bytes(&d->info); j++)
				assert(pixels[j] == 0);
		}
	}
	type_scratch_init(&d->sb, temporary_b, sizeof temporary_b);
	assert(type_font_render_bitmap_scratch(d->font, d->gid, 256, TYPE_AA_BOX, pixels, sizeof pixels,
	                                       &d->sb, &d->info) == TYPE_OK);
}

static void check_shaped_runs(TypeFixture *d)
{
	memset(d->run, 0, sizeof d->run);
	d->run[0].font_id = d->run[1].font_id = d->id_a;
	d->run[0].glyph_id = d->run[1].glyph_id = d->gid;
	d->run[0].cluster = 9;
	d->run[1].cluster = 2; /* visual RTL cluster order retained */
	d->run[0].x_advance = 12.25f;
	d->run[1].x_advance = 0;
	d->run[1].x_offset = -3.5f;
	d->run[1].y_offset = -4;
	d->revision = type_ctx_atlas_revision(d->a);
	type_set_scratch(d->a, NULL);
	assert(type_measure_shaped(d->a, d->run, 2, 32, &d->written, &d->measured) == TYPE_OK);
	assert(d->written == 2 && d->measured.advance_x == 12.25f && d->measured.has_ink);
	assert(type_ctx_atlas_revision(d->a) == d->revision);
	/* Existing cached glyph IDs need no scratch, unlike a cold bitmap. */
	assert(type_layout_shaped(d->a, d->run, 2, 32, d->cells, 2, &d->written, &d->rendered) ==
	       TYPE_OK);
	assert(d->written == 2 && d->cells[0].cluster == 9 && d->cells[1].cluster == 2);
	assert(d->cells[1].x == 8.75f && d->cells[1].y == -4);
	assert(d->measured.advance_x == d->rendered.advance_x &&
	       d->measured.advance_y == d->rendered.advance_y);
	assert(d->measured.min_x == d->rendered.min_x && d->measured.min_y == d->rendered.min_y);
	assert(d->measured.max_x == d->rendered.max_x && d->measured.max_y == d->rendered.max_y);
	assert(type_layout_shaped(d->a, d->run, 2, 32, d->cells, 1, &d->written, &d->prefix) ==
	       TYPE_ERR_BUF_TOO_SMALL);
	assert(d->written == 1 && d->prefix.advance_x == 12.25f);
	d->run[1].glyph_id = UINT32_MAX;
	assert(type_measure_shaped(d->a, d->run, 2, 32, &d->written, &d->prefix) ==
	           TYPE_ERR_GLYPH_NOT_FOUND &&
	       d->written == 1);
	d->run[1].glyph_id = d->gid;
	d->run[1].x_offset = NAN;
	assert(type_layout_shaped(d->a, d->run, 2, 32, d->cells, 2, &d->written, NULL) ==
	           TYPE_ERR_INVALID_FONT &&
	       d->written == 1);
}

static void check_space_and_stale_ids(TypeFixture *d)
{
	assert(!type_utf8_validate("\xF0\x80\x80\x80", 4));
	assert(type_font_glyph_index(d->font, ' ', &d->space) == TYPE_OK);
	assert(type_font_render_bitmap(d->font, d->space, 32, TYPE_AA_BOX, NULL, 0, &d->info) ==
	       TYPE_OK);
	assert(d->info.width == 0 && d->info.height == 0);
	memset(d->run, 0, sizeof d->run);
	d->run[0].font_id = d->id_a;
	d->run[0].glyph_id = d->space;
	d->run[0].x_advance = 8.5f;
	assert(type_measure_shaped(d->a, d->run, 1, 32, &d->written, &d->measured) == TYPE_OK);
	assert(d->written == 1 && d->measured.advance_x == 8.5f && !d->measured.has_ink);
	assert(d->measured.min_x == 0 && d->measured.max_x == 0 && d->measured.min_y == 0 &&
	       d->measured.max_y == 0);
	assert(type_glyph_id(d->a, UINT32_MAX, d->gid, 32, &d->ga) == TYPE_ERR_INVALID_FONT);
	type_clear_fonts(d->a); /* stale IDs invalid after clear until reused */
	assert(type_glyph_id(d->a, d->id_a, d->gid, 32, &d->ga) == TYPE_ERR_INVALID_FONT);
}

int main(int argc, char **argv)
{
	const char *path = argc > 1 ? argv[1] : "assets/NotoSans-Regular.ttf";
	TypeFixture fixture = {0};
	fixture.a = fixture_ctx(permanent_a, sizeof permanent_a);
	fixture.b = fixture_ctx(permanent_b, sizeof permanent_b);
	check_setup(&fixture, path);
	check_glyph_cache(&fixture);
	check_cold_glyphs(&fixture);
	check_bitmaps(&fixture);
	check_shaped_runs(&fixture);
	check_space_and_stale_ids(&fixture);
	puts("Type contracts: passed");
	return 0;
}
