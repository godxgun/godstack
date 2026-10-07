/* ===========================================================================
 * PEAK - Copyright @ Vasco Alves - See LICENSE at the end of file.
 *
 * ███ ███ ███ █ █
 * █ █ █   █ █ █ █
 * ███ ███ █ █ ██
 * █   █   ███ █ █
 * █   ███ █ █ █ █
 *
 * figlet font: maxiwi
 *
 * Platform layer. It just works, don't think about it too much.
 * At least, that's the goal.
 *
 * Peak is meant to be included directly via source code for a unity build.
 * It is a self-contained single header; no Peak source files are needed.
 *
 * USAGE (C99):
 * - Include "peak.h" for declarations in any translation unit.
 * - In exactly one translation unit, before including it, define:
 *       #define PEAK_IMPLEMENTATION
 *       #include "peak.h"
 * - A declarations-only include may precede the implementation include in
 *   that same translation unit. Repeated includes do not emit it again.
 * - Set configuration flags before the first include, consistently across
 *   translation units. On POSIX, include Peak before system headers, or
 *   define _POSIX_C_SOURCE=200809L in the compiler flags.
 *
 * SYSTEM DEPENDENCIES (implementation only):
 * - Linux: X11 and Wayland development headers; link -ldl, and -pthread
 *   unless PEAK_NO_AUDIO. X11, Wayland, xkbcommon, and PulseAudio libraries
 *   are loaded at runtime; no direct link to those libraries is required.
 *   Used xdg-shell protocol metadata is bundled below.
 * - Win32: Windows SDK; user32, gdi32, winmm are loaded at runtime.
 * - macOS: compile the implementation as Objective-C; link AppKit,
 *   AudioToolbox, CoreGraphics, and QuartzCore frameworks.
 * - Web: compile/link with emscripten; for strict C99, also define
 *   _POSIX_C_SOURCE=200809L in the compiler flags.
 * - PEAK_VULKAN: requires Vulkan development headers and loader linkage.
 *
 * SUPPORTED PLATFORMS:
 * - Desktop: Win32, MacOS and Linux (Wayland & X11).
 * - Web via emscripten.
 *
 * PREFIX: PEAK (macros)  Peak (types)  peak_ (functions)
 *
 * MACRO FLAGS (you define):
 * - PEAK_IMPLEMENTATION emit implementation in exactly one translation unit.
 * - PEAK_VULKAN         Vulkan WSI. Sets VK_USE_PLATFORM_*.
 * - PEAK_NO_AUDIO       Linux audio off. start returns 0. no pthread / pulse.
 * - P_LOG_WARN_ENABLED  default 1. PWARN.
 * - P_LOG_INFO_ENABLED  default 1. PINFO.
 * - P_LOG_DEBUG_ENABLED default 0. PDEBUG.
 * - P_LOG_TRACE_ENABLED default 0. PTRACE.
 *
 * DEFINED:
 * - PEAK_WEB                wasm / emscripten
 * - PEAK_WIN32              Windows
 * - PEAK_APPLE              Darwin
 * - PEAK_IOS                iPhone
 * - PEAK_MACOS              macOS
 * - PEAK_ANDROID            also PEAK_LINUX
 * - PEAK_LINUX              Linux
 * - PEAK_BSD                *BSD
 * - PEAK_UNIX               Linux / BSD / Apple
 * - PEAK_WINDOW_TRANSPARENT window_open: ARGB visual
 * - PEAK_WINDOW_FULLSCREEN  window_open: start fullscreen
 * - PEAK_HANDLE             fd, or HANDLE on Win32
 * - PEAK_HANDLE_INVALID     closed / failed
 * - PEAK                    extern
 *
 * The platform detection macros may be useful in your project,
 * feel free to use them. Detecting a platform does not mean it's supported.
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

#define PEAK_MAJOR "1"
#define PEAK_MINOR "0"
#define PEAK_PATCH "0"

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
 * 0.5.8 - @vasco - peak_window_pending; XPending so idle poll can sleep
 * 0.5.9 - @vasco - wheel up/down as PeakPointerType (X11 Button4/5)
 * 0.6.0 - @vasco - macos (Cocoa/Metal/AudioQueue)
 * 0.6.1 - @vasco - PEAK_WINDOW_TRANSPARENT (X11 ARGB visual)
 * 0.6.2 - @vasco - PEAK_HANDLE, pty, wait, sock, job, mirror ring
 * 0.6.3 - @vasco - win32 sizeof style; platform fn prototypes
 * 0.6.4 - @vasco - win32 audio_stop before start
 * 0.6.5 - @vasco - ISO_Left_Tab; linux syscall prototype
 * 0.6.6 - @vasco - pending is this window only; WSI events no longer spin poll
 * 0.7.0 - @vasco - clipboard; keymod flags; Insert; pointer.mod; PEAK_EVENT_CLIP
 * 0.8.0 - @vasco - keys F1-12 Home End Page Super; title size fullscreen cursor relative scale; text/drop; filesystem; sock_connect; wayland then x11; pointer connect
 * 0.9.0 - @vasco - pid, env, dir list, symlink, child reap fd, sock SCM_RIGHTS, pointer pid
 * 0.9.1 - @vasco - PEAK_NO_AUDIO; linux skips pthread and pulse
 * 0.10.0 - @vasco - peak_aligned_alloc / peak_aligned_free
 * 0.10.1 - @vasco - wayland marshal new_id slots; create_pool no longer sends size as fd
 * 0.10.2 - @vasco - wayland seat v4; wl_keyboard.repeat_info; xdg_toplevel bounds/caps
 * 0.10.3 - @vasco - wayland read socket; data device clip; pointer/key mods
 * 0.10.4 - @vasco - wayland pump drains socket; xdg first commit before map
 * 0.10.5 - @vasco - wayland client key repeat (repeat_info + timerfd)
 * 0.10.6 - @vasco - wayland file drop + peak_drop_drag (data_device v3)
 * 0.10.7 - @vasco - wayland dnd: defer offer destroy across drop+leave; finish live copy/move only
 * 0.10.8 - @vasco - wayland compositor keymap via libxkbcommon (layout, group, compose)
 * 0.10.9 - @vasco - peak_env_get; SIGUSR1 wakeup fd
 * 0.10.10 - @vasco - peak_pipe_capacity / peak_pipe_set_capacity
 * 1.0.0 - @vasco - self-contained peak.h; opt-in PEAK_IMPLEMENTATION
 */

#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#if !defined(__cplusplus)
#if !( \
    (defined(__STDC__) && __STDC__ == 1 && defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)\
)
#error "Peak requires C99."
#endif
#endif

//   █
//   █      █           █
// ███ ███ ███ ███ ███ ███
// █ █ ███  █  ███ █    █
// █ █ █    █  █   █    █
// ███ ███  ██ ███ ███  ██

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
#if defined(PEAK_LINUX)
#ifndef VK_USE_PLATFORM_WAYLAND_KHR
#define VK_USE_PLATFORM_WAYLAND_KHR
#endif
#ifndef VK_USE_PLATFORM_XLIB_KHR
#define VK_USE_PLATFORM_XLIB_KHR
#endif
#elif defined(PEAK_WIN32) && !defined(VK_USE_PLATFORM_WIN32_KHR)
#define VK_USE_PLATFORM_WIN32_KHR
#elif defined(PEAK_MACOS) && !defined(VK_USE_PLATFORM_METAL_EXT)
#define VK_USE_PLATFORM_METAL_EXT
#endif
#endif

#if defined(PEAK_WIN32)
typedef void *PEAK_HANDLE;
#else
typedef int PEAK_HANDLE;
#endif
#define PEAK_HANDLE_INVALID ((PEAK_HANDLE)(intptr_t)-1)
#define PEAK extern

// █     █
//          █
// █ ███ █ ███
// █ █ █ █  █
// █ █ █ █  █
// █ █ █ █  ██

PEAK int  peak_init(void);
PEAK void peak_quit(void);

//                  █
// ███ █ █ ███ ███ ███
// ███ █ █ ███ █ █  █
// █   █ █ █   █ █  █
// ███  █  ███ █ █  ██

typedef enum {
    PEAK_KEYMOD_NONE  = 0,
    PEAK_KEYMOD_SHIFT = 1 << 0,
    PEAK_KEYMOD_CTRL  = 1 << 1,
    PEAK_KEYMOD_ALT   = 1 << 2,
    PEAK_KEYMOD_CAPS  = 1 << 3,
    PEAK_KEYMOD_SUPER = 1 << 4,
} PeakKeyMod;

typedef enum {
    PEAK_KEY_UNKNOWN = 0,
    PEAK_KEY_UP,
    PEAK_KEY_DOWN,
    PEAK_KEY_LEFT,
    PEAK_KEY_RIGHT,
    PEAK_KEY_SPACE,
    PEAK_KEY_ESCAPE,
    PEAK_KEY_ENTER,
    PEAK_KEY_BACKSPACE,
    PEAK_KEY_TAB,
    PEAK_KEY_DELETE,
    PEAK_KEY_INSERT,
    PEAK_KEY_HOME,
    PEAK_KEY_END,
    PEAK_KEY_PAGEUP,
    PEAK_KEY_PAGEDOWN,

    /* F Keys */
    PEAK_KEY_F1, PEAK_KEY_F2, PEAK_KEY_F3, PEAK_KEY_F4, PEAK_KEY_F5, PEAK_KEY_F6,
    PEAK_KEY_F7, PEAK_KEY_F8, PEAK_KEY_F9, PEAK_KEY_F10, PEAK_KEY_F11, PEAK_KEY_F12,

    /* Numbers */
    PEAK_KEY_0 = '0', // 48
    PEAK_KEY_1 = '1', // 49
    PEAK_KEY_2 = '2', // 50
    PEAK_KEY_3 = '3', // 51
    PEAK_KEY_4 = '4', // 52
    PEAK_KEY_5 = '5', // 53
    PEAK_KEY_6 = '6', // 54
    PEAK_KEY_7 = '7', // 55
    PEAK_KEY_8 = '8', // 56
    PEAK_KEY_9 = '9', // 57

    /* [58;64] */

    /* Letters */
    PEAK_KEY_A = 'A', // 65
    PEAK_KEY_B = 'B', // 66
    PEAK_KEY_C = 'C', // 67
    PEAK_KEY_D = 'D', // 68
    PEAK_KEY_E = 'E', // 69
    PEAK_KEY_F = 'F', // 70
    PEAK_KEY_G = 'G', // 71
    PEAK_KEY_H = 'H', // 72
    PEAK_KEY_I = 'I', // 73
    PEAK_KEY_J = 'J', // 74
    PEAK_KEY_K = 'K', // 75
    PEAK_KEY_L = 'L', // 76
    PEAK_KEY_M = 'M', // 77
    PEAK_KEY_N = 'N', // 78
    PEAK_KEY_O = 'O', // 79
    PEAK_KEY_P = 'P', // 80
    PEAK_KEY_Q = 'Q', // 81
    PEAK_KEY_R = 'R', // 82
    PEAK_KEY_S = 'S', // 83
    PEAK_KEY_T = 'T', // 84
    PEAK_KEY_U = 'U', // 85
    PEAK_KEY_V = 'V', // 86
    PEAK_KEY_W = 'W', // 87
    PEAK_KEY_X = 'X', // 88
    PEAK_KEY_Y = 'Y', // 89
    PEAK_KEY_Z = 'Z', // 90
} PeakKeyCode;

typedef enum {
    PEAK_CLIP_CLIPBOARD = 0,
    PEAK_CLIP_PRIMARY,
} PeakClip;

typedef enum {
    PEAK_EVENT_NONE = 0,
    PEAK_EVENT_KEY_DOWN,
    PEAK_EVENT_KEY_UP,
    PEAK_EVENT_WINDOW_CLOSE,
    PEAK_EVENT_WINDOW_RESIZE,
    PEAK_EVENT_POINTER,
    PEAK_EVENT_POINTER_CONNECTED,
    PEAK_EVENT_POINTER_DISCONNECTED,
    PEAK_EVENT_CLIP,
    PEAK_EVENT_TEXT,
    PEAK_EVENT_DROP,
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
    PEAK_POINTER_WHEEL_UP,
    PEAK_POINTER_WHEEL_DOWN,
} PeakPointerType;

typedef struct {
    PeakEventType type;
    union {
        struct { PeakKeyCode key; PeakKeyMod mod; uint32_t code; } key;
        struct { uint32_t width, height; } resize;
        struct { PeakPointerState state; PeakPointerType type; float x, y; PeakKeyMod mod; } pointer;
        struct { PeakClip which; size_t n; } clip;
        struct { size_t n; } text;
        struct { size_t n; } drop;
    };
} PeakEvent;

//       █       █
//               █
// █ █ █ █ ███ ███ ███ █ █ █
// █ █ █ █ █ █ █ █ █ █ █ █ █
// █ █ █ █ █ █ █ █ █ █ █ █ █
//  █ █  █ █ █ ███ ███  █ █

enum PeakWindowFlags {
    PEAK_WINDOW_TRANSPARENT = 1 << 0,
    PEAK_WINDOW_FULLSCREEN  = 1 << 1
};

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
    uint16_t *audio;
    int running;
} PeakWindow;

PEAK PeakWindow peak_window_open(const char *name, uint32_t width, uint32_t height, uint32_t flags);
PEAK void       peak_window_close(PeakWindow *window);
PEAK void       peak_window_run(PeakWindow *win, int (*peak_tick)(PeakWindow *win, void *userdata), void *userdata); /* hijack main loop (web) */
PEAK int        peak_window_epoll(PeakWindow *win, PeakEvent *ev);
PEAK int        peak_window_fd(PeakWindow *win); /* display connection fd, or -1 */
PEAK int        peak_window_pending(PeakWindow *win); /* queued window events; 0 if none */
PEAK uint32_t  *peak_window_backbuffer(PeakWindow *win, size_t *width, size_t *height);
PEAK void       peak_window_clear(PeakWindow *win, float r, float g, float b, float a);
PEAK void       peak_window_present(PeakWindow *win);
PEAK void       peak_window_set_title(PeakWindow *win, const char *name);
PEAK void       peak_window_set_size(PeakWindow *win, uint32_t width, uint32_t height);
PEAK void       peak_window_fullscreen(PeakWindow *win, int on);
PEAK void       peak_window_cursor(PeakWindow *win, int on); /* 1 show, 0 hide */
PEAK void       peak_window_pointer_relative(PeakWindow *win, int on); /* 1 deltas, 0 absolute */
PEAK float      peak_window_scale(PeakWindow *win); /* framebuffer / window; 1.0 if unknown */

//           █ █
//           █
// ███ █ █ ███ █ ███
//   █ █ █ █ █ █ █ █
// ███ █ █ █ █ █ █ █
// ███ ███ ███ █ ███

PEAK int  peak_audio_start(uint32_t channels, uint32_t rate, void (*fill)(int16_t *out, size_t frames, void *userdata), void *userdata); /* device pulls interleaved s16le */
PEAK void peak_audio_stop(void);

//     █
//  █
// ███ █ █████ ███
//  █  █ █ █ █ ███
//  █  █ █ █ █ █
//  ██ █ █ █ █ ███

#define NANOS_PER_SEC 1000000000ull

PEAK uint64_t peak_get_time(void); /* nanoseconds */
PEAK void     peak_sleep_ns(int64_t ns);

//  ██ █ █
//  █    █
// ███ █ █  ███
//  █  █ █  ███
//  █  █ █  █
//  █  █ ██ ███

PEAK int   peak_file_exists(const char *path);
PEAK void *peak_file_alloc(const char *path, unsigned long *buf_size);
PEAK int   peak_file_write(const char *path, const void *buf, size_t n); /* create/overwrite */
PEAK void *peak_aligned_alloc(size_t size, size_t alignment); /* power-of-two; 0 on fail */
PEAK void  peak_aligned_free(void *p);

PEAK int peak_pid(void);
PEAK int peak_env_set(const char *name, const char *value); /* NULL unsets */
PEAK int peak_env_get(const char *name, char *buf, size_t cap); /* 1 if set and fits */

PEAK int peak_filesystem_mkdir(const char *path); /* one level */
PEAK int peak_filesystem_rm(const char *path); /* unlink, or rmdir if empty */
PEAK int peak_filesystem_cwd(char *buf, size_t cap);
PEAK int peak_filesystem_chdir(const char *path);
PEAK int peak_filesystem_rename(const char *from, const char *to);
PEAK int peak_filesystem_list(const char *path, int (*fn)(const char *name, void *ud), void *ud); /* fn 0 stops; 1 if opened */
PEAK int peak_filesystem_symlink(const char *target, const char *path);
PEAK int peak_filesystem_readlink(const char *path, char *dst, size_t cap);

// ███ ███ ███ ███
// █ █ █   █ █ █
// █ █ █   █ █ █
// ███ █   ███ ███
// █
// █

typedef struct PeakProc {
    PEAK_HANDLE fd; /* PEAK_HANDLE_INVALID if closed / failed */
    int pid;        /* 0 if none */
} PeakProc;

/* Child + PTY. File descriptor is nonblocking. Fails as PEAK_HANDLE_INVALID. */
PEAK PeakProc peak_pty_spawn(const char *file, const char **argv, uint32_t cols, uint32_t rows, uint32_t xpixel, uint32_t ypixel);
PEAK void     peak_pty_resize(PeakProc *pty, uint32_t cols, uint32_t rows, uint32_t xpixel, uint32_t ypixel);
PEAK int      peak_pty_reap(PeakProc *pty); /* 1 if dead */
PEAK void     peak_pty_close(PeakProc *pty);

/* Off-grid shell. */
PEAK PeakProc peak_job_run(const char *cmd, const char *cwd);
PEAK int      peak_job_reap(PeakProc *job, int *code); /* 1 if exited */
PEAK void     peak_job_kill(PeakProc *job);
PEAK int      peak_pid_cwd(int pid, char *buf, size_t cap);

/* Dead-child wakeup. fd pollable or INVALID. Missing OS: arm 1, fd INVALID, reap 0. */
PEAK int         peak_child_arm(void);
PEAK void        peak_child_disarm(void);
PEAK PEAK_HANDLE peak_child_fd(void);
PEAK void        peak_child_ack(void);
PEAK int         peak_child_reap(int *pid, int *code); /* 1 if one */

/* SIGUSR1 wakeup. fd pollable or INVALID. Missing OS: arm 1, fd INVALID, ack 0. */
PEAK int         peak_usr1_arm(void);
PEAK void        peak_usr1_disarm(void);
PEAK PEAK_HANDLE peak_usr1_fd(void);
PEAK int         peak_usr1_ack(void); /* 1 if fired */

PEAK int peak_stdout_silence(void); /* stdout -> platform null */
PEAK int peak_stdout_restore(void);

// █
//
// █ ███
// █ █ █
// █ █ █
// █ ███

/* Sleep until window (nullable) or any fd is ready. timeout_ms: -1 block, 0 poll. 1 if ready. */
PEAK int peak_wait(PeakWindow *win, const PEAK_HANDLE *fds, uint32_t n, int timeout_ms);

/* Local stream (unix socket / named pipe). listen/accept fds are nonblocking. */
PEAK int         peak_runtime_dir(char *buf, size_t cap, const char *app);
PEAK PEAK_HANDLE peak_sock_listen(const char *path);
PEAK PEAK_HANDLE peak_sock_accept(PEAK_HANDLE listen_fd);
PEAK PEAK_HANDLE peak_sock_connect(const char *path);
PEAK int         peak_sock_send(PEAK_HANDLE sock, const void *buf, size_t n, PEAK_HANDLE pass);
PEAK int         peak_sock_recv(PEAK_HANDLE sock, void *buf, size_t n, PEAK_HANDLE *pass);
PEAK int         peak_pointer_pid(PeakWindow *win); /* _NET_WM_PID under pointer; 0 if none */
PEAK int         peak_pointer_local(PeakWindow *win, int *x, int *y); /* 1 if pointer is in this window */

/* Byte IO on Peak fds (pty, sock, job). -1 would-block, 0 EOF, >0 count. */
PEAK int  peak_fd_read(PEAK_HANDLE fd, void *buf, size_t n);
PEAK int  peak_fd_write(PEAK_HANDLE fd, const void *buf, size_t n);
PEAK void peak_fd_close(PEAK_HANDLE fd);
/* Kernel pipe buffer. 0 if not a pipe. set returns the size in force. */
PEAK size_t peak_pipe_capacity(PEAK_HANDLE fd);
PEAK size_t peak_pipe_set_capacity(PEAK_HANDLE fd, size_t n);

//     █
//
// ███ █ ███ ███
// █   █ █ █ █ █
// █   █ █ █ █ █
// █   █ █ █ ███
//             █
//           ███

/* Page-mirrored ring: size must be page-aligned. pointer valid for size*2. */
PEAK size_t peak_page_size(void);
PEAK void  *peak_mirror_map(size_t size);
PEAK void   peak_mirror_unmap(void *p, size_t size);

//      ██
//      █
// ███ ███ █ █
// █ █  █   █
// █ █  █   █
// ███  █  █ █
//   █
// ███

PEAK const char **peak_vulkan_get_extensions(uint32_t *count);
PEAK int          peak_vulkan_create_surface(PeakWindow *win, void *instance, const void *allocator, void *out_surface); /* needs PEAK_VULKAN */

//     █  █
//     █
// ███ █  █ ███
// █   █  █ █ █
// █   █  █ █ █
// ███ ██ █ ███
//          █
//          █

/* UTF-8 clipboard. Cap 1 MiB. win NULL: process-local slot. PRIMARY aliases
 * CLIPBOARD on Win32/macOS/web. request completes as PEAK_EVENT_CLIP; take copies. */
PEAK int peak_clip_set(PeakWindow *win, PeakClip which, const char *utf8, size_t n);
PEAK int peak_clip_request(PeakWindow *win, PeakClip which);
PEAK int peak_clip_take(PeakWindow *win, char *dst, size_t cap, size_t *n);

/* UTF-8 text / drop path. Cap 1 MiB. Completes as PEAK_EVENT_TEXT / DROP; take copies. */
PEAK int peak_text_take(PeakWindow *win, char *dst, size_t cap, size_t *n);
PEAK int peak_drop_take(PeakWindow *win, char *dst, size_t cap, size_t *n);
PEAK int peak_drop_drag(PeakWindow *win, const char *utf8, size_t n); /* start OS drag; 0 if none */

//   █     █
//   █     █
// ███ ███ ███ █ █ ███
// █ █ ███ █ █ █ █ █ █
// █ █ █   █ █ █ █ █ █
// ███ ███ ███ ███ ███
//                   █
//                 ███

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

PEAK void  peak_log_printf(PeakLogLevel level, const char *src, ...);
PEAK void *peak_debug_malloc_impl(size_t size, const char *file, int line, const char *func);
PEAK void  peak_debug_free_impl(void *ptr, const char *file, int line, const char *func);
PEAK void *peak_debug_realloc_impl(void *ptr, size_t size, const char *file, int line, const char *func);
PEAK void  peak_debug_memory_report(void);

#endif /* PEAK_H */

/* Implementation is outside PEAK_H so it may follow a declarations-only include. */
#if defined(PEAK_IMPLEMENTATION) && !defined(PEAK_IMPLEMENTATION_INCLUDED)
#define PEAK_IMPLEMENTATION_INCLUDED

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

#define PEAK_CLIP_MAX (1024u * 1024u)

static struct {
    char *own[2];
    size_t own_n[2];
    char *paste;
    size_t paste_n;
    int paste_ready;
    PeakClip paste_which;
} peak_clip;

static struct {
    char *text;
    size_t text_n;
    int text_ready;
    char *drop;
    size_t drop_n;
    int drop_ready;
} peak_xfer;

static int
peak_clip_own_store(PeakClip which, const char *utf8, size_t n)
{
    char *p;

    if ((unsigned)which > 1)
        return 0;
    if (n && !utf8)
        return 0;
    p = realloc(peak_clip.own[which], n ? n : 1);
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

    if (n > PEAK_CLIP_MAX)
        n = PEAK_CLIP_MAX;
    if (n && !utf8)
        n = 0;
    p = realloc(peak_clip.paste, n ? n : 1);
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

    if (n > PEAK_CLIP_MAX)
        n = PEAK_CLIP_MAX;
    if (n && !utf8)
        n = 0;
    p = realloc(peak_xfer.text, n ? n : 1);
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

    if (n > PEAK_CLIP_MAX)
        n = PEAK_CLIP_MAX;
    if (n && !utf8)
        n = 0;
    p = realloc(peak_xfer.drop, n ? n : 1);
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
/* BEGIN p_win32.c */
/*
 * Win32 window, input, StretchDIBits present, and waveOut audio.
 * user32.dll, gdi32.dll, and winmm.dll are loaded at runtime.
 *
 * * 0.4.0 - @vasco - win32
 * * 0.5.0 - @vasco - audio start/stop, s16le pull callback
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

/* windows.h must precede mmsystem.h */
#include <windows.h>
#include <shellapi.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef PEAK_VULKAN
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#endif

#define PEAK_WIN32_USER32 "user32.dll"
#define PEAK_WIN32_GDI32  "gdi32.dll"
#define PEAK_WIN32_WINMM  "winmm.dll"
#define PEAK_WIN32_CLASS  "PeakWindow"
#define PEAK_WIN32_PROP   "Peak"
#define PEAK_AUDIO_FRAMES  256
#define PEAK_AUDIO_BUFFERS 2

#define PEAK_USER32_API(X) \
        X(RegisterClassExA,   ATOM,    WINAPI, (const WNDCLASSEXA *)) \
        X(UnregisterClassA,   BOOL,    WINAPI, (LPCSTR, HINSTANCE)) \
        X(CreateWindowExA,    HWND,    WINAPI, (DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID)) \
        X(DestroyWindow,      BOOL,    WINAPI, (HWND)) \
        X(ShowWindow,         BOOL,    WINAPI, (HWND, int)) \
        X(GetDC,              HDC,     WINAPI, (HWND)) \
        X(ReleaseDC,          int,     WINAPI, (HWND, HDC)) \
        X(PeekMessageA,       BOOL,    WINAPI, (LPMSG, HWND, UINT, UINT, UINT)) \
        X(TranslateMessage,   BOOL,    WINAPI, (const MSG *)) \
        X(DispatchMessageA,   LRESULT, WINAPI, (const MSG *)) \
        X(DefWindowProcA,     LRESULT, WINAPI, (HWND, UINT, WPARAM, LPARAM)) \
        X(SetPropA,           BOOL,    WINAPI, (HWND, LPCSTR, HANDLE)) \
        X(GetPropA,           HANDLE,  WINAPI, (HWND, LPCSTR)) \
        X(RemovePropA,        HANDLE,  WINAPI, (HWND, LPCSTR)) \
        X(BeginPaint,         HDC,     WINAPI, (HWND, LPPAINTSTRUCT)) \
        X(EndPaint,           BOOL,    WINAPI, (HWND, const PAINTSTRUCT *)) \
        X(AdjustWindowRectEx, BOOL,    WINAPI, (LPRECT, DWORD, BOOL, DWORD)) \
        X(LoadCursorA,        HCURSOR, WINAPI, (HINSTANCE, LPCSTR)) \
        X(GetKeyState,        SHORT,   WINAPI, (int)) \
        X(OpenClipboard,      BOOL,    WINAPI, (HWND)) \
        X(CloseClipboard,     BOOL,    WINAPI, (void)) \
        X(EmptyClipboard,     BOOL,    WINAPI, (void)) \
        X(SetClipboardData,   HANDLE,  WINAPI, (UINT, HANDLE)) \
        X(GetClipboardData,   HANDLE,  WINAPI, (UINT)) \
        X(SetWindowTextA,     BOOL,    WINAPI, (HWND, LPCSTR)) \
        X(SetWindowPos,       BOOL,    WINAPI, (HWND, HWND, int, int, int, int, UINT)) \
        X(ShowCursor,         int,     WINAPI, (BOOL)) \
        X(SetCursor,          HCURSOR, WINAPI, (HCURSOR)) \
        X(SetCursorPos,       BOOL,    WINAPI, (int, int)) \
        X(GetCursorPos,       BOOL,    WINAPI, (LPPOINT)) \
        X(ScreenToClient,     BOOL,    WINAPI, (HWND, LPPOINT)) \
        X(ClientToScreen,     BOOL,    WINAPI, (HWND, LPPOINT)) \
        X(GetClientRect,      BOOL,    WINAPI, (HWND, LPRECT)) \
        X(GetWindowRect,      BOOL,    WINAPI, (HWND, LPRECT)) \
        X(GetWindowLongPtrA,  LONG_PTR,WINAPI, (HWND, int)) \
        X(SetWindowLongPtrA,  LONG_PTR,WINAPI, (HWND, int, LONG_PTR)) \
        X(GetSystemMetrics,   int,     WINAPI, (int)) \
        X(UpdateLayeredWindow,BOOL,    WINAPI, (HWND, HDC, POINT *, SIZE *, HDC, POINT *, COLORREF, BLENDFUNCTION *, DWORD)) \
        X(SetCapture,         HWND,    WINAPI, (HWND)) \
        X(ReleaseCapture,     BOOL,    WINAPI, (void)) \
        X(GetWindowPlacement, BOOL,    WINAPI, (HWND, WINDOWPLACEMENT *)) \
        X(SetWindowPlacement, BOOL,    WINAPI, (HWND, const WINDOWPLACEMENT *))

#define PEAK_GDI32_API(X) \
        X(StretchDIBits, int, WINAPI, (HDC, int, int, int, int, int, int, int, int, const void *, const BITMAPINFO *, UINT, DWORD)) \
        X(CreateCompatibleDC, HDC, WINAPI, (HDC)) \
        X(CreateDIBSection, HBITMAP, WINAPI, (HDC, const BITMAPINFO *, UINT, void **, HANDLE, DWORD)) \
        X(SelectObject, HGDIOBJ, WINAPI, (HDC, HGDIOBJ)) \
        X(DeleteDC, BOOL, WINAPI, (HDC)) \
        X(DeleteObject, BOOL, WINAPI, (HGDIOBJ))

#define PEAK_WINMM_API(X) \
        X(waveOutOpen,            MMRESULT, WINAPI, (LPHWAVEOUT, UINT, LPCWAVEFORMATEX, DWORD_PTR, DWORD_PTR, DWORD)) \
        X(waveOutClose,           MMRESULT, WINAPI, (HWAVEOUT)) \
        X(waveOutPrepareHeader,   MMRESULT, WINAPI, (HWAVEOUT, LPWAVEHDR, UINT)) \
        X(waveOutUnprepareHeader, MMRESULT, WINAPI, (HWAVEOUT, LPWAVEHDR, UINT)) \
        X(waveOutWrite,           MMRESULT, WINAPI, (HWAVEOUT, LPWAVEHDR, UINT)) \
        X(waveOutReset,           MMRESULT, WINAPI, (HWAVEOUT))

typedef struct {
#define X(name, ret, conv, args) ret (conv *name) args;
	PEAK_USER32_API(X)
#undef X
} PeakUser32Api;

typedef struct {
#define X(name, ret, conv, args) ret (conv *name) args;
	PEAK_GDI32_API(X)
#undef X
} PeakGdi32Api;

typedef struct {
#define X(name, ret, conv, args) ret (conv *name) args;
	PEAK_WINMM_API(X)
#undef X
} PeakWinmmApi;

typedef struct {
	volatile int run;
	HANDLE thread;
	HANDLE event;
	HWAVEOUT out;
	WAVEHDR hdr[PEAK_AUDIO_BUFFERS];
	int16_t *pcm[PEAK_AUDIO_BUFFERS];
	uint32_t channels;
	uint32_t bytes;
	void (*fill)(int16_t *out, size_t frames, void *userdata);
	void *userdata;
} PeakAudio;

typedef struct {
	HMODULE user32;
	HMODULE gdi32;
	int class_reg;
} PeakWin32;

struct peak_win32_win {
	HWND hwnd;
	HDC hdc;
	uint32_t *buffer;
	uint32_t width;
	uint32_t height;
	int flags;
	int cursor_on;
	int relative;
	int layered;
	int touch_n;
	float last_x, last_y;
	WINDOWPLACEMENT place;
	DWORD style, ex;
	PeakQ q;
};


static int peak_internal_user32_load(HMODULE handle);
static int peak_internal_gdi32_load(HMODULE handle);
static PeakKeyCode peak_internal_win32_key_map(WPARAM vk);
static PeakKeyMod peak_internal_win32_mod_map(void);
static int peak_internal_win32_buffer(struct peak_win32_win *w, uint32_t width, uint32_t height);
static LRESULT CALLBACK peak_internal_win32_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
static int peak_platform_init(void);
static void peak_platform_quit(void);
static PeakWindowInternal peak_platform_window_open(const char *name, uint32_t width, uint32_t height, uint32_t flags);
static void peak_platform_window_close(PeakWindowInternal *intern);
static uint32_t *peak_platform_window_buffer(PeakWindowInternal *intern, size_t *width, size_t *height);
static void peak_platform_window_present(PeakWindowInternal *intern);
static bool peak_platform_epoll(PeakWindowInternal *intern, PeakEvent *ev);
static int peak_platform_fd(PeakWindowInternal *intern);
static int peak_platform_pending(PeakWindowInternal *intern);
static int peak_internal_winmm_load(void);
static void peak_internal_win32_audio_fill(int i);
static DWORD WINAPI peak_internal_win32_audio_thread(LPVOID arg);
static void peak_platform_audio_stop(void);
static int peak_platform_audio_start(uint32_t channels, uint32_t rate, void (*fill)(int16_t *out, size_t frames, void *userdata), void *userdata);
static uint64_t peak_platform_get_time(void);
static void peak_platform_sleep_ns(int64_t ns);
static const char **peak_platform_vulkan_get_extensions(uint32_t *count);
static int peak_platform_vulkan_create_surface(PeakWindowInternal *intern, void *instance, const void *allocator, void *out_surface);
static void peak_platform_window_set_title(PeakWindowInternal *intern, const char *name);
static void peak_platform_window_set_size(PeakWindowInternal *intern, uint32_t width, uint32_t height);
static void peak_platform_window_fullscreen(PeakWindowInternal *intern, int on);
static void peak_platform_window_cursor(PeakWindowInternal *intern, int on);
static void peak_platform_window_pointer_relative(PeakWindowInternal *intern, int on);
static float peak_platform_window_scale(PeakWindowInternal *intern);

static PeakWin32 peak_win32;
static PeakUser32Api peak_user32;
static PeakGdi32Api peak_gdi32;
static PeakWinmmApi peak_winmm;
static PeakAudio peak_audio;
static void (WINAPI *peak_DragAcceptFiles)(HWND, BOOL);
static UINT (WINAPI *peak_DragQueryFileA)(HDROP, UINT, LPSTR, UINT);
static void (WINAPI *peak_DragFinish)(HDROP);

static int
peak_internal_user32_load(HMODULE handle)
{
#define X(name, ret, conv, args) peak_user32.name = (ret (conv *) args)(void *)GetProcAddress(handle, #name);
	PEAK_USER32_API(X)
#undef X
#define X(name, ret, conv, args) || !peak_user32.name
	if (0 PEAK_USER32_API(X))
		return 0;
#undef X
	return 1;
}

static int
peak_internal_gdi32_load(HMODULE handle)
{
#define X(name, ret, conv, args) peak_gdi32.name = (ret (conv *) args)(void *)GetProcAddress(handle, #name);
	PEAK_GDI32_API(X)
#undef X
#define X(name, ret, conv, args) || !peak_gdi32.name
	if (0 PEAK_GDI32_API(X))
		return 0;
#undef X
	return 1;
}

static PeakKeyCode
peak_internal_win32_key_map(WPARAM vk)
{
	if (vk >= 'A' && vk <= 'Z')
		return (PeakKeyCode)(PEAK_KEY_A + (int)(vk - 'A'));
	if (vk >= '0' && vk <= '9')
		return (PeakKeyCode)(PEAK_KEY_0 + (int)(vk - '0'));
	switch (vk) {
	case VK_UP:
		return PEAK_KEY_UP;
	case VK_DOWN:
		return PEAK_KEY_DOWN;
	case VK_LEFT:
		return PEAK_KEY_LEFT;
	case VK_RIGHT:
		return PEAK_KEY_RIGHT;
	case VK_SPACE:
		return PEAK_KEY_SPACE;
	case VK_ESCAPE:
		return PEAK_KEY_ESCAPE;
	case VK_RETURN:
		return PEAK_KEY_ENTER;
	case VK_BACK:
		return PEAK_KEY_BACKSPACE;
	case VK_TAB:
		return PEAK_KEY_TAB;
	case VK_DELETE:
		return PEAK_KEY_DELETE;
	case VK_INSERT:
		return PEAK_KEY_INSERT;
	case VK_HOME:
		return PEAK_KEY_HOME;
	case VK_END:
		return PEAK_KEY_END;
	case VK_PRIOR:
		return PEAK_KEY_PAGEUP;
	case VK_NEXT:
		return PEAK_KEY_PAGEDOWN;
	case VK_F1: return PEAK_KEY_F1;
	case VK_F2: return PEAK_KEY_F2;
	case VK_F3: return PEAK_KEY_F3;
	case VK_F4: return PEAK_KEY_F4;
	case VK_F5: return PEAK_KEY_F5;
	case VK_F6: return PEAK_KEY_F6;
	case VK_F7: return PEAK_KEY_F7;
	case VK_F8: return PEAK_KEY_F8;
	case VK_F9: return PEAK_KEY_F9;
	case VK_F10: return PEAK_KEY_F10;
	case VK_F11: return PEAK_KEY_F11;
	case VK_F12: return PEAK_KEY_F12;
	default:
		return PEAK_KEY_UNKNOWN;
	}
}

static PeakKeyMod
peak_internal_win32_mod_map(void)
{
	PeakKeyMod m;

	m = 0;
	if (peak_user32.GetKeyState(VK_SHIFT) & 0x8000)
		m |= PEAK_KEYMOD_SHIFT;
	if (peak_user32.GetKeyState(VK_CONTROL) & 0x8000)
		m |= PEAK_KEYMOD_CTRL;
	if (peak_user32.GetKeyState(VK_MENU) & 0x8000)
		m |= PEAK_KEYMOD_ALT;
	if (peak_user32.GetKeyState(VK_CAPITAL) & 1)
		m |= PEAK_KEYMOD_CAPS;
	if (peak_user32.GetKeyState(VK_LWIN) & 0x8000 || peak_user32.GetKeyState(VK_RWIN) & 0x8000)
		m |= PEAK_KEYMOD_SUPER;
	return m;
}

static int
peak_internal_win32_buffer(struct peak_win32_win *w, uint32_t width, uint32_t height)
{
	uint32_t *buffer;

	if (!(buffer = calloc((size_t)width * height, sizeof *buffer)))
		return 0;
	free(w->buffer);
	w->buffer = buffer;
	w->width = width;
	w->height = height;
	return 1;
}

static LRESULT CALLBACK
peak_internal_win32_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	struct peak_win32_win *w;
	PeakEvent ev;
	CREATESTRUCTA *cs;

	if (msg == WM_NCCREATE) {
		cs = (CREATESTRUCTA *)lparam;
		peak_user32.SetPropA(hwnd, PEAK_WIN32_PROP, cs->lpCreateParams);
	}

	w = (struct peak_win32_win *)peak_user32.GetPropA(hwnd, PEAK_WIN32_PROP);
	if (!w)
		return peak_user32.DefWindowProcA(hwnd, msg, wparam, lparam);

	switch (msg) {
	case WM_CLOSE:
		memset(&ev, 0, sizeof ev);
		ev.type = PEAK_EVENT_WINDOW_CLOSE;
		peak_q_push(&w->q, ev);
		return 0;
	case WM_SIZE: {
		uint32_t width, height;

		if (wparam == SIZE_MINIMIZED)
			return 0;
		width = (uint32_t)LOWORD(lparam);
		height = (uint32_t)HIWORD(lparam);
		if (!width || !height || (width == w->width && height == w->height))
			return 0;
		if (!peak_internal_win32_buffer(w, width, height))
			return 0;
		memset(&ev, 0, sizeof ev);
		ev.type = PEAK_EVENT_WINDOW_RESIZE;
		ev.resize.width = w->width;
		ev.resize.height = w->height;
		peak_q_push(&w->q, ev);
		return 0;
	}
	case WM_KEYDOWN: /* FALLTHROUGH */
	case WM_KEYUP: /* FALLTHROUGH */
	case WM_SYSKEYDOWN: /* FALLTHROUGH */
	case WM_SYSKEYUP:
		memset(&ev, 0, sizeof ev);
		ev.type = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) ? PEAK_EVENT_KEY_DOWN : PEAK_EVENT_KEY_UP;
		ev.key.key = peak_internal_win32_key_map(wparam);
		ev.key.mod = peak_internal_win32_mod_map();
		peak_q_push(&w->q, ev);
		return 0;
	case WM_CHAR: {
		char utf8[8];
		int n;

		n = WideCharToMultiByte(CP_UTF8, 0, (wchar_t *)&wparam, 1, utf8, (int)sizeof utf8, NULL, NULL);
		if (n > 0 && wparam >= 32) {
			peak_text_store(utf8, (size_t)n);
			memset(&ev, 0, sizeof ev);
			ev.type = PEAK_EVENT_TEXT;
			ev.text.n = (size_t)n;
			peak_q_push(&w->q, ev);
		}
		return 0;
	}
	case WM_MOUSEWHEEL:
		memset(&ev, 0, sizeof ev);
		ev.type = PEAK_EVENT_POINTER;
		ev.pointer.state = PEAK_POINTER_PRESSED;
		ev.pointer.type = ((short)HIWORD(wparam) > 0) ? PEAK_POINTER_WHEEL_UP : PEAK_POINTER_WHEEL_DOWN;
		ev.pointer.x = (float)(short)LOWORD(lparam);
		ev.pointer.y = (float)(short)HIWORD(lparam);
		ev.pointer.mod = peak_internal_win32_mod_map();
		peak_q_push(&w->q, ev);
		return 0;
	case WM_DROPFILES:
		if (peak_DragQueryFileA) {
			char path[MAX_PATH];
			UINT n;

			n = peak_DragQueryFileA((HDROP)wparam, 0, path, MAX_PATH);
			if (n) {
				peak_drop_store(path, n);
				memset(&ev, 0, sizeof ev);
				ev.type = PEAK_EVENT_DROP;
				ev.drop.n = n;
				peak_q_push(&w->q, ev);
			}
			if (peak_DragFinish)
				peak_DragFinish((HDROP)wparam);
		}
		return 0;
	case WM_MOUSEMOVE: /* FALLTHROUGH */
	case WM_LBUTTONDOWN: /* FALLTHROUGH */
	case WM_LBUTTONUP: /* FALLTHROUGH */
	case WM_RBUTTONDOWN: /* FALLTHROUGH */
	case WM_RBUTTONUP: /* FALLTHROUGH */
	case WM_MBUTTONDOWN: /* FALLTHROUGH */
	case WM_MBUTTONUP:
		memset(&ev, 0, sizeof ev);
		ev.type = PEAK_EVENT_POINTER;
		ev.pointer.x = (float)(short)LOWORD(lparam);
		ev.pointer.y = (float)(short)HIWORD(lparam);
		if (w->relative && msg == WM_MOUSEMOVE) {
			ev.pointer.x -= w->last_x;
			ev.pointer.y -= w->last_y;
		}
		w->last_x = (float)(short)LOWORD(lparam);
		w->last_y = (float)(short)HIWORD(lparam);
		if (msg == WM_MOUSEMOVE) {
			ev.pointer.state = PEAK_POINTER_MOVED;
			ev.pointer.type = (wparam & MK_RBUTTON) ? PEAK_POINTER_RIGHT :
			                  (wparam & MK_MBUTTON) ? PEAK_POINTER_MIDDLE : PEAK_POINTER_LEFT;
		} else if (msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN || msg == WM_MBUTTONDOWN) {
			ev.pointer.state = PEAK_POINTER_PRESSED;
			ev.pointer.type = (msg == WM_RBUTTONDOWN) ? PEAK_POINTER_RIGHT :
			                  (msg == WM_MBUTTONDOWN) ? PEAK_POINTER_MIDDLE : PEAK_POINTER_LEFT;
		} else {
			ev.pointer.state = PEAK_POINTER_RELEASED;
			ev.pointer.type = (msg == WM_RBUTTONUP) ? PEAK_POINTER_RIGHT :
			                  (msg == WM_MBUTTONUP) ? PEAK_POINTER_MIDDLE : PEAK_POINTER_LEFT;
		}
		ev.pointer.mod = peak_internal_win32_mod_map();
		peak_q_push(&w->q, ev);
		if (w->relative && msg == WM_MOUSEMOVE && peak_user32.SetCursorPos) {
			POINT pt;
			RECT rc;

			peak_user32.GetClientRect(w->hwnd, &rc);
			pt.x = (rc.right - rc.left) / 2;
			pt.y = (rc.bottom - rc.top) / 2;
			peak_user32.ClientToScreen(w->hwnd, &pt);
			peak_user32.SetCursorPos(pt.x, pt.y);
			w->last_x = (float)((rc.right - rc.left) / 2);
			w->last_y = (float)((rc.bottom - rc.top) / 2);
		}
		return 0;
	case WM_PAINT: {
		PAINTSTRUCT ps;

		peak_user32.BeginPaint(hwnd, &ps);
		peak_user32.EndPaint(hwnd, &ps);
		return 0;
	}
	case WM_ERASEBKGND:
		return 1;
	case WM_DESTROY:
		peak_user32.RemovePropA(hwnd, PEAK_WIN32_PROP);
		return 0;
	default:
		return peak_user32.DefWindowProcA(hwnd, msg, wparam, lparam);
	}
}

static int
peak_platform_init(void)
{
	WNDCLASSEXA wc;

	if (!peak_user32.CreateWindowExA) {
		if (!(peak_win32.user32 = LoadLibraryA(PEAK_WIN32_USER32))) {
			fputs("Failed to load user32.dll. What system are you fucking using and abusing?", stderr);
			return 0;
		}
		if (!peak_internal_user32_load(peak_win32.user32)) {
			fputs("Failed to load user32 symbols", stderr);
			return 0;
		}
	}
	if (!peak_gdi32.StretchDIBits) {
		if (!(peak_win32.gdi32 = LoadLibraryA(PEAK_WIN32_GDI32))) {
			fputs("Failed to load gdi32.dll. What system are you fucking using and abusing?", stderr);
			return 0;
		}
		if (!peak_internal_gdi32_load(peak_win32.gdi32)) {
			fputs("Failed to load gdi32 symbols", stderr);
			return 0;
		}
	}
	if (!peak_DragAcceptFiles) {
		HMODULE sh;

		sh = LoadLibraryA("shell32.dll");
		if (sh) {
			peak_DragAcceptFiles = (void (WINAPI *)(HWND, BOOL))(void *)GetProcAddress(sh, "DragAcceptFiles");
			peak_DragQueryFileA = (UINT (WINAPI *)(HDROP, UINT, LPSTR, UINT))(void *)GetProcAddress(sh, "DragQueryFileA");
			peak_DragFinish = (void (WINAPI *)(HDROP))(void *)GetProcAddress(sh, "DragFinish");
		}
	}
	if (!peak_win32.class_reg) {
		memset(&wc, 0, sizeof wc);
		wc.cbSize = sizeof wc;
		wc.style = CS_OWNDC;
		wc.lpfnWndProc = peak_internal_win32_wndproc;
		wc.hInstance = GetModuleHandleA(NULL);
		wc.hCursor = peak_user32.LoadCursorA(NULL, IDC_ARROW);
		wc.lpszClassName = PEAK_WIN32_CLASS;
		if (!peak_user32.RegisterClassExA(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
			fputs("Failed to register window class", stderr);
			return 0;
		}
		peak_win32.class_reg = 1;
	}
	return 1;
}

static void
peak_platform_quit(void)
{
	if (!peak_win32.class_reg)
		return;
	peak_user32.UnregisterClassA(PEAK_WIN32_CLASS, GetModuleHandleA(NULL));
	peak_win32.class_reg = 0;
}

static PeakWindowInternal
peak_platform_window_open(const char *name, uint32_t width, uint32_t height, uint32_t flags)
{
	PeakWindowInternal intern = {0};
	struct peak_win32_win *w;
	DWORD style, ex;
	RECT r;
	HINSTANCE inst;

	if (!peak_user32.CreateWindowExA && !peak_platform_init())
		return intern;

	if (!(w = calloc(1, sizeof *w)))
		return intern;
	if (!peak_internal_win32_buffer(w, width, height)) {
		free(w);
		return intern;
	}

	ex = (flags & PEAK_WINDOW_TRANSPARENT) ? WS_EX_LAYERED : 0;
	style = (flags & PEAK_WINDOW_FULLSCREEN) ? (WS_POPUP | WS_VISIBLE) : WS_OVERLAPPEDWINDOW;
	r.left = 0;
	r.top = 0;
	r.right = (LONG)width;
	r.bottom = (LONG)height;
	peak_user32.AdjustWindowRectEx(&r, style, FALSE, ex);
	inst = GetModuleHandleA(NULL);
	w->hwnd = peak_user32.CreateWindowExA(ex, PEAK_WIN32_CLASS, name, style,
		CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
		NULL, NULL, inst, w);
	if (!w->hwnd) {
		free(w->buffer);
		free(w);
		return intern;
	}

	w->hdc = peak_user32.GetDC(w->hwnd);
	if (!w->hdc) {
		peak_user32.DestroyWindow(w->hwnd);
		free(w->buffer);
		free(w);
		return intern;
	}

	w->flags = (int)flags;
	w->cursor_on = 1;
	w->layered = !!(flags & PEAK_WINDOW_TRANSPARENT);
	w->style = style;
	w->ex = ex;
	if (peak_DragAcceptFiles)
		peak_DragAcceptFiles(w->hwnd, TRUE);
	peak_user32.ShowWindow(w->hwnd, SW_SHOWNORMAL);
	if (flags & PEAK_WINDOW_FULLSCREEN) {
		intern.w = w;
		peak_platform_window_fullscreen(&intern, 1);
		return intern;
	}
	intern.w = w;
	return intern;
}

static void
peak_platform_window_close(PeakWindowInternal *intern)
{
	struct peak_win32_win *w;

	if (!intern || !intern->w)
		return;
	w = intern->w;
	if (w->hdc && w->hwnd)
		peak_user32.ReleaseDC(w->hwnd, w->hdc);
	if (w->hwnd) {
		peak_user32.RemovePropA(w->hwnd, PEAK_WIN32_PROP);
		peak_user32.DestroyWindow(w->hwnd);
	}
	free(w->buffer);
	free(w);
	intern->w = NULL;
}

static uint32_t *
peak_platform_window_buffer(PeakWindowInternal *intern, size_t *width, size_t *height)
{
	struct peak_win32_win *w;

	w = intern ? intern->w : NULL;
	if (!w) {
		*width = 0;
		*height = 0;
		return NULL;
	}
	*width = w->width;
	*height = w->height;
	return w->buffer;
}

static void
peak_platform_window_present(PeakWindowInternal *intern)
{
	struct peak_win32_win *w;
	BITMAPINFO bmi;

	w = intern ? intern->w : NULL;
	if (!w || !w->hwnd || !w->hdc || !w->buffer)
		return;

	memset(&bmi, 0, sizeof bmi);
	bmi.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = (LONG)w->width;
	bmi.bmiHeader.biHeight = -(LONG)w->height;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;
	if (w->layered && peak_user32.UpdateLayeredWindow && peak_gdi32.CreateCompatibleDC) {
		HDC mem;
		HBITMAP dib, old;
		void *bits;
		SIZE size;
		POINT dst, src;
		BLENDFUNCTION blend;

		mem = peak_gdi32.CreateCompatibleDC(w->hdc);
		if (!mem)
			return;
		dib = peak_gdi32.CreateDIBSection(mem, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
		if (!dib) {
			peak_gdi32.DeleteDC(mem);
			return;
		}
		old = peak_gdi32.SelectObject(mem, dib);
		if (bits)
			memcpy(bits, w->buffer, (size_t)w->width * w->height * 4);
		dst.x = dst.y = 0;
		src.x = src.y = 0;
		size.cx = (LONG)w->width;
		size.cy = (LONG)w->height;
		blend.BlendOp = AC_SRC_OVER;
		blend.BlendFlags = 0;
		blend.SourceConstantAlpha = 255;
		blend.AlphaFormat = AC_SRC_ALPHA;
		{
			RECT wr;

			peak_user32.GetWindowRect(w->hwnd, &wr);
			dst.x = wr.left;
			dst.y = wr.top;
		}
		peak_user32.UpdateLayeredWindow(w->hwnd, w->hdc, &dst, &size, mem, &src, 0, &blend, ULW_ALPHA);
		peak_gdi32.SelectObject(mem, old);
		peak_gdi32.DeleteObject(dib);
		peak_gdi32.DeleteDC(mem);
		return;
	}
	peak_gdi32.StretchDIBits(w->hdc,
		0, 0, (int)w->width, (int)w->height,
		0, 0, (int)w->width, (int)w->height,
		w->buffer, &bmi, DIB_RGB_COLORS, SRCCOPY);
}

static int
peak_platform_drop_drag(PeakWindowInternal *intern, const char *utf8, size_t n)
{
	(void)intern;
	(void)utf8;
	(void)n;
	return 0;
}

static int
peak_platform_clip_set(PeakWindowInternal *intern, PeakClip which, const char *utf8, size_t n)
{
	struct peak_win32_win *w;
	wchar_t *wide;
	int wlen;
	HGLOBAL mem;
	wchar_t *lock;

	(void)which;
	w = intern ? intern->w : NULL;
	if (!w || !w->hwnd || !peak_user32.OpenClipboard)
		return 0;
	wlen = MultiByteToWideChar(CP_UTF8, 0, utf8 ? utf8 : "", n ? (int)n : 0, NULL, 0);
	if (wlen < 0)
		return 0;
	wide = malloc(((size_t)wlen + 1) * sizeof *wide);
	if (!wide)
		return 0;
	MultiByteToWideChar(CP_UTF8, 0, utf8 ? utf8 : "", n ? (int)n : 0, wide, wlen);
	wide[wlen] = 0;
	if (!peak_user32.OpenClipboard(w->hwnd)) {
		free(wide);
		return 0;
	}
	peak_user32.EmptyClipboard();
	mem = GlobalAlloc(GMEM_MOVEABLE, ((size_t)wlen + 1) * sizeof (wchar_t));
	if (!mem) {
		peak_user32.CloseClipboard();
		free(wide);
		return 0;
	}
	lock = GlobalLock(mem);
	if (!lock) {
		GlobalFree(mem);
		peak_user32.CloseClipboard();
		free(wide);
		return 0;
	}
	memcpy(lock, wide, ((size_t)wlen + 1) * sizeof (wchar_t));
	GlobalUnlock(mem);
	free(wide);
	if (!peak_user32.SetClipboardData(CF_UNICODETEXT, mem)) {
		GlobalFree(mem);
		peak_user32.CloseClipboard();
		return 0;
	}
	peak_user32.CloseClipboard();
	return 1;
}

static int
peak_platform_clip_request(PeakWindowInternal *intern, PeakClip which)
{
	struct peak_win32_win *w;
	HANDLE mem;
	wchar_t *lock;
	char *utf8;
	int n;
	PeakEvent ev;

	w = intern ? intern->w : NULL;
	if (!w || !w->hwnd || !peak_user32.OpenClipboard)
		return 0;
	if (!peak_user32.OpenClipboard(w->hwnd)) {
		const char *p;
		size_t pn;

		if (!peak_clip_own_get(which, &p, &pn))
			return 0;
		peak_clip_paste_store(which, p, pn);
		memset(&ev, 0, sizeof ev);
		ev.type = PEAK_EVENT_CLIP;
		ev.clip.which = which;
		ev.clip.n = pn;
		peak_q_push(&w->q, ev);
		return 1;
	}
	mem = peak_user32.GetClipboardData(CF_UNICODETEXT);
	if (!mem) {
		peak_user32.CloseClipboard();
		return 0;
	}
	lock = GlobalLock(mem);
	if (!lock) {
		peak_user32.CloseClipboard();
		return 0;
	}
	n = WideCharToMultiByte(CP_UTF8, 0, lock, -1, NULL, 0, NULL, NULL);
	if (n <= 0) {
		GlobalUnlock(mem);
		peak_user32.CloseClipboard();
		return 0;
	}
	utf8 = malloc((size_t)n);
	if (!utf8) {
		GlobalUnlock(mem);
		peak_user32.CloseClipboard();
		return 0;
	}
	WideCharToMultiByte(CP_UTF8, 0, lock, -1, utf8, n, NULL, NULL);
	GlobalUnlock(mem);
	peak_user32.CloseClipboard();
	if (n > 0 && utf8[n - 1] == 0)
		n--;
	if ((size_t)n > PEAK_CLIP_MAX)
		n = (int)PEAK_CLIP_MAX;
	peak_clip_paste_store(which, utf8, (size_t)n);
	free(utf8);
	memset(&ev, 0, sizeof ev);
	ev.type = PEAK_EVENT_CLIP;
	ev.clip.which = which;
	ev.clip.n = peak_clip.paste_n;
	peak_q_push(&w->q, ev);
	return 1;
}

static bool
peak_platform_epoll(PeakWindowInternal *intern, PeakEvent *ev)
{
	MSG msg;
	struct peak_win32_win *w;

	w = intern ? intern->w : NULL;
	if (!w || !w->hwnd)
		return 0;

	while (peak_user32.PeekMessageA(&msg, w->hwnd, 0, 0, PM_REMOVE)) {
		peak_user32.TranslateMessage(&msg);
		peak_user32.DispatchMessageA(&msg);
		if (peak_q_pop(&w->q, ev))
			return 1;
	}
	return peak_q_pop(&w->q, ev);
}

static int
peak_platform_fd(PeakWindowInternal *intern)
{
	(void)intern;
	return -1;
}

static int
peak_platform_pending(PeakWindowInternal *intern)
{
	struct peak_win32_win *w;
	MSG msg;

	w = intern ? intern->w : NULL;
	if (!w)
		return 0;
	if (w->q.n)
		return (int)w->q.n;
	if (peak_user32.PeekMessageA && peak_user32.PeekMessageA(&msg, w->hwnd, 0, 0, PM_NOREMOVE))
		return 1;
	return 0;
}

static int
peak_internal_winmm_load(void)
{
	HMODULE handle;

	if (peak_winmm.waveOutOpen)
		return 1;
	if (!(handle = LoadLibraryA(PEAK_WIN32_WINMM))) {
		fputs("Failed to load winmm.dll. What system are you fucking using and abusing?", stderr);
		return 0;
	}
#define X(name, ret, conv, args) peak_winmm.name = (ret (conv *) args)(void *)GetProcAddress(handle, #name);
	PEAK_WINMM_API(X)
#undef X
#define X(name, ret, conv, args) || !peak_winmm.name
	if (0 PEAK_WINMM_API(X)) {
		fputs("Failed to load winmm symbols", stderr);
		return 0;
	}
#undef X
	return 1;
}

static void
peak_internal_win32_audio_fill(int i)
{
	memset(peak_audio.pcm[i], 0, peak_audio.bytes);
	if (peak_audio.fill)
		peak_audio.fill(peak_audio.pcm[i], PEAK_AUDIO_FRAMES, peak_audio.userdata);
	peak_winmm.waveOutWrite(peak_audio.out, &peak_audio.hdr[i], sizeof (WAVEHDR));
}

static DWORD WINAPI
peak_internal_win32_audio_thread(LPVOID arg)
{
	int i, did;

	(void)arg;
	for (;;) {
		did = 0;
		if (!peak_audio.run)
			break;
		for (i = 0; i < PEAK_AUDIO_BUFFERS; i++) {
			if (peak_audio.hdr[i].dwFlags & WHDR_DONE) {
				peak_internal_win32_audio_fill(i);
				did = 1;
			}
		}
		if (did)
			continue;
		WaitForSingleObject(peak_audio.event, INFINITE);
		ResetEvent(peak_audio.event);
	}
	return 0;
}

static void
peak_platform_audio_stop(void)
{
	int i;

	peak_audio.run = 0;
	if (peak_audio.event)
		SetEvent(peak_audio.event);
	if (peak_audio.thread) {
		WaitForSingleObject(peak_audio.thread, INFINITE);
		CloseHandle(peak_audio.thread);
		peak_audio.thread = NULL;
	}
	if (peak_audio.out) {
		peak_winmm.waveOutReset(peak_audio.out);
		for (i = 0; i < PEAK_AUDIO_BUFFERS; i++) {
			if (peak_audio.hdr[i].dwFlags & WHDR_PREPARED)
				peak_winmm.waveOutUnprepareHeader(peak_audio.out, &peak_audio.hdr[i], sizeof (WAVEHDR));
			free(peak_audio.pcm[i]);
			peak_audio.pcm[i] = NULL;
			memset(&peak_audio.hdr[i], 0, sizeof peak_audio.hdr[i]);
		}
		peak_winmm.waveOutClose(peak_audio.out);
		peak_audio.out = NULL;
	}
	if (peak_audio.event) {
		CloseHandle(peak_audio.event);
		peak_audio.event = NULL;
	}
	peak_audio.fill = NULL;
	peak_audio.userdata = NULL;
}

static int
peak_platform_audio_start(uint32_t channels, uint32_t rate, void (*fill)(int16_t *out, size_t frames, void *userdata), void *userdata)
{
	WAVEFORMATEX fmt;
	int i;

	if (channels > 32)
		return 0;
	if (!peak_internal_winmm_load())
		return 0;

	memset(&fmt, 0, sizeof fmt);
	fmt.wFormatTag = WAVE_FORMAT_PCM;
	fmt.nChannels = (WORD)channels;
	fmt.nSamplesPerSec = rate;
	fmt.wBitsPerSample = 16;
	fmt.nBlockAlign = (WORD)(channels * 2);
	fmt.nAvgBytesPerSec = rate * fmt.nBlockAlign;

	peak_audio.event = CreateEventA(NULL, TRUE, FALSE, NULL);
	if (!peak_audio.event)
		return 0;
	if (peak_winmm.waveOutOpen(&peak_audio.out, WAVE_MAPPER, &fmt, (DWORD_PTR)peak_audio.event, 0, CALLBACK_EVENT) != MMSYSERR_NOERROR) {
		CloseHandle(peak_audio.event);
		peak_audio.event = NULL;
		fputs("Failed to open waveOut device", stderr);
		return 0;
	}

	peak_audio.channels = channels;
	peak_audio.bytes = (uint32_t)channels * PEAK_AUDIO_FRAMES * sizeof (int16_t);
	peak_audio.fill = fill;
	peak_audio.userdata = userdata;
	peak_audio.run = 1;

	for (i = 0; i < PEAK_AUDIO_BUFFERS; i++) {
		if (!(peak_audio.pcm[i] = calloc(1, peak_audio.bytes)))
			goto fail;
		memset(&peak_audio.hdr[i], 0, sizeof peak_audio.hdr[i]);
		peak_audio.hdr[i].lpData = (LPSTR)peak_audio.pcm[i];
		peak_audio.hdr[i].dwBufferLength = peak_audio.bytes;
		if (peak_winmm.waveOutPrepareHeader(peak_audio.out, &peak_audio.hdr[i], sizeof (WAVEHDR)) != MMSYSERR_NOERROR)
			goto fail;
		peak_audio.hdr[i].dwFlags |= WHDR_DONE;
	}

	for (i = 0; i < PEAK_AUDIO_BUFFERS; i++)
		peak_internal_win32_audio_fill(i);

	peak_audio.thread = CreateThread(NULL, 0, peak_internal_win32_audio_thread, NULL, 0, NULL);
	if (!peak_audio.thread)
		goto fail;
	return 1;

fail:
	peak_platform_audio_stop();
	return 0;
}

static uint64_t
peak_platform_get_time(void)
{
	LARGE_INTEGER count, freq;

	QueryPerformanceCounter(&count);
	QueryPerformanceFrequency(&freq);
	return (uint64_t)((count.QuadPart * 1000000000ull) / freq.QuadPart);
}

static void
peak_platform_sleep_ns(int64_t ns)
{
	if (ns <= 0)
		return;
	Sleep((DWORD)(ns / 1000000));
}

static const char **
peak_platform_vulkan_get_extensions(uint32_t *count)
{
	static const char *exts[] = {
		"VK_KHR_surface",
		"VK_KHR_win32_surface",
	};
	if (count)
		*count = 2;
	return exts;
}

static int
peak_platform_vulkan_create_surface(PeakWindowInternal *intern, void *instance, const void *allocator, void *out_surface)
{
#ifdef PEAK_VULKAN
	struct peak_win32_win *w;
	VkWin32SurfaceCreateInfoKHR ci;

	w = intern ? intern->w : NULL;
	if (!w || !w->hwnd)
		return 0;
	memset(&ci, 0, sizeof ci);
	ci.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	ci.hwnd = w->hwnd;
	ci.hinstance = GetModuleHandleA(NULL);
	return vkCreateWin32SurfaceKHR((VkInstance)instance, &ci, (const VkAllocationCallbacks *)allocator, (VkSurfaceKHR *)out_surface) == VK_SUCCESS;
#else
	(void)intern;
	(void)instance;
	(void)allocator;
	(void)out_surface;
	return 0;
#endif
}

static void
peak_platform_window_set_title(PeakWindowInternal *intern, const char *name)
{
	struct peak_win32_win *w;

	w = intern ? intern->w : NULL;
	if (!w || !w->hwnd || !name || !peak_user32.SetWindowTextA)
		return;
	peak_user32.SetWindowTextA(w->hwnd, name);
}

static void
peak_platform_window_set_size(PeakWindowInternal *intern, uint32_t width, uint32_t height)
{
	struct peak_win32_win *w;
	RECT r;

	w = intern ? intern->w : NULL;
	if (!w || !w->hwnd || !peak_user32.SetWindowPos)
		return;
	r.left = 0;
	r.top = 0;
	r.right = (LONG)width;
	r.bottom = (LONG)height;
	peak_user32.AdjustWindowRectEx(&r, w->style ? w->style : WS_OVERLAPPEDWINDOW, FALSE, w->ex);
	peak_user32.SetWindowPos(w->hwnd, NULL, 0, 0, r.right - r.left, r.bottom - r.top, SWP_NOMOVE | SWP_NOZORDER);
}

static void
peak_platform_window_fullscreen(PeakWindowInternal *intern, int on)
{
	struct peak_win32_win *w;
	int sw, sh;

	w = intern ? intern->w : NULL;
	if (!w || !w->hwnd || !peak_user32.SetWindowLongPtrA)
		return;
	if (on) {
		w->place.length = sizeof w->place;
		peak_user32.GetWindowPlacement(w->hwnd, &w->place);
		w->style = (DWORD)peak_user32.GetWindowLongPtrA(w->hwnd, GWL_STYLE);
		peak_user32.SetWindowLongPtrA(w->hwnd, GWL_STYLE, (LONG_PTR)((w->style & ~WS_OVERLAPPEDWINDOW) | WS_POPUP));
		sw = peak_user32.GetSystemMetrics(SM_CXSCREEN);
		sh = peak_user32.GetSystemMetrics(SM_CYSCREEN);
		peak_user32.SetWindowPos(w->hwnd, HWND_TOP, 0, 0, sw, sh, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
	} else {
		peak_user32.SetWindowLongPtrA(w->hwnd, GWL_STYLE, (LONG_PTR)(w->style | WS_OVERLAPPEDWINDOW));
		peak_user32.SetWindowPlacement(w->hwnd, &w->place);
		peak_user32.SetWindowPos(w->hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
	}
}

static void
peak_platform_window_cursor(PeakWindowInternal *intern, int on)
{
	struct peak_win32_win *w;

	w = intern ? intern->w : NULL;
	if (!w)
		return;
	if (on == w->cursor_on)
		return;
	w->cursor_on = on;
	if (peak_user32.ShowCursor)
		peak_user32.ShowCursor(on ? TRUE : FALSE);
}

static void
peak_platform_window_pointer_relative(PeakWindowInternal *intern, int on)
{
	struct peak_win32_win *w;

	w = intern ? intern->w : NULL;
	if (!w || !w->hwnd)
		return;
	w->relative = on;
	if (on && peak_user32.SetCapture)
		peak_user32.SetCapture(w->hwnd);
	else if (!on && peak_user32.ReleaseCapture)
		peak_user32.ReleaseCapture();
}

static float
peak_platform_window_scale(PeakWindowInternal *intern)
{
	(void)intern;
	return 1.f;
}

/* BEGIN p_win32_proc.c */
#include <fcntl.h>
#include <io.h>

#ifndef PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
#define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE 0x00020016
#endif

#ifndef HPCON
typedef HANDLE HPCON;
#endif

#define PEAK_PROC_MAX 32
#define PEAK_WAIT_SLICE 1

typedef HRESULT (WINAPI *PeakCreatePseudoConsole)(COORD, HANDLE, HANDLE, DWORD, HPCON *);
typedef HRESULT (WINAPI *PeakResizePseudoConsole)(HPCON, COORD);
typedef void (WINAPI *PeakClosePseudoConsole)(HPCON);

typedef struct PeakProcRec {
	HANDLE read;
	HANDLE write;
	HANDLE proc;
	HPCON pc;
	int pid;
} PeakProcRec;

static PeakProcRec peak_procs[PEAK_PROC_MAX];
static PeakCreatePseudoConsole peak_create_pc;
static PeakResizePseudoConsole peak_resize_pc;
static PeakClosePseudoConsole peak_close_pc;
static int peak_conpty_tried;
static char peak_pipe_name[MAX_PATH];
static int peak_stdout_saved = -1;

static PeakProc peak_internal_proc_fail(void);
static void peak_internal_conpty_load(void);
static PeakProcRec *peak_internal_proc_find(HANDLE h);
static PeakProcRec *peak_internal_proc_slot(void);
static void peak_internal_proc_clear(PeakProcRec *r);
static int peak_internal_join_argv(char *out, size_t cap, const char *file, const char **argv);
static HANDLE peak_internal_write_handle(PEAK_HANDLE fd);
static int peak_internal_pipe_ready(HANDLE h);
static void peak_internal_pipe_name(const char *path, char *out, size_t cap);
static size_t peak_internal_io_n(size_t n);

static PeakProc
peak_internal_proc_fail(void)
{
	PeakProc p;

	p.fd = PEAK_HANDLE_INVALID;
	p.pid = 0;
	return p;
}

static void
peak_internal_conpty_load(void)
{
	HMODULE k;

	if (peak_conpty_tried)
		return;
	peak_conpty_tried = 1;
	k = GetModuleHandleA("kernel32.dll");
	if (!k)
		return;
	peak_create_pc = (PeakCreatePseudoConsole)(void *)GetProcAddress(k, "CreatePseudoConsole");
	peak_resize_pc = (PeakResizePseudoConsole)(void *)GetProcAddress(k, "ResizePseudoConsole");
	peak_close_pc = (PeakClosePseudoConsole)(void *)GetProcAddress(k, "ClosePseudoConsole");
}

static PeakProcRec *
peak_internal_proc_find(HANDLE h)
{
	int i;

	if (!h || h == INVALID_HANDLE_VALUE)
		return NULL;
	for (i = 0; i < PEAK_PROC_MAX; i++) {
		if (peak_procs[i].read == h)
			return &peak_procs[i];
	}
	return NULL;
}

static PeakProcRec *
peak_internal_proc_slot(void)
{
	int i;

	for (i = 0; i < PEAK_PROC_MAX; i++) {
		if (!peak_procs[i].read)
			return &peak_procs[i];
	}
	return NULL;
}

static void
peak_internal_proc_clear(PeakProcRec *r)
{
	if (!r)
		return;
	if (r->write && r->write != r->read && r->write != INVALID_HANDLE_VALUE)
		CloseHandle(r->write);
	if (r->pc && peak_close_pc)
		peak_close_pc(r->pc);
	if (r->proc)
		CloseHandle(r->proc);
	memset(r, 0, sizeof *r);
}

static size_t
peak_internal_io_n(size_t n)
{
	if (n > (size_t)0x40000000)
		return (size_t)0x40000000;
	return n;
}

static int
peak_internal_join_argv(char *out, size_t cap, const char *file, const char **argv)
{
	size_t n;
	int i;

	if (!out || cap < 2)
		return 0;
	out[0] = 0;
	n = 0;
	if (file) {
		n = (size_t)snprintf(out, cap, "\"%s\"", file);
		if (n >= cap)
			return 0;
	}
	if (!argv)
		return 1;
	for (i = file ? 1 : 0; argv[i]; i++) {
		int w;

		w = snprintf(out + n, cap - n, "%s\"%s\"", n ? " " : "", argv[i]);
		if (w < 0 || (size_t)w >= cap - n)
			return 0;
		n += (size_t)w;
	}
	return 1;
}

static HANDLE
peak_internal_write_handle(PEAK_HANDLE fd)
{
	PeakProcRec *r;

	r = peak_internal_proc_find((HANDLE)fd);
	if (r && r->write && r->write != INVALID_HANDLE_VALUE)
		return r->write;
	return (HANDLE)fd;
}

static int
peak_internal_pipe_ready(HANDLE h)
{
	DWORD avail, flags;

	if (!h || h == INVALID_HANDLE_VALUE)
		return 0;
	if (PeekNamedPipe(h, NULL, 0, NULL, &avail, NULL))
		return avail > 0;
	if (GetNamedPipeHandleStateA(h, &flags, NULL, NULL, NULL, NULL, 0))
		return 0;
	return 1;
}

static void
peak_internal_pipe_name(const char *path, char *out, size_t cap)
{
	size_t i, n;

	if (!path)
		path = "peak";
	n = (size_t)snprintf(out, cap, "\\\\.\\pipe\\peak_");
	if (n >= cap) {
		out[0] = 0;
		return;
	}
	for (i = 0; path[i] && n + 1 < cap; i++) {
		char c;

		c = path[i];
		if (c == '\\' || c == '/' || c == ':' || c == ' ')
			c = '_';
		out[n++] = c;
	}
	out[n] = 0;
}

PeakProc
peak_pty_spawn(const char *file, const char **argv, uint32_t cols, uint32_t rows, uint32_t xpixel, uint32_t ypixel)
{
	PeakProc p;
	PeakProcRec *rec;
	HANDLE in_r, in_w, out_r, out_w;
	HPCON pc;
	COORD size;
	SIZE_T attr_n;
	STARTUPINFOEXA si;
	PROCESS_INFORMATION pi;
	char cmd[1024];
	SECURITY_ATTRIBUTES sa;

	(void)xpixel;
	(void)ypixel;
	p = peak_internal_proc_fail();
	if (!file || !argv)
		return p;
	peak_internal_conpty_load();
	if (!peak_create_pc)
		return p;
	rec = peak_internal_proc_slot();
	if (!rec)
		return p;
	if (!peak_internal_join_argv(cmd, sizeof cmd, file, argv))
		return p;
	memset(&sa, 0, sizeof sa);
	sa.nLength = sizeof sa;
	sa.bInheritHandle = TRUE;
	in_r = in_w = out_r = out_w = NULL;
	if (!CreatePipe(&in_r, &in_w, &sa, 0) || !CreatePipe(&out_r, &out_w, &sa, 0))
		goto fail_pipes;
	size.X = cols ? (SHORT)cols : 80;
	size.Y = rows ? (SHORT)rows : 24;
	if (peak_create_pc(size, in_r, out_w, 0, &pc) != S_OK)
		goto fail_pipes;
	CloseHandle(in_r);
	in_r = NULL;
	CloseHandle(out_w);
	out_w = NULL;
	memset(&si, 0, sizeof si);
	si.StartupInfo.cb = sizeof si;
	attr_n = 0;
	InitializeProcThreadAttributeList(NULL, 1, 0, &attr_n);
	si.lpAttributeList = malloc(attr_n);
	if (!si.lpAttributeList || !InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &attr_n))
		goto fail_pc;
	if (!UpdateProcThreadAttribute(si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, pc, sizeof pc, NULL, NULL))
		goto fail_attr;
	memset(&pi, 0, sizeof pi);
	if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, EXTENDED_STARTUPINFO_PRESENT, NULL, NULL, &si.StartupInfo, &pi))
		goto fail_attr;
	CloseHandle(pi.hThread);
	DeleteProcThreadAttributeList(si.lpAttributeList);
	free(si.lpAttributeList);
	rec->read = out_r;
	rec->write = in_w;
	rec->proc = pi.hProcess;
	rec->pc = pc;
	rec->pid = (int)pi.dwProcessId;
	p.fd = out_r;
	p.pid = rec->pid;
	return p;

fail_attr:
	if (si.lpAttributeList) {
		DeleteProcThreadAttributeList(si.lpAttributeList);
		free(si.lpAttributeList);
	}
fail_pc:
	if (peak_close_pc)
		peak_close_pc(pc);
fail_pipes:
	if (in_r)
		CloseHandle(in_r);
	if (in_w)
		CloseHandle(in_w);
	if (out_r)
		CloseHandle(out_r);
	if (out_w)
		CloseHandle(out_w);
	return p;
}

void
peak_pty_resize(PeakProc *pty, uint32_t cols, uint32_t rows, uint32_t xpixel, uint32_t ypixel)
{
	PeakProcRec *r;
	COORD size;

	(void)xpixel;
	(void)ypixel;
	if (!pty)
		return;
	r = peak_internal_proc_find((HANDLE)pty->fd);
	if (!r || !r->pc || !peak_resize_pc)
		return;
	size.X = cols ? (SHORT)cols : 80;
	size.Y = rows ? (SHORT)rows : 24;
	peak_resize_pc(r->pc, size);
}

int
peak_pty_reap(PeakProc *pty)
{
	PeakProcRec *r;
	DWORD code;

	if (!pty || pty->pid <= 0)
		return 0;
	r = peak_internal_proc_find((HANDLE)pty->fd);
	if (!r || !r->proc)
		return 0;
	if (WaitForSingleObject(r->proc, 0) != WAIT_OBJECT_0)
		return 0;
	GetExitCodeProcess(r->proc, &code);
	pty->pid = 0;
	return 1;
}

void
peak_pty_close(PeakProc *pty)
{
	PeakProcRec *r;

	if (!pty)
		return;
	r = peak_internal_proc_find((HANDLE)pty->fd);
	if (r) {
		if (r->proc)
			WaitForSingleObject(r->proc, INFINITE);
		if (r->read)
			CloseHandle(r->read);
		peak_internal_proc_clear(r);
	} else if (pty->fd != PEAK_HANDLE_INVALID) {
		CloseHandle((HANDLE)pty->fd);
	}
	pty->fd = PEAK_HANDLE_INVALID;
	pty->pid = 0;
}

int
peak_wait(PeakWindow *win, const PEAK_HANDLE *fds, uint32_t n, int timeout_ms)
{
	DWORD left, slice, got;
	uint32_t i;

	left = timeout_ms < 0 ? INFINITE : (DWORD)timeout_ms;
	for (;;) {
		if (win && peak_window_pending(win) > 0)
			return 1;
		for (i = 0; i < n; i++) {
			if (fds && peak_internal_pipe_ready((HANDLE)fds[i]))
				return 1;
		}
		if (timeout_ms == 0)
			return 0;
		slice = PEAK_WAIT_SLICE;
		if (left != INFINITE) {
			if (left == 0)
				return 0;
			if (left < slice)
				slice = left;
		}
		if (win)
			got = MsgWaitForMultipleObjects(0, NULL, FALSE, slice, QS_ALLINPUT);
		else
			got = WaitForSingleObject(GetCurrentProcess(), slice);
		(void)got;
		if (left != INFINITE) {
			if (left <= slice)
				return 0;
			left -= slice;
		}
	}
}

int
peak_runtime_dir(char *buf, size_t cap, const char *app)
{
	char root[MAX_PATH];
	DWORD n;

	if (!buf || cap < 2 || !app || !app[0])
		return 0;
	n = GetEnvironmentVariableA("LOCALAPPDATA", root, MAX_PATH);
	if (!n || n >= MAX_PATH) {
		n = GetTempPathA(MAX_PATH, root);
		if (!n || n >= MAX_PATH)
			return 0;
	}
	if (snprintf(buf, cap, "%s\\%s", root, app) < 0 || strlen(buf) >= cap)
		return 0;
	if (!CreateDirectoryA(buf, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
		return 0;
	return 1;
}

PEAK_HANDLE
peak_sock_listen(const char *path)
{
	HANDLE h;

	peak_internal_pipe_name(path, peak_pipe_name, sizeof peak_pipe_name);
	if (!peak_pipe_name[0])
		return PEAK_HANDLE_INVALID;
	h = CreateNamedPipeA(peak_pipe_name, PIPE_ACCESS_DUPLEX,
		PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_NOWAIT, PIPE_UNLIMITED_INSTANCES,
		4096, 4096, 0, NULL);
	if (h == INVALID_HANDLE_VALUE)
		return PEAK_HANDLE_INVALID;
	return h;
}

PEAK_HANDLE
peak_sock_accept(PEAK_HANDLE listen_fd)
{
	HANDLE h;
	DWORD err;

	(void)listen_fd;
	if (!peak_pipe_name[0])
		return PEAK_HANDLE_INVALID;
	h = CreateNamedPipeA(peak_pipe_name, PIPE_ACCESS_DUPLEX,
		PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_NOWAIT, PIPE_UNLIMITED_INSTANCES,
		4096, 4096, 0, NULL);
	if (h == INVALID_HANDLE_VALUE)
		return PEAK_HANDLE_INVALID;
	if (ConnectNamedPipe(h, NULL))
		return h;
	err = GetLastError();
	if (err == ERROR_PIPE_CONNECTED)
		return h;
	CloseHandle(h);
	return PEAK_HANDLE_INVALID;
}

PEAK_HANDLE
peak_sock_connect(const char *path)
{
	char name[MAX_PATH];
	HANDLE h;
	DWORD mode;

	peak_internal_pipe_name(path, name, sizeof name);
	if (!name[0])
		return PEAK_HANDLE_INVALID;
	h = CreateFileA(name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (h == INVALID_HANDLE_VALUE)
		return PEAK_HANDLE_INVALID;
	mode = PIPE_READMODE_BYTE | PIPE_NOWAIT;
	SetNamedPipeHandleState(h, &mode, NULL, NULL);
	return h;
}

int
peak_sock_send(PEAK_HANDLE sock, const void *buf, size_t n, PEAK_HANDLE pass)
{
	(void)pass;
	return peak_fd_write(sock, buf, n) > 0;
}

int
peak_sock_recv(PEAK_HANDLE sock, void *buf, size_t n, PEAK_HANDLE *pass)
{
	if (pass)
		*pass = PEAK_HANDLE_INVALID;
	return peak_fd_read(sock, buf, n);
}

int
peak_pointer_pid(PeakWindow *win)
{
	(void)win;
	return 0;
}

int
peak_pointer_local(PeakWindow *win, int *x, int *y)
{
	(void)win;
	(void)x;
	(void)y;
	return 0;
}

int
peak_filesystem_mkdir(const char *path)
{
	if (!path || !path[0])
		return 0;
	return CreateDirectoryA(path, NULL) != 0;
}

int
peak_filesystem_rm(const char *path)
{
	DWORD attr;

	if (!path || !path[0])
		return 0;
	attr = GetFileAttributesA(path);
	if (attr == INVALID_FILE_ATTRIBUTES)
		return 0;
	if (attr & FILE_ATTRIBUTE_DIRECTORY)
		return RemoveDirectoryA(path) != 0;
	return DeleteFileA(path) != 0;
}

int
peak_filesystem_cwd(char *buf, size_t cap)
{
	if (!buf || cap < 2)
		return 0;
	return GetCurrentDirectoryA((DWORD)cap, buf) != 0;
}

int
peak_filesystem_chdir(const char *path)
{
	if (!path || !path[0])
		return 0;
	return SetCurrentDirectoryA(path) != 0;
}

int
peak_filesystem_rename(const char *from, const char *to)
{
	if (!from || !from[0] || !to || !to[0])
		return 0;
	return MoveFileExA(from, to, MOVEFILE_REPLACE_EXISTING) != 0;
}

int
peak_fd_read(PEAK_HANDLE fd, void *buf, size_t n)
{
	DWORD got;
	HANDLE h;

	if (fd == PEAK_HANDLE_INVALID || !buf)
		return 0;
	h = (HANDLE)fd;
	n = peak_internal_io_n(n);
	if (!PeekNamedPipe(h, NULL, 0, NULL, &got, NULL)) {
		if (!ReadFile(h, buf, (DWORD)n, &got, NULL))
			return 0;
		return got ? (int)got : 0;
	}
	if (!got)
		return -1;
	if (got > n)
		got = (DWORD)n;
	if (!ReadFile(h, buf, got, &got, NULL))
		return 0;
	return got ? (int)got : 0;
}

int
peak_fd_write(PEAK_HANDLE fd, const void *buf, size_t n)
{
	DWORD got;
	HANDLE h;

	if (fd == PEAK_HANDLE_INVALID || !buf)
		return 0;
	h = peak_internal_write_handle(fd);
	n = peak_internal_io_n(n);
	if (!WriteFile(h, buf, (DWORD)n, &got, NULL))
		return 0;
	return got ? (int)got : 0;
}

size_t
peak_pipe_capacity(PEAK_HANDLE fd)
{
	(void)fd;
	return 0;
}

size_t
peak_pipe_set_capacity(PEAK_HANDLE fd, size_t n)
{
	(void)fd;
	(void)n;
	return 0;
}

void
peak_fd_close(PEAK_HANDLE fd)
{
	PeakProcRec *r;

	if (fd == PEAK_HANDLE_INVALID)
		return;
	r = peak_internal_proc_find((HANDLE)fd);
	if (r) {
		if (r->read)
			CloseHandle(r->read);
		r->read = NULL;
		if (r->write && r->write != INVALID_HANDLE_VALUE) {
			CloseHandle(r->write);
			r->write = NULL;
		}
		return;
	}
	CloseHandle((HANDLE)fd);
}

PeakProc
peak_job_run(const char *cmd, const char *cwd)
{
	PeakProc p;
	PeakProcRec *rec;
	HANDLE rd, wr, nul;
	SECURITY_ATTRIBUTES sa;
	STARTUPINFOA si;
	PROCESS_INFORMATION pi;
	char line[1024];

	p = peak_internal_proc_fail();
	if (!cmd || !cmd[0])
		return p;
	rec = peak_internal_proc_slot();
	if (!rec)
		return p;
	if (snprintf(line, sizeof line, "cmd.exe /c %s", cmd) < 0)
		return p;
	memset(&sa, 0, sizeof sa);
	sa.nLength = sizeof sa;
	sa.bInheritHandle = TRUE;
	rd = wr = nul = NULL;
	if (!CreatePipe(&rd, &wr, &sa, 0))
		return p;
	SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
	nul = CreateFileA("NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);
	memset(&si, 0, sizeof si);
	si.cb = sizeof si;
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdInput = nul ? nul : GetStdHandle(STD_INPUT_HANDLE);
	si.hStdOutput = wr;
	si.hStdError = wr;
	memset(&pi, 0, sizeof pi);
	if (!CreateProcessA(NULL, line, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL,
			cwd && cwd[0] ? cwd : NULL, &si, &pi)) {
		CloseHandle(rd);
		CloseHandle(wr);
		if (nul)
			CloseHandle(nul);
		return p;
	}
	CloseHandle(wr);
	if (nul)
		CloseHandle(nul);
	CloseHandle(pi.hThread);
	rec->read = rd;
	rec->write = NULL;
	rec->proc = pi.hProcess;
	rec->pc = NULL;
	rec->pid = (int)pi.dwProcessId;
	p.fd = rd;
	p.pid = rec->pid;
	return p;
}

int
peak_job_reap(PeakProc *job, int *code)
{
	PeakProcRec *r;
	DWORD ec;

	if (!job || job->pid <= 0)
		return 0;
	r = peak_internal_proc_find((HANDLE)job->fd);
	if (!r || !r->proc)
		return 0;
	if (WaitForSingleObject(r->proc, 0) != WAIT_OBJECT_0)
		return 0;
	if (code) {
		GetExitCodeProcess(r->proc, &ec);
		*code = (int)ec;
	}
	job->pid = 0;
	return 1;
}

void
peak_job_kill(PeakProc *job)
{
	PeakProcRec *r;

	if (!job)
		return;
	r = peak_internal_proc_find((HANDLE)job->fd);
	if (r) {
		if (r->proc)
			TerminateProcess(r->proc, 1);
		if (r->read)
			CloseHandle(r->read);
		peak_internal_proc_clear(r);
	} else if (job->fd != PEAK_HANDLE_INVALID) {
		CloseHandle((HANDLE)job->fd);
	}
	job->fd = PEAK_HANDLE_INVALID;
	job->pid = 0;
}

int
peak_pid_cwd(int pid, char *buf, size_t cap)
{
	if (!buf || cap < 2)
		return 0;
	if (pid != (int)GetCurrentProcessId())
		return 0;
	return GetCurrentDirectoryA((DWORD)cap, buf) != 0;
}

size_t
peak_page_size(void)
{
	SYSTEM_INFO si;

	GetSystemInfo(&si);
	return si.dwPageSize ? (size_t)si.dwPageSize : 4096;
}

void *
peak_mirror_map(size_t size)
{
	HANDLE map;
	void *base;
	void *a, *b;
	int i;

	if (!size || size % peak_page_size())
		return NULL;
	if (size > 0x7fffffff)
		return NULL;
	map = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, (DWORD)size, NULL);
	if (!map)
		return NULL;
	for (i = 0; i < 16; i++) {
		base = VirtualAlloc(NULL, size * 2, MEM_RESERVE, PAGE_NOACCESS);
		if (!base)
			break;
		VirtualFree(base, 0, MEM_RELEASE);
		a = MapViewOfFileEx(map, FILE_MAP_ALL_ACCESS, 0, 0, size, base);
		if (!a)
			continue;
		b = MapViewOfFileEx(map, FILE_MAP_ALL_ACCESS, 0, 0, size, (char *)base + size);
		if (b) {
			CloseHandle(map);
			return base;
		}
		UnmapViewOfFile(a);
	}
	CloseHandle(map);
	return NULL;
}

void
peak_mirror_unmap(void *p, size_t size)
{
	if (!p || !size)
		return;
	UnmapViewOfFile(p);
	UnmapViewOfFile((char *)p + size);
}

int
peak_pid(void)
{
	return (int)GetCurrentProcessId();
}

int
peak_env_set(const char *name, const char *value)
{
	if (!name || !name[0])
		return 0;
	return SetEnvironmentVariableA(name, value) != 0;
}

int
peak_env_get(const char *name, char *buf, size_t cap)
{
	DWORD n;

	if (!name || !name[0] || !buf || cap < 2 || cap > 0x7fffffff)
		return 0;
	n = GetEnvironmentVariableA(name, buf, (DWORD)cap);
	if (n == 0 || n >= cap)
		return 0;
	return buf[0] != 0;
}

int
peak_filesystem_list(const char *path, int (*fn)(const char *name, void *ud), void *ud)
{
	char pat[MAX_PATH];
	WIN32_FIND_DATAA fd;
	HANDLE h;

	if (!path || !path[0] || !fn)
		return 0;
	if (snprintf(pat, sizeof pat, "%s\\*", path) < 0)
		return 0;
	h = FindFirstFileA(pat, &fd);
	if (h == INVALID_HANDLE_VALUE)
		return 0;
	do {
		if (fn(fd.cFileName, ud) == 0)
			break;
	} while (FindNextFileA(h, &fd));
	FindClose(h);
	return 1;
}

int
peak_filesystem_symlink(const char *target, const char *path)
{
	(void)target;
	(void)path;
	return 0;
}

int
peak_filesystem_readlink(const char *path, char *dst, size_t cap)
{
	(void)path;
	(void)dst;
	(void)cap;
	return 0;
}

int
peak_child_arm(void)
{
	return 1;
}

void
peak_child_disarm(void)
{
}

PEAK_HANDLE
peak_child_fd(void)
{
	return PEAK_HANDLE_INVALID;
}

void
peak_child_ack(void)
{
}

int
peak_usr1_arm(void)
{
	return 1;
}

void
peak_usr1_disarm(void)
{
}

PEAK_HANDLE
peak_usr1_fd(void)
{
	return PEAK_HANDLE_INVALID;
}

int
peak_usr1_ack(void)
{
	return 0;
}

int
peak_child_reap(int *pid, int *code)
{
	(void)pid;
	(void)code;
	return 0;
}

int
peak_stdout_silence(void)
{
	int nfd;

	if (peak_stdout_saved >= 0)
		return 1;
	peak_stdout_saved = _dup(_fileno(stdout));
	if (peak_stdout_saved < 0)
		return 0;
	nfd = _open("NUL", _O_WRONLY);
	if (nfd < 0) {
		_close(peak_stdout_saved);
		peak_stdout_saved = -1;
		return 0;
	}
	if (_dup2(nfd, _fileno(stdout)) < 0) {
		_close(nfd);
		_close(peak_stdout_saved);
		peak_stdout_saved = -1;
		return 0;
	}
	_close(nfd);
	return 1;
}

int
peak_stdout_restore(void)
{
	if (peak_stdout_saved < 0)
		return 0;
	fflush(stdout);
	_dup2(peak_stdout_saved, _fileno(stdout));
	_close(peak_stdout_saved);
	peak_stdout_saved = -1;
	return 1;
}
/* END p_win32_proc.c */
/* END p_win32.c */
#elif defined(PEAK_LINUX)
/* BEGIN p_linux.c */
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/joystick.h>
#ifndef PEAK_NO_AUDIO
#include <pthread.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#ifdef PEAK_VULKAN
#define VK_USE_PLATFORM_WAYLAND_KHR
#define VK_USE_PLATFORM_XLIB_KHR
#include <vulkan/vulkan.h>
#endif

#define PEAK_X11_LINUX "libX11.so.6"
#ifndef PEAK_NO_AUDIO
#define PEAK_PULSE_LINUX "libpulse-simple.so.0"
#define PEAK_AUDIO_FRAMES 256

#define PEAK_PULSE_API(X) \
    X(pa_simple_new,   void *, (const char *, const char *, int, const char *, const char *, const void *, const void *, const void *, int *)) \
    X(pa_simple_free,  void,   (void *)) \
    X(pa_simple_write, int,    (void *, const void *, size_t, int *))
#endif

#define PEAK_X11_API(X) \
	X(XOpenDisplay,        Display *, (const char *)) \
	X(XCloseDisplay,       int, (Display *)) \
	X(XCreateSimpleWindow, Window, (Display *, Window, int, int, unsigned int, unsigned int, unsigned int, unsigned long, unsigned long)) \
	X(XCreateWindow,       Window, (Display *, Window, int, int, unsigned int, unsigned int, unsigned int, int, unsigned int, Visual *, unsigned long, XSetWindowAttributes *)) \
	X(XGetVisualInfo,      XVisualInfo *, (Display *, long, XVisualInfo *, int *)) \
	X(XCreateColormap,     Colormap, (Display *, Window, Visual *, int)) \
	X(XFreeColormap,       int, (Display *, Colormap)) \
	X(XFree,               int, (void *)) \
	X(XStoreName,          int, (Display *, Window, const char *)) \
	X(XInternAtom,         Atom, (Display *, const char *, Bool)) \
	X(XSetWMProtocols,     Status, (Display *, Window, Atom *, int)) \
	X(XSelectInput,        int, (Display *, Window, long)) \
	X(XCreateGC,           GC, (Display *, Window, unsigned long, XGCValues *)) \
	X(XCreateImage,        XImage *, (Display *, Visual *, unsigned int, int, int, char *, unsigned int, unsigned int, int, int)) \
	X(XMapRaised,          int, (Display *, Window)) \
	X(XFlush,              int, (Display *)) \
	X(XFreeGC,             int, (Display *, GC)) \
	X(XDestroyWindow,      int, (Display *, Window)) \
	X(XCheckIfEvent,       Bool, (Display *, XEvent *, Bool (*)(Display *, XEvent *, XPointer), XPointer)) \
	X(XPutBackEvent,       void, (Display *, XEvent *)) \
	X(XPending,            int, (Display *)) \
	X(XLookupKeysym,       KeySym, (XKeyEvent *, int)) \
	X(XLookupString,       int, (XKeyEvent *, char *, int, KeySym *, XComposeStatus *)) \
	X(XPutImage,           int, (Display *, Drawable, GC, XImage *, int, int, int, int, unsigned int, unsigned int)) \
	X(XSetSelectionOwner,  int, (Display *, Atom, Window, Time)) \
	X(XConvertSelection,   int, (Display *, Atom, Atom, Atom, Window, Time)) \
	X(XChangeProperty,     int, (Display *, Window, Atom, Atom, int, int, const unsigned char *, int)) \
	X(XGetWindowProperty,  int, (Display *, Window, Atom, long, long, Bool, Atom, Atom *, int *, unsigned long *, unsigned long *, unsigned char **)) \
	X(XDeleteProperty,     int, (Display *, Window, Atom)) \
	X(XSendEvent,          Status, (Display *, Window, Bool, long, XEvent *)) \
	X(XResizeWindow,       int, (Display *, Window, unsigned int, unsigned int)) \
	X(XDefineCursor,       int, (Display *, Window, Cursor)) \
	X(XUndefineCursor,     int, (Display *, Window)) \
	X(XCreatePixmap,       Pixmap, (Display *, Drawable, unsigned int, unsigned int, unsigned int)) \
	X(XFreePixmap,         int, (Display *, Pixmap)) \
	X(XCreatePixmapCursor, Cursor, (Display *, Pixmap, Pixmap, XColor *, XColor *, unsigned int, unsigned int)) \
	X(XFreeCursor,         int, (Display *, Cursor)) \
	X(XWarpPointer,        int, (Display *, Window, Window, int, int, unsigned int, unsigned int, int, int)) \
	X(XGrabPointer,        int, (Display *, Window, Bool, unsigned int, int, int, Window, Cursor, Time)) \
	X(XUngrabPointer,      int, (Display *, Time)) \
	X(XGetWindowAttributes, Status, (Display *, Window, XWindowAttributes *)) \
	X(XQueryPointer,       Bool, (Display *, Window, Window *, Window *, int *, int *, int *, int *, unsigned int *)) \
	X(XQueryTree,          Status, (Display *, Window, Window *, Window *, Window **, unsigned int *))

typedef struct {
#define X(name, ret, args) ret (*name) args;
	PEAK_X11_API(X)
#undef X
} PeakX11Api;

#ifndef PEAK_NO_AUDIO
typedef struct {
#define X(name, ret, args) ret (*name) args;
	PEAK_PULSE_API(X)
#undef X
} PeakPulseApi;

typedef struct {
	int format;
	uint32_t rate;
	uint8_t channels;
} PeakPaSampleSpec;
#endif

typedef struct {
	Display *display;
	Atom wm_delete_window;
	Atom clip_clipboard;
	Atom clip_utf8;
	Atom clip_targets;
	Atom clip_incr;
	Atom clip_text;
	Atom clip_prop;
	Atom net_wm_state;
	Atom net_wm_state_fullscreen;
	Atom xdnd_aware;
	Atom xdnd_enter;
	Atom xdnd_position;
	Atom xdnd_drop;
	Atom xdnd_status;
	Atom xdnd_finished;
	Atom xdnd_selection;
	Atom xdnd_type_list;
	Atom xdnd_action_copy;
	Atom xdnd_prop;
	Atom uri_list;
	Atom net_wm_pid;
} PeakLinux;

#ifndef PEAK_NO_AUDIO
typedef struct {
	volatile int run;
	int thread_on;
	pthread_t thread;
	void *stream;
	int16_t *buf;
	uint32_t channels;
	void (*fill)(int16_t *out, size_t frames, void *userdata);
	void *userdata;
} PeakAudio;
#endif

struct peak_linux_win {
	Window window;
	GC gfx_ctx;
	XImage *ximage;
	Visual *visual;
	Colormap colormap;
	uint32_t *buffer;
	uint32_t width;
	uint32_t height;
	int depth;
	int colormap_owned;
	int flags;
	int cursor_on;
	int relative;
	int extra_on;
	int touch_n;
	float last_x, last_y;
	Cursor blank;
	Window xdnd_source;
	Time xdnd_time;
	PeakEvent extra;
};

static PeakLinux peak_linux;
static PeakX11Api peak_x11;
#ifndef PEAK_NO_AUDIO
static PeakPulseApi peak_pulse;
#endif
static char *peak_clip_incr;
static size_t peak_clip_incr_n;
static PeakClip peak_clip_incr_which;
static int peak_clip_incr_on;
static PeakClip peak_clip_req_which;
static int peak_clip_req_on;
static int peak_clip_req_xa;
#ifndef PEAK_NO_AUDIO
static PeakAudio peak_audio;
#endif
static int peak_linux_kind;
#define PEAK_LINUX_NONE    0
#define PEAK_LINUX_WAYLAND 1
#define PEAK_LINUX_X11     2

/* BEGIN p_wayland.c */
/*
 * Wayland window, input, shm present, and WSI.
 * libwayland-client and libxkbcommon are dlopened.
 */

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <poll.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/timerfd.h>
#include <unistd.h>
long syscall(long number, ...);
#include <wayland-client-core.h>
#ifdef PEAK_VULKAN
#ifndef VK_USE_PLATFORM_WAYLAND_KHR
#define VK_USE_PLATFORM_WAYLAND_KHR
#endif
#include <vulkan/vulkan.h>
#endif

#define PEAK_WL_CLIENT "libwayland-client.so.0"
#define PEAK_XKB_SO "libxkbcommon.so.0"

struct xkb_context;
struct xkb_keymap;
struct xkb_state;
struct xkb_compose_table;
struct xkb_compose_state;

#define PEAK_XKB_API(X) \
	X(xkb_context_new, struct xkb_context *, (int)) \
	X(xkb_context_unref, void, (struct xkb_context *)) \
	X(xkb_keymap_new_from_buffer, struct xkb_keymap *, (struct xkb_context *, const char *, size_t, int, int)) \
	X(xkb_keymap_unref, void, (struct xkb_keymap *)) \
	X(xkb_state_new, struct xkb_state *, (struct xkb_keymap *)) \
	X(xkb_state_unref, void, (struct xkb_state *)) \
	X(xkb_state_update_mask, int, (struct xkb_state *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)) \
	X(xkb_state_key_get_utf8, int, (struct xkb_state *, uint32_t, char *, size_t)) \
	X(xkb_state_key_get_one_sym, uint32_t, (struct xkb_state *, uint32_t)) \
	X(xkb_state_mod_name_is_active, int, (struct xkb_state *, const char *, int)) \
	X(xkb_compose_table_new_from_locale, struct xkb_compose_table *, (struct xkb_context *, const char *, int)) \
	X(xkb_compose_table_unref, void, (struct xkb_compose_table *)) \
	X(xkb_compose_state_new, struct xkb_compose_state *, (struct xkb_compose_table *, int)) \
	X(xkb_compose_state_unref, void, (struct xkb_compose_state *)) \
	X(xkb_compose_state_feed, int, (struct xkb_compose_state *, uint32_t)) \
	X(xkb_compose_state_get_status, int, (struct xkb_compose_state *)) \
	X(xkb_compose_state_get_utf8, int, (struct xkb_compose_state *, char *, size_t)) \
	X(xkb_compose_state_reset, void, (struct xkb_compose_state *))

typedef struct {
#define X(name, ret, args) ret (*name) args;
	PEAK_XKB_API(X)
#undef X
	void *handle;
} PeakXkbApi;

#define PEAK_WL_API(X) \
	X(wl_display_connect, struct wl_display *, (const char *)) \
	X(wl_display_disconnect, void, (struct wl_display *)) \
	X(wl_display_dispatch, int, (struct wl_display *)) \
	X(wl_display_dispatch_pending, int, (struct wl_display *)) \
	X(wl_display_flush, int, (struct wl_display *)) \
	X(wl_display_roundtrip, int, (struct wl_display *)) \
	X(wl_display_get_fd, int, (struct wl_display *)) \
	X(wl_display_prepare_read, int, (struct wl_display *)) \
	X(wl_display_cancel_read, void, (struct wl_display *)) \
	X(wl_display_read_events, int, (struct wl_display *)) \
	X(wl_proxy_marshal_array_flags, struct wl_proxy *, (struct wl_proxy *, uint32_t, const struct wl_interface *, uint32_t, uint32_t, union wl_argument *)) \
	X(wl_proxy_add_listener, int, (struct wl_proxy *, void (**)(void), void *)) \
	X(wl_proxy_get_version, uint32_t, (struct wl_proxy *)) \
	X(wl_proxy_destroy, void, (struct wl_proxy *))

typedef struct {
#define X(name, ret, args) ret (*name) args;
	PEAK_WL_API(X)
#undef X
	void *handle;
} PeakWlApi;

struct wl_interface wl_registry_interface;
struct wl_interface wl_compositor_interface;
struct wl_interface wl_shm_interface;
struct wl_interface wl_shm_pool_interface;
struct wl_interface wl_buffer_interface;
struct wl_interface wl_surface_interface;
struct wl_interface wl_seat_interface;
struct wl_interface wl_pointer_interface;
struct wl_interface wl_keyboard_interface;
struct wl_interface wl_touch_interface;
struct wl_interface wl_output_interface;
struct wl_interface wl_data_device_manager_interface;
struct wl_interface wl_data_device_interface;
struct wl_interface wl_data_source_interface;
struct wl_interface wl_data_offer_interface;
struct wl_interface wl_callback_interface;

/* BEGIN xdg-shell-protocol.c */
/* Generated by wayland-scanner 1.26.0 */

/*
 * Copyright (c) 2008-2013 Kristian Hogsberg
 * Copyright (c) 2013      Rafael Antognolli
 * Copyright (c) 2013      Jasper St. Pierre
 * Copyright (c) 2010-2013 Intel Corporation
 * Copyright (c) 2015-2017 Samsung Electronics Co., Ltd
 * Copyright (c) 2015-2017 Red Hat Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <wayland-util.h>

#ifndef __has_attribute
# define __has_attribute(x) 0  /* Compatibility with non-clang compilers. */
#endif

#if (__has_attribute(visibility) || defined(__GNUC__) && __GNUC__ >= 4)
#define WL_PRIVATE __attribute__ ((visibility("hidden")))
#else
#define WL_PRIVATE
#endif

extern struct wl_interface wl_output_interface;
extern struct wl_interface wl_seat_interface;
extern struct wl_interface wl_surface_interface;
extern const struct wl_interface xdg_popup_interface;
extern const struct wl_interface xdg_positioner_interface;
extern const struct wl_interface xdg_surface_interface;
extern const struct wl_interface xdg_toplevel_interface;

static const struct wl_interface *xdg_shell_types[] = {
	NULL,
	NULL,
	NULL,
	NULL,
	&xdg_positioner_interface,
	&xdg_surface_interface,
	&wl_surface_interface,
	&xdg_toplevel_interface,
	&xdg_popup_interface,
	&xdg_surface_interface,
	&xdg_positioner_interface,
	&xdg_toplevel_interface,
	&wl_seat_interface,
	NULL,
	NULL,
	NULL,
	&wl_seat_interface,
	NULL,
	&wl_seat_interface,
	NULL,
	NULL,
	&wl_output_interface,
	&wl_seat_interface,
	NULL,
	&xdg_positioner_interface,
	NULL,
};

static const struct wl_message xdg_wm_base_requests[] = {
	{ "destroy", "", xdg_shell_types + 0 },
	{ "create_positioner", "n", xdg_shell_types + 4 },
	{ "get_xdg_surface", "no", xdg_shell_types + 5 },
	{ "pong", "u", xdg_shell_types + 0 },
};

static const struct wl_message xdg_wm_base_events[] = {
	{ "ping", "u", xdg_shell_types + 0 },
};

WL_PRIVATE const struct wl_interface xdg_wm_base_interface = {
	"xdg_wm_base", 6,
	4, xdg_wm_base_requests,
	1, xdg_wm_base_events,
};

static const struct wl_message xdg_positioner_requests[] = {
	{ "destroy", "", xdg_shell_types + 0 },
	{ "set_size", "ii", xdg_shell_types + 0 },
	{ "set_anchor_rect", "iiii", xdg_shell_types + 0 },
	{ "set_anchor", "u", xdg_shell_types + 0 },
	{ "set_gravity", "u", xdg_shell_types + 0 },
	{ "set_constraint_adjustment", "u", xdg_shell_types + 0 },
	{ "set_offset", "ii", xdg_shell_types + 0 },
	{ "set_reactive", "3", xdg_shell_types + 0 },
	{ "set_parent_size", "3ii", xdg_shell_types + 0 },
	{ "set_parent_configure", "3u", xdg_shell_types + 0 },
};

WL_PRIVATE const struct wl_interface xdg_positioner_interface = {
	"xdg_positioner", 6,
	10, xdg_positioner_requests,
	0, NULL,
};

static const struct wl_message xdg_surface_requests[] = {
	{ "destroy", "", xdg_shell_types + 0 },
	{ "get_toplevel", "n", xdg_shell_types + 7 },
	{ "get_popup", "n?oo", xdg_shell_types + 8 },
	{ "set_window_geometry", "iiii", xdg_shell_types + 0 },
	{ "ack_configure", "u", xdg_shell_types + 0 },
};

static const struct wl_message xdg_surface_events[] = {
	{ "configure", "u", xdg_shell_types + 0 },
};

WL_PRIVATE const struct wl_interface xdg_surface_interface = {
	"xdg_surface", 6,
	5, xdg_surface_requests,
	1, xdg_surface_events,
};

static const struct wl_message xdg_toplevel_requests[] = {
	{ "destroy", "", xdg_shell_types + 0 },
	{ "set_parent", "?o", xdg_shell_types + 11 },
	{ "set_title", "s", xdg_shell_types + 0 },
	{ "set_app_id", "s", xdg_shell_types + 0 },
	{ "show_window_menu", "ouii", xdg_shell_types + 12 },
	{ "move", "ou", xdg_shell_types + 16 },
	{ "resize", "ouu", xdg_shell_types + 18 },
	{ "set_max_size", "ii", xdg_shell_types + 0 },
	{ "set_min_size", "ii", xdg_shell_types + 0 },
	{ "set_maximized", "", xdg_shell_types + 0 },
	{ "unset_maximized", "", xdg_shell_types + 0 },
	{ "set_fullscreen", "?o", xdg_shell_types + 21 },
	{ "unset_fullscreen", "", xdg_shell_types + 0 },
	{ "set_minimized", "", xdg_shell_types + 0 },
};

static const struct wl_message xdg_toplevel_events[] = {
	{ "configure", "iia", xdg_shell_types + 0 },
	{ "close", "", xdg_shell_types + 0 },
	{ "configure_bounds", "4ii", xdg_shell_types + 0 },
	{ "wm_capabilities", "5a", xdg_shell_types + 0 },
};

WL_PRIVATE const struct wl_interface xdg_toplevel_interface = {
	"xdg_toplevel", 6,
	14, xdg_toplevel_requests,
	4, xdg_toplevel_events,
};

static const struct wl_message xdg_popup_requests[] = {
	{ "destroy", "", xdg_shell_types + 0 },
	{ "grab", "ou", xdg_shell_types + 22 },
	{ "reposition", "3ou", xdg_shell_types + 24 },
};

static const struct wl_message xdg_popup_events[] = {
	{ "configure", "iiii", xdg_shell_types + 0 },
	{ "popup_done", "", xdg_shell_types + 0 },
	{ "repositioned", "3u", xdg_shell_types + 0 },
};

WL_PRIVATE const struct wl_interface xdg_popup_interface = {
	"xdg_popup", 6,
	3, xdg_popup_requests,
	3, xdg_popup_events,
};

/* END xdg-shell-protocol.c */

struct peak_wayland_win {
	struct wl_surface *surface;
	struct xdg_surface *xdg_surface;
	struct xdg_toplevel *xdg_toplevel;
	uint32_t *buffer;
	uint32_t width;
	uint32_t height;
	int shm_fd;
	void *shm;
	size_t shm_n;
	struct wl_buffer *wl_buf;
	int configured;
	int flags;
	int cursor_on;
	int relative;
	int touch_n;
	int pointer_in;
	float last_x, last_y;
	PeakQ q;
};

typedef struct {
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct wl_shm *shm;
	struct wl_seat *seat;
	struct wl_pointer *pointer;
	struct wl_keyboard *keyboard;
	struct wl_touch *touch;
	struct xdg_wm_base *wm;
	struct wl_data_device_manager *ddm;
	struct wl_data_device *dd;
	struct wl_data_source *ds;
	struct wl_data_source *drag_ds;
	struct wl_data_offer *offer;
	struct wl_data_offer *dnd;
	struct wl_data_offer *fresh;
	char *drag;
	size_t drag_n;
	int offer_utf8;
	int offer_uri;
	int fresh_utf8;
	int fresh_uri;
	int dnd_utf8;
	int dnd_uri;
	const char *fresh_mime;
	const char *offer_mime;
	const char *dnd_mime;
	int dnd_busy;
	int dnd_left;
	int scale;
	uint32_t serial;
	uint32_t btn_serial;
	uint32_t dnd_serial;
	uint32_t dnd_source_actions;
	uint32_t dnd_action;
	uint32_t buttons;
	PeakKeyMod mod;
	struct peak_wayland_win *focus;
	int32_t repeat_rate;
	int32_t repeat_delay;
	uint32_t repeat_key;
	int timer_fd;
	int epoll_fd;
	struct xkb_context *xkb_ctx;
	struct xkb_keymap *xkb_keymap;
	struct xkb_state *xkb_state;
	struct xkb_compose_table *xkb_compose_table;
	struct xkb_compose_state *xkb_compose;
} PeakWayland;

static PeakWlApi peak_wl;
static PeakXkbApi peak_xkb;
static PeakWayland peak_wayland;

static int peak_wayland_load(void);
static int peak_wayland_copy_iface(const char *name, struct wl_interface *dst);
static struct wl_proxy *peak_wayland_marshal(struct wl_proxy *p, uint32_t op, const struct wl_interface *iface, union wl_argument *args);
static void peak_wayland_registry_global(void *data, struct wl_registry *reg, uint32_t name, const char *iface, uint32_t ver);
static void peak_wayland_registry_remove(void *data, struct wl_registry *reg, uint32_t name);
static void peak_wayland_wm_ping(void *data, struct xdg_wm_base *wm, uint32_t serial);
static void peak_wayland_xdg_configure(void *data, struct xdg_surface *surf, uint32_t serial);
static void peak_wayland_toplevel_configure(void *data, struct xdg_toplevel *top, int32_t w, int32_t h, struct wl_array *states);
static void peak_wayland_toplevel_close(void *data, struct xdg_toplevel *top);
static void peak_wayland_toplevel_bounds(void *data, struct xdg_toplevel *top, int32_t w, int32_t h);
static void peak_wayland_toplevel_caps(void *data, struct xdg_toplevel *top, struct wl_array *caps);
static void peak_wayland_pointer_enter(void *data, struct wl_pointer *p, uint32_t serial, struct wl_surface *s, wl_fixed_t x, wl_fixed_t y);
static void peak_wayland_pointer_leave(void *data, struct wl_pointer *p, uint32_t serial, struct wl_surface *s);
static void peak_wayland_pointer_motion(void *data, struct wl_pointer *p, uint32_t time, wl_fixed_t x, wl_fixed_t y);
static void peak_wayland_pointer_button(void *data, struct wl_pointer *p, uint32_t serial, uint32_t time, uint32_t button, uint32_t state);
static void peak_wayland_pointer_axis(void *data, struct wl_pointer *p, uint32_t time, uint32_t axis, wl_fixed_t value);
static void peak_wayland_keyboard_keymap(void *data, struct wl_keyboard *k, uint32_t fmt, int fd, uint32_t size);
static void peak_wayland_keyboard_enter(void *data, struct wl_keyboard *k, uint32_t serial, struct wl_surface *s, struct wl_array *keys);
static void peak_wayland_keyboard_leave(void *data, struct wl_keyboard *k, uint32_t serial, struct wl_surface *s);
static void peak_wayland_keyboard_key(void *data, struct wl_keyboard *k, uint32_t serial, uint32_t time, uint32_t key, uint32_t state);
static void peak_wayland_keyboard_mod(void *data, struct wl_keyboard *k, uint32_t serial, uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group);
static void peak_wayland_keyboard_repeat(void *data, struct wl_keyboard *k, int32_t rate, int32_t delay);
static int peak_wayland_key_mod(uint32_t key);
static void peak_wayland_repeat_stop(void);
static void peak_wayland_repeat_start(uint32_t key);
static void peak_wayland_repeat_tick(void);
static void peak_wayland_touch_down(void *data, struct wl_touch *t, uint32_t serial, uint32_t time, struct wl_surface *s, int32_t id, wl_fixed_t x, wl_fixed_t y);
static void peak_wayland_touch_up(void *data, struct wl_touch *t, uint32_t serial, uint32_t time, int32_t id);
static void peak_wayland_touch_motion(void *data, struct wl_touch *t, uint32_t time, int32_t id, wl_fixed_t x, wl_fixed_t y);
static void peak_wayland_touch_frame(void *data, struct wl_touch *t);
static void peak_wayland_touch_cancel(void *data, struct wl_touch *t);
static void peak_wayland_seat_caps(void *data, struct wl_seat *seat, uint32_t caps);
static void peak_wayland_seat_name(void *data, struct wl_seat *seat, const char *name);
static PeakKeyCode peak_wayland_key_map(uint32_t key);
static uint32_t peak_wayland_key_ascii(uint32_t key, PeakKeyMod mod);
static int peak_wayland_xkb_load(void);
static void peak_wayland_xkb_drop_state(void);
static void peak_wayland_xkb_quit(void);
static PeakKeyMod peak_wayland_xkb_mod(void);
static uint32_t peak_wayland_key_utf8(uint32_t key, int compose, char *buf, size_t cap, size_t *n_out);
static void peak_wayland_key_emit(struct peak_wayland_win *w, uint32_t key, int down, int compose);
static PeakPointerType peak_wayland_ptr_type(void);
static void peak_wayland_pump(void);
static void peak_wayland_offer_kill(struct wl_data_offer **slot);
static void peak_wayland_dd_offer(void *data, struct wl_data_device *dd, struct wl_data_offer *id);
static void peak_wayland_dd_enter(void *data, struct wl_data_device *dd, uint32_t serial, struct wl_surface *s, wl_fixed_t x, wl_fixed_t y, struct wl_data_offer *id);
static void peak_wayland_dd_leave(void *data, struct wl_data_device *dd);
static void peak_wayland_dd_motion(void *data, struct wl_data_device *dd, uint32_t time, wl_fixed_t x, wl_fixed_t y);
static void peak_wayland_dd_drop(void *data, struct wl_data_device *dd);
static void peak_wayland_dd_selection(void *data, struct wl_data_device *dd, struct wl_data_offer *id);
static void peak_wayland_offer_mime(void *data, struct wl_data_offer *o, const char *mime);
static void peak_wayland_ds_target(void *data, struct wl_data_source *ds, const char *mime);
static void peak_wayland_ds_send(void *data, struct wl_data_source *ds, const char *mime, int32_t fd);
static void peak_wayland_ds_cancelled(void *data, struct wl_data_source *ds);
static void peak_wayland_ds_dnd_drop(void *data, struct wl_data_source *ds);
static void peak_wayland_ds_dnd_finished(void *data, struct wl_data_source *ds);
static void peak_wayland_ds_action(void *data, struct wl_data_source *ds, uint32_t action);
static void peak_wayland_offer_source_actions(void *data, struct wl_data_offer *o, uint32_t actions);
static void peak_wayland_offer_action(void *data, struct wl_data_offer *o, uint32_t action);
static int peak_wayland_mime_utf8(const char *mime);
static int peak_wayland_mime_uri(const char *mime);
static const char *peak_wayland_mime_lit(const char *mime);
static int peak_wayland_mime_rank(const char *m);
static void peak_wayland_ds_kill(struct wl_data_source **slot);
static void peak_wayland_dnd_accept(void);
static int peak_wayland_offer_recv(struct wl_data_offer *o, const char *mime, char **out, size_t *out_n);
static int peak_wayland_drop_drag(PeakWindowInternal *intern, const char *utf8, size_t n);
static int peak_wayland_shm_resize(struct peak_wayland_win *w, uint32_t width, uint32_t height);
static int peak_wayland_init(void);
static void peak_wayland_quit(void);
static PeakWindowInternal peak_wayland_window_open(const char *name, uint32_t width, uint32_t height, uint32_t flags);
static void peak_wayland_window_close(PeakWindowInternal *intern);
static uint32_t *peak_wayland_window_buffer(PeakWindowInternal *intern, size_t *width, size_t *height);
static void peak_wayland_window_present(PeakWindowInternal *intern);
static void peak_wayland_window_set_title(PeakWindowInternal *intern, const char *name);
static void peak_wayland_window_set_size(PeakWindowInternal *intern, uint32_t width, uint32_t height);
static void peak_wayland_window_fullscreen(PeakWindowInternal *intern, int on);
static void peak_wayland_window_cursor(PeakWindowInternal *intern, int on);
static void peak_wayland_window_pointer_relative(PeakWindowInternal *intern, int on);
static float peak_wayland_window_scale(PeakWindowInternal *intern);
static int peak_wayland_clip_set(PeakWindowInternal *intern, PeakClip which, const char *utf8, size_t n);
static int peak_wayland_clip_request(PeakWindowInternal *intern, PeakClip which);
static int peak_wayland_epoll(PeakWindowInternal *intern, PeakEvent *ev);
static int peak_wayland_fd(PeakWindowInternal *intern);
static int peak_wayland_pending(PeakWindowInternal *intern);
static const char **peak_wayland_vulkan_get_extensions(uint32_t *count);
static int peak_wayland_vulkan_create_surface(PeakWindowInternal *intern, void *instance, const void *allocator, void *out_surface);

static int
peak_wayland_copy_iface(const char *name, struct wl_interface *dst)
{
	const struct wl_interface *src;

	src = dlsym(peak_wl.handle, name);
	if (!src)
		return 0;
	*dst = *src;
	return 1;
}

static int
peak_wayland_load(void)
{
	if (peak_wl.wl_display_connect)
		return 1;
	peak_wl.handle = dlopen(PEAK_WL_CLIENT, RTLD_LOCAL | RTLD_NOW);
	if (!peak_wl.handle)
		return 0;
#define X(name, ret, args) peak_wl.name = (ret (*) args)dlsym(peak_wl.handle, #name);
	PEAK_WL_API(X)
#undef X
#define X(name, ret, args) || !peak_wl.name
	if (0 PEAK_WL_API(X))
		return 0;
#undef X
	if (!peak_wayland_copy_iface("wl_registry_interface", &wl_registry_interface))
		return 0;
	if (!peak_wayland_copy_iface("wl_compositor_interface", &wl_compositor_interface))
		return 0;
	if (!peak_wayland_copy_iface("wl_shm_interface", &wl_shm_interface))
		return 0;
	if (!peak_wayland_copy_iface("wl_shm_pool_interface", &wl_shm_pool_interface))
		return 0;
	if (!peak_wayland_copy_iface("wl_buffer_interface", &wl_buffer_interface))
		return 0;
	if (!peak_wayland_copy_iface("wl_surface_interface", &wl_surface_interface))
		return 0;
	if (!peak_wayland_copy_iface("wl_seat_interface", &wl_seat_interface))
		return 0;
	if (!peak_wayland_copy_iface("wl_pointer_interface", &wl_pointer_interface))
		return 0;
	if (!peak_wayland_copy_iface("wl_keyboard_interface", &wl_keyboard_interface))
		return 0;
	if (!peak_wayland_copy_iface("wl_touch_interface", &wl_touch_interface))
		return 0;
	if (!peak_wayland_copy_iface("wl_output_interface", &wl_output_interface))
		return 0;
	peak_wayland_copy_iface("wl_data_device_manager_interface", &wl_data_device_manager_interface);
	peak_wayland_copy_iface("wl_data_device_interface", &wl_data_device_interface);
	peak_wayland_copy_iface("wl_data_source_interface", &wl_data_source_interface);
	peak_wayland_copy_iface("wl_data_offer_interface", &wl_data_offer_interface);
	peak_wayland_copy_iface("wl_callback_interface", &wl_callback_interface);
	return 1;
}

static struct wl_proxy *
peak_wayland_marshal(struct wl_proxy *p, uint32_t op, const struct wl_interface *iface, union wl_argument *args)
{
	uint32_t ver;
	union wl_argument empty[1];

	if (!p || !peak_wl.wl_proxy_marshal_array_flags)
		return NULL;
	ver = peak_wl.wl_proxy_get_version(p);
	if (!args) {
		memset(empty, 0, sizeof empty);
		args = empty;
	}
	return peak_wl.wl_proxy_marshal_array_flags(p, op, iface, ver, 0, args);
}

static void
peak_wayland_registry_global(void *data, struct wl_registry *reg, uint32_t name, const char *iface, uint32_t ver)
{
	union wl_argument args[4];

	(void)data;
	if (!iface)
		return;
	memset(args, 0, sizeof args);
	args[0].u = name;
	args[2].u = ver;
	if (!strcmp(iface, "wl_compositor") && !peak_wayland.compositor) {
		args[1].s = "wl_compositor";
		peak_wayland.compositor = (struct wl_compositor *)peak_wayland_marshal((struct wl_proxy *)reg, 0, &wl_compositor_interface, args);
	} else if (!strcmp(iface, "wl_shm") && !peak_wayland.shm) {
		args[1].s = "wl_shm";
		peak_wayland.shm = (struct wl_shm *)peak_wayland_marshal((struct wl_proxy *)reg, 0, &wl_shm_interface, args);
	} else if (!strcmp(iface, "wl_seat") && !peak_wayland.seat) {
		args[1].s = "wl_seat";
		if (ver > 4)
			args[2].u = 4;
		peak_wayland.seat = (struct wl_seat *)peak_wayland_marshal((struct wl_proxy *)reg, 0, &wl_seat_interface, args);
	} else if (!strcmp(iface, "xdg_wm_base") && !peak_wayland.wm) {
		args[1].s = "xdg_wm_base";
		if (ver > 6)
			args[2].u = 6;
		peak_wayland.wm = (struct xdg_wm_base *)peak_wayland_marshal((struct wl_proxy *)reg, 0, &xdg_wm_base_interface, args);
	} else if (!strcmp(iface, "wl_data_device_manager") && !peak_wayland.ddm && wl_data_device_manager_interface.name) {
		args[1].s = "wl_data_device_manager";
		if (ver > 3)
			args[2].u = 3;
		peak_wayland.ddm = (struct wl_data_device_manager *)peak_wayland_marshal((struct wl_proxy *)reg, 0, &wl_data_device_manager_interface, args);
	}
}

static void
peak_wayland_registry_remove(void *data, struct wl_registry *reg, uint32_t name)
{
	(void)data;
	(void)reg;
	(void)name;
}

static void
peak_wayland_wm_ping(void *data, struct xdg_wm_base *wm, uint32_t serial)
{
	union wl_argument args[1];

	(void)data;
	args[0].u = serial;
	peak_wayland_marshal((struct wl_proxy *)wm, 3, NULL, args);
}

static void
peak_wayland_xdg_configure(void *data, struct xdg_surface *surf, uint32_t serial)
{
	struct peak_wayland_win *w;
	union wl_argument args[1];

	w = data;
	args[0].u = serial;
	peak_wayland_marshal((struct wl_proxy *)surf, 4, NULL, args);
	w->configured = 1;
}

static void
peak_wayland_toplevel_configure(void *data, struct xdg_toplevel *top, int32_t w, int32_t h, struct wl_array *states)
{
	struct peak_wayland_win *win;
	PeakEvent ev;

	(void)top;
	(void)states;
	win = data;
	if (w <= 0 || h <= 0)
		return;
	if ((uint32_t)w == win->width && (uint32_t)h == win->height)
		return;
	if (!peak_wayland_shm_resize(win, (uint32_t)w, (uint32_t)h)) {
		win->width = (uint32_t)w;
		win->height = (uint32_t)h;
	}
	memset(&ev, 0, sizeof ev);
	ev.type = PEAK_EVENT_WINDOW_RESIZE;
	ev.resize.width = win->width;
	ev.resize.height = win->height;
	peak_q_push(&win->q, ev);
}

static void
peak_wayland_toplevel_close(void *data, struct xdg_toplevel *top)
{
	struct peak_wayland_win *w;
	PeakEvent ev;

	(void)top;
	w = data;
	memset(&ev, 0, sizeof ev);
	ev.type = PEAK_EVENT_WINDOW_CLOSE;
	peak_q_push(&w->q, ev);
}

static void
peak_wayland_toplevel_bounds(void *data, struct xdg_toplevel *top, int32_t w, int32_t h)
{
	(void)data;
	(void)top;
	(void)w;
	(void)h;
}

static void
peak_wayland_toplevel_caps(void *data, struct xdg_toplevel *top, struct wl_array *caps)
{
	(void)data;
	(void)top;
	(void)caps;
}

static void
peak_wayland_pointer_enter(void *data, struct wl_pointer *p, uint32_t serial, struct wl_surface *s, wl_fixed_t x, wl_fixed_t y)
{
	(void)data;
	(void)p;
	(void)s;
	peak_wayland.serial = serial;
	if (peak_wayland.focus) {
		peak_wayland.focus->pointer_in = 1;
		peak_wayland.focus->last_x = (float)wl_fixed_to_double(x);
		peak_wayland.focus->last_y = (float)wl_fixed_to_double(y);
	}
}

static void
peak_wayland_pointer_leave(void *data, struct wl_pointer *p, uint32_t serial, struct wl_surface *s)
{
	struct peak_wayland_win *w;
	PeakEvent ev;

	(void)data;
	(void)p;
	(void)s;
	peak_wayland.serial = serial;
	w = peak_wayland.focus;
	if (!w)
		return;
	w->pointer_in = 0;
	memset(&ev, 0, sizeof ev);
	ev.type = PEAK_EVENT_POINTER;
	ev.pointer.state = PEAK_POINTER_MOVED;
	ev.pointer.type = peak_wayland_ptr_type();
	ev.pointer.x = w->last_x;
	ev.pointer.y = w->last_y;
	ev.pointer.mod = peak_wayland.mod;
	peak_q_push(&w->q, ev);
}

static void
peak_wayland_pointer_motion(void *data, struct wl_pointer *p, uint32_t time, wl_fixed_t x, wl_fixed_t y)
{
	struct peak_wayland_win *w;
	PeakEvent ev;
	float px, py;

	(void)data;
	(void)p;
	(void)time;
	w = peak_wayland.focus;
	if (!w)
		return;
	px = (float)wl_fixed_to_double(x);
	py = (float)wl_fixed_to_double(y);
	memset(&ev, 0, sizeof ev);
	ev.type = PEAK_EVENT_POINTER;
	ev.pointer.state = PEAK_POINTER_MOVED;
	ev.pointer.type = peak_wayland_ptr_type();
	ev.pointer.mod = peak_wayland.mod;
	if (w->relative) {
		ev.pointer.x = px - w->last_x;
		ev.pointer.y = py - w->last_y;
	} else {
		ev.pointer.x = px;
		ev.pointer.y = py;
	}
	w->last_x = px;
	w->last_y = py;
	if (w->q.n) {
		PeakEvent *last;

		last = &w->q.e[(w->q.h + w->q.n - 1) % PEAK_Q];
		if (last->type == PEAK_EVENT_POINTER && last->pointer.state == PEAK_POINTER_MOVED) {
			*last = ev;
			return;
		}
	}
	peak_q_push(&w->q, ev);
}

static void
peak_wayland_pointer_button(void *data, struct wl_pointer *p, uint32_t serial, uint32_t time, uint32_t button, uint32_t state)
{
	struct peak_wayland_win *w;
	PeakEvent ev;

	(void)data;
	(void)p;
	(void)time;
	w = peak_wayland.focus;
	if (!w)
		return;
	peak_wayland.serial = serial;
	if (state)
		peak_wayland.btn_serial = serial;
	memset(&ev, 0, sizeof ev);
	ev.type = PEAK_EVENT_POINTER;
	ev.pointer.state = state ? PEAK_POINTER_PRESSED : PEAK_POINTER_RELEASED;
	ev.pointer.x = w->last_x;
	ev.pointer.y = w->last_y;
	ev.pointer.mod = peak_wayland.mod;
	if (button == BTN_RIGHT)
		ev.pointer.type = PEAK_POINTER_RIGHT;
	else if (button == BTN_MIDDLE)
		ev.pointer.type = PEAK_POINTER_MIDDLE;
	else
		ev.pointer.type = PEAK_POINTER_LEFT;
	if (state) {
		if (ev.pointer.type == PEAK_POINTER_RIGHT)
			peak_wayland.buttons |= 4;
		else if (ev.pointer.type == PEAK_POINTER_MIDDLE)
			peak_wayland.buttons |= 2;
		else
			peak_wayland.buttons |= 1;
	} else {
		if (ev.pointer.type == PEAK_POINTER_RIGHT)
			peak_wayland.buttons &= ~4u;
		else if (ev.pointer.type == PEAK_POINTER_MIDDLE)
			peak_wayland.buttons &= ~2u;
		else
			peak_wayland.buttons &= ~1u;
	}
	peak_q_push(&w->q, ev);
}

static void
peak_wayland_pointer_axis(void *data, struct wl_pointer *p, uint32_t time, uint32_t axis, wl_fixed_t value)
{
	struct peak_wayland_win *w;
	PeakEvent ev;

	(void)data;
	(void)p;
	(void)time;
	(void)axis;
	w = peak_wayland.focus;
	if (!w)
		return;
	memset(&ev, 0, sizeof ev);
	ev.type = PEAK_EVENT_POINTER;
	ev.pointer.state = PEAK_POINTER_PRESSED;
	ev.pointer.type = (wl_fixed_to_double(value) > 0) ? PEAK_POINTER_WHEEL_DOWN : PEAK_POINTER_WHEEL_UP;
	ev.pointer.x = w->last_x;
	ev.pointer.y = w->last_y;
	ev.pointer.mod = peak_wayland.mod;
	peak_q_push(&w->q, ev);
}

static int
peak_wayland_xkb_load(void)
{
	if (peak_xkb.handle)
		return peak_xkb.xkb_context_new != NULL;
	peak_xkb.handle = dlopen(PEAK_XKB_SO, RTLD_LOCAL | RTLD_NOW);
	if (!peak_xkb.handle)
		return 0;
#define X(name, ret, args) peak_xkb.name = (ret (*) args)dlsym(peak_xkb.handle, #name);
	PEAK_XKB_API(X)
#undef X
#define X(name, ret, args) || !peak_xkb.name
	if (0 PEAK_XKB_API(X)) {
		peak_xkb.xkb_context_new = NULL;
		return 0;
	}
#undef X
	return 1;
}

static void
peak_wayland_xkb_drop_state(void)
{
	if (peak_xkb.xkb_state_unref && peak_wayland.xkb_state)
		peak_xkb.xkb_state_unref(peak_wayland.xkb_state);
	if (peak_xkb.xkb_keymap_unref && peak_wayland.xkb_keymap)
		peak_xkb.xkb_keymap_unref(peak_wayland.xkb_keymap);
	peak_wayland.xkb_state = NULL;
	peak_wayland.xkb_keymap = NULL;
}

static void
peak_wayland_xkb_quit(void)
{
	peak_wayland_xkb_drop_state();
	if (peak_xkb.xkb_compose_state_unref && peak_wayland.xkb_compose)
		peak_xkb.xkb_compose_state_unref(peak_wayland.xkb_compose);
	if (peak_xkb.xkb_compose_table_unref && peak_wayland.xkb_compose_table)
		peak_xkb.xkb_compose_table_unref(peak_wayland.xkb_compose_table);
	if (peak_xkb.xkb_context_unref && peak_wayland.xkb_ctx)
		peak_xkb.xkb_context_unref(peak_wayland.xkb_ctx);
	peak_wayland.xkb_compose = NULL;
	peak_wayland.xkb_compose_table = NULL;
	peak_wayland.xkb_ctx = NULL;
}

static PeakKeyMod
peak_wayland_xkb_mod(void)
{
	PeakKeyMod m;

	m = 0;
	if (!peak_wayland.xkb_state || !peak_xkb.xkb_state_mod_name_is_active)
		return peak_wayland.mod;
	if (peak_xkb.xkb_state_mod_name_is_active(peak_wayland.xkb_state, "Shift", 8) > 0)
		m |= PEAK_KEYMOD_SHIFT;
	if (peak_xkb.xkb_state_mod_name_is_active(peak_wayland.xkb_state, "Lock", 8) > 0)
		m |= PEAK_KEYMOD_CAPS;
	if (peak_xkb.xkb_state_mod_name_is_active(peak_wayland.xkb_state, "Control", 8) > 0)
		m |= PEAK_KEYMOD_CTRL;
	if (peak_xkb.xkb_state_mod_name_is_active(peak_wayland.xkb_state, "Mod1", 8) > 0
		|| peak_xkb.xkb_state_mod_name_is_active(peak_wayland.xkb_state, "Alt", 8) > 0)
		m |= PEAK_KEYMOD_ALT;
	if (peak_xkb.xkb_state_mod_name_is_active(peak_wayland.xkb_state, "Mod4", 8) > 0
		|| peak_xkb.xkb_state_mod_name_is_active(peak_wayland.xkb_state, "Super", 8) > 0)
		m |= PEAK_KEYMOD_SUPER;
	return m;
}

static uint32_t
peak_wayland_key_utf8(uint32_t key, int compose, char *buf, size_t cap, size_t *n_out)
{
	uint32_t kc;
	uint32_t sym;
	uint32_t code;
	int n;
	int st;

	*n_out = 0;
	if (buf && cap)
		buf[0] = 0;
	if (!peak_wayland.xkb_state || !peak_xkb.xkb_state_key_get_utf8) {
		code = peak_wayland_key_ascii(key, peak_wayland.mod);
		if (code && buf && cap > 1) {
			buf[0] = (char)code;
			buf[1] = 0;
			*n_out = 1;
		}
		return code;
	}
	kc = key + 8;
	n = peak_xkb.xkb_state_key_get_utf8(peak_wayland.xkb_state, kc, buf, cap);
	if (compose && peak_wayland.xkb_compose && peak_xkb.xkb_compose_state_feed) {
		sym = peak_xkb.xkb_state_key_get_one_sym(peak_wayland.xkb_state, kc);
		peak_xkb.xkb_compose_state_feed(peak_wayland.xkb_compose, sym);
		st = peak_xkb.xkb_compose_state_get_status(peak_wayland.xkb_compose);
		if (st == 1 || st == 3)
			n = 0;
		else if (st == 2)
			n = peak_xkb.xkb_compose_state_get_utf8(peak_wayland.xkb_compose, buf, cap);
	}
	if (n < 0)
		n = 0;
	if (cap && (size_t)n >= cap)
		n = (int)cap - 1;
	*n_out = (size_t)n;
	if (n <= 0 || !buf)
		return 0;
	return (uint32_t)(unsigned char)buf[0];
}

static void
peak_wayland_key_emit(struct peak_wayland_win *w, uint32_t key, int down, int compose)
{
	PeakEvent ev;
	char buf[64];
	size_t n;

	memset(&ev, 0, sizeof ev);
	ev.type = down ? PEAK_EVENT_KEY_DOWN : PEAK_EVENT_KEY_UP;
	ev.key.key = peak_wayland_key_map(key);
	ev.key.mod = peak_wayland.mod;
	n = 0;
	ev.key.code = peak_wayland_key_utf8(key, compose && down, buf, sizeof buf, &n);
	peak_q_push(&w->q, ev);
	if (!down || !n || (unsigned char)buf[0] < 32)
		return;
	peak_text_store(buf, n);
	memset(&ev, 0, sizeof ev);
	ev.type = PEAK_EVENT_TEXT;
	ev.text.n = n;
	peak_q_push(&w->q, ev);
}

static void
peak_wayland_keyboard_keymap(void *data, struct wl_keyboard *k, uint32_t fmt, int fd, uint32_t size)
{
	const char *map;
	const char *locale;
	struct xkb_keymap *km;
	struct xkb_state *st;

	(void)data;
	(void)k;
	if (fd < 0)
		return;
	if (fmt != 1 || !size || !peak_wayland_xkb_load()) {
		close(fd);
		return;
	}
	if (!peak_wayland.xkb_ctx) {
		peak_wayland.xkb_ctx = peak_xkb.xkb_context_new(0);
		if (!peak_wayland.xkb_ctx) {
			close(fd);
			return;
		}
		locale = getenv("LC_ALL");
		if (!locale || !locale[0])
			locale = getenv("LC_CTYPE");
		if (!locale || !locale[0])
			locale = getenv("LANG");
		if (!locale || !locale[0])
			locale = "C";
		peak_wayland.xkb_compose_table = peak_xkb.xkb_compose_table_new_from_locale(peak_wayland.xkb_ctx, locale, 0);
		if (peak_wayland.xkb_compose_table)
			peak_wayland.xkb_compose = peak_xkb.xkb_compose_state_new(peak_wayland.xkb_compose_table, 0);
	}
	map = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	if (map == MAP_FAILED)
		return;
	km = peak_xkb.xkb_keymap_new_from_buffer(peak_wayland.xkb_ctx, map, size, 1, 0);
	munmap((void *)map, size);
	if (!km)
		return;
	st = peak_xkb.xkb_state_new(km);
	if (!st) {
		peak_xkb.xkb_keymap_unref(km);
		return;
	}
	peak_wayland_xkb_drop_state();
	peak_wayland.xkb_keymap = km;
	peak_wayland.xkb_state = st;
	if (peak_wayland.xkb_compose && peak_xkb.xkb_compose_state_reset)
		peak_xkb.xkb_compose_state_reset(peak_wayland.xkb_compose);
}

static void
peak_wayland_keyboard_enter(void *data, struct wl_keyboard *k, uint32_t serial, struct wl_surface *s, struct wl_array *keys)
{
	(void)data;
	(void)k;
	(void)s;
	(void)keys;
	peak_wayland.serial = serial;
}

static void
peak_wayland_keyboard_leave(void *data, struct wl_keyboard *k, uint32_t serial, struct wl_surface *s)
{
	(void)data;
	(void)k;
	(void)serial;
	(void)s;
	peak_wayland_repeat_stop();
	if (peak_wayland.xkb_compose && peak_xkb.xkb_compose_state_reset)
		peak_xkb.xkb_compose_state_reset(peak_wayland.xkb_compose);
}

static PeakPointerType
peak_wayland_ptr_type(void)
{
	if (peak_wayland.buttons & 4)
		return PEAK_POINTER_RIGHT;
	if (peak_wayland.buttons & 2)
		return PEAK_POINTER_MIDDLE;
	return PEAK_POINTER_LEFT;
}

static uint32_t
peak_wayland_key_ascii(uint32_t key, PeakKeyMod mod)
{
	int shift, caps;

	shift = (mod & PEAK_KEYMOD_SHIFT) ? 1 : 0;
	caps = (mod & PEAK_KEYMOD_CAPS) ? 1 : 0;
	if (key >= KEY_Q && key <= KEY_P) {
		static const char row[] = "qwertyuiop";

		return (uint32_t)((shift ^ caps) ? row[key - KEY_Q] - 32 : row[key - KEY_Q]);
	}
	if (key >= KEY_A && key <= KEY_L) {
		static const char row[] = "asdfghjkl";

		return (uint32_t)((shift ^ caps) ? row[key - KEY_A] - 32 : row[key - KEY_A]);
	}
	if (key >= KEY_Z && key <= KEY_M) {
		static const char row[] = "zxcvbnm";

		return (uint32_t)((shift ^ caps) ? row[key - KEY_Z] - 32 : row[key - KEY_Z]);
	}
	switch (key) {
	case KEY_1: return shift ? '!' : '1';
	case KEY_2: return shift ? '@' : '2';
	case KEY_3: return shift ? '#' : '3';
	case KEY_4: return shift ? '$' : '4';
	case KEY_5: return shift ? '%' : '5';
	case KEY_6: return shift ? '^' : '6';
	case KEY_7: return shift ? '&' : '7';
	case KEY_8: return shift ? '*' : '8';
	case KEY_9: return shift ? '(' : '9';
	case KEY_0: return shift ? ')' : '0';
	case KEY_MINUS: return shift ? '_' : '-';
	case KEY_EQUAL: return shift ? '+' : '=';
	case KEY_LEFTBRACE: return shift ? '{' : '[';
	case KEY_RIGHTBRACE: return shift ? '}' : ']';
	case KEY_BACKSLASH: return shift ? '|' : '\\';
	case KEY_SEMICOLON: return shift ? ':' : ';';
	case KEY_APOSTROPHE: return shift ? '"' : '\'';
	case KEY_GRAVE: return shift ? '~' : '`';
	case KEY_COMMA: return shift ? '<' : ',';
	case KEY_DOT: return shift ? '>' : '.';
	case KEY_SLASH: return shift ? '?' : '/';
	case KEY_SPACE: return ' ';
	default: return 0;
	}
}

static PeakKeyCode
peak_wayland_key_map(uint32_t key)
{
	if (key >= KEY_1 && key <= KEY_9)
		return (PeakKeyCode)(PEAK_KEY_1 + (int)(key - KEY_1));
	if (key == KEY_0)
		return PEAK_KEY_0;
	if (key >= KEY_F1 && key <= KEY_F12)
		return (PeakKeyCode)(PEAK_KEY_F1 + (int)(key - KEY_F1));
	switch (key) {
	case KEY_A: return PEAK_KEY_A;
	case KEY_B: return PEAK_KEY_B;
	case KEY_C: return PEAK_KEY_C;
	case KEY_D: return PEAK_KEY_D;
	case KEY_E: return PEAK_KEY_E;
	case KEY_F: return PEAK_KEY_F;
	case KEY_G: return PEAK_KEY_G;
	case KEY_H: return PEAK_KEY_H;
	case KEY_I: return PEAK_KEY_I;
	case KEY_J: return PEAK_KEY_J;
	case KEY_K: return PEAK_KEY_K;
	case KEY_L: return PEAK_KEY_L;
	case KEY_M: return PEAK_KEY_M;
	case KEY_N: return PEAK_KEY_N;
	case KEY_O: return PEAK_KEY_O;
	case KEY_P: return PEAK_KEY_P;
	case KEY_Q: return PEAK_KEY_Q;
	case KEY_R: return PEAK_KEY_R;
	case KEY_S: return PEAK_KEY_S;
	case KEY_T: return PEAK_KEY_T;
	case KEY_U: return PEAK_KEY_U;
	case KEY_V: return PEAK_KEY_V;
	case KEY_W: return PEAK_KEY_W;
	case KEY_X: return PEAK_KEY_X;
	case KEY_Y: return PEAK_KEY_Y;
	case KEY_Z: return PEAK_KEY_Z;
	case KEY_UP: return PEAK_KEY_UP;
	case KEY_DOWN: return PEAK_KEY_DOWN;
	case KEY_LEFT: return PEAK_KEY_LEFT;
	case KEY_RIGHT: return PEAK_KEY_RIGHT;
	case KEY_SPACE: return PEAK_KEY_SPACE;
	case KEY_ESC: return PEAK_KEY_ESCAPE;
	case KEY_ENTER: return PEAK_KEY_ENTER;
	case KEY_BACKSPACE: return PEAK_KEY_BACKSPACE;
	case KEY_TAB: return PEAK_KEY_TAB;
	case KEY_DELETE: return PEAK_KEY_DELETE;
	case KEY_INSERT: return PEAK_KEY_INSERT;
	case KEY_HOME: return PEAK_KEY_HOME;
	case KEY_END: return PEAK_KEY_END;
	case KEY_PAGEUP: return PEAK_KEY_PAGEUP;
	case KEY_PAGEDOWN: return PEAK_KEY_PAGEDOWN;
	case KEY_LEFTSHIFT:
	case KEY_RIGHTSHIFT:
	case KEY_LEFTCTRL:
	case KEY_RIGHTCTRL:
	case KEY_LEFTALT:
	case KEY_RIGHTALT:
	case KEY_LEFTMETA:
	case KEY_RIGHTMETA:
		return PEAK_KEY_UNKNOWN;
	default:
		return PEAK_KEY_UNKNOWN;
	}
}

static void
peak_wayland_keyboard_key(void *data, struct wl_keyboard *k, uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
	struct peak_wayland_win *w;

	(void)data;
	(void)k;
	(void)time;
	w = peak_wayland.focus;
	if (!w)
		return;
	peak_wayland.serial = serial;
	if (!peak_wayland.xkb_state) {
		if (key == KEY_LEFTSHIFT || key == KEY_RIGHTSHIFT) {
			if (state)
				peak_wayland.mod |= PEAK_KEYMOD_SHIFT;
			else
				peak_wayland.mod &= (PeakKeyMod)~PEAK_KEYMOD_SHIFT;
		} else if (key == KEY_LEFTCTRL || key == KEY_RIGHTCTRL) {
			if (state)
				peak_wayland.mod |= PEAK_KEYMOD_CTRL;
			else
				peak_wayland.mod &= (PeakKeyMod)~PEAK_KEYMOD_CTRL;
		} else if (key == KEY_LEFTALT || key == KEY_RIGHTALT) {
			if (state)
				peak_wayland.mod |= PEAK_KEYMOD_ALT;
			else
				peak_wayland.mod &= (PeakKeyMod)~PEAK_KEYMOD_ALT;
		} else if (key == KEY_LEFTMETA || key == KEY_RIGHTMETA) {
			if (state)
				peak_wayland.mod |= PEAK_KEYMOD_SUPER;
			else
				peak_wayland.mod &= (PeakKeyMod)~PEAK_KEYMOD_SUPER;
		} else if (key == KEY_CAPSLOCK && state)
			peak_wayland.mod ^= PEAK_KEYMOD_CAPS;
	} else
		peak_wayland.mod = peak_wayland_xkb_mod();
	peak_wayland_key_emit(w, key, state ? 1 : 0, 1);
	if (peak_wayland_key_mod(key))
		return;
	if (state)
		peak_wayland_repeat_start(key);
	else if (key == peak_wayland.repeat_key)
		peak_wayland_repeat_stop();
}

static void
peak_wayland_keyboard_mod(void *data, struct wl_keyboard *k, uint32_t serial, uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group)
{
	uint32_t bits;
	PeakKeyMod m;

	(void)data;
	(void)k;
	peak_wayland.serial = serial;
	if (peak_wayland.xkb_state && peak_xkb.xkb_state_update_mask) {
		peak_xkb.xkb_state_update_mask(peak_wayland.xkb_state, depressed, latched, locked, 0, 0, group);
		peak_wayland.mod = peak_wayland_xkb_mod();
		return;
	}
	bits = depressed | latched | locked;
	m = 0;
	if (bits & (1u << 0))
		m |= PEAK_KEYMOD_SHIFT;
	if (bits & (1u << 1))
		m |= PEAK_KEYMOD_CAPS;
	if (bits & (1u << 2))
		m |= PEAK_KEYMOD_CTRL;
	if (bits & (1u << 3))
		m |= PEAK_KEYMOD_ALT;
	if (bits & (1u << 6))
		m |= PEAK_KEYMOD_SUPER;
	peak_wayland.mod = m;
}

static int
peak_wayland_key_mod(uint32_t key)
{
	return key == KEY_LEFTSHIFT || key == KEY_RIGHTSHIFT
		|| key == KEY_LEFTCTRL || key == KEY_RIGHTCTRL
		|| key == KEY_LEFTALT || key == KEY_RIGHTALT
		|| key == KEY_LEFTMETA || key == KEY_RIGHTMETA
		|| key == KEY_CAPSLOCK;
}

static void
peak_wayland_repeat_stop(void)
{
	struct itimerspec ts;
	uint64_t n;

	peak_wayland.repeat_key = 0;
	if (peak_wayland.timer_fd < 0)
		return;
	memset(&ts, 0, sizeof ts);
	timerfd_settime(peak_wayland.timer_fd, 0, &ts, NULL);
	read(peak_wayland.timer_fd, &n, sizeof n);
}

static void
peak_wayland_repeat_start(uint32_t key)
{
	struct itimerspec ts;
	int32_t delay, rate;
	uint64_t ns;

	if (!key || peak_wayland.repeat_rate <= 0 || peak_wayland.timer_fd < 0) {
		peak_wayland_repeat_stop();
		return;
	}
	peak_wayland.repeat_key = key;
	delay = peak_wayland.repeat_delay;
	rate = peak_wayland.repeat_rate;
	memset(&ts, 0, sizeof ts);
	if (delay < 1)
		delay = 1;
	ts.it_value.tv_sec = delay / 1000;
	ts.it_value.tv_nsec = (long)(delay % 1000) * 1000000L;
	ns = 1000000000ull / (uint32_t)rate;
	ts.it_interval.tv_sec = (time_t)(ns / 1000000000ull);
	ts.it_interval.tv_nsec = (long)(ns % 1000000000ull);
	if (timerfd_settime(peak_wayland.timer_fd, 0, &ts, NULL) != 0)
		peak_wayland.repeat_key = 0;
}

static void
peak_wayland_repeat_tick(void)
{
	struct peak_wayland_win *w;
	uint64_t n;
	ssize_t r;

	if (peak_wayland.timer_fd < 0 || !peak_wayland.repeat_key)
		return;
	r = read(peak_wayland.timer_fd, &n, sizeof n);
	if (r != (ssize_t)sizeof n || n == 0)
		return;
	w = peak_wayland.focus;
	if (!w) {
		peak_wayland_repeat_stop();
		return;
	}
	peak_wayland_key_emit(w, peak_wayland.repeat_key, 1, 0);
}

static void
peak_wayland_keyboard_repeat(void *data, struct wl_keyboard *k, int32_t rate, int32_t delay)
{
	(void)data;
	(void)k;
	peak_wayland.repeat_rate = rate;
	peak_wayland.repeat_delay = delay;
	if (rate <= 0)
		peak_wayland_repeat_stop();
}

static void
peak_wayland_touch_down(void *data, struct wl_touch *t, uint32_t serial, uint32_t time, struct wl_surface *s, int32_t id, wl_fixed_t x, wl_fixed_t y)
{
	struct peak_wayland_win *w;
	PeakEvent ev;

	(void)data;
	(void)t;
	(void)serial;
	(void)time;
	(void)s;
	(void)id;
	w = peak_wayland.focus;
	if (!w)
		return;
	if (!w->touch_n) {
		memset(&ev, 0, sizeof ev);
		ev.type = PEAK_EVENT_POINTER_CONNECTED;
		peak_q_push(&w->q, ev);
	}
	w->touch_n++;
	memset(&ev, 0, sizeof ev);
	ev.type = PEAK_EVENT_POINTER;
	ev.pointer.state = PEAK_POINTER_PRESSED;
	ev.pointer.type = PEAK_POINTER_TOUCH;
	ev.pointer.x = (float)wl_fixed_to_double(x);
	ev.pointer.y = (float)wl_fixed_to_double(y);
	peak_q_push(&w->q, ev);
}

static void
peak_wayland_touch_up(void *data, struct wl_touch *t, uint32_t serial, uint32_t time, int32_t id)
{
	struct peak_wayland_win *w;
	PeakEvent ev;

	(void)data;
	(void)t;
	(void)serial;
	(void)time;
	(void)id;
	w = peak_wayland.focus;
	if (!w)
		return;
	memset(&ev, 0, sizeof ev);
	ev.type = PEAK_EVENT_POINTER;
	ev.pointer.state = PEAK_POINTER_RELEASED;
	ev.pointer.type = PEAK_POINTER_TOUCH;
	peak_q_push(&w->q, ev);
	if (w->touch_n > 0)
		w->touch_n--;
	if (!w->touch_n) {
		memset(&ev, 0, sizeof ev);
		ev.type = PEAK_EVENT_POINTER_DISCONNECTED;
		peak_q_push(&w->q, ev);
	}
}

static void
peak_wayland_touch_motion(void *data, struct wl_touch *t, uint32_t time, int32_t id, wl_fixed_t x, wl_fixed_t y)
{
	struct peak_wayland_win *w;
	PeakEvent ev;

	(void)data;
	(void)t;
	(void)time;
	(void)id;
	w = peak_wayland.focus;
	if (!w)
		return;
	memset(&ev, 0, sizeof ev);
	ev.type = PEAK_EVENT_POINTER;
	ev.pointer.state = PEAK_POINTER_MOVED;
	ev.pointer.type = PEAK_POINTER_TOUCH;
	ev.pointer.x = (float)wl_fixed_to_double(x);
	ev.pointer.y = (float)wl_fixed_to_double(y);
	peak_q_push(&w->q, ev);
}

static void
peak_wayland_touch_frame(void *data, struct wl_touch *t)
{
	(void)data;
	(void)t;
}

static void
peak_wayland_touch_cancel(void *data, struct wl_touch *t)
{
	(void)data;
	(void)t;
}

static void
peak_wayland_seat_caps(void *data, struct wl_seat *seat, uint32_t caps)
{
	static const struct {
		void (*enter)(void *, struct wl_pointer *, uint32_t, struct wl_surface *, wl_fixed_t, wl_fixed_t);
		void (*leave)(void *, struct wl_pointer *, uint32_t, struct wl_surface *);
		void (*motion)(void *, struct wl_pointer *, uint32_t, wl_fixed_t, wl_fixed_t);
		void (*button)(void *, struct wl_pointer *, uint32_t, uint32_t, uint32_t, uint32_t);
		void (*axis)(void *, struct wl_pointer *, uint32_t, uint32_t, wl_fixed_t);
	} pl = {
		peak_wayland_pointer_enter, peak_wayland_pointer_leave, peak_wayland_pointer_motion,
		peak_wayland_pointer_button, peak_wayland_pointer_axis
	};
	static const struct {
		void (*keymap)(void *, struct wl_keyboard *, uint32_t, int, uint32_t);
		void (*enter)(void *, struct wl_keyboard *, uint32_t, struct wl_surface *, struct wl_array *);
		void (*leave)(void *, struct wl_keyboard *, uint32_t, struct wl_surface *);
		void (*key)(void *, struct wl_keyboard *, uint32_t, uint32_t, uint32_t, uint32_t);
		void (*mod)(void *, struct wl_keyboard *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
		void (*repeat)(void *, struct wl_keyboard *, int32_t, int32_t);
	} kl = {
		peak_wayland_keyboard_keymap, peak_wayland_keyboard_enter, peak_wayland_keyboard_leave,
		peak_wayland_keyboard_key, peak_wayland_keyboard_mod, peak_wayland_keyboard_repeat
	};
	static const struct {
		void (*down)(void *, struct wl_touch *, uint32_t, uint32_t, struct wl_surface *, int32_t, wl_fixed_t, wl_fixed_t);
		void (*up)(void *, struct wl_touch *, uint32_t, uint32_t, int32_t);
		void (*motion)(void *, struct wl_touch *, uint32_t, int32_t, wl_fixed_t, wl_fixed_t);
		void (*frame)(void *, struct wl_touch *);
		void (*cancel)(void *, struct wl_touch *);
	} tl = {
		peak_wayland_touch_down, peak_wayland_touch_up, peak_wayland_touch_motion,
		peak_wayland_touch_frame, peak_wayland_touch_cancel
	};

	(void)data;
	if ((caps & 1) && !peak_wayland.pointer) {
		peak_wayland.pointer = (struct wl_pointer *)peak_wayland_marshal((struct wl_proxy *)seat, 0, &wl_pointer_interface, NULL);
		if (peak_wayland.pointer)
			peak_wl.wl_proxy_add_listener((struct wl_proxy *)peak_wayland.pointer, (void (**)(void))(void *)&pl, NULL);
	}
	if ((caps & 2) && !peak_wayland.keyboard) {
		peak_wayland.keyboard = (struct wl_keyboard *)peak_wayland_marshal((struct wl_proxy *)seat, 1, &wl_keyboard_interface, NULL);
		if (peak_wayland.keyboard)
			peak_wl.wl_proxy_add_listener((struct wl_proxy *)peak_wayland.keyboard, (void (**)(void))(void *)&kl, NULL);
	}
	if ((caps & 4) && !peak_wayland.touch) {
		peak_wayland.touch = (struct wl_touch *)peak_wayland_marshal((struct wl_proxy *)seat, 2, &wl_touch_interface, NULL);
		if (peak_wayland.touch)
			peak_wl.wl_proxy_add_listener((struct wl_proxy *)peak_wayland.touch, (void (**)(void))(void *)&tl, NULL);
	}
}

static void
peak_wayland_seat_name(void *data, struct wl_seat *seat, const char *name)
{
	(void)data;
	(void)seat;
	(void)name;
}

static int
peak_wayland_shm_resize(struct peak_wayland_win *w, uint32_t width, uint32_t height)
{
	size_t n;
	union wl_argument args[6];
	struct wl_shm_pool *pool;
	uint32_t *buffer;

	if (!width || !height)
		return 0;
	n = (size_t)width * height * 4;
	if (w->shm) {
		munmap(w->shm, w->shm_n);
		w->shm = NULL;
	}
	if (w->shm_fd >= 0) {
		close(w->shm_fd);
		w->shm_fd = -1;
	}
	if (w->wl_buf) {
		peak_wl.wl_proxy_destroy((struct wl_proxy *)w->wl_buf);
		w->wl_buf = NULL;
	}
	free(w->buffer);
	w->buffer = NULL;
	w->shm_fd = (int)syscall(SYS_memfd_create, "peak-wl", 0);
	if (w->shm_fd < 0 || ftruncate(w->shm_fd, (off_t)n) < 0)
		return 0;
	w->shm = mmap(NULL, n, PROT_READ | PROT_WRITE, MAP_SHARED, w->shm_fd, 0);
	if (w->shm == MAP_FAILED) {
		w->shm = NULL;
		return 0;
	}
	w->shm_n = n;
	memset(args, 0, sizeof args);
	args[1].h = w->shm_fd;
	args[2].i = (int32_t)n;
	pool = (struct wl_shm_pool *)peak_wayland_marshal((struct wl_proxy *)peak_wayland.shm, 0, &wl_shm_pool_interface, args);
	if (!pool)
		return 0;
	memset(args, 0, sizeof args);
	args[1].i = 0;
	args[2].i = (int32_t)width;
	args[3].i = (int32_t)height;
	args[4].i = (int32_t)(width * 4);
	args[5].u = 0; /* WL_SHM_FORMAT_ARGB8888 */
	w->wl_buf = (struct wl_buffer *)peak_wayland_marshal((struct wl_proxy *)pool, 0, &wl_buffer_interface, args);
	peak_wl.wl_proxy_destroy((struct wl_proxy *)pool);
	if (!w->wl_buf)
		return 0;
	buffer = calloc((size_t)width * height, sizeof *buffer);
	if (!buffer)
		return 0;
	w->buffer = buffer;
	w->width = width;
	w->height = height;
	return 1;
}

static int
peak_wayland_init(void)
{
	static const struct {
		void (*global)(void *, struct wl_registry *, uint32_t, const char *, uint32_t);
		void (*remove)(void *, struct wl_registry *, uint32_t);
	} rl = { peak_wayland_registry_global, peak_wayland_registry_remove };
	static const struct {
		void (*ping)(void *, struct xdg_wm_base *, uint32_t);
	} wml = { peak_wayland_wm_ping };
	static const struct {
		void (*caps)(void *, struct wl_seat *, uint32_t);
		void (*name)(void *, struct wl_seat *, const char *);
	} sl = { peak_wayland_seat_caps, peak_wayland_seat_name };
	static const struct {
		void (*offer)(void *, struct wl_data_device *, struct wl_data_offer *);
		void (*enter)(void *, struct wl_data_device *, uint32_t, struct wl_surface *, wl_fixed_t, wl_fixed_t, struct wl_data_offer *);
		void (*leave)(void *, struct wl_data_device *);
		void (*motion)(void *, struct wl_data_device *, uint32_t, wl_fixed_t, wl_fixed_t);
		void (*drop)(void *, struct wl_data_device *);
		void (*selection)(void *, struct wl_data_device *, struct wl_data_offer *);
	} ddl = {
		peak_wayland_dd_offer, peak_wayland_dd_enter, peak_wayland_dd_leave,
		peak_wayland_dd_motion, peak_wayland_dd_drop, peak_wayland_dd_selection
	};

	if (peak_wayland.display)
		return 1;
	if (!peak_wayland_load())
		return 0;
	peak_wayland.display = peak_wl.wl_display_connect(NULL);
	if (!peak_wayland.display)
		return 0;
	peak_wayland.scale = 1;
	peak_wayland.repeat_rate = 25;
	peak_wayland.repeat_delay = 600;
	peak_wayland.timer_fd = -1;
	peak_wayland.epoll_fd = -1;
	{
		struct epoll_event ee;
		int dfd;

		dfd = peak_wl.wl_display_get_fd(peak_wayland.display);
		peak_wayland.epoll_fd = epoll_create1(EPOLL_CLOEXEC);
		peak_wayland.timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
		if (peak_wayland.epoll_fd >= 0 && dfd >= 0) {
			memset(&ee, 0, sizeof ee);
			ee.events = EPOLLIN;
			ee.data.fd = dfd;
			if (epoll_ctl(peak_wayland.epoll_fd, EPOLL_CTL_ADD, dfd, &ee) != 0) {
				close(peak_wayland.epoll_fd);
				peak_wayland.epoll_fd = -1;
			}
		}
		if (peak_wayland.epoll_fd >= 0 && peak_wayland.timer_fd >= 0) {
			memset(&ee, 0, sizeof ee);
			ee.events = EPOLLIN;
			ee.data.fd = peak_wayland.timer_fd;
			epoll_ctl(peak_wayland.epoll_fd, EPOLL_CTL_ADD, peak_wayland.timer_fd, &ee);
		}
	}
	peak_wayland.registry = (struct wl_registry *)peak_wayland_marshal((struct wl_proxy *)peak_wayland.display, 1, &wl_registry_interface, NULL);
	if (!peak_wayland.registry)
		goto fail;
	peak_wl.wl_proxy_add_listener((struct wl_proxy *)peak_wayland.registry, (void (**)(void))(void *)&rl, NULL);
	peak_wl.wl_display_roundtrip(peak_wayland.display);
	if (!peak_wayland.compositor || !peak_wayland.shm || !peak_wayland.wm)
		goto fail;
	peak_wl.wl_proxy_add_listener((struct wl_proxy *)peak_wayland.wm, (void (**)(void))(void *)&wml, NULL);
	if (peak_wayland.seat)
		peak_wl.wl_proxy_add_listener((struct wl_proxy *)peak_wayland.seat, (void (**)(void))(void *)&sl, NULL);
	if (peak_wayland.ddm && peak_wayland.seat && wl_data_device_interface.name) {
		union wl_argument dargs[2];

		memset(dargs, 0, sizeof dargs);
		dargs[1].o = (struct wl_object *)peak_wayland.seat;
		peak_wayland.dd = (struct wl_data_device *)peak_wayland_marshal((struct wl_proxy *)peak_wayland.ddm, 1, &wl_data_device_interface, dargs);
		if (peak_wayland.dd)
			peak_wl.wl_proxy_add_listener((struct wl_proxy *)peak_wayland.dd, (void (**)(void))(void *)&ddl, NULL);
	}
	peak_wl.wl_display_roundtrip(peak_wayland.display);
	return 1;
fail:
	peak_wayland_quit();
	return 0;
}

static void
peak_wayland_quit(void)
{
	free(peak_wayland.drag);
	peak_wayland_xkb_quit();
	if (peak_wayland.timer_fd >= 0)
		close(peak_wayland.timer_fd);
	if (peak_wayland.epoll_fd >= 0)
		close(peak_wayland.epoll_fd);
	if (peak_wayland.display)
		peak_wl.wl_display_disconnect(peak_wayland.display);
	memset(&peak_wayland, 0, sizeof peak_wayland);
}

static PeakWindowInternal
peak_wayland_window_open(const char *name, uint32_t width, uint32_t height, uint32_t flags)
{
	PeakWindowInternal intern = {0};
	struct peak_wayland_win *w;
	union wl_argument args[2];
	static const struct {
		void (*configure)(void *, struct xdg_surface *, uint32_t);
	} xsl = { peak_wayland_xdg_configure };
	static const struct {
		void (*configure)(void *, struct xdg_toplevel *, int32_t, int32_t, struct wl_array *);
		void (*close)(void *, struct xdg_toplevel *);
		void (*bounds)(void *, struct xdg_toplevel *, int32_t, int32_t);
		void (*caps)(void *, struct xdg_toplevel *, struct wl_array *);
	} xtl = {
		peak_wayland_toplevel_configure, peak_wayland_toplevel_close,
		peak_wayland_toplevel_bounds, peak_wayland_toplevel_caps
	};

	if (!peak_wayland.display && !peak_wayland_init())
		return intern;
	w = calloc(1, sizeof *w);
	if (!w)
		return intern;
	w->shm_fd = -1;
	w->cursor_on = 1;
	w->flags = (int)flags;
	w->surface = (struct wl_surface *)peak_wayland_marshal((struct wl_proxy *)peak_wayland.compositor, 0, &wl_surface_interface, NULL);
	if (!w->surface)
		goto fail;
	memset(args, 0, sizeof args);
	args[1].o = (struct wl_object *)w->surface;
	w->xdg_surface = (struct xdg_surface *)peak_wayland_marshal((struct wl_proxy *)peak_wayland.wm, 2, &xdg_surface_interface, args);
	if (!w->xdg_surface)
		goto fail;
	peak_wl.wl_proxy_add_listener((struct wl_proxy *)w->xdg_surface, (void (**)(void))(void *)&xsl, w);
	w->xdg_toplevel = (struct xdg_toplevel *)peak_wayland_marshal((struct wl_proxy *)w->xdg_surface, 1, &xdg_toplevel_interface, NULL);
	if (!w->xdg_toplevel)
		goto fail;
	peak_wl.wl_proxy_add_listener((struct wl_proxy *)w->xdg_toplevel, (void (**)(void))(void *)&xtl, w);
	memset(args, 0, sizeof args);
	args[0].s = name;
	peak_wayland_marshal((struct wl_proxy *)w->xdg_toplevel, 2, NULL, args);
	if (flags & PEAK_WINDOW_FULLSCREEN) {
		memset(args, 0, sizeof args);
		peak_wayland_marshal((struct wl_proxy *)w->xdg_toplevel, 11, NULL, args);
	}
	if (!peak_wayland_shm_resize(w, width, height))
		goto fail;
	peak_wayland_marshal((struct wl_proxy *)w->surface, 6, NULL, NULL);
	if (peak_wl.wl_display_roundtrip(peak_wayland.display) < 0)
		goto fail;
	if (!w->configured) {
		peak_wayland_marshal((struct wl_proxy *)w->surface, 6, NULL, NULL);
		peak_wl.wl_display_roundtrip(peak_wayland.display);
	}
	peak_wayland.focus = w;
	intern.w = w;
	return intern;
fail:
	peak_wayland_window_close(&intern);
	free(w);
	return intern;
}

static void
peak_wayland_window_close(PeakWindowInternal *intern)
{
	struct peak_wayland_win *w;

	w = intern ? intern->w : NULL;
	if (!w)
		return;
	if (peak_wayland.focus == w) {
		peak_wayland.focus = NULL;
		peak_wayland_repeat_stop();
	}
	if (w->xdg_toplevel)
		peak_wl.wl_proxy_destroy((struct wl_proxy *)w->xdg_toplevel);
	if (w->xdg_surface)
		peak_wl.wl_proxy_destroy((struct wl_proxy *)w->xdg_surface);
	if (w->wl_buf)
		peak_wl.wl_proxy_destroy((struct wl_proxy *)w->wl_buf);
	if (w->surface)
		peak_wl.wl_proxy_destroy((struct wl_proxy *)w->surface);
	if (w->shm)
		munmap(w->shm, w->shm_n);
	if (w->shm_fd >= 0)
		close(w->shm_fd);
	free(w->buffer);
	free(w);
	if (intern)
		intern->w = NULL;
}

static uint32_t *
peak_wayland_window_buffer(PeakWindowInternal *intern, size_t *width, size_t *height)
{
	struct peak_wayland_win *w;

	w = intern ? intern->w : NULL;
	if (!w) {
		if (width) *width = 0;
		if (height) *height = 0;
		return NULL;
	}
	if (width) *width = w->width;
	if (height) *height = w->height;
	return w->buffer;
}

static void
peak_wayland_window_present(PeakWindowInternal *intern)
{
	struct peak_wayland_win *w;
	union wl_argument args[4];

	w = intern ? intern->w : NULL;
	if (!w || !w->surface || !w->wl_buf || !w->buffer || !w->shm)
		return;
	memcpy(w->shm, w->buffer, w->shm_n);
	args[0].o = (struct wl_object *)w->wl_buf;
	args[1].i = 0;
	args[2].i = 0;
	peak_wayland_marshal((struct wl_proxy *)w->surface, 1, NULL, args);
	args[0].i = 0;
	args[1].i = 0;
	args[2].i = (int32_t)w->width;
	args[3].i = (int32_t)w->height;
	peak_wayland_marshal((struct wl_proxy *)w->surface, 2, NULL, args);
	peak_wayland_marshal((struct wl_proxy *)w->surface, 6, NULL, NULL);
	peak_wl.wl_display_flush(peak_wayland.display);
}

static void
peak_wayland_window_set_title(PeakWindowInternal *intern, const char *name)
{
	struct peak_wayland_win *w;
	union wl_argument args[1];

	w = intern ? intern->w : NULL;
	if (!w || !w->xdg_toplevel || !name)
		return;
	args[0].s = name;
	peak_wayland_marshal((struct wl_proxy *)w->xdg_toplevel, 2, NULL, args);
	peak_wl.wl_display_flush(peak_wayland.display);
}

static void
peak_wayland_window_set_size(PeakWindowInternal *intern, uint32_t width, uint32_t height)
{
	struct peak_wayland_win *w;

	w = intern ? intern->w : NULL;
	if (!w)
		return;
	peak_wayland_shm_resize(w, width, height);
}

static void
peak_wayland_window_fullscreen(PeakWindowInternal *intern, int on)
{
	struct peak_wayland_win *w;
	union wl_argument args[1];

	w = intern ? intern->w : NULL;
	if (!w || !w->xdg_toplevel)
		return;
	args[0].o = NULL;
	peak_wayland_marshal((struct wl_proxy *)w->xdg_toplevel, on ? 11 : 12, NULL, args);
	peak_wl.wl_display_flush(peak_wayland.display);
}

static void
peak_wayland_window_cursor(PeakWindowInternal *intern, int on)
{
	struct peak_wayland_win *w;

	w = intern ? intern->w : NULL;
	if (!w)
		return;
	w->cursor_on = on;
}

static void
peak_wayland_window_pointer_relative(PeakWindowInternal *intern, int on)
{
	struct peak_wayland_win *w;

	w = intern ? intern->w : NULL;
	if (!w)
		return;
	w->relative = on;
}

static float
peak_wayland_window_scale(PeakWindowInternal *intern)
{
	(void)intern;
	return peak_wayland.scale > 0 ? (float)peak_wayland.scale : 1.f;
}

static void
peak_wayland_pump(void)
{
	struct pollfd pfd;

	if (!peak_wayland.display)
		return;
	pfd.fd = peak_wl.wl_display_get_fd(peak_wayland.display);
	for (;;) {
		while (peak_wl.wl_display_prepare_read(peak_wayland.display) != 0) {
			if (peak_wl.wl_display_dispatch_pending(peak_wayland.display) < 0)
				return;
		}
		if (peak_wl.wl_display_flush(peak_wayland.display) < 0) {
			peak_wl.wl_display_cancel_read(peak_wayland.display);
			return;
		}
		pfd.events = POLLIN;
		pfd.revents = 0;
		if (poll(&pfd, 1, 0) <= 0) {
			peak_wl.wl_display_cancel_read(peak_wayland.display);
			peak_wl.wl_display_dispatch_pending(peak_wayland.display);
			return;
		}
		if (peak_wl.wl_display_read_events(peak_wayland.display) < 0)
			return;
		if (peak_wl.wl_display_dispatch_pending(peak_wayland.display) < 0)
			return;
	}
}

static void
peak_wayland_offer_kill(struct wl_data_offer **slot)
{
	struct wl_data_offer *o;

	o = slot ? *slot : NULL;
	if (!o)
		return;
	peak_wayland_marshal((struct wl_proxy *)o, 2, NULL, NULL);
	peak_wl.wl_proxy_destroy((struct wl_proxy *)o);
	if (peak_wayland.fresh == o) {
		peak_wayland.fresh = NULL;
		peak_wayland.fresh_mime = NULL;
		peak_wayland.fresh_utf8 = 0;
		peak_wayland.fresh_uri = 0;
	}
	if (peak_wayland.dnd == o) {
		peak_wayland.dnd = NULL;
		peak_wayland.dnd_mime = NULL;
		peak_wayland.dnd_utf8 = 0;
		peak_wayland.dnd_uri = 0;
		peak_wayland.dnd_action = 0;
		peak_wayland.dnd_source_actions = 0;
		peak_wayland.dnd_serial = 0;
	}
	if (peak_wayland.offer == o) {
		peak_wayland.offer = NULL;
		peak_wayland.offer_mime = NULL;
		peak_wayland.offer_utf8 = 0;
		peak_wayland.offer_uri = 0;
	}
	if (slot)
		*slot = NULL;
}

static int
peak_wayland_mime_utf8(const char *mime)
{
	return mime && (!strcmp(mime, "text/plain;charset=utf-8") || !strcmp(mime, "text/plain") || !strcmp(mime, "UTF8_STRING"));
}

static int
peak_wayland_mime_uri(const char *mime)
{
	return mime && !strcmp(mime, "text/uri-list");
}

static const char *
peak_wayland_mime_lit(const char *mime)
{
	if (!mime)
		return NULL;
	if (!strcmp(mime, "text/uri-list"))
		return "text/uri-list";
	if (!strcmp(mime, "text/plain;charset=utf-8"))
		return "text/plain;charset=utf-8";
	if (!strcmp(mime, "text/plain"))
		return "text/plain";
	if (!strcmp(mime, "UTF8_STRING"))
		return "UTF8_STRING";
	return NULL;
}

static int
peak_wayland_mime_rank(const char *m)
{
	if (!m)
		return 0;
	if (!strcmp(m, "text/uri-list"))
		return 4;
	if (!strcmp(m, "text/plain;charset=utf-8"))
		return 3;
	if (!strcmp(m, "text/plain"))
		return 2;
	if (!strcmp(m, "UTF8_STRING"))
		return 1;
	return 0;
}

static void
peak_wayland_ds_kill(struct wl_data_source **slot)
{
	struct wl_data_source *ds;

	ds = slot ? *slot : NULL;
	if (!ds)
		return;
	peak_wayland_marshal((struct wl_proxy *)ds, 1, NULL, NULL);
	peak_wl.wl_proxy_destroy((struct wl_proxy *)ds);
	if (peak_wayland.ds == ds)
		peak_wayland.ds = NULL;
	if (peak_wayland.drag_ds == ds) {
		peak_wayland.drag_ds = NULL;
		free(peak_wayland.drag);
		peak_wayland.drag = NULL;
		peak_wayland.drag_n = 0;
	}
	if (slot)
		*slot = NULL;
}

static void
peak_wayland_dnd_accept(void)
{
	union wl_argument args[2];
	struct wl_data_offer *o;
	const char *mime;

	o = peak_wayland.dnd;
	if (!o || peak_wayland.dnd_busy)
		return;
	mime = peak_wayland.dnd_mime;
	if (!mime)
		return;
	memset(args, 0, sizeof args);
	args[0].u = peak_wayland.dnd_serial ? peak_wayland.dnd_serial : peak_wayland.serial;
	args[1].s = mime;
	peak_wayland_marshal((struct wl_proxy *)o, 0, NULL, args);
	if (peak_wl.wl_proxy_get_version((struct wl_proxy *)o) >= 3) {
		memset(args, 0, sizeof args);
		args[0].u = 1;
		args[1].u = 1;
		peak_wayland_marshal((struct wl_proxy *)o, 4, NULL, args);
	}
	peak_wl.wl_display_flush(peak_wayland.display);
}

static void
peak_wayland_dd_offer(void *data, struct wl_data_device *dd, struct wl_data_offer *id)
{
	static const struct {
		void (*mime)(void *, struct wl_data_offer *, const char *);
		void (*source_actions)(void *, struct wl_data_offer *, uint32_t);
		void (*action)(void *, struct wl_data_offer *, uint32_t);
	} ol = { peak_wayland_offer_mime, peak_wayland_offer_source_actions, peak_wayland_offer_action };

	(void)data;
	(void)dd;
	if (!id)
		return;
	if (peak_wayland.fresh && peak_wayland.fresh != peak_wayland.offer && peak_wayland.fresh != peak_wayland.dnd)
		peak_wayland_offer_kill(&peak_wayland.fresh);
	peak_wayland.fresh = id;
	peak_wayland.fresh_utf8 = 0;
	peak_wayland.fresh_uri = 0;
	peak_wayland.fresh_mime = NULL;
	peak_wl.wl_proxy_add_listener((struct wl_proxy *)id, (void (**)(void))(void *)&ol, NULL);
}

static void
peak_wayland_dd_enter(void *data, struct wl_data_device *dd, uint32_t serial, struct wl_surface *s, wl_fixed_t x, wl_fixed_t y, struct wl_data_offer *id)
{
	struct peak_wayland_win *w;

	(void)data;
	(void)dd;
	(void)s;
	peak_wayland.serial = serial;
	peak_wayland.dnd_serial = serial;
	w = peak_wayland.focus;
	if (w) {
		w->pointer_in = 1;
		w->last_x = (float)wl_fixed_to_double(x);
		w->last_y = (float)wl_fixed_to_double(y);
	}
	if (peak_wayland.dnd && peak_wayland.dnd != id) {
		if (peak_wayland.dnd_busy)
			return;
		peak_wayland_offer_kill(&peak_wayland.dnd);
	}
	peak_wayland.dnd = id;
	peak_wayland.dnd_utf8 = 0;
	peak_wayland.dnd_uri = 0;
	peak_wayland.dnd_mime = NULL;
	if (peak_wayland.fresh == id) {
		peak_wayland.dnd_utf8 = peak_wayland.fresh_utf8;
		peak_wayland.dnd_uri = peak_wayland.fresh_uri;
		peak_wayland.dnd_mime = peak_wayland.fresh_mime;
		peak_wayland.fresh = NULL;
		peak_wayland.fresh_mime = NULL;
	}
	peak_wayland_dnd_accept();
}

static void
peak_wayland_dd_leave(void *data, struct wl_data_device *dd)
{
	(void)data;
	(void)dd;
	if (peak_wayland.dnd_busy) {
		peak_wayland.dnd_left = 1;
		return;
	}
	peak_wayland_offer_kill(&peak_wayland.dnd);
}

static void
peak_wayland_dd_motion(void *data, struct wl_data_device *dd, uint32_t time, wl_fixed_t x, wl_fixed_t y)
{
	struct peak_wayland_win *w;

	(void)data;
	(void)dd;
	(void)time;
	w = peak_wayland.focus;
	if (!w)
		return;
	w->pointer_in = 1;
	w->last_x = (float)wl_fixed_to_double(x);
	w->last_y = (float)wl_fixed_to_double(y);
	if (!peak_wayland.dnd_busy)
		peak_wayland_dnd_accept();
}

static void
peak_wayland_dd_drop(void *data, struct wl_data_device *dd)
{
	struct peak_wayland_win *w;
	struct wl_data_offer *o;
	char *acc;
	size_t n;
	const char *mime;
	uint32_t ver;
	uint32_t action;
	PeakEvent ev;

	(void)data;
	(void)dd;
	o = peak_wayland.dnd;
	if (!o)
		return;
	mime = peak_wayland.dnd_mime;
	ver = peak_wl.wl_proxy_get_version((struct wl_proxy *)o);
	action = peak_wayland.dnd_action;
	acc = NULL;
	n = 0;
	peak_wayland.dnd_busy = 1;
	peak_wayland.dnd_left = 0;
	if (!mime)
		mime = peak_wayland.dnd_uri ? "text/uri-list" :
			peak_wayland.dnd_utf8 ? "text/plain;charset=utf-8" : NULL;
	if (mime)
		peak_wayland_offer_recv(o, mime, &acc, &n);
	if (peak_wayland.dnd == o && !peak_wayland.dnd_left && ver >= 3 && (action == 1 || action == 2) && n)
		peak_wayland_marshal((struct wl_proxy *)o, 3, NULL, NULL);
	if (peak_wayland.dnd == o)
		peak_wayland_offer_kill(&peak_wayland.dnd);
	peak_wayland.dnd_busy = 0;
	peak_wayland.dnd_left = 0;
	if (!acc || !n) {
		free(acc);
		return;
	}
	peak_drop_store(acc, n);
	free(acc);
	w = peak_wayland.focus;
	if (!w)
		return;
	memset(&ev, 0, sizeof ev);
	ev.type = PEAK_EVENT_DROP;
	ev.drop.n = n;
	peak_q_push(&w->q, ev);
}

static void
peak_wayland_dd_selection(void *data, struct wl_data_device *dd, struct wl_data_offer *id)
{
	(void)data;
	(void)dd;
	if (peak_wayland.offer && peak_wayland.offer != id)
		peak_wayland_offer_kill(&peak_wayland.offer);
	peak_wayland.offer = id;
	if (id && peak_wayland.fresh == id) {
		peak_wayland.offer_utf8 = peak_wayland.fresh_utf8;
		peak_wayland.offer_uri = peak_wayland.fresh_uri;
		peak_wayland.offer_mime = peak_wayland.fresh_mime;
		peak_wayland.fresh = NULL;
		peak_wayland.fresh_mime = NULL;
	} else if (!id) {
		peak_wayland.offer_utf8 = 0;
		peak_wayland.offer_uri = 0;
		peak_wayland.offer_mime = NULL;
	}
}

static void
peak_wayland_offer_mime(void *data, struct wl_data_offer *o, const char *mime)
{
	const char *lit;
	int utf8, uri;

	(void)data;
	lit = peak_wayland_mime_lit(mime);
	if (!lit)
		return;
	utf8 = peak_wayland_mime_utf8(lit);
	uri = peak_wayland_mime_uri(lit);
	if (o == peak_wayland.fresh) {
		if (peak_wayland_mime_rank(lit) > peak_wayland_mime_rank(peak_wayland.fresh_mime))
			peak_wayland.fresh_mime = lit;
		if (utf8)
			peak_wayland.fresh_utf8 = 1;
		if (uri)
			peak_wayland.fresh_uri = 1;
	}
	if (o == peak_wayland.offer) {
		if (peak_wayland_mime_rank(lit) > peak_wayland_mime_rank(peak_wayland.offer_mime))
			peak_wayland.offer_mime = lit;
		if (utf8)
			peak_wayland.offer_utf8 = 1;
		if (uri)
			peak_wayland.offer_uri = 1;
	}
	if (o == peak_wayland.dnd) {
		if (peak_wayland_mime_rank(lit) > peak_wayland_mime_rank(peak_wayland.dnd_mime))
			peak_wayland.dnd_mime = lit;
		if (utf8)
			peak_wayland.dnd_utf8 = 1;
		if (uri)
			peak_wayland.dnd_uri = 1;
		peak_wayland_dnd_accept();
	}
}

static void
peak_wayland_offer_source_actions(void *data, struct wl_data_offer *o, uint32_t actions)
{
	(void)data;
	if (o != peak_wayland.dnd && o != peak_wayland.fresh)
		return;
	peak_wayland.dnd_source_actions = actions;
	if (o == peak_wayland.dnd)
		peak_wayland_dnd_accept();
}

static void
peak_wayland_offer_action(void *data, struct wl_data_offer *o, uint32_t action)
{
	(void)data;
	if (o != peak_wayland.dnd && o != peak_wayland.fresh)
		return;
	peak_wayland.dnd_action = action;
}

static void
peak_wayland_ds_target(void *data, struct wl_data_source *ds, const char *mime)
{
	(void)data;
	(void)ds;
	(void)mime;
}

static void
peak_wayland_ds_write(int fd, const char *p, size_t n)
{
	size_t off;

	off = 0;
	while (off < n) {
		ssize_t w;

		w = write(fd, p + off, n - off);
		if (w < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		off += (size_t)w;
	}
}

static void
peak_wayland_ds_send(void *data, struct wl_data_source *ds, const char *mime, int32_t fd)
{
	const char *p;
	size_t n;

	(void)data;
	if (fd < 0)
		return;
	if (ds == peak_wayland.drag_ds && peak_wayland.drag) {
		p = peak_wayland.drag;
		n = peak_wayland.drag_n;
		if (peak_wayland_mime_uri(mime) && (n < 7 || memcmp(p, "file://", 7))) {
			peak_wayland_ds_write(fd, "file://", 7);
			peak_wayland_ds_write(fd, p, n);
			peak_wayland_ds_write(fd, "\n", 1);
		} else
			peak_wayland_ds_write(fd, p, n);
		close(fd);
		return;
	}
	if (!peak_clip_own_get(PEAK_CLIP_CLIPBOARD, &p, &n))
		n = 0;
	peak_wayland_ds_write(fd, p, n);
	close(fd);
}

static void
peak_wayland_ds_cancelled(void *data, struct wl_data_source *ds)
{
	(void)data;
	if (ds && peak_wayland.ds == ds)
		peak_wayland_ds_kill(&peak_wayland.ds);
	else if (ds && peak_wayland.drag_ds == ds)
		peak_wayland_ds_kill(&peak_wayland.drag_ds);
}

static void
peak_wayland_ds_dnd_drop(void *data, struct wl_data_source *ds)
{
	(void)data;
	(void)ds;
}

static void
peak_wayland_ds_dnd_finished(void *data, struct wl_data_source *ds)
{
	peak_wayland_ds_cancelled(data, ds);
}

static void
peak_wayland_ds_action(void *data, struct wl_data_source *ds, uint32_t action)
{
	(void)data;
	(void)ds;
	(void)action;
}

static int
peak_wayland_clip_set(PeakWindowInternal *intern, PeakClip which, const char *utf8, size_t n)
{
	union wl_argument args[2];
	static const struct {
		void (*target)(void *, struct wl_data_source *, const char *);
		void (*send)(void *, struct wl_data_source *, const char *, int32_t);
		void (*cancelled)(void *, struct wl_data_source *);
		void (*drop)(void *, struct wl_data_source *);
		void (*finished)(void *, struct wl_data_source *);
		void (*action)(void *, struct wl_data_source *, uint32_t);
	} dsl = {
		peak_wayland_ds_target, peak_wayland_ds_send, peak_wayland_ds_cancelled,
		peak_wayland_ds_dnd_drop, peak_wayland_ds_dnd_finished, peak_wayland_ds_action
	};

	(void)intern;
	(void)utf8;
	(void)n;
	if (which != PEAK_CLIP_CLIPBOARD)
		return 1;
	if (!peak_wayland.dd || !peak_wayland.ddm || !wl_data_source_interface.name)
		return 1;
	if (peak_wayland.ds)
		peak_wayland_ds_kill(&peak_wayland.ds);
	peak_wayland.ds = (struct wl_data_source *)peak_wayland_marshal((struct wl_proxy *)peak_wayland.ddm, 0, &wl_data_source_interface, NULL);
	if (!peak_wayland.ds)
		return 1;
	peak_wl.wl_proxy_add_listener((struct wl_proxy *)peak_wayland.ds, (void (**)(void))(void *)&dsl, NULL);
	memset(args, 0, sizeof args);
	args[0].s = "text/plain;charset=utf-8";
	peak_wayland_marshal((struct wl_proxy *)peak_wayland.ds, 0, NULL, args);
	args[0].s = "text/plain";
	peak_wayland_marshal((struct wl_proxy *)peak_wayland.ds, 0, NULL, args);
	memset(args, 0, sizeof args);
	args[0].o = (struct wl_object *)peak_wayland.ds;
	args[1].u = peak_wayland.serial;
	peak_wayland_marshal((struct wl_proxy *)peak_wayland.dd, 1, NULL, args);
	peak_wl.wl_display_flush(peak_wayland.display);
	return 1;
}

static int
peak_wayland_offer_recv(struct wl_data_offer *o, const char *mime, char **out, size_t *out_n)
{
	int pfd[2], rfd, flags, got, empty;
	union wl_argument args[2];
	char buf[4096], *acc;
	size_t n;
	struct pollfd pf[2];

	if (!o || !mime || !out || !out_n)
		return 0;
	*out = NULL;
	*out_n = 0;
	if (pipe(pfd) < 0)
		return 0;
	fcntl(pfd[0], F_SETFD, FD_CLOEXEC);
	fcntl(pfd[1], F_SETFD, FD_CLOEXEC);
	memset(args, 0, sizeof args);
	args[0].s = mime;
	args[1].h = pfd[1];
	peak_wayland_marshal((struct wl_proxy *)o, 1, NULL, args);
	close(pfd[1]);
	peak_wl.wl_display_flush(peak_wayland.display);
	rfd = pfd[0];
	flags = fcntl(rfd, F_GETFL, 0);
	if (flags >= 0)
		fcntl(rfd, F_SETFL, flags | O_NONBLOCK);
	acc = NULL;
	n = 0;
	got = 0;
	empty = 0;
	for (;;) {
		ssize_t r;

		r = read(rfd, buf, sizeof buf);
		if (r > 0) {
			char *q;

			if (n + (size_t)r > PEAK_CLIP_MAX)
				r = (ssize_t)(PEAK_CLIP_MAX - n);
			if (r <= 0)
				break;
			q = realloc(acc, n + (size_t)r);
			if (!q)
				break;
			acc = q;
			memcpy(acc + n, buf, (size_t)r);
			n += (size_t)r;
			got = 1;
			empty = 0;
			continue;
		}
		if (r == 0)
			break;
		if (errno != EAGAIN && errno != EINTR)
			break;
		pf[0].fd = rfd;
		pf[0].events = POLLIN;
		pf[0].revents = 0;
		pf[1].fd = peak_wl.wl_display_get_fd(peak_wayland.display);
		pf[1].events = POLLIN;
		pf[1].revents = 0;
		if (poll(pf, 2, 250) <= 0) {
			if (++empty >= 8)
				break;
			continue;
		}
		empty = 0;
		if (pf[1].revents & POLLIN)
			peak_wayland_pump();
	}
	close(rfd);
	if (!got) {
		free(acc);
		return 0;
	}
	*out = acc ? acc : calloc(1, 1);
	*out_n = n;
	return *out ? 1 : 0;
}

static int
peak_wayland_clip_take_offer(PeakClip which, struct peak_wayland_win *w)
{
	char *acc;
	size_t n;
	PeakEvent ev;

	if (!peak_wayland.offer || !peak_wayland.offer_utf8 || !w)
		return 0;
	acc = NULL;
	n = 0;
	if (!peak_wayland_offer_recv(peak_wayland.offer, "text/plain;charset=utf-8", &acc, &n))
		return 0;
	peak_clip_paste_store(which, acc ? acc : "", n);
	free(acc);
	memset(&ev, 0, sizeof ev);
	ev.type = PEAK_EVENT_CLIP;
	ev.clip.which = which;
	ev.clip.n = n;
	peak_q_push(&w->q, ev);
	return 1;
}

static int
peak_wayland_drop_drag(PeakWindowInternal *intern, const char *utf8, size_t n)
{
	struct peak_wayland_win *w;
	union wl_argument args[4];
	char *p;
	static const struct {
		void (*target)(void *, struct wl_data_source *, const char *);
		void (*send)(void *, struct wl_data_source *, const char *, int32_t);
		void (*cancelled)(void *, struct wl_data_source *);
		void (*drop)(void *, struct wl_data_source *);
		void (*finished)(void *, struct wl_data_source *);
		void (*action)(void *, struct wl_data_source *, uint32_t);
	} dsl = {
		peak_wayland_ds_target, peak_wayland_ds_send, peak_wayland_ds_cancelled,
		peak_wayland_ds_dnd_drop, peak_wayland_ds_dnd_finished, peak_wayland_ds_action
	};

	w = intern ? intern->w : NULL;
	if (!w || !w->surface || !utf8)
		return 0;
	if (!peak_wayland.dd || !peak_wayland.ddm || !wl_data_source_interface.name)
		return 0;
	if (peak_wayland.drag_ds)
		return 1;
	p = malloc(n ? n : 1);
	if (!p)
		return 0;
	if (n)
		memcpy(p, utf8, n);
	free(peak_wayland.drag);
	peak_wayland.drag = p;
	peak_wayland.drag_n = n;
	peak_wayland.drag_ds = (struct wl_data_source *)peak_wayland_marshal((struct wl_proxy *)peak_wayland.ddm, 0, &wl_data_source_interface, NULL);
	if (!peak_wayland.drag_ds) {
		free(p);
		peak_wayland.drag = NULL;
		peak_wayland.drag_n = 0;
		return 0;
	}
	peak_wl.wl_proxy_add_listener((struct wl_proxy *)peak_wayland.drag_ds, (void (**)(void))(void *)&dsl, NULL);
	memset(args, 0, sizeof args);
	args[0].s = "text/uri-list";
	peak_wayland_marshal((struct wl_proxy *)peak_wayland.drag_ds, 0, NULL, args);
	args[0].s = "text/plain;charset=utf-8";
	peak_wayland_marshal((struct wl_proxy *)peak_wayland.drag_ds, 0, NULL, args);
	args[0].s = "text/plain";
	peak_wayland_marshal((struct wl_proxy *)peak_wayland.drag_ds, 0, NULL, args);
	if (peak_wl.wl_proxy_get_version((struct wl_proxy *)peak_wayland.drag_ds) >= 3) {
		memset(args, 0, sizeof args);
		args[0].u = 1;
		peak_wayland_marshal((struct wl_proxy *)peak_wayland.drag_ds, 2, NULL, args);
	}
	memset(args, 0, sizeof args);
	args[0].o = (struct wl_object *)peak_wayland.drag_ds;
	args[1].o = (struct wl_object *)w->surface;
	args[2].o = NULL;
	args[3].u = peak_wayland.btn_serial ? peak_wayland.btn_serial : peak_wayland.serial;
	peak_wayland_marshal((struct wl_proxy *)peak_wayland.dd, 0, NULL, args);
	peak_wl.wl_display_flush(peak_wayland.display);
	return 1;
}

static int
peak_wayland_clip_request(PeakWindowInternal *intern, PeakClip which)
{
	struct peak_wayland_win *w;
	const char *p;
	size_t n;
	PeakEvent ev;

	w = intern ? intern->w : NULL;
	if (!w)
		return 0;
	if (which == PEAK_CLIP_PRIMARY && peak_clip_own_get(which, &p, &n) && n) {
		peak_clip_paste_store(which, p, n);
		memset(&ev, 0, sizeof ev);
		ev.type = PEAK_EVENT_CLIP;
		ev.clip.which = which;
		ev.clip.n = n;
		peak_q_push(&w->q, ev);
		return 1;
	}
	if (peak_wayland_clip_take_offer(which, w))
		return 1;
	if (!peak_clip_own_get(which, &p, &n) || !n)
		return 0;
	peak_clip_paste_store(which, p, n);
	memset(&ev, 0, sizeof ev);
	ev.type = PEAK_EVENT_CLIP;
	ev.clip.which = which;
	ev.clip.n = n;
	peak_q_push(&w->q, ev);
	return 1;
}

static int
peak_wayland_epoll(PeakWindowInternal *intern, PeakEvent *ev)
{
	struct peak_wayland_win *w;

	w = intern ? intern->w : NULL;
	if (!w)
		return 0;
	if (peak_q_pop(&w->q, ev))
		return 1;
	peak_wayland_pump();
	peak_wayland_repeat_tick();
	return peak_q_pop(&w->q, ev);
}

static int
peak_wayland_fd(PeakWindowInternal *intern)
{
	(void)intern;
	if (peak_wayland.epoll_fd >= 0)
		return peak_wayland.epoll_fd;
	if (!peak_wayland.display)
		return -1;
	return peak_wl.wl_display_get_fd(peak_wayland.display);
}

static int
peak_wayland_pending(PeakWindowInternal *intern)
{
	struct peak_wayland_win *w;

	w = intern ? intern->w : NULL;
	if (!w)
		return 0;
	if (w->q.n)
		return (int)w->q.n;
	peak_wayland_pump();
	peak_wayland_repeat_tick();
	return w->q.n ? (int)w->q.n : 0;
}

static const char **
peak_wayland_vulkan_get_extensions(uint32_t *count)
{
	static const char *exts[] = {
		"VK_KHR_surface",
		"VK_KHR_wayland_surface",
	};
	if (count)
		*count = 2;
	return exts;
}

static int
peak_wayland_vulkan_create_surface(PeakWindowInternal *intern, void *instance, const void *allocator, void *out_surface)
{
#ifdef PEAK_VULKAN
	struct peak_wayland_win *w;
	VkWaylandSurfaceCreateInfoKHR ci;

	w = intern ? intern->w : NULL;
	if (!w || !w->surface || !peak_wayland.display)
		return 0;
	memset(&ci, 0, sizeof ci);
	ci.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
	ci.display = peak_wayland.display;
	ci.surface = w->surface;
	return vkCreateWaylandSurfaceKHR((VkInstance)instance, &ci,
		(const VkAllocationCallbacks *)allocator, (VkSurfaceKHR *)out_surface) == VK_SUCCESS;
#else
	(void)intern;
	(void)instance;
	(void)allocator;
	(void)out_surface;
	return 0;
#endif
}
/* END p_wayland.c */

static void peak_platform_window_set_title(PeakWindowInternal *intern, const char *name);
static void peak_platform_window_set_size(PeakWindowInternal *intern, uint32_t width, uint32_t height);
static void peak_platform_window_fullscreen(PeakWindowInternal *intern, int on);
static void peak_platform_window_cursor(PeakWindowInternal *intern, int on);
static void peak_platform_window_pointer_relative(PeakWindowInternal *intern, int on);
static float peak_platform_window_scale(PeakWindowInternal *intern);

static int
peak_internal_x11_load(void *handle)
{
#define X(name, ret, args) peak_x11.name = (ret (*) args)dlsym(handle, #name);
	PEAK_X11_API(X)
#undef X
#define X(name, ret, args) || !peak_x11.name
	if (0 PEAK_X11_API(X))
		return 0;
#undef X
	return 1;
}

static PeakKeyCode
peak_internal_x11_key_map(KeySym sym)
{
	if (sym >= XK_a && sym <= XK_z)
		return (PeakKeyCode)(PEAK_KEY_A + (int)(sym - XK_a));
	if (sym >= XK_A && sym <= XK_Z)
		return (PeakKeyCode)(PEAK_KEY_A + (int)(sym - XK_A));
	if (sym >= XK_0 && sym <= XK_9)
		return (PeakKeyCode)(PEAK_KEY_0 + (int)(sym - XK_0));
	switch (sym) {
	case XK_Up: return PEAK_KEY_UP;
	case XK_Down: return PEAK_KEY_DOWN;
	case XK_Left: return PEAK_KEY_LEFT;
	case XK_Right: return PEAK_KEY_RIGHT;
	case XK_space: return PEAK_KEY_SPACE;
	case XK_Escape: return PEAK_KEY_ESCAPE;
	case XK_Return: return PEAK_KEY_ENTER;
	case XK_BackSpace: return PEAK_KEY_BACKSPACE;
	case XK_Tab:
	case XK_ISO_Left_Tab: return PEAK_KEY_TAB;
	case XK_Delete: return PEAK_KEY_DELETE;
	case XK_Insert:
	case XK_KP_Insert: return PEAK_KEY_INSERT;
	case XK_Home:
	case XK_KP_Home: return PEAK_KEY_HOME;
	case XK_End:
	case XK_KP_End: return PEAK_KEY_END;
	case XK_Page_Up:
	case XK_KP_Page_Up: return PEAK_KEY_PAGEUP;
	case XK_Page_Down:
	case XK_KP_Page_Down: return PEAK_KEY_PAGEDOWN;
	case XK_F1: return PEAK_KEY_F1;
	case XK_F2: return PEAK_KEY_F2;
	case XK_F3: return PEAK_KEY_F3;
	case XK_F4: return PEAK_KEY_F4;
	case XK_F5: return PEAK_KEY_F5;
	case XK_F6: return PEAK_KEY_F6;
	case XK_F7: return PEAK_KEY_F7;
	case XK_F8: return PEAK_KEY_F8;
	case XK_F9: return PEAK_KEY_F9;
	case XK_F10: return PEAK_KEY_F10;
	case XK_F11: return PEAK_KEY_F11;
	case XK_F12: return PEAK_KEY_F12;
	default: return PEAK_KEY_UNKNOWN;
	}
}

static PeakKeyMod
peak_internal_x11_mod_map(unsigned int state)
{
	PeakKeyMod m;

	m = 0;
	if (state & ShiftMask)
		m |= PEAK_KEYMOD_SHIFT;
	if (state & ControlMask)
		m |= PEAK_KEYMOD_CTRL;
	if (state & Mod1Mask)
		m |= PEAK_KEYMOD_ALT;
	if (state & LockMask)
		m |= PEAK_KEYMOD_CAPS;
	if (state & Mod4Mask)
		m |= PEAK_KEYMOD_SUPER;
	return m;
}

static Bool
peak_internal_x11_window_match(Display *dpy, XEvent *ev, XPointer arg)
{
	(void)dpy;
	return ev->xany.window == *(Window *)arg;
}

static int
peak_linux_buffer(struct peak_linux_win *w, uint32_t width, uint32_t height)
{
	int screen;

	if (w->ximage) {
		w->ximage->data = NULL;
		XDestroyImage(w->ximage);
		w->ximage = NULL;
	}
	free(w->buffer);
	w->buffer = calloc((size_t)width * height, sizeof *w->buffer);
	if (!w->buffer)
		return 0;

	w->width = width;
	w->height = height;
	screen = DefaultScreen(peak_linux.display);
	w->ximage = peak_x11.XCreateImage(peak_linux.display,
		w->visual ? w->visual : DefaultVisual(peak_linux.display, screen),
		w->depth ? (unsigned int)w->depth : (unsigned int)DefaultDepth(peak_linux.display, screen),
		ZPixmap, 0, (char *)w->buffer, width, height, 32, 0);
	if (!w->ximage) {
		free(w->buffer);
		w->buffer = NULL;
		return 0;
	}
	return 1;
}

static int peak_linux_gp_fd[4];
static int peak_linux_gp_on[4];

static void
peak_linux_gamepad_scan(void)
{
	char path[32];
	int i, fd;

	for (i = 0; i < 4; i++) {
		if (peak_linux_gp_fd[i] >= 0)
			continue;
		snprintf(path, sizeof path, "/dev/input/js%d", i);
		fd = open(path, O_RDONLY | O_NONBLOCK);
		if (fd >= 0)
			peak_linux_gp_fd[i] = fd;
	}
}

static int
peak_linux_gamepad_poll(PeakEvent *ev)
{
	struct js_event js;
	int i, n;

	peak_linux_gamepad_scan();
	for (i = 0; i < 4; i++) {
		if (peak_linux_gp_fd[i] < 0)
			continue;
		n = (int)read(peak_linux_gp_fd[i], &js, sizeof js);
		if (n < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				continue;
			close(peak_linux_gp_fd[i]);
			peak_linux_gp_fd[i] = -1;
			if (peak_linux_gp_on[i]) {
				peak_linux_gp_on[i] = 0;
				memset(ev, 0, sizeof *ev);
				ev->type = PEAK_EVENT_POINTER_DISCONNECTED;
				return 1;
			}
			continue;
		}
		if (n != (int)sizeof js)
			continue;
		if (js.type & JS_EVENT_INIT)
			continue;
		if (!peak_linux_gp_on[i]) {
			peak_linux_gp_on[i] = 1;
			memset(ev, 0, sizeof *ev);
			ev->type = PEAK_EVENT_POINTER_CONNECTED;
			return 1;
		}
		memset(ev, 0, sizeof *ev);
		ev->type = PEAK_EVENT_POINTER;
		if (js.type & JS_EVENT_BUTTON) {
			ev->pointer.state = js.value ? PEAK_POINTER_PRESSED : PEAK_POINTER_RELEASED;
			ev->pointer.type = (js.number == 1) ? PEAK_POINTER_RIGHT :
			                   (js.number == 2) ? PEAK_POINTER_MIDDLE : PEAK_POINTER_LEFT;
		} else {
			ev->pointer.state = PEAK_POINTER_MOVED;
			ev->pointer.type = PEAK_POINTER_LEFT;
			if (js.number == 0)
				ev->pointer.x = (float)js.value / 32767.f;
			else
				ev->pointer.y = (float)js.value / 32767.f;
		}
		return 1;
	}
	return 0;
}

static int
peak_platform_init(void)
{
	void *handle;
	int i;

	if (peak_linux_kind == PEAK_LINUX_WAYLAND || peak_linux_kind == PEAK_LINUX_X11)
		return 1;
	for (i = 0; i < 4; i++)
		peak_linux_gp_fd[i] = -1;
	if (peak_wayland_init()) {
		peak_linux_kind = PEAK_LINUX_WAYLAND;
		return 1;
	}

	if (!peak_x11.XOpenDisplay) {
		if (!(handle = dlopen(PEAK_X11_LINUX, RTLD_LOCAL | RTLD_NOW))) {
			fputs("Failed to load X11 library. What system are you fucking using and abusing?", stderr);
			return 0;
		}
		if (!peak_internal_x11_load(handle)) {
			fputs("Failed to load X11 symbols", stderr);
			return 0;
		}
	}
	if (!peak_linux.display) {
		if (!(peak_linux.display = peak_x11.XOpenDisplay(NULL))) {
			fputs("Failed to open X11 display", stderr);
			return 0;
		}
		peak_linux.wm_delete_window = peak_x11.XInternAtom(peak_linux.display, "WM_DELETE_WINDOW", False);
		peak_linux.net_wm_state = peak_x11.XInternAtom(peak_linux.display, "_NET_WM_STATE", False);
		peak_linux.net_wm_state_fullscreen = peak_x11.XInternAtom(peak_linux.display, "_NET_WM_STATE_FULLSCREEN", False);
		peak_linux.xdnd_aware = peak_x11.XInternAtom(peak_linux.display, "XdndAware", False);
		peak_linux.xdnd_enter = peak_x11.XInternAtom(peak_linux.display, "XdndEnter", False);
		peak_linux.xdnd_position = peak_x11.XInternAtom(peak_linux.display, "XdndPosition", False);
		peak_linux.xdnd_drop = peak_x11.XInternAtom(peak_linux.display, "XdndDrop", False);
		peak_linux.xdnd_status = peak_x11.XInternAtom(peak_linux.display, "XdndStatus", False);
		peak_linux.xdnd_finished = peak_x11.XInternAtom(peak_linux.display, "XdndFinished", False);
		peak_linux.xdnd_selection = peak_x11.XInternAtom(peak_linux.display, "XdndSelection", False);
		peak_linux.xdnd_type_list = peak_x11.XInternAtom(peak_linux.display, "XdndTypeList", False);
		peak_linux.xdnd_action_copy = peak_x11.XInternAtom(peak_linux.display, "XdndActionCopy", False);
		peak_linux.xdnd_prop = peak_x11.XInternAtom(peak_linux.display, "PEAK_XDND", False);
		peak_linux.uri_list = peak_x11.XInternAtom(peak_linux.display, "text/uri-list", False);
		peak_linux.net_wm_pid = peak_x11.XInternAtom(peak_linux.display, "_NET_WM_PID", False);
	}
	peak_linux_kind = PEAK_LINUX_X11;
	return 1;
}

static void
peak_platform_quit(void)
{
	int i;

	if (peak_linux_kind == PEAK_LINUX_WAYLAND) {
		peak_wayland_quit();
		peak_linux_kind = PEAK_LINUX_NONE;
		return;
	}
	for (i = 0; i < 4; i++) {
		if (peak_linux_gp_fd[i] >= 0) {
			close(peak_linux_gp_fd[i]);
			peak_linux_gp_fd[i] = -1;
		}
		peak_linux_gp_on[i] = 0;
	}
	/* NOTE: NVIDIA's Vulkan ICD registers an XCloseDisplay hook, then
	 * vkDestroyInstance unloads the ICD. Closing afterwards is a SIGSEGV
	 * into unmapped memory. The connection is dropped on process exit. */
	if (!peak_linux.display)
		return;
	peak_linux.display = 0;
	peak_linux_kind = PEAK_LINUX_NONE;
}

static int
peak_linux_visual32(int screen, XVisualInfo *out)
{
	XVisualInfo tpl;
	XVisualInfo *list;
	int n;

	memset(&tpl, 0, sizeof tpl);
	tpl.screen = screen;
	tpl.depth = 32;
	tpl.class = TrueColor;
	list = peak_x11.XGetVisualInfo(peak_linux.display,
		VisualScreenMask | VisualDepthMask | VisualClassMask, &tpl, &n);
	if (!list || n < 1)
		return 0;
	*out = list[0];
	peak_x11.XFree(list);
	return 1;
}

static PeakWindowInternal
peak_platform_window_open(const char *name, uint32_t width, uint32_t height, uint32_t flags)
{
	PeakWindowInternal intern = {0};
	struct peak_linux_win *w;
	XVisualInfo vi;
	int screen;
	long evmask;
	unsigned long xdnd_ver = 5;

	if (peak_linux_kind != PEAK_LINUX_X11 && peak_platform_init() && peak_linux_kind == PEAK_LINUX_WAYLAND)
		return peak_wayland_window_open(name, width, height, flags);
	if (!peak_linux.display && !peak_platform_init())
		return intern;

	w = calloc(1, sizeof *w);
	if (!w)
		return intern;

	screen = DefaultScreen(peak_linux.display);
	evmask = KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask |
		PointerMotionMask | StructureNotifyMask | PropertyChangeMask;
	w->visual = DefaultVisual(peak_linux.display, screen);
	w->depth = DefaultDepth(peak_linux.display, screen);
	if ((flags & PEAK_WINDOW_TRANSPARENT) && peak_linux_visual32(screen, &vi)) {
		XSetWindowAttributes swa;

		memset(&swa, 0, sizeof swa);
		w->visual = vi.visual;
		w->depth = vi.depth;
		w->colormap = peak_x11.XCreateColormap(peak_linux.display,
			RootWindow(peak_linux.display, screen), vi.visual, AllocNone);
		w->colormap_owned = 1;
		swa.colormap = w->colormap;
		swa.border_pixel = 0;
		swa.background_pixel = 0;
		swa.event_mask = evmask;
		w->window = peak_x11.XCreateWindow(peak_linux.display,
			RootWindow(peak_linux.display, screen), 0, 0, width, height, 0,
			vi.depth, InputOutput, vi.visual,
			CWColormap | CWBorderPixel | CWBackPixel | CWEventMask, &swa);
	} else {
		w->window = peak_x11.XCreateSimpleWindow(peak_linux.display,
			RootWindow(peak_linux.display, screen), 0, 0, width, height, 0,
			BlackPixel(peak_linux.display, screen), BlackPixel(peak_linux.display, screen));
		peak_x11.XSelectInput(peak_linux.display, w->window, evmask);
	}
	peak_x11.XStoreName(peak_linux.display, w->window, name);
	peak_x11.XSetWMProtocols(peak_linux.display, w->window, &peak_linux.wm_delete_window, 1);
	w->gfx_ctx = peak_x11.XCreateGC(peak_linux.display, w->window, 0, NULL);

	if (!peak_linux_buffer(w, width, height)) {
		peak_x11.XFreeGC(peak_linux.display, w->gfx_ctx);
		peak_x11.XDestroyWindow(peak_linux.display, w->window);
		if (w->colormap_owned)
			peak_x11.XFreeColormap(peak_linux.display, w->colormap);
		free(w);
		return intern;
	}

	w->flags = (int)flags;
	w->cursor_on = 1;
	intern.w = w;
	if (peak_linux.xdnd_aware)
		peak_x11.XChangeProperty(peak_linux.display, w->window, peak_linux.xdnd_aware, XA_ATOM, 32,
			PropModeReplace, (unsigned char *)&xdnd_ver, 1);
	if (peak_linux.net_wm_pid) {
		unsigned long pid;

		pid = (unsigned long)getpid();
		peak_x11.XChangeProperty(peak_linux.display, w->window, peak_linux.net_wm_pid, XA_CARDINAL, 32,
			PropModeReplace, (unsigned char *)&pid, 1);
	}
	if (flags & PEAK_WINDOW_FULLSCREEN)
		peak_platform_window_fullscreen(&intern, 1);
	peak_x11.XMapRaised(peak_linux.display, w->window);
	peak_x11.XFlush(peak_linux.display);
	return intern;
}

static void
peak_platform_window_close(PeakWindowInternal *intern)
{
	struct peak_linux_win *w;

	if (peak_linux_kind == PEAK_LINUX_WAYLAND) {
		peak_wayland_window_close(intern);
		return;
	}
	w = intern ? intern->w : NULL;
	if (!w || !w->window || !peak_linux.display)
		return;
	if (w->blank && peak_x11.XFreeCursor)
		peak_x11.XFreeCursor(peak_linux.display, w->blank);
	if (w->ximage) {
		w->ximage->data = NULL;
		XDestroyImage(w->ximage);
	}
	free(w->buffer);
	if (w->gfx_ctx)
		peak_x11.XFreeGC(peak_linux.display, w->gfx_ctx);
	peak_x11.XDestroyWindow(peak_linux.display, w->window);
	if (w->colormap_owned)
		peak_x11.XFreeColormap(peak_linux.display, w->colormap);
	free(w);
	intern->w = NULL;
}

static uint32_t *
peak_platform_window_buffer(PeakWindowInternal *intern, size_t *width, size_t *height)
{
	if (peak_linux_kind == PEAK_LINUX_WAYLAND)
		return peak_wayland_window_buffer(intern, width, height);
	struct peak_linux_win *w;

	w = intern ? intern->w : NULL;
	if (!w || !w->window) {
		*width = 0;
		*height = 0;
		return NULL;
	}
	*width = w->width;
	*height = w->height;
	return w->buffer;
}

static void
peak_platform_window_present(PeakWindowInternal *intern)
{
	struct peak_linux_win *w;

	if (peak_linux_kind == PEAK_LINUX_WAYLAND) {
		peak_wayland_window_present(intern);
		return;
	}
	w = intern ? intern->w : NULL;
	if (!w || !w->ximage || !peak_linux.display)
		return;
	peak_x11.XPutImage(peak_linux.display, w->window, w->gfx_ctx, w->ximage,
		0, 0, 0, 0, w->width, w->height);
	peak_x11.XFlush(peak_linux.display);
}

static void
peak_linux_clip_atoms(void)
{
	if (!peak_linux.display || peak_linux.clip_clipboard)
		return;
	peak_linux.clip_clipboard = peak_x11.XInternAtom(peak_linux.display, "CLIPBOARD", False);
	peak_linux.clip_utf8 = peak_x11.XInternAtom(peak_linux.display, "UTF8_STRING", False);
	peak_linux.clip_targets = peak_x11.XInternAtom(peak_linux.display, "TARGETS", False);
	peak_linux.clip_incr = peak_x11.XInternAtom(peak_linux.display, "INCR", False);
	peak_linux.clip_text = peak_x11.XInternAtom(peak_linux.display, "TEXT", False);
	peak_linux.clip_prop = peak_x11.XInternAtom(peak_linux.display, "PEAK_CLIP", False);
}

static Atom
peak_linux_clip_atom(PeakClip which)
{
	peak_linux_clip_atoms();
	return which == PEAK_CLIP_PRIMARY ? XA_PRIMARY : peak_linux.clip_clipboard;
}

static PeakClip
peak_linux_clip_which(Atom a)
{
	peak_linux_clip_atoms();
	if (a == XA_PRIMARY)
		return PEAK_CLIP_PRIMARY;
	return PEAK_CLIP_CLIPBOARD;
}

static void
peak_linux_clip_reply(XSelectionRequestEvent *req, Atom prop)
{
	XEvent ev;

	memset(&ev, 0, sizeof ev);
	ev.xselection.type = SelectionNotify;
	ev.xselection.display = req->display;
	ev.xselection.requestor = req->requestor;
	ev.xselection.selection = req->selection;
	ev.xselection.target = req->target;
	ev.xselection.property = prop;
	ev.xselection.time = req->time;
	peak_x11.XSendEvent(peak_linux.display, req->requestor, False, NoEventMask, &ev);
	peak_x11.XFlush(peak_linux.display);
}

static void
peak_linux_clip_request_sel(XSelectionRequestEvent *req)
{
	const char *p;
	size_t n;
	PeakClip which;
	Atom targets[5];

	peak_linux_clip_atoms();
	if (req->property == None) {
		peak_linux_clip_reply(req, None);
		return;
	}
	which = peak_linux_clip_which(req->selection);
	if (!peak_clip_own_get(which, &p, &n))
		n = 0;
	if (req->target == peak_linux.clip_targets) {
		targets[0] = peak_linux.clip_targets;
		targets[1] = peak_linux.clip_utf8;
		targets[2] = XA_STRING;
		targets[3] = peak_linux.clip_text;
		peak_x11.XChangeProperty(peak_linux.display, req->requestor, req->property,
			XA_ATOM, 32, PropModeReplace, (const unsigned char *)targets, 4);
		peak_linux_clip_reply(req, req->property);
		return;
	}
	if (req->target == peak_linux.clip_utf8 || req->target == peak_linux.clip_text) {
		peak_x11.XChangeProperty(peak_linux.display, req->requestor, req->property,
			peak_linux.clip_utf8, 8, PropModeReplace, (const unsigned char *)p, (int)n);
		peak_linux_clip_reply(req, req->property);
		return;
	}
	if (req->target == XA_STRING) {
		peak_x11.XChangeProperty(peak_linux.display, req->requestor, req->property,
			XA_STRING, 8, PropModeReplace, (const unsigned char *)p, (int)n);
		peak_linux_clip_reply(req, req->property);
		return;
	}
	peak_linux_clip_reply(req, None);
}

static int
peak_linux_clip_incr_add(const char *p, size_t n)
{
	char *q;

	if (peak_clip_incr_n + n > PEAK_CLIP_MAX)
		n = PEAK_CLIP_MAX - peak_clip_incr_n;
	if (!n)
		return 1;
	q = realloc(peak_clip_incr, peak_clip_incr_n + n);
	if (!q)
		return 0;
	memcpy(q + peak_clip_incr_n, p, n);
	peak_clip_incr = q;
	peak_clip_incr_n += n;
	return 1;
}

static int
peak_linux_latin1_utf8(const unsigned char *s, size_t n, char **out, size_t *out_n)
{
	char *d;
	size_t i;
	size_t o;

	d = malloc(n * 2 + 1);
	if (!d)
		return 0;
	o = 0;
	for (i = 0; i < n && o + 2 < n * 2 + 1; i++) {
		if (s[i] < 0x80)
			d[o++] = (char)s[i];
		else {
			d[o++] = (char)(0xC0 | (s[i] >> 6));
			d[o++] = (char)(0x80 | (s[i] & 0x3F));
		}
	}
	*out = d;
	*out_n = o;
	return 1;
}

static int
peak_linux_clip_take_prop(Window window, Atom prop, PeakClip which, PeakEvent *ev)
{
	Atom type;
	int fmt;
	unsigned long nitems;
	unsigned long remain;
	unsigned char *data;
	char *utf8;
	size_t un;

	data = NULL;
	if (peak_x11.XGetWindowProperty(peak_linux.display, window, prop, 0, (long)(PEAK_CLIP_MAX / 4),
			False, AnyPropertyType, &type, &fmt, &nitems, &remain, &data) != Success) {
		peak_clip_req_on = 0;
		return 0;
	}
	if (type == None || !data) {
		if (data)
			peak_x11.XFree(data);
		peak_x11.XDeleteProperty(peak_linux.display, window, prop);
		if (!peak_clip_req_xa && peak_clip_req_on) {
			peak_clip_req_xa = 1;
			peak_x11.XConvertSelection(peak_linux.display, peak_linux_clip_atom(which),
				XA_STRING, peak_linux.clip_prop, window, CurrentTime);
			peak_x11.XFlush(peak_linux.display);
		} else {
			peak_clip_req_on = 0;
		}
		return 0;
	}
	if (type == peak_linux.clip_incr) {
		free(peak_clip_incr);
		peak_clip_incr = NULL;
		peak_clip_incr_n = 0;
		peak_clip_incr_on = 1;
		peak_clip_incr_which = which;
		peak_x11.XFree(data);
		peak_x11.XDeleteProperty(peak_linux.display, window, prop);
		return 0;
	}
	utf8 = NULL;
	un = 0;
	if (type == XA_STRING || fmt != 8) {
		if (!peak_linux_latin1_utf8(data, (size_t)nitems, &utf8, &un)) {
			peak_x11.XFree(data);
			peak_x11.XDeleteProperty(peak_linux.display, window, prop);
			peak_clip_req_on = 0;
			return 0;
		}
		peak_clip_paste_store(which, utf8, un);
		free(utf8);
	} else {
		peak_clip_paste_store(which, (const char *)data, (size_t)nitems);
	}
	peak_x11.XFree(data);
	peak_x11.XDeleteProperty(peak_linux.display, window, prop);
	peak_clip_req_on = 0;
	ev->type = PEAK_EVENT_CLIP;
	ev->clip.which = which;
	ev->clip.n = peak_clip.paste_n;
	return 1;
}

static int
peak_linux_clip_property(struct peak_linux_win *w, XPropertyEvent *pe, PeakEvent *ev)
{
	Atom type;
	int fmt;
	unsigned long nitems;
	unsigned long remain;
	unsigned char *data;

	if (!peak_clip_incr_on || pe->state != PropertyNewValue || pe->atom != peak_linux.clip_prop)
		return 0;
	data = NULL;
	if (peak_x11.XGetWindowProperty(peak_linux.display, w->window, pe->atom, 0,
			(long)(PEAK_CLIP_MAX / 4), False, AnyPropertyType, &type, &fmt, &nitems, &remain, &data) != Success)
		return 0;
	if (!nitems) {
		if (data)
			peak_x11.XFree(data);
		peak_x11.XDeleteProperty(peak_linux.display, w->window, pe->atom);
		peak_clip_paste_store(peak_clip_incr_which, peak_clip_incr ? peak_clip_incr : "", peak_clip_incr_n);
		free(peak_clip_incr);
		peak_clip_incr = NULL;
		peak_clip_incr_n = 0;
		peak_clip_incr_on = 0;
		peak_clip_req_on = 0;
		ev->type = PEAK_EVENT_CLIP;
		ev->clip.which = peak_clip_incr_which;
		ev->clip.n = peak_clip.paste_n;
		return 1;
	}
	peak_linux_clip_incr_add((const char *)data, (size_t)nitems);
	if (data)
		peak_x11.XFree(data);
	peak_x11.XDeleteProperty(peak_linux.display, w->window, pe->atom);
	return 0;
}

static void
peak_platform_window_set_title(PeakWindowInternal *intern, const char *name)
{
	struct peak_linux_win *w;

	if (peak_linux_kind == PEAK_LINUX_WAYLAND) {
		peak_wayland_window_set_title(intern, name);
		return;
	}
	w = intern ? intern->w : NULL;
	if (!w || !w->window || !peak_linux.display || !name)
		return;
	peak_x11.XStoreName(peak_linux.display, w->window, name);
	peak_x11.XFlush(peak_linux.display);
}

static void
peak_platform_window_set_size(PeakWindowInternal *intern, uint32_t width, uint32_t height)
{
	struct peak_linux_win *w;

	if (peak_linux_kind == PEAK_LINUX_WAYLAND) {
		peak_wayland_window_set_size(intern, width, height);
		return;
	}
	w = intern ? intern->w : NULL;
	if (!w || !w->window || !peak_linux.display || !peak_x11.XResizeWindow)
		return;
	peak_x11.XResizeWindow(peak_linux.display, w->window, width, height);
	peak_x11.XFlush(peak_linux.display);
}

static void
peak_platform_window_fullscreen(PeakWindowInternal *intern, int on)
{
	struct peak_linux_win *w;
	XEvent e;

	if (peak_linux_kind == PEAK_LINUX_WAYLAND) {
		peak_wayland_window_fullscreen(intern, on);
		return;
	}
	w = intern ? intern->w : NULL;
	if (!w || !w->window || !peak_linux.display || !peak_linux.net_wm_state)
		return;
	memset(&e, 0, sizeof e);
	e.xclient.type = ClientMessage;
	e.xclient.window = w->window;
	e.xclient.message_type = peak_linux.net_wm_state;
	e.xclient.format = 32;
	e.xclient.data.l[0] = on ? 1 : 0;
	e.xclient.data.l[1] = (long)peak_linux.net_wm_state_fullscreen;
	e.xclient.data.l[2] = 0;
	e.xclient.data.l[3] = 1;
	peak_x11.XSendEvent(peak_linux.display, DefaultRootWindow(peak_linux.display), False,
		SubstructureNotifyMask | SubstructureRedirectMask, &e);
	peak_x11.XFlush(peak_linux.display);
}

static void
peak_platform_window_cursor(PeakWindowInternal *intern, int on)
{
	struct peak_linux_win *w;
	Pixmap bm;
	XColor black;

	if (peak_linux_kind == PEAK_LINUX_WAYLAND) {
		peak_wayland_window_cursor(intern, on);
		return;
	}
	w = intern ? intern->w : NULL;
	if (!w || !w->window || !peak_linux.display)
		return;
	w->cursor_on = on;
	if (on) {
		if (peak_x11.XUndefineCursor)
			peak_x11.XUndefineCursor(peak_linux.display, w->window);
		return;
	}
	if (!w->blank && peak_x11.XCreatePixmap && peak_x11.XCreatePixmapCursor) {
		memset(&black, 0, sizeof black);
		bm = peak_x11.XCreatePixmap(peak_linux.display, w->window, 1, 1, 1);
		w->blank = peak_x11.XCreatePixmapCursor(peak_linux.display, bm, bm, &black, &black, 0, 0);
		peak_x11.XFreePixmap(peak_linux.display, bm);
	}
	if (w->blank)
		peak_x11.XDefineCursor(peak_linux.display, w->window, w->blank);
}

static void
peak_platform_window_pointer_relative(PeakWindowInternal *intern, int on)
{
	struct peak_linux_win *w;

	if (peak_linux_kind == PEAK_LINUX_WAYLAND) {
		peak_wayland_window_pointer_relative(intern, on);
		return;
	}
	w = intern ? intern->w : NULL;
	if (!w || !w->window || !peak_linux.display)
		return;
	w->relative = on;
	if (on && peak_x11.XGrabPointer)
		peak_x11.XGrabPointer(peak_linux.display, w->window, True,
			PointerMotionMask | ButtonPressMask | ButtonReleaseMask,
			GrabModeAsync, GrabModeAsync, w->window, None, CurrentTime);
	else if (!on && peak_x11.XUngrabPointer)
		peak_x11.XUngrabPointer(peak_linux.display, CurrentTime);
}

static float
peak_platform_window_scale(PeakWindowInternal *intern)
{
	if (peak_linux_kind == PEAK_LINUX_WAYLAND)
		return peak_wayland_window_scale(intern);
	(void)intern;
	return 1.f;
}

static int
peak_platform_drop_drag(PeakWindowInternal *intern, const char *utf8, size_t n)
{
	if (peak_linux_kind == PEAK_LINUX_WAYLAND)
		return peak_wayland_drop_drag(intern, utf8, n);
	(void)intern;
	(void)utf8;
	(void)n;
	return 0;
}

static int
peak_platform_clip_set(PeakWindowInternal *intern, PeakClip which, const char *utf8, size_t n)
{
	if (peak_linux_kind == PEAK_LINUX_WAYLAND)
		return peak_wayland_clip_set(intern, which, utf8, n);
	struct peak_linux_win *w;

	(void)utf8;
	(void)n;
	w = intern ? intern->w : NULL;
	if (!w || !w->window || !peak_linux.display)
		return 0;
	peak_linux_clip_atoms();
	peak_x11.XSetSelectionOwner(peak_linux.display, peak_linux_clip_atom(which), w->window, CurrentTime);
	peak_x11.XFlush(peak_linux.display);
	return 1;
}

static int
peak_platform_clip_request(PeakWindowInternal *intern, PeakClip which)
{
	struct peak_linux_win *w;

	if (peak_linux_kind == PEAK_LINUX_WAYLAND)
		return peak_wayland_clip_request(intern, which);
	w = intern ? intern->w : NULL;
	if (!w || !w->window || !peak_linux.display)
		return 0;
	peak_linux_clip_atoms();
	peak_clip_req_on = 1;
	peak_clip_req_which = which;
	peak_clip_req_xa = 0;
	peak_clip_incr_on = 0;
	peak_x11.XConvertSelection(peak_linux.display, peak_linux_clip_atom(which),
		peak_linux.clip_utf8, peak_linux.clip_prop, w->window, CurrentTime);
	peak_x11.XFlush(peak_linux.display);
	return 1;
}

static void
peak_linux_xdnd_finished(struct peak_linux_win *w, int ok)
{
	XEvent r;

	if (!w || !w->xdnd_source || !peak_linux.display || !peak_linux.xdnd_finished)
		return;
	memset(&r, 0, sizeof r);
	r.xclient.type = ClientMessage;
	r.xclient.display = peak_linux.display;
	r.xclient.window = w->xdnd_source;
	r.xclient.message_type = peak_linux.xdnd_finished;
	r.xclient.format = 32;
	r.xclient.data.l[0] = (long)w->window;
	r.xclient.data.l[1] = ok ? 1 : 0;
	r.xclient.data.l[2] = ok ? (long)peak_linux.xdnd_action_copy : 0;
	peak_x11.XSendEvent(peak_linux.display, w->xdnd_source, False, 0, &r);
	peak_x11.XFlush(peak_linux.display);
}

static bool
peak_platform_epoll(PeakWindowInternal *intern, PeakEvent *ev)
{
	struct peak_linux_win *w;
	XEvent xev;

	if (peak_linux_kind == PEAK_LINUX_WAYLAND) {
		if (peak_wayland_epoll(intern, ev))
			return 1;
		return peak_linux_gamepad_poll(ev);
	}
	w = intern ? intern->w : NULL;
	if (!w || !w->window || !peak_linux.display)
		return 0;
	if (w->extra_on) {
		*ev = w->extra;
		w->extra_on = 0;
		return 1;
	}
	if (peak_linux_gamepad_poll(ev))
		return 1;

	while (peak_x11.XCheckIfEvent(peak_linux.display, &xev, peak_internal_x11_window_match, (XPointer)&w->window)) {
		switch (xev.type) {
		case ClientMessage:
			if ((Atom)xev.xclient.data.l[0] == peak_linux.wm_delete_window) {
				ev->type = PEAK_EVENT_WINDOW_CLOSE;
				return 1;
			}
			if (xev.xclient.message_type == peak_linux.xdnd_enter) {
				w->xdnd_source = (Window)xev.xclient.data.l[0];
				continue;
			}
			if (xev.xclient.message_type == peak_linux.xdnd_position) {
				XEvent r;

				w->xdnd_source = (Window)xev.xclient.data.l[0];
				memset(&r, 0, sizeof r);
				r.xclient.type = ClientMessage;
				r.xclient.display = peak_linux.display;
				r.xclient.window = w->xdnd_source;
				r.xclient.message_type = peak_linux.xdnd_status;
				r.xclient.format = 32;
				r.xclient.data.l[0] = (long)w->window;
				r.xclient.data.l[1] = 3;
				r.xclient.data.l[4] = (long)peak_linux.xdnd_action_copy;
				peak_x11.XSendEvent(peak_linux.display, w->xdnd_source, False, 0, &r);
				peak_x11.XFlush(peak_linux.display);
				continue;
			}
			if (xev.xclient.message_type == peak_linux.xdnd_drop) {
				Time t;

				w->xdnd_source = (Window)xev.xclient.data.l[0];
				t = (Time)xev.xclient.data.l[2];
				w->xdnd_time = t ? t : CurrentTime;
				peak_x11.XConvertSelection(peak_linux.display, peak_linux.xdnd_selection,
					peak_linux.uri_list, peak_linux.xdnd_prop ? peak_linux.xdnd_prop : peak_linux.clip_prop,
					w->window, w->xdnd_time);
				peak_x11.XFlush(peak_linux.display);
				continue;
			}
			continue;
		case KeyPress:
		case KeyRelease: {
			char buf[8];
			KeySym ks = 0;
			int n;

			memset(buf, 0, sizeof buf);
			n = peak_x11.XLookupString(&xev.xkey, buf, (int)sizeof buf, &ks, NULL);
			ev->type = (xev.type == KeyPress) ? PEAK_EVENT_KEY_DOWN : PEAK_EVENT_KEY_UP;
			ev->key.key = peak_internal_x11_key_map(ks ? ks : peak_x11.XLookupKeysym(&xev.xkey, 0));
			ev->key.mod = peak_internal_x11_mod_map(xev.xkey.state);
			ev->key.code = (n > 0) ? (uint32_t)(unsigned char)buf[0] : 0;
			if (xev.type == KeyPress && n > 0 && (unsigned char)buf[0] >= 32) {
				peak_text_store(buf, (size_t)n);
				w->extra_on = 1;
				memset(&w->extra, 0, sizeof w->extra);
				w->extra.type = PEAK_EVENT_TEXT;
				w->extra.text.n = (size_t)n;
			}
			return 1;
		}
		case ButtonPress:
		case ButtonRelease:
			ev->type = PEAK_EVENT_POINTER;
			ev->pointer.state = (xev.type == ButtonPress) ? PEAK_POINTER_PRESSED : PEAK_POINTER_RELEASED;
			ev->pointer.x = (float)xev.xbutton.x;
			ev->pointer.y = (float)xev.xbutton.y;
			ev->pointer.mod = peak_internal_x11_mod_map(xev.xbutton.state);
			if (xev.xbutton.button == Button4)
				ev->pointer.type = PEAK_POINTER_WHEEL_UP;
			else if (xev.xbutton.button == Button5)
				ev->pointer.type = PEAK_POINTER_WHEEL_DOWN;
			else if (xev.xbutton.button == Button2)
				ev->pointer.type = PEAK_POINTER_MIDDLE;
			else if (xev.xbutton.button == Button3)
				ev->pointer.type = PEAK_POINTER_RIGHT;
			else
				ev->pointer.type = PEAK_POINTER_LEFT;
			return 1;
		case MotionNotify:
			ev->type = PEAK_EVENT_POINTER;
			ev->pointer.state = PEAK_POINTER_MOVED;
			ev->pointer.mod = peak_internal_x11_mod_map(xev.xmotion.state);
			ev->pointer.type = (xev.xmotion.state & Button3Mask) ? PEAK_POINTER_RIGHT :
			                   (xev.xmotion.state & Button2Mask) ? PEAK_POINTER_MIDDLE : PEAK_POINTER_LEFT;
			if (w->relative) {
				ev->pointer.x = (float)xev.xmotion.x - w->last_x;
				ev->pointer.y = (float)xev.xmotion.y - w->last_y;
			} else {
				ev->pointer.x = (float)xev.xmotion.x;
				ev->pointer.y = (float)xev.xmotion.y;
			}
			w->last_x = (float)xev.xmotion.x;
			w->last_y = (float)xev.xmotion.y;
			if (w->relative && peak_x11.XWarpPointer)
				peak_x11.XWarpPointer(peak_linux.display, None, w->window, 0, 0, 0, 0,
					(int)w->width / 2, (int)w->height / 2);
			return 1;
		case ConfigureNotify: {
			uint32_t width = (uint32_t)xev.xconfigure.width;
			uint32_t height = (uint32_t)xev.xconfigure.height;
			if (width == w->width && height == w->height)
				continue;
			if (!peak_linux_buffer(w, width, height))
				continue;
			ev->type = PEAK_EVENT_WINDOW_RESIZE;
			ev->resize.width = w->width;
			ev->resize.height = w->height;
			return 1;
		}
		case SelectionRequest:
			peak_linux_clip_request_sel(&xev.xselectionrequest);
			continue;
		case SelectionClear:
			continue;
		case SelectionNotify:
			if (xev.xselection.selection == peak_linux.xdnd_selection) {
				Atom type = 0;
				int fmt = 0;
				unsigned long nitems = 0, after = 0;
				unsigned char *data = NULL;
				const char *p, *nl;

				if (xev.xselection.property == None) {
					peak_linux_xdnd_finished(w, 0);
					continue;
				}
				if (peak_x11.XGetWindowProperty(peak_linux.display, w->window, xev.xselection.property,
						0, 0x10000, True, AnyPropertyType, &type, &fmt, &nitems, &after, &data) == Success && data) {
					p = (const char *)data;
					if (!strncmp(p, "file://", 7))
						p += 7;
					nl = strchr(p, '\n');
					nitems = nl ? (unsigned long)(nl - p) : nitems;
					while (nitems && (p[nitems - 1] == '\r' || p[nitems - 1] == '\n'))
						nitems--;
					peak_drop_store(p, (size_t)nitems);
					ev->type = PEAK_EVENT_DROP;
					ev->drop.n = (size_t)nitems;
					peak_x11.XFree(data);
					peak_linux_xdnd_finished(w, 1);
					return 1;
				}
				if (data)
					peak_x11.XFree(data);
				peak_linux_xdnd_finished(w, 0);
				continue;
			}
			if (!peak_clip_req_on)
				continue;
			if (xev.xselection.property == None) {
				if (!peak_clip_req_xa) {
					peak_linux_clip_atoms();
					peak_clip_req_xa = 1;
					peak_x11.XConvertSelection(peak_linux.display,
						peak_linux_clip_atom(peak_clip_req_which), XA_STRING,
						peak_linux.clip_prop, w->window, CurrentTime);
					peak_x11.XFlush(peak_linux.display);
				} else {
					peak_clip_req_on = 0;
				}
				continue;
			}
			if (peak_linux_clip_take_prop(w->window, xev.xselection.property,
					peak_clip_req_which, ev))
				return 1;
			continue;
		case PropertyNotify:
			if (peak_linux_clip_property(w, &xev.xproperty, ev))
				return 1;
			continue;
		default:
			continue;
		}
	}
	return 0;
}

static int
peak_platform_fd(PeakWindowInternal *intern)
{
	if (peak_linux_kind == PEAK_LINUX_WAYLAND)
		return peak_wayland_fd(intern);
	(void)intern;
	if (!peak_linux.display)
		return -1;
	return ConnectionNumber(peak_linux.display);
}

static int
peak_platform_pending(PeakWindowInternal *intern)
{
	struct peak_linux_win *w;
	XEvent ev;

	if (peak_linux_kind == PEAK_LINUX_WAYLAND)
		return peak_wayland_pending(intern);
	w = intern ? intern->w : NULL;
	if (!w || !w->window || !peak_linux.display || !peak_x11.XPending)
		return 0;
	/* Display-wide XPending is true for Vulkan WSI / other windows too.
	 * Timeout 0 on that spins the client after the first present. */
	if (peak_x11.XPending(peak_linux.display) <= 0)
		return 0;
	if (!peak_x11.XCheckIfEvent(peak_linux.display, &ev, peak_internal_x11_window_match, (XPointer)&w->window))
		return 0;
	peak_x11.XPutBackEvent(peak_linux.display, &ev);
	return 1;
}

#ifndef PEAK_NO_AUDIO
static int
peak_internal_pulse_load(void)
{
	void *handle;

	if (peak_pulse.pa_simple_new)
		return 1;
	if (!(handle = dlopen(PEAK_PULSE_LINUX, RTLD_LOCAL | RTLD_NOW))) {
		fputs("Failed to load PulseAudio library. What system are you fucking using and abusing?", stderr);
		return 0;
	}
#define X(name, ret, args) peak_pulse.name = (ret (*) args)dlsym(handle, #name);
	PEAK_PULSE_API(X)
#undef X
#define X(name, ret, args) || !peak_pulse.name
	if (0 PEAK_PULSE_API(X)) {
		fputs("Failed to load PulseAudio symbols", stderr);
		return 0;
	}
#undef X
	return 1;
}

static void *
peak_internal_audio_thread(void *arg)
{
	size_t n;
	int error;

	(void)arg;
	n = (size_t)peak_audio.channels * PEAK_AUDIO_FRAMES;
	while (peak_audio.run) {
		memset(peak_audio.buf, 0, n * sizeof(int16_t));
		if (peak_audio.fill)
			peak_audio.fill(peak_audio.buf, PEAK_AUDIO_FRAMES, peak_audio.userdata);
		if (peak_pulse.pa_simple_write(peak_audio.stream, peak_audio.buf, n * sizeof(int16_t), &error) < 0)
			break;
	}
	return NULL;
}

static int
peak_platform_audio_start(uint32_t channels, uint32_t rate, void (*fill)(int16_t *out, size_t frames, void *userdata), void *userdata)
{
	PeakPaSampleSpec ss;
	int error;

	if (channels > 32)
		return 0;
	if (!peak_internal_pulse_load())
		return 0;

	ss.format = 3; /* PA_SAMPLE_S16LE */
	ss.rate = rate;
	ss.channels = (uint8_t)channels;
	peak_audio.stream = peak_pulse.pa_simple_new(NULL, "Peak", 1, NULL, "playback", &ss, NULL, NULL, &error);
	if (!peak_audio.stream) {
		fputs("Failed to open PulseAudio stream", stderr);
		return 0;
	}
	if (!(peak_audio.buf = calloc((size_t)channels * PEAK_AUDIO_FRAMES, sizeof(int16_t)))) {
		peak_pulse.pa_simple_free(peak_audio.stream);
		peak_audio.stream = NULL;
		return 0;
	}
	peak_audio.fill = fill;
	peak_audio.userdata = userdata;
	peak_audio.channels = channels;
	peak_audio.run = 1;
	if (pthread_create(&peak_audio.thread, NULL, peak_internal_audio_thread, NULL) != 0) {
		peak_audio.thread_on = 0;
		peak_audio.run = 0;
		free(peak_audio.buf);
		peak_audio.buf = NULL;
		peak_pulse.pa_simple_free(peak_audio.stream);
		peak_audio.stream = NULL;
		return 0;
	}
	peak_audio.thread_on = 1;
	return 1;
}
#endif

static uint64_t
peak_platform_get_time(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * NANOS_PER_SEC + (uint64_t)ts.tv_nsec;
}

static void
peak_platform_sleep_ns(int64_t ns)
{
	struct timespec ts;
	if (ns <= 0) return;
	ts.tv_sec = ns / 1000000000ll;
	ts.tv_nsec = ns % 1000000000ll;
	nanosleep(&ts, NULL);
}

static const char **
peak_platform_vulkan_get_extensions(uint32_t *count)
{
	static const char *exts[] = {
		"VK_KHR_surface",
		"VK_KHR_xlib_surface",
	};
	if (peak_linux_kind == PEAK_LINUX_WAYLAND)
		return peak_wayland_vulkan_get_extensions(count);
	if (count) *count = 2;
	return exts;
}

static int
peak_platform_vulkan_create_surface(PeakWindowInternal *intern, void *instance, const void *allocator, void *out_surface)
{
	if (peak_linux_kind == PEAK_LINUX_WAYLAND)
		return peak_wayland_vulkan_create_surface(intern, instance, allocator, out_surface);
#ifdef PEAK_VULKAN
	struct peak_linux_win *w;
	VkXlibSurfaceCreateInfoKHR ci;
	w = intern ? intern->w : NULL;
	if (!w || !peak_linux.display || !w->window) return 0;
	memset(&ci, 0, sizeof ci);
	ci.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
	ci.dpy = peak_linux.display;
	ci.window = w->window;
	return vkCreateXlibSurfaceKHR((VkInstance)instance, &ci, (const VkAllocationCallbacks *)allocator, (VkSurfaceKHR *)out_surface) == VK_SUCCESS;
#else
	(void)intern; (void)instance; (void)allocator; (void)out_surface;
	return 0;
#endif
}

#ifndef PEAK_NO_AUDIO
static void
peak_platform_audio_stop(void)
{
	if (!peak_audio.run && !peak_audio.stream)
		return;
	peak_audio.run = 0;
	if (peak_audio.thread_on) {
		pthread_join(peak_audio.thread, NULL);
		peak_audio.thread_on = 0;
	}
	if (peak_audio.stream) {
		peak_pulse.pa_simple_free(peak_audio.stream);
		peak_audio.stream = NULL;
	}
	free(peak_audio.buf);
	peak_audio.buf = NULL;
	peak_audio.fill = NULL;
	peak_audio.userdata = NULL;
}
#else
static int
peak_platform_audio_start(uint32_t channels, uint32_t rate, void (*fill)(int16_t *out, size_t frames, void *userdata), void *userdata)
{
	(void)channels;
	(void)rate;
	(void)fill;
	(void)userdata;
	return 0;
}

static void
peak_platform_audio_stop(void)
{
}
#endif

static int
peak_linux_net_pid(Window w)
{
	Atom type;
	int fmt;
	unsigned long nitems;
	unsigned long after;
	unsigned char *data;
	int pid;

	type = 0;
	fmt = 0;
	nitems = 0;
	after = 0;
	data = NULL;
	pid = 0;
	if (!peak_linux.display || !peak_linux.net_wm_pid || !w)
		return 0;
	if (peak_x11.XGetWindowProperty(peak_linux.display, w, peak_linux.net_wm_pid,
			0, 1, False, XA_CARDINAL, &type, &fmt, &nitems, &after, &data) == Success
			&& data && nitems)
		pid = (int)(*(unsigned long *)data);
	if (data)
		peak_x11.XFree(data);
	return pid;
}

int
peak_pointer_pid(PeakWindow *win)
{
	Window root;
	Window root_ret;
	Window child;
	Window parent;
	Window w;
	Window *kids;
	int rx;
	int ry;
	int wx;
	int wy;
	unsigned int mask;
	unsigned int n;
	int pid;

	(void)win;
	if (peak_linux_kind != PEAK_LINUX_X11 || !peak_linux.display || !peak_x11.XQueryPointer
			|| !peak_x11.XQueryTree)
		return 0;
	root = DefaultRootWindow(peak_linux.display);
	child = 0;
	if (!peak_x11.XQueryPointer(peak_linux.display, root, &root_ret, &child,
			&rx, &ry, &wx, &wy, &mask) || !child)
		return 0;
	w = child;
	for (;;) {
		child = 0;
		if (!peak_x11.XQueryPointer(peak_linux.display, w, &root_ret, &child,
				&rx, &ry, &wx, &wy, &mask) || !child || child == w)
			break;
		w = child;
	}
	for (;;) {
		pid = peak_linux_net_pid(w);
		if (pid)
			return pid;
		kids = NULL;
		n = 0;
		parent = 0;
		if (!peak_x11.XQueryTree(peak_linux.display, w, &root_ret, &parent, &kids, &n))
			return 0;
		if (kids)
			peak_x11.XFree(kids);
		if (!parent || parent == root_ret || parent == w)
			return 0;
		w = parent;
	}
}

int
peak_pointer_local(PeakWindow *win, int *x, int *y)
{
	struct peak_linux_win *xw;
	struct peak_wayland_win *ww;
	Window root;
	Window child;
	int rx;
	int ry;
	int wx;
	int wy;
	unsigned int mask;
	int pid;

	if (!win || !win->internal.w)
		return 0;
	if (peak_linux_kind == PEAK_LINUX_WAYLAND) {
		ww = win->internal.w;
		if (!ww->pointer_in)
			return 0;
		wx = (int)ww->last_x;
		wy = (int)ww->last_y;
		if (wx < 0 || wy < 0 || wx >= (int)ww->width || wy >= (int)ww->height)
			return 0;
		if (x)
			*x = wx;
		if (y)
			*y = wy;
		return 1;
	}
	if (peak_linux_kind != PEAK_LINUX_X11 || !peak_linux.display || !peak_x11.XQueryPointer)
		return 0;
	xw = win->internal.w;
	if (!xw->window)
		return 0;
	if (!peak_x11.XQueryPointer(peak_linux.display, xw->window, &root, &child,
			&rx, &ry, &wx, &wy, &mask))
		return 0;
	if (wx < 0 || wy < 0 || wx >= (int)xw->width || wy >= (int)xw->height)
		return 0;
	pid = peak_pointer_pid(win);
	if (pid && pid != (int)getpid())
		return 0;
	if (x)
		*x = wx;
	if (y)
		*y = wy;
	return 1;
}

#define PEAK_HAS_POINTER_PID 1
/* END p_linux.c */
#elif defined(PEAK_MACOS)
/* BEGIN p_macos.c */
/*
 * macOS window, input, CALayer present, Metal WSI, and AudioQueue.
 * AppKit is Objective-C. Compile the implementation as ObjC (clang -x objective-c).
 *
 * * 0.6.0 - @vasco - macos
 */

#include <AppKit/AppKit.h>
#include <AudioToolbox/AudioToolbox.h>
#include <CoreGraphics/CoreGraphics.h>
#include <QuartzCore/CAMetalLayer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef PEAK_VULKAN
#define VK_USE_PLATFORM_METAL_EXT
#include <vulkan/vulkan.h>
#endif

#define PEAK_AUDIO_FRAMES 256
#define PEAK_AUDIO_BUFFERS 3

struct peak_macos_win {
	NSWindow *window;
	NSView *view;
	CAMetalLayer *layer;
	id delegate;
	uint32_t *buffer;
	uint32_t width;
	uint32_t height;
	int force_close;
	int flags;
	int cursor_on;
	int relative;
	int touch_n;
	float last_x, last_y;
	PeakQ q;
};

typedef struct {
	volatile int run;
	AudioQueueRef queue;
	AudioQueueBufferRef buf[PEAK_AUDIO_BUFFERS];
	uint32_t channels;
	uint32_t bytes;
	void (*fill)(int16_t *out, size_t frames, void *userdata);
	void *userdata;
} PeakAudio;

@interface PeakMacDelegate : NSObject <NSWindowDelegate>
@property(nonatomic, assign) struct peak_macos_win *w;
@end

@interface PeakMacView : NSView
@property(nonatomic, assign) struct peak_macos_win *w;
@end

static int peak_internal_macos_buffer(struct peak_macos_win *w, uint32_t width, uint32_t height);
static PeakKeyCode peak_internal_macos_key_map(unsigned short kc);
static PeakKeyMod peak_internal_macos_mod_map(NSEventModifierFlags flags);
static void peak_internal_macos_translate(struct peak_macos_win *w, NSEvent *ev);
static void peak_internal_macos_pump(struct peak_macos_win *w);
static void peak_internal_macos_audio_cb(void *ud, AudioQueueRef q, AudioQueueBufferRef buf);
static int peak_platform_init(void);
static void peak_platform_quit(void);
static PeakWindowInternal peak_platform_window_open(const char *name, uint32_t width, uint32_t height, uint32_t flags);
static void peak_platform_window_close(PeakWindowInternal *intern);
static uint32_t *peak_platform_window_buffer(PeakWindowInternal *intern, size_t *width, size_t *height);
static void peak_platform_window_present(PeakWindowInternal *intern);
static bool peak_platform_epoll(PeakWindowInternal *intern, PeakEvent *ev);
static int peak_platform_fd(PeakWindowInternal *intern);
static int peak_platform_pending(PeakWindowInternal *intern);
static int peak_platform_audio_start(uint32_t channels, uint32_t rate, void (*fill)(int16_t *out, size_t frames, void *userdata), void *userdata);
static void peak_platform_audio_stop(void);
static uint64_t peak_platform_get_time(void);
static void peak_platform_sleep_ns(int64_t ns);
static const char **peak_platform_vulkan_get_extensions(uint32_t *count);
static int peak_platform_vulkan_create_surface(PeakWindowInternal *intern, void *instance, const void *allocator, void *out_surface);
static void peak_platform_window_set_title(PeakWindowInternal *intern, const char *name);
static void peak_platform_window_set_size(PeakWindowInternal *intern, uint32_t width, uint32_t height);
static void peak_platform_window_fullscreen(PeakWindowInternal *intern, int on);
static void peak_platform_window_cursor(PeakWindowInternal *intern, int on);
static void peak_platform_window_pointer_relative(PeakWindowInternal *intern, int on);
static float peak_platform_window_scale(PeakWindowInternal *intern);

static NSApplication *peak_macos_app;
static PeakAudio peak_audio;

@implementation PeakMacDelegate
- (BOOL)windowShouldClose:(NSWindow *)sender
{
	struct peak_macos_win *w;
	PeakEvent ev;

	(void)sender;
	w = self.w;
	if (!w)
		return YES;
	if (w->force_close)
		return YES;
	memset(&ev, 0, sizeof ev);
	ev.type = PEAK_EVENT_WINDOW_CLOSE;
	peak_q_push(&w->q, ev);
	return NO;
}
@end

@implementation PeakMacView
- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender
{
	(void)sender;
	return NSDragOperationCopy;
}
- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender
{
	NSArray *files;
	NSString *p;
	const char *u;
	PeakEvent ev;
	size_t n;

	files = [[sender draggingPasteboard] propertyListForType:NSFilenamesPboardType];
	if (!files || ![files count] || !self.w)
		return NO;
	p = [files objectAtIndex:0];
	u = [p UTF8String];
	if (!u)
		return NO;
	n = strlen(u);
	peak_drop_store(u, n);
	memset(&ev, 0, sizeof ev);
	ev.type = PEAK_EVENT_DROP;
	ev.drop.n = n;
	peak_q_push(&self.w->q, ev);
	return YES;
}
@end

static int
peak_internal_macos_buffer(struct peak_macos_win *w, uint32_t width, uint32_t height)
{
	uint32_t *buffer;

	if (!width || !height)
		return 0;
	if (!(buffer = calloc((size_t)width * height, sizeof *buffer)))
		return 0;
	free(w->buffer);
	w->buffer = buffer;
	w->width = width;
	w->height = height;
	return 1;
}

static PeakKeyCode
peak_internal_macos_key_map(unsigned short kc)
{
	switch (kc) {
	case 0x00: return PEAK_KEY_A;
	case 0x0B: return PEAK_KEY_B;
	case 0x08: return PEAK_KEY_C;
	case 0x02: return PEAK_KEY_D;
	case 0x0E: return PEAK_KEY_E;
	case 0x03: return PEAK_KEY_F;
	case 0x05: return PEAK_KEY_G;
	case 0x04: return PEAK_KEY_H;
	case 0x22: return PEAK_KEY_I;
	case 0x26: return PEAK_KEY_J;
	case 0x28: return PEAK_KEY_K;
	case 0x25: return PEAK_KEY_L;
	case 0x2E: return PEAK_KEY_M;
	case 0x2D: return PEAK_KEY_N;
	case 0x1F: return PEAK_KEY_O;
	case 0x23: return PEAK_KEY_P;
	case 0x0C: return PEAK_KEY_Q;
	case 0x0F: return PEAK_KEY_R;
	case 0x01: return PEAK_KEY_S;
	case 0x11: return PEAK_KEY_T;
	case 0x20: return PEAK_KEY_U;
	case 0x09: return PEAK_KEY_V;
	case 0x0D: return PEAK_KEY_W;
	case 0x07: return PEAK_KEY_X;
	case 0x10: return PEAK_KEY_Y;
	case 0x06: return PEAK_KEY_Z;
	case 0x1D: return PEAK_KEY_0;
	case 0x12: return PEAK_KEY_1;
	case 0x13: return PEAK_KEY_2;
	case 0x14: return PEAK_KEY_3;
	case 0x15: return PEAK_KEY_4;
	case 0x17: return PEAK_KEY_5;
	case 0x16: return PEAK_KEY_6;
	case 0x1A: return PEAK_KEY_7;
	case 0x1C: return PEAK_KEY_8;
	case 0x19: return PEAK_KEY_9;
	case 0x7E: return PEAK_KEY_UP;
	case 0x7D: return PEAK_KEY_DOWN;
	case 0x7B: return PEAK_KEY_LEFT;
	case 0x7C: return PEAK_KEY_RIGHT;
	case 0x31: return PEAK_KEY_SPACE;
	case 0x35: return PEAK_KEY_ESCAPE;
	case 0x24: return PEAK_KEY_ENTER;
	case 0x4C: return PEAK_KEY_ENTER;
	case 0x33: return PEAK_KEY_BACKSPACE;
	case 0x30: return PEAK_KEY_TAB;
	case 0x75: return PEAK_KEY_DELETE;
	case 0x72: return PEAK_KEY_INSERT;
	case 0x73: return PEAK_KEY_HOME;
	case 0x77: return PEAK_KEY_END;
	case 0x74: return PEAK_KEY_PAGEUP;
	case 0x79: return PEAK_KEY_PAGEDOWN;
	case 0x7A: return PEAK_KEY_F1;
	case 0x78: return PEAK_KEY_F2;
	case 0x63: return PEAK_KEY_F3;
	case 0x76: return PEAK_KEY_F4;
	case 0x60: return PEAK_KEY_F5;
	case 0x61: return PEAK_KEY_F6;
	case 0x62: return PEAK_KEY_F7;
	case 0x64: return PEAK_KEY_F8;
	case 0x65: return PEAK_KEY_F9;
	case 0x6D: return PEAK_KEY_F10;
	case 0x67: return PEAK_KEY_F11;
	case 0x6F: return PEAK_KEY_F12;
	default: return PEAK_KEY_UNKNOWN;
	}
}

static PeakKeyMod
peak_internal_macos_mod_map(NSEventModifierFlags flags)
{
	PeakKeyMod m;

	m = 0;
	if (flags & NSEventModifierFlagShift)
		m |= PEAK_KEYMOD_SHIFT;
	if (flags & NSEventModifierFlagControl)
		m |= PEAK_KEYMOD_CTRL;
	if (flags & NSEventModifierFlagCommand)
		m |= PEAK_KEYMOD_SUPER;
	if (flags & NSEventModifierFlagOption)
		m |= PEAK_KEYMOD_ALT;
	if (flags & NSEventModifierFlagCapsLock)
		m |= PEAK_KEYMOD_CAPS;
	return m;
}

static void
peak_internal_macos_translate(struct peak_macos_win *w, NSEvent *ev)
{
	PeakEvent out;
	NSPoint pt;
	const char *utf8;

	memset(&out, 0, sizeof out);
	pt = [ev locationInWindow];
	switch ([ev type]) {
	case NSEventTypeKeyDown:
	case NSEventTypeKeyUp:
		out.type = ([ev type] == NSEventTypeKeyDown) ? PEAK_EVENT_KEY_DOWN : PEAK_EVENT_KEY_UP;
		out.key.key = peak_internal_macos_key_map([ev keyCode]);
		out.key.mod = peak_internal_macos_mod_map([ev modifierFlags]);
		utf8 = [[ev characters] UTF8String];
		out.key.code = (utf8 && utf8[0]) ? (uint32_t)(unsigned char)utf8[0] : 0;
		peak_q_push(&w->q, out);
		if ([ev type] == NSEventTypeKeyDown && utf8 && utf8[0] && (unsigned char)utf8[0] >= 32) {
			PeakEvent tev;
			size_t n;

			n = strlen(utf8);
			peak_text_store(utf8, n);
			memset(&tev, 0, sizeof tev);
			tev.type = PEAK_EVENT_TEXT;
			tev.text.n = n;
			peak_q_push(&w->q, tev);
		}
		break;
	case NSEventTypeScrollWheel:
		out.type = PEAK_EVENT_POINTER;
		out.pointer.state = PEAK_POINTER_PRESSED;
		out.pointer.type = ([ev deltaY] < 0) ? PEAK_POINTER_WHEEL_DOWN : PEAK_POINTER_WHEEL_UP;
		out.pointer.x = (float)pt.x;
		out.pointer.y = (float)((double)w->height - pt.y);
		out.pointer.mod = peak_internal_macos_mod_map([ev modifierFlags]);
		peak_q_push(&w->q, out);
		break;
	case NSEventTypeLeftMouseDown:
	case NSEventTypeRightMouseDown:
	case NSEventTypeOtherMouseDown:
	case NSEventTypeLeftMouseUp:
	case NSEventTypeRightMouseUp:
	case NSEventTypeOtherMouseUp:
	case NSEventTypeMouseMoved:
	case NSEventTypeLeftMouseDragged:
	case NSEventTypeRightMouseDragged:
		out.type = PEAK_EVENT_POINTER;
		out.pointer.x = (float)pt.x;
		out.pointer.y = (float)((double)w->height - pt.y);
		if (w->relative && ([ev type] == NSEventTypeMouseMoved || [ev type] == NSEventTypeLeftMouseDragged
		    || [ev type] == NSEventTypeRightMouseDragged)) {
			out.pointer.x -= w->last_x;
			out.pointer.y -= w->last_y;
		}
		w->last_x = (float)pt.x;
		w->last_y = (float)((double)w->height - pt.y);
		out.pointer.mod = peak_internal_macos_mod_map([ev modifierFlags]);
		if ([ev type] == NSEventTypeMouseMoved || [ev type] == NSEventTypeLeftMouseDragged
		    || [ev type] == NSEventTypeRightMouseDragged) {
			out.pointer.state = PEAK_POINTER_MOVED;
			out.pointer.type = ([ev type] == NSEventTypeRightMouseDragged)
				? PEAK_POINTER_RIGHT : PEAK_POINTER_LEFT;
		} else if ([ev type] == NSEventTypeLeftMouseDown || [ev type] == NSEventTypeRightMouseDown
		    || [ev type] == NSEventTypeOtherMouseDown) {
			out.pointer.state = PEAK_POINTER_PRESSED;
			out.pointer.type = ([ev type] == NSEventTypeRightMouseDown) ? PEAK_POINTER_RIGHT :
			                   ([ev type] == NSEventTypeOtherMouseDown) ? PEAK_POINTER_MIDDLE : PEAK_POINTER_LEFT;
		} else {
			out.pointer.state = PEAK_POINTER_RELEASED;
			out.pointer.type = ([ev type] == NSEventTypeRightMouseUp) ? PEAK_POINTER_RIGHT :
			                   ([ev type] == NSEventTypeOtherMouseUp) ? PEAK_POINTER_MIDDLE : PEAK_POINTER_LEFT;
		}
		peak_q_push(&w->q, out);
		break;
	default:
		break;
	}
}

static void
peak_internal_macos_pump(struct peak_macos_win *w)
{
	NSEvent *ev;
	NSRect bounds;
	CGFloat scale;
	uint32_t width, height;

	if (!peak_macos_app || !w->window)
		return;
	for (;;) {
		ev = [peak_macos_app nextEventMatchingMask:NSEventMaskAny
			untilDate:[NSDate distantPast]
			inMode:NSDefaultRunLoopMode
			dequeue:YES];
		if (!ev)
			break;
		peak_internal_macos_translate(w, ev);
	}
	bounds = [w->view bounds];
	scale = [w->window backingScaleFactor];
	if (scale < 1.0)
		scale = 1.0;
	width = (uint32_t)(bounds.size.width * scale);
	height = (uint32_t)(bounds.size.height * scale);
	if (width && height && (width != w->width || height != w->height)) {
		PeakEvent evr;

		if (peak_internal_macos_buffer(w, width, height)) {
			memset(&evr, 0, sizeof evr);
			evr.type = PEAK_EVENT_WINDOW_RESIZE;
			evr.resize.width = w->width;
			evr.resize.height = w->height;
			peak_q_push(&w->q, evr);
			w->layer.drawableSize = CGSizeMake((CGFloat)w->width, (CGFloat)w->height);
		}
	}
}

static int
peak_platform_init(void)
{
	if (peak_macos_app)
		return 1;
	peak_macos_app = [NSApplication sharedApplication];
	if (!peak_macos_app) {
		fputs("Failed to get NSApplication. What system are you fucking using and abusing?", stderr);
		return 0;
	}
	[peak_macos_app setActivationPolicy:NSApplicationActivationPolicyRegular];
	[peak_macos_app finishLaunching];
	return 1;
}

static void
peak_platform_quit(void)
{
	peak_macos_app = nil;
}

static PeakWindowInternal
peak_platform_window_open(const char *name, uint32_t width, uint32_t height, uint32_t flags)
{
	PeakWindowInternal intern = {0};
	struct peak_macos_win *w;
	PeakMacDelegate *del;
	NSRect rect;

	if (!peak_macos_app && !peak_platform_init())
		return intern;
	if (!(w = calloc(1, sizeof *w)))
		return intern;
	if (!peak_internal_macos_buffer(w, width, height)) {
		free(w);
		return intern;
	}

	rect = NSMakeRect(0, 0, (CGFloat)width, (CGFloat)height);
	w->window = [[NSWindow alloc]
		initWithContentRect:rect
		styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
			| NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable)
		backing:NSBackingStoreBuffered
		defer:NO];
	if (!w->window) {
		free(w->buffer);
		free(w);
		return intern;
	}
	[w->window setTitle:[NSString stringWithUTF8String:name]];
	w->flags = (int)flags;
	w->cursor_on = 1;
	if (flags & PEAK_WINDOW_TRANSPARENT) {
		[w->window setOpaque:NO];
		[w->window setBackgroundColor:[NSColor clearColor]];
		[w->window setHasShadow:NO];
	}
	if (flags & PEAK_WINDOW_FULLSCREEN)
		[w->window toggleFullScreen:nil];
	[w->window registerForDraggedTypes:[NSArray arrayWithObject:NSFilenamesPboardType]];
	{
		PeakMacView *view;

		view = [[PeakMacView alloc] initWithFrame:rect];
		view.w = w;
		[w->window setContentView:view];
		w->view = view;
	}
	w->layer = [CAMetalLayer new];
	[w->view setLayer:w->layer];
	[w->view setWantsLayer:YES];
	if (flags & PEAK_WINDOW_TRANSPARENT)
		w->layer.opaque = NO;
	w->layer.drawableSize = CGSizeMake((CGFloat)width, (CGFloat)height);
	del = [PeakMacDelegate new];
	del.w = w;
	w->delegate = del;
	[w->window setDelegate:del];
	[w->window makeKeyAndOrderFront:nil];
	[peak_macos_app activateIgnoringOtherApps:YES];
	intern.w = w;
	return intern;
}

static void
peak_platform_window_close(PeakWindowInternal *intern)
{
	struct peak_macos_win *w;

	w = intern ? intern->w : NULL;
	if (!w)
		return;
	w->force_close = 1;
	if (w->window) {
		[w->window setDelegate:nil];
		[w->window close];
		[w->window release];
	}
	if (w->layer)
		[w->layer release];
	if (w->delegate)
		[w->delegate release];
	free(w->buffer);
	free(w);
	intern->w = NULL;
}

static uint32_t *
peak_platform_window_buffer(PeakWindowInternal *intern, size_t *width, size_t *height)
{
	struct peak_macos_win *w;

	w = intern ? intern->w : NULL;
	if (!w) {
		*width = 0;
		*height = 0;
		return NULL;
	}
	*width = w->width;
	*height = w->height;
	return w->buffer;
}

static void
peak_platform_window_present(PeakWindowInternal *intern)
{
	struct peak_macos_win *w;
	CGColorSpaceRef cs;
	CGDataProviderRef prov;
	CGImageRef img;
	size_t nbytes;

	w = intern ? intern->w : NULL;
	if (!w || !w->layer || !w->buffer)
		return;
	nbytes = (size_t)w->width * w->height * 4;
	cs = CGColorSpaceCreateDeviceRGB();
	prov = CGDataProviderCreateWithData(NULL, w->buffer, nbytes, NULL);
	img = CGImageCreate((size_t)w->width, (size_t)w->height, 8, 32, (size_t)w->width * 4, cs,
		kCGBitmapByteOrder32Little | ((w->flags & PEAK_WINDOW_TRANSPARENT)
			? kCGImageAlphaPremultipliedFirst : kCGImageAlphaNoneSkipFirst),
		prov, NULL, false, kCGRenderingIntentDefault);
	w->layer.contents = (id)img;
	if (img)
		CGImageRelease(img);
	if (prov)
		CGDataProviderRelease(prov);
	if (cs)
		CGColorSpaceRelease(cs);
}

static int
peak_platform_drop_drag(PeakWindowInternal *intern, const char *utf8, size_t n)
{
	(void)intern;
	(void)utf8;
	(void)n;
	return 0;
}

static int
peak_platform_clip_set(PeakWindowInternal *intern, PeakClip which, const char *utf8, size_t n)
{
	NSPasteboard *pb;
	NSString *s;
	char *z;

	(void)intern;
	(void)which;
	z = malloc(n + 1);
	if (!z)
		return 0;
	if (n)
		memcpy(z, utf8, n);
	z[n] = 0;
	s = [[NSString alloc] initWithUTF8String:z];
	free(z);
	if (!s)
		return 0;
	pb = [NSPasteboard generalPasteboard];
	[pb clearContents];
	[pb setString:s forType:NSPasteboardTypeString];
	[s release];
	return 1;
}

static int
peak_platform_clip_request(PeakWindowInternal *intern, PeakClip which)
{
	struct peak_macos_win *w;
	NSPasteboard *pb;
	NSString *s;
	const char *utf8;
	size_t n;
	PeakEvent ev;

	w = intern ? intern->w : NULL;
	if (!w)
		return 0;
	pb = [NSPasteboard generalPasteboard];
	s = [pb stringForType:NSPasteboardTypeString];
	utf8 = s ? [s UTF8String] : "";
	n = utf8 ? strlen(utf8) : 0;
	if (n > PEAK_CLIP_MAX)
		n = PEAK_CLIP_MAX;
	peak_clip_paste_store(which, utf8, n);
	memset(&ev, 0, sizeof ev);
	ev.type = PEAK_EVENT_CLIP;
	ev.clip.which = which;
	ev.clip.n = n;
	peak_q_push(&w->q, ev);
	return 1;
}

static bool
peak_platform_epoll(PeakWindowInternal *intern, PeakEvent *ev)
{
	struct peak_macos_win *w;

	w = intern ? intern->w : NULL;
	if (!w || !w->window)
		return 0;
	if (peak_q_pop(&w->q, ev))
		return 1;
	peak_internal_macos_pump(w);
	return peak_q_pop(&w->q, ev);
}

static int
peak_platform_fd(PeakWindowInternal *intern)
{
	(void)intern;
	return -1;
}

static int
peak_platform_pending(PeakWindowInternal *intern)
{
	struct peak_macos_win *w;
	NSEvent *ev;

	w = intern ? intern->w : NULL;
	if (!w)
		return 0;
	if (w->q.n)
		return (int)w->q.n;
	if (!peak_macos_app)
		return 0;
	ev = [peak_macos_app nextEventMatchingMask:NSEventMaskAny
		untilDate:[NSDate distantPast]
		inMode:NSDefaultRunLoopMode
		dequeue:NO];
	return ev ? 1 : 0;
}

static void
peak_internal_macos_audio_cb(void *ud, AudioQueueRef q, AudioQueueBufferRef buf)
{
	(void)ud;
	if (!peak_audio.run)
		return;
	memset(buf->mAudioData, 0, buf->mAudioDataByteSize);
	if (peak_audio.fill)
		peak_audio.fill((int16_t *)buf->mAudioData,
			(size_t)buf->mAudioDataByteSize / (peak_audio.channels * sizeof(int16_t)),
			peak_audio.userdata);
	AudioQueueEnqueueBuffer(q, buf, 0, NULL);
}

static int
peak_platform_audio_start(uint32_t channels, uint32_t rate, void (*fill)(int16_t *out, size_t frames, void *userdata), void *userdata)
{
	AudioStreamBasicDescription fmt;
	int i;

	if (channels > 32)
		return 0;
	memset(&fmt, 0, sizeof fmt);
	fmt.mSampleRate = (Float64)rate;
	fmt.mFormatID = kAudioFormatLinearPCM;
	fmt.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
	fmt.mBytesPerPacket = channels * 2;
	fmt.mFramesPerPacket = 1;
	fmt.mBytesPerFrame = channels * 2;
	fmt.mChannelsPerFrame = channels;
	fmt.mBitsPerChannel = 16;
	peak_audio.channels = channels;
	peak_audio.bytes = channels * PEAK_AUDIO_FRAMES * 2;
	peak_audio.fill = fill;
	peak_audio.userdata = userdata;
	peak_audio.run = 1;
	if (AudioQueueNewOutput(&fmt, peak_internal_macos_audio_cb, NULL, NULL, NULL, 0, &peak_audio.queue) != 0)
		goto fail;
	for (i = 0; i < PEAK_AUDIO_BUFFERS; i++) {
		if (AudioQueueAllocateBuffer(peak_audio.queue, peak_audio.bytes, &peak_audio.buf[i]) != 0)
			goto fail;
		peak_audio.buf[i]->mAudioDataByteSize = peak_audio.bytes;
		peak_internal_macos_audio_cb(NULL, peak_audio.queue, peak_audio.buf[i]);
	}
	if (AudioQueueStart(peak_audio.queue, NULL) != 0)
		goto fail;
	return 1;
fail:
	peak_platform_audio_stop();
	return 0;
}

static void
peak_platform_audio_stop(void)
{
	int i;

	peak_audio.run = 0;
	if (peak_audio.queue) {
		AudioQueueStop(peak_audio.queue, 1);
		for (i = 0; i < PEAK_AUDIO_BUFFERS; i++)
			peak_audio.buf[i] = NULL;
		AudioQueueDispose(peak_audio.queue, 1);
		peak_audio.queue = NULL;
	}
	peak_audio.fill = NULL;
	peak_audio.userdata = NULL;
}

static uint64_t
peak_platform_get_time(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * NANOS_PER_SEC + (uint64_t)ts.tv_nsec;
}

static void
peak_platform_sleep_ns(int64_t ns)
{
	struct timespec ts;

	if (ns <= 0)
		return;
	ts.tv_sec = ns / 1000000000ll;
	ts.tv_nsec = ns % 1000000000ll;
	nanosleep(&ts, NULL);
}

static const char **
peak_platform_vulkan_get_extensions(uint32_t *count)
{
	static const char *exts[] = {
		"VK_KHR_surface",
		"VK_EXT_metal_surface",
		"VK_KHR_portability_enumeration",
	};
	if (count)
		*count = 3;
	return exts;
}

static int
peak_platform_vulkan_create_surface(PeakWindowInternal *intern, void *instance, const void *allocator, void *out_surface)
{
#ifdef PEAK_VULKAN
	struct peak_macos_win *w;
	VkMetalSurfaceCreateInfoEXT ci;

	w = intern ? intern->w : NULL;
	if (!w || !w->layer)
		return 0;
	memset(&ci, 0, sizeof ci);
	ci.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
	ci.pLayer = (const void *)w->layer;
	return vkCreateMetalSurfaceEXT((VkInstance)instance, &ci,
		(const VkAllocationCallbacks *)allocator, (VkSurfaceKHR *)out_surface) == VK_SUCCESS;
#else
	(void)intern;
	(void)instance;
	(void)allocator;
	(void)out_surface;
	return 0;
#endif
}

static void
peak_platform_window_set_title(PeakWindowInternal *intern, const char *name)
{
	struct peak_macos_win *w;

	w = intern ? intern->w : NULL;
	if (!w || !w->window || !name)
		return;
	[w->window setTitle:[NSString stringWithUTF8String:name]];
}

static void
peak_platform_window_set_size(PeakWindowInternal *intern, uint32_t width, uint32_t height)
{
	struct peak_macos_win *w;
	NSRect r;

	w = intern ? intern->w : NULL;
	if (!w || !w->window)
		return;
	r = [w->window frame];
	r.size.width = (CGFloat)width;
	r.size.height = (CGFloat)height;
	[w->window setFrame:r display:YES];
}

static void
peak_platform_window_fullscreen(PeakWindowInternal *intern, int on)
{
	struct peak_macos_win *w;
	int isfs;

	w = intern ? intern->w : NULL;
	if (!w || !w->window)
		return;
	isfs = ([w->window styleMask] & NSWindowStyleMaskFullScreen) != 0;
	if ((on && !isfs) || (!on && isfs))
		[w->window toggleFullScreen:nil];
}

static void
peak_platform_window_cursor(PeakWindowInternal *intern, int on)
{
	struct peak_macos_win *w;

	w = intern ? intern->w : NULL;
	if (!w)
		return;
	w->cursor_on = on;
	if (on)
		[NSCursor unhide];
	else
		[NSCursor hide];
}

static void
peak_platform_window_pointer_relative(PeakWindowInternal *intern, int on)
{
	struct peak_macos_win *w;

	w = intern ? intern->w : NULL;
	if (!w)
		return;
	w->relative = on;
	if (on)
		CGAssociateMouseAndMouseCursorPosition(false);
	else
		CGAssociateMouseAndMouseCursorPosition(true);
}

static float
peak_platform_window_scale(PeakWindowInternal *intern)
{
	struct peak_macos_win *w;
	CGFloat s;

	w = intern ? intern->w : NULL;
	if (!w || !w->window)
		return 1.f;
	s = [w->window backingScaleFactor];
	return s > 0 ? (float)s : 1.f;
}

/* END p_macos.c */
#elif defined(PEAK_WEB)
/* BEGIN p_emscripten.c */
#include <emscripten.h>
#include <emscripten/html5.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

EM_JS(void, peak_web_dom_open, (const char *id, int w, int h), {
	var name = UTF8ToString(id);
	var c = document.getElementById(name);
	if (!c) {
		c = document.createElement('canvas');
		c.id = name;
		document.body.appendChild(c);
	}
	c.width = w;
	c.height = h;
	c.tabIndex = 0;
	c.focus();
});

EM_JS(void, peak_web_dom_present, (const char *id, int w, int h, uintptr_t pixels), {
	var c = document.getElementById(UTF8ToString(id));
	if (!c) return;
	var ctx = c.getContext('2d');
	if (c.width !== w || c.height !== h) {
		c.width = w;
		c.height = h;
	}
	var img = ctx.createImageData(w, h);
	img.data.set(HEAPU8.subarray(pixels, pixels + w * h * 4));
	ctx.putImageData(img, 0, 0);
});

struct peak_web_win {
	char name[64];
	uint32_t width, height;
	PeakQ q;
	uint32_t buffer[];
};


static void
peak_web_sel(const char *name, char *out, size_t n)
{
	out[0] = '#';
	strncpy(out + 1, name, n - 2);
	out[n - 1] = 0;
}

static PeakKeyCode
peak_web_key_map(const char *code)
{
	if (code[0] == 'K' && code[1] == 'e' && code[2] == 'y' && code[3] >= 'A' && code[3] <= 'Z' && code[4] == 0)
		return (PeakKeyCode)(PEAK_KEY_A + (code[3] - 'A'));
	if (!strcmp(code, "ArrowUp")) return PEAK_KEY_UP;
	if (!strcmp(code, "ArrowDown")) return PEAK_KEY_DOWN;
	if (!strcmp(code, "ArrowLeft")) return PEAK_KEY_LEFT;
	if (!strcmp(code, "ArrowRight")) return PEAK_KEY_RIGHT;
	if (!strcmp(code, "Space")) return PEAK_KEY_SPACE;
	if (!strcmp(code, "Escape")) return PEAK_KEY_ESCAPE;
	if (!strcmp(code, "Enter")) return PEAK_KEY_ENTER;
	if (!strcmp(code, "Backspace")) return PEAK_KEY_BACKSPACE;
	if (!strcmp(code, "Tab")) return PEAK_KEY_TAB;
	if (!strcmp(code, "Delete")) return PEAK_KEY_DELETE;
	if (!strcmp(code, "Insert")) return PEAK_KEY_INSERT;
	if (!strcmp(code, "Home")) return PEAK_KEY_HOME;
	if (!strcmp(code, "End")) return PEAK_KEY_END;
	if (!strcmp(code, "PageUp")) return PEAK_KEY_PAGEUP;
	if (!strcmp(code, "PageDown")) return PEAK_KEY_PAGEDOWN;
	if (code[0] == 'F' && code[1] >= '1' && code[1] <= '9' && code[2] == 0)
		return (PeakKeyCode)(PEAK_KEY_F1 + (code[1] - '1'));
	if (!strcmp(code, "F10")) return PEAK_KEY_F10;
	if (!strcmp(code, "F11")) return PEAK_KEY_F11;
	if (!strcmp(code, "F12")) return PEAK_KEY_F12;
	if (code[0] == 'D' && code[1] == 'i' && code[2] == 'g' && code[3] == 'i' && code[4] == 't' &&
	    code[5] >= '0' && code[5] <= '9' && code[6] == 0)
		return (PeakKeyCode)(PEAK_KEY_0 + (code[5] - '0'));
	return PEAK_KEY_UNKNOWN;
}

static EM_BOOL
peak_web_key(int type, const EmscriptenKeyboardEvent *e, void *ud)
{
	PeakEvent ev = {0};
	ev.type = (type == EMSCRIPTEN_EVENT_KEYDOWN) ? PEAK_EVENT_KEY_DOWN : PEAK_EVENT_KEY_UP;
	ev.key.key = peak_web_key_map(e->code);
	ev.key.mod = (e->shiftKey ? PEAK_KEYMOD_SHIFT : 0) | (e->ctrlKey ? PEAK_KEYMOD_CTRL : 0) | (e->altKey ? PEAK_KEYMOD_ALT : 0) | (e->metaKey ? PEAK_KEYMOD_SUPER : 0);
	peak_q_push(&((struct peak_web_win *)ud)->q, ev);
	if (type == EMSCRIPTEN_EVENT_KEYDOWN && e->key[0] && (unsigned char)e->key[0] >= 32 && e->key[1] == 0) {
		PeakEvent tev = {0};
		peak_text_store(e->key, 1);
		tev.type = PEAK_EVENT_TEXT;
		tev.text.n = 1;
		peak_q_push(&((struct peak_web_win *)ud)->q, tev);
	}
	return EM_TRUE;
}

static EM_BOOL
peak_web_mouse(int type, const EmscriptenMouseEvent *e, void *ud)
{
	PeakEvent ev = {0};
	ev.type = PEAK_EVENT_POINTER;
	ev.pointer.x = (float)e->targetX;
	ev.pointer.y = (float)e->targetY;
	if (type == EMSCRIPTEN_EVENT_MOUSEDOWN)
		ev.pointer.state = PEAK_POINTER_PRESSED;
	else if (type == EMSCRIPTEN_EVENT_MOUSEUP)
		ev.pointer.state = PEAK_POINTER_RELEASED;
	else
		ev.pointer.state = PEAK_POINTER_MOVED;
	if (type == EMSCRIPTEN_EVENT_MOUSEMOVE) {
		ev.pointer.type = (e->buttons & 4) ? PEAK_POINTER_MIDDLE :
		                  (e->buttons & 2) ? PEAK_POINTER_RIGHT : PEAK_POINTER_LEFT;
	} else {
		ev.pointer.type = (e->button == 1) ? PEAK_POINTER_MIDDLE :
		                  (e->button == 2) ? PEAK_POINTER_RIGHT : PEAK_POINTER_LEFT;
	}
	ev.pointer.mod = (e->shiftKey ? PEAK_KEYMOD_SHIFT : 0) | (e->ctrlKey ? PEAK_KEYMOD_CTRL : 0) | (e->altKey ? PEAK_KEYMOD_ALT : 0);
	peak_q_push(&((struct peak_web_win *)ud)->q, ev);
	return EM_TRUE;
}

static void
peak_web_listen(struct peak_web_win *w, int on)
{
	char sel[66];
	peak_web_sel(w->name, sel, sizeof sel);
	emscripten_set_keydown_callback(sel, w, EM_TRUE, on ? peak_web_key : NULL);
	emscripten_set_keyup_callback(sel, w, EM_TRUE, on ? peak_web_key : NULL);
	emscripten_set_mousedown_callback(sel, w, EM_TRUE, on ? peak_web_mouse : NULL);
	emscripten_set_mouseup_callback(sel, w, EM_TRUE, on ? peak_web_mouse : NULL);
	emscripten_set_mousemove_callback(sel, w, EM_TRUE, on ? peak_web_mouse : NULL);
}

static int
peak_platform_init(void)
{
	return 1;
}

static void
peak_platform_quit(void)
{
}

static PeakWindowInternal
peak_platform_window_open(const char *name, uint32_t width, uint32_t height, uint32_t flags)
{
	PeakWindowInternal intern = {0};
	struct peak_web_win *w;

	(void)flags;

	w = calloc(1, sizeof *w + (size_t)width * height * sizeof *w->buffer);
	if (!w)
		return intern;
	strncpy(w->name, name, sizeof w->name - 1);
	w->width = width;
	w->height = height;

	peak_web_dom_open(w->name, (int)width, (int)height);

	peak_web_listen(w, 1);
	intern.w = w;
	return intern;
}

static void
peak_platform_window_close(PeakWindowInternal *intern)
{
	struct peak_web_win *w;
	if (!intern || !intern->w)
		return;
	w = intern->w;
	peak_web_listen(w, 0);
	free(w);
	intern->w = NULL;
}

static uint32_t *
peak_platform_window_buffer(PeakWindowInternal *intern, size_t *width, size_t *height)
{
	struct peak_web_win *w = intern ? intern->w : NULL;
	if (!w) {
		*width = 0;
		*height = 0;
		return NULL;
	}
	*width = w->width;
	*height = w->height;
	return w->buffer;
}

static void
peak_platform_window_present(PeakWindowInternal *intern)
{
	struct peak_web_win *w = intern ? intern->w : NULL;
	if (!w)
		return;
	peak_web_dom_present(w->name, (int)w->width, (int)w->height, (uintptr_t)w->buffer);
}

static int
peak_platform_drop_drag(PeakWindowInternal *intern, const char *utf8, size_t n)
{
	(void)intern;
	(void)utf8;
	(void)n;
	return 0;
}

static int
peak_platform_clip_set(PeakWindowInternal *intern, PeakClip which, const char *utf8, size_t n)
{
	(void)intern;
	(void)which;
	(void)utf8;
	(void)n;
	return 1;
}

static int
peak_platform_clip_request(PeakWindowInternal *intern, PeakClip which)
{
	struct peak_web_win *w;
	const char *p;
	size_t n;
	PeakEvent ev;

	w = intern ? intern->w : NULL;
	if (!w || !peak_clip_own_get(which, &p, &n))
		return 0;
	peak_clip_paste_store(which, p, n);
	memset(&ev, 0, sizeof ev);
	ev.type = PEAK_EVENT_CLIP;
	ev.clip.which = which;
	ev.clip.n = n;
	peak_q_push(&w->q, ev);
	return 1;
}

static bool
peak_platform_epoll(PeakWindowInternal *intern, PeakEvent *ev)
{
	struct peak_web_win *w = intern ? intern->w : NULL;
	return w ? peak_q_pop(&w->q, ev) : 0;
}

static int
peak_platform_fd(PeakWindowInternal *intern)
{
	(void)intern;
	return -1;
}

static int
peak_platform_pending(PeakWindowInternal *intern)
{
	struct peak_web_win *w = intern ? intern->w : NULL;
	return w ? (int)w->q.n : 0;
}

static void
peak_platform_window_set_title(PeakWindowInternal *intern, const char *name)
{
	(void)intern;
	if (name)
		emscripten_set_window_title(name);
}

static void
peak_platform_window_set_size(PeakWindowInternal *intern, uint32_t width, uint32_t height)
{
	struct peak_web_win *w = intern ? intern->w : NULL;
	if (!w || !width || !height)
		return;
	w->width = width;
	w->height = height;
	peak_web_dom_open(w->name, (int)width, (int)height);
}

static void
peak_platform_window_fullscreen(PeakWindowInternal *intern, int on)
{
	struct peak_web_win *w = intern ? intern->w : NULL;
	char sel[66];
	if (!w)
		return;
	peak_web_sel(w->name, sel, sizeof sel);
	if (on)
		emscripten_request_fullscreen(sel, EM_TRUE);
	else
		emscripten_exit_fullscreen();
}

static void
peak_platform_window_cursor(PeakWindowInternal *intern, int on)
{
	(void)intern;
	emscripten_hide_mouse();
	(void)on;
}

static void
peak_platform_window_pointer_relative(PeakWindowInternal *intern, int on)
{
	(void)intern;
	(void)on;
}

static float
peak_platform_window_scale(PeakWindowInternal *intern)
{
	(void)intern;
	return 1.f;
}

#define PEAK_AUDIO_FRAMES 1024

static struct {
	int run;
	uint32_t channels;
	int16_t *buf;
	void (*fill)(int16_t *out, size_t frames, void *userdata);
	void *userdata;
} peak_web_audio;

void EMSCRIPTEN_KEEPALIVE
peak_internal_web_audio_fill(int16_t *out, int frames)
{
	size_t n;

	if (!peak_web_audio.run || !out || frames <= 0)
		return;
	n = (size_t)frames * peak_web_audio.channels;
	memset(out, 0, n * sizeof(int16_t));
	if (peak_web_audio.fill)
		peak_web_audio.fill(out, (size_t)frames, peak_web_audio.userdata);
}

EM_JS(int, peak_web_audio_dom_start, (int channels, int rate, int frames, uintptr_t ptr), {
	var AC = window.AudioContext || window.webkitAudioContext;
	var ctx, proc, i, c, heap, off, ch;
	if (!AC || Module._peak_web_audio)
		return 0;
	ctx = new AC({ sampleRate: rate });
	if (ctx.sampleRate !== rate) {
		ctx.close();
		return 0;
	}
	proc = ctx.createScriptProcessor(frames, 0, channels);
	proc.onaudioprocess = function(e) {
		Module._peak_internal_web_audio_fill(ptr, frames);
		heap = Module.HEAP16;
		off = ptr >> 1;
		for (c = 0; c < channels; c++) {
			ch = e.outputBuffer.getChannelData(c);
			for (i = 0; i < frames; i++)
				ch[i] = heap[off + i * channels + c] / 32768.0;
		}
	};
	proc.connect(ctx.destination);
	ctx.resume();
	Module._peak_web_audio = { ctx: ctx, proc: proc };
	return 1;
});

EM_JS(void, peak_web_audio_dom_stop, (void), {
	var a = Module._peak_web_audio;
	if (!a)
		return;
	a.proc.disconnect();
	a.ctx.close();
	Module._peak_web_audio = null;
});

static int
peak_platform_audio_start(uint32_t channels, uint32_t rate, void (*fill)(int16_t *out, size_t frames, void *userdata), void *userdata)
{
	if (channels > 32)
		return 0;
	if (!(peak_web_audio.buf = calloc((size_t)channels * PEAK_AUDIO_FRAMES, sizeof(int16_t))))
		return 0;
	peak_web_audio.fill = fill;
	peak_web_audio.userdata = userdata;
	peak_web_audio.channels = channels;
	peak_web_audio.run = 1;
	if (!peak_web_audio_dom_start((int)channels, (int)rate, PEAK_AUDIO_FRAMES, (uintptr_t)peak_web_audio.buf)) {
		free(peak_web_audio.buf);
		peak_web_audio.buf = NULL;
		peak_web_audio.run = 0;
		peak_web_audio.fill = NULL;
		return 0;
	}
	return 1;
}

static uint64_t
peak_platform_get_time(void)
{
	return (uint64_t)(emscripten_get_now() * 1000000.0);
}

static void
peak_platform_sleep_ns(int64_t ns)
{
	if (ns <= 0) return;
	emscripten_sleep((unsigned)(ns / 1000000));
}

static const char **
peak_platform_vulkan_get_extensions(uint32_t *count)
{
	if (count) *count = 0;
	return NULL;
}

static int
peak_platform_vulkan_create_surface(PeakWindowInternal *w, void *instance, const void *allocator, void *out_surface)
{
	(void)w; (void)instance; (void)allocator; (void)out_surface;
	return 0;
}

static void
peak_platform_audio_stop(void)
{
	peak_web_audio.run = 0;
	peak_web_audio_dom_stop();
	free(peak_web_audio.buf);
	peak_web_audio.buf = NULL;
	peak_web_audio.fill = NULL;
	peak_web_audio.userdata = NULL;
}

static PeakProc
peak_internal_proc_fail(void)
{
	PeakProc p;

	p.fd = PEAK_HANDLE_INVALID;
	p.pid = 0;
	return p;
}

PeakProc
peak_pty_spawn(const char *file, const char **argv, uint32_t cols, uint32_t rows, uint32_t xpixel, uint32_t ypixel)
{
	(void)file; (void)argv; (void)cols; (void)rows; (void)xpixel; (void)ypixel;
	return peak_internal_proc_fail();
}

void
peak_pty_resize(PeakProc *pty, uint32_t cols, uint32_t rows, uint32_t xpixel, uint32_t ypixel)
{
	(void)pty; (void)cols; (void)rows; (void)xpixel; (void)ypixel;
}

int
peak_pty_reap(PeakProc *pty)
{
	(void)pty;
	return 0;
}

void
peak_pty_close(PeakProc *pty)
{
	if (!pty)
		return;
	pty->fd = PEAK_HANDLE_INVALID;
	pty->pid = 0;
}

int
peak_wait(PeakWindow *win, const PEAK_HANDLE *fds, uint32_t n, int timeout_ms)
{
	(void)win; (void)fds; (void)n; (void)timeout_ms;
	return 0;
}

int
peak_runtime_dir(char *buf, size_t cap, const char *app)
{
	(void)buf; (void)cap; (void)app;
	return 0;
}

PEAK_HANDLE
peak_sock_listen(const char *path)
{
	(void)path;
	return PEAK_HANDLE_INVALID;
}

PEAK_HANDLE
peak_sock_connect(const char *path)
{
	(void)path;
	return PEAK_HANDLE_INVALID;
}

int
peak_sock_send(PEAK_HANDLE sock, const void *buf, size_t n, PEAK_HANDLE pass)
{
	(void)sock; (void)buf; (void)n; (void)pass;
	return 0;
}

int
peak_sock_recv(PEAK_HANDLE sock, void *buf, size_t n, PEAK_HANDLE *pass)
{
	if (pass)
		*pass = PEAK_HANDLE_INVALID;
	(void)sock; (void)buf; (void)n;
	return -1;
}

int
peak_pointer_pid(PeakWindow *win)
{
	(void)win;
	return 0;
}

int
peak_pointer_local(PeakWindow *win, int *x, int *y)
{
	(void)win;
	(void)x;
	(void)y;
	return 0;
}

int
peak_filesystem_mkdir(const char *path)
{
	if (!path || !path[0])
		return 0;
	return mkdir(path, 0777) == 0;
}

int
peak_filesystem_rm(const char *path)
{
	if (!path || !path[0])
		return 0;
	if (unlink(path) == 0)
		return 1;
	return rmdir(path) == 0;
}

int
peak_filesystem_cwd(char *buf, size_t cap)
{
	if (!buf || cap < 2)
		return 0;
	return getcwd(buf, cap) != NULL;
}

int
peak_filesystem_chdir(const char *path)
{
	if (!path || !path[0])
		return 0;
	return chdir(path) == 0;
}

int
peak_filesystem_rename(const char *from, const char *to)
{
	if (!from || !from[0] || !to || !to[0])
		return 0;
	return rename(from, to) == 0;
}

int
peak_pid(void)
{
	return (int)getpid();
}

int
peak_env_set(const char *name, const char *value)
{
	if (!name || !name[0])
		return 0;
	if (value)
		return setenv(name, value, 1) == 0;
	return unsetenv(name) == 0;
}

int
peak_env_get(const char *name, char *buf, size_t cap)
{
	const char *v;
	size_t n;

	if (!name || !name[0] || !buf || cap < 2)
		return 0;
	v = getenv(name);
	if (!v || !v[0])
		return 0;
	n = strlen(v);
	if (n >= cap)
		return 0;
	memcpy(buf, v, n + 1);
	return 1;
}

int
peak_filesystem_list(const char *path, int (*fn)(const char *name, void *ud), void *ud)
{
	(void)path;
	(void)fn;
	(void)ud;
	return 0;
}

int
peak_filesystem_symlink(const char *target, const char *path)
{
	(void)target;
	(void)path;
	return 0;
}

int
peak_filesystem_readlink(const char *path, char *dst, size_t cap)
{
	(void)path;
	(void)dst;
	(void)cap;
	return 0;
}

int
peak_child_arm(void)
{
	return 1;
}

void
peak_child_disarm(void)
{
}

PEAK_HANDLE
peak_child_fd(void)
{
	return PEAK_HANDLE_INVALID;
}

void
peak_child_ack(void)
{
}

int
peak_usr1_arm(void)
{
	return 1;
}

void
peak_usr1_disarm(void)
{
}

PEAK_HANDLE
peak_usr1_fd(void)
{
	return PEAK_HANDLE_INVALID;
}

int
peak_usr1_ack(void)
{
	return 0;
}

int
peak_child_reap(int *pid, int *code)
{
	(void)pid;
	(void)code;
	return 0;
}

int
peak_stdout_silence(void)
{
	return 0;
}

int
peak_stdout_restore(void)
{
	return 0;
}

PEAK_HANDLE
peak_sock_accept(PEAK_HANDLE listen_fd)
{
	(void)listen_fd;
	return PEAK_HANDLE_INVALID;
}

int
peak_fd_read(PEAK_HANDLE fd, void *buf, size_t n)
{
	(void)fd; (void)buf; (void)n;
	return 0;
}

int
peak_fd_write(PEAK_HANDLE fd, const void *buf, size_t n)
{
	(void)fd; (void)buf; (void)n;
	return 0;
}

void
peak_fd_close(PEAK_HANDLE fd)
{
	(void)fd;
}

size_t
peak_pipe_capacity(PEAK_HANDLE fd)
{
	(void)fd;
	return 0;
}

size_t
peak_pipe_set_capacity(PEAK_HANDLE fd, size_t n)
{
	(void)fd;
	(void)n;
	return 0;
}

PeakProc
peak_job_run(const char *cmd, const char *cwd)
{
	(void)cmd; (void)cwd;
	return peak_internal_proc_fail();
}

int
peak_job_reap(PeakProc *job, int *code)
{
	(void)job; (void)code;
	return 0;
}

void
peak_job_kill(PeakProc *job)
{
	if (!job)
		return;
	job->fd = PEAK_HANDLE_INVALID;
	job->pid = 0;
}

int
peak_pid_cwd(int pid, char *buf, size_t cap)
{
	(void)pid; (void)buf; (void)cap;
	return 0;
}

size_t
peak_page_size(void)
{
	return 4096;
}

void *
peak_mirror_map(size_t size)
{
	(void)size;
	return NULL;
}

void
peak_mirror_unmap(void *p, size_t size)
{
	(void)p; (void)size;
}
/* END p_emscripten.c */
#endif

#if defined(PEAK_LINUX) || defined(PEAK_MACOS)
/* BEGIN p_posix.c */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <util.h>
#else
#include <pty.h>
#endif
#ifdef __linux__
#include <sys/syscall.h>
long syscall(long number, ...);
#ifndef F_SETPIPE_SZ
#define F_SETPIPE_SZ 1031
#endif
#ifndef F_GETPIPE_SZ
#define F_GETPIPE_SZ 1032
#endif
#endif

#define PEAK_WAIT_MAX 64
#define PEAK_PROC_MAX 32

typedef struct PeakProcRec {
	int used;
	int out;
	int tty;
} PeakProcRec;

static PeakProcRec peak_procs[PEAK_PROC_MAX];

static int peak_internal_nb(int fd);
static PeakProc peak_internal_proc_fail(void);
static int peak_internal_memfd(void);
static size_t peak_internal_io_n(size_t n);
static PeakProcRec *peak_internal_proc_find(int fd);
static PeakProcRec *peak_internal_proc_slot(void);
static void peak_internal_proc_clear(PeakProcRec *r);
static void peak_internal_proc_bind(int out, int tty);
static int peak_internal_read(int fd, void *buf, size_t n);
static void peak_internal_tty_no_opost(int fd);
static int peak_internal_status_code(int status);
static void peak_internal_sigchld(int sig);
static void peak_internal_sigusr1(int sig);

static int peak_child_r = -1;
static int peak_child_w = -1;
static int peak_usr1_r = -1;
static int peak_usr1_w = -1;
static int peak_stdout_saved = -1;

static int
peak_internal_nb(int fd)
{
	int flags;

	if (fd < 0)
		return -1;
	flags = fcntl(fd, F_GETFL);
	if (flags >= 0)
		fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	flags = fcntl(fd, F_GETFD);
	if (flags >= 0)
		fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
	return fd;
}

static PeakProc
peak_internal_proc_fail(void)
{
	PeakProc p;

	p.fd = PEAK_HANDLE_INVALID;
	p.pid = 0;
	return p;
}

static int
peak_internal_memfd(void)
{
#ifdef __linux__
	return (int)syscall(SYS_memfd_create, "peak", 0);
#else
	char name[64];
	int fd;

	snprintf(name, sizeof name, "/peak.%d.%d", (int)getpid(), rand());
	fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
	if (fd >= 0)
		shm_unlink(name);
	return fd;
#endif
}

static size_t
peak_internal_io_n(size_t n)
{
	if (n > (size_t)0x40000000)
		return (size_t)0x40000000;
	return n;
}

static PeakProcRec *
peak_internal_proc_find(int fd)
{
	int i;

	if (fd < 0)
		return NULL;
	for (i = 0; i < PEAK_PROC_MAX; i++) {
		if (peak_procs[i].used && peak_procs[i].out == fd)
			return &peak_procs[i];
	}
	return NULL;
}

static PeakProcRec *
peak_internal_proc_slot(void)
{
	int i;

	for (i = 0; i < PEAK_PROC_MAX; i++) {
		if (!peak_procs[i].used)
			return &peak_procs[i];
	}
	return NULL;
}

static void
peak_internal_proc_clear(PeakProcRec *r)
{
	if (!r)
		return;
	if (r->out >= 0)
		close(r->out);
	if (r->tty >= 0 && r->tty != r->out)
		close(r->tty);
	r->out = -1;
	r->tty = -1;
	r->used = 0;
}

static void
peak_internal_proc_bind(int out, int tty)
{
	PeakProcRec *r;

	if (out < 0 || tty < 0)
		return;
	r = peak_internal_proc_find(out);
	if (!r)
		r = peak_internal_proc_slot();
	if (!r)
		return;
	r->out = out;
	r->tty = tty;
	r->used = 1;
}

static int
peak_internal_read(int fd, void *buf, size_t n)
{
	ssize_t r;

	if (fd < 0 || !buf)
		return 0;
	for (;;) {
		r = read(fd, buf, n);
		if (r > 0)
			return (int)r;
		if (r == 0)
			return 0;
		if (errno == EINTR)
			continue;
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return -1;
		return 0;
	}
}

void
peak_internal_tty_no_opost(int fd)
{
	struct termios tio;

	if (fd < 0)
		return;
	if (tcgetattr(fd, &tio) < 0)
		return;
	tio.c_oflag &= ~OPOST;
	(void)tcsetattr(fd, TCSANOW, &tio);
}

static int
peak_internal_status_code(int status)
{
	if (WIFEXITED(status))
		return WEXITSTATUS(status);
	if (WIFSIGNALED(status))
		return 128 + WTERMSIG(status);
	return 1;
}

static void
peak_internal_sigchld(int sig)
{
	int saved;
	char x;

	(void)sig;
	saved = errno;
	x = 0;
	if (peak_child_w >= 0)
		(void)write(peak_child_w, &x, 1);
	errno = saved;
}

static void
peak_internal_sigusr1(int sig)
{
	int saved;
	char x;

	(void)sig;
	saved = errno;
	x = 0;
	if (peak_usr1_w >= 0)
		(void)write(peak_usr1_w, &x, 1);
	errno = saved;
}

PeakProc
peak_pty_spawn(const char *file, const char **argv, uint32_t cols, uint32_t rows, uint32_t xpixel, uint32_t ypixel)
{
	PeakProc p;
	struct winsize ws;
	int master, slave, pid;

	if (!file || !argv)
		return peak_internal_proc_fail();
	memset(&ws, 0, sizeof ws);
	ws.ws_row = (unsigned short)rows;
	ws.ws_col = (unsigned short)cols;
	ws.ws_xpixel = (unsigned short)xpixel;
	ws.ws_ypixel = (unsigned short)ypixel;
	if (openpty(&master, &slave, NULL, NULL, &ws) < 0)
		return peak_internal_proc_fail();
	peak_internal_tty_no_opost(slave);
	pid = fork();
	if (pid < 0) {
		close(master);
		close(slave);
		return peak_internal_proc_fail();
	}
	if (pid == 0) {
		close(master);
		setsid();
		if (ioctl(slave, TIOCSCTTY, NULL) < 0)
			_Exit(1);
		dup2(slave, STDIN_FILENO);
		dup2(slave, STDOUT_FILENO);
		dup2(slave, STDERR_FILENO);
		if (slave > STDERR_FILENO)
			close(slave);
		execvp(file, (char *const *)argv);
		_Exit(127);
	}
	close(slave);
	p.fd = peak_internal_nb(master);
	p.pid = pid;
	(void)peak_pipe_set_capacity(p.fd, (size_t)1 << 20);
	return p;
}

void
peak_pty_resize(PeakProc *pty, uint32_t cols, uint32_t rows, uint32_t xpixel, uint32_t ypixel)
{
	struct winsize ws;
	PeakProcRec *r;
	int fd;

	if (!pty || pty->fd < 0)
		return;
	fd = pty->fd;
	r = peak_internal_proc_find(fd);
	if (r && r->tty >= 0)
		fd = r->tty;
	memset(&ws, 0, sizeof ws);
	ws.ws_row = (unsigned short)rows;
	ws.ws_col = (unsigned short)cols;
	ws.ws_xpixel = (unsigned short)xpixel;
	ws.ws_ypixel = (unsigned short)ypixel;
	ioctl(fd, TIOCSWINSZ, &ws);
}

int
peak_pty_reap(PeakProc *pty)
{
	int r;

	if (!pty || pty->pid <= 0)
		return 0;
	r = waitpid(pty->pid, NULL, WNOHANG);
	if (r <= 0)
		return 0;
	pty->pid = 0;
	return 1;
}

void
peak_pty_close(PeakProc *pty)
{
	PeakProcRec *r;

	if (!pty)
		return;
	if (pty->fd >= 0) {
		r = peak_internal_proc_find(pty->fd);
		if (r)
			peak_internal_proc_clear(r);
		else
			close(pty->fd);
		pty->fd = PEAK_HANDLE_INVALID;
	}
	if (pty->pid > 0) {
		waitpid(pty->pid, NULL, 0);
		pty->pid = 0;
	}
}

int
peak_wait(PeakWindow *win, const PEAK_HANDLE *fds, uint32_t n, int timeout_ms)
{
	struct pollfd pfd[PEAK_WAIT_MAX];
	PeakProcRec *rec;
	uint32_t i, np;
	int xfd;

	np = 0;
	if (n > PEAK_WAIT_MAX - 2)
		n = PEAK_WAIT_MAX - 2;
	if (win) {
		xfd = peak_window_fd(win);
		if (xfd >= 0) {
			pfd[np].fd = xfd;
			pfd[np].events = POLLIN;
			np++;
		}
		if (peak_window_pending(win) > 0)
			timeout_ms = 0;
	}
	for (i = 0; i < n; i++) {
		if (!fds || fds[i] < 0)
			continue;
		if (np >= PEAK_WAIT_MAX)
			break;
		pfd[np].fd = fds[i];
		pfd[np].events = POLLIN | POLLHUP | POLLERR;
		np++;
		rec = peak_internal_proc_find(fds[i]);
		if (rec && rec->tty >= 0 && rec->tty != fds[i] && np < PEAK_WAIT_MAX) {
			pfd[np].fd = rec->tty;
			pfd[np].events = POLLIN | POLLHUP | POLLERR;
			np++;
		}
	}
	if (!np)
		return 0;
	return poll(pfd, (nfds_t)np, timeout_ms) > 0;
}

int
peak_runtime_dir(char *buf, size_t cap, const char *app)
{
	const char *rt;
	int n;

	if (!buf || cap < 2 || !app || !app[0])
		return 0;
	rt = getenv("XDG_RUNTIME_DIR");
	if (rt && rt[0])
		n = snprintf(buf, cap, "%s/%s", rt, app);
	else
		n = snprintf(buf, cap, "/tmp/%s-%d", app, (int)getuid());
	if (n < 0 || (size_t)n >= cap)
		return 0;
	if (mkdir(buf, 0700) < 0 && errno != EEXIST)
		return 0;
	return 1;
}

PEAK_HANDLE
peak_sock_listen(const char *path)
{
	struct sockaddr_un addr;
	int fd;
	size_t n;

	if (!path || !path[0])
		return PEAK_HANDLE_INVALID;
	n = strlen(path);
	if (n >= sizeof addr.sun_path)
		return PEAK_HANDLE_INVALID;
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0)
		return PEAK_HANDLE_INVALID;
	peak_internal_nb(fd);
	memset(&addr, 0, sizeof addr);
	addr.sun_family = AF_UNIX;
	memcpy(addr.sun_path, path, n + 1);
	unlink(path);
	if (bind(fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
		close(fd);
		return PEAK_HANDLE_INVALID;
	}
	if (chmod(path, 0600) < 0 || listen(fd, 8) < 0) {
		unlink(path);
		close(fd);
		return PEAK_HANDLE_INVALID;
	}
	return fd;
}

PEAK_HANDLE
peak_sock_accept(PEAK_HANDLE listen_fd)
{
	int fd;

	if (listen_fd < 0)
		return PEAK_HANDLE_INVALID;
	for (;;) {
		fd = accept(listen_fd, NULL, NULL);
		if (fd >= 0)
			return peak_internal_nb(fd);
		if (errno == EINTR)
			continue;
		return PEAK_HANDLE_INVALID;
	}
}

PEAK_HANDLE
peak_sock_connect(const char *path)
{
	struct sockaddr_un addr;
	int fd;
	size_t n;

	if (!path || !path[0])
		return PEAK_HANDLE_INVALID;
	n = strlen(path);
	if (n >= sizeof addr.sun_path)
		return PEAK_HANDLE_INVALID;
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0)
		return PEAK_HANDLE_INVALID;
	memset(&addr, 0, sizeof addr);
	addr.sun_family = AF_UNIX;
	memcpy(addr.sun_path, path, n + 1);
	if (connect(fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
		close(fd);
		return PEAK_HANDLE_INVALID;
	}
	return peak_internal_nb(fd);
}

int
peak_sock_send(PEAK_HANDLE sock, const void *buf, size_t n, PEAK_HANDLE pass)
{
	struct msghdr msg;
	struct iovec iov;
	union {
		struct cmsghdr c;
		char b[CMSG_SPACE(sizeof(int) * 2)];
	} u;
	struct cmsghdr *c;
	PeakProcRec *rec;
	int fds[2];
	int nf;
	ssize_t r;

	if (sock < 0 || !buf || !n)
		return 0;
	memset(&msg, 0, sizeof msg);
	iov.iov_base = (void *)buf;
	iov.iov_len = n;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	if (pass != PEAK_HANDLE_INVALID) {
		nf = 1;
		fds[0] = (int)pass;
		rec = peak_internal_proc_find(pass);
		if (rec && rec->tty >= 0) {
			fds[1] = rec->tty;
			nf = 2;
		}
		memset(&u, 0, sizeof u);
		msg.msg_control = u.b;
		msg.msg_controllen = CMSG_SPACE(sizeof(int) * (size_t)nf);
		c = CMSG_FIRSTHDR(&msg);
		if (!c)
			return 0;
		c->cmsg_level = SOL_SOCKET;
		c->cmsg_type = SCM_RIGHTS;
		c->cmsg_len = CMSG_LEN(sizeof(int) * (size_t)nf);
		memcpy(CMSG_DATA(c), fds, sizeof(int) * (size_t)nf);
	}
	for (;;) {
		r = sendmsg(sock, &msg, 0);
		if (r > 0)
			return 1;
		if (r == 0)
			return 0;
		if (errno == EINTR)
			continue;
		return 0;
	}
}

int
peak_sock_recv(PEAK_HANDLE sock, void *buf, size_t n, PEAK_HANDLE *pass)
{
	struct msghdr msg;
	struct iovec iov;
	union {
		struct cmsghdr c;
		char b[CMSG_SPACE(sizeof(int) * 2)];
	} u;
	struct cmsghdr *c;
	ssize_t r;
	int fds[2];
	int nf;
	int out;
	int tty;

	if (pass)
		*pass = PEAK_HANDLE_INVALID;
	if (sock < 0 || !buf || !n)
		return -1;
	memset(&msg, 0, sizeof msg);
	memset(&u, 0, sizeof u);
	iov.iov_base = buf;
	iov.iov_len = peak_internal_io_n(n);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = u.b;
	msg.msg_controllen = sizeof u.b;
	for (;;) {
		r = recvmsg(sock, &msg, 0);
		if (r > 0)
			break;
		if (r == 0)
			return 0;
		if (errno == EINTR)
			continue;
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return -1;
		return 0;
	}
	for (c = CMSG_FIRSTHDR(&msg); c; c = CMSG_NXTHDR(&msg, c)) {
		if (c->cmsg_level != SOL_SOCKET || c->cmsg_type != SCM_RIGHTS)
			continue;
		if (c->cmsg_len < CMSG_LEN(sizeof(int)))
			continue;
		nf = c->cmsg_len >= CMSG_LEN(sizeof(int) * 2) ? 2 : 1;
		memcpy(fds, CMSG_DATA(c), sizeof(int) * (size_t)nf);
		if (fds[0] < 0)
			continue;
		out = peak_internal_nb(fds[0]);
		if (pass && *pass == PEAK_HANDLE_INVALID)
			*pass = out;
		else
			close(out);
		if (nf < 2 || fds[1] < 0)
			continue;
		tty = peak_internal_nb(fds[1]);
		if (pass && *pass == out)
			peak_internal_proc_bind(out, tty);
		else
			close(tty);
	}
	return (int)r;
}

#ifndef PEAK_HAS_POINTER_PID
int
peak_pointer_pid(PeakWindow *win)
{
	(void)win;
	return 0;
}

int
peak_pointer_local(PeakWindow *win, int *x, int *y)
{
	(void)win;
	(void)x;
	(void)y;
	return 0;
}
#endif

int
peak_filesystem_mkdir(const char *path)
{
	if (!path || !path[0])
		return 0;
	return mkdir(path, 0777) == 0;
}

int
peak_filesystem_rm(const char *path)
{
	if (!path || !path[0])
		return 0;
	if (unlink(path) == 0)
		return 1;
	if (errno == EISDIR || errno == EPERM)
		return rmdir(path) == 0;
	return 0;
}

int
peak_filesystem_cwd(char *buf, size_t cap)
{
	if (!buf || cap < 2)
		return 0;
	return getcwd(buf, cap) != NULL;
}

int
peak_filesystem_chdir(const char *path)
{
	if (!path || !path[0])
		return 0;
	return chdir(path) == 0;
}

int
peak_filesystem_rename(const char *from, const char *to)
{
	if (!from || !from[0] || !to || !to[0])
		return 0;
	return rename(from, to) == 0;
}

int
peak_pid(void)
{
	return (int)getpid();
}

int
peak_env_set(const char *name, const char *value)
{
	if (!name || !name[0])
		return 0;
	if (value)
		return setenv(name, value, 1) == 0;
	return unsetenv(name) == 0;
}

int
peak_env_get(const char *name, char *buf, size_t cap)
{
	const char *v;
	size_t n;

	if (!name || !name[0] || !buf || cap < 2)
		return 0;
	v = getenv(name);
	if (!v || !v[0])
		return 0;
	n = strlen(v);
	if (n >= cap)
		return 0;
	memcpy(buf, v, n + 1);
	return 1;
}

int
peak_filesystem_list(const char *path, int (*fn)(const char *name, void *ud), void *ud)
{
	DIR *d;
	struct dirent *e;

	if (!path || !path[0] || !fn)
		return 0;
	d = opendir(path);
	if (!d)
		return 0;
	while ((e = readdir(d))) {
		if (!e->d_name[0])
			continue;
		if (fn(e->d_name, ud) == 0)
			break;
	}
	closedir(d);
	return 1;
}

int
peak_filesystem_symlink(const char *target, const char *path)
{
	if (!target || !target[0] || !path || !path[0])
		return 0;
	return symlink(target, path) == 0;
}

int
peak_filesystem_readlink(const char *path, char *dst, size_t cap)
{
	ssize_t n;

	if (!path || !path[0] || !dst || cap < 2)
		return 0;
	n = readlink(path, dst, cap - 1);
	if (n < 0)
		return 0;
	dst[n] = 0;
	return 1;
}

int
peak_child_arm(void)
{
	int p[2];
	struct sigaction sa;

	if (peak_child_r >= 0)
		return 1;
	if (pipe(p) < 0)
		return 0;
	peak_internal_nb(p[0]);
	peak_internal_nb(p[1]);
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = peak_internal_sigchld;
	sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
	sigemptyset(&sa.sa_mask);
	if (sigaction(SIGCHLD, &sa, NULL) != 0) {
		close(p[0]);
		close(p[1]);
		return 0;
	}
	peak_child_r = p[0];
	peak_child_w = p[1];
	return 1;
}

void
peak_child_disarm(void)
{
	struct sigaction sa;

	memset(&sa, 0, sizeof sa);
	sa.sa_handler = SIG_DFL;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGCHLD, &sa, NULL);
	if (peak_child_r >= 0) {
		close(peak_child_r);
		peak_child_r = -1;
	}
	if (peak_child_w >= 0) {
		close(peak_child_w);
		peak_child_w = -1;
	}
}

PEAK_HANDLE
peak_child_fd(void)
{
	return peak_child_r >= 0 ? peak_child_r : PEAK_HANDLE_INVALID;
}

void
peak_child_ack(void)
{
	char buf[64];

	if (peak_child_r < 0)
		return;
	while (read(peak_child_r, buf, sizeof buf) > 0)
		;
}

int
peak_usr1_arm(void)
{
	int p[2];
	struct sigaction sa;

	if (peak_usr1_r >= 0)
		return 1;
	if (pipe(p) < 0)
		return 0;
	peak_internal_nb(p[0]);
	peak_internal_nb(p[1]);
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = peak_internal_sigusr1;
	sa.sa_flags = SA_RESTART;
	sigemptyset(&sa.sa_mask);
	if (sigaction(SIGUSR1, &sa, NULL) != 0) {
		close(p[0]);
		close(p[1]);
		return 0;
	}
	peak_usr1_r = p[0];
	peak_usr1_w = p[1];
	return 1;
}

void
peak_usr1_disarm(void)
{
	struct sigaction sa;

	memset(&sa, 0, sizeof sa);
	sa.sa_handler = SIG_DFL;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGUSR1, &sa, NULL);
	if (peak_usr1_r >= 0) {
		close(peak_usr1_r);
		peak_usr1_r = -1;
	}
	if (peak_usr1_w >= 0) {
		close(peak_usr1_w);
		peak_usr1_w = -1;
	}
}

PEAK_HANDLE
peak_usr1_fd(void)
{
	return peak_usr1_r >= 0 ? peak_usr1_r : PEAK_HANDLE_INVALID;
}

int
peak_usr1_ack(void)
{
	char buf[64];
	int n;
	int hit;

	if (peak_usr1_r < 0)
		return 0;
	hit = 0;
	while ((n = (int)read(peak_usr1_r, buf, sizeof buf)) > 0)
		hit = 1;
	return hit;
}

int
peak_child_reap(int *pid, int *code)
{
	int r, status;

	r = waitpid(-1, &status, WNOHANG);
	if (r <= 0)
		return 0;
	if (pid)
		*pid = r;
	if (code)
		*code = peak_internal_status_code(status);
	return 1;
}

int
peak_stdout_silence(void)
{
	int nfd;

	if (peak_stdout_saved >= 0)
		return 1;
	peak_stdout_saved = dup(STDOUT_FILENO);
	if (peak_stdout_saved < 0)
		return 0;
	nfd = open("/dev/null", O_WRONLY);
	if (nfd < 0) {
		close(peak_stdout_saved);
		peak_stdout_saved = -1;
		return 0;
	}
	if (dup2(nfd, STDOUT_FILENO) < 0) {
		close(nfd);
		close(peak_stdout_saved);
		peak_stdout_saved = -1;
		return 0;
	}
	close(nfd);
	return 1;
}

int
peak_stdout_restore(void)
{
	if (peak_stdout_saved < 0)
		return 0;
	fflush(stdout);
	dup2(peak_stdout_saved, STDOUT_FILENO);
	close(peak_stdout_saved);
	peak_stdout_saved = -1;
	return 1;
}

int
peak_fd_read(PEAK_HANDLE fd, void *buf, size_t n)
{
	PeakProcRec *r;
	int got;
	int tgot;

	if (fd < 0 || !buf)
		return 0;
	n = peak_internal_io_n(n);
	r = peak_internal_proc_find(fd);
	got = peak_internal_read(fd, buf, n);
	if (got > 0 || !r || r->tty < 0)
		return got;
	tgot = peak_internal_read(r->tty, buf, n);
	if (tgot > 0)
		return tgot;
	if (got == 0)
		return 0;
	return tgot;
}

int
peak_fd_write(PEAK_HANDLE fd, const void *buf, size_t n)
{
	PeakProcRec *rec;
	ssize_t r;

	if (fd < 0 || !buf)
		return 0;
	rec = peak_internal_proc_find(fd);
	if (rec && rec->tty >= 0)
		fd = rec->tty;
	n = peak_internal_io_n(n);
	for (;;) {
		r = write(fd, buf, n);
		if (r > 0)
			return (int)r;
		if (r == 0)
			return 0;
		if (errno == EINTR)
			continue;
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return -1;
		return 0;
	}
}

void
peak_fd_close(PEAK_HANDLE fd)
{
	PeakProcRec *r;

	if (fd < 0)
		return;
	r = peak_internal_proc_find(fd);
	if (r) {
		peak_internal_proc_clear(r);
		return;
	}
	close(fd);
}

size_t
peak_pipe_capacity(PEAK_HANDLE fd)
{
#ifdef __linux__
	int r;

	if (fd < 0)
		return 0;
	r = fcntl(fd, F_GETPIPE_SZ);
	if (r > 0)
		return (size_t)r;
#else
	(void)fd;
#endif
	return 0;
}

size_t
peak_pipe_set_capacity(PEAK_HANDLE fd, size_t n)
{
#ifdef __linux__
	int want[4];
	int i;
	int r;

	if (fd < 0)
		return 0;
	want[0] = n > (size_t)0x7fffffff ? 0x7fffffff : (int)n;
	want[1] = 1 << 20;
	want[2] = 65536;
	want[3] = 0;
	for (i = 0; i < 3; i++) {
		if (want[i] < 4096)
			continue;
		r = fcntl(fd, F_SETPIPE_SZ, want[i]);
		if (r > 0)
			return (size_t)r;
	}
#else
	(void)fd;
	(void)n;
#endif
	return 0;
}

PeakProc
peak_job_run(const char *cmd, const char *cwd)
{
	PeakProc p;
	int pipefd[2], pid;

	if (!cmd || !cmd[0])
		return peak_internal_proc_fail();
	if (pipe(pipefd) < 0)
		return peak_internal_proc_fail();
	pid = fork();
	if (pid < 0) {
		close(pipefd[0]);
		close(pipefd[1]);
		return peak_internal_proc_fail();
	}
	if (pid == 0) {
		int nullfd;

		close(pipefd[0]);
		dup2(pipefd[1], STDOUT_FILENO);
		dup2(pipefd[1], STDERR_FILENO);
		if (pipefd[1] > STDERR_FILENO)
			close(pipefd[1]);
		nullfd = open("/dev/null", O_RDONLY);
		if (nullfd >= 0) {
			dup2(nullfd, STDIN_FILENO);
			if (nullfd > STDERR_FILENO)
				close(nullfd);
		}
		if (cwd && cwd[0] && chdir(cwd) < 0) {
			/* inherit parent cwd */
		}
		execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
		_Exit(127);
	}
	close(pipefd[1]);
	(void)peak_pipe_set_capacity(pipefd[0], (size_t)1 << 20);
	p.fd = peak_internal_nb(pipefd[0]);
	p.pid = pid;
	return p;
}

int
peak_job_reap(PeakProc *job, int *code)
{
	int r, status;

	if (!job || job->pid <= 0)
		return 0;
	r = waitpid(job->pid, &status, WNOHANG);
	if (r <= 0)
		return 0;
	job->pid = 0;
	if (code)
		*code = peak_internal_status_code(status);
	return 1;
}

void
peak_job_kill(PeakProc *job)
{
	if (!job)
		return;
	if (job->fd >= 0) {
		close(job->fd);
		job->fd = PEAK_HANDLE_INVALID;
	}
	if (job->pid > 0) {
		kill(job->pid, SIGKILL);
		waitpid(job->pid, NULL, 0);
		job->pid = 0;
	}
}

int
peak_pid_cwd(int pid, char *buf, size_t cap)
{
	char path[64];
	ssize_t n;

	if (pid <= 0 || !buf || cap < 2)
		return 0;
#ifdef __linux__
	snprintf(path, sizeof path, "/proc/%d/cwd", pid);
	n = readlink(path, buf, cap - 1);
	if (n < 0)
		return 0;
	buf[n] = 0;
	return 1;
#else
	(void)path;
	(void)n;
	if (pid != (int)getpid())
		return 0;
	return getcwd(buf, cap) != NULL;
#endif
}

size_t
peak_page_size(void)
{
	long n;

	n = sysconf(_SC_PAGESIZE);
	if (n <= 0)
		return 4096;
	return (size_t)n;
}

void *
peak_mirror_map(size_t size)
{
	int fd;
	char *base;
	void *a, *b;

	if (!size || size % peak_page_size())
		return NULL;
	fd = peak_internal_memfd();
	if (fd < 0 || ftruncate(fd, (off_t)size) < 0) {
		if (fd >= 0)
			close(fd);
		return NULL;
	}
	base = mmap(NULL, size * 2, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (base == MAP_FAILED) {
		close(fd);
		return NULL;
	}
	a = mmap(base, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);
	b = mmap(base + size, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);
	close(fd);
	if (a == MAP_FAILED || b == MAP_FAILED) {
		munmap(base, size * 2);
		return NULL;
	}
	return base;
}

void
peak_mirror_unmap(void *p, size_t size)
{
	if (!p || !size)
		return;
	munmap(p, size * 2);
}
/* END p_posix.c */
#endif

/* BEGIN p_log.c */
/*
 * Logging!
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

static const char *p_prefix[P_COUNT_LOG_LEVEL] = {
    [P_LOG_LEVEL_FATAL] = "[FATAL]",
    [P_LOG_LEVEL_ERROR] = "[ERROR]",
    [P_LOG_LEVEL_WARN]  = "[WARNI]",
    [P_LOG_LEVEL_INFO]  = "[INFOR]",
    [P_LOG_LEVEL_DEBUG] = "[DEBUG]",
    [P_LOG_LEVEL_TRACE] = "[TRACE]",
};

#define PEAK_MAX_PRINTF 1024
#define PEAK_MAX_ALLOCS 512

typedef struct {
    void *ptr;
    size_t size;
    const char *file;
    const char *func;
    int line;
} PeakDebugMemoryInfo;

static uint64_t peak_alloc_count = 0;
static PeakDebugMemoryInfo peak_ptr_array[PEAK_MAX_ALLOCS];

void
peak_log_printf(PeakLogLevel level, const char *src, ...)
{
    char out[PEAK_MAX_PRINTF];
    va_list ap;
    int len;
    size_t offset = P_PREFIX_LEN + 1;

    if (level < 0 || level >= P_COUNT_LOG_LEVEL)
        level = P_LOG_LEVEL_ERROR;
    memcpy(out, p_prefix[level], P_PREFIX_LEN);
    out[P_PREFIX_LEN] = ' ';
    va_start(ap, src);
    len = vsnprintf(out + offset, PEAK_MAX_PRINTF - offset, src, ap);
    va_end(ap);
    if (len < 0) len = 0;
    if (offset + (size_t)len >= PEAK_MAX_PRINTF)
        len = (int)(PEAK_MAX_PRINTF - offset - 1);
    out[offset + (size_t)len] = '\n';
    fwrite(out, 1, offset + (size_t)len + 1, (level <= P_LOG_LEVEL_ERROR) ? stderr : stdout);
}

void *
peak_debug_malloc_impl(size_t size, const char *file, int line, const char *func)
{
    void *ptr = malloc(size);
    printf("[ALLOC] %p (%zu bytes) -> %s:%d %s()\n", ptr, size, file, line, func);

    if (ptr) {
        if (peak_alloc_count < PEAK_MAX_ALLOCS) {
            peak_ptr_array[peak_alloc_count++] = (PeakDebugMemoryInfo){
                .ptr = ptr,
                .size = size,
                .file = file,
                .func = func,
                .line = line
            };
        } else {
            fprintf(stderr, "[ERROR] Debug allocator tracking capacity (%d) exceeded!\n", PEAK_MAX_ALLOCS);
        }
    }
    return ptr;
}

void
peak_debug_free_impl(void *ptr, const char *file, int line, const char *func)
{
    printf("[FREE]  %p -> %s:%d %s()\n", ptr, file, line, func);

    if (!ptr) return;

    bool found = false;
    uint64_t index = 0;

    for (index = 0; index < peak_alloc_count; ++index) {
        if (peak_ptr_array[index].ptr == ptr) {
            found = true;
            break;
        }
    }

    if (found) {
        for (uint64_t j = index; j < peak_alloc_count - 1; ++j) {
            peak_ptr_array[j] = peak_ptr_array[j + 1];
        }
        peak_alloc_count--;
    } else {
        fprintf(stderr, "[WARNING] Attempted to free untracked/double-freed pointer %p at %s:%d %s()\n",
                ptr, file, line, func);
    }

    free(ptr);
}

void *
peak_debug_realloc_impl(void *ptr, size_t size, const char *file, int line, const char *func)
{
    if (!ptr) {
        return peak_debug_malloc_impl(size, file, line, func);
    }
    if (size == 0) {
        peak_debug_free_impl(ptr, file, line, func);
        return NULL;
    }

    uintptr_t old_addr = (uintptr_t)ptr;
    void *new_ptr = realloc(ptr, size);
    printf("[REALLOC] %p -> %p (%zu bytes) -> %s:%d %s()\n", (void *)old_addr, new_ptr, size, file, line, func);

    if (new_ptr) {
        bool found = false;
        for (uint64_t i = 0; i < peak_alloc_count; ++i) {
            if (peak_ptr_array[i].ptr == (void *)old_addr) {
                peak_ptr_array[i].ptr = new_ptr;
                peak_ptr_array[i].size = size;
                peak_ptr_array[i].file = file;
                peak_ptr_array[i].line = line;
                peak_ptr_array[i].func = func;
                found = true;
                break;
            }
        }
        if (!found) {
            if (peak_alloc_count < PEAK_MAX_ALLOCS) {
                peak_ptr_array[peak_alloc_count++] = (PeakDebugMemoryInfo){
                    .ptr = new_ptr,
                    .size = size,
                    .file = file,
                    .func = func,
                    .line = line
                };
            }
        }
    }
    return new_ptr;
}

void
peak_debug_memory_report(void)
{
    printf("\n==================== MEMORY REPORT ====================\n");
    printf("Remaining unfreed allocations: %lu\n", (unsigned long)peak_alloc_count);

    for (uint64_t i = 0; i < peak_alloc_count; ++i) {
        PeakDebugMemoryInfo *info = &peak_ptr_array[i];
        printf("[LEAK] %p (%zu bytes) allocated at %s:%d in %s()\n",
               info->ptr, info->size, info->file, info->line, info->func);
    }
    printf("=======================================================\n");
}
/* END p_log.c */

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

void
peak_window_set_title(PeakWindow *win, const char *name)
{
    if (!win || !name)
        return;
    peak_platform_window_set_title(&win->internal, name);
}

void
peak_window_set_size(PeakWindow *win, uint32_t width, uint32_t height)
{
    if (!win || !width || !height)
        return;
    peak_platform_window_set_size(&win->internal, width, height);
    peak_window_sync(win, NULL, NULL);
}

void
peak_window_fullscreen(PeakWindow *win, int on)
{
    if (!win)
        return;
    peak_platform_window_fullscreen(&win->internal, on);
}

void
peak_window_cursor(PeakWindow *win, int on)
{
    if (!win)
        return;
    peak_platform_window_cursor(&win->internal, on);
}

void
peak_window_pointer_relative(PeakWindow *win, int on)
{
    if (!win)
        return;
    peak_platform_window_pointer_relative(&win->internal, on);
}

float
peak_window_scale(PeakWindow *win)
{
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

void *
peak_aligned_alloc(size_t size, size_t alignment)
{
    void *p;

    if (!size)
        return NULL;
    if (alignment < sizeof (void *))
        alignment = sizeof (void *);
#if defined(PEAK_WIN32)
    p = _aligned_malloc(size, alignment);
#else
    if (posix_memalign(&p, alignment, size) != 0)
        return NULL;
#endif
    return p;
}

void
peak_aligned_free(void *p)
{
    if (!p)
        return;
#if defined(PEAK_WIN32)
    _aligned_free(p);
#else
    free(p);
#endif
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

int
peak_clip_set(PeakWindow *win, PeakClip which, const char *utf8, size_t n)
{
    if (which != PEAK_CLIP_CLIPBOARD && which != PEAK_CLIP_PRIMARY)
        return 0;
    if (n && !utf8)
        return 0;
    if (n > PEAK_CLIP_MAX)
        n = PEAK_CLIP_MAX;
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
peak_clip_request(PeakWindow *win, PeakClip which)
{
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
peak_clip_take(PeakWindow *win, char *dst, size_t cap, size_t *n)
{
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
peak_text_take(PeakWindow *win, char *dst, size_t cap, size_t *n)
{
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
peak_drop_drag(PeakWindow *win, const char *utf8, size_t n)
{
    if (!win || (n && !utf8))
        return 0;
    if (n > PEAK_CLIP_MAX)
        n = PEAK_CLIP_MAX;
    return peak_platform_drop_drag(&win->internal, utf8, n);
}

int
peak_drop_take(PeakWindow *win, char *dst, size_t cap, size_t *n)
{
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

#endif /* PEAK_IMPLEMENTATION && !PEAK_IMPLEMENTATION_INCLUDED */


/*
------------------------------------------------------------------------------
MIT License
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
*/
