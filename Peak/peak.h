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
/* --- Start of p_win32.c --- */
#include <windows.h>

void
peak_platform_window_open()
{
}

void
peak_platform_window_close()
{
}

uint32_t*
peak_platform_window_buffer(size_t *width, size_t *height)
{
}

bool
peak_platform_epoll(PeakEvent *ev)
{
}
/* --- End of p_win32.c --- */
#elif defined(PEAK_LINUX)
/* --- Start of p_linux.c --- */
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
/* --- End of p_linux.c --- */
#elif defined(PEAK_WEB)
/* --- Start of p_emscripten.c --- */
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define PEAK_CANVAS "#PeakCanvas"

static PeakKeyCode map_key(const char *code);
static PeakKeyMod map_mod(const EmscriptenKeyboardEvent *keyEvent);
static EM_BOOL key_callback(int eventType, const EmscriptenKeyboardEvent *keyEvent, void *userData);
static EM_BOOL mouse_callback(int eventType, const EmscriptenMouseEvent *mouseEvent, void *userData);
static EM_BOOL resize_callback(int eventType, const EmscriptenUiEvent *uiEvent, void *userData);

void
peak_platform_window_open()
{
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, EM_TRUE, key_callback);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, EM_TRUE, key_callback);
    emscripten_set_mousedown_callback(PEAK_CANVAS, NULL, EM_TRUE, mouse_callback);
    emscripten_set_mouseup_callback(PEAK_CANVAS, NULL, EM_TRUE, mouse_callback);
    emscripten_set_mousemove_callback(PEAK_CANVAS, NULL, EM_TRUE, mouse_callback);
    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, EM_TRUE, resize_callback);
}

void
peak_platform_window_close()
{
}

uint32_t*
peak_platform_window_buffer(size_t *width, size_t *height)
{
}

bool
peak_platform_epoll(PeakEvent *ev)
{
    return 0;
}
/* --- End of p_emscripten.c --- */
#endif

/* --- Start of peak.c --- */
static inline void peak__put_pixel(int x, int y, uint32_t color);
static inline uint32_t peak__lerp_color(uint32_t c0, uint32_t c1, float t);
static inline float peak__edge_cross(float ax, float ay, float bx, float by, float cx, float cy);

static uint32_t *peak_window_buffer;
static size_t peak_window_width;
static size_t peak_window_height;
static int peak_clip_x;
static int peak_clip_y;
static size_t peak_clip_width;
static size_t peak_clip_height;

void
peak__put_pixel(int x, int y, uint32_t color) 
{
    if (x < peak_clip_x || x >= (int)(peak_clip_x + peak_clip_width) ||
        y < peak_clip_y || y >= (int)(peak_clip_y + peak_clip_height)) {
        return;
    }
    if (x < 0 || x >= (int)peak_window_width || y < 0 || y >= (int)peak_window_height) {
        return;
    }
    peak_window_buffer[y * peak_window_width + x] = color;
}

uint32_t
peak__lerp_color(uint32_t c0, uint32_t c1, float t) 
{
    if (t <= 0.0f) return c0;
    if (t >= 1.0f) return c1;

    uint32_t a0 = (c0 >> 24) & 0xFF, a1 = (c1 >> 24) & 0xFF;
    uint32_t r0 = (c0 >> 16) & 0xFF, r1 = (c1 >> 16) & 0xFF;
    uint32_t g0 = (c0 >> 8)  & 0xFF, g1 = (c1 >> 8)  & 0xFF;
    uint32_t b0 = c0 & 0xFF,        b1 = c1 & 0xFF;

    uint32_t a = (uint32_t)(a0 + t * ((float)a1 - (float)a0));
    uint32_t r = (uint32_t)(r0 + t * ((float)r1 - (float)r0));
    uint32_t g = (uint32_t)(g0 + t * ((float)g1 - (float)g0));
    uint32_t b = (uint32_t)(b0 + t * ((float)b1 - (float)b0));

    return (a << 24) | (r << 16) | (g << 8) | b;
}

