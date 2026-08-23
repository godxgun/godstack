#include <dlfcn.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define PEAK_X11_LINUX "/usr/lib/libX11.so.6"

#define PEAK_LINUX_DEFAULT_WIDTH  800
#define PEAK_LINUX_DEFAULT_HEIGHT 600

typedef Display*  (*XOpenDisplay_t)(const char*);
typedef int       (*XCloseDisplay_t)(Display*);
typedef Window    (*XCreateSimpleWindow_t)(Display*, Window, int, int, unsigned int, unsigned int, unsigned int, unsigned long, unsigned long);
typedef int       (*XStoreName_t)(Display*, Window, const char*);
typedef Atom      (*XInternAtom_t)(Display*, const char*, Bool);
typedef Status    (*XSetWMProtocols_t)(Display*, Window, Atom*, int);
typedef int       (*XSelectInput_t)(Display*, Window, long);
typedef GC        (*XCreateGC_t)(Display*, Window, unsigned long, XGCValues*);
typedef XImage*   (*XCreateImage_t)(Display*, Visual*, unsigned int, int, int, char*, unsigned int, unsigned int, int, int);
typedef int       (*XMapRaised_t)(Display*, Window);
typedef int       (*XFlush_t)(Display*);
typedef int       (*XFreeGC_t)(Display*, GC);
typedef int       (*XDestroyWindow_t)(Display*, Window);
typedef int       (*XPending_t)(Display*);
typedef int       (*XNextEvent_t)(Display*, XEvent*);
typedef KeySym    (*XLookupKeysym_t)(XKeyEvent*, int);
typedef int       (*XPutImage_t)(Display*, Drawable, GC, XImage*, int, int, int, int, unsigned int, unsigned int);
typedef int       (*XDefaultScreen_t)(Display*);
typedef Window    (*XRootWindow_t)(Display*, int);
typedef unsigned long (*XBlackPixel_t)(Display*, int);
typedef Visual*   (*XDefaultVisual_t)(Display*, int);
typedef int       (*XDefaultDepth_t)(Display*, int);

typedef struct {
    XOpenDisplay_t        XOpenDisplay;
    XCloseDisplay_t       XCloseDisplay;
    XCreateSimpleWindow_t XCreateSimpleWindow;
    XStoreName_t          XStoreName;
    XInternAtom_t         XInternAtom;
    XSetWMProtocols_t     XSetWMProtocols;
    XSelectInput_t        XSelectInput;
    XCreateGC_t           XCreateGC;
    XCreateImage_t        XCreateImage;
    XMapRaised_t          XMapRaised;
    XFlush_t              XFlush;
    XFreeGC_t             XFreeGC;
    XDestroyWindow_t      XDestroyWindow;
    XPending_t            XPending;
    XNextEvent_t          XNextEvent;
    XLookupKeysym_t       XLookupKeysym;
    XPutImage_t           XPutImage;
    XDefaultScreen_t      XDefaultScreen;
    XRootWindow_t         XRootWindow;
    XBlackPixel_t         XBlackPixel;
    XDefaultVisual_t      XDefaultVisual;
    XDefaultDepth_t       XDefaultDepth;
} PeakX11Api;

static PeakX11Api peak_x11;

static int peak_load_x11(void *handle) {
    peak_x11.XOpenDisplay        = (XOpenDisplay_t)dlsym(handle, "XOpenDisplay");
    peak_x11.XCloseDisplay       = (XCloseDisplay_t)dlsym(handle, "XCloseDisplay");
    peak_x11.XCreateSimpleWindow = (XCreateSimpleWindow_t)dlsym(handle, "XCreateSimpleWindow");
    peak_x11.XStoreName          = (XStoreName_t)dlsym(handle, "XStoreName");
    peak_x11.XInternAtom         = (XInternAtom_t)dlsym(handle, "XInternAtom");
    peak_x11.XSetWMProtocols     = (XSetWMProtocols_t)dlsym(handle, "XSetWMProtocols");
    peak_x11.XSelectInput        = (XSelectInput_t)dlsym(handle, "XSelectInput");
    peak_x11.XCreateGC           = (XCreateGC_t)dlsym(handle, "XCreateGC");
    peak_x11.XCreateImage        = (XCreateImage_t)dlsym(handle, "XCreateImage");
    peak_x11.XMapRaised          = (XMapRaised_t)dlsym(handle, "XMapRaised");
    peak_x11.XFlush              = (XFlush_t)dlsym(handle, "XFlush");
    peak_x11.XFreeGC             = (XFreeGC_t)dlsym(handle, "XFreeGC");
    peak_x11.XDestroyWindow      = (XDestroyWindow_t)dlsym(handle, "XDestroyWindow");
    peak_x11.XPending            = (XPending_t)dlsym(handle, "XPending");
    peak_x11.XNextEvent          = (XNextEvent_t)dlsym(handle, "XNextEvent");
    peak_x11.XLookupKeysym       = (XLookupKeysym_t)dlsym(handle, "XLookupKeysym");
    peak_x11.XPutImage           = (XPutImage_t)dlsym(handle, "XPutImage");
    peak_x11.XDefaultScreen      = (XDefaultScreen_t)dlsym(handle, "XDefaultScreen");
    peak_x11.XRootWindow         = (XRootWindow_t)dlsym(handle, "XRootWindow");
    peak_x11.XBlackPixel         = (XBlackPixel_t)dlsym(handle, "XBlackPixel");
    peak_x11.XDefaultVisual      = (XDefaultVisual_t)dlsym(handle, "XDefaultVisual");
    peak_x11.XDefaultDepth       = (XDefaultDepth_t)dlsym(handle, "XDefaultDepth");

    if (!peak_x11.XOpenDisplay || !peak_x11.XCloseDisplay || !peak_x11.XCreateSimpleWindow ||
        !peak_x11.XStoreName || !peak_x11.XInternAtom || !peak_x11.XSetWMProtocols ||
        !peak_x11.XSelectInput || !peak_x11.XCreateGC || !peak_x11.XCreateImage ||
        !peak_x11.XMapRaised || !peak_x11.XFlush || 
        !peak_x11.XFreeGC || !peak_x11.XDestroyWindow || !peak_x11.XPending ||
        !peak_x11.XNextEvent || !peak_x11.XLookupKeysym || !peak_x11.XPutImage ||
        !peak_x11.XDefaultScreen || !peak_x11.XRootWindow || !peak_x11.XBlackPixel ||
        !peak_x11.XDefaultVisual || !peak_x11.XDefaultDepth) {
        return 0;
    }
    return 1;
}


