/* Approved caller-backed host contract: public local transfers, native slot
 * exhaustion/reuse, X11 conversion/INCR, and bounded Wayland pipe reception.
 * Compile with -DPEAK_VULKAN -DPEAK_NO_AUDIO -DPEAK_NO_GAMEPAD -IPeak.
 * Default is display-free. --native opens/closes only; --x11-protocol requires
 * an isolated display because it writes the selection on that display. */
#include "peak.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t backing_calls;
static void *test_malloc(size_t n) { backing_calls++; return malloc(n); }
static void *test_calloc(size_t n, size_t s) { backing_calls++; return calloc(n, s); }
static void *test_realloc(void *p, size_t n) { backing_calls++; return realloc(p, n); }
static void test_free(void *p) { if (p) backing_calls++; free(p); }
#define malloc(n) test_malloc(n)
#define calloc(n,s) test_calloc(n,s)
#define realloc(p,n) test_realloc(p,n)
#define free(p) test_free(p)
#include "../Peak/peak.c"
#undef malloc
#undef calloc
#undef realloc
#undef free

static int fails;
static unsigned char payload[PEAK_CLIP_MAX + 32];
static char result[2 * PEAK_CLIP_MAX + 1];
static Display *retained_display;
static Window retained_root;
static unsigned int native_baseline;
static void expect(int ok, const char *what);
static void local_transfers(void);
static void native_windows(int protocol);
static void x11_protocol(PeakWindow *win);
static int fixture_request(Window owner, XSelectionRequestEvent *req);
static Bool fixture_owner_match(Display *d, XEvent *e, XPointer p);
static Window fixture_create_fail(Display *d, Window p, int x, int y, unsigned int w, unsigned int h, unsigned int border, unsigned long bp, unsigned long bg);
static void x11_queued_replacement(PeakWindow *win);
static void wayland_pipe_contract(void);
static size_t fixture_bytes;
static pid_t fixture_pid;

static uint32_t
fixture_version(struct wl_proxy *p)
{
	(void)p;
	return 1;
}

static int
fixture_display(struct wl_display *d)
{
	(void)d;
	return -1;
}

static struct wl_proxy *
fixture_send(struct wl_proxy *p, uint32_t op, const struct wl_interface *iface, uint32_t ver, uint32_t flags, union wl_argument *args)
{
	int fd = args[1].h;
	(void)p; (void)op; (void)iface; (void)ver; (void)flags;
	fixture_pid = fork();
	if (!fixture_pid) {
		size_t sent = 0;
		int i;
		signal(SIGPIPE, SIG_IGN);
		for (i = 3; i < 1024; i++) if (i != fd) close(i);
		while (sent < fixture_bytes) {
			size_t n = fixture_bytes - sent;
			ssize_t r;
			if (n > 4096) n = 4096;
			r = write(fd, payload, n);
			if (r <= 0) _exit(errno == EPIPE ? 0 : 2);
			sent += (size_t)r;
		}
		close(fd);
		_exit(0);
	}
	return NULL;
}

static void
wayland_pipe_contract(void)
{
	PeakWlApi saved = peak_wl;
	struct peak_wayland_win w = {0};
	PeakEvent ev;
	size_t n, i;
	int status;
	const size_t sizes[] = {0, 1, PEAK_TRANSFER_CAP, PEAK_TRANSFER_CAP + 32, SIZE_MAX};
	peak_wl.wl_proxy_get_version = fixture_version;
	peak_wl.wl_proxy_marshal_array_flags = fixture_send;
	peak_wl.wl_display_flush = fixture_display;
	peak_wl.wl_display_get_fd = fixture_display;
	peak_wayland.offer = (struct wl_data_offer *)(uintptr_t)1;
	peak_wayland.offer_utf8 = 1;
	for (i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
		int got;
		fixture_bytes = sizes[i];
		got = peak_wayland_clip_take_offer(PEAK_CLIP_CLIPBOARD, &w);
		if (!sizes[i]) {
			expect(!got && !peak_clip.paste_ready && !peak_q_pop(&w.q, &ev), "Wayland empty/failed transport no ready or event");
		} else {
			size_t want = sizes[i] > PEAK_TRANSFER_CAP ? PEAK_TRANSFER_CAP : sizes[i];
			expect(got && peak_q_pop(&w.q, &ev) && ev.type == PEAK_EVENT_CLIP && ev.clip.n == want, "Wayland pipe completion event and result");
			expect(peak_clip_take(peak_active, NULL, result, sizeof result, &n) && n == want && !memcmp(result, payload, n), "Wayland bounded prefix readiness");
			expect(!peak_clip_take(peak_active, NULL, result, sizeof result, &n), "Wayland consumed readiness");
		}
		waitpid(fixture_pid, &status, 0);
		expect(WIFEXITED(status) && !WEXITSTATUS(status), "Wayland sender finishes or observes early close (productive sender bounded)");
	}
	peak_wayland.offer = NULL;
	peak_wayland.offer_utf8 = 0;
	peak_wl = saved;
}

