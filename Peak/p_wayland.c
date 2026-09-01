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
