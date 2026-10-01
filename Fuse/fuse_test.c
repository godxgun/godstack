#ifndef FUSE_DEBUG
#define FUSE_DEBUG
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fuse.h"
#include "fuse.c"

static int g_fails;

// NOTE(vasco): helper because bad things can happen to good people
FuseError get_error(FuseCanvas c) {
    return (c) ? c->error : FUSE_ERR_BUF_TOO_SMALL;
}

static void expect(int ok, const char *what);
static int nearly(float a, float b, float eps);
static FuseCmd *find_rect(FuseCmd *cmds, size_t n, uint32_t color);
static void test_memory(void);
static void test_two_canvases(void);
static void test_screen_origin(void);
static void test_sidebar(void);
static void test_insert_does_not_steal(void);
static void test_unbalanced(void);
static void test_percent(void);
static void test_rect_text(void);
static void test_scroll(void);
static void test_debug(void);
static void test_columns(void);
static void test_sizing(void);
static void test_textbox(void);
static void test_radial(void);
static FuseCmd *find_text(FuseCmd *cmds, size_t n);
static int has_rect_at(FuseCmd *cmds, size_t n, float x, float y, float w, float h, uint32_t color);
static int has_panel(FuseCmd *cmds, size_t n, float width, float height);

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

int
nearly(float a, float b, float eps)
{
    float d;
    d = a - b;
    if (d < 0.0f)
        d = -d;
    return d <= eps;
}

FuseCmd *
find_rect(FuseCmd *cmds, size_t n, uint32_t color)
{
    size_t i;
    if (!cmds)
        return NULL;
    for (i = 0; i < n; i++) {
        if (cmds[i].type == FUSE_CMD_RECT && cmds[i].rect.color == color)
            return &cmds[i];
    }
    return NULL;
}

int
has_rect_at(FuseCmd *cmds, size_t n, float x, float y, float w, float h, uint32_t color)
{
    size_t i;

    if (!cmds)
        return 0;
    for (i = 0; i < n; i++) {
        if (cmds[i].type != FUSE_CMD_RECT)
            continue;
        if (cmds[i].rect.color != color)
            continue;
        if (!nearly(cmds[i].rect.x, x, 0.01f) || !nearly(cmds[i].rect.y, y, 0.01f))
            continue;
        if (!nearly(cmds[i].rect.w, w, 0.01f) || !nearly(cmds[i].rect.h, h, 0.01f))
            continue;
        return 1;
    }
    return 0;
}

FuseCmd *
find_text(FuseCmd *cmds, size_t n)
{
    size_t i;

    if (!cmds)
        return NULL;
    for (i = 0; i < n; i++) {
        if (cmds[i].type == FUSE_CMD_TEXT)
            return &cmds[i];
    }
    return NULL;
}

int
has_panel(FuseCmd *cmds, size_t n, float width, float height)
{
    size_t i;

    if (!cmds)
        return 0;
    for (i = 0; i < n; i++) {
        float x, y, w, h;

        if (cmds[i].type != FUSE_CMD_RECT)
            continue;
        x = cmds[i].rect.x;
        y = cmds[i].rect.y;
        w = cmds[i].rect.w;
        h = cmds[i].rect.h;
        if (nearly(y, 0.0f, 0.01f) && nearly(h, height, 0.01f) &&
            nearly(x + w, width, 0.01f) && w > 40.0f && w < width)
            return 1;
    }
    return 0;
}

void
test_memory(void)
{
    unsigned char small[8];
    unsigned char buf[1 << 16];
    size_t need;
    FuseCanvas c;
    printf("memory\n");
    expect(fuse_canvas_create(NULL, 1024) == NULL, "create null buf");
    expect(fuse_canvas_create(small, sizeof small) == NULL, "create too small");
    expect(get_error(NULL) == FUSE_ERR_BUF_TOO_SMALL, "error null canvas");
    need = fuse_canvas_memory(64);
    expect(need > 0 && need <= sizeof buf, "memory(64) fits");
    /* Triangle commands add one float to each command-buffer slot. */
    expect(fuse_canvas_memory(8192) <= (2u << 20) + (128u << 10), "memory(8192) fits 2MiB + 128KiB");
    c = fuse_canvas_create(buf, need);
    expect(c != NULL, "create exact");
    expect(get_error(c) == FUSE_ERR_OK, "create ok");
}

void
test_two_canvases(void)
{
    unsigned char buf_a[1 << 16];
    unsigned char buf_b[1 << 16];
    FuseCanvas a, b;
    size_t na, nb;
    FuseCmd *cmds;
    int clicked_a, clicked_b;
    printf("two canvases\n");
    a = fuse_canvas_create(buf_a, sizeof buf_a);
    b = fuse_canvas_create(buf_b, sizeof buf_b);
    expect(a && b && a != b, "isolated handles");

    fuse_canvas_resize(a, 200, 200);
    fuse_canvas_resize(b, 200, 200);
    fuse_canvas_pointer(a, FUSE_POINTER_RELEASED, 10, 10);
    fuse_canvas_pointer(b, FUSE_POINTER_RELEASED, 10, 10);

    fuse_canvas_clear(a);
    fuse_id(a, "ok");
    fuse_button(a, 0, 0, 20, 20, 0x11111111, 0x22222222);
    cmds = fuse_canvas_draw(a, &na);
    expect(cmds && na > 0, "a frame 1");

    fuse_canvas_clear(b);
    fuse_id(b, "ok");
    fuse_button(b, 0, 0, 20, 20, 0x11111111, 0x22222222);
    cmds = fuse_canvas_draw(b, &nb);
    expect(cmds && nb > 0, "b frame 1");

    fuse_canvas_pointer(a, FUSE_POINTER_PRESSED, 10, 10);
    fuse_canvas_pointer(a, FUSE_POINTER_RELEASED, 10, 10);
    fuse_canvas_clear(a);
    fuse_id(a, "ok");
    clicked_a = fuse_button(a, 0, 0, 20, 20, 0x11111111, 0x22222222);
    fuse_canvas_draw(a, &na);

    fuse_canvas_clear(b);
    fuse_id(b, "ok");
    clicked_b = fuse_button(b, 0, 0, 20, 20, 0x11111111, 0x22222222);
    fuse_canvas_draw(b, &nb);

    expect(clicked_a, "click on a");
    expect(!clicked_b, "no click on b");
}

