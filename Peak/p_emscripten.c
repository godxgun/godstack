#include <emscripten.h>
#include <emscripten/html5.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

EM_JS(void, peak_web_dom_open, (const char *id, int w, int h), {
	var name = UTF8ToString(id);
	var c = document.getElementById(name);
	if (!c) {
		c = document.createElement('canvas');
		c.id = name;
		document.body.appendChild(c);
	}
	c.width = w;
	c.height = h;
	c.tabIndex = 0;
	c.focus();
});

EM_JS(void, peak_web_dom_present, (const char *id, int w, int h, uintptr_t pixels), {
	var c = document.getElementById(UTF8ToString(id));
	if (!c) return;
	var ctx = c.getContext('2d');
	if (c.width !== w || c.height !== h) {
		c.width = w;
		c.height = h;
	}
	var img = ctx.createImageData(w, h);
	img.data.set(HEAPU8.subarray(pixels, pixels + w * h * 4));
	ctx.putImageData(img, 0, 0);
});

struct peak_web_win {
	char name[64];
	uint32_t width, height;
	PeakQ q;
	uint32_t buffer[];
};


static void
peak_web_sel(const char *name, char *out, size_t n)
{
	out[0] = '#';
	strncpy(out + 1, name, n - 2);
	out[n - 1] = 0;
}

static PeakKeyCode
peak_web_key_map(const char *code)
{
	if (code[0] == 'K' && code[1] == 'e' && code[2] == 'y' && code[3] >= 'A' && code[3] <= 'Z' && code[4] == 0)
		return (PeakKeyCode)(PEAK_KEY_A + (code[3] - 'A'));
	if (!strcmp(code, "ArrowUp")) return PEAK_KEY_UP;
	if (!strcmp(code, "ArrowDown")) return PEAK_KEY_DOWN;
	if (!strcmp(code, "ArrowLeft")) return PEAK_KEY_LEFT;
	if (!strcmp(code, "ArrowRight")) return PEAK_KEY_RIGHT;
	if (!strcmp(code, "Space")) return PEAK_KEY_SPACE;
	if (!strcmp(code, "Escape")) return PEAK_KEY_ESCAPE;
	if (!strcmp(code, "Enter")) return PEAK_KEY_ENTER;
	if (!strcmp(code, "Backspace")) return PEAK_KEY_BACKSPACE;
	if (!strcmp(code, "Tab")) return PEAK_KEY_TAB;
	if (!strcmp(code, "Delete")) return PEAK_KEY_DELETE;
	if (!strcmp(code, "Insert")) return PEAK_KEY_INSERT;
	if (!strcmp(code, "Home")) return PEAK_KEY_HOME;
	if (!strcmp(code, "End")) return PEAK_KEY_END;
	if (!strcmp(code, "PageUp")) return PEAK_KEY_PAGEUP;
	if (!strcmp(code, "PageDown")) return PEAK_KEY_PAGEDOWN;
	if (code[0] == 'F' && code[1] >= '1' && code[1] <= '9' && code[2] == 0)
		return (PeakKeyCode)(PEAK_KEY_F1 + (code[1] - '1'));
	if (!strcmp(code, "F10")) return PEAK_KEY_F10;
	if (!strcmp(code, "F11")) return PEAK_KEY_F11;
	if (!strcmp(code, "F12")) return PEAK_KEY_F12;
	if (code[0] == 'D' && code[1] == 'i' && code[2] == 'g' && code[3] == 'i' && code[4] == 't' &&
	    code[5] >= '0' && code[5] <= '9' && code[6] == 0)
		return (PeakKeyCode)(PEAK_KEY_0 + (code[5] - '0'));
	return PEAK_KEY_UNKNOWN;
}

