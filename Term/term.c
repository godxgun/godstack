#include "term.h"

#define TERM_GROW 2
#define TERM_BEL  0x07
#define TERM_BS   0x08
#define TERM_ESC  0x1B
#define TERM_CAN  0x18
#define TERM_SUB  0x1A
#define TERM_DEL  0x7F

#define TERM_IS_C0(c)  (TERM_BETWEEN((c), 0, 0x1f) || (c) == TERM_DEL)
#define TERM_IS_C1(c)  TERM_BETWEEN((c), 0x80, 0x9f)

static void term_screen_init(TermScreen *s, uint32_t cols, uint32_t rows);
static void term_screen_free(TermScreen *s);
static int  term_screen_grow(TermScreen *s, uint32_t cols, uint32_t rows);
static TermScreen *term_live(Term *t);
static void term_next_line(Term *t);
static void term_cursor_forward(Term *t);
static int  term_codepoint_width(uint32_t c);
static int  term_utf8_consume(Term *t, unsigned char ch, uint32_t *out);
static void term_putc(Term *t, uint32_t c);
static void term_handle_c0(Term *t, unsigned char code);
static void term_handle_c1(Term *t, unsigned char code);
static int  term_handle_esc(Term *t, unsigned char ascii);
static void term_char_feed(Term *t, unsigned char ch);
static void term_cursor_sgr(Term *t, const int32_t *attr, int32_t n);
static int  term_parse_csi(Term *t);
static void term_handle_csi(Term *t);
static void term_handle_str(Term *t);
static TermCell *term_cell_xy(Term *t, uint32_t x, uint32_t y);
static TermCell *term_cell_cursor(Term *t);
static void term_insert_blank(Term *t, uint32_t n);
static void term_clear_region(Term *t, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1);
static void term_move_to(Term *t, uint32_t x, uint32_t y);
static void term_move_abs(Term *t, uint32_t x, uint32_t y);
static void term_colors_default(TermColors *c);

static const TermColors term_colors_stock = {
    .fg = {
        0x1d2021, 0xea6962, 0xa9b665, 0xd8a657,
        0x7daea3, 0xd3869b, 0x89b482, 0xd4be98,
        0x928374, 0xef938e, 0xbbc585, 0xe1bb7e,
        0x9dc2ba, 0xe1acbb, 0xa7c7a2, 0xe2d3ba,
    },
    .bg = {
        0x1D2021, 0x800000, 0x008000, 0x808000,
        0x000080, 0x800080, 0x008080, 0xC8C8C8,
    },
    .fg_default = 7,
    .bg_default = 0,
};

static void
term_colors_default(TermColors *c)
{
    *c = term_colors_stock;
}

static void
term_screen_init(TermScreen *s, uint32_t cols, uint32_t rows)
{
    memset(s, 0, sizeof *s);
    s->cell_buffer = calloc((size_t)rows * cols, sizeof *s->cell_buffer);
    s->cols = cols;
    s->rows = rows;
    s->capacity = cols * rows;
}

static void
term_screen_free(TermScreen *s)
{
    if (s->cell_buffer)
        free(s->cell_buffer);
    memset(s, 0, sizeof *s);
}

static int
term_screen_grow(TermScreen *s, uint32_t cols, uint32_t rows)
{
    size_t old_cap;
    size_t need;
    size_t new_cap;
    TermCell *cells;

    if (cols * rows <= s->capacity) {
        s->cols = cols;
        s->rows = rows;
        return 1;
    }

    old_cap = s->capacity;
    need = (size_t)cols * (size_t)rows;
    new_cap = old_cap * TERM_GROW;
    if (new_cap < need)
        new_cap = need;

    cells = realloc(s->cell_buffer, new_cap * sizeof *s->cell_buffer);
    if (cells) {
        s->cell_buffer = cells;
        memset(s->cell_buffer + old_cap, 0, (new_cap - old_cap) * sizeof *s->cell_buffer);
        s->cols = cols;
        s->rows = rows;
        s->capacity = (uint32_t)new_cap;
        return 1;
    }

    cells = calloc(new_cap, sizeof *s->cell_buffer);
    if (!cells)
        return 0;
    memcpy(cells, s->cell_buffer, old_cap * sizeof *s->cell_buffer);
    free(s->cell_buffer);
    s->cell_buffer = cells;
    s->cols = cols;
    s->rows = rows;
    s->capacity = (uint32_t)new_cap;
    return 1;
}

