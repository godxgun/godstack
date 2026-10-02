/* Logging and bounded, process-lifetime backing-request diagnostics. */
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PEAK_MAX_PRINTF 1024
#ifndef PEAK_MAX_ALLOCS
#define PEAK_MAX_ALLOCS 512
#endif
#ifndef PEAK_DEBUG_MEMORY_TRACE
#define PEAK_DEBUG_MEMORY_TRACE 0
#endif
#if PEAK_DEBUG_MEMORY_TRACE
#define PEAK_MEMORY_TRACE(...) printf(__VA_ARGS__)
#else
#define PEAK_MEMORY_TRACE(...) ((void)0)
#endif

typedef struct {
	void *ptr;
	size_t size;
	const char *file;
	const char *func;
	int line;
} PeakDebugMemoryInfo;

static PeakMemoryDomain peak_memory_domain(PeakMemoryDomain domain);
static void peak_memory_add(PeakMemoryStats *stats, uint64_t *counter, uint64_t n);
static void peak_memory_sub(PeakMemoryStats *stats, uint64_t *counter, uint64_t n);
static PeakDebugMemoryInfo *peak_memory_find(void *ptr, PeakMemoryDomain *domain);
static void peak_memory_unknown(PeakMemoryDomain domain, void *ptr);
static void peak_memory_insert(void *ptr, size_t size, PeakMemoryDomain domain, const char *file, int line, const char *func);
static void peak_memory_resize(PeakMemoryStats *stats, size_t old_size, size_t new_size);

/* Separate tables ensure driver pressure cannot evict core leak evidence. */
static PeakDebugMemoryInfo peak_ptr_array[PEAK_MEMORY_DOMAIN_COUNT][PEAK_MAX_ALLOCS];
static PeakMemoryStats peak_memory_counters[PEAK_MEMORY_DOMAIN_COUNT] = {
	{ .tracking_complete = 1, .accounting_complete = 1 },
	{ .tracking_complete = 1, .accounting_complete = 1 }
};

void
peak_log_printf(PeakLogLevel level, const char *src, ...)
{
	char out[PEAK_MAX_PRINTF];
	va_list ap;
	int len;
	size_t offset = P_PREFIX_LEN + 1;

	if (level < 0 || level >= P_COUNT_LOG_LEVEL)
		level = P_LOG_LEVEL_ERROR;
	memcpy(out, p_prefix[level], P_PREFIX_LEN);
	out[P_PREFIX_LEN] = ' ';
	va_start(ap, src);
	len = vsnprintf(out + offset, PEAK_MAX_PRINTF - offset, src, ap);
	va_end(ap);
	if (len < 0) len = 0;
	if (offset + (size_t)len >= PEAK_MAX_PRINTF)
		len = (int)(PEAK_MAX_PRINTF - offset - 1);
	out[offset + (size_t)len] = '\n';
	fwrite(out, 1, offset + (size_t)len + 1, (level <= P_LOG_LEVEL_ERROR) ? stderr : stdout);
}

PeakMemoryDomain
peak_memory_domain(PeakMemoryDomain domain)
{
	if (domain >= PEAK_MEMORY_NON_DRIVER && domain < PEAK_MEMORY_DOMAIN_COUNT)
		return domain;
	fprintf(stderr, "[ERROR] Invalid Peak memory domain %d; using non-driver\n", (int)domain);
	peak_memory_counters[PEAK_MEMORY_NON_DRIVER].accounting_complete = 0;
	return PEAK_MEMORY_NON_DRIVER;
}

void
peak_memory_add(PeakMemoryStats *stats, uint64_t *counter, uint64_t n)
{
	if (n > UINT64_MAX - *counter) {
		*counter = UINT64_MAX;
		stats->accounting_complete = 0;
	} else {
		*counter += n;
	}
}

void
peak_memory_sub(PeakMemoryStats *stats, uint64_t *counter, uint64_t n)
{
	if (n > *counter) {
		*counter = 0;
		stats->accounting_complete = 0;
	} else {
		*counter -= n;
	}
}

PeakDebugMemoryInfo *
peak_memory_find(void *ptr, PeakMemoryDomain *domain)
{
	for (int d = 0; d < PEAK_MEMORY_DOMAIN_COUNT; ++d) {
		for (size_t i = 0; i < PEAK_MAX_ALLOCS; ++i) {
			PeakDebugMemoryInfo *entry = &peak_ptr_array[d][i];
			if (entry->ptr != ptr)
				continue;
			if (*domain != (PeakMemoryDomain)d) {
				peak_memory_add(&peak_memory_counters[d], &peak_memory_counters[d].domain_errors, 1);
				fprintf(stderr, "[ERROR] Peak memory domain mismatch for %p: supplied %d, original %d\n", ptr, (int)*domain, d);
			}
			*domain = (PeakMemoryDomain)d;
			return entry;
		}
	}
	return NULL;
}