void
test_screen_origin(void)
{
    unsigned char buf[1 << 16];
    FuseCanvas c;
    size_t n;
    FuseCmd *cmds, *r;
    printf("screen origin\n");
    c = fuse_canvas_create(buf, sizeof buf);
    fuse_canvas_resize(c, 800, 600);
    fuse_canvas_pointer(c, FUSE_POINTER_RELEASED, 0, 0);
    fuse_canvas_clear(c);
    fuse_div_begin(c, 200, 50, 400, 300, NULL); {
        fuse_button(c, 0, 0, 10, 10, 0xAABBCCDD, 0xAABBCCDD);
    } fuse_div_end(c);
    cmds = fuse_canvas_draw(c, &n);
    expect(cmds != NULL, "draw");
    r = find_rect(cmds, n, 0xAABBCCDD);
    expect(r != NULL, "button rect");
    expect(r && nearly(r->rect.x, 200, 0.01f) && nearly(r->rect.y, 50, 0.01f), "local 0,0 is div origin");
}

void
test_sidebar(void)
{
    unsigned char buf[1 << 16];
    FuseCanvas c;
    static FuseClass sidebar;
    static FuseClass row;
    float nob;
    size_t n, i;
    FuseCmd *cmds;
    int saw_clip, saw_button, saw_track, clicked;
    printf("sidebar\n");
    memset(&sidebar, 0, sizeof sidebar);
    memset(&row, 0, sizeof row);
    sidebar.color = 0xFF111111;
    sidebar.direction = FUSE_DIRECTION_COLUMN;
    sidebar.gap = 8;
    row.direction = FUSE_DIRECTION_ROW;
    row.gap = 8;
    nob = 0.25f;
    c = fuse_canvas_create(buf, sizeof buf);
    fuse_canvas_resize(c, 800, 600);
    fuse_canvas_pointer(c, FUSE_POINTER_RELEASED, 40, 20);

    fuse_canvas_clear(c);
    fuse_div_begin(c, 0, 0, 800, 600, &row); {
        fuse_div_begin(c, 0, 0, 200, 600, &sidebar); {
            fuse_id(c, "ok");
            fuse_button(c, 8, 8, 184, 32, 0xFF333333, 0xFF555555);
            fuse_idi(c, "vol", 0);
            fuse_slider(c, 8, 48, 184, 16, 0xFF222222, 0xFFFFFFFF, &nob);
        } fuse_div_end(c);
        fuse_div_begin(c, 200, 0, 600, 600, NULL); {
        } fuse_div_end(c);
    } fuse_div_end(c);
    cmds = fuse_canvas_draw(c, &n);
    expect(cmds != NULL && n > 0, "frame 1 cmds");
    expect(get_error(c) == FUSE_ERR_OK, "frame 1 ok");

    saw_clip = 0;
    saw_button = 0;
    saw_track = 0;
    for (i = 0; i < n; i++) {
        if (cmds[i].type == FUSE_CMD_CLIP_START)
            saw_clip = 1;
        if (cmds[i].type == FUSE_CMD_RECT && cmds[i].rect.color == 0xFF333333)
            saw_button = 1;
        if (cmds[i].type == FUSE_CMD_RECT && cmds[i].rect.color == 0xFF222222)
            saw_track = 1;
    }
    expect(saw_clip && saw_button && saw_track, "sidebar cmds");

    fuse_canvas_pointer(c, FUSE_POINTER_PRESSED, 40, 20);
    fuse_canvas_pointer(c, FUSE_POINTER_RELEASED, 40, 20);
    fuse_canvas_clear(c);
    fuse_div_begin(c, 0, 0, 800, 600, &row); {
        fuse_div_begin(c, 0, 0, 200, 600, &sidebar); {
            fuse_id(c, "ok");
            clicked = fuse_button(c, 8, 8, 184, 32, 0xFF333333, 0xFF555555);
            fuse_idi(c, "vol", 0);
            fuse_slider(c, 8, 48, 184, 16, 0xFF222222, 0xFFFFFFFF, &nob);
        } fuse_div_end(c);
        fuse_div_begin(c, 200, 0, 600, 600, NULL); {
        } fuse_div_end(c);
    } fuse_div_end(c);
    fuse_canvas_draw(c, &n);
    expect(clicked, "button click last-frame hover");
}