static TermScreen *
term_live(Term *t)
{
    return (t->mode & TERM_MODE_ALTSCREEN) ? &t->alt : &t->screen;
}

TermScreen *
term_screen(Term *t)
{
    TASSERT(t, "Invalid term.");
    if (!t)
        return NULL;
    return term_live(t);
}

int
term_init(Term *t, uint32_t cols, uint32_t rows, const TermColors *colors)
{
    TASSERT(t, "Invalid term.");
    if (!t || !cols || !rows)
        return 0;

    memset(t, 0, sizeof *t);
    if (colors)
        t->colors = *colors;
    else
        term_colors_default(&t->colors);

    term_screen_init(&t->screen, cols, rows);
    term_screen_init(&t->alt, cols, rows);
    if (!t->screen.cell_buffer || !t->alt.cell_buffer) {
        term_destroy(t);
        return 0;
    }

    t->cursor.fg = t->colors.fg[t->colors.fg_default < 16 ? t->colors.fg_default : 7];
    t->cursor.bg = t->colors.bg[t->colors.bg_default < 8 ? t->colors.bg_default : 0];
    t->mode = TERM_MODE_UTF8;
    return 1;
}

void
term_destroy(Term *t)
{
    if (!t)
        return;
    term_screen_free(&t->screen);
    term_screen_free(&t->alt);
    memset(t, 0, sizeof *t);
}

void
term_resize(Term *t, uint32_t cols, uint32_t rows)
{
    TASSERT(t, "Invalid term.");
    if (!t)
        return;
    term_screen_grow(&t->screen, cols, rows);
    term_screen_grow(&t->alt, cols, rows);
    if (cols)
        t->cursor.x = TERM_MIN(t->cursor.x, cols - 1);
    if (rows)
        t->cursor.y = TERM_MIN(t->cursor.y, rows - 1);
}

static void
term_next_line(Term *t)
{
    TermScreen *s;

    s = term_live(t);
    t->bot++;
    t->cursor.x = 0;
    if (t->cursor.y == s->rows - 1) {
        memmove(s->cell_buffer, s->cell_buffer + s->cols,
            s->cols * (s->rows - 1) * sizeof *s->cell_buffer);
        memset(s->cell_buffer + s->cols * (s->rows - 1), 0,
            s->cols * sizeof *s->cell_buffer);
    } else {
        t->cursor.y = t->cursor.y + 1;
    }
}

static void
term_cursor_forward(Term *t)
{
    t->cursor.x++;
    if (t->cursor.x == t->screen.cols)
        term_next_line(t);
}

static int
term_codepoint_width(uint32_t c)
{
    if (c == 0)
        return 0;
    if (c < 0x20 || c == 0x7F)
        return -1;
    if ((c >= 0x0300 && c <= 0x036F) ||
        (c >= 0x1AB0 && c <= 0x1AFF) ||
        (c >= 0x1DC0 && c <= 0x1DFF) ||
        (c >= 0x20D0 && c <= 0x20FF) ||
        (c >= 0xFE20 && c <= 0xFE2F))
        return 0;
    if ((c >= 0x1100 && c <= 0x115F) ||
        (c >= 0x2329 && c <= 0x232A) ||
        (c >= 0x2E80 && c <= 0xA4CF && c != 0x303F) ||
        (c >= 0xAC00 && c <= 0xD7A3) ||
        (c >= 0xF900 && c <= 0xFAFF) ||
        (c >= 0xFE10 && c <= 0xFE19) ||
        (c >= 0xFE30 && c <= 0xFE6F) ||
        (c >= 0xFF00 && c <= 0xFF60) ||
        (c >= 0xFFE0 && c <= 0xFFE6) ||
        (c >= 0x1F300 && c <= 0x1FAFF))
        return 2;
    return 1;
}