static void
expect(int ok, const char *what)
{
	printf("%s %s\n", ok ? "ok" : "FAIL", what);
	if (!ok) fails++;
}

static void
local_transfers(void)
{
	size_t n, i;
	const size_t sizes[] = {0, 1, PEAK_TRANSFER_CAP, PEAK_TRANSFER_CAP + 32};
	for (i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
		size_t want = sizes[i] > PEAK_TRANSFER_CAP ? PEAK_TRANSFER_CAP : sizes[i];
		size_t owned = sizes[i] > PEAK_OWN_CAP ? PEAK_OWN_CAP : sizes[i];
		size_t pasted = owned > PEAK_TRANSFER_CAP ? PEAK_TRANSFER_CAP : owned;
		expect(peak_clip_set(peak_active, NULL, PEAK_CLIP_CLIPBOARD, (char *)payload, sizes[i]), "local set result");
		expect(peak_clip_request(peak_active, NULL, PEAK_CLIP_CLIPBOARD), "local request result");
		n = SIZE_MAX;
		expect(peak_clip_take(peak_active, NULL, result, sizeof result, &n) && n == pasted && !memcmp(result, payload, n), "local prefix and ready");
		expect(!peak_clip_take(peak_active, NULL, result, sizeof result, &n), "local consumed readiness");
		peak_text_store((char *)payload, sizes[i]);
		peak_drop_store((char *)payload, sizes[i]);
		expect(peak_text_take(peak_active, NULL, result, sizeof result, &n) && n == want, "text bounded readiness");
		expect(peak_drop_take(peak_active, NULL, result, sizeof result, &n) && n == want, "drop bounded readiness");
		expect(!peak_text_take(peak_active, NULL, result, sizeof result, &n) && !peak_drop_take(peak_active, NULL, result, sizeof result, &n), "text/drop consumed");
	}
	expect(peak_clip_set(peak_active, NULL, PEAK_CLIP_PRIMARY, "primary", 7), "separate primary");
	expect(peak_clip_request(peak_active, NULL, PEAK_CLIP_PRIMARY) && peak_clip_take(peak_active, NULL, result, 3, &n) && n == 3 && !memcmp(result, "pri", 3), "destination truncation");
	expect(!peak_clip_set(peak_active, NULL, (PeakClip)2, "x", 1) && !peak_clip_set(peak_active, NULL, PEAK_CLIP_PRIMARY, NULL, 1), "invalid local inputs");
	{
		char *converted;
		size_t cn;
		memset(payload, 0xff, PEAK_TRANSFER_CAP);
		expect(peak_linux_latin1_utf8(payload, PEAK_TRANSFER_CAP, &converted, &cn) && cn == 2 * PEAK_TRANSFER_CAP, "worst-case Latin-1 workspace");
		peak_clip_paste_store(PEAK_CLIP_CLIPBOARD, converted, cn);
		expect(peak_clip_take(peak_active, NULL, result, sizeof result, &n) && n == PEAK_TRANSFER_CAP && (unsigned char)result[0] == 0xc3 && (unsigned char)result[n-1] == ((n & 1) ? 0xc3 : 0xbf), "convert then byte cap");
		expect(!peak_linux_latin1_utf8(payload, PEAK_TRANSFER_CAP + 1, &converted, &cn), "conversion rejects out-of-contract input without overflow");
		peak_host_transfer_free(converted);
		memset(payload, 'p', sizeof payload);
		payload[PEAK_TRANSFER_CAP - 1] = 0xff;
		expect(peak_linux_latin1_utf8(payload, PEAK_TRANSFER_CAP, &converted, &cn) && cn == PEAK_TRANSFER_CAP + 1, "conversion byte-prefix fixture");
		peak_clip_paste_store(PEAK_CLIP_CLIPBOARD, converted, cn);
		expect(peak_clip_take(peak_active, NULL, result, sizeof result, &n) && n == PEAK_TRANSFER_CAP && (unsigned char)result[n - 1] == 0xc3, "legacy byte-prefix cap may split UTF-8 (no codepoint policy change)");
		peak_host_transfer_free(converted);
		memset(payload, 'p', sizeof payload);
	}
	{
		char *a = peak_host_recv_alloc(), *b = peak_host_recv_alloc();
		expect(a && b && a != b && !peak_host_recv_alloc(), "overlapping receive capacity bounded");
		peak_host_transfer_free(a);
		expect(peak_host_recv_alloc() == a, "receive slot reuse");
		peak_host_transfer_free(a);
		peak_host_transfer_free(b);
	}
}

