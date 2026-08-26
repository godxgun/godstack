/* ===========================================================================
 * TERM - Copyright @ Vasco Alves - See LICENSE at the end of file.
 *
 * Cell-grid terminal emulator.
 * Partial st-style state-machine / parser.
 * Feed bytes -> Read the grid -> Profit.
 *
 * PREFIX: TERM (macros)  Term (types)  term_ (functions)
 *
 * USAGE:
 *     #include "term.h"
 *     #include "term.c"
 *
 *     TermColors colors = {0};
 *     Term term;
 *     term_init(&term, 80, 24, &colors);
 *     term_feed(&term, bytes, len);
 *     TermScreen *s = term_screen(&term);
 *     term_destroy(&term);
 *
 * =========================================================================== */

#ifndef TERM_H
#define TERM_H

#define TERM_MAJOR 0
#define TERM_MINOR 3
#define TERM_PATCH 4

/* CHANGE LOG
 * 0.1.0 - @vasco - extract from vt: feed, grid, live CSI
 * 0.2.0 - @vasco - term_feed_ascii: clean CR/LF/printable, no state machine
 * 0.3.0 - @vasco - alt screen, scroll region, IL/DL/DCH/ECH, SGR 256/RGB, DSR/DA
 * 0.3.1 - @vasco - resize copies rows; alt switch resets scroll region
 * 0.3.2 - @vasco - CSI 18 t, DECRQM, XTVERSION; larger reply
 * 0.3.3 - @vasco - primary scroll hist; term_hist_line / term_hist_count
 * 0.3.4 - @vasco - DECSET mouse 1000/1002/1003/1006
 */

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(TERM_DEBUG)
#define TASSERT_N(_1, _2, N, ...) N
#define TASSERT(...) TASSERT_N(__VA_ARGS__, TASSERT2, TASSERT1)(__VA_ARGS__)
#define TASSERT1(a) assert(a)
#define TASSERT2(a, s) assert((a) && (s))
#else
#define TASSERT(...) ((void)0)
#endif

#define TERM_TODO \
    do { \
        fprintf(stderr, "TERM TODO: %s() in %s:%d\n", __func__, __FILE__, __LINE__); \
        abort(); \
    } while (0)

#define TERM_MIN(a, b) ((a) < (b) ? (a) : (b))
#define TERM_MAX(a, b) ((a) > (b) ? (b) : (a))
#define TERM_BETWEEN(x, a, b) (((unsigned)((x) - (a))) <= (unsigned)((b) - (a)))
#define TERM_DEFAULT(a, b) ((a) = (a) ? (a) : (b))

#define TERM_UTF_INVALID 0xFFFDu

#define TERM_MODE_WRAP      (1u << 0)
#define TERM_MODE_INSERT    (1u << 1)
#define TERM_MODE_ALTSCREEN (1u << 2)
#define TERM_MODE_CRLF      (1u << 3)
#define TERM_MODE_ECHO      (1u << 4)
#define TERM_MODE_PRINT     (1u << 5)
#define TERM_MODE_UTF8      (1u << 6)
#define TERM_MODE_HIDE      (1u << 7)
#define TERM_MODE_MOUSEBTN  (1u << 8)
#define TERM_MODE_MOUSEMOT  (1u << 9)
#define TERM_MODE_MOUSEMANY (1u << 10)
#define TERM_MODE_MOUSESGR  (1u << 11)
#define TERM_MODE_MOUSE     (TERM_MODE_MOUSEBTN | TERM_MODE_MOUSEMOT | TERM_MODE_MOUSEMANY)
#define TERM_WRAPNEXT       1u

#define TERM_ESC_START      1u
#define TERM_ESC_CSI        2u
#define TERM_ESC_STR        4u
#define TERM_ESC_ALTCHARSET 8u
#define TERM_ESC_STR_END    16u
#define TERM_ESC_TEST       32u
#define TERM_ESC_UTF8       64u

#define TERM_ATTR_NONE      0u
#define TERM_ATTR_BOLD      (1u << 0)
#define TERM_ATTR_FAINT     (1u << 1)
#define TERM_ATTR_ITALIC    (1u << 2)
#define TERM_ATTR_UNDERLINE (1u << 3)
#define TERM_ATTR_BLINK     (1u << 4)
#define TERM_ATTR_REVERSE   (1u << 5)
#define TERM_ATTR_INVISIBLE (1u << 6)
#define TERM_ATTR_STRUCK    (1u << 7)

#define TERM_ESC_ARG_SIZ 16
#define TERM_CSI_BUF_SIZ 256
#define TERM_HIST_MAX    1024

typedef struct TermColors {
    uint32_t fg[16];
    uint32_t bg[8];
    uint32_t fg_default;
    uint32_t bg_default;
} TermColors;

typedef struct TermCursor {
    uint32_t fg;
    uint32_t bg;
    uint32_t x;
    uint32_t y;
    uint8_t attr;
    uint8_t state;
} TermCursor;

typedef struct TermCell {
    uint32_t fg;
    uint32_t bg;
    uint32_t codepoint;
    bool is_dirty;
} TermCell;

typedef struct TermScreen {
    TermCell *cell_buffer;
    uint32_t cols;
    uint32_t rows;
    uint32_t capacity;
} TermScreen;

typedef struct TermCsi {
    char buf[TERM_CSI_BUF_SIZ];
    int32_t arg[TERM_ESC_ARG_SIZ];
    uint32_t len;
    int32_t narg;
    char priv;
    char mode[2];
} TermCsi;

typedef struct TermStr {
    uint32_t len;
    char type;
} TermStr;

typedef struct Term {
    TermScreen screen;
    TermScreen alt;
    TermCursor cursor;
    TermCursor saved;
    TermColors colors;
    TermCsi csi;
    TermStr str;
    uint32_t top;
    uint32_t bot;
    uint32_t mode;
    uint32_t state;
    uint32_t utf8_acc;
    uint32_t utf8_min;
    uint32_t last_ch;
    uint8_t utf8_rem;
    char reply[256];
    uint32_t reply_n;
    char str_buf[128];
    TermCell *hist;
    uint32_t hist_cap;
    uint32_t hist_n;
    uint32_t hist_i;
    uint32_t hist_cols;
} Term;

int  term_init(Term *t, uint32_t cols, uint32_t rows, const TermColors *colors);
void term_destroy(Term *t);
void term_resize(Term *t, uint32_t cols, uint32_t rows);
void term_feed(Term *t, const char *bytes, size_t len);
void term_feed_ascii(Term *t, const char *bytes, size_t len); /* no ESC, no UTF-8; CR/LF/putc only */
TermScreen *term_screen(Term *t);
uint32_t term_hist_count(const Term *t);
const TermCell *term_hist_line(const Term *t, uint32_t back);

#endif /* TERM_H */

/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2026 Vasco Alves
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
------------------------------------------------------------------------------
*/