static int
term_utf8_consume(Term *t, unsigned char ch, uint32_t *out)
{
    if (t->utf8_rem) {
        if ((ch & 0xC0) == 0x80) {
            t->utf8_acc = (t->utf8_acc << 6) | (uint32_t)(ch & 0x3F);
            t->utf8_rem--;
            if (t->utf8_rem)
                return 0;
            if (t->utf8_acc > 0x10FFFF ||
                (t->utf8_acc >= 0xD800 && t->utf8_acc <= 0xDFFF) ||
                t->utf8_acc < t->utf8_min) {
                *out = TERM_UTF_INVALID;
                return 1;
            }
            *out = t->utf8_acc;
            return 1;
        }
        t->utf8_rem = 0;
        *out = TERM_UTF_INVALID;
        return 2;
    }

    if (ch < 0x80) {
        *out = ch;
        return 1;
    }
    if ((ch & 0xE0) == 0xC0) {
        t->utf8_acc = ch & 0x1F;
        t->utf8_rem = 1;
        t->utf8_min = 0x80;
        return 0;
    }
    if ((ch & 0xF0) == 0xE0) {
        t->utf8_acc = ch & 0x0F;
        t->utf8_rem = 2;
        t->utf8_min = 0x800;
        return 0;
    }
    if ((ch & 0xF8) == 0xF0) {
        t->utf8_acc = ch & 0x07;
        t->utf8_rem = 3;
        t->utf8_min = 0x10000;
        return 0;
    }
    *out = TERM_UTF_INVALID;
    return 1;
}

static void
term_putc(Term *t, uint32_t c)
{
    int width;
    uint32_t idx;
    uint32_t fg;
    uint32_t bg;

    width = term_codepoint_width(c);
    if (width <= 0)
        return;

    if (t->cursor.x + (uint32_t)width > t->screen.cols)
        term_next_line(t);

    idx = t->cursor.x + t->cursor.y * t->screen.cols;
    TASSERT(idx < t->screen.capacity);
    if (idx >= t->screen.capacity)
        return;

    fg = (t->cursor.fg << 8) | t->cursor.attr;
    bg = t->cursor.bg << 8;
    t->screen.cell_buffer[idx].codepoint = c;
    t->screen.cell_buffer[idx].fg = fg;
    t->screen.cell_buffer[idx].bg = bg;
    term_cursor_forward(t);

    if (width == 2 && t->cursor.x != 0) {
        idx = t->cursor.x + t->cursor.y * t->screen.cols;
        t->screen.cell_buffer[idx].codepoint = 0;
        t->screen.cell_buffer[idx].fg = fg;
        t->screen.cell_buffer[idx].bg = bg;
        term_cursor_forward(t);
    }
}

static void
term_move_to(Term *t, uint32_t x, uint32_t y)
{
    t->cursor.x = TERM_MIN(x, t->screen.cols ? t->screen.cols - 1 : 0);
    t->cursor.y = TERM_MIN(y, t->screen.rows ? t->screen.rows - 1 : 0);
}

static void
term_move_abs(Term *t, uint32_t x, uint32_t y)
{
    term_move_to(t, x, y);
}

static TermCell *
term_cell_xy(Term *t, uint32_t x, uint32_t y)
{
    TermScreen *s;

    s = term_live(t);
    x = TERM_MIN(x, s->cols ? s->cols - 1 : 0);
    y = TERM_MIN(y, s->rows ? s->rows - 1 : 0);
    return s->cell_buffer + (y * s->cols) + x;
}

static TermCell *
term_cell_cursor(Term *t)
{
    TermScreen *s;

    s = term_live(t);
    return s->cell_buffer + (t->cursor.y * s->cols) + t->cursor.x;
}

