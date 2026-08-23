/* ===========================================================================   
 * PODIUM - Kitchen Sink - Copyright (c) 2026 Vasco Alves
 * ---------------------------------------------------------------------------   
 * To stand on the shoulders of giants... Giants must stand still!"
 * ---------------------------------------------------------------------------   
 * PREFIX: P_ (types) or p_ (functions & variables)
 * ---------------------------------------------------------------------------   
 *                            COMPILE TIME FLAGS
 * ._________________________________________________________________________,
 * | Name            | Description                                           |
 * |-----------------|-------------------------------------------------------|
 * | PODIUM_WIN32    | Used to explicitly compile for Windows                |
 * | PODIUM_LINUX    | Used to explicitly compile for Linux and FreeBSD.     |
 * '-------------------------------------------------------------------------'  
 *                                 MODULES
 * ._________________________________________________________________________,
 * | Name            | Description                                           |
 * |-----------------|-------------------------------------------------------|
 * | P_MODULE_VULKAN | Gives access to vulkan related utilities.             |
 * | P_MODULE_STRING | Include string manipulation utilities.                |
 * | P_MODULE_MATH   | Trigonometry and linear algebra functions.            |
 * | P_MODULE_ALLOC  | Memory allocators: Arena, Stack & Pool.               |
 * '-------------------------------------------------------------------------'
 * =========================================================================== */

#ifndef _PODIUM_H_
#define _PODIUM_H_

#define PODIUM_MAJOR "0" // needs more revisions and use to reach 1.0 status
#define PODIUM_MINOR "2" // vulkan module
#define PODIUM_PATCH "2" // gamepad

#define POAPI static inline
#define PODEF static inline

/*
 * Auto-detect platforms.
 */ 

/* Windows */
#if defined(_WIN32) || defined(_WIN64) || defined(__WIN32__) || defined(__TOS_WIN__)
    #ifndef PODIUM_WIN32
        #define PODIUM_WIN32
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
/* Apple */
#elif defined(__APPLE__) || defined(__MACH__)
    #include <TargetConditionals.h>
    #ifndef PODIUM_APPLE
        #define PODIUM_APPLE
    #endif
    #if TARGET_OS_IPHONE
        #define PODIUM_IOS
    #else
        #define PODIUM_MACOS
    #endif
/* Linux / BSD */ 
#elif defined(__linux__) || defined(__FreeBSD__)
    #ifndef PODIUM_LINUX
        #define PODIUM_LINUX
    #endif
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200112L
    #endif 
#endif 

#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h> /* memcpy */
#include <stdio.h> /* vsnprintf */

/*
 * NOTE: The <REPLACE> comments are used by the build
 * script to know what includes to inline into the
 * final single header file! 
 */

#include "p_types.h"   // <REPLACE>
#include "p_log.h"     // <REPLACE>
#include "p_ds.h"      // <REPLACE>

/*
 * Main Platform Types (Keyboard, Events, Context, etc.)
 */

typedef enum {
    P_KEYMOD_ALT = 0,
    P_KEYMOD_SHIFT,
    P_KEYMOD_CTRL,
    P_KEYMOD_CAPS,
} P_KeyMod;

typedef enum {
    P_KEY_UNKNOWN = 0,
    P_KEY_UP, P_KEY_DOWN, P_KEY_LEFT, P_KEY_RIGHT,
    P_KEY_SPACE, P_KEY_ESCAPE, P_KEY_ENTER,
    P_KEY_A, P_KEY_B, P_KEY_C, P_KEY_D, P_KEY_E, P_KEY_F, P_KEY_G, P_KEY_H, P_KEY_I,
    P_KEY_J, P_KEY_K, P_KEY_L, P_KEY_M, P_KEY_N, P_KEY_O, P_KEY_P, P_KEY_Q, P_KEY_R,
    P_KEY_S, P_KEY_T, P_KEY_U, P_KEY_V, P_KEY_W, P_KEY_X, P_KEY_Y, P_KEY_Z,
} P_KeyCode;

/* Platform event type */
typedef enum {
    P_EVENT_NONE = 0,
    P_EVENT_KEY_DOWN,
    P_EVENT_KEY_UP,
    P_EVENT_WINDOW_CLOSE,
    P_EVENT_WINDOW_RESIZE,
    P_EVENT_POINTER,
    P_EVENT_CONTROLLER_CONNECTED,
    P_EVENT_CONTROLLER_DISCONNECTED,
} P_EvenType;

typedef enum {
    P_POINTER_MOVED = 0,
    P_POINTER_PRESSED,
    P_POINTER_RELEASED
} P_PointerState;

typedef struct {
    P_EvenType type;
    union {
        struct {P_KeyCode key; P_KeyMod mod; } key;
        struct { u32 width, height; } resize;
        struct { P_PointerState state; u32 button; u32 x, y; } pointer;
    };
} P_Event;