void
test_insert_does_not_steal(void)
{
    unsigned char buf[1 << 16];
    FuseCanvas c;
    size_t n;
    int clicked;
    printf("insert before button\n");
    c = fuse_canvas_create(buf, sizeof buf);
    fuse_canvas_resize(c, 200, 200);
    fuse_canvas_pointer(c, FUSE_POINTER_RELEASED, 15, 15);

    fuse_canvas_clear(c);
    fuse_id(c, "ok");
    fuse_button(c, 10, 10, 20, 20, 0x1, 0x2);
    fuse_canvas_draw(c, &n);

    fuse_canvas_pointer(c, FUSE_POINTER_PRESSED, 15, 15);
    fuse_canvas_pointer(c, FUSE_POINTER_RELEASED, 15, 15);
    fuse_canvas_clear(c);
    fuse_button(c, 80, 80, 20, 20, 0x3, 0x4);
    fuse_id(c, "ok");
    clicked = fuse_button(c, 10, 10, 20, 20, 0x1, 0x2);
    fuse_canvas_draw(c, &n);
    expect(clicked, "named id keeps click");
}

void
test_unbalanced(void)
{
    unsigned char buf[1 << 16];
    FuseCanvas c;
    size_t n;
    FuseCmd *cmds;
    printf("unbalanced\n");
    c = fuse_canvas_create(buf, sizeof buf);
    fuse_canvas_resize(c, 100, 100);
    fuse_canvas_clear(c);
    fuse_div_end(c);
    expect(get_error(c) == FUSE_ERR_UNBALANCED, "extra end");
    cmds = fuse_canvas_draw(c, &n);
    expect(cmds == NULL && n == 0, "draw fails");

    fuse_canvas_clear(c);
    expect(get_error(c) == FUSE_ERR_OK, "clear resets error");
    fuse_div_begin(c, 0, 0, 50, 50, NULL);
    cmds = fuse_canvas_draw(c, &n);
    expect(cmds == NULL && get_error(c) == FUSE_ERR_UNBALANCED, "missing end");
}

void
test_percent(void)
{
    unsigned char buf[1 << 16];
    FuseCanvas c;
    printf("percent\n");
    c = fuse_canvas_create(buf, sizeof buf);
    fuse_canvas_resize(c, 200, 100);
    fuse_canvas_clear(c);
    expect(nearly(fuse_percent_x(c, 50), 100, 0.01f), "root 50% x");
    expect(nearly(fuse_percent_y(c, 25), 25, 0.01f), "root 25% y");
    fuse_div_begin(c, 0, 0, 80, 40, NULL);
    expect(nearly(fuse_percent_x(c, 50), 40, 0.01f), "div 50% x");
    fuse_div_end(c);
}

void
test_rect_text(void)
{
    unsigned char buf[1 << 16];
    FuseCanvas c;
    size_t n, i;
    FuseCmd *cmds;
    int nrect;
    int clicked;
    FuseCmd *r;

    printf("rect text\n");
    c = fuse_canvas_create(buf, sizeof buf);
    fuse_canvas_resize(c, 200, 100);
    fuse_canvas_pointer(c, FUSE_POINTER_RELEASED, 0, 0);
    fuse_canvas_clear(c);
    fuse_rect(c, 10, 20, 30, 40, 0xAABBCCDD);
    cmds = fuse_canvas_draw(c, &n);
    r = find_rect(cmds, n, 0xAABBCCDD);
    expect(r && nearly(r->rect.x, 10, 0.01f) && nearly(r->rect.y, 20, 0.01f), "rect pos");
    expect(r && nearly(r->rect.w, 30, 0.01f) && nearly(r->rect.h, 40, 0.01f), "rect size");
    expect(nearly(fuse_text_width("HI", 2.0f), 22.0f, 0.01f), "text width");

    fuse_canvas_clear(c);
    fuse_text(c, 0, 0, 1.0f, "H", 0xFF0000FFu);
    cmds = fuse_canvas_draw(c, &n);
    nrect = 0;
    if (cmds) {
        for (i = 0; i < n; i++) {
            if (cmds[i].type == FUSE_CMD_RECT && cmds[i].rect.color == 0xFF0000FFu)
                nrect++;
        }
    }
    expect(nrect > 0, "glyph rects");

    fuse_canvas_pointer(c, FUSE_POINTER_RELEASED, 10, 10);
    fuse_canvas_clear(c);
    fuse_id(c, "go");
    fuse_button_text(c, "GO", 0, 0, 20, 20, 0x11111111, 0x22222222);
    cmds = fuse_canvas_draw(c, &n);
    expect(cmds && n > 1, "button_text cmds");

    fuse_canvas_pointer(c, FUSE_POINTER_PRESSED, 10, 10);
    fuse_canvas_pointer(c, FUSE_POINTER_RELEASED, 10, 10);
    fuse_canvas_clear(c);
    fuse_id(c, "go");
    clicked = fuse_button_text(c, "GO", 0, 0, 20, 20, 0x11111111, 0x22222222);
    fuse_canvas_draw(c, &n);
    expect(clicked, "button_text click");
}