static void
term_insert_blank(Term *t, uint32_t n)
{
    uint32_t new_x;
    TermCell *left;
    TermCell *right;
    TermCell *end;

    if (n == 0)
        return;

    new_x = TERM_MIN(t->cursor.x + n, t->screen.cols ? t->screen.cols - 1 : 0);
    left = term_cell_cursor(t);
    right = left + n;
    end = term_cell_xy(t, new_x, t->cursor.y);

    for (; right <= end; left++, right++) {
        left->is_dirty = true;
        *right = *left;
        left->codepoint = (uint32_t)' ';
    }
    for (; left <= end; left++) {
        left->is_dirty = true;
        left->codepoint = (uint32_t)' ';
    }
}

static void
term_clear_region(Term *t, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1)
{
    TermScreen *s;
    uint32_t y;
    uint32_t x;

    s = &t->screen;
    if (s->cols == 0 || s->rows == 0)
        return;
    if (x0 > x1) { uint32_t tmp = x0; x0 = x1; x1 = tmp; }
    if (y0 > y1) { uint32_t tmp = y0; y0 = y1; y1 = tmp; }
    if (x0 >= s->cols || y0 >= s->rows)
        return;
    x1 = TERM_MIN(x1, s->cols - 1);
    y1 = TERM_MIN(y1, s->rows - 1);

    for (y = y0; y <= y1; y++) {
        for (x = x0; x <= x1; x++) {
            TermCell *c = &s->cell_buffer[y * s->cols + x];
            c->codepoint = 0;
            c->fg = (t->cursor.fg << 8) | t->cursor.attr;
            c->bg = t->cursor.bg << 8;
            c->is_dirty = true;
        }
    }
}

static void
term_handle_c0(Term *t, unsigned char code)
{
    switch (code) {
    case '\t':
        return;
    case '\b':
        if (t->cursor.x > 0)
            t->cursor.x--;
        return;
    case '\r':
        term_move_to(t, 0, t->cursor.y);
        return;
    case '\f':
    case '\v':
    case '\n':
        term_next_line(t);
        return;
    case TERM_BEL:
        if (t->state & TERM_ESC_STR_END)
            term_handle_str(t);
        return;
    case TERM_ESC:
        t->state &= ~(TERM_ESC_CSI | TERM_ESC_ALTCHARSET | TERM_ESC_TEST | TERM_ESC_STR | TERM_ESC_STR_END);
        t->state |= TERM_ESC_START;
        memset(&t->csi, 0, sizeof t->csi);
        return;
    case '\016':
    case '\017':
        return;
    case TERM_SUB:
    case TERM_CAN:
        break;
    default:
        break;
    }
    t->state &= ~(TERM_ESC_STR_END | TERM_ESC_STR);
}

static void
term_handle_c1(Term *t, unsigned char code)
{
    switch (code) {
    case 0x9f:
        return;
    default:
        break;
    }
    t->state &= ~(TERM_ESC_STR_END | TERM_ESC_STR);
}

static int
term_handle_esc(Term *t, unsigned char ascii)
{
    switch (ascii) {
    case '#':
        t->state |= TERM_ESC_TEST;
        return 0;
    case '%':
        t->state |= TERM_ESC_UTF8;
        return 0;
    case 'P':
    case '_':
    case '^':
    case ']':
    case 'k':
        return 0;
    case 'n':
    case 'o':
        break;
    case '(':
    case ')':
    case '*':
    case '+':
        return 0;
    case 'D':
        break;
    case 'E':
        break;
    case 'H':
        break;
    case 'M':
        if (t->cursor.y == t->top) {
            /* stub scroll down */
        } else {
            term_move_to(t, t->cursor.x, t->cursor.y - 1);
        }
        break;
    case 'Z':
        break;
    case 'c':
        break;
    case '=':
        break;
    case '>':
        break;
    case '7':
        break;
    case '8':
        break;
    case '\\':
        if (t->state & TERM_ESC_STR_END)
            term_handle_str(t);
        break;
    default:
        break;
    }
    return 1;
}

static void
term_handle_str(Term *t)
{
    (void)t;
}