static Bool
fixture_owner_match(Display *d, XEvent *e, XPointer p)
{
	(void)d;
	return e->type == SelectionRequest && e->xselectionrequest.owner == *(Window *)p;
}

static Window
fixture_create_fail(Display *d, Window p, int x, int y, unsigned int w, unsigned int h, unsigned int border, unsigned long bp, unsigned long bg)
{
	(void)d; (void)p; (void)x; (void)y; (void)w; (void)h; (void)border; (void)bp; (void)bg;
	return None;
}

static int
fixture_request(Window owner, XSelectionRequestEvent *req)
{
	XEvent e;
	int i;
	for (i = 0; i < 100; i++) {
		if (peak_x11.XCheckIfEvent(peak_linux.display, &e, fixture_owner_match, (XPointer)&owner) && e.type == SelectionRequest) {
			*req = e.xselectionrequest;
			return 1;
		}
		peak_sleep_ns(1000000);
	}
	return 0;
}

static void
x11_queued_replacement(PeakWindow *win)
{
	Window owner, root, parent, *children = NULL;
	unsigned int baseline, after;
	XSelectionRequestEvent req;
	PeakEvent ev;
	int i, round;
	size_t n;
	peak_x11.XQueryTree(peak_linux.display, DefaultRootWindow(peak_linux.display), &root, &parent, &children, &baseline);
	if (children) peak_x11.XFree(children);
	owner = peak_x11.XCreateSimpleWindow(peak_linux.display, DefaultRootWindow(peak_linux.display), 0, 0, 1, 1, 0, 0, 0);
	peak_x11.XSetSelectionOwner(peak_linux.display, peak_linux.clip_clipboard, owner, CurrentTime);
	peak_x11.XSetSelectionOwner(peak_linux.display, XA_PRIMARY, owner, CurrentTime);
	for (round = 0; round < 2; round++) {
		PeakClip next = round ? PEAK_CLIP_CLIPBOARD : PEAK_CLIP_PRIMARY;
		expect(peak_clip_request(peak_active, win, PEAK_CLIP_CLIPBOARD), "queued request A accepted");
		if (!fixture_request(owner, &req)) { expect(0, "fixture sees request A"); break; }
		peak_x11.XChangeProperty(peak_linux.display, req.requestor, req.property, req.target, 8, PropModeReplace, (unsigned char *)"oldA", 4);
		peak_linux_clip_reply(&req, req.property);
		/* A reply is now queued, but not dispatched. B gets a fresh bounded
		 * requestor identity, including for repeated same-kind requests. */
		expect(peak_clip_request(peak_active, win, next) && peak_clip_req_window != req.requestor, "queued replacement B gets fresh requestor XID");
		while (peak_window_epoll(win, &ev)) expect(ev.type != PEAK_EVENT_CLIP, "canceled queued A emits no clip event");
		expect(!peak_clip.paste_ready, "canceled queued A exposes no stale bytes");
		if (!fixture_request(owner, &req)) { expect(0, "fixture sees serialized B"); break; }
		peak_x11.XChangeProperty(peak_linux.display, req.requestor, req.property, req.target, 8, PropModeReplace, (unsigned char *)"newB", 4);
		peak_linux_clip_reply(&req, req.property);
		for (i = 0; i < 100 && !peak_clip.paste_ready; i++) {
			while (peak_window_epoll(win, &ev)) if (ev.type == PEAK_EVENT_CLIP) expect(ev.clip.which == next && ev.clip.n == 4, "B completion identity matches B");
			peak_sleep_ns(1000000);
		}
		expect(peak_clip_take(peak_active, win, result, sizeof result, &n) && n == 4 && !memcmp(result, "newB", 4), "only B payload published after replacement");
	}
	/* Begin real INCR, queue an old chunk, then cancel without draining it. */
	expect(peak_clip_request(peak_active, win, PEAK_CLIP_CLIPBOARD), "active INCR fixture request");
	if (fixture_request(owner, &req)) {
		unsigned long total = PEAK_CLIP_MAX + 100;
		Window old;
		peak_x11.XChangeProperty(peak_linux.display, req.requestor, req.property, peak_linux.clip_incr, 32, PropModeReplace, (unsigned char *)&total, 1);
		peak_linux_clip_reply(&req, req.property);
		for (i = 0; i < 100 && !peak_clip_incr_on; i++) {
			while (peak_window_epoll(win, &ev)) expect(ev.type != PEAK_EVENT_CLIP, "INCR starts with no ready event");
			peak_sleep_ns(1000000);
		}
		expect(peak_clip_incr_on, "real INCR transport active");
		old = req.requestor;
		peak_x11.XChangeProperty(peak_linux.display, old, req.property, req.target, 8, PropModeReplace, (unsigned char *)"oldA", 4);
		expect(peak_clip_request(peak_active, win, PEAK_CLIP_CLIPBOARD) && peak_clip_req_window != old && !peak_clip_incr_on, "active INCR replacement immediate (no sender drain)");
		while (peak_window_epoll(win, &ev)) expect(ev.type != PEAK_EVENT_CLIP, "queued canceled INCR chunk emits no event");
		if (fixture_request(owner, &req)) {
			peak_x11.XChangeProperty(peak_linux.display, req.requestor, req.property, req.target, 8, PropModeReplace, (unsigned char *)"newB", 4);
			peak_linux_clip_reply(&req, req.property);
			for (i = 0; i < 100 && !peak_clip.paste_ready; i++) {
				while (peak_window_epoll(win, &ev)) {}
				peak_sleep_ns(1000000);
			}
			expect(peak_clip_take(peak_active, win, result, sizeof result, &n) && n == 4 && !memcmp(result, "newB", 4), "replacement after canceled INCR publishes only B");
		} else expect(0, "replacement INCR fixture receives B immediately");
	} else expect(0, "INCR fixture receives A");
	{
		PeakX11Api saved = peak_x11;
		expect(peak_clip_request(peak_active, win, PEAK_CLIP_CLIPBOARD), "request before injected create failure");
		peak_x11.XCreateSimpleWindow = fixture_create_fail;
		expect(!peak_clip_request(peak_active, win, PEAK_CLIP_CLIPBOARD) && !peak_clip_req_on && !peak_clip_req_window && !((struct peak_linux_win *)win->internal.w)->clip_window && !peak_clip.paste_ready, "replacement create failure releases old handle without stale publication");
		peak_x11 = saved;
	}
	peak_x11.XSetSelectionOwner(peak_linux.display, peak_linux.clip_clipboard, None, CurrentTime);
	peak_x11.XSetSelectionOwner(peak_linux.display, XA_PRIMARY, None, CurrentTime);
	peak_x11.XDestroyWindow(peak_linux.display, owner);
	children = NULL;
	peak_x11.XQueryTree(peak_linux.display, DefaultRootWindow(peak_linux.display), &root, &parent, &children, &after);
	if (children) peak_x11.XFree(children);
	expect(after == baseline, "request completion/replacement/failure leave no server transport windows");
}

