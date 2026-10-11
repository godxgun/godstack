/* Peak public-API workload: caller-backed sessions, local clipboard delivery,
 * mirrored message storage, file round trips, and direct allocation reporting.
 * See LICENSE. No window, desktop clipboard, audio, or GPU is used. */
#define _POSIX_C_SOURCE 200809L
#if defined(__linux__)
#define PEAK_VULKAN /* Peak's caller-backed Linux host profile; no GPU creation. */
#endif
#define PEAK_NO_AUDIO
#define PEAK_NO_GAMEPAD

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Peak.h"
#define PEAK_IMPLEMENTATION
#include "Peak.h"

#define DEMO_CAPACITY 4096
#define DEMO_SESSIONS 8
#define DEMO_MESSAGES 256

typedef struct PeakDemo {

	PeakParams params;
	PeakCtx *ctx;
	PeakMemoryStats before[PEAK_MEMORY_DOMAIN_COUNT], after;
	PeakMemoryDomain domain;
	char directory[512], app[96], path[560], moved[560];
	char received[DEMO_CAPACITY + 1];
	unsigned char *ring, *message;
	void *backing, *storage, *loaded;
	unsigned long loaded_bytes;
	size_t size, page, bytes, length, offset, taken;
	uint64_t started;
	int session, sequence, i, made_directory, ok;

} PeakDemo;

static int demo_prepare(PeakDemo *d)
{

	for (d->i = 0; d->i < PEAK_MEMORY_DOMAIN_COUNT; d->i++)
		d->before[d->i] = peak_debug_memory_stats((PeakMemoryDomain)d->i);
	snprintf(d->app, sizeof(d->app), "godstack-demo-%d-%" PRIu64, peak_pid(), d->started);
	if (!peak_runtime_dir(d->directory, sizeof(d->directory), d->app)) {
		fprintf(stderr, "peak demo: cannot create a private runtime directory\n");
		return 0;
	}
	d->made_directory = 1;
	snprintf(d->path, sizeof(d->path), "%s/message", d->directory);
	snprintf(d->moved, sizeof(d->moved), "%s/delivered", d->directory);
	if (!d->page || d->page < DEMO_CAPACITY || !(d->ring = peak_mirror_map(d->page))) {
		fprintf(stderr, "peak demo: mirrored message storage unavailable\n");
		return 0;
	}
#if defined(PEAK_LINUX)
	d->size = peak_memory(&d->params);
	if (!d->size || d->size > SIZE_MAX - 15 || !(d->backing = malloc(d->size + 15))) {
		fprintf(stderr, "peak demo: caller backing unavailable\n");
		return 0;
	}
	d->storage = (void *)(((uintptr_t)d->backing + 15) & ~(uintptr_t)15);
#else
	(void)d->params;
	(void)d->size;
	(void)d->storage;
#endif
	return 1;
}

static int demo_deliver(PeakDemo *d)
{

	void *grown;
	PeakClip clip = d->sequence & 1 ? PEAK_CLIP_PRIMARY : PEAK_CLIP_CLIPBOARD;
	d->length = 1 + ((size_t)d->sequence * 97 + (size_t)d->session * 17) % DEMO_CAPACITY;
	d->domain = (PeakMemoryDomain)(d->sequence % PEAK_MEMORY_DOMAIN_COUNT);
	d->message = peak_debug_malloc_domain_impl(d->length, d->domain, __FILE__, __LINE__, __func__);
	if (!d->message)
		return 0;
	memset(d->message, 'a' + d->sequence % 26, d->length);
	grown = peak_debug_realloc_domain_impl(d->message, d->length + 1, d->domain, __FILE__, __LINE__,
	                                       __func__);
	if (!grown)
		return 0;
	d->message = grown;
	d->message[d->length] = 0;
	/* A contiguous copy spans the mirrored ring's wrap boundary. */
	d->offset = d->page - d->length / 2;
	memcpy(d->ring + d->offset, d->message, d->length);
	d->taken = 0;
	if (!peak_clip_set(d->ctx, NULL, clip, (char *)d->ring + d->offset, d->length) ||
	    !peak_clip_request(d->ctx, NULL, clip) ||
	    !peak_clip_take(d->ctx, NULL, d->received, sizeof(d->received), &d->taken) ||
	    d->taken != d->length || memcmp(d->received, d->message, d->length)) {
		fprintf(stderr, "peak demo: message delivery failed\n");
		return 0;
	}
	if (!(d->sequence % 32)) {
		if (!peak_file_write(d->path, d->received, d->taken) ||
		    !peak_filesystem_rename(d->path, d->moved))
			return 0;
		d->loaded_bytes = 0;
		d->loaded = peak_file_alloc(d->moved, &d->loaded_bytes);
		if (!d->loaded || d->loaded_bytes != d->length ||
		    memcmp(d->loaded, d->message, d->length)) {
			fprintf(stderr, "peak demo: file delivery failed\n");
			return 0;
		}
		free(d->loaded);
		d->loaded = NULL;
		if (!peak_filesystem_rm(d->moved))
			return 0;
	}
	d->bytes += d->length;
	peak_debug_free_domain_impl(d->message, d->domain, __FILE__, __LINE__, __func__);
	d->message = NULL;
	return 1;
}

