/*
 * macOS window, input, CALayer present, Metal WSI, and AudioQueue.
 * AppKit is Objective-C. This file is compiled as ObjC (clang -x objective-c).
 *
 * * 0.6.0 - @vasco - macos
 */

#include <AppKit/AppKit.h>
#include <AudioToolbox/AudioToolbox.h>
#include <CoreGraphics/CoreGraphics.h>
#include <QuartzCore/CAMetalLayer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef PEAK_VULKAN
#define VK_USE_PLATFORM_METAL_EXT
#include <vulkan/vulkan.h>
#endif

#define PEAK_AUDIO_FRAMES 256
#define PEAK_AUDIO_BUFFERS 3

struct peak_macos_win {
	NSWindow *window;
	NSView *view;
	CAMetalLayer *layer;
	id delegate;
	uint32_t *buffer;
	uint32_t width;
	uint32_t height;
	int force_close;
	PeakQ q;
};

typedef struct {
	volatile int run;
	AudioQueueRef queue;
	AudioQueueBufferRef buf[PEAK_AUDIO_BUFFERS];
	uint32_t channels;
	uint32_t bytes;
	void (*fill)(int16_t *out, size_t frames, void *userdata);
	void *userdata;
} PeakAudio;

@interface PeakMacDelegate : NSObject <NSWindowDelegate>
@property(nonatomic, assign) struct peak_macos_win *w;
@end

static int peak_internal_macos_buffer(struct peak_macos_win *w, uint32_t width, uint32_t height);
static PeakKeyCode peak_internal_macos_key_map(unsigned short kc);
static PeakKeyMod peak_internal_macos_mod_map(NSEventModifierFlags flags);
static void peak_internal_macos_translate(struct peak_macos_win *w, NSEvent *ev);
static void peak_internal_macos_pump(struct peak_macos_win *w);
static void peak_internal_macos_audio_cb(void *ud, AudioQueueRef q, AudioQueueBufferRef buf);
static int peak_platform_init(void);
static void peak_platform_quit(void);
static PeakWindowInternal peak_platform_window_open(const char *name, uint32_t width, uint32_t height, uint32_t flags);
static void peak_platform_window_close(PeakWindowInternal *intern);
static uint32_t *peak_platform_window_buffer(PeakWindowInternal *intern, size_t *width, size_t *height);
static void peak_platform_window_present(PeakWindowInternal *intern);
static bool peak_platform_epoll(PeakWindowInternal *intern, PeakEvent *ev);
static int peak_platform_fd(PeakWindowInternal *intern);
static int peak_platform_pending(PeakWindowInternal *intern);
static int peak_platform_audio_start(uint32_t channels, uint32_t rate, void (*fill)(int16_t *out, size_t frames, void *userdata), void *userdata);
static void peak_platform_audio_stop(void);
static uint64_t peak_platform_get_time(void);
static void peak_platform_sleep_ns(int64_t ns);
static const char **peak_platform_vulkan_get_extensions(uint32_t *count);
static int peak_platform_vulkan_create_surface(PeakWindowInternal *intern, void *instance, const void *allocator, void *out_surface);

static NSApplication *peak_macos_app;
static PeakAudio peak_audio;

@implementation PeakMacDelegate
- (BOOL)windowShouldClose:(NSWindow *)sender
{
	struct peak_macos_win *w;
	PeakEvent ev;

	(void)sender;
	w = self.w;
	if (!w)
		return YES;
	if (w->force_close)
		return YES;
	memset(&ev, 0, sizeof ev);
	ev.type = PEAK_EVENT_WINDOW_CLOSE;
	peak_q_push(&w->q, ev);
	return NO;
}
@end

static int
peak_internal_macos_buffer(struct peak_macos_win *w, uint32_t width, uint32_t height)
{
	uint32_t *buffer;

	if (!width || !height)
		return 0;
	if (!(buffer = calloc((size_t)width * height, sizeof *buffer)))
		return 0;
	free(w->buffer);
	w->buffer = buffer;
	w->width = width;
	w->height = height;
	return 1;
}