static void
term_cursor_sgr(Term *t, const int32_t *attr, int32_t n)
{
    int32_t i;
    uint32_t def_fg;
    uint32_t def_bg;

    def_fg = t->colors.fg[t->colors.fg_default < 16 ? t->colors.fg_default : 7];
    def_bg = t->colors.bg[t->colors.bg_default < 8 ? t->colors.bg_default : 0];

    for (i = 0; i < n; i++) {
        switch (attr[i]) {
        case 0:
            t->cursor.attr = TERM_ATTR_NONE;
            t->cursor.fg = def_fg;
            t->cursor.bg = def_bg;
            break;
        case 1:
            t->cursor.attr |= TERM_ATTR_BOLD;
            break;
        case 2:
            t->cursor.attr |= TERM_ATTR_FAINT;
            break;
        case 3:
            t->cursor.attr |= TERM_ATTR_ITALIC;
            break;
        case 4:
            t->cursor.attr |= TERM_ATTR_UNDERLINE;
            break;
        case 5:
        case 6:
            t->cursor.attr |= TERM_ATTR_BLINK;
            break;
        case 7:
            t->cursor.attr |= TERM_ATTR_REVERSE;
            break;
        case 8:
            t->cursor.attr |= TERM_ATTR_INVISIBLE;
            break;
        case 9:
            t->cursor.attr |= TERM_ATTR_STRUCK;
            break;
        case 22:
            t->cursor.fg &= ~(TERM_ATTR_BOLD | TERM_ATTR_FAINT);
            break;
        case 23:
            t->cursor.fg &= ~TERM_ATTR_ITALIC;
            break;
        case 24:
            t->cursor.fg &= ~TERM_ATTR_UNDERLINE;
            break;
        case 25:
            t->cursor.fg &= ~TERM_ATTR_BLINK;
            break;
        case 27:
            t->cursor.fg &= ~TERM_ATTR_REVERSE;
            break;
        case 28:
            t->cursor.fg &= ~TERM_ATTR_INVISIBLE;
            break;
        case 29:
            t->cursor.fg &= ~TERM_ATTR_STRUCK;
            break;
        case 38:
            break;
        case 39:
            t->cursor.fg = def_fg;
            break;
        case 48:
            break;
        case 49:
            t->cursor.bg = def_bg;
            break;
        default:
            if (TERM_BETWEEN(attr[i], 30, 37))
                t->cursor.fg = t->colors.fg[attr[i] - 30];
            else if (TERM_BETWEEN(attr[i], 40, 47))
                t->cursor.bg = t->colors.bg[attr[i] - 40];
            else if (TERM_BETWEEN(attr[i], 90, 97))
                t->cursor.fg = t->colors.fg[attr[i] - 90 + 8];
            else if (TERM_BETWEEN(attr[i], 100, 107))
                t->cursor.bg = t->colors.bg[attr[i] - 100];
            break;
        }
    }
}

static int
term_parse_csi(Term *t)
{
    char *ptr;
    char *end;
    char *np;
    int32_t value;

    if (t->csi.len == 0)
        return 0;
    ptr = t->csi.buf;
    end = ptr + t->csi.len;
    t->csi.narg = 0;

    if (*ptr == '?') {
        t->csi.priv = 1;
        ptr++;
    }

    for (; ptr < end; ptr++) {
        np = NULL;
        value = (int32_t)strtol(ptr, &np, 10);
        if (np == ptr)
            value = 0;
        t->csi.arg[t->csi.narg++] = value;
        ptr = np;
        if (*ptr != ';' || t->csi.narg == TERM_ESC_ARG_SIZ)
            break;
    }

    if (ptr >= end || *ptr < 0x40 || *ptr > 0x7E) {
        memset(&t->csi, 0, sizeof t->csi);
        return 0;
    }

    t->csi.mode[0] = *ptr;
    t->csi.mode[1] = (ptr + 1 < end) ? *(ptr + 1) : '\0';
    return 1;
}

