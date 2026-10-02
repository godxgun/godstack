/* Reporting-contract regression: no display, platform, or GPU required.
 * Fault injection makes allocation failure and realloc movement deterministic.
 * Overflow alone seeds the internal counters: 2^64 real requests are infeasible.
 */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../Peak/peak.h"

static void *test_malloc(size_t size);
static void *test_realloc(void *ptr, size_t size);
static void test_free(void *ptr);
static int fail_next, stay_next, move_next;
static size_t move_size;
static unsigned backing_requests;

#define malloc test_malloc
#define realloc test_realloc
#define free test_free
#include "../Peak/p_log.c"
#undef malloc
#undef realloc
#undef free

/* Match Pixel Pro's explicit opt-in plus unity macros. No double wrapping. */
#ifndef TEST_REND_CUSTOM
#define TEST_REND_CUSTOM 0
#endif
#if !TEST_REND_CUSTOM
#define REND_DEBUG_MEMORY
#endif
#define malloc(n) peak_debug_malloc_impl((n), __FILE__, __LINE__, __func__)
#define realloc(p, n) peak_debug_realloc_impl((p), (n), __FILE__, __LINE__, __func__)
#define free(p) peak_debug_free_impl((p), __FILE__, __LINE__, __func__)
#include "../Rend/rend_internal.h"

/* Without explicit opt-in the consumer's macro allocation policy survives. */
static void
test_custom_allocator(void)
{
	void *p = rmalloc(8);
	assert(p);
	p = rrealloc(p, 16);
	assert(p);
	rfree(p);
	assert(peak_debug_memory_stats(PEAK_MEMORY_DRIVER).allocation_requests == 0);
	assert(peak_debug_memory_stats(PEAK_MEMORY_NON_DRIVER).allocation_requests == 2);
}
#undef malloc
#undef realloc
#undef free

#define CORE(n) peak_debug_malloc_impl((n), __FILE__, __LINE__, __func__)
#define FREE(p) peak_debug_free_impl((p), __FILE__, __LINE__, __func__)
#define REALLOC(p, n) peak_debug_realloc_impl((p), (n), __FILE__, __LINE__, __func__)
#define CALLOC(n, s) peak_debug_calloc_impl((n), (s), __FILE__, __LINE__, __func__)

void *
test_malloc(size_t size)
{
	backing_requests++;
	if (fail_next) {
		fail_next = 0;
		return NULL;
	}
	return malloc(size);
}

void *
test_realloc(void *ptr, size_t size)
{
	void *new_ptr;
	backing_requests++;
	if (fail_next) {
		fail_next = 0;
		return NULL;
	}
	if (stay_next) {
		stay_next = 0;
		return ptr; /* test previously reserved at least size bytes */
	}
	if (move_next) {
		move_next = 0;
		new_ptr = malloc(size);
		assert(new_ptr);
		memcpy(new_ptr, ptr, move_size < size ? move_size : size);
		free(ptr);
		return new_ptr;
	}
	return realloc(ptr, size);
}

void
test_free(void *ptr)
{
	free(ptr);
}

