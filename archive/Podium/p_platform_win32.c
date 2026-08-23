#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <xinput.h> /* gamepad support */

typedef struct {
    HWAVEOUT wave_out;
    WAVEHDR wave_header;
} P_Win32Ctx;

struct P_Window_Impl {
    HWND hwnd;
    HDC hdc;
    BITMAPINFO bitmap_info;
    int width;
    int height;
    bool closed;
};

P_Win32Ctx p_ctx = {0};

static inline P_KeyCode
_win32_translate_vkey(WPARAM vk)
{
    switch (vk) {
        case VK_LEFT:   return P_KEY_LEFT;
        case VK_RIGHT:  return P_KEY_RIGHT;
        case VK_UP:     return P_KEY_UP;
        case VK_DOWN:   return P_KEY_DOWN;
        case VK_SPACE:  return P_KEY_SPACE;
        case VK_ESCAPE: return P_KEY_ESCAPE;
        case VK_RETURN: return P_KEY_ENTER;
        case 'A': return P_KEY_A;
        case 'B': return P_KEY_B;
        case 'C': return P_KEY_C;
        case 'D': return P_KEY_D;
        case 'E': return P_KEY_E;
        case 'F': return P_KEY_F;
        case 'G': return P_KEY_G;
        case 'H': return P_KEY_H;
        case 'I': return P_KEY_I;
        case 'J': return P_KEY_J;
        case 'K': return P_KEY_K;
        case 'L': return P_KEY_L;
        case 'M': return P_KEY_M;
        case 'N': return P_KEY_N;
        case 'O': return P_KEY_O;
        case 'P': return P_KEY_P;
        case 'Q': return P_KEY_Q;
        case 'R': return P_KEY_R;
        case 'S': return P_KEY_S;
        case 'T': return P_KEY_T;
        case 'U': return P_KEY_U;
        case 'V': return P_KEY_V;
        case 'W': return P_KEY_W;
        case 'X': return P_KEY_X;
        case 'Y': return P_KEY_Y;
        case 'Z': return P_KEY_Z;
        default:  return P_KEY_UNKNOWN;
    }
}

POAPI u64
p_get_time(void) 
{
    LARGE_INTEGER count, freq;
    QueryPerformanceCounter(&count);
    QueryPerformanceFrequency(&freq);
    return (u64)((count.QuadPart * 1000000000ULL) / freq.QuadPart);
}

POAPI void
p_sleep_ns(i64 ns) 
{
    /* windows sleep is millisecond-based........ */
    Sleep((DWORD)(ns / 1000000));
}

POAPI void
p_stdout(void *msg, usize bytes)
{
    HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD bytes_written;
    WriteFile(stdout_handle, msg, bytes, &bytes_written, NULL);
}

POAPI bool
p_file_exists(const char *path)
{
    DWORD attr = GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
}

POAPI usize 
p_file_size(const char *path)
{
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (GetFileAttributesExA(path, GetFileExInfoStandard, &data)) {
        return data.nFileSizeLow;
    }
    return 0;
}

PODEF void
p_file_load(const char *path, void *buf_ptr, unsigned long buf_size) 
{
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (file != INVALID_HANDLE_VALUE) {
        DWORD bytes_read;
        ReadFile(file, buf_ptr, buf_size, &bytes_read, NULL);
        CloseHandle(file);
    }
}

PODEF void
p_file_write(const char *path, void *buf_ptr, unsigned long buf_size)
{
    HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, NULL,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file != INVALID_HANDLE_VALUE) {
        DWORD bytes_written;
        WriteFile(file, buf_ptr, buf_size, &bytes_written, NULL);
        CloseHandle(file);
    }
}

static LRESULT CALLBACK 
_win32_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    P_Window *win = (P_Window*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
    switch (msg) {
        case WM_CLOSE:
            /* Handled in poll_event; mark as closed */
            if (win) win->closed = true;
            return 0;
    }
    return DefWindowProcA(hwnd, msg, wparam, lparam);
}

