/* MIT License; see LICENSE. */
#define TYPE_IMPLEMENTATION
#include "../Type.h"
#include "support.h"
#include <stdlib.h>
static unsigned char bytes[2000000], placed[4096], face[1024], temporary[4000000], pixels[100000],
    reference[100000];
static TypeVertex vertices[TYPE_MAX_VERTICES];
int main(void)
{
    unsigned fi;
    unsigned maxdiff = 0, mismatched = 0;
    double worstink = 0;
    uint64_t digest = UINT64_C(1469598103934665603);
    for (fi = 0; fi < 7; fi++) {
        char path[256];
        size_t len;
        TypeTTF *ttf;
        TypeFont *font;
        TypeScratch scratch;
        TypeMetrics m;
        FILE *ref;
        int upem, asc, desc, gap, glyphs, j;
        snprintf(path, sizeof path, "tests/fonts/%s", test_fonts[fi]);
        len = test_read(path, bytes, sizeof bytes);
        ttf = type_ttf_place(test_align(placed), sizeof placed - 15, bytes, len);
        if (!ttf) {
            fprintf(stderr, "parse %s: error %d\n", path, type_ttf_error(test_align(placed)));
            return 2;
        }
        font = type_font_from_ttf(test_align(face), sizeof face - 15, ttf, NULL);
        assert(font);
        type_scratch_init(&scratch, temporary, sizeof temporary);
        snprintf(path, sizeof path, "tests/reference/%s.txt", test_fonts[fi]);
        ref = fopen(path, "r");
        assert(ref);
        assert(fscanf(ref, "%d%d%d%d%d", &upem, &asc, &desc, &gap, &glyphs) == 5);
        assert(ttf->num_glyphs == glyphs);
        assert(type_font_metrics(font, (float)upem, &m) == TYPE_OK);
        assert(m.units_per_em == upem && m.ascender == asc && m.descender == desc &&
               m.line_gap == gap);
        for (j = 0; j < 10; j++) {
            unsigned cp;
            int gid, adv, lsb, vn, k, x0, y0, x1, y1;
            uint16_t g, advance;
            uint32_t count;
            TypeBitmap info;
            uint64_t ink = 0, refink = 0;
            unsigned glyphdiff = 0;
            TypeError err;
            assert(fscanf(ref, "%u%d%d%d%d", &cp, &gid, &adv, &lsb, &vn) == 5);
            err = type_font_glyph_index(font, cp, &g);
            assert(gid ? err == TYPE_OK && g == gid : err == TYPE_ERR_GLYPH_NOT_FOUND);
            assert(type_font_advance(font, (uint16_t)gid, &advance) == TYPE_OK && advance == adv);
            err = type_font_outline(font, (uint16_t)gid, &scratch, vertices, TYPE_MAX_VERTICES,
                                    &count);
            assert(err == TYPE_OK);
            if (count != (uint32_t)vn) {
                fprintf(stderr, "outline count %s U+%x: %u / %d\n", test_fonts[fi], cp, count, vn);
                mismatched++;
            }
            for (k = 0; k < vn; k++) {
                int op, x, y, cx, cy, cx1, cy1;
                assert(fscanf(ref, "%d%d%d%d%d%d%d", &op, &x, &y, &cx, &cy, &cx1, &cy1) == 7);
                if (count == (uint32_t)vn) {
                    TypeVertex v = vertices[k];
                    if (v.type != op || fabs(v.x - x) > 0.51 || fabs(v.y - y) > 0.51 ||
                        (op >= 3 && (fabs(v.cx - cx) > 0.51 || fabs(v.cy - cy) > 0.51))) {
                        fprintf(stderr, "outline %s cp%x vertex%d differs\n", test_fonts[fi], cp,
                                k);
                        mismatched++;
                    }
                }
            }
            assert(fscanf(ref, "%d%d%d%d", &x0, &y0, &x1, &y1) == 4);
            assert(type_font_bitmap_info(font, (uint16_t)gid, 32, &info) == TYPE_OK);
            assert(info.width == x1 - x0 && info.height == y1 - y0);
            if (info.width && info.height)
                assert(info.bearing_x == x0 && info.bearing_y == -y0);
            assert(type_font_render_bitmap_scratch(font, (uint16_t)gid, 32, TYPE_AA_BOX, pixels,
                                                   sizeof pixels, &scratch, &info) == TYPE_OK);
            assert(scratch.used == 0);
            snprintf(path, sizeof path, "tests/reference/%s-%u.bin", test_fonts[fi], cp);
            assert(test_read(path, reference, sizeof reference) == type_bitmap_bytes(&info));
            for (k = 0; k < (int)type_bitmap_bytes(&info); k++) {
                unsigned d = (unsigned)abs((int)pixels[k] - reference[k]);
                if (d > maxdiff)
                    maxdiff = d;
                if (d > glyphdiff)
                    glyphdiff = d;
                ink += pixels[k];
                refink += reference[k];
                digest ^= pixels[k];
                digest *= UINT64_C(1099511628211);
            }
            {
                double delta = refink ? fabs((double)ink - (double)refink) / refink : 0;
                if (delta > worstink)
                    worstink = delta;
                if (glyphdiff > 20)
                    printf("raster %s U+%x: max=%u ink=%.6f%%\n", test_fonts[fi], cp, glyphdiff,
                           delta * 100);
                assert(delta < 0.04);
            }
        }
        assert(!fclose(ref));
        /* Validate every glyph's metrics and decoded outline, including composites. */
        {
            int gid;
            uint16_t advance;
            for (gid = 0; gid < glyphs; gid++) {
                uint32_t count;
                assert(type_font_advance(font, (uint16_t)gid, &advance) == TYPE_OK);
                assert(type_font_outline(font, (uint16_t)gid, &scratch, NULL, 0, &count) ==
                       TYPE_OK);
            }
        }
        {
            const void *name;
            size_t n;
            uint16_t pid;
            assert(type_font_name(font, 4, &name, &n, &pid) == TYPE_OK && name && n);
        }
    }
    printf("reference: metrics/cmap/advances equal; outline differences=%u; raster max=%u "
           "ink=%.6f%%\n",
           mismatched, maxdiff, worstink * 100);
    printf("digest=%016llx\n", (unsigned long long)digest);
    assert(maxdiff <= 96);
    assert(mismatched == 0);
    return 0;
}
