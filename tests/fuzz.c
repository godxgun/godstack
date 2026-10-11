/* GPL-3.0-only OR a separately negotiated commercial license; see LICENSE. */
#define TYPE_IMPLEMENTATION
#include "../Type.h"
#include <stdio.h>
#include <stdlib.h>
static unsigned char parsebuf[4096], facebuf[256], temporary[4000000], pixels[65536];
static void *fuzz_align(void *p)
{
    return (void *)(((uintptr_t)p + 15u) & ~(uintptr_t)15u);
}
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    TypeTTF *t;
    TypeFont *f;
    TypeScratch s;
    unsigned i;
    uint32_t selector = 0;
    TypeMetrics m;
    TypeBitmap b;
    if (size > TYPE_MAX_FONT_BYTES)
        return 0;
    for (i = 0; i < 8 && i < size; i++)
        selector = selector * 33 + data[size - 1 - i];
    t = type_ttf_place_index(fuzz_align(parsebuf), sizeof parsebuf - 15, data, size, 0);
    if (!t)
        return 0;
    f = type_font_from_ttf(fuzz_align(facebuf), sizeof facebuf - 15, t, NULL);
    if (!f)
        return 0;
    type_scratch_init(&s, temporary, sizeof temporary);
    (void)type_font_metrics(f, 32, &m);
    for (i = 0; i < 3; i++) {
        uint16_t gid, advance;
        uint32_t count;
        TypeError e;
        (void)type_font_glyph_index(f, i == 0 ? 65 : (selector % (0x110000u)), &gid);
        gid = (uint16_t)(i == 0 ? selector % (uint32_t)t->num_glyphs
                                : (i == 1 ? 0 : 1 % (uint32_t)t->num_glyphs));
        (void)type_font_advance(f, gid, &advance);
        e = type_font_outline(f, gid, &s, NULL, 0, &count);
        if (e == TYPE_OK && i == 0) {
            e = type_font_bitmap_info(f, gid, 16, &b);
            if (e == TYPE_OK && b.width <= 256 && b.height <= 256)
                (void)type_font_render_bitmap_scratch(f, gid, 16, TYPE_AA_BOX, pixels,
                                                      sizeof pixels, &s, NULL);
        }
    }
    return 0;
}
#ifndef TYPE_LIBFUZZER
#include "synthetic.h"
static uint64_t rng = UINT64_C(0x6a09e667f3bcc909);
static uint32_t random32(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return (uint32_t)rng;
}
static unsigned char original[2000000], mutated[2000000];
static uint32_t read32(const unsigned char *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}
int main(int argc, char **argv)
{
    static const char *fonts[] = {
        "NotoSans-Regular.ttf",       "NotoSans-Bold.ttf",          "NotoSansHebrew-Regular.ttf",
        "NotoSansHebrew-Bold.ttf",    "NotoSansArabic-Regular.ttf", "NotoSansArabic-Bold.ttf",
        "LiberationSans-Regular.ttf", "synthetic-cff1.otf",         "synthetic-cff2.otf"};
    uint32_t total = argc > 1 ? (uint32_t)strtoul(argv[1], NULL, 10) : 200000u, done = 0, fi;
    for (fi = 0; fi < 9; fi++) {
        FILE *file;
        char path[256];
        size_t n;
        uint32_t iterations = total / 9 + (fi < total % 9), it;
        snprintf(path, sizeof path, "tests/fonts/%s", fonts[fi]);
        file = fopen(path, "rb");
        if (!file)
            return 2;
        n = fread(original, 1, sizeof original, file);
        if (ferror(file) || fgetc(file) != EOF || fclose(file) || !n)
            return 2;
        for (it = 0; it < iterations; it++) {
            size_t size = n;
            uint32_t mode = it % 9, j;
            memcpy(mutated, original, n);
            if (mode == 0) {
                for (j = 0; j < 1 + random32() % 8; j++)
                    mutated[random32() % n] ^= (uint8_t)(1u << (random32() % 8));
            } else if (mode == 1)
                size = random32() % (n + 1);
            else if (mode == 2 || mode == 3) {
                uint32_t tables = (uint32_t)original[4] * 256 + original[5],
                         index = random32() % tables;
                size_t at = 12 + 16 * index + (mode == 2 ? 8 : 12);
                synth_u32(mutated + at, (it & 1u) ? UINT32_MAX : random32());
            } else if (mode == 4) {
                size_t at = random32() % (n - 3);
                synth_u32(mutated + at, UINT32_MAX);
            } else if (mode == 5) {
                uint32_t tables = (uint32_t)original[4] * 256 + original[5],
                         index = random32() % tables;
                size_t entry = 12 + 16 * index;
                uint32_t off = read32(original + entry + 8), len = read32(original + entry + 12);
                if (len > 4) {
                    size_t at = off + random32() % (len - 3);
                    synth_u32(mutated + at, (it & 1u) ? 0 : UINT32_MAX);
                }
            } else if (mode == 6) {
                synth_u16(mutated + 4, 65535);
            } else if (mode == 7) { /* valid fonts keep exercising complete glyph/raster
                                       paths */
                mutated[n - 1] ^= (uint8_t)random32();
            } else {
                size_t at = random32() % n;
                size = at + 1;
                mutated[at] = 255;
            }
            LLVMFuzzerTestOneInput(mutated, size);
            done++;
        }
    }
    /* Explicit global/local subroutine loops, default blends, composite cycles/depth. */
    for (fi = 0; fi < 8; fi++) {
        size_t n = synth_font(mutated, fi & 1u, fi < 4 ? (fi < 2 ? 1 : 3) : 0,
                              fi >= 4 && fi < 6 ? (int)fi - 3 : 0);
        LLVMFuzzerTestOneInput(mutated, n);
    }
    printf("mutation fuzz: %u inputs + 8 cycle/depth/subr seeds; zero sanitizer findings\n", done);
    return 0;
}
#endif
