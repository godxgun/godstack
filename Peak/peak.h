/* ===========================================================================   
 * PEAK - Copyright @ Vasco Alves - See LICENSE at the end of file.
 * 
 * Platform layer.
 * It just works, don't think about it too much.
 *   
 * =========================================================================== */

#ifndef PEAK_H
#define PEAK_H

#if !defined(_WIN32) && !defined(_WIN64) && !defined(__APPLE__) && !defined(__MACH__) \
 && !defined(__wasm__) && !defined(__wasm32__) && !defined(__wasm64__) && !defined(__EMSCRIPTEN__)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#define PEAK_MAJOR "0"
#define PEAK_MINOR "5"
#define PEAK_PATCH "7"

/* CHANGE LOG 
 * 0.0.0 - @vasco - prototyping
 * 0.1.0 - @vasco - linux x11 that automagically loads X11 DLL
 * 0.1.1 - @vasco - fixed event handling on linux
 * 0.1.2 - @vasco - better handling of windows, if multiple windows becomes necessary in the future
 * 0.2.0 - @vasco - multiple windows, major-ish API changes.
 * 0.3.0 - @vasco - web via emscripten
 * 0.4.0 - @vasco - win32
 * 0.5.0 - @vasco - audio start/stop, s16le pull callback
 * 0.5.1 - @vasco - log, file, time, vulkan extensions
 * 0.5.2 - @vasco - vulkan surface from window
 * 0.5.3 - @vasco - skip XCloseDisplay after vulkan teardown
 * 0.5.4 - @vasco - posix feature test macro before includes
 * 0.5.5 - @vasco - include peak.c; no PEAK_IMPLEMENTATION
 * 0.5.6 - @vasco - digits, tab, backspace, delete; key.code from XLookupString
 * 0.5.7 - @vasco - peak_window_fd; X11 ConnectionNumber
 */

#define NANOS_PER_SEC 1000000000ull

#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#if !( \
    (defined(__STDC__) && __STDC__ == 1 && defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)\
)
#error "Peak requires C99."
#endif

/*
 * Detecting a platform does not mean it's supported.
 * Feel free to use these macros if you need them.
 */
#if defined(__wasm__) || defined(__wasm32__) || defined(__wasm64__) || defined(__EMSCRIPTEN__)
    #define PEAK_WEB
#elif defined(_WIN32) || defined(_WIN64) || defined(__WIN32__) || defined(__TOS_WIN__)
    #define PEAK_WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
#elif defined(__APPLE__) || defined(__MACH__)
    #include <TargetConditionals.h>
    #define PEAK_APPLE
    #if TARGET_OS_IPHONE
        #define PEAK_IOS
    #else
        #define PEAK_MACOS
    #endif
#elif defined(__ANDROID__)
    #define PEAK_ANDROID
    #define PEAK_LINUX
#elif defined(__linux__)
    #define PEAK_LINUX
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__bsdi__) || defined(__DragonFly__)
    #define PEAK_BSD
#endif 

#if defined(PEAK_LINUX) || defined(PEAK_BSD) || defined(PEAK_APPLE)
    #define PEAK_UNIX
#endif

#ifdef PEAK_VULKAN
#if defined(PEAK_LINUX) && !defined(VK_USE_PLATFORM_XLIB_KHR)
#define VK_USE_PLATFORM_XLIB_KHR
#elif defined(PEAK_WIN32) && !defined(VK_USE_PLATFORM_WIN32_KHR)
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#endif

#define PEAK extern

typedef enum PeakLogLevel {
    P_LOG_LEVEL_FATAL = 0,
    P_LOG_LEVEL_ERROR,
    P_LOG_LEVEL_WARN,
    P_LOG_LEVEL_INFO,
    P_LOG_LEVEL_DEBUG,
    P_LOG_LEVEL_TRACE,
    P_COUNT_LOG_LEVEL
} PeakLogLevel;

