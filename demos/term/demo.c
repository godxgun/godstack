/* Term behaviour. Ghostty/xterm where they disagree with raw VT500. */

#include "term.h"
#include "term.c"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

static int g_fails;

static void expect(int ok, const char *what);
static void title(const char *s);
static void feed(Term *t, const char *s);
static uint32_t cell_cp(Term *t, uint32_t x, uint32_t y);
static int cell_dirty(Term *t, uint32_t x, uint32_t y);
static uint32_t cell_bg(Term *t, uint32_t x, uint32_t y);
static void test_term_max(void);
static void test_putc_dirty(void);
static void test_lf_keeps_column(void);
static void test_lf_lnm(void);
static void test_decstbm_oneline(void);
static void test_cuu_top_margin(void);
static void test_cud_bot_margin(void);
static void test_cuu_above_region(void);
static void test_cud_below_region(void);
static void test_decaln(void);
static void test_ris(void);
static void test_sgr_256(void);
static void test_acs_g0(void);
static void test_resize_on_inplace(void);
static void test_resize_on_owned(void);

void
expect(int ok, const char *what)
{
    if (ok) {
        printf("  ok   %s\n", what);
        return;
    }
    printf("  FAIL %s\n", what);
    g_fails++;
}

void
title(const char *s)
{
    printf("%s\n", s);
}

void
feed(Term *t, const char *s)
{
    term_feed(t, s, strlen(s));
}

uint32_t
cell_cp(Term *t, uint32_t x, uint32_t y)
{
    TermScreen *s;

    s = term_screen(t);
    return s->cell_buffer[y * s->cols + x].codepoint;
}

int
cell_dirty(Term *t, uint32_t x, uint32_t y)
{
    TermScreen *s;

    s = term_screen(t);
    return s->cell_buffer[y * s->cols + x].is_dirty;
}

uint32_t
cell_bg(Term *t, uint32_t x, uint32_t y)
{
    TermScreen *s;

    s = term_screen(t);
    return s->cell_buffer[y * s->cols + x].bg >> 8;
}

void
test_term_max(void)
{
    title("TERM_MAX");
    expect(TERM_MAX(2, 5) == 5, "TERM_MAX(2,5)==5");
    expect(TERM_MAX(5, 2) == 5, "TERM_MAX(5,2)==5");
    expect(TERM_MIN(2, 5) == 2, "TERM_MIN(2,5)==2");
}

void
test_putc_dirty(void)
{
    Term t;

    title("putc sets is_dirty");
    term_init(&t, 8, 4, NULL);
    feed(&t, "A");
    expect(cell_cp(&t, 0, 0) == 'A', "wrote A");
    expect(cell_dirty(&t, 0, 0), "is_dirty after putc");
    term_destroy(&t);
}

void
test_lf_keeps_column(void)
{
    Term t;

    title("LF is IND (Ghostty/xterm, LNM off)");
    term_init(&t, 8, 4, NULL);
    feed(&t, "AB\nC");
    expect(t.cursor.x == 3, "cursor.x==3 after AB\\nC");
    expect(t.cursor.y == 1, "cursor.y==1");
    expect(cell_cp(&t, 2, 1) == 'C', "C at (2,1)");
    term_destroy(&t);
}

void
test_lf_lnm(void)
{
    Term t;

    title("LNM (CSI 20 h) makes LF also CR");
    term_init(&t, 8, 4, NULL);
    feed(&t, "\033[20hAB\nC");
    expect(t.cursor.x == 1, "C after CR+IND");
    expect(cell_cp(&t, 0, 1) == 'C', "C at (0,1)");
    term_destroy(&t);
}

void
test_decstbm_oneline(void)
{
    Term t;

    title("DECSTBM allows a 1-line region");
    term_init(&t, 80, 24, NULL);
    feed(&t, "\033[10;10r");
    expect(t.top == 9 && t.bot == 9, "CSI 10;10 r -> top=bot=9");
    term_destroy(&t);
}

void
test_cuu_top_margin(void)
{
    Term t;

    title("CUU inside region stops at top margin");
    term_init(&t, 80, 24, NULL);
    feed(&t, "\033[5;20r");
    feed(&t, "\033[6;1H");
    feed(&t, "\033[10A");
    expect(t.top == 4 && t.bot == 19, "region 5..20");
    expect(t.cursor.y == 4, "CUU 10 from row 6 stops at row 5");
    term_destroy(&t);
}

void
test_cud_bot_margin(void)
{
    Term t;

    title("CUD inside region stops at bottom margin");
    term_init(&t, 80, 24, NULL);
    feed(&t, "\033[5;20r");
    feed(&t, "\033[20;1H");
    feed(&t, "\033[10B");
    expect(t.cursor.y == 19, "CUD 10 from row 20 stays on row 20");
    term_destroy(&t);
}

