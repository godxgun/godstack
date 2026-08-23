/* ===========================================================================   
 * PEAK - Copyright @ Vasco Alves - See LICENSE at the end of file.
 * 
 * Platform layer.
 * It just works, don't think about it too much.
 *   
 * =========================================================================== */

#ifndef PEAK_H
#define PEAK_H

#define PEAK_MAJOR "0"
#define PEAK_MINOR "1"
#define PEAK_PATCH "1"

/* CHANGE LOG 
 * 0.0.0 - @vasco - prototyping
 * 0.1.0 - @vasco - linux x11 that automagically loads X11 DLL
 * 0.1.1 - @vasco - fixed event handling on linux
 *
 * TODO: demo that stress tests the whole API
 */

#include <assert.h>
#include <stdarg.h>
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
#elif defined(__linux__)
    #define PEAK_LINUX
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__bsdi__) || defined(__DragonFly__)
    #define PEAK_BSD
#elif defined(__ANDROID__)
    #define PEAK_ANDROID
    #define PEAK_LINUX
#elif defined(__linux__)
    #define PEAK_LINUX
#endif 

#if defined(PEAK_LINUX) || defined(PEAK_BSD) || defined(PEAK_APPLE)
    #define PEAK_UNIX
    #if !defined(PEAK_APPLE) && !defined(_POSIX_C_SOURCE)
        #define _POSIX_C_SOURCE 200112L
    #endif
#endif

#ifdef PEAK_WEB
    #define PEAK static inline
#else
    #define PEAK extern
#endif

typedef enum {
    PEAK_KEYMOD_ALT = 0,
    PEAK_KEYMOD_SHIFT,
    PEAK_KEYMOD_CTRL,
    PEAK_KEYMOD_CAPS,
} PeakKeyMod;

typedef enum {
    PEAK_KEY_UNKNOWN = 0,
    PEAK_KEY_UP, PEAK_KEY_DOWN, PEAK_KEY_LEFT, PEAK_KEY_RIGHT,
    PEAK_KEY_SPACE, PEAK_KEY_ESCAPE, PEAK_KEY_ENTER,
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
} PeakEvenType;

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
    PeakEvenType type;
    union {
        struct { PeakKeyCode key; PeakKeyMod mod; } key;
        struct { uint32_t width, height; } resize;
        struct { PeakPointerState state; PeakPointerType type; float x, y; } pointer;
    };
} PeakEvent;

/* Public API */
PEAK void peak_init(void); // open a window and initialize graphics. called automatically if using peak_setup().
PEAK void peak_quit(void); // close the window and release resources. called automatically if using peak_setup().
PEAK bool peak_poll_events(PeakEvent *ev); // Poll the next event from the window queue. Returns true if an event was retrieved.
PEAK void peak_extensions(void); // retrieve gpu window surface extensions for native api access (e.g., vulkan/opengl).
PEAK void peak_blit(int offset_x, int offset_y, const uint32_t *rgba, size_t width, size_t height); // blit rgba pixels to the screen.
PEAK void peak_clip(int x, int y, size_t w, size_t h); // restrict all subsequent rendering operations to this bounding rectangle.
PEAK void peak_clip_reset(void); // reset clipping plane to window size.
PEAK void peak_draw_rectangle(int x, int y, size_t w, size_t h, uint32_t color); // draw a solid-color filled rectangle.
PEAK void peak_draw_rectangle_gradient(int x, int y, size_t w, size_t h, uint32_t c0, uint32_t c1, uint32_t c2, uint32_t c3); // fill rectangle interpolating four corner colors in clock-wise order.
PEAK void peak_draw_triangle(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t color); // draw a solid-color filled triangle.
PEAK void peak_draw_triangle_gradient(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t c0, uint32_t c1, uint32_t c2); // fill triangle with color interpolation clock-wise across its three vertices.
PEAK void peak_draw_line(int x0, int y0, int x1, int y1, size_t width, uint32_t color); // draw a line between two points.
PEAK void peak_draw_line_gradient(int x0, int y0, int x1, int y1, size_t width, uint32_t c0, uint32_t c1); // draw a line with a color gradient.

/* Implemented by the platform */
PEAK void peak_platform_window_open();
PEAK void peak_platform_window_close();
PEAK uint32_t *peak_platform_window_buffer(size_t *width, size_t *height);
PEAK bool peak_platform_epoll(PeakEvent *ev);

/*
 * He was wipping up amazing foods, like making happiness.
 * Actual happiness?
 * Actual happiness and joy. 
 * Oh the smell, it was so divine.
 * He was making happiness!?
 * Happiness in the kettle!
 */
#ifdef PEAK_REPLACE_MAIN
enum PeakReturn { PEAK_CONTINUE = 0, PEAK_STOP = 1 };
extern int  peak__main(int argc, char**argv);
extern void peak_events(PeakEvent ev);
extern void peak_tick();
static bool running = true;
static inline void peak_stop() { running = false; }
#ifndef PEAK_WEB
#define main(...)\
main(int argc, char **argv) {\
    assert(peak_events != 0 && "Declare peak_events() to handle window events.");\
    assert(peak_tick != 0 && "Declare peak_tick() to handle each frame ticks.");\
    peak_init();\
    int ret = peak__main(argc, argv);\
    if (ret != PEAK_CONTINUE) return ret;\
    PeakEvent ev;\
    while (running) {\
        while (running && peak_poll_events(&ev)) {\
            peak_events(ev);\
        }\
        peak_tick();\
    }\
    peak_quit();\
}\
int peak__main(__VA_ARGS__)
#else
#include <emscripten.h>
/*
 * He was wipping up agony in the kettle.
 * Actual pain?
 * Boiling hatred in the kettle.
 * The smell, it's pain.
 * Boiling anger!?
 * Yes, boiling anger.
 */

static inline void
peak__emscripten_loop_step(void *arg) 
{
    (void)arg;
    PeakEvent ev;
    while (peak_poll_events(&ev)) {
        peak_events(ev);
    }
    
    if (running) {
        peak_tick();
    } else {
        emscripten_cancel_main_loop();
        peak_quit();
    }
}

#define main(...)\
    main(int argc, char **argv) {\
        _Static_assert(peak_events != 0, "When using peak_setup, declare peak_events() to handle window events.");\
        _Static_assert(peak_tick != 0, "When using peak_setup, declare peak_tick() to handle frame ticks.");\
        peak_init();\
        int ret = peak__main(argc, argv);\
        if (ret != PEAK_CONTINUE) return ret;\
        emscripten_set_main_loop_arg(peak__emscripten_loop_step, NULL, 0, 1);\
        return 0;\
    }\
    int peak__main(__VA_ARGS__)
#endif
#endif // PEAK_DONT_REPLACE_MAIN
        
#endif // PEAK_H

#ifdef PEAK_IMPLEMENTATION 
#undef PEAK_IMPLEMENTATION

#if defined(PEAK_WIN32)
#include "p_win32.c" // <REPLACE>
#elif defined(PEAK_LINUX)
#include "p_linux.c" // <REPLACE>
#elif defined(PEAK_WEB)
#include "p_emscripten.c" // <REPLACE>
#endif

#include "peak.c" // <REPLACE>

#endif // PEAK_IMPLEMENTATION


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