static void
x11_protocol(PeakWindow *win)
{
	struct peak_linux_win *w = win->internal.w;
	PeakEvent ev = {0};
	XPropertyEvent pe = {0};
	unsigned long total = PEAK_CLIP_MAX + 100;
	size_t n;
	int i;

	peak_linux_clip_atoms();
	/* Real properties/protocol handler calls, isolated from desktop clipboard. */
	peak_clip_req_on = 1;
	peak_x11.XChangeProperty(peak_linux.display, w->window, peak_linux.clip_prop, XA_STRING, 8, PropModeReplace, payload, 31);
	expect(peak_linux_clip_take_prop(w->window, peak_linux.clip_prop, PEAK_CLIP_PRIMARY, &ev) && ev.type == PEAK_EVENT_CLIP && ev.clip.which == PEAK_CLIP_PRIMARY && ev.clip.n == 31, "X11 property completion event");
	expect(peak_clip_take(peak_active, win, result, sizeof result, &n) && n == 31 && !memcmp(result, payload, n), "X11 property ready/payload");
	peak_x11.XChangeProperty(peak_linux.display, w->window, peak_linux.clip_prop, peak_linux.clip_incr, 32, PropModeReplace, (unsigned char *)&total, 1);
	expect(!peak_linux_clip_take_prop(w->window, peak_linux.clip_prop, PEAK_CLIP_CLIPBOARD, &ev) && peak_clip_incr_on, "X11 INCR starts without completion");
	pe.state = PropertyNewValue;
	pe.atom = peak_linux.clip_prop;
	expect(!peak_linux_clip_property(w, &pe, &ev) && peak_clip_incr_on && !peak_clip.paste_ready, "absent INCR property is not a zero-byte terminator");
	for (i = 0; i < 17; i++) {
		peak_x11.XChangeProperty(peak_linux.display, w->window, pe.atom, peak_linux.clip_utf8, 8, PropModeReplace, payload, 65536);
		expect(!peak_linux_clip_property(w, &pe, &ev), "INCR chunk does not publish early");
	}
	expect(peak_clip_incr_n == PEAK_CLIP_MAX && peak_clip_incr_on && !peak_clip.paste_ready, "INCR discards excess but awaits terminator");
	peak_x11.XChangeProperty(peak_linux.display, w->window, pe.atom, peak_linux.clip_utf8, 8, PropModeReplace, payload, 0);
	expect(peak_linux_clip_property(w, &pe, &ev) && ev.type == PEAK_EVENT_CLIP && ev.clip.n == PEAK_CLIP_MAX && !peak_clip_incr_on && !peak_clip_req_on, "INCR completes only at terminator");
	expect(peak_clip_take(peak_active, win, result, sizeof result, &n) && n == PEAK_CLIP_MAX && !memcmp(result, payload, n), "INCR bounded prefix");
	/* Replacement cancels an active incremental request; empty owner request
	 * eventually clears request state without a false completion event. */
	peak_clip_incr_on = 1;
	peak_clip_incr_n = 100;
	expect(peak_clip_request(peak_active, win, PEAK_CLIP_PRIMARY) && !peak_clip_incr_on, "replacement cancels INCR");
	for (i = 0; i < 100 && peak_clip_req_on; i++) {
		while (peak_window_epoll(win, &ev))
			expect(ev.type != PEAK_EVENT_CLIP, "missing selection emits no completion");
		peak_sleep_ns(1000000);
	}
	expect(!peak_clip_req_on && !peak_clip.paste_ready, "failed request no stale readiness");
	expect(peak_clip_set(peak_active, win, PEAK_CLIP_CLIPBOARD, (char *)payload, PEAK_CLIP_MAX + 32) && peak_clip.own_n[0] == PEAK_CLIP_MAX, "native oversized local write preserves result and prefix cap");
	expect(peak_clip_set(peak_active, win, PEAK_CLIP_CLIPBOARD, (char *)payload, 31), "native local clipboard ownership result");
	expect(peak_clip_request(peak_active, win, PEAK_CLIP_CLIPBOARD), "native owned request result");
	for (i = 0; i < 100 && !peak_clip.paste_ready; i++) {
		while (peak_window_epoll(win, &ev)) {
			if (ev.type == PEAK_EVENT_CLIP)
				expect(ev.clip.which == PEAK_CLIP_CLIPBOARD && ev.clip.n == 31, "native owned request completion metadata");
		}
		peak_sleep_ns(1000000);
	}
	expect(peak_clip_take(peak_active, win, result, sizeof result, &n) && n == 31 && !memcmp(result, payload, n), "native owned request ready and bytes");
	x11_queued_replacement(win);
	expect(peak_clip_set(peak_active, win, PEAK_CLIP_CLIPBOARD, "oldA", 4) && peak_clip_request(peak_active, win, PEAK_CLIP_CLIPBOARD), "local-owner request A queued");
	{
		Window old = peak_clip_req_window;
		XEvent queued;
		expect(peak_clip_set(peak_active, win, PEAK_CLIP_CLIPBOARD, "newB", 4) && peak_clip_request(peak_active, win, PEAK_CLIP_CLIPBOARD) && peak_clip_req_window != old, "local-owner A to B cancellation gets fresh XID");
		expect(!peak_x11.XCheckIfEvent(peak_linux.display, &queued, peak_linux_clip_canceled_request, (XPointer)&old), "cancel barrier discards only queued local request for old XID");
	}
	for (i = 0; i < 100 && !peak_clip.paste_ready; i++) {
		while (peak_window_epoll(win, &ev)) if (ev.type == PEAK_EVENT_CLIP) expect(ev.clip.n == 4, "local-owner B completes without BadWindow or stale event");
		peak_sleep_ns(1000000);
	}
	expect(peak_clip_take(peak_active, win, result, sizeof result, &n) && n == 4 && !memcmp(result, "newB", 4), "local-owner cancellation publishes only B");
	peak_clip_incr_on = peak_clip_req_on = 1;
	peak_x11.XChangeProperty(peak_linux.display, w->window, pe.atom, peak_linux.clip_utf8, 16, PropModeReplace, payload, 1);
	expect(!peak_linux_clip_property(w, &pe, &ev) && !peak_clip_incr_on && !peak_clip_req_on && !peak_clip.paste_ready, "malformed INCR format fails without false completion");
}