static void
term_handle_csi(Term *t)
{
    switch (t->csi.mode[0]) {
    default:
        memset(&t->csi, 0, sizeof t->csi);
        break;
    case '@':
        TERM_DEFAULT(t->csi.arg[0], 1);
        term_insert_blank(t, (uint32_t)t->csi.arg[0]);
        break;
    case 'A':
        TERM_DEFAULT(t->csi.arg[0], 1);
        term_move_to(t, t->cursor.x,
            t->cursor.y > (uint32_t)t->csi.arg[0] ? t->cursor.y - (uint32_t)t->csi.arg[0] : 0);
        break;
    case 'B':
    case 'e':
        TERM_DEFAULT(t->csi.arg[0], 1);
        term_move_to(t, t->cursor.x, t->cursor.y + (uint32_t)t->csi.arg[0]);
        break;
    case 'c':
        break;
    case 'b':
        TERM_DEFAULT(t->csi.arg[0], 1);
        if (t->last_ch > 0) {
            while (t->csi.arg[0]-- > 0)
                term_putc(t, t->last_ch);
        }
        break;
    case 'C':
    case 'a':
        TERM_DEFAULT(t->csi.arg[0], 1);
        term_move_to(t, t->cursor.x + (uint32_t)t->csi.arg[0], t->cursor.y);
        break;
    case 'D':
        TERM_DEFAULT(t->csi.arg[0], 1);
        term_move_to(t,
            t->cursor.x > (uint32_t)t->csi.arg[0] ? t->cursor.x - (uint32_t)t->csi.arg[0] : 0,
            t->cursor.y);
        break;
    case 'E':
        TERM_DEFAULT(t->csi.arg[0], 1);
        term_move_to(t, 0, t->cursor.y + (uint32_t)t->csi.arg[0]);
        break;
    case 'F':
        TERM_DEFAULT(t->csi.arg[0], 1);
        term_move_to(t, 0,
            t->cursor.y > (uint32_t)t->csi.arg[0] ? t->cursor.y - (uint32_t)t->csi.arg[0] : 0);
        break;
    case 'g':
        break;
    case 'G':
    case '`':
        TERM_DEFAULT(t->csi.arg[0], 1);
        term_move_to(t, (uint32_t)t->csi.arg[0] - 1, t->cursor.y);
        break;
    case 'H':
    case 'f':
        TERM_DEFAULT(t->csi.arg[0], 1);
        TERM_DEFAULT(t->csi.arg[1], 1);
        term_move_abs(t, (uint32_t)t->csi.arg[1] - 1, (uint32_t)t->csi.arg[0] - 1);
        break;
    case 'I':
        break;
    case 'J':
        switch (t->csi.arg[0]) {
        case 0:
            term_clear_region(t, t->cursor.x, t->cursor.y, t->screen.cols - 1, t->cursor.y);
            if (t->cursor.y + 1 < t->screen.rows)
                term_clear_region(t, 0, t->cursor.y + 1, t->screen.cols - 1, t->screen.rows - 1);
            break;
        case 1:
            if (t->cursor.y > 0)
                term_clear_region(t, 0, 0, t->screen.cols - 1, t->cursor.y - 1);
            term_clear_region(t, 0, t->cursor.y, t->cursor.x, t->cursor.y);
            break;
        default:
            term_clear_region(t, 0, 0, t->screen.cols - 1, t->screen.rows - 1);
            break;
        }
        break;
    case 'K':
        switch (t->csi.arg[0]) {
        case 1:
            term_clear_region(t, 0, t->cursor.y, t->cursor.x, t->cursor.y);
            break;
        case 2:
            term_clear_region(t, 0, t->cursor.y, t->screen.cols - 1, t->cursor.y);
            break;
        default:
            term_clear_region(t, t->cursor.x, t->cursor.y, t->screen.cols - 1, t->cursor.y);
            break;
        }
        break;
    case 'S':
    case 'T':
    case 'L':
    case 'l':
    case 'M':
    case 'X':
    case 'P':
    case 'Z':
        break;
    case 'd':
        term_move_abs(t, t->cursor.x, (uint32_t)t->csi.arg[0] - 1);
        break;
    case 'h':
        break;
    case 'm':
        term_cursor_sgr(t, t->csi.arg, t->csi.narg);
        break;
    case 'n':
    case 'r':
    case 's':
    case 'u':
    case ' ':
        break;
    }
}

