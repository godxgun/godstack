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
	struct peak_win32_win *w;
	VkWin32SurfaceCreateInfoKHR ci;
	w = intern ? intern->w : NULL;
	if (!w || !w->hwnd) return 0;
	memset(&ci, 0, sizeof ci);
	ci.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	ci.hwnd = w->hwnd;
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