static void
native_windows(int protocol)
{
	PeakWindow a, b, stale;
	PeakEvent ev;
	int i;
	/* Protocol baseline requires the connection but normal native tests exercise
	 * public lazy initialization directly at first open. */
	if (protocol) {
		expect((peak_initialized = peak_platform_init()), "native protocol fixture init");
		if (!peak_initialized) return;
	}
	if (protocol && peak_linux_kind == PEAK_LINUX_X11) {
		Window root, parent, *children = NULL;
		retained_display = peak_linux.display;
		retained_root = DefaultRootWindow(retained_display);
		peak_x11.XQueryTree(retained_display, retained_root, &root, &parent, &children, &native_baseline);
		if (children) peak_x11.XFree(children);
	}
	a = peak_window_open(peak_active, "Peak placement contract", 32, 32, 0);
	printf("native backend: %s\n", peak_linux_kind == PEAK_LINUX_WAYLAND ? "Wayland" : "X11");
	expect(a.internal.w && a.running && !a.buffer && a.width && a.height, "native Vulkan window no software buffer");
	if (!a.internal.w) return;
	b = peak_window_open(peak_active, "Peak exhausted", 32, 32, 0);
	expect(!b.internal.w, "one-window exhaustion no fallback");
	peak_window_set_size(&a, 48, 40);
	for (i = 0; i < 30; i++) {
		while (peak_window_epoll(&a, &ev)) {}
		peak_sleep_ns(1000000);
	}
	expect(a.width && a.height && !a.buffer, "native resize preserves framebuffer absence");
	if (peak_linux_kind == PEAK_LINUX_X11) {
		struct peak_linux_win *w = a.internal.w;
		XEvent drop = {0};
		size_t n = 0, path_n = PEAK_TRANSFER_CAP < 65536 ? PEAK_TRANSFER_CAP + 16 : 65536;
		size_t want = path_n > PEAK_TRANSFER_CAP ? PEAK_TRANSFER_CAP : path_n;
		memcpy(payload, "file://", 7);
		memset(payload + 7, 'd', path_n);
		peak_x11.XChangeProperty(peak_linux.display, w->window, peak_linux.xdnd_prop,
			peak_linux.uri_list, 8, PropModeReplace, payload, (int)(path_n + 7));
		drop.xselection.type = SelectionNotify;
		drop.xselection.display = peak_linux.display;
		drop.xselection.requestor = w->window;
		drop.xselection.selection = peak_linux.xdnd_selection;
		drop.xselection.property = peak_linux.xdnd_prop;
		peak_x11.XSendEvent(peak_linux.display, w->window, False, 0, &drop);
		peak_x11.XFlush(peak_linux.display);
		for (i = 0; i < 100; i++) {
			if (peak_window_epoll(&a, &ev) && ev.type == PEAK_EVENT_DROP) break;
			peak_sleep_ns(1000000);
		}
		expect(i < 100 && ev.drop.n == want &&
			peak_drop_take(peak_active, &a, result, sizeof result, &n) && n == want && result[0] == 'd' && result[n - 1] == 'd',
			"X11 drop configured prefix and notification (URI without newline)");
		memset(payload, 'p', sizeof payload);
	}
	if (protocol) {
		expect(peak_linux_kind == PEAK_LINUX_X11, "isolated X11 selected");
		if (peak_linux_kind == PEAK_LINUX_X11) x11_protocol(&a);
	}
	if (protocol && peak_linux_kind == PEAK_LINUX_X11) {
		peak_clip_request(peak_active, &a, PEAK_CLIP_CLIPBOARD);
		peak_clip_incr_on = 1;
	}
	stale = a;
	peak_window_close(&a);
	expect(!a.internal.w, "native close clears pointer");
	if (protocol) expect(!peak_clip_req_on && !peak_clip_incr_on, "close cancels placed incremental ownership before slot reuse");
	b = peak_window_open(peak_active, "Peak slot reuse", 32, 32, 0);
	expect(b.internal.w && b.running, "native slot reused");
	expect(!peak_window_epoll(&stale, &ev) && peak_window_fd(&stale) == -1, "stale slot rejected after reuse");
	peak_window_close(&stale);
	expect(peak_window_valid(&b), "stale close does not close reused window");
	peak_window_close(&b);
	if (protocol && peak_linux_kind == PEAK_LINUX_X11) {
		/* Quit must release even an unclosed application window and its
		 * outstanding transport. Root child count is checked after quit. */
		b = peak_window_open(peak_active, "Peak quit cleanup", 32, 32, 0);
		expect(b.internal.w && peak_clip_request(peak_active, &b, PEAK_CLIP_CLIPBOARD), "quit fixture leaves native and transport handles active");
	}
}