POAPI bool
p_window_open(P_Window *win, int width, int height, const char *title)
{
    HINSTANCE instance = GetModuleHandleA(NULL);

    WNDCLASSA wc = {0};
    wc.lpfnWndProc = _win32_wnd_proc;
    wc.hInstance = instance;
    wc.lpszClassName = "P_Window_Class";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClassA(&wc);

    win->hwnd = CreateWindowExA(0, wc.lpszClassName, title,
                               WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                               CW_USEDEFAULT, CW_USEDEFAULT, width, height,
                               NULL, NULL, instance, NULL);

    if (!win->hwnd) return false;

    SetWindowLongPtrA(win->hwnd, GWLP_USERDATA, (LONG_PTR)win);
    win->hdc    = GetDC(win->hwnd);
    win->width  = width;
    win->height = height;
    win->closed = false;

    return true;
}

POAPI void
p_window_close(P_Window *win)
{
    if (!win) return;
    if (win->hdc)  ReleaseDC(win->hwnd, win->hdc);
    if (win->hwnd) DestroyWindow(win->hwnd);
    win->hdc  = NULL;
    win->hwnd = NULL;
}

POAPI bool
p_window_is_open(P_Window *win)
{
    return win && win->hwnd != NULL;
}

POAPI void
p_window_size(P_Window *win, int *window_width, int *window_height)
{
    if (!win) return;
    RECT rect;
    GetClientRect(win->hwnd, &rect);
    *window_width  = rect.right  - rect.left;
    *window_height = rect.bottom - rect.top;
    win->width  = *window_width;
    win->height = *window_height;
}

POAPI void
p_window_draw(P_Window *win, u32 *pixels, int width, int height)
{
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize        = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth       = width;
    bmi.bmiHeader.biHeight      = -height; /* top-down */
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    StretchDIBits(win->hdc, 
                  0, 0, win->width, win->height, 
                  0, 0, width, height, 
                  pixels, &bmi, DIB_RGB_COLORS, SRCCOPY);
}

POAPI bool
p_window_poll_event(P_Window *win, P_Event *ev) 
{
    if (win->closed) {
        win->closed = false;
        ev->type = P_EVENT_WINDOW_CLOSE;
        return true;
    }

    MSG msg;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
        switch (msg.message) {

            case WM_QUIT:
                ev->type = P_EVENT_WINDOW_CLOSE;
                return true;

            /* Key press */
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
                ev->type    = P_EVENT_KEY_DOWN;
                ev->key.key = _win32_translate_vkey(msg.wParam);
                return true;

            /* Key release */
            case WM_KEYUP:
            case WM_SYSKEYUP:
                ev->type    = P_EVENT_KEY_UP;
                ev->key.key = _win32_translate_vkey(msg.wParam);
                return true;

            /* Mouse movement */
            case WM_MOUSEMOVE:
                ev->type             = P_EVENT_POINTER;
                ev->pointer.state    = P_POINTER_MOVED;
                ev->pointer.button   = 0;
                ev->pointer.x        = (u32)(msg.lParam & 0xFFFF);
                ev->pointer.y        = (u32)((msg.lParam >> 16) & 0xFFFF);
                return true;

            /* Mouse buttons */
            case WM_LBUTTONDOWN:
                ev->type             = P_EVENT_POINTER;
                ev->pointer.state    = P_POINTER_PRESSED;
                ev->pointer.button   = 1; /* 1 = LMB, matching X11 convention */
                ev->pointer.x        = (u32)(msg.lParam & 0xFFFF);
                ev->pointer.y        = (u32)((msg.lParam >> 16) & 0xFFFF);
                return true;

            case WM_LBUTTONUP:
                ev->type             = P_EVENT_POINTER;
                ev->pointer.state    = P_POINTER_RELEASED;
                ev->pointer.button   = 1;
                ev->pointer.x        = (u32)(msg.lParam & 0xFFFF);
                ev->pointer.y        = (u32)((msg.lParam >> 16) & 0xFFFF);
                return true;

            case WM_RBUTTONDOWN:
                ev->type             = P_EVENT_POINTER;
                ev->pointer.state    = P_POINTER_PRESSED;
                ev->pointer.button   = 3; /* 3 = RMB */
                ev->pointer.x        = (u32)(msg.lParam & 0xFFFF);
                ev->pointer.y        = (u32)((msg.lParam >> 16) & 0xFFFF);
                return true;

            case WM_RBUTTONUP:
                ev->type             = P_EVENT_POINTER;
                ev->pointer.state    = P_POINTER_RELEASED;
                ev->pointer.button   = 3;
                ev->pointer.x        = (u32)(msg.lParam & 0xFFFF);
                ev->pointer.y        = (u32)((msg.lParam >> 16) & 0xFFFF);
                return true;

            case WM_MBUTTONDOWN:
                ev->type             = P_EVENT_POINTER;
                ev->pointer.state    = P_POINTER_PRESSED;
                ev->pointer.button   = 2; /* 2 = MMB */
                ev->pointer.x        = (u32)(msg.lParam & 0xFFFF);
                ev->pointer.y        = (u32)((msg.lParam >> 16) & 0xFFFF);
                return true;

            case WM_MBUTTONUP:
                ev->type             = P_EVENT_POINTER;
                ev->pointer.state    = P_POINTER_RELEASED;
                ev->pointer.button   = 2;
                ev->pointer.x        = (u32)(msg.lParam & 0xFFFF);
                ev->pointer.y        = (u32)((msg.lParam >> 16) & 0xFFFF);
                return true;

            /* Window resize */
            case WM_SIZE:
                ev->type          = P_EVENT_WINDOW_RESIZE;
                ev->resize.width  = (u32)(msg.lParam & 0xFFFF);
                ev->resize.height = (u32)((msg.lParam >> 16) & 0xFFFF);
                win->width  = ev->resize.width;
                win->height = ev->resize.height;
                return true;

            default:
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
                break;
        }
    }

    ev->type = P_EVENT_NONE;
    return false;
}


