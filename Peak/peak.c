#include "peak.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(PEAK_WIN32)
#include <malloc.h>
#endif

#if defined(PEAK_WIN32) || defined(PEAK_WEB) || defined(PEAK_MACOS) || defined(PEAK_LINUX)
#define PEAK_Q 64

typedef struct {
    unsigned h, n;
    PeakEvent e[PEAK_Q];
} PeakQ;

static int
peak_q_motion(PeakEvent ev)
{
    return ev.type == PEAK_EVENT_POINTER && ev.pointer.state == PEAK_POINTER_MOVED;
}

static void
peak_q_push(PeakQ *q, PeakEvent ev)
{
    if (!q)
        return;
    /* Motion floods must not eat a button release. A dropped release leaves
     * a drag running, so a later click changes the selection. */
    if (q->n == PEAK_Q) {
        if (peak_q_motion(ev))
            return;
        q->h = (q->h + 1) % PEAK_Q;
        q->n--;
    }
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

#define PEAK_CLIP_MAX (1024u * 1024u)
#define PEAK_HOST_ALIGN 16u

enum {
    PEAK_STORE_OWN0, PEAK_STORE_OWN1, PEAK_STORE_PASTE,
    PEAK_STORE_TEXT, PEAK_STORE_DROP, PEAK_STORE_INCR,
    PEAK_STORE_CONVERT, PEAK_STORE_RECV0, PEAK_STORE_RECV1,
    PEAK_STORE_DRAG, PEAK_STORE_COUNT
};

/* Linux legacy windows retain lifetime metadata in their existing allocation. */
typedef struct PeakLegacySlot {
    struct PeakLegacySlot *next;
    uint64_t generation;
} PeakLegacySlot;
#define PEAK_LEGACY_HEADER ((sizeof(PeakLegacySlot) + PEAK_HOST_ALIGN - 1) & ~(size_t)(PEAK_HOST_ALIGN - 1))

struct PeakCtx {
    PeakLegacySlot *legacy_windows;
    struct {
        unsigned char *base, *windows;
        size_t size, stride, max_windows, clipboard_capacity, transfer_capacity;
        char *store[PEAK_STORE_COUNT];
        unsigned char recv_busy[2];
    } host;
    struct {
        char *own[2];
        size_t own_n[2];
        char *paste;
        size_t paste_n;
        int paste_ready;
        PeakClip paste_which;
    } clip;
    struct {
        char *text;
        size_t text_n;
        int text_ready;
        char *drop;
        size_t drop_n;
        int drop_ready;
    } xfer;
};
static PeakCtx *peak_active;
static PeakCtx peak_legacy;
/* Internal dispatch is single-runtime; all public entry points validate first. */
#define peak_host (peak_active->host)
#define peak_clip (peak_active->clip)
#define peak_xfer (peak_active->xfer)
#define PEAK_TRANSFER_CAP (peak_active ? peak_host.transfer_capacity : PEAK_CLIP_MAX)
#define PEAK_OWN_CAP (peak_active ? peak_host.clipboard_capacity : PEAK_CLIP_MAX)
static int peak_initialized;
static uint64_t peak_generation;
static int peak_window_valid(PeakWindow *win);

static size_t peak_host_window_size(void);
#if defined(PEAK_LINUX)
static void *peak_host_window_alloc(size_t size);
static void peak_host_window_free(void *p);
static void peak_host_transfer_free(void *p);
static char *peak_host_recv_alloc(void);
#endif

static int
peak_clip_own_store(PeakClip which, const char *utf8, size_t n)
{
    char *p;

    if ((unsigned)which > 1)
        return 0;
    if (n && !utf8)
        return 0;
    if (n > PEAK_OWN_CAP)
        n = PEAK_OWN_CAP;
    p = peak_host.base ? peak_clip.own[which] : realloc(peak_clip.own[which], n ? n : 1);
    if (!p)
        return 0;
    if (n)
        memcpy(p, utf8, n);
    peak_clip.own[which] = p;
    peak_clip.own_n[which] = n;
    return 1;
}

static int
peak_clip_own_get(PeakClip which, const char **p, size_t *n)
{
    if ((unsigned)which > 1 || !p || !n)
        return 0;
    *p = peak_clip.own[which] ? peak_clip.own[which] : "";
    *n = peak_clip.own_n[which];
    return 1;
}

static void
peak_clip_paste_store(PeakClip which, const char *utf8, size_t n)
{
    char *p;

    if (n > PEAK_TRANSFER_CAP)
        n = PEAK_TRANSFER_CAP;
    if (n && !utf8)
        n = 0;
    p = peak_host.base ? peak_clip.paste : realloc(peak_clip.paste, n ? n : 1);
    if (!p) {
        peak_clip.paste_ready = 0;
        return;
    }
    if (n)
        memcpy(p, utf8, n);
    peak_clip.paste = p;
    peak_clip.paste_n = n;
    peak_clip.paste_which = which;
    peak_clip.paste_ready = 1;
}

static void
peak_text_store(const char *utf8, size_t n)
{
    char *p;

    if (n > PEAK_TRANSFER_CAP)
        n = PEAK_TRANSFER_CAP;
    if (n && !utf8)
        n = 0;
    p = peak_host.base ? peak_xfer.text : realloc(peak_xfer.text, n ? n : 1);
    if (!p) {
        peak_xfer.text_ready = 0;
        return;
    }
    if (n)
        memcpy(p, utf8, n);
    peak_xfer.text = p;
    peak_xfer.text_n = n;
    peak_xfer.text_ready = 1;
}

static void
peak_drop_store(const char *utf8, size_t n)
{
    char *p;

    if (n > PEAK_TRANSFER_CAP)
        n = PEAK_TRANSFER_CAP;
    if (n && !utf8)
        n = 0;
    p = peak_host.base ? peak_xfer.drop : realloc(peak_xfer.drop, n ? n : 1);
    if (!p) {
        peak_xfer.drop_ready = 0;
        return;
    }
    if (n)
        memcpy(p, utf8, n);
    peak_xfer.drop = p;
    peak_xfer.drop_n = n;
    peak_xfer.drop_ready = 1;
}

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

static size_t
peak_host_window_size(void)
{
#if defined(PEAK_LINUX) && defined(PEAK_VULKAN)
    size_t n = sizeof(struct peak_linux_win);
    if (n < sizeof(struct peak_wayland_win))
        n = sizeof(struct peak_wayland_win);
    return (n + PEAK_HOST_ALIGN - 1) & ~(size_t)(PEAK_HOST_ALIGN - 1);
#else
    return 0;
#endif
}

static const PeakParams peak_defaults = {1, PEAK_CLIP_MAX, PEAK_CLIP_MAX};

static size_t
peak_store_bytes(const PeakParams *params, size_t i)
{
    size_t cap = i < PEAK_STORE_PASTE ? params->clipboard_capacity : params->transfer_capacity;
    if (i == PEAK_STORE_CONVERT) cap *= 2;
    return (cap + PEAK_HOST_ALIGN) & ~(size_t)(PEAK_HOST_ALIGN - 1);
}

size_t
peak_memory(const PeakParams *params)
{
    size_t stride = peak_host_window_size(), total, bytes, i;
    if (!params) params = &peak_defaults;
    /* X11 byte counts and conversion lengths use signed int. */
    if (!stride || !params->max_windows || params->max_windows > 8 ||
        !params->clipboard_capacity || !params->transfer_capacity ||
        params->clipboard_capacity > INT_MAX || params->transfer_capacity > INT_MAX / 2)
        return 0;
    total = (sizeof(PeakCtx) + PEAK_HOST_ALIGN - 1) & ~(size_t)(PEAK_HOST_ALIGN - 1);
    bytes = params->max_windows * (stride + PEAK_HOST_ALIGN);
    if (total > SIZE_MAX - bytes) return 0;
    total += bytes;
    for (i = 0; i < PEAK_STORE_COUNT; i++) {
        bytes = peak_store_bytes(params, i);
        if (total > SIZE_MAX - bytes) return 0;
        total += bytes;
    }
    return total;
}

PeakCtx *
peak_place_in_memory_and_init(void *buf, size_t size, const PeakParams *params)
{
    PeakParams profile = params ? *params : peak_defaults;
    size_t need = peak_memory(&profile), i;
    unsigned char *p;
    if (!need || !buf || (uintptr_t)buf % PEAK_HOST_ALIGN || size < need || peak_active)
        return NULL;
    /* Preserve parameters even when the caller stores them in the backing. */
    params = &profile;
    memset(buf, 0, need);
    peak_active = buf;
    peak_host.base = buf;
    peak_host.size = need;
    peak_host.max_windows = params->max_windows;
    peak_host.clipboard_capacity = params->clipboard_capacity;
    peak_host.transfer_capacity = params->transfer_capacity;
    peak_host.stride = peak_host_window_size() + PEAK_HOST_ALIGN;
    peak_host.windows = (unsigned char *)buf + ((sizeof(PeakCtx) + PEAK_HOST_ALIGN - 1) & ~(size_t)(PEAK_HOST_ALIGN - 1));
    p = peak_host.windows + peak_host.stride * params->max_windows;
    for (i = 0; i < PEAK_STORE_COUNT; i++) {
        peak_host.store[i] = (char *)p;
        p += peak_store_bytes(params, i);
    }
    assert((size_t)(p - peak_host.base) == need);
    peak_clip.own[0] = peak_host.store[PEAK_STORE_OWN0];
    peak_clip.own[1] = peak_host.store[PEAK_STORE_OWN1];
    peak_clip.paste = peak_host.store[PEAK_STORE_PASTE];
    peak_xfer.text = peak_host.store[PEAK_STORE_TEXT];
    peak_xfer.drop = peak_host.store[PEAK_STORE_DROP];
    return peak_active;
}

static int
peak_window_valid(PeakWindow *win)
{
    size_t i;
    if (!win || !peak_active || win->ctx != peak_active || !win->internal.w)
        return 0;
    if (!peak_host.base) {
#if defined(PEAK_LINUX)
        PeakLegacySlot *slot;
        for (slot = peak_active->legacy_windows; slot; slot = slot->next)
            if (win->internal.w == (unsigned char *)slot + PEAK_LEGACY_HEADER)
                return win->generation && win->generation == slot->generation;
        return 0;
#else
        /* Other hosts retain legacy handle ownership, without copy validation. */
        return win->generation != 0;
#endif
    }
    for (i = 0; i < peak_host.max_windows; i++) {
        unsigned char *slot = peak_host.windows + i * peak_host.stride;
        if (win->internal.w == slot + PEAK_HOST_ALIGN)
            return win->generation && win->generation == *(uint64_t *)slot;
    }
    return 0;
}

#if defined(PEAK_LINUX)
static void *
peak_host_window_alloc(size_t size)
{
    size_t i;
    unsigned char *p;
    if (!peak_host.base) {
        PeakLegacySlot *slot;
        if (size > SIZE_MAX - PEAK_LEGACY_HEADER || !(slot = calloc(1, PEAK_LEGACY_HEADER + size)))
            return NULL;
        if (++peak_generation == 0) ++peak_generation;
        slot->generation = peak_generation;
        slot->next = peak_active->legacy_windows;
        peak_active->legacy_windows = slot;
        return (unsigned char *)slot + PEAK_LEGACY_HEADER;
    }
    if (size > peak_host.stride - PEAK_HOST_ALIGN)
        return NULL;
    for (i = 0; i < peak_host.max_windows; i++) {
        p = peak_host.windows + i * peak_host.stride;
        if (!*(uint64_t *)p) {
            memset(p, 0, peak_host.stride);
            if (++peak_generation == 0) ++peak_generation;
            *(uint64_t *)p = peak_generation;
            return p + PEAK_HOST_ALIGN;
        }
    }
    return NULL;
}

static void
peak_host_window_free(void *p)
{
    if (!p)
        return;
    if (peak_host.base) {
        unsigned char *slot = (unsigned char *)p - PEAK_HOST_ALIGN;
        memset(slot, 0, peak_host.stride);
    } else {
        PeakLegacySlot **link = &peak_active->legacy_windows;
        while (*link) {
            PeakLegacySlot *slot = *link;
            if (p == (unsigned char *)slot + PEAK_LEGACY_HEADER) {
                *link = slot->next;
                free(slot);
                return;
            }
            link = &slot->next;
        }
    }
}

static void
peak_host_transfer_free(void *p)
{
    if (!p)
        return;
    if (peak_host.base) {
        if (p == peak_host.store[PEAK_STORE_RECV0]) peak_host.recv_busy[0] = 0;
        if (p == peak_host.store[PEAK_STORE_RECV1]) peak_host.recv_busy[1] = 0;
    } else {
        free(p);
    }
}

static char *
peak_host_recv_alloc(void)
{
    size_t i;
    for (i = 0; i < 2; i++) {
        if (!peak_host.recv_busy[i]) {
            peak_host.recv_busy[i] = 1;
            return peak_host.store[PEAK_STORE_RECV0 + i];
        }
    }
    return NULL;
}
#endif

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

PeakCtx *
peak_init_legacy(void)
{
    if (peak_active) return NULL;
    memset(&peak_legacy, 0, sizeof peak_legacy);
    peak_active = &peak_legacy;
    peak_host.clipboard_capacity = peak_host.transfer_capacity = PEAK_CLIP_MAX;
    return peak_active;
}

void
peak_quit(PeakCtx *ctx)
{
    size_t i;
    int placed;
    if (!ctx || ctx != peak_active) return;
    placed = peak_host.base != NULL;
    peak_audio_stop();
    if (peak_host.base) {
        for (i = 0; i < peak_host.max_windows; i++) {
            unsigned char *slot = peak_host.windows + i * peak_host.stride;
            if (*(uint64_t *)slot) {
                PeakWindowInternal intern = {slot + PEAK_HOST_ALIGN};
                peak_platform_window_close(&intern);
            }
        }
    }
#if defined(PEAK_LINUX)
    while (peak_active->legacy_windows) {
        PeakWindowInternal intern = {(unsigned char *)peak_active->legacy_windows + PEAK_LEGACY_HEADER};
        peak_platform_window_close(&intern);
    }
#endif
    if (peak_initialized) peak_platform_quit();
#if defined(PEAK_LINUX)
    if (peak_host.base) {
        peak_host_transfer_free(peak_clip_incr);
        peak_clip_incr = NULL;
        peak_clip_incr_n = 0;
        peak_clip_incr_on = peak_clip_req_on = peak_clip_req_xa = 0;
        peak_clip_req_window = None;
        peak_clip_req_owner = NULL;
    }
#endif
    if (peak_host.base) {
        memset(&peak_clip, 0, sizeof peak_clip);
        memset(&peak_xfer, 0, sizeof peak_xfer);
        memset(&peak_host, 0, sizeof peak_host);
    }
    if (!placed) {
        free(peak_clip.own[0]); free(peak_clip.own[1]); free(peak_clip.paste);
        free(peak_xfer.text); free(peak_xfer.drop);
    }
    peak_initialized = 0;
    peak_active = NULL;
}

PeakWindow
peak_window_open(PeakCtx *ctx, const char *name, uint32_t width, uint32_t height, uint32_t flags)
{
    PeakWindow win = {0};
    if (!ctx || ctx != peak_active) return win;
    if (!name || !name[0]) name = "Peak";
    if (!width) width = 800;
    if (!height) height = 600;
    /* Implicit native initialization must obey the same placement barrier. */
    if (!peak_initialized && !(peak_initialized = peak_platform_init())) {
        peak_platform_quit();
        return win;
    }
    win.internal = peak_platform_window_open(name, width, height, flags);
    peak_window_sync(&win, NULL, NULL);
    if (!win.internal.w) return win;
    win.ctx = ctx;
    if (peak_host.base) win.generation = *(uint64_t *)((unsigned char *)win.internal.w - PEAK_HOST_ALIGN);
#if defined(PEAK_LINUX)
    else win.generation = ((PeakLegacySlot *)((unsigned char *)win.internal.w - PEAK_LEGACY_HEADER))->generation;
#else
    else { if (++peak_generation == 0) ++peak_generation; win.generation = peak_generation; }
#endif
    win.running = 1;
    return win;
}

void
peak_window_close(PeakWindow *win)
{
    if (!peak_window_valid(win)) return;
    win->running = 0;
    win->generation = 0;
    peak_platform_window_close(&win->internal);
    win->buffer = NULL;
    win->width = 0;
    win->height = 0;
    win->bufsize = 0;
}

int
peak_window_epoll(PeakWindow *win, PeakEvent *ev)
{
    if (!peak_window_valid(win)) return 0;
    int got = peak_platform_epoll(&win->internal, ev);
    if (got && ev->type == PEAK_EVENT_WINDOW_RESIZE)
        peak_window_sync(win, NULL, NULL);
    return got;
}

int
peak_window_fd(PeakWindow *win)
{
    if (!peak_window_valid(win)) return -1;
    if (!win)
        return -1;
    return peak_platform_fd(&win->internal);
}

int
peak_window_pending(PeakWindow *win)
{
    if (!peak_window_valid(win)) return 0;
    if (!win)
        return 0;
    return peak_platform_pending(&win->internal);
}

uint32_t *
peak_window_backbuffer(PeakWindow *win, size_t *width, size_t *height)
{
    if (!peak_window_valid(win)) return NULL;
    return peak_window_sync(win, width, height);
}

void
peak_window_clear(PeakWindow *win, float r, float g, float b, float a)
{
    if (!peak_window_valid(win)) return;
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
    if (!peak_window_valid(win)) return;
    if (win) peak_platform_window_present(&win->internal);
}

void
peak_window_set_title(PeakWindow *win, const char *name)
{
    if (!peak_window_valid(win)) return;
    if (!win || !name)
        return;
    peak_platform_window_set_title(&win->internal, name);
}

void
peak_window_set_class(PeakWindow *win, const char *name)
{
    if (!peak_window_valid(win)) return;
    if (!win || !name || !name[0])
        return;
    peak_platform_window_set_class(&win->internal, name);
}

void
peak_window_set_opacity(PeakWindow *win, uint8_t alpha)
{
    if (!peak_window_valid(win)) return;
    if (!win)
        return;
    peak_platform_window_set_opacity(&win->internal, alpha);
}

void
peak_window_set_size(PeakWindow *win, uint32_t width, uint32_t height)
{
    if (!peak_window_valid(win)) return;
    if (!win || !width || !height)
        return;
    peak_platform_window_set_size(&win->internal, width, height);
    peak_window_sync(win, NULL, NULL);
}

void
peak_window_fullscreen(PeakWindow *win, int on)
{
    if (!peak_window_valid(win)) return;
    if (!win)
        return;
    peak_platform_window_fullscreen(&win->internal, on);
}

void
peak_window_cursor(PeakWindow *win, int on)
{
    if (!peak_window_valid(win)) return;
    if (!win)
        return;
    peak_platform_window_cursor(&win->internal, on);
}

void
peak_window_cursor_shape(PeakWindow *win, int shape)
{
    if (!peak_window_valid(win)) return;
    if (!win)
        return;
    peak_platform_window_cursor_shape(&win->internal, shape);
}

void
peak_window_pointer_relative(PeakWindow *win, int on)
{
    if (!peak_window_valid(win)) return;
    if (!win)
        return;
    peak_platform_window_pointer_relative(&win->internal, on);
}

float
peak_window_scale(PeakWindow *win)
{
    if (!peak_window_valid(win)) return 1.0f;
    if (!win)
        return 1.f;
    return peak_platform_window_scale(&win->internal);
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
    if (!peak_window_valid(win)) return;
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
    if ((peak_active && peak_host.base) || !fill || !channels || !rate)
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

int
peak_file_write(const char *path, const void *buf, size_t n)
{
    FILE *f;

    if (!path || (n && !buf))
        return 0;
    f = fopen(path, "wb");
    if (!f)
        return 0;
    if (n && fwrite(buf, 1, n, f) != n) {
        fclose(f);
        return 0;
    }
    fclose(f);
    return 1;
}

const char **
peak_vulkan_get_extensions(uint32_t *count)
{
    return peak_platform_vulkan_get_extensions(count);
}

int
peak_vulkan_create_surface(PeakWindow *win, void *instance, const void *allocator, void *out_surface)
{
    if (!peak_window_valid(win) || !instance || !out_surface) return 0;
    return peak_platform_vulkan_create_surface(&win->internal, instance, allocator, out_surface);
}

int
peak_clip_set(PeakCtx *ctx, PeakWindow *win, PeakClip which, const char *utf8, size_t n)
{
    if (!ctx || ctx != peak_active || (win && !peak_window_valid(win))) return 0;
    if (which != PEAK_CLIP_CLIPBOARD && which != PEAK_CLIP_PRIMARY)
        return 0;
    if (n && !utf8)
        return 0;
    if (n > PEAK_OWN_CAP)
        n = PEAK_OWN_CAP;
    if (!peak_clip_own_store(which, utf8, n))
        return 0;
#if defined(PEAK_WIN32) || defined(PEAK_MACOS) || defined(PEAK_WEB)
    if (!peak_clip_own_store(which == PEAK_CLIP_PRIMARY ? PEAK_CLIP_CLIPBOARD : PEAK_CLIP_PRIMARY, utf8, n))
        return 0;
#endif
    if (!win)
        return 1;
    return peak_platform_clip_set(&win->internal, which, utf8, n);
}

int
peak_clip_request(PeakCtx *ctx, PeakWindow *win, PeakClip which)
{
    if (!ctx || ctx != peak_active || (win && !peak_window_valid(win))) return 0;
    const char *p;
    size_t n;

    if (which != PEAK_CLIP_CLIPBOARD && which != PEAK_CLIP_PRIMARY)
        return 0;
    if (!win) {
        if (!peak_clip_own_get(which, &p, &n))
            return 0;
        peak_clip_paste_store(which, p, n);
        return 1;
    }
    return peak_platform_clip_request(&win->internal, which);
}

int
peak_clip_take(PeakCtx *ctx, PeakWindow *win, char *dst, size_t cap, size_t *n)
{
    if (!ctx || ctx != peak_active || (win && !peak_window_valid(win))) return 0;
    size_t c;

    (void)win;
    if (!peak_clip.paste_ready)
        return 0;
    peak_clip.paste_ready = 0;
    c = peak_clip.paste_n;
    if (c > cap)
        c = cap;
    if (dst && c)
        memcpy(dst, peak_clip.paste, c);
    if (n)
        *n = c;
    return 1;
}

int
peak_text_take(PeakCtx *ctx, PeakWindow *win, char *dst, size_t cap, size_t *n)
{
    if (!ctx || ctx != peak_active || (win && !peak_window_valid(win))) return 0;
    size_t c;

    (void)win;
    if (!peak_xfer.text_ready)
        return 0;
    peak_xfer.text_ready = 0;
    c = peak_xfer.text_n;
    if (c > cap)
        c = cap;
    if (dst && c)
        memcpy(dst, peak_xfer.text, c);
    if (n)
        *n = c;
    return 1;
}

int
peak_drop_drag(PeakCtx *ctx, PeakWindow *win, const char *utf8, size_t n)
{
    if (!ctx || ctx != peak_active || (win && !peak_window_valid(win))) return 0;
    if (!win || (n && !utf8))
        return 0;
    if (n > PEAK_TRANSFER_CAP)
        n = PEAK_TRANSFER_CAP;
    return peak_platform_drop_drag(&win->internal, utf8, n);
}

int
peak_drop_take(PeakCtx *ctx, PeakWindow *win, char *dst, size_t cap, size_t *n)
{
    if (!ctx || ctx != peak_active || (win && !peak_window_valid(win))) return 0;
    size_t c;

    (void)win;
    if (!peak_xfer.drop_ready)
        return 0;
    peak_xfer.drop_ready = 0;
    c = peak_xfer.drop_n;
    if (c > cap)
        c = cap;
    if (dst && c)
        memcpy(dst, peak_xfer.drop, c);
    if (n)
        *n = c;
    return 1;
}