static int demo_sessions(PeakDemo *d)
{
	for (d->session = 0; d->session < DEMO_SESSIONS; d->session++) {
#if defined(PEAK_LINUX)
		d->ctx = peak_place_in_memory_and_init(d->storage, d->size, &d->params);
#else
		d->ctx = peak_init_legacy();
#endif
		if (!d->ctx) {
			fprintf(stderr, "peak demo: session initialization failed\n");
			return 0;
		}
		for (d->sequence = 0; d->sequence < DEMO_MESSAGES; d->sequence++) {
			if (!demo_deliver(d))
				return 0;
		}

		peak_quit(d->ctx);
		d->ctx = NULL;
	}
	return 1;
}

static int demo_report(PeakDemo *d)
{
	for (d->i = 0; d->i < PEAK_MEMORY_DOMAIN_COUNT; d->i++) {
		d->after = peak_debug_memory_stats((PeakMemoryDomain)d->i);
		if (!d->after.tracking_complete || !d->after.accounting_complete ||
		    d->after.live_blocks != d->before[d->i].live_blocks ||
		    d->after.live_bytes != d->before[d->i].live_bytes) {
			fprintf(stderr, "peak demo: direct backing did not retire completely\n");
			return 0;
		}
	}
	printf("Peak: %d sessions, %d delivered messages, %zu bytes, %.3f ms\n", DEMO_SESSIONS,
	       DEMO_SESSIONS * DEMO_MESSAGES, d->bytes,
	       (double)(peak_get_time() - d->started) / 1000000.0);
	peak_debug_memory_report();
	return 1;
}

static void demo_shutdown(PeakDemo *d)
{

	if (d->message)
		peak_debug_free_domain_impl(d->message, d->domain, __FILE__, __LINE__, __func__);
	free(d->loaded);
	peak_quit(d->ctx);
	free(d->backing);
	if (d->ring)
		peak_mirror_unmap(d->ring, d->page);
	if (d->made_directory) {
		if (peak_file_exists(d->path) && !peak_filesystem_rm(d->path))
			d->ok = 0;
		if (peak_file_exists(d->moved) && !peak_filesystem_rm(d->moved))
			d->ok = 0;
		if (!peak_filesystem_rm(d->directory))
			d->ok = 0;
	}
	if (!d->ok)
		fprintf(stderr, "peak demo: workload failed\n");
}

int main(void)
{
	PeakDemo demo = {0};
	demo.params = (PeakParams){1, DEMO_CAPACITY, DEMO_CAPACITY};
	demo.domain = PEAK_MEMORY_NON_DRIVER;
	demo.page = peak_page_size();
	demo.started = peak_get_time();
	demo.ok = demo_prepare(&demo) && demo_sessions(&demo) && demo_report(&demo);
	demo_shutdown(&demo);
	return demo.ok ? 0 : 1;
}