int
main(int argc, char **argv)
{
	PeakParams params = {1, PEAK_CLIP_MAX, PEAK_CLIP_MAX}, bad = {1, 0, PEAK_CLIP_MAX};
	size_t size = peak_memory(&params);
	void *raw = malloc(size + 16);
	void *buf = (void *)(((uintptr_t)raw + 15) & ~(uintptr_t)15);
	memset(payload, 'p', sizeof payload);
	if (argc > 1 && !strcmp(argv[1], "--legacy-barrier")) {
		expect(peak_init_legacy() != NULL, "explicit legacy init");
		PeakWindow legacy = peak_window_open(peak_active, "Peak legacy placement barrier", 32, 32, 0);
		memset(buf, 0x5a, size);
		expect(legacy.internal.w && peak_initialized, "implicit legacy native init recorded");
		expect(!peak_place_in_memory_and_init(buf, size, &params) && peak_active == &peak_legacy && *(unsigned char *)buf == 0x5a, "placement after implicit native init rejected without mutation");
		PeakWindow stale = legacy;
		peak_window_close(&legacy);
		expect(!peak_window_valid(&stale), "copied legacy window rejected after close");
		PeakEvent ev;
		expect(!peak_window_epoll(&stale, &ev), "stale legacy poll does not access freed allocation");
		peak_window_close(&stale);
		legacy = peak_window_open(peak_active, "Peak legacy reuse", 32, 32, 0);
		expect(legacy.internal.w && !peak_window_valid(&stale), "legacy allocation reuse preserves lifetime identity");
		stale = legacy;
		PeakCtx *old = peak_active;
		peak_quit(old);
		peak_quit(old);
		expect(!peak_window_valid(&stale), "legacy shutdown rejects copied window");
		expect(peak_init_legacy() != NULL && !peak_window_valid(&stale), "legacy reinitialization rejects old window lifetime");
		peak_window_close(&stale);
		peak_quit(peak_active);
		free(raw);
		return fails ? 1 : 0;
	}
	expect(size && peak_memory(NULL) == size && !peak_memory(&bad), "supported/default profile sizing and zero clipboard rejection");
	bad = params; bad.transfer_capacity = 0;
	expect(!peak_memory(&bad), "zero transfer capacity rejected");
	bad = params; bad.max_windows = 0;
	expect(!peak_memory(&bad), "zero windows rejected");
	bad.max_windows = 9;
	expect(!peak_memory(&bad), "unsupported window count rejected");
	expect(!peak_place_in_memory_and_init(buf, size - 1, &params) && !(peak_active != NULL), "short placement rejected before mutation");
	expect(!peak_place_in_memory_and_init((char *)buf + 1, size, &params) && !(peak_active != NULL), "misaligned placement rejected before mutation");
	expect(peak_place_in_memory_and_init(buf, size, &params) == buf, "exact placement returns backing-resident context");
	expect(!peak_place_in_memory_and_init(buf, size, &params), "duplicate placement rejected");
	local_transfers();
	wayland_pipe_contract();
	if (argc > 1 && strcmp(argv[1], "--failed-native")) native_windows(!strcmp(argv[1], "--x11-protocol"));
	peak_quit(peak_active);
	if (retained_display) {
		Window root, parent, *children = NULL;
		unsigned int count;
		peak_x11.XQueryTree(retained_display, retained_root, &root, &parent, &children, &count);
		if (children) peak_x11.XFree(children);
		expect(count == native_baseline, "close/quit leave no application or transport X11 handles");
	}
	expect(!peak_active, "quit detaches all borrowed stores");
	expect(!backing_calls, "zero direct Peak backing calls throughout placed host lifecycle");
	expect(peak_place_in_memory_and_init(buf, size, &params) != NULL, "re-placement after quit");
	peak_quit(peak_active);
	peak_quit(NULL);
	{
		PeakParams custom = {1, 23, 37}, overflow = {1, SIZE_MAX, SIZE_MAX};
		PeakCtx *ctx;
		size_t custom_size = peak_memory(&custom);
		expect(!peak_memory(&overflow), "overflow profile rejected");
		expect(custom_size && custom_size < size, "custom capacities size actual backing");
		ctx = peak_place_in_memory_and_init(buf, custom_size, &custom);
		expect(ctx == buf && !peak_initialized, "metadata placed and display setup lazy");
		local_transfers();
		wayland_pipe_contract();
		if (argc > 1 && strcmp(argv[1], "--failed-native")) native_windows(0);
		if (argc > 1 && !strcmp(argv[1], "--failed-native")) {
			PeakWindow failed;
			setenv("DISPLAY", ":59999", 1);
			setenv("WAYLAND_DISPLAY", "peak-no-such-display", 1);
			failed = peak_window_open(ctx, "unavailable", 16, 16, 0);
			expect(!failed.internal.w && !peak_initialized && peak_active == ctx, "failed lazy native start leaves context usable");
			expect(peak_clip_set(ctx, NULL, PEAK_CLIP_PRIMARY, "p", 1), "local operation after failed native start");
		}
		peak_quit(ctx);
		peak_quit(ctx);
		expect(!peak_clip_set(ctx, NULL, PEAK_CLIP_PRIMARY, "p", 1), "stale context rejected after shutdown");
		custom.clipboard_capacity = 37;
		custom.transfer_capacity = 23;
		ctx = peak_place_in_memory_and_init(buf, peak_memory(&custom), &custom);
		expect(ctx != NULL, "independent larger clipboard/smaller transfer profile");
		local_transfers();
		wayland_pipe_contract();
		peak_clip_set(ctx, NULL, PEAK_CLIP_CLIPBOARD, "clipboard", 9);
		peak_clip_set(ctx, NULL, PEAK_CLIP_PRIMARY, "primary", 7);
		peak_clip_request(ctx, NULL, PEAK_CLIP_CLIPBOARD);
		peak_text_store("text", 4);
		peak_drop_store("drop", 4);
		{
			size_t n;
			expect(peak_clip_take(ctx, NULL, result, sizeof result, &n) && n == 9 && !memcmp(result, "clipboard", 9), "paste survives simultaneous text/drop");
			expect(peak_text_take(ctx, NULL, result, sizeof result, &n) && n == 4 && !memcmp(result, "text", 4), "text independent ownership");
			expect(peak_drop_take(ctx, NULL, result, sizeof result, &n) && n == 4 && !memcmp(result, "drop", 4), "drop independent ownership");
			expect(peak_clip_request(ctx, NULL, PEAK_CLIP_PRIMARY) && peak_clip_take(ctx, NULL, result, sizeof result, &n) && n == 7 && !memcmp(result, "primary", 7), "primary retained independently");
		}
		peak_quit(ctx);
		expect(!backing_calls, "custom capacities also make zero direct backing calls");
	}
	{
		PeakParams embedded = {1, 23, 37};
		memcpy(buf, &embedded, sizeof embedded);
		expect(peak_place_in_memory_and_init(buf, size, buf) == buf, "parameters may overlap destination backing");
		expect(PEAK_OWN_CAP == 23 && PEAK_TRANSFER_CAP == 37, "overlapping parameters snapshotted before initialization");
		peak_quit(peak_active);
	}
	if (argc > 1 && !strcmp(argv[1], "--x11-protocol")) {
		PeakParams two = {2, 23, 37};
		PeakEvent ev;
		XSelectionRequestEvent req;
		Window owner;
		size_t n;
		int i, completions = 0;
		expect(peak_place_in_memory_and_init(buf, size, &two) != NULL, "two-window routing profile initialized");
		PeakWindow a = peak_window_open(peak_active, "Peak request owner", 32, 32, 0);
		PeakWindow b = peak_window_open(peak_active, "Peak nonrequest owner", 32, 32, 0);
		expect(a.internal.w && b.internal.w, "two native windows opened");
		owner = peak_x11.XCreateSimpleWindow(peak_linux.display, DefaultRootWindow(peak_linux.display), 0, 0, 1, 1, 0, 0, 0);
		peak_x11.XSetSelectionOwner(peak_linux.display, peak_linux.clip_clipboard, owner, CurrentTime);
		expect(peak_clip_request(peak_active, &a, PEAK_CLIP_CLIPBOARD), "request belongs to first window");
		if (fixture_request(owner, &req)) {
			peak_x11.XChangeProperty(peak_linux.display, req.requestor, req.property, req.target, 8, PropModeReplace, (unsigned char *)"route", 5);
			peak_linux_clip_reply(&req, req.property);
			{
				Window root, parent, *children = NULL;
				unsigned int count;
				peak_x11.XQueryTree(peak_linux.display, owner, &root, &parent, &children, &count); /* round trip queues reply */
				if (children) peak_x11.XFree(children);
			}
			while (peak_window_epoll(&b, &ev)) expect(ev.type != PEAK_EVENT_CLIP, "second window cannot consume first window completion");
			for (i = 0; i < 100 && !completions; i++) {
				while (peak_window_epoll(&a, &ev)) if (ev.type == PEAK_EVENT_CLIP) completions++;
				peak_sleep_ns(1000000);
			}
			expect(completions == 1 && peak_clip_take(peak_active, &a, result, sizeof result, &n) && n == 5 && !memcmp(result, "route", 5), "requesting window receives its own payload/event");
		} else expect(0, "routing fixture sees clipboard request");
		peak_x11.XDestroyWindow(peak_linux.display, owner);
		peak_quit(peak_active);
		expect(!backing_calls, "two-window routing also makes zero direct backing calls");
	}
	free(raw);
	return fails ? 1 : 0;
}
