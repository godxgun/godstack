/* MIT License; see LICENSE. */
#ifndef TYPE_SYNTHETIC_H
#define TYPE_SYNTHETIC_H
/* Original synthetic sfnt data: MIT License; see LICENSE. */
static void synth_u16(unsigned char *p, uint32_t n)
{
    p[0] = (unsigned char)(n >> 8);
    p[1] = (unsigned char)n;
}
static void synth_u32(unsigned char *p, uint32_t n)
{
    p[0] = (unsigned char)(n >> 24);
    p[1] = (unsigned char)(n >> 16);
    p[2] = (unsigned char)(n >> 8);
    p[3] = (unsigned char)n;
}
static size_t synth_num(unsigned char *p, int n)
{
    p[0] = 28;
    synth_u16(p + 1, (uint16_t)n);
    return 3;
}
static size_t synth_idx(unsigned char *p, const unsigned char *a, size_t an, const unsigned char *b,
                        size_t bn, int wide)
{
    size_t at = wide ? 4 : 2;
    unsigned n = b ? 2 : 1;
    if (wide)
        synth_u32(p, n);
    else
        synth_u16(p, n);
    p[at++] = 2;
    synth_u16(p + at, 1);
    at += 2;
    synth_u16(p + at, (uint32_t)an + 1);
    at += 2;
    if (b) {
        synth_u16(p + at, (uint32_t)(an + bn + 1));
        at += 2;
    }
    memcpy(p + at, a, an);
    at += an;
    if (b) {
        memcpy(p + at, b, bn);
        at += bn;
    }
    return at;
}
static size_t synth_font(unsigned char *out, int cff2, int loop, int ttmode)
{
    unsigned char head[54] = {0}, hhea[36] = {0}, hmtx[128] = {0}, maxp[32] = {0}, cmap[40] = {0},
                  outline[4096] = {0}, loca[128] = {0};
    unsigned char cs[128], dict[32], empty[1] = {14}, sub[3] = {32, 29, 11};
    size_t csn = 0, at = 0, dn = 0, off;
    unsigned i, n = ttmode == 2 ? 20 : (loop == 2 ? 3 : 2), tables = ttmode ? 7 : 6;
    const unsigned char *data[7];
    size_t lengths[7];
    const char *tags[7] = {
        "head", "hhea", "hmtx", "maxp", "cmap", ttmode ? "glyf" : (cff2 ? "CFF2" : "CFF "), "loca"};
    memset(out, 0, 8192);
    synth_u32(head, 0x10000);
    synth_u32(head + 12, 0x5f0f3cf5);
    synth_u16(head + 18, 1000);
    synth_u16(head + 50, 1);
    synth_u32(hhea, 0x10000);
    synth_u16(hhea + 4, 800);
    synth_u16(hhea + 6, (uint16_t)-200);
    synth_u16(hhea + 34, n);
    synth_u32(maxp, ttmode ? 0x10000 : 0x5000);
    synth_u16(maxp + 4, n);
    for (i = 0; i < n; i++)
        synth_u16(hmtx + 4 * i, 600);
    synth_u16(cmap + 2, 1);
    synth_u16(cmap + 4, 3);
    synth_u16(cmap + 6, 10);
    synth_u32(cmap + 8, 12);
    synth_u16(cmap + 12, 12);
    synth_u32(cmap + 16, 28);
    synth_u32(cmap + 24, 1);
    synth_u32(cmap + 28, 65);
    synth_u32(cmap + 32, 65);
    synth_u32(cmap + 36, 1);
    if (ttmode) {
        for (i = 0; i < n; i++) {
            synth_u32(loca + 4 * i, (uint32_t)at);
            if (i == 0 || (ttmode == 2 && i == n - 1) ||
                ttmode == 3) { /* triangle: three on-curve points */
                synth_u16(outline + at, 1);
                synth_u16(outline + at + 6, 500);
                synth_u16(outline + at + 8, 500);
                synth_u16(outline + at + 10, 2);
                synth_u16(outline + at + 12, 0);
                outline[at + 14] = 1;
                outline[at + 15] = 1;
                outline[at + 16] = 1;
                synth_u16(outline + at + 17, 0);
                synth_u16(outline + at + 19, 500);
                synth_u16(outline + at + 21, (uint16_t)-500);
                synth_u16(outline + at + 23, 0);
                synth_u16(outline + at + 25, 0);
                synth_u16(outline + at + 27, 500);
                at += 30;
            } else {
                synth_u16(outline + at, 65535);
                synth_u16(outline + at + 6, 500);
                synth_u16(outline + at + 8, 500);
                synth_u16(outline + at + 10, 3);
                synth_u16(outline + at + 12, ttmode == 1 ? i : i + 1);
                at += 18;
            }
        }
        synth_u32(loca + 4 * n, (uint32_t)at);
    } else {
        if ((loop == 4 || loop == 5)) {
            csn += synth_num(cs + csn, 100);
            csn += synth_num(cs + csn, 100);
            cs[csn++] = 140;
            cs[csn++] = 16;
        } else
            cs[csn++] = 139;
        cs[csn++] = 139;
        cs[csn++] = 21;
        if (loop == 1 || loop == 3) {
            cs[csn++] = 32;
            cs[csn++] = loop == 3 ? 10 : 29;
        } else {
            int delta[6] = {0, 500, 500, 0, 0, -500};
            for (i = 0; i < 6; i++)
                csn += synth_num(cs + csn, delta[i]);
            cs[csn++] = 8;
            csn += synth_num(cs + csn, -500);
            cs[csn++] = 139;
            cs[csn++] = 5;
        }
        if (!cff2)
            cs[csn++] = 14;
        if (cff2) {
            outline[at++] = 2;
            outline[at++] = 0;
            outline[at++] = 5;
            synth_u16(outline + at,
                      loop == 5 ? 19 : ((loop == 4 || loop == 5) ? 12 : (loop == 3 ? 13 : 6)));
            at += 2;
        } else {
            outline[at++] = 1;
            outline[at++] = 0;
            outline[at++] = 4;
            outline[at++] = 4;
            at += synth_idx(outline + at, (const unsigned char *)"T", 1, NULL, 0, 0);
        }
        /* Top DICT uses a fixed 5-byte offset operand, allowing one patch. */
        dict[dn++] = 29;
        synth_u32(dict + dn, 0);
        dn += 4;
        dict[dn++] = 17;
        if (loop == 3) {
            dict[dn++] = 141;
            dict[dn++] = 29;
            synth_u32(dict + dn, 0);
            dn += 4;
            dict[dn++] = 18;
        }
        if ((loop == 4 || loop == 5)) {
            dict[dn++] = 29;
            synth_u32(dict + dn, 0);
            dn += 4;
            dict[dn++] = 24;
        }
        if (loop == 5) {
            dict[dn++] = 141;
            dict[dn++] = 29;
            synth_u32(dict + dn, 0);
            dn += 4;
            dict[dn++] = 18;
        }
        off = at;
        if (cff2) {
            memcpy(outline + at, dict, dn);
            at += dn;
        } else
            at += synth_idx(outline + at, dict, dn, NULL, 0, 0);
        if (!cff2) {
            synth_u16(outline + at, 0);
            at += 2;
        } /* String INDEX */
        if (loop == 1)
            at += synth_idx(outline + at, sub, sizeof sub, NULL, 0, cff2);
        else {
            if (cff2) {
                synth_u32(outline + at, 0);
                at += 4;
            } else {
                synth_u16(outline + at, 0);
                at += 2;
            }
        }
        if (loop == 3) {
            synth_u32(outline + off + (cff2 ? 8 : 15), (uint32_t)at);
            outline[at++] = 141;
            outline[at++] = 19;
            sub[1] = 10;
            at += synth_idx(outline + at, sub, 3, NULL, 0, cff2);
        }
        if ((loop == 4 || loop == 5)) {
            synth_u32(outline + off + (cff2 ? 7 : 14), (uint32_t)at);
            if (loop == 5) {
                /* Record 0 has no regions; Private vsindex chooses record 1. */
                synth_u16(outline + at, 40);
                synth_u16(outline + at + 2, 1);
                synth_u32(outline + at + 4, 30);
                synth_u16(outline + at + 8, 2);
                synth_u32(outline + at + 10, 16);
                synth_u32(outline + at + 14, 22);
                synth_u16(outline + at + 28, 1);
                synth_u16(outline + at + 32, 1);
                synth_u16(outline + at + 34, 1);
                synth_u16(outline + at + 38, 16384);
                synth_u16(outline + at + 40, 16384);
                at += 42;
                synth_u32(outline + off + 14, (uint32_t)at);
                outline[at++] = 140;
                outline[at++] = 22;
            } else {
                synth_u16(outline + at, 30);
                synth_u16(outline + at + 2, 1);
                synth_u32(outline + at + 4, 20);
                synth_u16(outline + at + 8, 1);
                synth_u32(outline + at + 10, 12);
                synth_u16(outline + at + 18, 1);
                synth_u16(outline + at + 22, 1);
                synth_u16(outline + at + 24, 1);
                synth_u16(outline + at + 28, 16384);
                synth_u16(outline + at + 30, 16384);
                at += 32;
            }
        }
        synth_u32(outline + off + (cff2 ? 1 : 8), (uint32_t)at);
        if (loop == 2) {
            unsigned char seac[16];
            size_t sn = 0;
            sn += synth_num(seac + sn, 100);
            sn += synth_num(seac + sn, 200);
            seac[sn++] = 171;
            seac[sn++] = 171;
            seac[sn++] = 14;
            synth_u16(outline + at, 3);
            outline[at + 2] = 2;
            synth_u16(outline + at + 3, 1);
            synth_u16(outline + at + 5, 2);
            synth_u16(outline + at + 7, (uint32_t)csn + 2);
            synth_u16(outline + at + 9, (uint32_t)(csn + sn + 2));
            at += 11;
            outline[at++] = 14;
            memcpy(outline + at, cs, csn);
            at += csn;
            memcpy(outline + at, seac, sn);
            at += sn;
        } else
            at += synth_idx(outline + at, empty, cff2 ? 0 : 1, cs, csn, cff2);
    }
    data[0] = head;
    lengths[0] = sizeof head;
    data[1] = hhea;
    lengths[1] = sizeof hhea;
    data[2] = hmtx;
    lengths[2] = 4 * n;
    data[3] = maxp;
    lengths[3] = ttmode ? 32 : 6;
    data[4] = cmap;
    lengths[4] = sizeof cmap;
    data[5] = outline;
    lengths[5] = at;
    data[6] = loca;
    lengths[6] = 4 * (n + 1);
    synth_u32(out, ttmode ? 0x10000 : 0x4f54544f);
    synth_u16(out + 4, tables);
    at = 12 + 16 * tables;
    for (i = 0; i < tables; i++) {
        size_t entry = 12 + 16 * i;
        memcpy(out + entry, tags[i], 4);
        synth_u32(out + entry + 8, (uint32_t)at);
        synth_u32(out + entry + 12, (uint32_t)lengths[i]);
        memcpy(out + at, data[i], lengths[i]);
        at += lengths[i];
        at = (at + 3) & ~(size_t)3;
    }
    return at;
}
#endif
