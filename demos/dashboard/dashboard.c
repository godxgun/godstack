#include "rend.h"
#include "peak.c"
#include "rend.c"
#include "fuse.h"
#include "fuse.c"
#include "fuse_rend.h"
#include "fuse_rend.c"
#include "demos/headless.h"

#include <stdint.h>
#include <string.h>

#define COL_BG       0xFF0E1116u
#define COL_SIDE     0xFF161B22u
#define COL_TOP      0xFF1C2330u
#define COL_CARD     0xFF212833u
#define COL_BTN      0xFF2D3644u
#define COL_BTN_H    0xFF3D4A5Cu
#define COL_ACCENT   0xFF3B82F6u
#define COL_ACCENT_H 0xFF60A5FAu
#define COL_OK       0xFF22C55Eu
#define COL_WARN     0xFFF59E0Bu
#define COL_BAD      0xFFEF4444u
#define COL_TRACK    0xFF111827u
#define COL_NOB      0xFFF8FAFCu

#define MAX_UI 256
#define MAX_VERTS 65536

enum {
    PAGE_OVERVIEW = 0,
    PAGE_USERS,
    PAGE_JOBS,
    PAGE_SETTINGS,
    PAGE_COUNT
};

static uint8_t *
load_spv(const char *a, const char *b, unsigned long *size)
{
    uint8_t *p;
    p = peak_file_alloc(a, size);
    if (p)
        return p;
    return peak_file_alloc(b, size);
}

