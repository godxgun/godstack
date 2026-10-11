/* GPL-3.0-only OR a separately negotiated commercial license; see LICENSE. */
#define TYPE_IMPLEMENTATION
#include "../Type.h"
#include "support.h"
#include "synthetic.h"
static unsigned char bytes[2000000], placed[4096], face[1024], temporary[4000000], pixels[100000];
static uint64_t digest = UINT64_C(1469598103934665603);
static unsigned renders;
static void hash(unsigned value)
{
    digest ^= value;
    digest *= UINT64_C(1099511628211);
}
static void render(TypeFont *font, uint16_t gid)
{
    static const float sizes[] = {8, 12, 16, 24, 32, 48, 64, 96};
    size_t j, k;
    TypeScratch s;
    type_scratch_init(&s, temporary, sizeof temporary);
    for (j = 0; j < sizeof sizes / sizeof sizes[0]; j++) {
        TypeBitmap b;
        assert(type_font_render_bitmap_scratch(font, gid, sizes[j], TYPE_AA_BOX, pixels,
                                               sizeof pixels, &s, &b) == TYPE_OK);
        hash(b.width);
        hash(b.height);
        for (k = 0; k < type_bitmap_bytes(&b); k++)
            hash(pixels[k]);
        assert(!s.used);
        renders++;
    }
}
int main(void)
{
    static const uint32_t cps[] = {32, 65, 103, 233, 937, 1046, 1488, 1604, 1575, 128512};
    unsigned i, k;
    for (i = 0; i < 13; i++) {
        size_t n;
        TypeTTF *ttf;
        TypeFont *font;
        if (i < 7) {
            char path[256];
            snprintf(path, sizeof path, "tests/fonts/%s", test_fonts[i]);
            n = test_read(path, bytes, sizeof bytes);
        } else {
            unsigned m = i - 7;
            n = synth_font(bytes, m & 1u, m >= 4 ? (m & 1u ? 4 : 0) : (m == 2 ? 2 : 0), 0);
        }
        ttf = type_ttf_place(test_align(placed), sizeof placed - 15, bytes, n);
        assert(ttf);
        font = type_font_from_ttf(test_align(face), sizeof face - 15, ttf, NULL);
        assert(font);
        if (i < 7) {
            for (k = 0; k < sizeof cps / sizeof cps[0]; k++) {
                uint16_t gid = 0;
                (void)type_font_glyph_index(font, cps[k], &gid);
                render(font, gid);
            }
        } else
            render(font, (uint16_t)(i == 9 ? 2 : 1));
    }
    printf("multisize digest=%016llx renders=%u (glyf/CFF/CFF2, 8..96px)\n",
           (unsigned long long)digest, renders);
    return 0;
}
