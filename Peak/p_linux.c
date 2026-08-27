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
	X(XSendEvent,          Status, (Display *, Window, Bool, long, XEvent *))

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
	Atom clip_clipboard;
	Atom clip_utf8;
	Atom clip_targets;
	Atom clip_incr;
	Atom clip_text;
	Atom clip_prop;
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
};

static PeakLinux peak_linux;
static PeakX11Api peak_x11;
static PeakPulseApi peak_pulse;
static char *peak_clip_incr;
static size_t peak_clip_incr_n;
static PeakClip peak_clip_incr_which;
static int peak_clip_incr_on;
static PeakClip peak_clip_req_which;
static int peak_clip_req_on;
static int peak_clip_req_xa;
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

	peak_x11.XMapRaised(peak_linux.display, w->window);
	peak_x11.XFlush(peak_linux.display);
	intern.w = w;
	return intern;
}

static void
peak_platform_window_close(PeakWindowInternal *intern)
{
	struct peak_linux_win *w;

	w = intern ? intern->w : NULL;
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
	if (w->colormap_owned)
		peak_x11.XFreeColormap(peak_linux.display, w->colormap);
	free(w);
	intern->w = NULL;
}

static uint32_t *
peak_platform_window_buffer(PeakWindowInternal *intern, size_t *width, size_t *height)
{
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

static int
peak_platform_clip_set(PeakWindowInternal *intern, PeakClip which, const char *utf8, size_t n)
{
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

static bool
peak_platform_epoll(PeakWindowInternal *intern, PeakEvent *ev)
{
	struct peak_linux_win *w;
	XEvent xev;

	w = intern ? intern->w : NULL;
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
			ev->pointer.x = (float)xev.xmotion.x;
			ev->pointer.y = (float)xev.xmotion.y;
			ev->pointer.mod = peak_internal_x11_mod_map(xev.xmotion.state);
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
		case SelectionRequest:
			peak_linux_clip_request_sel(&xev.xselectionrequest);
			continue;
		case SelectionClear:
			continue;
		case SelectionNotify:
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
peak_platform_vulkan_create_surface(PeakWindowInternal *intern, void *instance, const void *allocator, void *out_surface)
{
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

#include "p_posix.c"