void
test_scroll(void)
{
    unsigned char buf[1 << 16];
    FuseCanvas c;
    size_t n, i;
    FuseCmd *cmds, *r;
    float scroll;
    int saw_clip;
    int clicked;
    int nclip;

    printf("scroll\n");
    c = fuse_canvas_create(buf, sizeof buf);
    fuse_canvas_resize(c, 200, 200);
    fuse_canvas_pointer(c, FUSE_POINTER_RELEASED, 0, 0);

    scroll = 20.0f;
    fuse_canvas_clear(c);
    fuse_id(c, "list");
    fuse_div_begin_scroll(c, 50, 50, 100, 50, NULL, &scroll); {
        fuse_rect(c, 0, 0, 10, 10, 0xAABBCCDDu);
        fuse_rect(c, 0, 20, 10, 10, 0x11111111u);
        fuse_rect(c, 0, 40, 10, 10, 0x22222222u);
        fuse_rect(c, 0, 60, 10, 10, 0x33333333u);
    } fuse_div_end(c);
    cmds = fuse_canvas_draw(c, &n);
    expect(cmds != NULL, "scroll draw");
    expect(nearly(scroll, 20.0f, 0.01f), "scroll kept");
    r = find_rect(cmds, n, 0xAABBCCDD);
    expect(r && nearly(r->rect.x, 50, 0.01f) && nearly(r->rect.y, 30, 0.01f), "child offset by scroll");
    saw_clip = 0;
    nclip = 0;
    if (cmds) {
        for (i = 0; i < n; i++) {
            if (cmds[i].type == FUSE_CMD_CLIP_START) {
                saw_clip = 1;
                expect(nearly(cmds[i].clip.x, 50, 0.01f) && nearly(cmds[i].clip.h, 50, 0.01f), "clip viewport");
            }
            if (cmds[i].type == FUSE_CMD_CLIP_START || cmds[i].type == FUSE_CMD_CLIP_END)
                nclip++;
        }
    }
    expect(saw_clip && nclip >= 2, "clip pair");

    scroll = 999.0f;
    fuse_canvas_clear(c);
    fuse_id(c, "list");
    fuse_div_begin_scroll(c, 50, 50, 100, 50, NULL, &scroll); {
        fuse_rect(c, 0, 0, 10, 10, 0xAABBCCDDu);
        fuse_rect(c, 0, 70, 10, 10, 0x44444444u);
    } fuse_div_end(c);
    fuse_canvas_draw(c, &n);
    expect(nearly(scroll, 30.0f, 0.01f), "scroll clamped to content");

    scroll = 0.0f;
    fuse_canvas_pointer(c, FUSE_POINTER_RELEASED, 60, 60);
    fuse_canvas_clear(c);
    fuse_id(c, "list");
    fuse_div_begin_scroll(c, 50, 50, 100, 50, NULL, &scroll); {
        fuse_rect(c, 0, 0, 40, 40, 0x55555555u);
        fuse_rect(c, 0, 80, 10, 10, 0x66666666u);
    } fuse_div_end(c);
    fuse_canvas_draw(c, &n);

    fuse_canvas_wheel(c, 0, 24.0f);
    fuse_canvas_clear(c);
    fuse_id(c, "list");
    fuse_div_begin_scroll(c, 50, 50, 100, 50, NULL, &scroll); {
        fuse_rect(c, 0, 0, 40, 40, 0x55555555u);
        fuse_rect(c, 0, 80, 10, 10, 0x66666666u);
    } fuse_div_end(c);
    fuse_canvas_draw(c, &n);
    expect(nearly(scroll, 0.0f, 0.01f), "wheel up clamps at 0");

    fuse_canvas_wheel(c, 0, -24.0f);
    fuse_canvas_clear(c);
    fuse_id(c, "list");
    fuse_div_begin_scroll(c, 50, 50, 100, 50, NULL, &scroll); {
        fuse_rect(c, 0, 0, 40, 40, 0x55555555u);
        fuse_rect(c, 0, 80, 10, 10, 0x66666666u);
    } fuse_div_end(c);
    fuse_canvas_draw(c, &n);
    expect(nearly(scroll, 24.0f, 0.01f), "wheel down increases scroll");

    scroll = 80.0f;
    fuse_canvas_pointer(c, FUSE_POINTER_RELEASED, 60, 60);
    fuse_canvas_clear(c);
    fuse_id(c, "btn");
    fuse_div_begin_scroll(c, 50, 50, 100, 50, NULL, &scroll); {
        fuse_button(c, 0, 0, 20, 20, 0x1, 0x2);
        fuse_rect(c, 0, 200, 10, 10, 0x77777777u);
    } fuse_div_end(c);
    fuse_canvas_draw(c, &n);

    fuse_canvas_pointer(c, FUSE_POINTER_PRESSED, 60, 60);
    fuse_canvas_pointer(c, FUSE_POINTER_RELEASED, 60, 60);
    fuse_canvas_clear(c);
    fuse_id(c, "btn");
    fuse_div_begin_scroll(c, 50, 50, 100, 50, NULL, &scroll); {
        clicked = fuse_button(c, 0, 0, 20, 20, 0x1, 0x2);
        fuse_rect(c, 0, 200, 10, 10, 0x77777777u);
    } fuse_div_end(c);
    fuse_canvas_draw(c, &n);
    expect(!clicked, "scrolled-out button does not click");
}