float
peak__edge_cross(float ax, float ay, float bx, float by, float cx, float cy) 
{
    return (cx - ax) * (by - ay) - (cy - ay) * (bx - ax);
}

void
peak_init(void) 
{
    peak_platform_window_open();
    peak_window_buffer = peak_platform_window_buffer(&peak_window_width, &peak_window_height);
    peak_clip_x = 0;
    peak_clip_y = 0;
    peak_clip_width = peak_window_width;
    peak_clip_height = peak_window_height;
}

void
peak_quit(void) 
{
    peak_platform_window_close();
}

void
peak_extensions(void) 
{
    // TODO
}

bool
peak_poll_events(PeakEvent *ev)
{
    return peak_platform_epoll(ev);
}

void
peak_blit(int offset_x, int offset_y, const uint32_t *rgba, size_t width, size_t height) 
{
    for (size_t y = 0; y < height; ++y) {
        int target_y = offset_y + (int)y;
        if (target_y < peak_clip_y || target_y >= (int)(peak_clip_y + peak_clip_height) ||
                target_y < 0 || target_y >= (int)peak_window_height) continue;

        int start_x = offset_x < peak_clip_x ? peak_clip_x : offset_x;
        int end_x = (offset_x + (int)width) > (int)(peak_clip_x + peak_clip_width) 
            ? (int)(peak_clip_x + peak_clip_width) 
            : (offset_x + (int)width);

        if (start_x >= end_x) continue;

        size_t src_offset_x = (size_t)(start_x - offset_x);
        size_t copy_pixels = (size_t)(end_x - start_x);

        memcpy(&peak_window_buffer[target_y * peak_window_width + start_x], &rgba[y * width + src_offset_x], copy_pixels * sizeof(uint32_t));
    }
}

void
peak_clip(int x, int y, size_t w, size_t h) 
{
    peak_clip_x = x;
    peak_clip_y = y;
    peak_clip_width = w;
    peak_clip_height = h;
}

void
peak_clip_reset(void) 
{
    peak_clip_x = 0;
    peak_clip_y = 0;
    peak_clip_width = peak_window_width;
    peak_clip_height = peak_window_height;
}

void
peak_draw_rectangle(int x, int y, size_t w, size_t h, uint32_t color) 
{
    int start_x = (x < peak_clip_x) ? peak_clip_x : x;
    int start_y = (y < peak_clip_y) ? peak_clip_y : y;
    int end_x = (x + (int)w > (int)(peak_clip_x + peak_clip_width)) ? (int)(peak_clip_x + peak_clip_width) : x + (int)w;
    int end_y = (y + (int)h > (int)(peak_clip_y + peak_clip_height)) ? (int)(peak_clip_y + peak_clip_height) : y + (int)h;

    for (int py = start_y; py < end_y; ++py) {
        if (py < 0 || py >= (int)peak_window_height) continue;
        for (int px = start_x; px < end_x; ++px) {
            if (px < 0 || px >= (int)peak_window_width) continue;
            peak_window_buffer[py * peak_window_width + px] = color;
        }
    }
}

void
peak_draw_rectangle_gradient(int x, int y, size_t w, size_t h, uint32_t c0, uint32_t c1, uint32_t c2, uint32_t c3) 
{
    for (size_t ry = 0; ry < h; ++ry) {
        float ty = (h > 1) ? (float)ry / (float)(h - 1) : 0.0f;
        uint32_t left_color  = peak__lerp_color(c0, c3, ty);
        uint32_t right_color = peak__lerp_color(c1, c2, ty);

        for (size_t rx = 0; rx < w; ++rx) {
            float tx = (w > 1) ? (float)rx / (float)(w - 1) : 0.0f;
            uint32_t final_color = peak__lerp_color(left_color, right_color, tx);
            peak__put_pixel(x + (int)rx, y + (int)ry, final_color);
        }
    }
}

