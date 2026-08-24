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
#define PEAK_MINOR "5"
#define PEAK_PATCH "3"

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
    #if !defined(PEAK_APPLE) && !defined(_POSIX_C_SOURCE)
        #define _POSIX_C_SOURCE 200112L
    #endif
#endif

#ifdef PEAK_WEB
    #define PEAK static inline
#else
    #define PEAK extern
#endif

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
        struct { PeakKeyCode key; PeakKeyMod mod; } key;
        struct { uint32_t width, height; } resize;
        struct { PeakPointerState state; PeakPointerType type; float x, y; } pointer;
    };
} PeakEvent;

/* Implemented by the platform */
typedef struct peak_window_internal_t PeakWindowInternal;
typedef struct PeakWindow PeakWindow;  

/* Initialize the platform. */
PEAK int  peak_init(void); // Initialize platform context and load necessary DLLs.
PEAK void peak_quit(void); // Close platform context.

/* Do sexy stuff with the windows. */
PEAK PeakWindow peak_window_open(const char *name, uint32_t width, uint32_t height, uint32_t flags); // Open a window.
PEAK void       peak_window_close(PeakWindow *window); // Close a window.
PEAK void       peak_window_run(PeakWindow *win, int (*peak_tick)(PeakWindow *win, void *userdata), void *userdata); // Hijacking the main loop makes life easier on platforms like web
PEAK int        peak_window_epoll(PeakWindow *win, PeakEvent *ev); // Poll a window for events.
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

#ifdef PEAK_IMPLEMENTATION 
#undef PEAK_IMPLEMENTATION

/* NOTE(vasco): Some platforms require an event queue 
 * while others have it built in. */
#if defined(PEAK_WIN32) || defined(PEAK_WEB)
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
/* --- Start of p_win32.c --- */
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
        X(GetKeyState,        SHORT,   WINAPI, (int))

#define PEAK_GDI32_API(X) \
        X(StretchDIBits, int, WINAPI, (HDC, int, int, int, int, int, int, int, int, const void *, const BITMAPINFO *, UINT, DWORD))

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
	PeakQ q;
};

struct peak_window_internal_t {
	struct peak_win32_win *w;
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
static int peak_internal_winmm_load(void);
static void peak_internal_win32_audio_fill(int i);
static DWORD WINAPI peak_internal_win32_audio_thread(LPVOID arg);

static PeakWin32 peak_win32;
static PeakUser32Api peak_user32;
static PeakGdi32Api peak_gdi32;
static PeakWinmmApi peak_winmm;
static PeakAudio peak_audio;

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
	default:
		return PEAK_KEY_UNKNOWN;
	}
}

static PeakKeyMod
peak_internal_win32_mod_map(void)
{
	if (peak_user32.GetKeyState(VK_CONTROL) & 0x8000)
		return PEAK_KEYMOD_CTRL;
	if (peak_user32.GetKeyState(VK_MENU) & 0x8000)
		return PEAK_KEYMOD_ALT;
	if (peak_user32.GetKeyState(VK_SHIFT) & 0x8000)
		return PEAK_KEYMOD_SHIFT;
	if (peak_user32.GetKeyState(VK_CAPITAL) & 1)
		return PEAK_KEYMOD_CAPS;
	return (PeakKeyMod)0;
}