static void
term_char_feed(Term *t, unsigned char ch)
{
    uint32_t cp;
    int utf_st;
    int control;

    if (t->state & TERM_ESC_STR) {
        if (ch == TERM_BEL || ch == TERM_CAN || ch == TERM_SUB || ch == TERM_ESC || TERM_IS_C1(ch)) {
            t->state &= ~(TERM_ESC_START | TERM_ESC_STR);
            t->state |= TERM_ESC_STR_END;
            t->utf8_rem = 0;
            if (TERM_IS_C1(ch))
                term_handle_c1(t, ch);
            else
                term_handle_c0(t, ch);
            if (!t->state)
                t->last_ch = 0;
            return;
        }
        t->str.len++;
        return;
    }

    for (;;) {
        utf_st = term_utf8_consume(t, ch, &cp);
        if (utf_st == 0)
            return;
        if (utf_st == 2) {
            t->last_ch = 0;
            term_putc(t, TERM_UTF_INVALID);
            continue;
        }
        break;
    }

    control = (cp < 0x20 || cp == 0x7F || (cp >= 0x80 && cp <= 0x9F));
    if (control) {
        t->utf8_rem = 0;
        if (cp >= 0x80)
            term_handle_c1(t, (unsigned char)cp);
        else
            term_handle_c0(t, (unsigned char)cp);
        if (!t->state)
            t->last_ch = 0;
        return;
    }

    if (t->state & TERM_ESC_START) {
        if (cp > 0x7E) {
            t->state = 0;
            t->last_ch = cp;
            term_putc(t, cp);
            return;
        }
        ch = (unsigned char)cp;
        if (t->state & TERM_ESC_CSI) {
            if (t->csi.len < TERM_CSI_BUF_SIZ - 1)
                t->csi.buf[t->csi.len++] = (char)ch;
            if (TERM_BETWEEN(ch, 0x40, 0x7E) || t->csi.len > TERM_DEL * 10) {
                if (term_parse_csi(t))
                    term_handle_csi(t);
                t->state = 0;
                memset(&t->csi, 0, sizeof t->csi);
            }
            return;
        }
        switch (ch) {
        case '[':
            t->state |= TERM_ESC_CSI;
            memset(&t->csi, 0, sizeof t->csi);
            return;
        case 'P':
        case '_':
        case '^':
        case ']':
        case 'k':
            t->state |= TERM_ESC_STR;
            memset(&t->str, 0, sizeof t->str);
            t->str.type = (char)ch;
            return;
        }
        if (!term_handle_esc(t, ch))
            return;
        t->state = 0;
        return;
    }

    t->last_ch = cp;
    term_putc(t, cp);
}

void
term_feed(Term *t, const char *bytes, size_t len)
{
    size_t i;

    TASSERT(t, "Invalid term.");
    if (!t || !bytes || !len)
        return;

    for (i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)bytes[i];

        if (!(t->state & TERM_ESC_START) && !t->utf8_rem) {
            if (ch == '\n') {
                term_next_line(t);
                continue;
            }
            if (ch == '\r') {
                term_move_to(t, 0, t->cursor.y);
                continue;
            }
            if (ch >= 0x20 && ch < 0x7F) {
                t->last_ch = ch;
                term_putc(t, ch);
                continue;
            }
        }
        term_char_feed(t, ch);
    }
}

void
term_feed_ascii(Term *t, const char *bytes, size_t len)
{
    size_t i;

    TASSERT(t, "Invalid term.");
    if (!t || !bytes || !len)
        return;
    if ((t->state & TERM_ESC_START) || t->utf8_rem) {
        term_feed(t, bytes, len);
        return;
    }

    for (i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)bytes[i];

        if (ch == '\n')
            term_next_line(t);
        else if (ch == '\r')
            term_move_to(t, 0, t->cursor.y);
        else {
            t->last_ch = ch;
            term_putc(t, ch);
        }
    }
}
