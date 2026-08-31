/*
 * Wayland window, input, shm present, and WSI. libwayland-client is dlopened.
 */

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <sys/mman.h>
#include <sys/syscall.h>
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

#define PEAK_WL_API(X) \
	X(wl_display_connect, struct wl_display *, (const char *)) \
	X(wl_display_disconnect, void, (struct wl_display *)) \
	X(wl_display_dispatch, int, (struct wl_display *)) \
	X(wl_display_dispatch_pending, int, (struct wl_display *)) \
	X(wl_display_flush, int, (struct wl_display *)) \
	X(wl_display_roundtrip, int, (struct wl_display *)) \
	X(wl_display_get_fd, int, (struct wl_display *)) \
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

#include "xdg-shell-protocol.c"

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
	int scale;
	uint32_t serial;
	struct peak_wayland_win *focus;
} PeakWayland;

static PeakWlApi peak_wl;
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
static void peak_wayland_touch_down(void *data, struct wl_touch *t, uint32_t serial, uint32_t time, struct wl_surface *s, int32_t id, wl_fixed_t x, wl_fixed_t y);
static void peak_wayland_touch_up(void *data, struct wl_touch *t, uint32_t serial, uint32_t time, int32_t id);
static void peak_wayland_touch_motion(void *data, struct wl_touch *t, uint32_t time, int32_t id, wl_fixed_t x, wl_fixed_t y);
static void peak_wayland_touch_frame(void *data, struct wl_touch *t);
static void peak_wayland_touch_cancel(void *data, struct wl_touch *t);
static void peak_wayland_seat_caps(void *data, struct wl_seat *seat, uint32_t caps);
static void peak_wayland_seat_name(void *data, struct wl_seat *seat, const char *name);
static PeakKeyCode peak_wayland_key_map(uint32_t key);
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
	if (!peak_wayland_shm_resize(win, (uint32_t)w, (uint32_t)h))
		return;
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
	(void)data;
	(void)p;
	(void)serial;
	(void)s;
	if (peak_wayland.focus)
		peak_wayland.focus->pointer_in = 0;
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
	ev.pointer.type = PEAK_POINTER_LEFT;
	if (w->relative) {
		ev.pointer.x = px - w->last_x;
		ev.pointer.y = py - w->last_y;
	} else {
		ev.pointer.x = px;
		ev.pointer.y = py;
	}
	w->last_x = px;
	w->last_y = py;
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
	memset(&ev, 0, sizeof ev);
	ev.type = PEAK_EVENT_POINTER;
	ev.pointer.state = state ? PEAK_POINTER_PRESSED : PEAK_POINTER_RELEASED;
	ev.pointer.x = w->last_x;
	ev.pointer.y = w->last_y;
	if (button == BTN_RIGHT)
		ev.pointer.type = PEAK_POINTER_RIGHT;
	else if (button == BTN_MIDDLE)
		ev.pointer.type = PEAK_POINTER_MIDDLE;
	else
		ev.pointer.type = PEAK_POINTER_LEFT;
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
	peak_q_push(&w->q, ev);
}

static void
peak_wayland_keyboard_keymap(void *data, struct wl_keyboard *k, uint32_t fmt, int fd, uint32_t size)
{
	(void)data;
	(void)k;
	(void)fmt;
	(void)size;
	if (fd >= 0)
		close(fd);
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
	PeakEvent ev;

	(void)data;
	(void)k;
	(void)time;
	w = peak_wayland.focus;
	if (!w)
		return;
	peak_wayland.serial = serial;
	memset(&ev, 0, sizeof ev);
	ev.type = state ? PEAK_EVENT_KEY_DOWN : PEAK_EVENT_KEY_UP;
	ev.key.key = peak_wayland_key_map(key);
	peak_q_push(&w->q, ev);
}

static void
peak_wayland_keyboard_mod(void *data, struct wl_keyboard *k, uint32_t serial, uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group)
{
	(void)data;
	(void)k;
	(void)serial;
	(void)depressed;
	(void)latched;
	(void)locked;
	(void)group;
}

static void
peak_wayland_keyboard_repeat(void *data, struct wl_keyboard *k, int32_t rate, int32_t delay)
{
	(void)data;
	(void)k;
	(void)rate;
	(void)delay;
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

	if (peak_wayland.display)
		return 1;
	if (!peak_wayland_load())
		return 0;
	peak_wayland.display = peak_wl.wl_display_connect(NULL);
	if (!peak_wayland.display)
		return 0;
	peak_wayland.scale = 1;
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
		peak_wayland.dd = (struct wl_data_device *)peak_wayland_marshal((struct wl_proxy *)peak_wayland.ddm, 0, &wl_data_device_interface, dargs);
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
	peak_wl.wl_display_roundtrip(peak_wayland.display);
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
	if (peak_wayland.focus == w)
		peak_wayland.focus = NULL;
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

static int
peak_wayland_clip_set(PeakWindowInternal *intern, PeakClip which, const char *utf8, size_t n)
{
	(void)intern;
	(void)which;
	(void)utf8;
	(void)n;
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

static int
peak_wayland_epoll(PeakWindowInternal *intern, PeakEvent *ev)
{
	struct peak_wayland_win *w;

	w = intern ? intern->w : NULL;
	if (!w)
		return 0;
	if (peak_q_pop(&w->q, ev))
		return 1;
	if (peak_wayland.display)
		peak_wl.wl_display_dispatch_pending(peak_wayland.display);
	return peak_q_pop(&w->q, ev);
}

static int
peak_wayland_fd(PeakWindowInternal *intern)
{
	(void)intern;
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
	if (peak_wayland.display)
		peak_wl.wl_display_dispatch_pending(peak_wayland.display);
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