static EM_BOOL
peak_web_key(int type, const EmscriptenKeyboardEvent *e, void *ud)
{
	PeakEvent ev = {0};
	ev.type = (type == EMSCRIPTEN_EVENT_KEYDOWN) ? PEAK_EVENT_KEY_DOWN : PEAK_EVENT_KEY_UP;
	ev.key.key = peak_web_key_map(e->code);
	ev.key.mod = (e->shiftKey ? PEAK_KEYMOD_SHIFT : 0) | (e->ctrlKey ? PEAK_KEYMOD_CTRL : 0) | (e->altKey ? PEAK_KEYMOD_ALT : 0) | (e->metaKey ? PEAK_KEYMOD_SUPER : 0);
	peak_q_push(&((struct peak_web_win *)ud)->q, ev);
	if (type == EMSCRIPTEN_EVENT_KEYDOWN && e->key[0] && (unsigned char)e->key[0] >= 32 && e->key[1] == 0) {
		PeakEvent tev = {0};
		peak_text_store(e->key, 1);
		tev.type = PEAK_EVENT_TEXT;
		tev.text.n = 1;
		peak_q_push(&((struct peak_web_win *)ud)->q, tev);
	}
	return EM_TRUE;
}

static EM_BOOL
peak_web_mouse(int type, const EmscriptenMouseEvent *e, void *ud)
{
	PeakEvent ev = {0};
	ev.type = PEAK_EVENT_POINTER;
	ev.pointer.x = (float)e->targetX;
	ev.pointer.y = (float)e->targetY;
	if (type == EMSCRIPTEN_EVENT_MOUSEDOWN)
		ev.pointer.state = PEAK_POINTER_PRESSED;
	else if (type == EMSCRIPTEN_EVENT_MOUSEUP)
		ev.pointer.state = PEAK_POINTER_RELEASED;
	else
		ev.pointer.state = PEAK_POINTER_MOVED;
	if (type == EMSCRIPTEN_EVENT_MOUSEMOVE) {
		ev.pointer.type = (e->buttons & 4) ? PEAK_POINTER_MIDDLE :
		                  (e->buttons & 2) ? PEAK_POINTER_RIGHT : PEAK_POINTER_LEFT;
	} else {
		ev.pointer.type = (e->button == 1) ? PEAK_POINTER_MIDDLE :
		                  (e->button == 2) ? PEAK_POINTER_RIGHT : PEAK_POINTER_LEFT;
	}
	ev.pointer.mod = (e->shiftKey ? PEAK_KEYMOD_SHIFT : 0) | (e->ctrlKey ? PEAK_KEYMOD_CTRL : 0) | (e->altKey ? PEAK_KEYMOD_ALT : 0);
	peak_q_push(&((struct peak_web_win *)ud)->q, ev);
	return EM_TRUE;
}

static void
peak_web_listen(struct peak_web_win *w, int on)
{
	char sel[66];
	peak_web_sel(w->name, sel, sizeof sel);
	emscripten_set_keydown_callback(sel, w, EM_TRUE, on ? peak_web_key : NULL);
	emscripten_set_keyup_callback(sel, w, EM_TRUE, on ? peak_web_key : NULL);
	emscripten_set_mousedown_callback(sel, w, EM_TRUE, on ? peak_web_mouse : NULL);
	emscripten_set_mouseup_callback(sel, w, EM_TRUE, on ? peak_web_mouse : NULL);
	emscripten_set_mousemove_callback(sel, w, EM_TRUE, on ? peak_web_mouse : NULL);
}

static int
peak_platform_init(void)
{
	return 1;
}

static void
peak_platform_quit(void)
{
}

static PeakWindowInternal
peak_platform_window_open(const char *name, uint32_t width, uint32_t height, uint32_t flags)
{
	PeakWindowInternal intern = {0};
	struct peak_web_win *w;

	(void)flags;

	w = calloc(1, sizeof *w + (size_t)width * height * sizeof *w->buffer);
	if (!w)
		return intern;
	strncpy(w->name, name, sizeof w->name - 1);
	w->width = width;
	w->height = height;

	peak_web_dom_open(w->name, (int)width, (int)height);

	peak_web_listen(w, 1);
	intern.w = w;
	return intern;
}

static void
peak_platform_window_close(PeakWindowInternal *intern)
{
	struct peak_web_win *w;
	if (!intern || !intern->w)
		return;
	w = intern->w;
	peak_web_listen(w, 0);
	free(w);
	intern->w = NULL;
}

static uint32_t *
peak_platform_window_buffer(PeakWindowInternal *intern, size_t *width, size_t *height)
{
	struct peak_web_win *w = intern ? intern->w : NULL;
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
	struct peak_web_win *w = intern ? intern->w : NULL;
	if (!w)
		return;
	peak_web_dom_present(w->name, (int)w->width, (int)w->height, (uintptr_t)w->buffer);
}