int
main(int argc, char **argv)
{
    PeakWindow win;
    RendBindingInfo bind_info;
    RendRenderer renderer;
    FuseRend fr;
    FuseCanvas canvas;
    static unsigned char ui_buf[1 << 18];
    static unsigned char vert_buf[MAX_VERTS * sizeof (FuseRendVertex)];
    unsigned long vert_spv_n, frag_spv_n;
    uint8_t *vert_spv, *frag_spv;
    int running;
    int page;
    int selected_user;
    int notify_on;
    int pointer_state;
    float mx, my;
    float cpu, mem, net, volume, theme;
    float job_a, job_b, job_c, job_d;
    int headless;
    int frames;
    int frame_i;
    const char *ppm;
    uint32_t width;
    uint32_t height;

    width = 1280;
    height = 720;
    memset(&win, 0, sizeof win);
    headless = headless_parse(argc, argv, &frames, &ppm);

    if (!headless) {
        if (!peak_init()) {
            PFATAL("Failed to init Peak!");
            return 1;
        }

        win = peak_window_open("Fuse Dashboard", width, height, 0);
        if (!win.running) {
            PFATAL("Failed to open a window!");
            return 1;
        }
        width = win.width;
        height = win.height;
    }

    memset(&bind_info, 0, sizeof bind_info);
    if (headless)
        renderer = rend_renderer_create_offscreen(width, height, REND_FORMAT_R8G8B8A8_UNORM, REND_BACKEND_AUTO, &bind_info);
    else
        renderer = rend_renderer_create(&win, REND_BACKEND_AUTO, NULL, true, &bind_info);
    if (!renderer) {
        PFATAL("Failed to create renderer!");
        if (!headless) {
            peak_window_close(&win);
            peak_quit();
        }
        return 1;
    }

    vert_spv = load_spv("fuse_ui.vert.spv", "demos/dashboard/fuse_ui.vert.spv", &vert_spv_n);
    frag_spv = load_spv("fuse_ui.frag.spv", "demos/dashboard/fuse_ui.frag.spv", &frag_spv_n);
    if (!vert_spv || !frag_spv) {
        PFATAL("Failed to load dashboard shaders!");
        return 1;
    }

    if (!fuse_rend_init(&fr, vert_buf, sizeof vert_buf, renderer, vert_spv, vert_spv_n, frag_spv, frag_spv_n)) {
        PFATAL("Failed to init Fuse Rend backend!");
        return 1;
    }
    free(vert_spv);
    free(frag_spv);

    if (fuse_canvas_memory(MAX_UI) > sizeof ui_buf) {
        PFATAL("UI buffer too small!");
        return 1;
    }
    canvas = fuse_canvas_create(ui_buf, sizeof ui_buf);
    if (!canvas) {
        PFATAL("Failed to create Fuse canvas!");
        return 1;
    }

    page = PAGE_OVERVIEW;
    selected_user = 0;
    notify_on = 1;
    pointer_state = FUSE_POINTER_RELEASED;
    mx = 0.0f;
    my = 0.0f;
    cpu = 0.42f;
    mem = 0.61f;
    net = 0.18f;
    volume = 0.7f;
    theme = 0.35f;
    job_a = 0.82f;
    job_b = 0.45f;
    job_c = 0.12f;
    job_d = 0.96f;
    running = 1;
    frame_i = 0;

    while (running) {
        PeakEvent ev;
        float W, H;
        size_t ncmds;
        FuseCmd *cmds;
        static FuseClass sidebar;
        static FuseClass nav;
        int i;

        if (headless && frame_i >= frames)
            break;

        while (!headless && peak_window_epoll(&win, &ev)) {
            if (ev.type == PEAK_EVENT_WINDOW_CLOSE)
                running = 0;
            if (ev.type == PEAK_EVENT_KEY_DOWN && ev.key.key == PEAK_KEY_ESCAPE)
                running = 0;
            if (ev.type == PEAK_EVENT_WINDOW_RESIZE) {
                width = ev.resize.width;
                height = ev.resize.height;
                win.width = width;
                win.height = height;
            }
            if (ev.type == PEAK_EVENT_POINTER) {
                mx = ev.pointer.x;
                my = ev.pointer.y;
                if (ev.pointer.type == PEAK_POINTER_LEFT) {
                    if (ev.pointer.state == PEAK_POINTER_PRESSED)
                        pointer_state = FUSE_POINTER_PRESSED;
                    else if (ev.pointer.state == PEAK_POINTER_RELEASED)
                        pointer_state = FUSE_POINTER_RELEASED;
                } else if (ev.pointer.type == PEAK_POINTER_RIGHT &&
                           ev.pointer.state == PEAK_POINTER_PRESSED) {
                    pointer_state = FUSE_POINTER_ALT;
                }
            }
        }

        W = (float)width;
        H = (float)height;
        if (W < 640.0f)
            W = 640.0f;
        if (H < 400.0f)
            H = 400.0f;

        memset(&sidebar, 0, sizeof sidebar);
        sidebar.color = COL_SIDE;
        memset(&nav, 0, sizeof nav);
        nav.direction = FUSE_DIRECTION_COLUMN;
        nav.gap = 8.0f;
        nav.pad_l = 12.0f;
        nav.pad_r = 12.0f;
        nav.pad_t = 64.0f;
        nav.pad_b = 12.0f;
        nav.width_sizing = FUSE_SIZING_FIT;
        nav.height_sizing = FUSE_SIZING_FIT;

        fuse_canvas_resize(canvas, W, H);
        fuse_canvas_pointer(canvas, pointer_state, mx, my);
        fuse_canvas_clear(canvas);

        fuse_div_begin(canvas, 0, 0, W, H, NULL); {
            fuse_div_begin(canvas, 0, 0, 220, H, &sidebar); {
                fuse_div_begin(canvas, 0, 0, 220, H, &nav); {
                    for (i = 0; i < PAGE_COUNT; i++) {
                        uint32_t idle, hover;
                        idle = (page == i) ? COL_ACCENT : COL_BTN;
                        hover = (page == i) ? COL_ACCENT_H : COL_BTN_H;
                        fuse_idi(canvas, "nav", i);
                        if (fuse_button(canvas, 12, 0, 196, 36, idle, hover))
                            page = i;
                    }
                } fuse_div_end(canvas);
            } fuse_div_end(canvas);

            fuse_div_begin(canvas, 220, 0, W - 220, H, NULL); {
                fuse_div_begin(canvas, 0, 0, W - 220, 56, NULL);
                fuse_div_end(canvas);

                if (page == PAGE_OVERVIEW) {
                    float card_w;
                    card_w = (W - 220.0f - 48.0f) / 3.0f;
                    fuse_div_begin(canvas, 16, 72, card_w, 88, NULL);
                    fuse_div_end(canvas);
                    fuse_div_begin(canvas, 32 + card_w, 72, card_w, 88, NULL);
                    fuse_div_end(canvas);
                    fuse_div_begin(canvas, 48 + card_w * 2.0f, 72, card_w, 88, NULL);
                    fuse_div_end(canvas);

                    fuse_id(canvas, "cpu");
                    fuse_slider(canvas, 16, 200, W - 252, 18, COL_TRACK, COL_ACCENT, &cpu);
                    fuse_id(canvas, "mem");
                    fuse_slider(canvas, 16, 248, W - 252, 18, COL_TRACK, COL_OK, &mem);
                    fuse_id(canvas, "net");
                    fuse_slider(canvas, 16, 296, W - 252, 18, COL_TRACK, COL_WARN, &net);
                }

                if (page == PAGE_USERS) {
                    for (i = 0; i < 6; i++) {
                        uint32_t idle, hover;
                        idle = (selected_user == i) ? COL_ACCENT : COL_BTN;
                        hover = COL_BTN_H;
                        fuse_idi(canvas, "user", i);
                        if (fuse_button(canvas, 16, 72.0f + (float)i * 48.0f, W - 252, 40, idle, hover))
                            selected_user = i;
                    }
                }

                if (page == PAGE_JOBS) {
                    fuse_id(canvas, "job0");
                    fuse_slider(canvas, 16, 80, W - 252, 16, COL_TRACK, COL_OK, &job_a);
                    fuse_id(canvas, "job1");
                    fuse_slider(canvas, 16, 128, W - 252, 16, COL_TRACK, COL_ACCENT, &job_b);
                    fuse_id(canvas, "job2");
                    fuse_slider(canvas, 16, 176, W - 252, 16, COL_TRACK, COL_WARN, &job_c);
                    fuse_id(canvas, "job3");
                    fuse_slider(canvas, 16, 224, W - 252, 16, COL_TRACK, COL_BAD, &job_d);
                }

                if (page == PAGE_SETTINGS) {
                    fuse_id(canvas, "vol");
                    fuse_slider(canvas, 16, 88, W - 252, 18, COL_TRACK, COL_NOB, &volume);
                    fuse_id(canvas, "theme");
                    fuse_slider(canvas, 16, 144, W - 252, 18, COL_TRACK, COL_ACCENT, &theme);
                    fuse_id(canvas, "notify");
                    if (fuse_button(canvas, 16, 200, 200, 36,
                                    notify_on ? COL_OK : COL_BTN,
                                    COL_BTN_H))
                        notify_on = !notify_on;
                    fuse_id(canvas, "reset");
                    if (fuse_button(canvas, 16, 252, 200, 36, COL_BAD, COL_BTN_H)) {
                        cpu = 0.42f;
                        mem = 0.61f;
                        net = 0.18f;
                        volume = 0.7f;
                        theme = 0.35f;
                    }
                }
            } fuse_div_end(canvas);
        } fuse_div_end(canvas);

        cmds = fuse_canvas_draw(canvas, &ncmds);

        if (rend_renderer_frame_begin(renderer)) {
            float card_w;
            rend_cmd_render_begin(renderer, 0.055f, 0.067f, 0.086f, 1.0f);
            fuse_rend_begin(&fr, W, H);
            if (cmds)
                fuse_rend_cmds(&fr, cmds, ncmds);

            /* paint cards / chrome that need fill (divs with NULL class emit no RECT) */
            fuse_rend_quad(&fr, 220, 0, W - 220, 56, COL_TOP);
            if (page == PAGE_OVERVIEW) {
                card_w = (W - 220.0f - 48.0f) / 3.0f;
                fuse_rend_quad(&fr, 236, 72, card_w, 88, COL_CARD);
                fuse_rend_quad(&fr, 252 + card_w, 72, card_w, 88, COL_CARD);
                fuse_rend_quad(&fr, 268 + card_w * 2.0f, 72, card_w, 88, COL_CARD);
            }

            fuse_rend_flush(&fr, renderer);
            rend_cmd_render_end(renderer);
            rend_renderer_frame_end(renderer, NULL);
        }
        frame_i++;
    }

    if (headless && !headless_finish(renderer, width, height, REND_FORMAT_R8G8B8A8_UNORM, ppm)) {
        fuse_rend_shutdown(&fr);
        rend_renderer_destroy(renderer);
        rend_quit();
        return 1;
    }

    fuse_rend_shutdown(&fr);
    rend_renderer_destroy(renderer);
    if (!headless) {
        peak_window_close(&win);
        peak_quit();
    }
    rend_quit();
    return 0;
}
