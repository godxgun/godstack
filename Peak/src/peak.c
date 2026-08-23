static uint32_t *
peak_window_sync(PeakWindow *win, size_t *width, size_t *height)
{
    size_t w = 0, h = 0;
    win->buffer = peak_platform_window_buffer(&win->internal, &w, &h);
    win->width = (uint32_t)w;
    win->height = (uint32_t)h;
    win->bufsize = win->width * win->height;
    if (width) *width = w;
    if (height) *height = h;
    return win->buffer;
}

int
peak_init(void)
{
    return peak_platform_init();
}

void
peak_quit(void)
{
    peak_platform_quit();
}

PeakWindow
peak_window_open(const char *name, uint32_t width, uint32_t height, uint32_t flags)
{
    PeakWindow win = {0};
    if (!name || !name[0]) name = "Peak";
    if (!width) width = 800;
    if (!height) height = 600;
    win.internal = peak_platform_window_open(name, width, height, flags);
    if (!peak_window_sync(&win, NULL, NULL)) return win;
    win.running = 1;
    return win;
}

void
peak_window_close(PeakWindow *win)
{
    win->running = 0;
    peak_platform_window_close(&win->internal);
    win->buffer = NULL;
    win->width = 0;
    win->height = 0;
    win->bufsize = 0;
}

int
peak_window_epoll(PeakWindow *win, PeakEvent *ev)
{
    int got = peak_platform_epoll(&win->internal, ev);
    if (got && ev->type == PEAK_EVENT_WINDOW_RESIZE)
        peak_window_sync(win, NULL, NULL);
    return got;
}

uint32_t *
peak_window_backbuffer(PeakWindow *win, size_t *width, size_t *height)
{
    return peak_window_sync(win, width, height);
}

void
peak_window_clear(PeakWindow *win, float r, float g, float b, float a)
{
    uint32_t c, i;
    if (!win || !win->buffer) return;
#if defined(PEAK_WEB)
    /* ImageData RGBA bytes = LE 0xAABBGGRR */
    c =  (uint32_t)(r * 255.f)
      | ((uint32_t)(g * 255.f) << 8)
      | ((uint32_t)(b * 255.f) << 16)
      | ((uint32_t)(a * 255.f) << 24);
#else
    c = ((uint32_t)(a * 255.f) << 24)
      | ((uint32_t)(r * 255.f) << 16)
      | ((uint32_t)(g * 255.f) << 8)
      |  (uint32_t)(b * 255.f);
#endif
    for (i = 0; i < win->bufsize; i++)
        win->buffer[i] = c;
}

void
peak_window_present(PeakWindow *win)
{
    if (win) peak_platform_window_present(&win->internal);
}

#ifdef PEAK_WEB
#include <emscripten.h>
static void
peak_internal_web_step(void *arg)
{
    PeakWindow *win = arg;
    if (!win->tick(win, win->userdata) || !win->running) {
        win->running = 0;
        emscripten_cancel_main_loop();
    }
}
#endif

void
peak_window_run(PeakWindow *win, int (*peak_tick)(PeakWindow *win, void *userdata), void *userdata)
{
    assert(win && "peak_window_run needs a window");
    assert(peak_tick && "peak_window_run needs tick callback");
    win->tick = peak_tick;
    win->userdata = userdata;
    win->running = 1;
#ifdef PEAK_WEB
    emscripten_set_main_loop_arg(peak_internal_web_step, win, 0, 1);
#else
    while (win->running && win->tick(win, win->userdata));
#endif
}
