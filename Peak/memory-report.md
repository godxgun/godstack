# Direct backing-request reporting

Peak's debug wrappers maintain static process-lifetime statistics. Include `peak.h` and `peak.c` normally; the headless reporting regression includes only `p_log.c`. These diagnostics are single-threaded, not a general concurrent allocator interceptor.

`peak_debug_memory_report()` prints both domains and known leaks, and returns the successful **non-driver backing-request count**. `peak_debug_memory_stats(domain)` returns a read-only snapshot. Neither operation resets counters. Coverage is only calls through instrumented wrappers, not process-wide malloc, libc/DSO internals, Vulkan device allocations, or Vulkan's internal host allocator.

Default `peak_debug_*_impl` wrappers are non-driver. The corresponding `*_domain_impl` functions take `PeakMemoryDomain`. Define `REND_DEBUG_MEMORY` to tag Rend's backend-owned host metadata/backing as driver. Rend calls these wrappers explicitly, bypassing consumer malloc/free macros: one physical request produces one event. Without that explicit flag, Rend preserves the consumer's existing allocator policy. Core, Fuse, Type and Peak calls remain non-driver even during rendering; there is no ambient rendering exemption.

## Accounting

- Successful malloc/calloc: one request, including a zero-size request if the backing allocator returns a pointer. Overflowing calloc multiplication: one failed request without calling the backing allocator.
- Nonzero successful realloc: one request, whether moved or in place. Failure: one failed request, retaining the old pointer's metadata and ownership. NULL/nonzero realloc: one request, not two.
- Every realloc wrapper call increments realloc activity. `realloc(p,0)` releases a known block; `realloc(NULL,0)` deterministically returns NULL without a backing call or release.
- `free(NULL)` does nothing. Only release of a known pointer increments released blocks.
- Tracked pointer metadata's original domain is authoritative on free/realloc. Wrong supplied domains are diagnosed and recorded in the original domain; they never reclassify the allocation.
- Unknown-pointer free/realloc is diagnosed. No known release is invented. The supplied domain's tracking is marked incomplete, and **both domains' accounting** is marked incomplete because original ownership cannot be recovered. Numeric request counters remain useful but uncertified; unknown-pointer realloc must not be interpreted as proof of driver exclusion.

Two independent fixed tables hold 512 live pointers each by default (`PEAK_MAX_ALLOCS` overrides capacity at compile time). Driver exhaustion does not evict non-driver leak entries. Lifetime request counters still advance beyond capacity. Exhaustion permanently clears that domain's `tracking_complete`; live/release/requested-byte peak gauges must not be treated as exact thereafter. Later operations on missing entries also clear accounting completeness as above. Unknown successful realloc may insert a new entry, but does not restore completeness.

Counter addition saturates at `UINT64_MAX`, marking `accounting_complete` false rather than wrapping. Underflow also invalidates accounting. Requested-byte peak is not allocator resident memory. All tables/counters have static backing; no event log, reset API or reporting metadata heap is introduced.

Per-request formatting is disabled by default. Define `PEAK_DEBUG_MEMORY_TRACE=1` before the implementation to enable it; explicit report output remains available regardless of tracing.

## Reporting regression

From the Godstack root (outputs may be placed outside the checkout):

```sh
cc -std=c99 -Wall -Wextra -Werror -g tests/peak_memory.c -o /tmp/peak_memory
for mode in '' driver-exhaustion core-exhaustion overflow unknown report-output; do
    /tmp/peak_memory $mode || exit
done
cc -std=c99 -Wall -Wextra -Werror -g -DTEST_REND_CUSTOM=1 tests/peak_memory.c -o /tmp/peak_memory_custom
/tmp/peak_memory_custom
```

Repeat the first build/run with `-fsanitize=address,undefined -fno-omit-frame-pointer` and `ASAN_OPTIONS=detect_leaks=1`. Repeat with `-DPEAK_DEBUG_MEMORY_TRACE=1` to exercise enabled tracing. The harness injects deterministic failure, in-place and moved realloc, checks over 512 lifetime requests, both domains' exhaustion and opposite-domain unknown realloc, report labels and driver leak output, and macro double-counting/custom-allocator compatibility. Only the overflow scenario seeds internal counters, because executing 2^64 real requests is infeasible. No display, platform initialization or GPU is needed. This regression is a manual focused command, not added to the all-library build/test driver.