void
peak_draw_triangle(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t color) 
{
    int min_x = (x0 < x1) ? ((x0 < x2) ? x0 : x2) : ((x1 < x2) ? x1 : x2);
    int max_x = (x0 > x1) ? ((x0 > x2) ? x0 : x2) : ((x1 > x2) ? x1 : x2);
    int min_y = (y0 < y1) ? ((y0 < y2) ? y0 : y2) : ((y1 < y2) ? y1 : y2);
    int max_y = (y0 > y1) ? ((y0 > y2) ? y0 : y2) : ((y1 > y2) ? y1 : y2);

    float area = peak__edge_cross((float)x0, (float)y0, (float)x1, (float)y1, (float)x2, (float)y2);
    if (area == 0.0f) return;

    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            float w0 = peak__edge_cross((float)x1, (float)y1, (float)x2, (float)y2, (float)x, (float)y);
            float w1 = peak__edge_cross((float)x2, (float)y2, (float)x0, (float)y0, (float)x, (float)y);
            float w2 = peak__edge_cross((float)x0, (float)y0, (float)x1, (float)y1, (float)x, (float)y);

            if ((w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0)) {
                peak__put_pixel(x, y, color);
            }
        }
    }
}

void
peak_draw_triangle_gradient(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t c0, uint32_t c1, uint32_t c2) 
{
    int min_x = (x0 < x1) ? ((x0 < x2) ? x0 : x2) : ((x1 < x2) ? x1 : x2);
    int max_x = (x0 > x1) ? ((x0 > x2) ? x0 : x2) : ((x1 > x2) ? x1 : x2);
    int min_y = (y0 < y1) ? ((y0 < y2) ? y0 : y2) : ((y1 < y2) ? y1 : y2);
    int max_y = (y0 > y1) ? ((y0 > y2) ? y0 : y2) : ((y1 > y2) ? y1 : y2);

    float area = peak__edge_cross((float)x0, (float)y0, (float)x1, (float)y1, (float)x2, (float)y2);
    if (area == 0.0f) return;

    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            float w0 = peak__edge_cross((float)x1, (float)y1, (float)x2, (float)y2, (float)x, (float)y) / area;
            float w1 = peak__edge_cross((float)x2, (float)y2, (float)x0, (float)y0, (float)x, (float)y) / area;
            float w2 = peak__edge_cross((float)x0, (float)y0, (float)x1, (float)y1, (float)x, (float)y) / area;

            if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
                uint32_t col_a = peak__lerp_color(c0, c1, w1 / (w0 + w1 + 1e-6f));
                uint32_t final_color = peak__lerp_color(col_a, c2, w2);
                peak__put_pixel(x, y, final_color);
            }
        }
    }
}

void
peak_draw_line(int x0, int y0, int x1, int y1, size_t width, uint32_t color)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    int half_w = (int)width / 2;

    while (1) {
        /* Draw square footprint for line thickness */
        for (int ox = -half_w; ox <= half_w; ++ox) {
            for (int oy = -half_w; oy <= half_w; ++oy) {
                peak__put_pixel(x0 + ox, y0 + oy, color);
            }
        }
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void
peak_draw_line_gradient(int x0, int y0, int x1, int y1, size_t width, uint32_t c0, uint32_t c1) 
{
    float total_dist = sqrtf((float)((x1 - x0)*(x1 - x0) + (y1 - y0)*(y1 - y0)));
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    int orig_x0 = x0, orig_y0 = y0;
    int half_w = (int)width / 2;

    while (1) {
        float current_dist = sqrtf((float)((x0 - orig_x0)*(x0 - orig_x0) + (y0 - orig_y0)*(y0 - orig_y0)));
        float t = (total_dist > 0.0f) ? (current_dist / total_dist) : 0.0f;
        uint32_t color = peak__lerp_color(c0, c1, t);

        for (int ox = -half_w; ox <= half_w; ++ox) {
            for (int oy = -half_w; oy <= half_w; ++oy) {
                peak__put_pixel(x0 + ox, y0 + oy, color);
            }
        }

        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
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