static int
peak_internal_win32_buffer(struct peak_win32_win *w, uint32_t width, uint32_t height)
{
	uint32_t *buffer;

	if (!(buffer = calloc((size_t)width * height, sizeof(*buffer))))
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
		memset(&ev, 0, sizeof(ev));
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
		memset(&ev, 0, sizeof(ev));
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
		memset(&ev, 0, sizeof(ev));
		ev.type = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) ? PEAK_EVENT_KEY_DOWN : PEAK_EVENT_KEY_UP;
		ev.key.key = peak_internal_win32_key_map(wparam);
		ev.key.mod = peak_internal_win32_mod_map();
		peak_q_push(&w->q, ev);
		return 0;
	case WM_MOUSEMOVE: /* FALLTHROUGH */
	case WM_LBUTTONDOWN: /* FALLTHROUGH */
	case WM_LBUTTONUP: /* FALLTHROUGH */
	case WM_RBUTTONDOWN: /* FALLTHROUGH */
	case WM_RBUTTONUP: /* FALLTHROUGH */
	case WM_MBUTTONDOWN: /* FALLTHROUGH */
	case WM_MBUTTONUP:
		memset(&ev, 0, sizeof(ev));
		ev.type = PEAK_EVENT_POINTER;
		ev.pointer.x = (float)(short)LOWORD(lparam);
		ev.pointer.y = (float)(short)HIWORD(lparam);
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
		peak_q_push(&w->q, ev);
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
	if (!peak_win32.class_reg) {
		memset(&wc, 0, sizeof(wc));
		wc.cbSize = sizeof(wc);
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

	(void)flags;
	if (!peak_user32.CreateWindowExA && !peak_platform_init())
		return intern;

	if (!(w = calloc(1, sizeof(*w))))
		return intern;
	if (!peak_internal_win32_buffer(w, width, height)) {
		free(w);
		return intern;
	}

	ex = 0;
	style = WS_OVERLAPPEDWINDOW;
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

	peak_user32.ShowWindow(w->hwnd, SW_SHOWNORMAL);
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

	memset(&bmi, 0, sizeof(bmi));
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = (LONG)w->width;
	bmi.bmiHeader.biHeight = -(LONG)w->height;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;
	peak_gdi32.StretchDIBits(w->hdc,
		0, 0, (int)w->width, (int)w->height,
		0, 0, (int)w->width, (int)w->height,
		w->buffer, &bmi, DIB_RGB_COLORS, SRCCOPY);
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
	peak_winmm.waveOutWrite(peak_audio.out, &peak_audio.hdr[i], sizeof(WAVEHDR));
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

static int
peak_platform_audio_start(uint32_t channels, uint32_t rate, void (*fill)(int16_t *out, size_t frames, void *userdata), void *userdata)
{
	WAVEFORMATEX fmt;
	int i;

	if (channels > 32)
		return 0;
	if (!peak_internal_winmm_load())
		return 0;

	memset(&fmt, 0, sizeof(fmt));
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
	peak_audio.bytes = (uint32_t)channels * PEAK_AUDIO_FRAMES * sizeof(int16_t);
	peak_audio.fill = fill;
	peak_audio.userdata = userdata;
	peak_audio.run = 1;

	for (i = 0; i < PEAK_AUDIO_BUFFERS; i++) {
		if (!(peak_audio.pcm[i] = calloc(1, peak_audio.bytes)))
			goto fail;
		memset(&peak_audio.hdr[i], 0, sizeof(peak_audio.hdr[i]));
		peak_audio.hdr[i].lpData = (LPSTR)peak_audio.pcm[i];
		peak_audio.hdr[i].dwBufferLength = peak_audio.bytes;
		if (peak_winmm.waveOutPrepareHeader(peak_audio.out, &peak_audio.hdr[i], sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
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
	if (ns <= 0) return;
	Sleep((DWORD)(ns / 1000000));
}

static const char **
peak_platform_vulkan_get_extensions(uint32_t *count)
{
	static const char *exts[] = {
		"VK_KHR_surface",
		"VK_KHR_win32_surface",
	};
	if (count) *count = 2;
	return exts;
}

static int
peak_platform_vulkan_create_surface(PeakWindowInternal *intern, void *instance, const void *allocator, void *out_surface)
{
#ifdef PEAK_VULKAN
	VkWin32SurfaceCreateInfoKHR ci;
	if (!intern || !intern->w || !intern->w->hwnd) return 0;
	memset(&ci, 0, sizeof ci);
	ci.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	ci.hwnd = intern->w->hwnd;
	ci.hinstance = GetModuleHandleA(NULL);
	return vkCreateWin32SurfaceKHR((VkInstance)instance, &ci, (const VkAllocationCallbacks *)allocator, (VkSurfaceKHR *)out_surface) == VK_SUCCESS;
#else
	(void)intern; (void)instance; (void)allocator; (void)out_surface;
	return 0;
#endif
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
				peak_winmm.waveOutUnprepareHeader(peak_audio.out, &peak_audio.hdr[i], sizeof(WAVEHDR));
			free(peak_audio.pcm[i]);
			peak_audio.pcm[i] = NULL;
			memset(&peak_audio.hdr[i], 0, sizeof(peak_audio.hdr[i]));
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
/* --- End of p_win32.c --- */
#elif defined(PEAK_LINUX)
/* --- Start of p_linux.c --- */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef PEAK_VULKAN
#define VK_USE_PLATFORM_XLIB_KHR
#include <vulkan/vulkan.h>
#endif

#define PEAK_X11_LINUX "libX11.so.6"
#define PEAK_PULSE_LINUX "libpulse-simple.so.0"
#define PEAK_AUDIO_FRAMES 256

#define PEAK_PULSE_API(X) \
    X(pa_simple_new,   void *, (const char *, const char *, int, const char *, const char *, const void *, const void *, const void *, int *)) \
    X(pa_simple_free,  void,   (void *)) \
    X(pa_simple_write, int,    (void *, const void *, size_t, int *))

#define PEAK_X11_API(X) \
	X(XOpenDisplay,        Display *, (const char *)) \
	X(XCloseDisplay,       int, (Display *)) \
	X(XCreateSimpleWindow, Window, (Display *, Window, int, int, unsigned int, unsigned int, unsigned int, unsigned long, unsigned long)) \
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
	X(XLookupKeysym,       KeySym, (XKeyEvent *, int)) \
	X(XPutImage,           int, (Display *, Drawable, GC, XImage *, int, int, int, int, unsigned int, unsigned int))

typedef struct {
#define X(name, ret, args) ret (*name) args;
	PEAK_X11_API(X)
#undef X
} PeakX11Api;

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

typedef struct {
	Display *display;
	Atom wm_delete_window;
} PeakLinux;

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

struct peak_window_internal_t {
	Window window;
	GC gfx_ctx;
	XImage *ximage;
	uint32_t *buffer;
	uint32_t width;
	uint32_t height;
};

static PeakLinux peak_linux;
static PeakX11Api peak_x11;
static PeakPulseApi peak_pulse;
static PeakAudio peak_audio;

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
	switch (sym) {
	case XK_Up: return PEAK_KEY_UP;
	case XK_Down: return PEAK_KEY_DOWN;
	case XK_Left: return PEAK_KEY_LEFT;
	case XK_Right: return PEAK_KEY_RIGHT;
	case XK_space: return PEAK_KEY_SPACE;
	case XK_Escape: return PEAK_KEY_ESCAPE;
	case XK_Return: return PEAK_KEY_ENTER;
	default: return PEAK_KEY_UNKNOWN;
	}
}

static PeakKeyMod
peak_internal_x11_mod_map(unsigned int state)
{
	if (state & ControlMask) return PEAK_KEYMOD_CTRL;
	if (state & Mod1Mask) return PEAK_KEYMOD_ALT;
	if (state & ShiftMask) return PEAK_KEYMOD_SHIFT;
	if (state & LockMask) return PEAK_KEYMOD_CAPS;
	return (PeakKeyMod)0;
}

static Bool
peak_internal_x11_window_match(Display *dpy, XEvent *ev, XPointer arg)
{
	(void)dpy;
	return ev->xany.window == *(Window *)arg;
}

static int
peak_linux_buffer(PeakWindowInternal *w, uint32_t width, uint32_t height)
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
		DefaultVisual(peak_linux.display, screen),
		DefaultDepth(peak_linux.display, screen),
		ZPixmap, 0, (char *)w->buffer, width, height, 32, 0);
	if (!w->ximage) {
		free(w->buffer);
		w->buffer = NULL;
		return 0;
	}
	return 1;
}

static int
peak_platform_init(void)
{
	void *handle;

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
	}
	return 1;
}

static void
peak_platform_quit(void)
{
	/* NOTE: NVIDIA's Vulkan ICD registers an XCloseDisplay hook, then
	 * vkDestroyInstance unloads the ICD. Closing afterwards is a SIGSEGV
	 * into unmapped memory. The connection is dropped on process exit. */
	if (!peak_linux.display)
		return;
	peak_linux.display = 0;
}

static PeakWindowInternal
peak_platform_window_open(const char *name, uint32_t width, uint32_t height, uint32_t flags)
{
	PeakWindowInternal w = {0};
	int screen;

	(void)flags;
	if (!peak_linux.display && !peak_platform_init())
		return w;

	screen = DefaultScreen(peak_linux.display);
	w.window = peak_x11.XCreateSimpleWindow(peak_linux.display,
		RootWindow(peak_linux.display, screen), 0, 0, width, height, 0,
		BlackPixel(peak_linux.display, screen), BlackPixel(peak_linux.display, screen));
	peak_x11.XStoreName(peak_linux.display, w.window, name);
	peak_x11.XSetWMProtocols(peak_linux.display, w.window, &peak_linux.wm_delete_window, 1);
	peak_x11.XSelectInput(peak_linux.display, w.window,
		KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask |
		PointerMotionMask | StructureNotifyMask);
	w.gfx_ctx = peak_x11.XCreateGC(peak_linux.display, w.window, 0, NULL);

	if (!peak_linux_buffer(&w, width, height)) {
		peak_x11.XFreeGC(peak_linux.display, w.gfx_ctx);
		peak_x11.XDestroyWindow(peak_linux.display, w.window);
		return (PeakWindowInternal){0};
	}

	peak_x11.XMapRaised(peak_linux.display, w.window);
	peak_x11.XFlush(peak_linux.display);
	return w;
}

static void
peak_platform_window_close(PeakWindowInternal *w)
{
	if (!w || !w->window || !peak_linux.display)
		return;
	if (w->ximage) {
		w->ximage->data = NULL;
		XDestroyImage(w->ximage);
	}
	free(w->buffer);
	if (w->gfx_ctx)
		peak_x11.XFreeGC(peak_linux.display, w->gfx_ctx);
	peak_x11.XDestroyWindow(peak_linux.display, w->window);
	*w = (PeakWindowInternal){0};
}

static uint32_t *
peak_platform_window_buffer(PeakWindowInternal *w, size_t *width, size_t *height)
{
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
peak_platform_window_present(PeakWindowInternal *w)
{
	if (!w || !w->ximage || !peak_linux.display)
		return;
	peak_x11.XPutImage(peak_linux.display, w->window, w->gfx_ctx, w->ximage,
		0, 0, 0, 0, w->width, w->height);
	peak_x11.XFlush(peak_linux.display);
}

static bool
peak_platform_epoll(PeakWindowInternal *w, PeakEvent *ev)
{
	XEvent xev;

	if (!w || !w->window || !peak_linux.display)
		return 0;

	while (peak_x11.XCheckIfEvent(peak_linux.display, &xev, peak_internal_x11_window_match, (XPointer)&w->window)) {
		switch (xev.type) {
		case ClientMessage:
			if ((Atom)xev.xclient.data.l[0] == peak_linux.wm_delete_window) {
				ev->type = PEAK_EVENT_WINDOW_CLOSE;
				return 1;
			}
			continue;
		case KeyPress:
		case KeyRelease:
			ev->type = (xev.type == KeyPress) ? PEAK_EVENT_KEY_DOWN : PEAK_EVENT_KEY_UP;
			ev->key.key = peak_internal_x11_key_map(peak_x11.XLookupKeysym(&xev.xkey, 0));
			ev->key.mod = peak_internal_x11_mod_map(xev.xkey.state);
			return 1;
		case ButtonPress:
		case ButtonRelease:
			ev->type = PEAK_EVENT_POINTER;
			ev->pointer.state = (xev.type == ButtonPress) ? PEAK_POINTER_PRESSED : PEAK_POINTER_RELEASED;
			ev->pointer.x = (float)xev.xbutton.x;
			ev->pointer.y = (float)xev.xbutton.y;
			ev->pointer.type = (xev.xbutton.button == Button2) ? PEAK_POINTER_MIDDLE :
			                   (xev.xbutton.button == Button3) ? PEAK_POINTER_RIGHT : PEAK_POINTER_LEFT;
			return 1;
		case MotionNotify:
			ev->type = PEAK_EVENT_POINTER;
			ev->pointer.state = PEAK_POINTER_MOVED;
			ev->pointer.x = (float)xev.xmotion.x;
			ev->pointer.y = (float)xev.xmotion.y;
			ev->pointer.type = (xev.xmotion.state & Button3Mask) ? PEAK_POINTER_RIGHT :
			                   (xev.xmotion.state & Button2Mask) ? PEAK_POINTER_MIDDLE : PEAK_POINTER_LEFT;
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
		default:
			continue;
		}
	}
	return 0;
}

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
	if (count) *count = 2;
	return exts;
}

static int
peak_platform_vulkan_create_surface(PeakWindowInternal *w, void *instance, const void *allocator, void *out_surface)
{
#ifdef PEAK_VULKAN
	VkXlibSurfaceCreateInfoKHR ci;
	if (!w || !peak_linux.display || !w->window) return 0;
	memset(&ci, 0, sizeof ci);
	ci.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
	ci.dpy = peak_linux.display;
	ci.window = w->window;
	return vkCreateXlibSurfaceKHR((VkInstance)instance, &ci, (const VkAllocationCallbacks *)allocator, (VkSurfaceKHR *)out_surface) == VK_SUCCESS;
#else
	(void)w; (void)instance; (void)allocator; (void)out_surface;
	return 0;
#endif
}

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
/* --- End of p_linux.c --- */
#elif defined(PEAK_WEB)
/* --- Start of p_emscripten.c --- */
#include <emscripten.h>
#include <emscripten/html5.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

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

struct peak_window_internal_t {
	struct peak_web_win *w;
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
	return PEAK_KEY_UNKNOWN;
}

static EM_BOOL
peak_web_key(int type, const EmscriptenKeyboardEvent *e, void *ud)
{
	PeakEvent ev = {0};
	ev.type = (type == EMSCRIPTEN_EVENT_KEYDOWN) ? PEAK_EVENT_KEY_DOWN : PEAK_EVENT_KEY_UP;
	ev.key.key = peak_web_key_map(e->code);
	ev.key.mod = e->ctrlKey ? PEAK_KEYMOD_CTRL : e->altKey ? PEAK_KEYMOD_ALT : e->shiftKey ? PEAK_KEYMOD_SHIFT : 0;
	peak_q_push(&((struct peak_web_win *)ud)->q, ev);
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

static bool
peak_platform_epoll(PeakWindowInternal *intern, PeakEvent *ev)
{
	struct peak_web_win *w = intern ? intern->w : NULL;
	return w ? peak_q_pop(&w->q, ev) : 0;
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
/* --- End of p_emscripten.c --- */
#endif

/* NOTE(vasco): We define PeakWindow only when
 * we know the type of peak_window_internal_t */
struct PeakWindow { 
    PeakWindowInternal internal;
    int (*tick)(struct PeakWindow *win, void *userdata);
    void *userdata;
    uint32_t *buffer;
    uint32_t width;
    uint32_t height;
    uint32_t bufsize;
    uint16_t *audio; // LRLRLR
    int running;
};


/* --- Start of p_log.c --- */
/*
 * Logging!
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

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
/* --- End of p_log.c --- */
/* --- Start of peak.c --- */
#include <stdio.h>
#include <stdlib.h>

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
/* --- End of peak.c --- */

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