static PeakKeyCode
peak_internal_macos_key_map(unsigned short kc)
{
	switch (kc) {
	case 0x00: return PEAK_KEY_A;
	case 0x0B: return PEAK_KEY_B;
	case 0x08: return PEAK_KEY_C;
	case 0x02: return PEAK_KEY_D;
	case 0x0E: return PEAK_KEY_E;
	case 0x03: return PEAK_KEY_F;
	case 0x05: return PEAK_KEY_G;
	case 0x04: return PEAK_KEY_H;
	case 0x22: return PEAK_KEY_I;
	case 0x26: return PEAK_KEY_J;
	case 0x28: return PEAK_KEY_K;
	case 0x25: return PEAK_KEY_L;
	case 0x2E: return PEAK_KEY_M;
	case 0x2D: return PEAK_KEY_N;
	case 0x1F: return PEAK_KEY_O;
	case 0x23: return PEAK_KEY_P;
	case 0x0C: return PEAK_KEY_Q;
	case 0x0F: return PEAK_KEY_R;
	case 0x01: return PEAK_KEY_S;
	case 0x11: return PEAK_KEY_T;
	case 0x20: return PEAK_KEY_U;
	case 0x09: return PEAK_KEY_V;
	case 0x0D: return PEAK_KEY_W;
	case 0x07: return PEAK_KEY_X;
	case 0x10: return PEAK_KEY_Y;
	case 0x06: return PEAK_KEY_Z;
	case 0x1D: return PEAK_KEY_0;
	case 0x12: return PEAK_KEY_1;
	case 0x13: return PEAK_KEY_2;
	case 0x14: return PEAK_KEY_3;
	case 0x15: return PEAK_KEY_4;
	case 0x17: return PEAK_KEY_5;
	case 0x16: return PEAK_KEY_6;
	case 0x1A: return PEAK_KEY_7;
	case 0x1C: return PEAK_KEY_8;
	case 0x19: return PEAK_KEY_9;
	case 0x7E: return PEAK_KEY_UP;
	case 0x7D: return PEAK_KEY_DOWN;
	case 0x7B: return PEAK_KEY_LEFT;
	case 0x7C: return PEAK_KEY_RIGHT;
	case 0x31: return PEAK_KEY_SPACE;
	case 0x35: return PEAK_KEY_ESCAPE;
	case 0x24: return PEAK_KEY_ENTER;
	case 0x4C: return PEAK_KEY_ENTER;
	case 0x33: return PEAK_KEY_BACKSPACE;
	case 0x30: return PEAK_KEY_TAB;
	case 0x75: return PEAK_KEY_DELETE;
	default: return PEAK_KEY_UNKNOWN;
	}
}

static PeakKeyMod
peak_internal_macos_mod_map(NSEventModifierFlags flags)
{
	if (flags & (NSEventModifierFlagControl | NSEventModifierFlagCommand))
		return PEAK_KEYMOD_CTRL;
	if (flags & NSEventModifierFlagOption)
		return PEAK_KEYMOD_ALT;
	if (flags & NSEventModifierFlagShift)
		return PEAK_KEYMOD_SHIFT;
	if (flags & NSEventModifierFlagCapsLock)
		return PEAK_KEYMOD_CAPS;
	return (PeakKeyMod)0;
}