static void
test_basic(void)
{
	PeakMemoryStats s, d;
	unsigned char *p, *q;
	assert(rend_format_size[0] == 0); /* Header's static format table is otherwise unused. */
	p = CORE(16);
	assert(p);
	FREE(p);
	assert(peak_debug_memory_report() == 1);
	s = peak_debug_memory_stats(PEAK_MEMORY_NON_DRIVER);
	assert(s.live_blocks == 0 && s.live_bytes == 0 && s.released_blocks == 1);
	assert(s.peak_bytes == 16 && s.tracking_complete && s.accounting_complete);
	FREE(NULL);
	for (int i = 0; i < 1024; ++i) {
		p = CORE(1);
		assert(p);
		FREE(p);
	}
	assert(peak_debug_memory_stats(PEAK_MEMORY_NON_DRIVER).allocation_requests == 1025);
	p = CALLOC(4, 8);
	assert(p);
	for (int i = 0; i < 32; ++i) assert(p[i] == 0);
	fail_next = 1;
	assert(CORE(8) == NULL);
	assert(CALLOC(SIZE_MAX, 2) == NULL);
	fail_next = 1;
	assert(REALLOC(p, 64) == NULL);
	s = peak_debug_memory_stats(PEAK_MEMORY_NON_DRIVER);
	assert(s.allocation_requests == 1026 && s.failed_requests == 3);
	assert(s.live_blocks == 1 && s.live_bytes == 32 && s.realloc_requests == 1);
	stay_next = 1;
	q = REALLOC(p, 24);
	assert(q == p);
	move_next = 1;
	move_size = 24;
	q = REALLOC(p, 48);
	assert(q);
	p = q;
	s = peak_debug_memory_stats(PEAK_MEMORY_NON_DRIVER);
	assert(s.allocation_requests == 1028 && s.live_blocks == 1 && s.live_bytes == 48 && s.peak_bytes == 48);
	assert(REALLOC(p, 0) == NULL);
	p = REALLOC(NULL, 7);
	assert(p);
	FREE(p);
	/* NULL/zero is deterministic: activity only, no backing request/release. */
	assert(REALLOC(NULL, 0) == NULL);
	s = peak_debug_memory_stats(PEAK_MEMORY_NON_DRIVER);
	assert(s.allocation_requests == 1029);
	assert(s.realloc_requests == 6 && s.live_blocks == 0 && s.live_bytes == 0);
	assert(s.tracking_complete && s.accounting_complete);

	/* Simulated rendering path: explicit driver request plus ordinary core work. */
	p = rmalloc(10);
	q = CORE(3);
	assert(p && q);
	p = rrealloc(p, 20);
	assert(p);
	rfree(p);
	FREE(q);
	d = peak_debug_memory_stats(PEAK_MEMORY_DRIVER);
	assert(d.allocation_requests == 2 && d.realloc_requests == 1 && d.released_blocks == 1 && d.live_blocks == 0);
	assert(d.tracking_complete && d.accounting_complete);
	assert(peak_debug_memory_report() == s.allocation_requests + 1);
	/* Wrong supplied domain cannot reclassify either realloc or release. */
	p = CORE(9);
	p = rrealloc(p, 12);
	assert(p);
	rfree(p);
	s = peak_debug_memory_stats(PEAK_MEMORY_NON_DRIVER);
	assert(s.domain_errors == 2 && s.live_blocks == 0 && s.accounting_complete);
	assert(peak_debug_memory_stats(PEAK_MEMORY_DRIVER).allocation_requests == 2);
	p = rmalloc(9);
	p = REALLOC(p, 12);
	assert(p);
	FREE(p);
	d = peak_debug_memory_stats(PEAK_MEMORY_DRIVER);
	assert(d.domain_errors == 2 && d.allocation_requests == 4 && d.live_blocks == 0);
	/* Overflow-rejected calloc is the only failed request without a backing call. */
	assert(backing_requests == s.allocation_requests + s.failed_requests + d.allocation_requests + d.failed_requests - 1);
}

static void
test_exhaustion(PeakMemoryDomain domain)
{
	void *p[PEAK_MAX_ALLOCS + 1], *core;
	PeakMemoryStats s;
	for (size_t i = 0; i < PEAK_MAX_ALLOCS + 1; ++i) {
		p[i] = peak_debug_malloc_domain_impl(1, domain, __FILE__, __LINE__, __func__);
		assert(p[i]);
	}
	s = peak_debug_memory_stats(domain);
	assert(s.allocation_requests == PEAK_MAX_ALLOCS + 1 && s.live_blocks == PEAK_MAX_ALLOCS + 1);
	assert(!s.tracking_complete && s.accounting_complete);
	core = CORE(5);
	assert(core);
	FREE(core);
	if (domain == PEAK_MEMORY_DRIVER) {
		s = peak_debug_memory_stats(PEAK_MEMORY_NON_DRIVER);
		assert(s.tracking_complete && s.accounting_complete && s.live_blocks == 0 && s.allocation_requests == 1);
	}
	/* A missing entry must not allow opposite-domain realloc to certify ownership. */
	PeakMemoryDomain opposite = domain == PEAK_MEMORY_DRIVER ? PEAK_MEMORY_NON_DRIVER : PEAK_MEMORY_DRIVER;
	uint64_t requests = peak_debug_memory_stats(opposite).allocation_requests;
	p[PEAK_MAX_ALLOCS] = peak_debug_realloc_domain_impl(p[PEAK_MAX_ALLOCS], 2, opposite, __FILE__, __LINE__, __func__);
	assert(p[PEAK_MAX_ALLOCS]);
	assert(peak_debug_memory_stats(opposite).allocation_requests == requests + 1);
	assert(!peak_debug_memory_stats(opposite).accounting_complete);
	assert(!peak_debug_memory_stats(domain).accounting_complete);
	for (size_t i = 0; i < PEAK_MAX_ALLOCS + 1; ++i)
		peak_debug_free_domain_impl(p[i], domain, __FILE__, __LINE__, __func__);
	s = peak_debug_memory_stats(domain);
	assert(!s.tracking_complete && !s.accounting_complete);
	assert(peak_debug_memory_stats(opposite).unknown_operations >= 1);
	/* Unknown release is not invented as a known released block. */
	assert(s.released_blocks >= PEAK_MAX_ALLOCS && s.live_blocks >= 1);
	assert(peak_debug_memory_report() == peak_debug_memory_stats(PEAK_MEMORY_NON_DRIVER).allocation_requests);
}

