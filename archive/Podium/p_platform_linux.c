/* linux */
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
/* x11 */
#include <X11/Xlib.h>
#include <X11/keysym.h>
/* pulse audio */
#include <pulse/pulseaudio.h>
#include <pulse/simple.h>
#include <pulse/error.h>
/* joystick */
#include <linux/joystick.h>

typedef struct {
    pa_simple     *audio_stream;
} P_LinuxCtx;

struct P_Window_Impl {
    Display *display;
    XImage  *image;
    Window window;
    GC gc;
    int screen;
    int width;
    int height;
};

P_LinuxCtx p_ctx = {0};

static inline P_KeyCode
_x11_translate_keycode(XEvent xev) 
{
    static KeySym ks;
    ks = XLookupKeysym(&xev.xkey, 0);
    switch (ks) {
        /* Arrow keys */
        case XK_Left:        return P_KEY_LEFT;
        case XK_Right:       return P_KEY_RIGHT;
        case XK_Up:          return P_KEY_UP;
        case XK_Down:        return P_KEY_DOWN;
        /* Common keys */
        case XK_space:       return P_KEY_SPACE;
        case XK_Escape:      return P_KEY_ESCAPE;
        case XK_Return:      return P_KEY_ENTER;
        case XK_A: case XK_a: return P_KEY_A;
        case XK_B: case XK_b: return P_KEY_B;
        case XK_C: case XK_c: return P_KEY_C;
        case XK_D: case XK_d: return P_KEY_D;
        case XK_E: case XK_e: return P_KEY_E;
        case XK_F: case XK_f: return P_KEY_F;
        case XK_G: case XK_g: return P_KEY_G;
        case XK_H: case XK_h: return P_KEY_H;
        case XK_I: case XK_i: return P_KEY_I;
        case XK_J: case XK_j: return P_KEY_J;
        case XK_K: case XK_k: return P_KEY_K;
        case XK_L: case XK_l: return P_KEY_L;
        case XK_M: case XK_m: return P_KEY_M;
        case XK_N: case XK_n: return P_KEY_N;
        case XK_O: case XK_o: return P_KEY_O;
        case XK_P: case XK_p: return P_KEY_P;
        case XK_Q: case XK_q: return P_KEY_Q;
        case XK_R: case XK_r: return P_KEY_R;
        case XK_S: case XK_s: return P_KEY_S;
        case XK_T: case XK_t: return P_KEY_T;
        case XK_U: case XK_u: return P_KEY_U;
        case XK_V: case XK_v: return P_KEY_V;
        case XK_W: case XK_w: return P_KEY_W;
        case XK_X: case XK_x: return P_KEY_X;
        case XK_Y: case XK_y: return P_KEY_Y;
        case XK_Z: case XK_z: return P_KEY_Z;
        default:              return P_KEY_UNKNOWN;
    }
}

POAPI u64
p_get_time(void) 
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return NANOS_PER_SEC * ts.tv_sec + ts.tv_nsec;
}

POAPI void
p_sleep_ns(i64 ns) 
{
    struct timespec ts;
    ts.tv_sec  = ns / 1000000000L;
    ts.tv_nsec = ns % 1000000000L;
    clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL);
}

POAPI void
p_stdout(void *msg, usize bytes)
{
    write(1, msg, bytes);
}

POAPI bool
p_file_exists(const char *path)
{
    return (access(path, F_OK) == 0);
}

POAPI usize
p_file_size(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0) {
        return st.st_size;
    }
    return 0;
}

PODEF void
p_file_load(const char *path, void *buf_ptr, unsigned long buf_size) 
{
    int fd = open(path, O_RDONLY);
    if (fd >= 0) {
        read(fd, buf_ptr, (size_t)buf_size);
        close(fd);
    }
}

PODEF void
p_file_write(const char *path, void *buf_ptr, unsigned long buf_size) 
{
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd >= 0) {
        write(fd, buf_ptr, (size_t)buf_size);
        close(fd);
    }
}