POAPI void
p_audio_init(const int sample_rate, const int channels, const char *name, const char *desc) 
{
    (void)name; (void)desc; /* WinMM doesn't use a stream name */

    WAVEFORMATEX wfx = {0};
    wfx.wFormatTag      = WAVE_FORMAT_PCM;
    wfx.nChannels       = channels;
    wfx.nSamplesPerSec  = sample_rate;
    wfx.wBitsPerSample  = 16;
    wfx.nBlockAlign     = (wfx.nChannels * wfx.wBitsPerSample) / 8;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

    if (waveOutOpen(&p_ctx.wave_out, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        exit(1);
    }
}

POAPI void
p_audio_quit()
{
    if (p_ctx.wave_out) {
        waveOutReset(p_ctx.wave_out);
        waveOutClose(p_ctx.wave_out);
        p_ctx.wave_out = NULL;
    }
}

POAPI void
p_audio_write(const i16 *samples, usize count)
{
    if (!p_ctx.wave_out) return;

    WAVEHDR header = {0};
    header.lpData         = (LPSTR)samples;
    header.dwBufferLength = (DWORD)(count * sizeof(i16));
    
    waveOutPrepareHeader(p_ctx.wave_out, &header, sizeof(WAVEHDR));
    waveOutWrite(p_ctx.wave_out, &header, sizeof(WAVEHDR));
    
    /* wait for playback to finish (synchronous, mirrors pa_simple_drain) */
    while (!(header.dwFlags & WHDR_DONE)) Sleep(1);
    
    waveOutUnprepareHeader(p_ctx.wave_out, &header, sizeof(WAVEHDR));
}

#ifdef P_MODULE_VULKAN

#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <vulkan/vulkan_win32.h>

POAPI char**
p_vulkan_get_extensions()
{
    char **darr = NULL;
    p_darray_push(darr, "VK_KHR_surface");
    p_darray_push(darr, "VK_KHR_win32_surface");
    return darr;
}

POAPI bool
p_vulkan_create_surface(P_Window *window, VkInstance instance, const VkAllocationCallbacks* allocator, VkSurfaceKHR* out_surface)
{
    if (!instance || !window || !window->hwnd || !out_surface) {
        return false;
    }

    VkWin32SurfaceCreateInfoKHR create_info = {0};
    create_info.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    create_info.pNext     = NULL;
    create_info.flags     = 0;
    create_info.hwnd      = window->hwnd;
    create_info.hinstance = GetModuleHandleA(NULL);

    VkResult result = vkCreateWin32SurfaceKHR(instance, &create_info, allocator, out_surface);
    return (result == VK_SUCCESS);
}
#endif