typedef struct {
    Display *display;
    Atom     wm_delete_window;
    bool     initialized;
} PeakLinux;

typedef struct {
    Window window;
    GC     gfx_ctx;    /* GRAPHICS CONTEXT, not garbage collector lol */
} PeakLinuxWindow;

static PeakLinux       peak_linux        = {0};
static PeakLinuxWindow peak_linux_window = {0};
static XImage         *peak_linux_ximage = NULL;
static uint32_t       *peak_linux_buffer = NULL;
static size_t          peak_linux_width  = PEAK_LINUX_DEFAULT_WIDTH;
static size_t          peak_linux_height = PEAK_LINUX_DEFAULT_HEIGHT;

static PeakKeyCode
peak__x11_map_key(KeySym sym)
{
    if (sym >= XK_a && sym <= XK_z) return (PeakKeyCode)(PEAK_KEY_A + (int)(sym - XK_a));
    if (sym >= XK_A && sym <= XK_Z) return (PeakKeyCode)(PEAK_KEY_A + (int)(sym - XK_A));
    switch (sym) {
        case XK_Up:     return PEAK_KEY_UP;
        case XK_Down:   return PEAK_KEY_DOWN;
        case XK_Left:   return PEAK_KEY_LEFT;
        case XK_Right:  return PEAK_KEY_RIGHT;
        case XK_space:  return PEAK_KEY_SPACE;
        case XK_Escape: return PEAK_KEY_ESCAPE;
        case XK_Return: return PEAK_KEY_ENTER;
        default:        return PEAK_KEY_UNKNOWN;
    }
}

static PeakKeyMod
peak__x11_map_mod(unsigned int state)
{
    if (state & ControlMask) return PEAK_KEYMOD_CTRL;
    if (state & Mod1Mask)    return PEAK_KEYMOD_ALT;
    if (state & ShiftMask)   return PEAK_KEYMOD_SHIFT;
    if (state & LockMask)    return PEAK_KEYMOD_CAPS;
    return (PeakKeyMod)0;
}

void
peak_platform_window_open()
{
    void *x11_handle = dlopen(PEAK_X11_LINUX, RTLD_LOCAL | RTLD_NOW);
    assert(x11_handle && "Failed to load X11 library. What system are you fucking using and abusing?");

    int success = peak_load_x11(x11_handle);
    assert(success && "Failed to open X11 display");
    
    peak_linux.display = peak_x11.XOpenDisplay(NULL);
    assert(peak_linux.display && "Failed to open X11 display");

    /* Who design this API??? */
    int screen = DefaultScreen(peak_linux.display);
    peak_linux_window.window = peak_x11.XCreateSimpleWindow(peak_linux.display, RootWindow(peak_linux.display, screen), 0, 0, peak_linux_width, peak_linux_height, 0, BlackPixel(peak_linux.display, screen), BlackPixel(peak_linux.display, screen));
    peak_x11.XStoreName(peak_linux.display, peak_linux_window.window, "Peak");
    peak_linux.wm_delete_window = peak_x11.XInternAtom(peak_linux.display, "WM_DELETE_WINDOW", False);
    peak_x11.XSetWMProtocols(peak_linux.display, peak_linux_window.window, &peak_linux.wm_delete_window, 1);
    peak_x11.XSelectInput(peak_linux.display, peak_linux_window.window, ExposureMask      | KeyPressMask    | KeyReleaseMask   | ButtonPressMask   | ButtonReleaseMask | PointerMotionMask | StructureNotifyMask);
    peak_linux_window.gfx_ctx = peak_x11.XCreateGC(peak_linux.display, peak_linux_window.window, 0, NULL);
    peak_linux_buffer = (uint32_t *)calloc(peak_linux_width * peak_linux_height, sizeof(uint32_t));
    assert(peak_linux_buffer && "Failed to allocate framebuffer");

    /* Wrap the buffer in an XImage for XPutImage blitting */
    Visual *visual = DefaultVisual(peak_linux.display, screen);
    int     depth  = DefaultDepth(peak_linux.display, screen);

    peak_linux_ximage = peak_x11.XCreateImage(peak_linux.display, visual, depth, ZPixmap, 0, (char *)peak_linux_buffer, peak_linux_width, peak_linux_height, 32, 0);
    assert(peak_linux_ximage && "Failed to create XImage");

    peak_x11.XMapRaised(peak_linux.display, peak_linux_window.window);
    peak_x11.XFlush(peak_linux.display);

    peak_linux.initialized = true;
}

