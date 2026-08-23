#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

#define PEAK_X11_LINUX "libX11.so.6"

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
	Display *display;
	Atom wm_delete_window;
} PeakLinux;

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
	if (!peak_linux.display)
		return;
	peak_x11.XCloseDisplay(peak_linux.display);
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
