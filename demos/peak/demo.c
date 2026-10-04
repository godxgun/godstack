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

#include "peak.h"
#include "peak.c"

#define DEMO_CAPACITY 4096
#define DEMO_SESSIONS 8
#define DEMO_MESSAGES 256

int
main(void)
{
	PeakParams params = {1, DEMO_CAPACITY, DEMO_CAPACITY};
	PeakCtx *ctx = NULL;
	PeakMemoryStats before[PEAK_MEMORY_DOMAIN_COUNT], after;
	PeakMemoryDomain domain = PEAK_MEMORY_NON_DRIVER;
	char directory[512], app[96], path[560] = {0}, moved[560] = {0};
	char received[DEMO_CAPACITY + 1];
	unsigned char *ring = NULL, *message = NULL;
	void *backing = NULL, *storage = NULL, *loaded = NULL;
	unsigned long loaded_bytes;
	size_t size = 0, page = peak_page_size(), bytes = 0, length, offset, taken;
	uint64_t started = peak_get_time();
	int session, sequence, i, made_directory = 0, ok = 0;

	for (i = 0; i < PEAK_MEMORY_DOMAIN_COUNT; i++)
		before[i] = peak_debug_memory_stats((PeakMemoryDomain)i);
	snprintf(app, sizeof(app), "godstack-demo-%d-%" PRIu64, peak_pid(), started);
	if (!peak_runtime_dir(directory, sizeof(directory), app)) {
		fprintf(stderr, "peak demo: cannot create a private runtime directory\n");
		goto done;
	}
	made_directory = 1;
	snprintf(path, sizeof(path), "%s/message", directory);
	snprintf(moved, sizeof(moved), "%s/delivered", directory);
	if (!page || page < DEMO_CAPACITY || !(ring = peak_mirror_map(page))) {
		fprintf(stderr, "peak demo: mirrored message storage unavailable\n");
		goto done;
	}
#if defined(PEAK_LINUX)
	size = peak_memory(&params);
	if (!size || size > SIZE_MAX - 15 || !(backing = malloc(size + 15))) {
		fprintf(stderr, "peak demo: caller backing unavailable\n");
		goto done;
	}
	storage = (void *)(((uintptr_t)backing + 15) & ~(uintptr_t)15);
#else
	(void)params;
	(void)size;
	(void)storage;
#endif
	for (session = 0; session < DEMO_SESSIONS; session++) {
#if defined(PEAK_LINUX)
		ctx = peak_place_in_memory_and_init(storage, size, &params);
#else
		ctx = peak_init_legacy();
#endif
		if (!ctx) {
			fprintf(stderr, "peak demo: session initialization failed\n");
			goto done;
		}
		for (sequence = 0; sequence < DEMO_MESSAGES; sequence++) {
			void *grown;
			PeakClip clip = sequence & 1 ? PEAK_CLIP_PRIMARY : PEAK_CLIP_CLIPBOARD;
			length = 1 + ((size_t)sequence * 97 + (size_t)session * 17) % DEMO_CAPACITY;
			domain = (PeakMemoryDomain)(sequence % PEAK_MEMORY_DOMAIN_COUNT);
			message = peak_debug_malloc_domain_impl(length, domain, __FILE__, __LINE__, __func__);
			if (!message)
				goto done;
			memset(message, 'a' + sequence % 26, length);
			grown = peak_debug_realloc_domain_impl(message, length + 1, domain, __FILE__, __LINE__, __func__);
			if (!grown)
				goto done;
			message = grown;
			message[length] = 0;
			/* A contiguous copy spans the mirrored ring's wrap boundary. */
			offset = page - length / 2;
			memcpy(ring + offset, message, length);
			taken = 0;
			if (!peak_clip_set(ctx, NULL, clip, (char *)ring + offset, length) ||
			    !peak_clip_request(ctx, NULL, clip) ||
			    !peak_clip_take(ctx, NULL, received, sizeof(received), &taken) ||
			    taken != length || memcmp(received, message, length)) {
				fprintf(stderr, "peak demo: message delivery failed\n");
				goto done;
			}
			if (!(sequence % 32)) {
				if (!peak_file_write(path, received, taken) || !peak_filesystem_rename(path, moved))
					goto done;
				loaded_bytes = 0;
				loaded = peak_file_alloc(moved, &loaded_bytes);
				if (!loaded || loaded_bytes != length || memcmp(loaded, message, length)) {
					fprintf(stderr, "peak demo: file delivery failed\n");
					goto done;
				}
				free(loaded);
				loaded = NULL;
				if (!peak_filesystem_rm(moved))
					goto done;
			}
			bytes += length;
			peak_debug_free_domain_impl(message, domain, __FILE__, __LINE__, __func__);
			message = NULL;
		}
		peak_quit(ctx);
		ctx = NULL;
	}
	for (i = 0; i < PEAK_MEMORY_DOMAIN_COUNT; i++) {
		after = peak_debug_memory_stats((PeakMemoryDomain)i);
		if (!after.tracking_complete || !after.accounting_complete ||
		    after.live_blocks != before[i].live_blocks || after.live_bytes != before[i].live_bytes) {
			fprintf(stderr, "peak demo: direct backing did not retire completely\n");
			goto done;
		}
	}
	printf("Peak: %d sessions, %d delivered messages, %zu bytes, %.3f ms\n",
		DEMO_SESSIONS, DEMO_SESSIONS * DEMO_MESSAGES, bytes,
		(double)(peak_get_time() - started) / 1000000.0);
	peak_debug_memory_report();
	ok = 1;
done:
	if (message)
		peak_debug_free_domain_impl(message, domain, __FILE__, __LINE__, __func__);
	free(loaded);
	peak_quit(ctx);
	free(backing);
	if (ring)
		peak_mirror_unmap(ring, page);
	if (made_directory) {
		if (peak_file_exists(path) && !peak_filesystem_rm(path))
			ok = 0;
		if (peak_file_exists(moved) && !peak_filesystem_rm(moved))
			ok = 0;
		if (!peak_filesystem_rm(directory))
			ok = 0;
	}
	if (!ok)
		fprintf(stderr, "peak demo: workload failed\n");
	return ok ? 0 : 1;
}