void
test_debug(void)
{
    unsigned char buf[1 << 16];
    unsigned char buf_b[1 << 16];
    FuseCanvas c;
    FuseCanvas other;
    FuseCmd *cmds;
    size_t n;
    int clicked;
    float scroll;
    printf("debug\n");
    c = fuse_canvas_create(buf, sizeof buf);
    other = fuse_canvas_create(buf_b, sizeof buf_b);
    fuse_canvas_resize(c, 200, 100);
    fuse_canvas_resize(other, 200, 100);
    fuse_canvas_pointer(c, FUSE_POINTER_RELEASED, 20, 20);
    fuse_canvas_clear(c);
    fuse_id(c, "ok");
    fuse_button(c, 10, 10, 40, 20, 0xFF111111u, 0xFF222222u);
    cmds = fuse_canvas_draw(c, &n);
    expect(cmds && !has_panel(cmds, n, 200, 100), "debug off has no panel");

    fuse_canvas_debug(c, true);
    fuse_canvas_debug(other, false);
    fuse_canvas_clear(c);
    fuse_id(c, "ok");
    fuse_button(c, 10, 10, 40, 20, 0xFF111111u, 0xFF222222u);
    cmds = fuse_canvas_draw(c, &n);
    expect(cmds && get_error(c) == FUSE_ERR_OK, "debug draw");
    expect(has_panel(cmds, n, 200, 100), "panel on the right");
    expect(has_rect_at(cmds, n, 10, 10, 40, 1, 0xFFFF9900u), "hover box");

    fuse_canvas_clear(other);
    fuse_button(other, 10, 10, 40, 20, 0xFF111111u, 0xFF222222u);
    cmds = fuse_canvas_draw(other, &n);
    expect(cmds && !has_panel(cmds, n, 200, 100), "other canvas stays off");

    fuse_canvas_pointer(c, FUSE_POINTER_PRESSED, 20, 20);
    fuse_canvas_pointer(c, FUSE_POINTER_RELEASED, 20, 20);
    fuse_canvas_clear(c);
    fuse_id(c, "ok");
    clicked = fuse_button(c, 10, 10, 40, 20, 0xFF111111u, 0xFF222222u);
    cmds = fuse_canvas_draw(c, &n);
    expect(!clicked, "inspect does not click");
    expect(get_error(c) == FUSE_ERR_OK, "inspect click ok");

    fuse_canvas_pointer(c, FUSE_POINTER_RELEASED, 0, 0);
    fuse_canvas_clear(c);
    fuse_id(c, "ok");
    fuse_button(c, 10, 10, 40, 20, 0xFF111111u, 0xFF222222u);
    cmds = fuse_canvas_draw(c, &n);
    expect(has_rect_at(cmds, n, 10, 10, 40, 1, 0xFFFFFFFFu), "selected box stays");
    expect(!has_rect_at(cmds, n, 10, 10, 40, 1, 0xFFFF9900u), "hover box follows the pointer");

    scroll = 0.0f;
    fuse_canvas_pointer(c, FUSE_POINTER_RELEASED, 190, 40);
    fuse_canvas_wheel(c, 0.0f, -24.0f);
    fuse_canvas_clear(c);
    fuse_id(c, "list");
    fuse_div_begin_scroll(c, 0, 0, 80, 40, NULL, &scroll); {
        fuse_rect(c, 0, 0, 10, 80, 0xFFABCDEFu);
    } fuse_div_end(c);
    fuse_canvas_draw(c, &n);
    expect(nearly(scroll, 0.0f, 0.01f), "wheel over the panel does not scroll the list");
    expect(get_error(c) == FUSE_ERR_OK, "panel wheel ok");

    scroll = 0.0f;
    fuse_canvas_pointer(c, FUSE_POINTER_RELEASED, 20, 20);
    fuse_canvas_clear(c);
    fuse_id(c, "list");
    fuse_div_begin_scroll(c, 0, 0, 80, 40, NULL, &scroll); {
        fuse_rect(c, 0, 0, 10, 80, 0xFFABCDEFu);
    } fuse_div_end(c);
    fuse_canvas_draw(c, &n);

    fuse_canvas_wheel(c, 0.0f, -24.0f);
    fuse_canvas_clear(c);
    fuse_id(c, "list");
    fuse_div_begin_scroll(c, 0, 0, 80, 40, NULL, &scroll); {
        fuse_rect(c, 0, 0, 10, 80, 0xFFABCDEFu);
    } fuse_div_end(c);
    fuse_canvas_draw(c, &n);
    expect(nearly(scroll, 24.0f, 0.01f), "wheel over a list still scrolls");
}

