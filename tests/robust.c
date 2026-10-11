/* GPL-3.0-only OR a separately negotiated commercial license; see LICENSE. */
#define TYPE_IMPLEMENTATION
#include "../Type.h"
#include "support.h"
#include "synthetic.h"
static unsigned char bytes[2000000], fontbuf[4096], facebuf[256], scratchbuf[4000000],
    pixels[65536], ctxbuf[2000000];
static TypeVertex vertices[TYPE_MAX_VERTICES];
static TypeFont *place(size_t n)
{
    TypeTTF *t = type_ttf_place(test_align(fontbuf), sizeof fontbuf - 15, bytes, n);
    if (!t)
        fprintf(stderr, "synthetic parse error=%d\n", type_ttf_error(test_align(fontbuf)));
    assert(t);
    return type_font_from_ttf(test_align(facebuf), sizeof facebuf - 15, t, NULL);
}
int main(int argc, char **argv)
{
    int mode;
    TypeScratch s;
    TypeBitmap info;
    uint32_t count;
    TypeFont *f;
    TypeError e;
    TypeParams p;
    TypeCtx *ctx;
    size_t n;
    uint16_t gid;
    unsigned char map[1024];
    (void)test_fonts;
    type_scratch_init(&s, scratchbuf, sizeof scratchbuf);
    for (mode = 0; mode < 2; mode++) {
        n = synth_font(bytes, mode, 0, 0);
        f = place(n);
        assert(type_font_outline(f, 1, &s, vertices, TYPE_MAX_VERTICES, &count) == TYPE_OK &&
               count == 4);
        assert(vertices[1].type == TYPE_VERTEX_CUBIC);
        assert(type_font_render_bitmap_scratch(f, 1, 32, TYPE_AA_BOX, pixels, sizeof pixels, &s,
                                               &info) == TYPE_OK);
        assert(info.width == 16 && info.height == 16);
        if (argc == 2) {
            char path[256];
            FILE *out;
            snprintf(path, sizeof path, "%s/synthetic-cff%d.otf", argv[1], mode + 1);
            out = fopen(path, "wb");
            assert(out);
            assert(fwrite(bytes, 1, n, out) == n);
            assert(!fclose(out));
        }
        n = synth_font(bytes, mode, 1, 0);
        f = place(n);
        assert(type_font_outline(f, 1, &s, NULL, 0, &count) == TYPE_ERR_LIMIT);
    }
    /* CFF2 subroutines terminate at their byte boundary without return. */
    {
        TypeCS c;
        TypeOutline o;
        unsigned char sub[] = {139, 139, 21};
        n = synth_font(bytes, 1, 0, 0);
        f = place(n);
        memset(&c, 0, sizeof c);
        memset(&o, 0, sizeof o);
        c.f = f->ttf;
        c.o = &o;
        c.budget = 100;
        c.matrix[0] = c.matrix[3] = 1;
        assert(type_cs_run(&c, (TypeView){sub, sizeof sub}, 1, 1) == TYPE_OK && c.open && o.n == 1);
    }
    n = synth_font(bytes, 0, 2, 0);
    f = place(n);
    assert(type_font_outline(f, 2, &s, vertices, TYPE_MAX_VERTICES, &count) == TYPE_OK &&
           count == 8);
    assert(vertices[4].x == 100 && vertices[4].y == 200);
    for (mode = 0; mode < 2; mode++) {
        n = synth_font(bytes, mode, 3, 0);
        f = place(n);
        assert(type_font_outline(f, 1, &s, NULL, 0, &count) == TYPE_ERR_LIMIT);
    }
    n = synth_font(bytes, 1, 4, 0);
    f = place(n);
    assert(type_font_outline(f, 1, &s, vertices, TYPE_MAX_VERTICES, &count) == TYPE_OK &&
           vertices[0].x == 100);
    n = synth_font(bytes, 1, 5, 0);
    f = place(n);
    assert(f->ttf->default_vs == 1 &&
           type_font_outline(f, 1, &s, vertices, TYPE_MAX_VERTICES, &count) == TYPE_OK &&
           vertices[0].x == 100);
    {
        TypeCS c;
        unsigned char call[] = {29};
        memset(&c, 0, sizeof c);
        c.f = f->ttf;
        c.s[0] = INT32_MAX;
        c.n = 1;
        c.budget = 10;
        assert(type_cs_run(&c, (TypeView){call, 1}, 0, 0) == TYPE_ERR_INVALID_FONT);
    }
    for (mode = 1; mode <= 3; mode++) {
        n = synth_font(bytes, 0, 0, mode);
        f = place(n);
        e = type_font_outline(f, 1, &s, NULL, 0, &count);
        assert(e == (mode == 1 ? TYPE_ERR_INVALID_FONT : (mode == 2 ? TYPE_ERR_LIMIT : TYPE_OK)));
    }
    /* Limits are typed, before trusting nested point/instruction counts. */
    {
        unsigned field;
        for (field = 0; field < 3; field++) {
            TypeView all;
            size_t off;
            n = synth_font(bytes, 0, 0, 3);
            all = (TypeView){bytes, n};
            off = type_at(all, 12 + 5 * 16 + 8, 4);
            off += type_at(all, type_at(all, 12 + 6 * 16 + 8, 4) + 4, 4);
            synth_u16(bytes + off + (field == 0 ? 0 : (field == 1 ? 10 : 12)),
                      field == 0 ? 4097 : (field == 1 ? 16384 : 32769));
            f = place(n);
            assert(type_font_outline(f, 1, &s, NULL, 0, &count) == TYPE_ERR_LIMIT);
        }
    }
    /* A complete optional vertical metrics pair, including repeated last advance. */
    {
        size_t olddir, off, vhea;
        uint32_t i, tables;
        n = synth_font(bytes, 0, 0, 3);
        tables = type_at((TypeView){bytes, n}, 4, 2);
        olddir = 12 + 16 * tables;
        memmove(bytes + olddir + 32, bytes + olddir, n - olddir);
        for (i = 0; i < tables; i++) {
            off = 12 + 16 * i + 8;
            synth_u32(bytes + off, type_at((TypeView){bytes, n}, off, 4) + 32);
        }
        n += 32;
        vhea = n;
        memset(bytes + n, 0, 44);
        synth_u32(bytes + n, 0x10000);
        synth_u16(bytes + n + 34, 1);
        synth_u16(bytes + n + 36, 900);
        synth_u16(bytes + n + 38, 20);
        synth_u16(bytes + n + 40, 40);
        memcpy(bytes + olddir, "vhea", 4);
        synth_u32(bytes + olddir + 8, (uint32_t)vhea);
        synth_u32(bytes + olddir + 12, 36);
        memcpy(bytes + olddir + 16, "vmtx", 4);
        synth_u32(bytes + olddir + 24, (uint32_t)vhea + 36);
        synth_u32(bytes + olddir + 28, 6);
        synth_u16(bytes + 4, tables + 2);
        n += 44;
        f = place(n);
        {
            uint16_t advance;
            int16_t bearing;
            assert(type_font_vertical_advance(f, 1, &advance, &bearing) == TYPE_OK &&
                   advance == 900 && bearing == 40);
        }
    }
    /* TTC face selection, with absolute table offsets relocated by 32 bytes. */
    n = synth_font(bytes, 0, 0, 3);
    {
        uint32_t i, tables = type_at((TypeView){bytes, n}, 4, 2);
        memmove(bytes + 32, bytes, n);
        for (i = 0; i < tables; i++) {
            size_t off = 32 + 12 + 16 * i + 8;
            uint32_t v = type_at((TypeView){bytes, n + 32}, off, 4);
            synth_u32(bytes + off, v + 32);
        }
        memset(bytes, 0, 32);
        memcpy(bytes, "ttcf", 4);
        synth_u32(bytes + 4, 0x10000);
        synth_u32(bytes + 8, 2);
        synth_u32(bytes + 12, 32);
        synth_u32(bytes + 16, 32);
        assert(type_ttf_place_index(test_align(fontbuf), sizeof fontbuf - 15, bytes, n + 32, 1));
        assert(!type_ttf_place_index(test_align(fontbuf), sizeof fontbuf - 15, bytes, n + 32, 2));
    }
    /* cmap 0/6/13/14, as complete checked views. */
    memset(map, 0, sizeof map);
    synth_u16(map + 2, 262);
    map[6 + 65] = 1;
    assert(type_cmap_validate((TypeView){map, 262}, 2));
    synth_u16(map, 6);
    synth_u16(map + 2, 12);
    synth_u16(map + 6, 65);
    synth_u16(map + 8, 1);
    synth_u16(map + 10, 1);
    assert(type_cmap_validate((TypeView){map, 12}, 2));
    memset(map, 0, sizeof map);
    synth_u16(map, 13);
    synth_u32(map + 4, 28);
    synth_u32(map + 12, 1);
    synth_u32(map + 16, 0x10000);
    synth_u32(map + 20, 0x10ffff);
    synth_u32(map + 24, 1);
    assert(type_cmap_validate((TypeView){map, 28}, 2));
    memset(map, 0, sizeof map);
    synth_u16(map, 14);
    synth_u32(map + 2, 10);
    assert(type_cmap_validate((TypeView){map, 10}, 2));
    /* Context, shaped positioning, scratch rollback, shelves and capacity failure. */
    n = synth_font(bytes, 0, 0, 3);
    f = place(n);
    assert(type_font_glyph_index(f, 65, &gid) == TYPE_OK && gid == 1);
    type_params_default(&p);
    p.atlas_width = p.atlas_height = 64;
    p.max_faces = 2;
    p.cache_capacity = 8;
    assert(type_memory(&p) < sizeof ctxbuf - 15);
    ctx = type_place(test_align(ctxbuf), sizeof ctxbuf - 15, &p);
    assert(ctx);
    assert(type_add_font(ctx, bytes, n, NULL, &count) == TYPE_OK);
    type_set_scratch(ctx, &s);
    {
        TypeShapedGlyph run = {0, 1, 4, 20.5f, 0, 0.25f, -0.5f};
        TypePositionedGlyph out;
        TypeTextMetrics m;
        uint32_t done;
        assert(type_layout_shaped(ctx, &run, 1, 32, &out, 1, &done, &m) == TYPE_OK && done == 1 &&
               m.advance_x == 20.5f);
    }
    type_scratch_init(&s, scratchbuf, 1);
    memset(pixels, 255, sizeof pixels);
    e = type_font_render_bitmap_scratch(f, 1, 32, TYPE_AA_BOX, pixels, sizeof pixels, &s, NULL);
    assert(e == TYPE_ERR_SCRATCH_FULL && s.used == 0 && s.failed);
    {
        size_t i;
        for (i = 0; i < 256; i++)
            assert(!pixels[i]);
    }
    {
        TypeAtlas *a = type_atlas_place(test_align(ctxbuf), sizeof ctxbuf - 15, 10, 10);
        TypeRect r;
        assert(a);
        assert(type_atlas_pack(a, 6, 4, &r) && r.x == 0 && r.y == 0);
        assert(type_atlas_pack(a, 5, 6, &r) && r.x == 0 && r.y == 4);
        assert(!type_atlas_pack(a, 11, 1, &r));
        type_atlas_clear(a);
        assert(type_atlas_pack(a, 10, 10, &r) && r.x == 0 && r.y == 0);
    }
    /* Exact truncation boundaries on a tiny complete face. */
    n = synth_font(bytes, 0, 0, 3);
    {
        size_t cut;
        for (cut = 0; cut < n; cut++)
            assert(!type_ttf_place(test_align(fontbuf), sizeof fontbuf - 15, bytes, cut));
    }
    if (argc == 2) {
        unsigned i;
        for (i = 0; i < 11; i++) {
            char path[256];
            FILE *out;
            n = synth_font(bytes, i == 10 ? 1 : i & 1u,
                           i == 10 ? 5 : (i < 6 ? (i < 2 ? 1 : (i < 4 ? 3 : 4)) : 0),
                           i >= 6 && i < 10 ? (int)(i - 6) % 3 + 1 : 0);
            snprintf(path, sizeof path, "%s/seed-%u.sfnt", argv[1], i);
            out = fopen(path, "wb");
            assert(out && fwrite(bytes, 1, n, out) == n && !fclose(out));
        }
    }
    printf("robust: CFF/CFF2 cubic, subr loops, TTC, cmap, cycles/depth, scratch, shaped runs "
           "and shelves passed\n");
    return 0;
}