/*
 * Platform dependent functions to be implemented by each layer.
 */

POAPI u64  p_get_time(void); // Get time in nanoseconds.
POAPI void p_sleep_ns(i64 ns); // Sleep for nanoseconds.

POAPI void p_stdout(void *msg, usize bytes); // Print buffer to stdout.
PODEF void p_log_printf(P_Log_Level level, const char* src, ...); // Print buffer to stdout.


POAPI void* p_file_alloc(const char *path, unsigned long *buf_size);
POAPI bool  p_file_exists(const char *path); // Check if file exists.
POAPI usize p_file_size(const char *path); // Check file size.
POAPI void  p_file_load(const char *path, void *buf_ptr, unsigned long buf_size); // Load file to buffer.
PODEF void  p_file_write(const char *path, void *buf_ptr, unsigned long buf_size); // Write buffer to file.

typedef struct  P_Window_Impl   P_Window; 
POAPI bool p_window_open(P_Window *win, int width, int height, const char *title); // Opens a window. Returns false on failure.
POAPI void p_window_close(P_Window *win); // Close window.
POAPI bool p_window_is_open(P_Window *win); // Check if window is open.
POAPI void p_window_size(P_Window *win, int *window_width, int *window_height); // Get window size.
POAPI void p_window_draw(P_Window *win, u32 *pixels, int width, int height); // Draw directly to the window's buffer. Normally much slower than using the GPU.
POAPI bool p_window_poll_event(P_Window *win, P_Event *ev); // Returns true while there are events to poll. Write event data to a pointer.

POAPI void p_audio_init(const int sample_rate, const int channels, const char *name, const char *desc);
POAPI void p_audio_quit();
POAPI void p_audio_write(const i16 *samples, usize count);

/*
 * The Vulkan Module is platform specific
 */

#ifdef P_MODULE_VULKAN
#include <vulkan/vulkan.h>
POAPI char** p_vulkan_get_extensions();
POAPI bool   p_vulkan_create_surface(P_Window *window, VkInstance instance, const VkAllocationCallbacks* allocator, VkSurfaceKHR* out_surface);
#endif

/*
 * The Remaining Modules are PLATFORM INDEPENDENT!
 */

#ifdef P_MODULE_STRING
#include <p_string.h> // <REPLACE>
#endif

#ifdef P_MODULE_MATH
#include <p_math.h> // <REPLACE>
#endif

#ifdef P_MODULE_ALLOC
#include <p_allocators.h> // <REPLACE>
#endif

#endif /* _PODIUM_H_ */

#ifdef PODIUM_IMPLEMENTATION
#undef PODIUM_IMPLEMENTATION

#if defined(PODIUM_SDL)
/* #include "p_platform_sdl.c"  */
#elif defined(PODIUM_WIN32)
#include "p_platform_win32.c" // <REPLACE>
#elif defined(PODIUM_LINUX)
#include "p_platform_linux.c" // <REPLACE>
#endif /* linux */


POAPI void* p_file_alloc(const char *path, unsigned long *buf_size) {
    if (p_file_exists(path) == false) {
        return NULL;
    }

    usize file_size = p_file_size(path);
    *buf_size = file_size;

    void *ptr = malloc(file_size);
    if (ptr) {
        p_file_load(path, ptr, file_size); // Load file to buffer.
    }
    return ptr;
}


/*
 * Import Module Code
 */

#ifdef P_MODULE_STRING
#include <p_string.c> // <REPLACE>
#endif

#ifdef P_MODULE_MATH
#include <p_math.c> // <REPLACE>
#endif

#ifdef P_MODULE_ALLOC
#include <p_allocators.c> // <REPLACE>
#endif

#define P_MAX_PRINTF_SIZE 1024
PODEF void
p_log_printf(P_Log_Level level, const char* src, ...)
{
    static char out_message[P_MAX_PRINTF_SIZE];
    static u32 offset = P_PREFIX_LEN + 1;

    memcpy(out_message, p_prefix[level], P_PREFIX_LEN); /* prefixes have a fixed len */
    out_message[P_PREFIX_LEN] = ' '; /* write space */

    __builtin_va_list arg_ptr;
    va_start(arg_ptr, src);
    int len = vsnprintf(out_message+offset, P_MAX_PRINTF_SIZE-offset, src, arg_ptr); // append message
    va_end(arg_ptr);

    if (len < 0) len = 0;
    usize total_len = offset + len;
    if (total_len > P_MAX_PRINTF_SIZE) {
        total_len = P_MAX_PRINTF_SIZE;
    }

    p_stdout(out_message, total_len);
    p_stdout("\n", 1);
}

#endif // PODIUM_IMPLEMENTATION