static void
test_overflow(void)
{
	PeakMemoryStats *s = &peak_memory_counters[PEAK_MEMORY_NON_DRIVER];
	void *p;
	s->allocation_requests = UINT64_MAX;
	s->live_bytes = UINT64_MAX;
	p = CORE(8);
	assert(p);
	assert(s->allocation_requests == UINT64_MAX && s->live_bytes == UINT64_MAX && !s->accounting_complete);
	FREE(p);
	assert(peak_debug_memory_report() == UINT64_MAX);
}

static void
test_unknown(void)
{
	void *p = malloc(4);
	PeakMemoryStats s;
	assert(p);
	FREE(p);
	s = peak_debug_memory_stats(PEAK_MEMORY_NON_DRIVER);
	assert(s.unknown_operations == 1 && s.released_blocks == 0 && s.live_blocks == 0);
	assert(!s.tracking_complete && !s.accounting_complete);
	p = malloc(4);
	assert(p);
	p = REALLOC(p, 8);
	assert(p);
	FREE(p);
	assert(peak_debug_memory_stats(PEAK_MEMORY_NON_DRIVER).allocation_requests == 1);
}

static void
test_report_output(void)
{
	char output[4096];
	void *p = rmalloc(7), *q = CORE(3);
	FILE *capture = tmpfile();
	int saved;
	size_t n;
	assert(p && q && capture);
	FREE(q);
	fflush(stdout);
	saved = dup(STDOUT_FILENO);
	assert(saved >= 0 && dup2(fileno(capture), STDOUT_FILENO) >= 0);
	assert(peak_debug_memory_report() == 1);
	fflush(stdout);
	assert(dup2(saved, STDOUT_FILENO) >= 0);
	close(saved);
	rewind(capture);
	n = fread(output, 1, sizeof(output) - 1, capture);
	output[n] = 0;
	fclose(capture);
	assert(strstr(output, "Coverage: direct instrumented backing calls"));
	assert(strstr(output, "excludes libc/DSO/Vulkan internals"));
	assert(strstr(output, "incomplete values are not exact"));
	assert(strstr(output, "[driver] successful=1"));
	assert(strstr(output, "tracking-complete=1 accounting-complete=1"));
	assert(strstr(output, "[LEAK driver]") && strstr(output, "(7 bytes)"));
	rfree(p);
}

int
main(int argc, char **argv)
{
	if (TEST_REND_CUSTOM) test_custom_allocator();
	else if (argc == 1) test_basic();
	else if (!strcmp(argv[1], "driver-exhaustion")) test_exhaustion(PEAK_MEMORY_DRIVER);
	else if (!strcmp(argv[1], "core-exhaustion")) test_exhaustion(PEAK_MEMORY_NON_DRIVER);
	else if (!strcmp(argv[1], "overflow")) test_overflow();
	else if (!strcmp(argv[1], "unknown")) test_unknown();
	else if (!strcmp(argv[1], "report-output")) test_report_output();
	else return 2;
	puts("Peak memory reporting contract passed");
	return 0;
}