void
test_columns(void)
{
    unsigned char buf[1 << 16];
    FuseCanvas c;
    FuseClass cls;
    FuseCmd *cmds;
    size_t n;
    size_t i;
    float ratio[2];
    int saw_glyph;
    printf("columns\n");
    c = fuse_canvas_create(buf, sizeof buf);
    fuse_canvas_resize(c, 200, 200);
    fuse_canvas_pointer(c, FUSE_POINTER_RELEASED, 0, 0);
    ratio[0] = 1.0f;
    ratio[1] = 3.0f;
    fuse_canvas_clear(c);
    fuse_col_begin(c, 10.0f, 20.0f, 80.0f, NULL, ratio, 2); {
        fuse_rect(c, 0.0f, 0.0f, 8.0f, 8.0f, 0xFF010101u);
        fuse_text(c, 0.0f, 0.0f, 1.0f, "A", 0xFF00AA00u);
        fuse_col_switch(c);
        fuse_rect(c, 2.0f, 4.0f, 6.0f, 6.0f, 0xFF020202u);
    } fuse_col_end(c);
    cmds = fuse_canvas_draw(c, &n);
    expect(cmds && get_error(c) == FUSE_ERR_OK, "ratio draw");
    expect(has_rect_at(cmds, n, 10.0f, 20.0f, 8.0f, 8.0f, 0xFF010101u), "left column origin");
    expect(has_rect_at(cmds, n, 32.0f, 24.0f, 6.0f, 6.0f, 0xFF020202u), "right column is 3/4");
    saw_glyph = 0;
    if (cmds) {
        for (i = 0; i < n; i++) {
            if (cmds[i].type == FUSE_CMD_RECT && cmds[i].rect.color == 0xFF00AA00u &&
                cmds[i].rect.y >= 20.0f && cmds[i].rect.y < 27.0f &&
                cmds[i].rect.x >= 10.0f && cmds[i].rect.x < 16.0f)
                saw_glyph = 1;
        }
    }
    expect(saw_glyph, "bitmap text is inside the column");

    memset(&cls, 0, sizeof cls);
    cls.gap = 4.0f;
    cls.height_sizing = FUSE_SIZING_GROW;
    ratio[0] = 1.0f;
    ratio[1] = 1.0f;
    fuse_canvas_clear(c);
    fuse_col_begin(c, 0.0f, 0.0f, 100.0f, &cls, ratio, 2); {
        fuse_rect(c, 0.0f, 0.0f, 10.0f, 10.0f, 0xFF0A0A0Au);
        fuse_rect(c, 0.0f, 0.0f, 10.0f, 10.0f, 0xFF0B0B0Bu);
        fuse_col_switch(c);
        fuse_rect(c, 0.0f, 0.0f, 10.0f, 10.0f, 0xFF0C0C0Cu);
    } fuse_col_end(c);
    cmds = fuse_canvas_draw(c, &n);
    expect(cmds && get_error(c) == FUSE_ERR_OK, "stack draw");
    expect(has_rect_at(cmds, n, 0.0f, 0.0f, 10.0f, 10.0f, 0xFF0A0A0Au), "first stacked child");
    expect(has_rect_at(cmds, n, 0.0f, 14.0f, 10.0f, 10.0f, 0xFF0B0B0Bu), "second stacked child");
    expect(has_rect_at(cmds, n, 52.0f, 0.0f, 10.0f, 24.0f, 0xFF0C0C0Cu), "short column grows to the tall one");

    fuse_canvas_clear(c);
    fuse_col_end(c);
    expect(get_error(c) == FUSE_ERR_UNBALANCED, "end without begin");
    fuse_canvas_clear(c);
    fuse_col_begin(c, 0.0f, 0.0f, 40.0f, NULL, ratio, 2);
    fuse_col_switch(c);
    fuse_col_switch(c);
    expect(get_error(c) == FUSE_ERR_UNBALANCED, "switch past the last column");
}

static int
has_ink_in(FuseCmd *cmds, size_t n, float x, float y, float w, float h, uint32_t color)
{
    size_t i;

    if (!cmds)
        return 0;
    for (i = 0; i < n; i++) {
        if (cmds[i].type != FUSE_CMD_RECT)
            continue;
        if (cmds[i].rect.color != color)
            continue;
        if (cmds[i].rect.x < x || cmds[i].rect.y < y)
            continue;
        if (cmds[i].rect.x >= x + w || cmds[i].rect.y >= y + h)
            continue;
        return 1;
    }
    return 0;
}

void
test_sizing(void)
{
    unsigned char buf[1 << 16];
    FuseCanvas c;
    FuseClass row;
    FuseClass col;
    FuseCmd *cmds;
    size_t n;

    printf("sizing\n");
    c = fuse_canvas_create(buf, sizeof buf);
    fuse_canvas_resize(c, 200, 120);
    fuse_canvas_pointer(c, FUSE_POINTER_RELEASED, 0, 0);
    memset(&row, 0, sizeof row);
    row.direction = FUSE_DIRECTION_ROW;
    row.width_sizing = FUSE_SIZING_FIT;
    row.height_sizing = FUSE_SIZING_FIT;
    fuse_canvas_clear(c);
    fuse_div_begin(c, 0, 0, 200, 20, &row); {
        fuse_rect(c, 0, 0, 20, 10, 0xFF111111u);
        fuse_space(c);
        fuse_rect(c, 0, 0, 30, 10, 0xFF222222u);
    } fuse_div_end(c);
    cmds = fuse_canvas_draw(c, &n);
    expect(cmds && get_error(c) == FUSE_ERR_OK, "space draw");
    expect(has_rect_at(cmds, n, 0, 0, 20, 10, 0xFF111111u), "space keeps the left child");
    expect(has_rect_at(cmds, n, 170, 0, 30, 10, 0xFF222222u), "space pushes the right child");

    memset(&col, 0, sizeof col);
    col.direction = FUSE_DIRECTION_COLUMN;
    col.width_sizing = FUSE_SIZING_FIT;
    col.height_sizing = FUSE_SIZING_FIT;
    fuse_canvas_clear(c);
    fuse_div_begin(c, 0, 0, 40, 100, &col); {
        fuse_rect(c, 0, 0, 10, 10, 0xFF333333u);
        fuse_sizing(c, FUSE_SIZING_GROW, FUSE_SIZING_GROW);
        fuse_rect(c, 0, 0, 8, 8, 0xFF444444u);
    } fuse_div_end(c);
    cmds = fuse_canvas_draw(c, &n);
    expect(has_rect_at(cmds, n, 0, 0, 10, 10, 0xFF333333u), "column fit child stays");
    expect(has_rect_at(cmds, n, 0, 10, 40, 90, 0xFF444444u), "stemmed child takes leftover");

    memset(&row, 0, sizeof row);
    row.direction = FUSE_DIRECTION_ROW;
    row.width_sizing = FUSE_SIZING_FIT;
    row.height_sizing = FUSE_SIZING_FIT;
    fuse_canvas_clear(c);
    fuse_div_begin(c, 10, 10, 80, 20, &row); {
        fuse_sizing(c, FUSE_SIZING_GROW, FUSE_SIZING_FIT);
        fuse_button_text(c, "I", 0, 0, 20, 20, 0xFFAAAAAAu, 0xFFBBBBBBu);
    } fuse_div_end(c);
    cmds = fuse_canvas_draw(c, &n);
    expect(has_rect_at(cmds, n, 10, 10, 80, 20, 0xFFAAAAAAu), "button grows with the row");
    expect(has_ink_in(cmds, n, 45, 13, 10, 14, 0xFF000000u), "label recenters in the grown button");
    expect(!has_ink_in(cmds, n, 0, 0, 10, 10, 0xFF000000u), "label is not a sibling outside the button");
}