POAPI bool
p_window_open(P_Window *win, int width, int height, const char *title)
{
    win->width = width;
    win->height = height;

    XInitThreads();

    win->display = XOpenDisplay(NULL);
    if (win->display == NULL) {
        PERROR("Cannot open display!");
        return false;
    }

    int screen = DefaultScreen(win->display);
    win->window = XCreateSimpleWindow(
        win->display,
        RootWindow(win->display, screen),
        0, 0, width, height,
        1,
        BlackPixel(win->display, screen),
        BlackPixel(win->display, screen)
    );

    win->screen = screen;

    XStoreName(win->display, win->window, title);
    XSelectInput(win->display, win->window,
             KeyPressMask | KeyReleaseMask |
             ButtonPressMask | ButtonReleaseMask |
             PointerMotionMask | StructureNotifyMask);
    XMapWindow(win->display, win->window);

    win->gc = DefaultGC(win->display, screen);

    Atom wm_delete = XInternAtom(win->display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(win->display, win->window, &wm_delete, 1);

    return true;
}

POAPI void
p_window_close(P_Window *win)
{
    if (!win) return;

    if (win->image) {
        // XDestroyImage(win->image);
        win->image = NULL;
    }

    if (win->display) {
        Display *dpy = win->display;
        Window w = win->window;
        win->display = NULL;
        win->window = 0;

        if (w) {
            XDestroyWindow(dpy, w);
        }
        XSync(dpy, False);
    }
}

POAPI bool
p_window_is_open(P_Window *win)
{
    return win && win->display != NULL;
}

POAPI void
p_window_size(P_Window *win, int *window_width, int *window_height)
{
    if (!win) return;

    XWindowAttributes attr;
    XGetWindowAttributes(win->display, win->window, &attr);
    *window_width  = attr.width;
    *window_height = attr.height;

    win->width = attr.width;
    win->height = attr.height;
}

POAPI void
p_window_draw(P_Window *win, u32 *pixels, int width, int height)
{
    int screen = win->screen;

    win->image = XCreateImage(
            win->display,
            DefaultVisual(win->display, screen),
            DefaultDepth(win->display, screen),
            ZPixmap,
            0,
            (char*)pixels,
            width,
            height,
            32,
            width * 4
            );

    int window_width, window_height;
    p_window_size(win, &window_width, &window_height);

    XPutImage(
            win->display,
            win->window,
            win->gc,
            win->image,
            0, 0,
            0, 0,
            win->width,
            win->height);

    XFlush(win->display);
}

POAPI bool
p_window_poll_event(P_Window *win, P_Event *ev) 
{
    if (XPending(win->display)) {
        static XEvent xev;
        XNextEvent(win->display, &xev);

        switch (xev.type) {

            case KeyPress:
                ev->type    = P_EVENT_KEY_DOWN;
                ev->key.key = _x11_translate_keycode(xev);
                return true;

            case KeyRelease:
                ev->type    = P_EVENT_KEY_UP;
                ev->key.key = _x11_translate_keycode(xev);
                return true;

            case ConfigureNotify:
                ev->type          = P_EVENT_WINDOW_RESIZE;
                ev->resize.width  = xev.xconfigure.width;
                ev->resize.height = xev.xconfigure.height;
                return true;

            case ClientMessage:
                if ((Atom)xev.xclient.data.l[0] ==
                    XInternAtom(win->display, "WM_DELETE_WINDOW", False)) {
                    ev->type = P_EVENT_WINDOW_CLOSE;
                    return true;
                }
                break;

            case MotionNotify:
                ev->type             = P_EVENT_POINTER;
                ev->pointer.state    = P_POINTER_MOVED;
                ev->pointer.button   = 0;
                ev->pointer.x        = xev.xmotion.x;
                ev->pointer.y        = xev.xmotion.y;
                return true;

            case ButtonPress:
                ev->type             = P_EVENT_POINTER;
                ev->pointer.state    = P_POINTER_PRESSED;
                ev->pointer.button   = xev.xbutton.button; /* 1=LMB, 2=MMB, 3=RMB */
                ev->pointer.x        = xev.xbutton.x;
                ev->pointer.y        = xev.xbutton.y;
                return true;

            case ButtonRelease:
                ev->type             = P_EVENT_POINTER;
                ev->pointer.state    = P_POINTER_RELEASED;
                ev->pointer.button   = xev.xbutton.button;
                ev->pointer.x        = xev.xbutton.x;
                ev->pointer.y        = xev.xbutton.y;
                return true;
        }
    }

    ev->type = P_EVENT_NONE;
    return false;
}

POAPI void
p_audio_init(const int sample_rate, const int channels, const char *name, const char *desc) 
{
    pa_sample_spec ss = {
        .format   = PA_SAMPLE_S16LE,
        .rate     = sample_rate,
        .channels = channels,
    };

    int error;
    p_ctx.audio_stream = pa_simple_new(
        NULL,
        name,
        PA_STREAM_PLAYBACK,
        NULL,
        desc,
        &ss,
        NULL,
        NULL,
        &error
    );

    if (!p_ctx.audio_stream) {
        PERROR("PulseAudio init failed: %s\n", pa_strerror(error));
        exit(1);
    }
}

POAPI void
p_audio_quit() 
{
    if (p_ctx.audio_stream) {
        pa_simple_free(p_ctx.audio_stream);
        p_ctx.audio_stream = NULL;
    }
}

POAPI void
p_audio_write(const i16 *samples, usize count)
{
    if (!p_ctx.audio_stream) return;

    int error;
    if (pa_simple_write(p_ctx.audio_stream, samples, count * sizeof(i16), &error) < 0) {
        PERROR("PulseAudio write failed: %s\n", pa_strerror(error));
    }

    pa_simple_drain(p_ctx.audio_stream, &error);
}

#ifdef P_MODULE_VULKAN

#ifndef VK_USE_PLATFORM_XLIB_KHR
#define VK_USE_PLATFORM_XLIB_KHR
#endif
#include <vulkan/vulkan_xlib.h>

POAPI char**
p_vulkan_get_extensions()
{
    char **darr = NULL;
    p_darray_push(darr, "VK_KHR_surface");
    p_darray_push(darr, "VK_KHR_xcb_surface");
    p_darray_push(darr, "VK_KHR_xlib_surface");
    return darr;
}

POAPI bool
p_vulkan_create_surface(P_Window *window, VkInstance instance, const VkAllocationCallbacks* allocator, VkSurfaceKHR* out_surface)
{
    if (!instance || !window || !window->display || !window->window || !out_surface) {
        return false;
    }

    VkXlibSurfaceCreateInfoKHR create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    create_info.pNext = NULL;
    create_info.flags = 0;
    create_info.dpy = window->display;
    create_info.window = window->window;

    VkResult result = vkCreateXlibSurfaceKHR(instance, &create_info, allocator, out_surface);
    
    return (result == VK_SUCCESS);
}
#endif
