#include <emscripten.h>
#include <emscripten/html5.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

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

struct peak_window_internal_t {
	struct peak_web_win *w;
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
	return PEAK_KEY_UNKNOWN;
}

static EM_BOOL
peak_web_key(int type, const EmscriptenKeyboardEvent *e, void *ud)
{
	PeakEvent ev = {0};
	ev.type = (type == EMSCRIPTEN_EVENT_KEYDOWN) ? PEAK_EVENT_KEY_DOWN : PEAK_EVENT_KEY_UP;
	ev.key.key = peak_web_key_map(e->code);
	ev.key.mod = e->ctrlKey ? PEAK_KEYMOD_CTRL : e->altKey ? PEAK_KEYMOD_ALT : e->shiftKey ? PEAK_KEYMOD_SHIFT : 0;
	peak_q_push(&((struct peak_web_win *)ud)->q, ev);
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

static bool
peak_platform_epoll(PeakWindowInternal *intern, PeakEvent *ev)
{
	struct peak_web_win *w = intern ? intern->w : NULL;
	return w ? peak_q_pop(&w->q, ev) : 0;
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