static int
peak_platform_drop_drag(PeakWindowInternal *intern, const char *utf8, size_t n)
{
	(void)intern;
	(void)utf8;
	(void)n;
	return 0;
}

static int
peak_platform_clip_set(PeakWindowInternal *intern, PeakClip which, const char *utf8, size_t n)
{
	(void)intern;
	(void)which;
	(void)utf8;
	(void)n;
	return 1;
}

static int
peak_platform_clip_request(PeakWindowInternal *intern, PeakClip which)
{
	struct peak_web_win *w;
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

static bool
peak_platform_epoll(PeakWindowInternal *intern, PeakEvent *ev)
{
	struct peak_web_win *w = intern ? intern->w : NULL;
	return w ? peak_q_pop(&w->q, ev) : 0;
}

static int
peak_platform_fd(PeakWindowInternal *intern)
{
	(void)intern;
	return -1;
}

static int
peak_platform_pending(PeakWindowInternal *intern)
{
	struct peak_web_win *w = intern ? intern->w : NULL;
	return w ? (int)w->q.n : 0;
}

static void
peak_platform_window_set_title(PeakWindowInternal *intern, const char *name)
{
	(void)intern;
	if (name)
		emscripten_set_window_title(name);
}

static void
peak_platform_window_set_size(PeakWindowInternal *intern, uint32_t width, uint32_t height)
{
	struct peak_web_win *w = intern ? intern->w : NULL;
	if (!w || !width || !height)
		return;
	w->width = width;
	w->height = height;
	peak_web_dom_open(w->name, (int)width, (int)height);
}

static void
peak_platform_window_fullscreen(PeakWindowInternal *intern, int on)
{
	struct peak_web_win *w = intern ? intern->w : NULL;
	char sel[66];
	if (!w)
		return;
	peak_web_sel(w->name, sel, sizeof sel);
	if (on)
		emscripten_request_fullscreen(sel, EM_TRUE);
	else
		emscripten_exit_fullscreen();
}

static void
peak_platform_window_cursor(PeakWindowInternal *intern, int on)
{
	(void)intern;
	emscripten_hide_mouse();
	(void)on;
}

static void
peak_platform_window_pointer_relative(PeakWindowInternal *intern, int on)
{
	(void)intern;
	(void)on;
}

static float
peak_platform_window_scale(PeakWindowInternal *intern)
{
	(void)intern;
	return 1.f;
}

#define PEAK_AUDIO_FRAMES 1024

static struct {
	int run;
	uint32_t channels;
	int16_t *buf;
	void (*fill)(int16_t *out, size_t frames, void *userdata);
	void *userdata;
} peak_web_audio;

void EMSCRIPTEN_KEEPALIVE
peak_internal_web_audio_fill(int16_t *out, int frames)
{
	size_t n;

	if (!peak_web_audio.run || !out || frames <= 0)
		return;
	n = (size_t)frames * peak_web_audio.channels;
	memset(out, 0, n * sizeof(int16_t));
	if (peak_web_audio.fill)
		peak_web_audio.fill(out, (size_t)frames, peak_web_audio.userdata);
}

EM_JS(int, peak_web_audio_dom_start, (int channels, int rate, int frames, uintptr_t ptr), {
	var AC = window.AudioContext || window.webkitAudioContext;
	var ctx, proc, i, c, heap, off, ch;
	if (!AC || Module._peak_web_audio)
		return 0;
	ctx = new AC({ sampleRate: rate });
	if (ctx.sampleRate !== rate) {
		ctx.close();
		return 0;
	}
	proc = ctx.createScriptProcessor(frames, 0, channels);
	proc.onaudioprocess = function(e) {
		Module._peak_internal_web_audio_fill(ptr, frames);
		heap = Module.HEAP16;
		off = ptr >> 1;
		for (c = 0; c < channels; c++) {
			ch = e.outputBuffer.getChannelData(c);
			for (i = 0; i < frames; i++)
				ch[i] = heap[off + i * channels + c] / 32768.0;
		}
	};
	proc.connect(ctx.destination);
	ctx.resume();
	Module._peak_web_audio = { ctx: ctx, proc: proc };
	return 1;
});

EM_JS(void, peak_web_audio_dom_stop, (void), {
	var a = Module._peak_web_audio;
	if (!a)
		return;
	a.proc.disconnect();
	a.ctx.close();
	Module._peak_web_audio = null;
});

static int
peak_platform_audio_start(uint32_t channels, uint32_t rate, void (*fill)(int16_t *out, size_t frames, void *userdata), void *userdata)
{
	if (channels > 32)
		return 0;
	if (!(peak_web_audio.buf = calloc((size_t)channels * PEAK_AUDIO_FRAMES, sizeof(int16_t))))
		return 0;
	peak_web_audio.fill = fill;
	peak_web_audio.userdata = userdata;
	peak_web_audio.channels = channels;
	peak_web_audio.run = 1;
	if (!peak_web_audio_dom_start((int)channels, (int)rate, PEAK_AUDIO_FRAMES, (uintptr_t)peak_web_audio.buf)) {
		free(peak_web_audio.buf);
		peak_web_audio.buf = NULL;
		peak_web_audio.run = 0;
		peak_web_audio.fill = NULL;
		return 0;
	}
	return 1;
}

static uint64_t
peak_platform_get_time(void)
{
	return (uint64_t)(emscripten_get_now() * 1000000.0);
}

static void
peak_platform_sleep_ns(int64_t ns)
{
	if (ns <= 0) return;
	emscripten_sleep((unsigned)(ns / 1000000));
}

static const char **
peak_platform_vulkan_get_extensions(uint32_t *count)
{
	if (count) *count = 0;
	return NULL;
}

static int
peak_platform_vulkan_create_surface(PeakWindowInternal *w, void *instance, const void *allocator, void *out_surface)
{
	(void)w; (void)instance; (void)allocator; (void)out_surface;
	return 0;
}

static void
peak_platform_audio_stop(void)
{
	peak_web_audio.run = 0;
	peak_web_audio_dom_stop();
	free(peak_web_audio.buf);
	peak_web_audio.buf = NULL;
	peak_web_audio.fill = NULL;
	peak_web_audio.userdata = NULL;
}

static PeakProc
peak_internal_proc_fail(void)
{
	PeakProc p;

	p.fd = PEAK_HANDLE_INVALID;
	p.pid = 0;
	return p;
}

PeakProc
peak_pty_spawn(const char *file, const char **argv, uint32_t cols, uint32_t rows, uint32_t xpixel, uint32_t ypixel)
{
	(void)file; (void)argv; (void)cols; (void)rows; (void)xpixel; (void)ypixel;
	return peak_internal_proc_fail();
}

void
peak_pty_resize(PeakProc *pty, uint32_t cols, uint32_t rows, uint32_t xpixel, uint32_t ypixel)
{
	(void)pty; (void)cols; (void)rows; (void)xpixel; (void)ypixel;
}

int
peak_pty_reap(PeakProc *pty)
{
	(void)pty;
	return 0;
}

void
peak_pty_close(PeakProc *pty)
{
	if (!pty)
		return;
	pty->fd = PEAK_HANDLE_INVALID;
	pty->pid = 0;
}

int
peak_wait(PeakWindow *win, const PEAK_HANDLE *fds, uint32_t n, int timeout_ms)
{
	(void)win; (void)fds; (void)n; (void)timeout_ms;
	return 0;
}

int
peak_runtime_dir(char *buf, size_t cap, const char *app)
{
	(void)buf; (void)cap; (void)app;
	return 0;
}

PEAK_HANDLE
peak_sock_listen(const char *path)
{
	(void)path;
	return PEAK_HANDLE_INVALID;
}

PEAK_HANDLE
peak_sock_connect(const char *path)
{
	(void)path;
	return PEAK_HANDLE_INVALID;
}

int
peak_sock_send(PEAK_HANDLE sock, const void *buf, size_t n, PEAK_HANDLE pass)
{
	(void)sock; (void)buf; (void)n; (void)pass;
	return 0;
}

int
peak_sock_recv(PEAK_HANDLE sock, void *buf, size_t n, PEAK_HANDLE *pass)
{
	if (pass)
		*pass = PEAK_HANDLE_INVALID;
	(void)sock; (void)buf; (void)n;
	return -1;
}

int
peak_pointer_pid(PeakWindow *win)
{
	(void)win;
	return 0;
}

int
peak_pointer_local(PeakWindow *win, int *x, int *y)
{
	(void)win;
	(void)x;
	(void)y;
	return 0;
}

int
peak_filesystem_mkdir(const char *path)
{
	if (!path || !path[0])
		return 0;
	return mkdir(path, 0777) == 0;
}

int
peak_filesystem_rm(const char *path)
{
	if (!path || !path[0])
		return 0;
	if (unlink(path) == 0)
		return 1;
	return rmdir(path) == 0;
}

int
peak_filesystem_cwd(char *buf, size_t cap)
{
	if (!buf || cap < 2)
		return 0;
	return getcwd(buf, cap) != NULL;
}

int
peak_filesystem_chdir(const char *path)
{
	if (!path || !path[0])
		return 0;
	return chdir(path) == 0;
}

int
peak_filesystem_rename(const char *from, const char *to)
{
	if (!from || !from[0] || !to || !to[0])
		return 0;
	return rename(from, to) == 0;
}

int
peak_pid(void)
{
	return (int)getpid();
}

int
peak_env_set(const char *name, const char *value)
{
	if (!name || !name[0])
		return 0;
	if (value)
		return setenv(name, value, 1) == 0;
	return unsetenv(name) == 0;
}

int
peak_env_get(const char *name, char *buf, size_t cap)
{
	const char *v;
	size_t n;

	if (!name || !name[0] || !buf || cap < 2)
		return 0;
	v = getenv(name);
	if (!v || !v[0])
		return 0;
	n = strlen(v);
	if (n >= cap)
		return 0;
	memcpy(buf, v, n + 1);
	return 1;
}

int
peak_filesystem_list(const char *path, int (*fn)(const char *name, void *ud), void *ud)
{
	(void)path;
	(void)fn;
	(void)ud;
	return 0;
}

int
peak_filesystem_symlink(const char *target, const char *path)
{
	(void)target;
	(void)path;
	return 0;
}

int
peak_filesystem_readlink(const char *path, char *dst, size_t cap)
{
	(void)path;
	(void)dst;
	(void)cap;
	return 0;
}

int
peak_child_arm(void)
{
	return 1;
}

void
peak_child_disarm(void)
{
}

PEAK_HANDLE
peak_child_fd(void)
{
	return PEAK_HANDLE_INVALID;
}

void
peak_child_ack(void)
{
}

int
peak_usr1_arm(void)
{
	return 1;
}

void
peak_usr1_disarm(void)
{
}

PEAK_HANDLE
peak_usr1_fd(void)
{
	return PEAK_HANDLE_INVALID;
}

int
peak_usr1_ack(void)
{
	return 0;
}

int
peak_child_reap(int *pid, int *code)
{
	(void)pid;
	(void)code;
	return 0;
}

int
peak_stdout_silence(void)
{
	return 0;
}

int
peak_stdout_restore(void)
{
	return 0;
}

PEAK_HANDLE
peak_sock_accept(PEAK_HANDLE listen_fd)
{
	(void)listen_fd;
	return PEAK_HANDLE_INVALID;
}

int
peak_fd_read(PEAK_HANDLE fd, void *buf, size_t n)
{
	(void)fd; (void)buf; (void)n;
	return 0;
}

int
peak_fd_write(PEAK_HANDLE fd, const void *buf, size_t n)
{
	(void)fd; (void)buf; (void)n;
	return 0;
}

void
peak_fd_close(PEAK_HANDLE fd)
{
	(void)fd;
}

PeakProc
peak_job_run(const char *cmd, const char *cwd)
{
	(void)cmd; (void)cwd;
	return peak_internal_proc_fail();
}

int
peak_job_reap(PeakProc *job, int *code)
{
	(void)job; (void)code;
	return 0;
}

void
peak_job_kill(PeakProc *job)
{
	if (!job)
		return;
	job->fd = PEAK_HANDLE_INVALID;
	job->pid = 0;
}

int
peak_pid_cwd(int pid, char *buf, size_t cap)
{
	(void)pid; (void)buf; (void)cap;
	return 0;
}

size_t
peak_page_size(void)
{
	return 4096;
}

void *
peak_mirror_map(size_t size)
{
	(void)size;
	return NULL;
}

void
peak_mirror_unmap(void *p, size_t size)
{
	(void)p; (void)size;
}
