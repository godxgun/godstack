#ifndef FUSE_DEBUG
#define FUSE_DEBUG
#endif

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fuse.h"
#include "fuse.c"

static int g_fails;

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
    expect(fuse_canvas_error(NULL) == FUSE_ERR_BUF_TOO_SMALL, "error null canvas");
    need = fuse_canvas_memory(64);
    expect(need > 0 && need <= sizeof buf, "memory(64) fits");
    expect(fuse_canvas_memory(8192) <= (2u << 20), "memory(8192) fits 2MiB");
    c = fuse_canvas_create(buf, need);
    expect(c != NULL, "create exact");
    expect(fuse_canvas_error(c) == FUSE_ERR_OK, "create ok");
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
    expect(fuse_canvas_error(c) == FUSE_ERR_OK, "frame 1 ok");

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
    expect(fuse_canvas_error(c) == FUSE_ERR_UNBALANCED, "extra end");
    cmds = fuse_canvas_draw(c, &n);
    expect(cmds == NULL && n == 0, "draw fails");

    fuse_canvas_clear(c);
    expect(fuse_canvas_error(c) == FUSE_ERR_OK, "clear resets error");
    fuse_div_begin(c, 0, 0, 50, 50, NULL);
    cmds = fuse_canvas_draw(c, &n);
    expect(cmds == NULL && fuse_canvas_error(c) == FUSE_ERR_UNBALANCED, "missing end");
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

int
main(void)
{
    test_memory();
    test_two_canvases();
    test_screen_origin();
    test_sidebar();
    test_insert_does_not_steal();
    test_unbalanced();
    test_percent();
    test_rect_text();
    test_scroll();
    if (g_fails) {
        printf("%d failed\n", g_fails);
        return 1;
    }
    printf("all ok\n");
    return 0;
}