static void
peak_internal_macos_translate(struct peak_macos_win *w, NSEvent *ev)
{
	PeakEvent out;
	NSPoint pt;
	const char *utf8;

	memset(&out, 0, sizeof out);
	pt = [ev locationInWindow];
	switch ([ev type]) {
	case NSEventTypeKeyDown:
	case NSEventTypeKeyUp:
		out.type = ([ev type] == NSEventTypeKeyDown) ? PEAK_EVENT_KEY_DOWN : PEAK_EVENT_KEY_UP;
		out.key.key = peak_internal_macos_key_map([ev keyCode]);
		out.key.mod = peak_internal_macos_mod_map([ev modifierFlags]);
		utf8 = [[ev characters] UTF8String];
		out.key.code = (utf8 && utf8[0]) ? (uint32_t)(unsigned char)utf8[0] : 0;
		peak_q_push(&w->q, out);
		break;
	case NSEventTypeScrollWheel:
		out.type = PEAK_EVENT_POINTER;
		out.pointer.state = PEAK_POINTER_PRESSED;
		out.pointer.type = ([ev deltaY] < 0) ? PEAK_POINTER_WHEEL_DOWN : PEAK_POINTER_WHEEL_UP;
		out.pointer.x = (float)pt.x;
		out.pointer.y = (float)((double)w->height - pt.y);
		peak_q_push(&w->q, out);
		break;
	case NSEventTypeLeftMouseDown:
	case NSEventTypeRightMouseDown:
	case NSEventTypeOtherMouseDown:
	case NSEventTypeLeftMouseUp:
	case NSEventTypeRightMouseUp:
	case NSEventTypeOtherMouseUp:
	case NSEventTypeMouseMoved:
	case NSEventTypeLeftMouseDragged:
	case NSEventTypeRightMouseDragged:
		out.type = PEAK_EVENT_POINTER;
		out.pointer.x = (float)pt.x;
		out.pointer.y = (float)((double)w->height - pt.y);
		if ([ev type] == NSEventTypeMouseMoved || [ev type] == NSEventTypeLeftMouseDragged
		    || [ev type] == NSEventTypeRightMouseDragged) {
			out.pointer.state = PEAK_POINTER_MOVED;
			out.pointer.type = ([ev type] == NSEventTypeRightMouseDragged)
				? PEAK_POINTER_RIGHT : PEAK_POINTER_LEFT;
		} else if ([ev type] == NSEventTypeLeftMouseDown || [ev type] == NSEventTypeRightMouseDown
		    || [ev type] == NSEventTypeOtherMouseDown) {
			out.pointer.state = PEAK_POINTER_PRESSED;
			out.pointer.type = ([ev type] == NSEventTypeRightMouseDown) ? PEAK_POINTER_RIGHT :
			                   ([ev type] == NSEventTypeOtherMouseDown) ? PEAK_POINTER_MIDDLE : PEAK_POINTER_LEFT;
		} else {
			out.pointer.state = PEAK_POINTER_RELEASED;
			out.pointer.type = ([ev type] == NSEventTypeRightMouseUp) ? PEAK_POINTER_RIGHT :
			                   ([ev type] == NSEventTypeOtherMouseUp) ? PEAK_POINTER_MIDDLE : PEAK_POINTER_LEFT;
		}
		peak_q_push(&w->q, out);
		break;
	default:
		break;
	}
}

static void
peak_internal_macos_pump(struct peak_macos_win *w)
{
	NSEvent *ev;
	NSRect bounds;
	CGFloat scale;
	uint32_t width, height;

	if (!peak_macos_app || !w->window)
		return;
	for (;;) {
		ev = [peak_macos_app nextEventMatchingMask:NSEventMaskAny
			untilDate:[NSDate distantPast]
			inMode:NSDefaultRunLoopMode
			dequeue:YES];
		if (!ev)
			break;
		peak_internal_macos_translate(w, ev);
	}
	bounds = [w->view bounds];
	scale = [w->window backingScaleFactor];
	if (scale < 1.0)
		scale = 1.0;
	width = (uint32_t)(bounds.size.width * scale);
	height = (uint32_t)(bounds.size.height * scale);
	if (width && height && (width != w->width || height != w->height)) {
		PeakEvent evr;

		if (peak_internal_macos_buffer(w, width, height)) {
			memset(&evr, 0, sizeof evr);
			evr.type = PEAK_EVENT_WINDOW_RESIZE;
			evr.resize.width = w->width;
			evr.resize.height = w->height;
			peak_q_push(&w->q, evr);
			w->layer.drawableSize = CGSizeMake((CGFloat)w->width, (CGFloat)w->height);
		}
	}
}

static int
peak_platform_init(void)
{
	if (peak_macos_app)
		return 1;
	peak_macos_app = [NSApplication sharedApplication];
	if (!peak_macos_app) {
		fputs("Failed to get NSApplication. What system are you fucking using and abusing?", stderr);
		return 0;
	}
	[peak_macos_app setActivationPolicy:NSApplicationActivationPolicyRegular];
	[peak_macos_app finishLaunching];
	return 1;
}