void
peak_memory_unknown(PeakMemoryDomain domain, void *ptr)
{
	PeakMemoryStats *stats = &peak_memory_counters[domain];
	peak_memory_add(stats, &stats->unknown_operations, 1);
	/* Original ownership is unknowable, so neither domain may claim completeness.
	 * This does not invalidate otherwise complete leak lists in the other domain. */
	for (int d = 0; d < PEAK_MEMORY_DOMAIN_COUNT; ++d)
		peak_memory_counters[d].accounting_complete = 0;
	stats->tracking_complete = 0;
	fprintf(stderr, "[WARNING] Peak release/realloc of untracked pointer %p; original domain unknown\n", ptr);
}

void
peak_memory_insert(void *ptr, size_t size, PeakMemoryDomain domain, const char *file, int line, const char *func)
{
	PeakMemoryStats *stats = &peak_memory_counters[domain];
	peak_memory_add(stats, &stats->live_blocks, 1);
	peak_memory_resize(stats, 0, size);
	for (size_t i = 0; i < PEAK_MAX_ALLOCS; ++i) {
		if (peak_ptr_array[domain][i].ptr)
			continue;
		peak_ptr_array[domain][i] = (PeakDebugMemoryInfo){ ptr, size, file, func, line };
		return;
	}
	if (stats->tracking_complete)
		fprintf(stderr, "[ERROR] Peak domain %d live tracking capacity (%d) exceeded\n", (int)domain, PEAK_MAX_ALLOCS);
	stats->tracking_complete = 0;
}

void
peak_memory_resize(PeakMemoryStats *stats, size_t old_size, size_t new_size)
{
	peak_memory_sub(stats, &stats->live_bytes, old_size);
	peak_memory_add(stats, &stats->live_bytes, new_size);
	if (stats->live_bytes > stats->peak_bytes)
		stats->peak_bytes = stats->live_bytes;
}

void *
peak_debug_malloc_domain_impl(size_t size, PeakMemoryDomain domain, const char *file, int line, const char *func)
{
	void *ptr = (malloc)(size);
	PeakMemoryStats *stats;
	domain = peak_memory_domain(domain);
	stats = &peak_memory_counters[domain];
	PEAK_MEMORY_TRACE("[ALLOC] %p (%zu bytes) -> %s:%d %s()\n", ptr, size, file, line, func);
	peak_memory_add(stats, ptr ? &stats->allocation_requests : &stats->failed_requests, 1);
	if (ptr)
		peak_memory_insert(ptr, size, domain, file, line, func);
	return ptr;
}

void *
peak_debug_calloc_domain_impl(size_t count, size_t size, PeakMemoryDomain domain, const char *file, int line, const char *func)
{
	void *ptr;
	domain = peak_memory_domain(domain);
	if (size && count > SIZE_MAX / size) {
		PeakMemoryStats *stats = &peak_memory_counters[domain];
		peak_memory_add(stats, &stats->failed_requests, 1);
		return NULL;
	}
	ptr = peak_debug_malloc_domain_impl(count * size, domain, file, line, func);
	if (ptr)
		memset(ptr, 0, count * size);
	return ptr;
}

void
peak_debug_free_domain_impl(void *ptr, PeakMemoryDomain domain, const char *file, int line, const char *func)
{
	PeakDebugMemoryInfo *entry;
	PeakMemoryStats *stats;
	/* file/line/func only used by optional tracing. */
	(void)file; (void)line; (void)func;
	if (!ptr)
		return;
	domain = peak_memory_domain(domain);
	entry = peak_memory_find(ptr, &domain);
	stats = &peak_memory_counters[domain];
	PEAK_MEMORY_TRACE("[FREE] %p -> %s:%d %s()\n", ptr, file, line, func);
	if (entry) {
		peak_memory_add(stats, &stats->released_blocks, 1);
		peak_memory_sub(stats, &stats->live_blocks, 1);
		peak_memory_sub(stats, &stats->live_bytes, entry->size);
		entry->ptr = NULL;
	} else {
		peak_memory_unknown(domain, ptr);
	}
	(free)(ptr);
}