static int
allow_digit(unsigned char ch, void *user)
{
    (void)user;
    return ch >= '0' && ch <= '9';
}

void
test_textbox(void)
{
    unsigned char buf[1 << 16];
    FuseCanvas c;
    FuseCmd *cmds;
    FuseCmd *text;
    size_t n;
    size_t i;
    int saw_clip;
    char name[32];
    char path[32];
    int caret;
    int changed;

    printf("textbox\n");
    c = fuse_canvas_create(buf, sizeof buf);
    fuse_canvas_resize(c, 200, 100);
    fuse_canvas_pointer(c, FUSE_POINTER_RELEASED, 0, 0);

    name[0] = 0;
    caret = 0;
    fuse_focus(c, "name");
    fuse_canvas_text(c, "Hi!");
    fuse_canvas_clear(c);
    fuse_id(c, "name");
    changed = fuse_textbox(c, 10, 10, 120, 32, 8, 8, name, (int)sizeof name, &caret,
        "name", 16, 0xFF111111u, 0xFF222222u, 0xFFEEEEEE, 0xFF888888u, 1, NULL, NULL);
    cmds = fuse_canvas_draw(c, &n);
    text = find_text(cmds, n);
    {
        const FuseFieldRun *runs;
        size_t nf;
        const FuseFieldRun *run;

        runs = fuse_canvas_fields(c, &nf);
        run = (text && runs && text->text.slot < nf) ? &runs[text->text.slot] : NULL;
        expect(changed, "text inserts");
        expect(strcmp(name, "Hi!") == 0 && caret == 3, "buffer and caret");
        expect(run && run->str && strcmp(run->str, "Hi!") == 0, "text command");
        expect(run && run->caret == 3, "caret is sent to the renderer");
    }
    saw_clip = 0;
    if (cmds) {
        for (i = 0; i < n; i++) {
            if (cmds[i].type == FUSE_CMD_CLIP_START)
                saw_clip = 1;
        }
    }
    expect(saw_clip, "field clips the glyphs");
    expect(fuse_focused(c, "name"), "focus stays on the field");

    fuse_canvas_key(c, FUSE_KEY_BACKSPACE);
    fuse_canvas_clear(c);
    fuse_id(c, "name");
    changed = fuse_textbox(c, 10, 10, 120, 32, 8, 8, name, (int)sizeof name, &caret,
        "name", 16, 0xFF111111u, 0xFF222222u, 0xFFEEEEEE, 0xFF888888u, 0, NULL, NULL);
    cmds = fuse_canvas_draw(c, &n);
    text = find_text(cmds, n);
    {
        const FuseFieldRun *runs;
        size_t nf;
        const FuseFieldRun *run;

        runs = fuse_canvas_fields(c, &nf);
        run = (text && runs && text->text.slot < nf) ? &runs[text->text.slot] : NULL;
        expect(changed && strcmp(name, "Hi") == 0 && caret == 2, "backspace");
        expect(run && run->caret < 0, "blink hides the caret");
    }

    path[0] = 0;
    caret = 0;
    fuse_focus(c, "path");
    fuse_canvas_text(c, "a1b2");
    fuse_canvas_clear(c);
    fuse_id(c, "name");
    changed = fuse_textbox(c, 10, 10, 80, 32, 4, 4, name, (int)sizeof name, &caret,
        NULL, 16, 0xFF111111u, 0xFF222222u, 0xFFEEEEEE, 0xFF888888u, 1, NULL, NULL);
    expect(!changed && strcmp(name, "Hi") == 0, "unfocused field ignores keys");
    fuse_id(c, "path");
    changed = fuse_textbox(c, 10, 50, 80, 32, 4, 4, path, (int)sizeof path, &caret,
        "path", 16, 0xFF111111u, 0xFF222222u, 0xFFEEEEEE, 0xFF888888u, 1, allow_digit, NULL);
    cmds = fuse_canvas_draw(c, &n);
    {
        const FuseFieldRun *runs;
        size_t nf;
        const FuseFieldRun *run;

        runs = fuse_canvas_fields(c, &nf);
        run = NULL;
        if (cmds && runs) {
            for (i = 0; i < n; i++) {
                if (cmds[i].type != FUSE_CMD_TEXT || cmds[i].text.slot >= nf)
                    continue;
                if (runs[cmds[i].text.slot].str &&
                    strcmp(runs[cmds[i].text.slot].str, "12") == 0)
                    run = &runs[cmds[i].text.slot];
            }
        }
        expect(changed && strcmp(path, "12") == 0, "filter drops letters");
        expect(run && run->color == 0xFFEEEEEE, "value uses the ink");
    }

    name[0] = 0;
    caret = 0;
    fuse_focus(c, NULL);
    fuse_canvas_clear(c);
    fuse_id(c, "name");
    fuse_textbox(c, 10, 10, 80, 32, 4, 4, name, (int)sizeof name, &caret,
        "Find", 16, 0xFF111111u, 0xFF222222u, 0xFFEEEEEE, 0xFF888888u, 1, NULL, NULL);
    cmds = fuse_canvas_draw(c, &n);
    text = find_text(cmds, n);
    {
        const FuseFieldRun *runs;
        size_t nf;
        const FuseFieldRun *run;

        runs = fuse_canvas_fields(c, &nf);
        run = (text && runs && text->text.slot < nf) ? &runs[text->text.slot] : NULL;
        expect(run && run->str && strcmp(run->str, "Find") == 0, "placeholder");
        expect(run && run->color == 0xFF888888u, "placeholder is ghost ink");
    }

    fuse_canvas_pointer(c, FUSE_POINTER_PRESSED, 20, 20);
    fuse_canvas_pointer(c, FUSE_POINTER_RELEASED, 20, 20);
    fuse_canvas_clear(c);
    fuse_id(c, "name");
    fuse_textbox(c, 10, 10, 80, 32, 4, 4, name, (int)sizeof name, &caret,
        "Find", 16, 0xFF111111u, 0xFF222222u, 0xFFEEEEEE, 0xFF888888u, 1, NULL, NULL);
    fuse_canvas_draw(c, &n);
    expect(fuse_focused(c, "name"), "click focuses the field");
}