static void
peak_platform_quit(void)
{
	peak_macos_app = nil;
}

static PeakWindowInternal
peak_platform_window_open(const char *name, uint32_t width, uint32_t height, uint32_t flags)
{
	PeakWindowInternal intern = {0};
	struct peak_macos_win *w;
	PeakMacDelegate *del;
	NSRect rect;

	(void)flags;
	if (!peak_macos_app && !peak_platform_init())
		return intern;
	if (!(w = calloc(1, sizeof *w)))
		return intern;
	if (!peak_internal_macos_buffer(w, width, height)) {
		free(w);
		return intern;
	}

	rect = NSMakeRect(0, 0, (CGFloat)width, (CGFloat)height);
	w->window = [[NSWindow alloc]
		initWithContentRect:rect
		styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
			| NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable)
		backing:NSBackingStoreBuffered
		defer:NO];
	if (!w->window) {
		free(w->buffer);
		free(w);
		return intern;
	}
	[w->window setTitle:[NSString stringWithUTF8String:name]];
	w->view = [w->window contentView];
	w->layer = [CAMetalLayer new];
	[w->view setLayer:w->layer];
	[w->view setWantsLayer:YES];
	w->layer.drawableSize = CGSizeMake((CGFloat)width, (CGFloat)height);
	del = [PeakMacDelegate new];
	del.w = w;
	w->delegate = del;
	[w->window setDelegate:del];
	[w->window makeKeyAndOrderFront:nil];
	[peak_macos_app activateIgnoringOtherApps:YES];
	intern.w = w;
	return intern;
}

static void
peak_platform_window_close(PeakWindowInternal *intern)
{
	struct peak_macos_win *w;

	w = intern ? intern->w : NULL;
	if (!w)
		return;
	w->force_close = 1;
	if (w->window) {
		[w->window setDelegate:nil];
		[w->window close];
		[w->window release];
	}
	if (w->layer)
		[w->layer release];
	if (w->delegate)
		[w->delegate release];
	free(w->buffer);
	free(w);
	intern->w = NULL;
}

static uint32_t *
peak_platform_window_buffer(PeakWindowInternal *intern, size_t *width, size_t *height)
{
	struct peak_macos_win *w;

	w = intern ? intern->w : NULL;
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
	struct peak_macos_win *w;
	CGColorSpaceRef cs;
	CGDataProviderRef prov;
	CGImageRef img;
	size_t nbytes;

	w = intern ? intern->w : NULL;
	if (!w || !w->layer || !w->buffer)
		return;
	nbytes = (size_t)w->width * w->height * 4;
	cs = CGColorSpaceCreateDeviceRGB();
	prov = CGDataProviderCreateWithData(NULL, w->buffer, nbytes, NULL);
	img = CGImageCreate((size_t)w->width, (size_t)w->height, 8, 32, (size_t)w->width * 4, cs,
		kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst,
		prov, NULL, false, kCGRenderingIntentDefault);
	w->layer.contents = (id)img;
	if (img)
		CGImageRelease(img);
	if (prov)
		CGDataProviderRelease(prov);
	if (cs)
		CGColorSpaceRelease(cs);
}

static bool
peak_platform_epoll(PeakWindowInternal *intern, PeakEvent *ev)
{
	struct peak_macos_win *w;

	w = intern ? intern->w : NULL;
	if (!w || !w->window)
		return 0;
	if (peak_q_pop(&w->q, ev))
		return 1;
	peak_internal_macos_pump(w);
	return peak_q_pop(&w->q, ev);
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
	struct peak_macos_win *w;
	NSEvent *ev;

	w = intern ? intern->w : NULL;
	if (!w)
		return 0;
	if (w->q.n)
		return (int)w->q.n;
	if (!peak_macos_app)
		return 0;
	ev = [peak_macos_app nextEventMatchingMask:NSEventMaskAny
		untilDate:[NSDate distantPast]
		inMode:NSDefaultRunLoopMode
		dequeue:NO];
	return ev ? 1 : 0;
}