#define P_PREFIX_LEN 7
static const char *p_prefix[P_COUNT_LOG_LEVEL] = {
    [P_LOG_LEVEL_FATAL] = "[FATAL]",
    [P_LOG_LEVEL_ERROR] = "[ERROR]",
    [P_LOG_LEVEL_WARN]  = "[WARNI]",
    [P_LOG_LEVEL_INFO]  = "[INFOR]",
    [P_LOG_LEVEL_DEBUG] = "[DEBUG]",
    [P_LOG_LEVEL_TRACE] = "[TRACE]",
};

#ifndef P_LOG_WARN_ENABLED
#define P_LOG_WARN_ENABLED 1
#endif
#ifndef P_LOG_INFO_ENABLED
#define P_LOG_INFO_ENABLED 1
#endif
#ifndef P_LOG_DEBUG_ENABLED
#define P_LOG_DEBUG_ENABLED 0
#endif
#ifndef P_LOG_TRACE_ENABLED
#define P_LOG_TRACE_ENABLED 0
#endif

#define PFATAL(message, ...) peak_log_printf(P_LOG_LEVEL_FATAL, message, ##__VA_ARGS__)
#define PERROR(message, ...) peak_log_printf(P_LOG_LEVEL_ERROR, message, ##__VA_ARGS__)
#if P_LOG_WARN_ENABLED == 1
#define PWARN(message, ...)  peak_log_printf(P_LOG_LEVEL_WARN, message, ##__VA_ARGS__)
#else
#define PWARN(message, ...)
#endif
#if P_LOG_INFO_ENABLED == 1
#define PINFO(message, ...)  peak_log_printf(P_LOG_LEVEL_INFO, message, ##__VA_ARGS__)
#else
#define PINFO(message, ...)
#endif
#if P_LOG_DEBUG_ENABLED == 1
#define PDEBUG(message, ...) peak_log_printf(P_LOG_LEVEL_DEBUG, message, ##__VA_ARGS__)
#else
#define PDEBUG(message, ...)
#endif
#if P_LOG_TRACE_ENABLED == 1
#define PTRACE(message, ...) peak_log_printf(P_LOG_LEVEL_TRACE, message, ##__VA_ARGS__)
#else
#define PTRACE(message, ...)
#endif

typedef enum {
    PEAK_KEYMOD_NONE = 0,
    PEAK_KEYMOD_ALT,
    PEAK_KEYMOD_SHIFT,
    PEAK_KEYMOD_CTRL,
    PEAK_KEYMOD_CAPS,
} PeakKeyMod;

typedef enum {
    PEAK_KEY_UNKNOWN = 0,
    PEAK_KEY_UP, PEAK_KEY_DOWN, PEAK_KEY_LEFT, PEAK_KEY_RIGHT,
    PEAK_KEY_SPACE, PEAK_KEY_ESCAPE, PEAK_KEY_ENTER,
    PEAK_KEY_BACKSPACE, PEAK_KEY_TAB, PEAK_KEY_DELETE,
    PEAK_KEY_0, PEAK_KEY_1, PEAK_KEY_2, PEAK_KEY_3, PEAK_KEY_4,
    PEAK_KEY_5, PEAK_KEY_6, PEAK_KEY_7, PEAK_KEY_8, PEAK_KEY_9,
    PEAK_KEY_A, PEAK_KEY_B, PEAK_KEY_C, PEAK_KEY_D, PEAK_KEY_E, PEAK_KEY_F, PEAK_KEY_G, PEAK_KEY_H, PEAK_KEY_I,
    PEAK_KEY_J, PEAK_KEY_K, PEAK_KEY_L, PEAK_KEY_M, PEAK_KEY_N, PEAK_KEY_O, PEAK_KEY_P, PEAK_KEY_Q, PEAK_KEY_R,
    PEAK_KEY_S, PEAK_KEY_T, PEAK_KEY_U, PEAK_KEY_V, PEAK_KEY_W, PEAK_KEY_X, PEAK_KEY_Y, PEAK_KEY_Z,
} PeakKeyCode;

typedef enum {
    PEAK_EVENT_NONE = 0,
    PEAK_EVENT_KEY_DOWN,
    PEAK_EVENT_KEY_UP,
    PEAK_EVENT_WINDOW_CLOSE,
    PEAK_EVENT_WINDOW_RESIZE,
    PEAK_EVENT_POINTER,
    PEAK_EVENT_POINTER_CONNECTED,
    PEAK_EVENT_POINTER_DISCONNECTED,
    PEAK_EVENT_LAST
} PeakEventType;