void
test_cuu_above_region(void)
{
    Term t;

    title("CUU above region is free (Ghostty/xterm)");
    term_init(&t, 80, 24, NULL);
    feed(&t, "\033[10;20r");
    feed(&t, "\033[3;1H");
    feed(&t, "\033[10A");
    expect(t.cursor.y == 0, "from row 3, CUU 10 -> row 1");
    term_destroy(&t);
}

void
test_cud_below_region(void)
{
    Term t;

    title("CUD below region is free (Ghostty/xterm)");
    term_init(&t, 80, 24, NULL);
    feed(&t, "\033[5;10r");
    feed(&t, "\033[20;1H");
    feed(&t, "\033[10B");
    expect(t.cursor.y == 23, "from row 20, CUD 10 -> row 24");
    term_destroy(&t);
}

void
test_decaln(void)
{
    Term t;
    uint32_t i;
    uint32_t n;
    uint32_t es;
    TermScreen *s;

    title("ESC # 8 is DECALN");
    term_init(&t, 8, 4, NULL);
    feed(&t, "\033[3;3H*\0337");
    feed(&t, "\033[1;1HA");
    feed(&t, "\033#8");
    s = term_screen(&t);
    n = s->cols * s->rows;
    es = 0;
    for (i = 0; i < n; i++) {
        if (s->cell_buffer[i].codepoint == 'E')
            es++;
    }
    expect(es == n, "screen filled with E");
    expect(t.cursor.x == 0 && t.cursor.y == 0, "cursor home");
    term_destroy(&t);
}

void
test_ris(void)
{
    Term t;

    title("ESC c RIS resets grid and modes");
    term_init(&t, 8, 4, NULL);
    feed(&t, "HELLO");
    feed(&t, "\033[4h");
    feed(&t, "\033[?1049h");
    feed(&t, "\033c");
    expect(!(t.mode & TERM_MODE_ALTSCREEN), "left alt");
    expect(!(t.mode & TERM_MODE_INSERT), "IRM off");
    expect(t.cursor.x == 0 && t.cursor.y == 0, "cursor home");
    expect(t.screen.cell_buffer[0].codepoint == 0, "primary cleared");
    term_destroy(&t);
}

void
test_sgr_256(void)
{
    Term t;
    TermColors c;

    title("SGR 38/48;5;n share one 256 table (Ghostty)");
    memset(&c, 0, sizeof c);
    c.fg[1] = 0x111111;
    c.bg[1] = 0x222222;
    c.fg[7] = 0xaaaaaa;
    c.fg_default = 7;
    c.bg_default = 0;
    term_init(&t, 8, 4, &c);
    feed(&t, "\033[38;5;1mA\033[48;5;1mB");
    expect(cell_cp(&t, 0, 0) == 'A', "wrote A");
    expect(cell_bg(&t, 1, 0) == 0x111111, "48;5;1 == 38;5;1 == fg[1]");
    expect(cell_bg(&t, 1, 0) != 0x222222, "not the 8-color bg slot");
    term_destroy(&t);
}

void
test_acs_g0(void)
{
    Term t;

    title("ESC ( 0 maps ACS q to U+2500");
    term_init(&t, 8, 4, NULL);
    feed(&t, "\033(0q");
    expect(cell_cp(&t, 0, 0) == 0x2500, "q -> U+2500");
    term_destroy(&t);
}

void
test_resize_on_inplace(void)
{
    Term t;
    TermCell *scr;
    TermCell *alt;
    uint32_t cap;

    title("resize_on same pointers keeps cells");
    term_init(&t, 8, 4, NULL);
    feed(&t, "X");
    scr = t.screen.cell_buffer;
    alt = t.alt.cell_buffer;
    cap = t.screen.capacity;
    term_resize_on(&t, 8, 4, scr, alt, cap);
    expect(scr[0].codepoint == 'X', "in-place copy does not wipe src");
    t.cells_owned = 1;
    term_destroy(&t);
}

void
test_resize_on_owned(void)
{
    Term t;
    TermCell scr[16];
    TermCell alt[16];

    title("resize_on adopts external storage");
    term_init(&t, 4, 2, NULL);
    term_resize_on(&t, 4, 2, scr, alt, 16);
    expect(t.cells_owned == 0, "cells_owned=0 after resize_on");
    expect(t.screen.cell_buffer == scr, "screen is the given buffer");
    term_destroy(&t);
}

int
main(void)
{
    test_term_max();
    test_putc_dirty();
    test_lf_keeps_column();
    test_lf_lnm();
    test_decstbm_oneline();
    test_cuu_top_margin();
    test_cud_bot_margin();
    test_cuu_above_region();
    test_cud_below_region();
    test_decaln();
    test_ris();
    test_sgr_256();
    test_acs_g0();
    test_resize_on_inplace();
    test_resize_on_owned();
    if (g_fails) {
        printf("\n%d failed\n", g_fails);
        return 1;
    }
    printf("\nall ok\n");
    return 0;
}
