#include "peak.h"

#include <stdio.h>
#include <stdlib.h>

#if defined(PEAK_WIN32) || defined(PEAK_WEB) || defined(PEAK_MACOS)
#define PEAK_Q 64

typedef struct {
    unsigned h, n;
    PeakEvent e[PEAK_Q];
} PeakQ;

static void
peak_q_push(PeakQ *q, PeakEvent ev)
{
    if (!q || q->n == PEAK_Q)
        return;
    q->e[(q->h + q->n++) % PEAK_Q] = ev;
}

static int
peak_q_pop(PeakQ *q, PeakEvent *ev)
{
    if (!q || !q->n)
        return 0;
    *ev = q->e[q->h];
    q->h = (q->h + 1) % PEAK_Q;
    q->n--;
    return 1;
}
#endif

#if defined(PEAK_WIN32)
#include "p_win32.c"
#elif defined(PEAK_LINUX)
#include "p_linux.c"
#elif defined(PEAK_MACOS)
#include "p_macos.c"
#elif defined(PEAK_WEB)
#include "p_emscripten.c"
#endif

#include "p_log.c"

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
    peak_audio_stop();
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

int
peak_window_fd(PeakWindow *win)
{
    if (!win)
        return -1;
    return peak_platform_fd(&win->internal);
}

int
peak_window_pending(PeakWindow *win)
{
    if (!win)
        return 0;
    return peak_platform_pending(&win->internal);
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

int
peak_audio_start(uint32_t channels, uint32_t rate, void (*fill)(int16_t *out, size_t frames, void *userdata), void *userdata)
{
    if (!fill || !channels || !rate)
        return 0;
    peak_audio_stop();
    return peak_platform_audio_start(channels, rate, fill, userdata);
}

void
peak_audio_stop(void)
{
    peak_platform_audio_stop();
}

uint64_t
peak_get_time(void)
{
    return peak_platform_get_time();
}

void
peak_sleep_ns(int64_t ns)
{
    peak_platform_sleep_ns(ns);
}

int
peak_file_exists(const char *path)
{
    FILE *f;
    if (!path) return 0;
    f = fopen(path, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

void *
peak_file_alloc(const char *path, unsigned long *buf_size)
{
    FILE *f;
    long n;
    void *p;
    if (!path) return NULL;
    f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    n = ftell(f);
    if (n < 0) { fclose(f); return NULL; }
    rewind(f);
    p = malloc((size_t)n + (n == 0));
    if (!p) { fclose(f); return NULL; }
    if (n && fread(p, 1, (size_t)n, f) != (size_t)n) {
        free(p);
        fclose(f);
        return NULL;
    }
    fclose(f);
    if (buf_size) *buf_size = (unsigned long)n;
    return p;
}

const char **
peak_vulkan_get_extensions(uint32_t *count)
{
    return peak_platform_vulkan_get_extensions(count);
}

int
peak_vulkan_create_surface(PeakWindow *win, void *instance, const void *allocator, void *out_surface)
{
    if (!win || !instance || !out_surface) return 0;
    return peak_platform_vulkan_create_surface(&win->internal, instance, allocator, out_surface);
}