typedef enum {
    PEAK_POINTER_MOVED = 0,
    PEAK_POINTER_PRESSED,
    PEAK_POINTER_RELEASED
} PeakPointerState;

typedef enum {
    PEAK_POINTER_LEFT = 0,
    PEAK_POINTER_RIGHT,
    PEAK_POINTER_MIDDLE,
    PEAK_POINTER_TOUCH,
} PeakPointerType;

typedef struct {
    PeakEventType type;
    union {
        struct { PeakKeyCode key; PeakKeyMod mod; uint32_t code; } key;
        struct { uint32_t width, height; } resize;
        struct { PeakPointerState state; PeakPointerType type; float x, y; } pointer;
    };
} PeakEvent;

/* Platform handle. Opaque pointer; PeakWindow is complete in this header. */
typedef struct peak_window_internal_t {
    void *w;
} PeakWindowInternal;

typedef struct PeakWindow {
    PeakWindowInternal internal;
    int (*tick)(struct PeakWindow *win, void *userdata);
    void *userdata;
    uint32_t *buffer;
    uint32_t width;
    uint32_t height;
    uint32_t bufsize;
    uint16_t *audio; /* LRLRLR */
    int running;
} PeakWindow;

/* Initialize the platform. */
PEAK int  peak_init(void); // Initialize platform context and load necessary DLLs.
PEAK void peak_quit(void); // Close platform context.

/* Do sexy stuff with the windows. */
PEAK PeakWindow peak_window_open(const char *name, uint32_t width, uint32_t height, uint32_t flags); // Open a window.
PEAK void       peak_window_close(PeakWindow *window); // Close a window.
PEAK void       peak_window_run(PeakWindow *win, int (*peak_tick)(PeakWindow *win, void *userdata), void *userdata); // Hijacking the main loop makes life easier on platforms like web
PEAK int        peak_window_epoll(PeakWindow *win, PeakEvent *ev); // Poll a window for events.
PEAK int        peak_window_fd(PeakWindow *win); // Display connection fd, or -1.
PEAK uint32_t*  peak_window_backbuffer(PeakWindow *win, size_t *width, size_t *height); // Get the windows backbuffer.
PEAK void       peak_window_clear(PeakWindow *win, float r, float g, float b, float a); // Clear window to color.
PEAK void       peak_window_present(PeakWindow *win); // Present window backbuffer.

/* Play some tunes. */
PEAK int peak_audio_start(uint32_t channels, uint32_t rate, void (*fill)(int16_t *out, size_t frames, void *userdata), void *userdata); // Device pulls interleaved s16le
PEAK void peak_audio_stop(void); // Stop audio.
                        
/* Time */
PEAK uint64_t peak_get_time(void); // Get time in nanoseconds.
PEAK void peak_sleep_ns(int64_t ns); // Sleep for nanoseconds!!!

/* File */
PEAK int peak_file_exists(const char *path); // Does this file exist?
PEAK void *peak_file_alloc(const char *path, unsigned long *buf_size); // Allocate an entire file.

/* Graphics API bull... */
PEAK const char **peak_vulkan_get_extensions(uint32_t *count); // Get vulkan extensions.
PEAK int peak_vulkan_create_surface(PeakWindow *win, void *instance, const void *allocator, void *out_surface); // Create a vulkan surface for a window. Needs PEAK_VULKAN.

/* Debug your amazing code. */
PEAK void peak_log_printf(PeakLogLevel level, const char *src, ...); // Printf with log level.
PEAK void *peak_debug_malloc_impl(size_t size, const char *file, int line, const char *func); // Malloc for debuggin.
PEAK void peak_debug_free_impl(void *ptr, const char *file, int line, const char *func); // Free for debugging.
PEAK void *peak_debug_realloc_impl(void *ptr, size_t size, const char *file, int line, const char *func); // Realloc for debugging.
PEAK void peak_debug_memory_report(void); // Report memory.

#endif // PEAK_H


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
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/