void
test_radial(void)
{
    size_t bytes = fuse_canvas_memory(128), n, i, triangles = 0;
    void *memory = malloc(bytes);
    FuseCanvas c = fuse_canvas_create(memory, bytes);
    FuseCmd *cmds;

    expect(c != NULL, "radial canvas");
    fuse_canvas_resize(c, 300, 300);
    fuse_div_begin(c, 20, 30, 200, 200, NULL);
    expect(fuse_radial(c, 100, 100, 20, 80, 2, 150, 100, 1, 2) == 0, "radial right");
    fuse_div_end(c);
    cmds = fuse_canvas_draw(c, &n);
    expect(cmds != NULL, "radial draw");
    for (i = 0; cmds && i < n; i++) {
        if (cmds[i].type != FUSE_CMD_TRIANGLE)
            continue;
        triangles++;
        expect(cmds[i].triangle.x1 >= 40 && cmds[i].triangle.x1 <= 200 &&
            cmds[i].triangle.y1 >= 50 && cmds[i].triangle.y1 <= 210, "translated radial geometry");
    }
    expect(triangles == 128, "bounded radial tessellation");
    expect(fuse_radial_pick(0, 0, 20, 80, 2, -50, 0) == 1, "radial left");
    expect(fuse_radial_pick(0, 0, 20, 80, 2, 0, 0) == -1, "dead zone");
    expect(fuse_radial_pick(0, 0, 20, 80, 2, 20, 0) == -1, "inner boundary cancels");
    expect(fuse_radial_pick(0, 0, 20, 80, 2, 80, 0) == -1, "outer boundary cancels");
    expect(fuse_radial_pick(0, 0, 20, 80, 2, 0, 50) == 1, "lower angular boundary");
    expect(fuse_radial_pick(0, 0, 20, 80, 2, 0, -50) == 0, "upper angular boundary");
    expect(fuse_radial_pick(0, 0, 20, 80, 0, 50, 0) == -1, "invalid count");
    expect(fuse_radial_pick(0, 0, 20, 80, 2, NAN, 0) == -1, "nonfinite pointer");
    free(memory);
    bytes = fuse_canvas_memory(8);
    memory = malloc(bytes);
    c = fuse_canvas_create(memory, bytes);
    fuse_canvas_resize(c, 300, 300);
    fuse_radial(c, 100, 100, 20, 80, 2, 150, 100, 1, 2);
    expect(fuse_canvas_draw(c, &n) == NULL && get_error(c) == FUSE_ERR_OVERFLOW,
        "radial command overflow reported");
    free(memory);
}


#include "fuse_dock_test.c"

int
main(void)
{
    test_dock();
    test_dock_interaction();
    test_memory();
    test_two_canvases();
    test_screen_origin();
    test_sidebar();
    test_insert_does_not_steal();
    test_unbalanced();
    test_percent();
    test_rect_text();
    test_scroll();
    test_debug();
    test_columns();
    test_sizing();
    test_textbox();
    test_radial();
    if (g_fails) {
        printf("%d failed\n", g_fails);
        return 1;
    }
    printf("all ok\n");
    return 0;
}