static void
peak_internal_macos_audio_cb(void *ud, AudioQueueRef q, AudioQueueBufferRef buf)
{
	(void)ud;
	if (!peak_audio.run)
		return;
	memset(buf->mAudioData, 0, buf->mAudioDataByteSize);
	if (peak_audio.fill)
		peak_audio.fill((int16_t *)buf->mAudioData,
			(size_t)buf->mAudioDataByteSize / (peak_audio.channels * sizeof(int16_t)),
			peak_audio.userdata);
	AudioQueueEnqueueBuffer(q, buf, 0, NULL);
}

static int
peak_platform_audio_start(uint32_t channels, uint32_t rate, void (*fill)(int16_t *out, size_t frames, void *userdata), void *userdata)
{
	AudioStreamBasicDescription fmt;
	int i;

	if (channels > 32)
		return 0;
	memset(&fmt, 0, sizeof fmt);
	fmt.mSampleRate = (Float64)rate;
	fmt.mFormatID = kAudioFormatLinearPCM;
	fmt.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
	fmt.mBytesPerPacket = channels * 2;
	fmt.mFramesPerPacket = 1;
	fmt.mBytesPerFrame = channels * 2;
	fmt.mChannelsPerFrame = channels;
	fmt.mBitsPerChannel = 16;
	peak_audio.channels = channels;
	peak_audio.bytes = channels * PEAK_AUDIO_FRAMES * 2;
	peak_audio.fill = fill;
	peak_audio.userdata = userdata;
	peak_audio.run = 1;
	if (AudioQueueNewOutput(&fmt, peak_internal_macos_audio_cb, NULL, NULL, NULL, 0, &peak_audio.queue) != 0)
		goto fail;
	for (i = 0; i < PEAK_AUDIO_BUFFERS; i++) {
		if (AudioQueueAllocateBuffer(peak_audio.queue, peak_audio.bytes, &peak_audio.buf[i]) != 0)
			goto fail;
		peak_audio.buf[i]->mAudioDataByteSize = peak_audio.bytes;
		peak_internal_macos_audio_cb(NULL, peak_audio.queue, peak_audio.buf[i]);
	}
	if (AudioQueueStart(peak_audio.queue, NULL) != 0)
		goto fail;
	return 1;
fail:
	peak_platform_audio_stop();
	return 0;
}

static void
peak_platform_audio_stop(void)
{
	int i;

	peak_audio.run = 0;
	if (peak_audio.queue) {
		AudioQueueStop(peak_audio.queue, 1);
		for (i = 0; i < PEAK_AUDIO_BUFFERS; i++)
			peak_audio.buf[i] = NULL;
		AudioQueueDispose(peak_audio.queue, 1);
		peak_audio.queue = NULL;
	}
	peak_audio.fill = NULL;
	peak_audio.userdata = NULL;
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

	if (ns <= 0)
		return;
	ts.tv_sec = ns / 1000000000ll;
	ts.tv_nsec = ns % 1000000000ll;
	nanosleep(&ts, NULL);
}

static const char **
peak_platform_vulkan_get_extensions(uint32_t *count)
{
	static const char *exts[] = {
		"VK_KHR_surface",
		"VK_EXT_metal_surface",
		"VK_KHR_portability_enumeration",
	};
	if (count)
		*count = 3;
	return exts;
}

static int
peak_platform_vulkan_create_surface(PeakWindowInternal *intern, void *instance, const void *allocator, void *out_surface)
{
#ifdef PEAK_VULKAN
	struct peak_macos_win *w;
	VkMetalSurfaceCreateInfoEXT ci;

	w = intern ? intern->w : NULL;
	if (!w || !w->layer)
		return 0;
	memset(&ci, 0, sizeof ci);
	ci.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
	ci.pLayer = (const void *)w->layer;
	return vkCreateMetalSurfaceEXT((VkInstance)instance, &ci,
		(const VkAllocationCallbacks *)allocator, (VkSurfaceKHR *)out_surface) == VK_SUCCESS;
#else
	(void)intern;
	(void)instance;
	(void)allocator;
	(void)out_surface;
	return 0;
#endif
}

#include "p_posix.c"