void
peak_platform_window_close()
{
    if (!peak_linux.initialized) return;

    if (peak_linux_ximage) {
        peak_linux_ximage->data = NULL;
        XDestroyImage(peak_linux_ximage);
        peak_linux_ximage = NULL;
    }

    free(peak_linux_buffer);
    peak_linux_buffer = NULL;

    peak_x11.XFreeGC(peak_linux.display, peak_linux_window.gfx_ctx);
    peak_x11.XDestroyWindow(peak_linux.display, peak_linux_window.window);
    peak_x11.XCloseDisplay(peak_linux.display);

    peak_linux.initialized = false;
}

uint32_t *
peak_platform_window_buffer(size_t *width, size_t *height)
{
    *width  = peak_linux_width;
    *height = peak_linux_height;
    return peak_linux_buffer;
}

bool
peak_platform_epoll(PeakEvent *ev)
{
    if (!peak_linux.initialized) return false;

    Display *dpy = peak_linux.display;

    while (peak_x11.XPending(dpy) > 0) {
        XEvent xev;
        peak_x11.XNextEvent(dpy, &xev);

        switch (xev.type) {

        case ClientMessage:
            if ((Atom)xev.xclient.data.l[0] == peak_linux.wm_delete_window) {
                ev->type = PEAK_EVENT_WINDOW_CLOSE;
                printf("CLLOOOOOOOSEEEE");
                return true;
            }
            continue;

        case KeyPress:
        case KeyRelease: {
            KeySym sym  = peak_x11.XLookupKeysym(&xev.xkey, 0);
            ev->type    = (xev.type == KeyPress) ? PEAK_EVENT_KEY_DOWN : PEAK_EVENT_KEY_UP;
            ev->key.key = peak__x11_map_key(sym);
            ev->key.mod = peak__x11_map_mod(xev.xkey.state);
            return true;
        }

        case ButtonPress:
        case ButtonRelease: {
            ev->type          = PEAK_EVENT_POINTER;
            ev->pointer.state = (xev.type == ButtonPress) ? PEAK_POINTER_PRESSED : PEAK_POINTER_RELEASED;
            ev->pointer.x = (float)xev.xbutton.x;
            ev->pointer.y = (float)xev.xbutton.y;
            switch (xev.xbutton.button) {
                case Button1: ev->pointer.type = PEAK_POINTER_LEFT;   break;
                case Button2: ev->pointer.type = PEAK_POINTER_MIDDLE; break;
                case Button3: ev->pointer.type = PEAK_POINTER_RIGHT;  break;
                default:      ev->pointer.type = PEAK_POINTER_LEFT;   break;
            }
            return true;
        }

        case MotionNotify:
            ev->type          = PEAK_EVENT_POINTER;
            ev->pointer.state = PEAK_POINTER_MOVED;
            ev->pointer.x     = (float)xev.xmotion.x;
            ev->pointer.y     = (float)xev.xmotion.y;
            if (xev.xmotion.state & Button3Mask)
                ev->pointer.type = PEAK_POINTER_RIGHT;
            else if (xev.xmotion.state & Button2Mask)
                ev->pointer.type = PEAK_POINTER_MIDDLE;
            else
                ev->pointer.type = PEAK_POINTER_LEFT;
            return true;

        case ConfigureNotify:
            if ((size_t)xev.xconfigure.width  != peak_linux_width ||
                (size_t)xev.xconfigure.height != peak_linux_height) {
                ev->type          = PEAK_EVENT_WINDOW_RESIZE;
                ev->resize.width  = (uint32_t)xev.xconfigure.width;
                ev->resize.height = (uint32_t)xev.xconfigure.height;
                return true;
            }
            continue;

        case Expose:
            if (xev.xexpose.count == 0) {
                peak_x11.XPutImage(dpy, peak_linux_window.window, peak_linux_window.gfx_ctx, peak_linux_ximage, 0, 0, 0, 0, peak_linux_width, peak_linux_height);
            }
            continue;

        default:
            continue;
        }
    }

    peak_x11.XPutImage(dpy, peak_linux_window.window, peak_linux_window.gfx_ctx, peak_linux_ximage, 0, 0, 0, 0, peak_linux_width, peak_linux_height);
    peak_x11.XFlush(dpy);
    return false;
}