void *
peak_debug_realloc_domain_impl(void *ptr, size_t size, PeakMemoryDomain domain, const char *file, int line, const char *func)
{
	PeakDebugMemoryInfo *entry = NULL;
	PeakMemoryStats *stats;
	void *new_ptr;

	domain = peak_memory_domain(domain);
	if (ptr)
		entry = peak_memory_find(ptr, &domain);
	stats = &peak_memory_counters[domain];
	peak_memory_add(stats, &stats->realloc_requests, 1);
	/* Deterministic zero-size policy, including NULL: no backing request. */
	if (!ptr)
		return size ? peak_debug_malloc_domain_impl(size, domain, file, line, func) : NULL;
	if (!entry)
		peak_memory_unknown(domain, ptr);
	if (!size) {
		/* Avoid a second lookup/mismatch diagnostic for this single operation. */
		if (entry) {
			peak_memory_add(stats, &stats->released_blocks, 1);
			peak_memory_sub(stats, &stats->live_blocks, 1);
			peak_memory_sub(stats, &stats->live_bytes, entry->size);
			entry->ptr = NULL;
		}
		(free)(ptr);
		return NULL;
	}
	/* Locate metadata before realloc: old pointer is invalid on success. */
	new_ptr = (realloc)(ptr, size);
	PEAK_MEMORY_TRACE("[REALLOC] %p (%zu bytes) -> %s:%d %s()\n", new_ptr, size, file, line, func);
	peak_memory_add(stats, new_ptr ? &stats->allocation_requests : &stats->failed_requests, 1);
	if (!new_ptr)
		return NULL;
	if (entry) {
		peak_memory_resize(stats, entry->size, size);
		*entry = (PeakDebugMemoryInfo){ new_ptr, size, file, func, line };
	} else {
		peak_memory_insert(new_ptr, size, domain, file, line, func);
	}
	return new_ptr;
}

void *
peak_debug_malloc_impl(size_t size, const char *file, int line, const char *func)
{
	return peak_debug_malloc_domain_impl(size, PEAK_MEMORY_NON_DRIVER, file, line, func);
}

void *
peak_debug_calloc_impl(size_t count, size_t size, const char *file, int line, const char *func)
{
	return peak_debug_calloc_domain_impl(count, size, PEAK_MEMORY_NON_DRIVER, file, line, func);
}

void *
peak_debug_realloc_impl(void *ptr, size_t size, const char *file, int line, const char *func)
{
	return peak_debug_realloc_domain_impl(ptr, size, PEAK_MEMORY_NON_DRIVER, file, line, func);
}

void
peak_debug_free_impl(void *ptr, const char *file, int line, const char *func)
{
	peak_debug_free_domain_impl(ptr, PEAK_MEMORY_NON_DRIVER, file, line, func);
}

PeakMemoryStats
peak_debug_memory_stats(PeakMemoryDomain domain)
{
	/* Invalid accessor arguments must not mutate process statistics. */
	if (domain < PEAK_MEMORY_NON_DRIVER || domain >= PEAK_MEMORY_DOMAIN_COUNT)
		return (PeakMemoryStats){0};
	return peak_memory_counters[domain];
}

uint64_t
peak_debug_memory_report(void)
{
	printf("\n==================== MEMORY REPORT ====================\n");
	printf("Coverage: direct instrumented backing calls; excludes libc/DSO/Vulkan internals\n");
	printf("Live/release/peak gauges require complete tracking and accounting; incomplete values are not exact\n");
	for (int d = 0; d < PEAK_MEMORY_DOMAIN_COUNT; ++d) {
		PeakMemoryStats *s = &peak_memory_counters[d];
		printf("[%s] successful=%" PRIu64 " failed=%" PRIu64 " realloc=%" PRIu64 " released=%" PRIu64 " live=%" PRIu64 " bytes=%" PRIu64 " peak=%" PRIu64 " domain-errors=%" PRIu64 " unknown=%" PRIu64 " tracking-complete=%d accounting-complete=%d\n",
		       d == PEAK_MEMORY_DRIVER ? "driver" : "non-driver", s->allocation_requests, s->failed_requests, s->realloc_requests, s->released_blocks, s->live_blocks, s->live_bytes, s->peak_bytes, s->domain_errors, s->unknown_operations, s->tracking_complete, s->accounting_complete);
		for (size_t i = 0; i < PEAK_MAX_ALLOCS; ++i) {
			PeakDebugMemoryInfo *info = &peak_ptr_array[d][i];
			if (info->ptr)
				printf("[LEAK %s] %p (%zu bytes) allocated at %s:%d in %s()\n", d == PEAK_MEMORY_DRIVER ? "driver" : "non-driver", info->ptr, info->size, info->file, info->line, info->func);
		}
	}
	printf("=======================================================\n");
	return peak_memory_counters[PEAK_MEMORY_NON_DRIVER].allocation_requests;
}
