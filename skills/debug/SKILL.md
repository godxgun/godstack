---
name: finalpass
description: >
  Apply this project's debug error policy. Use when adding asserts,
  crashes, stubs, or reviewing user-space contract breaches.
---

If the target is unclear, ask once. Else start now.
Do not change public API signatures unless asked.
Propose surface-area cuts first. Wait. Then apply checks.
One pass, then stop.

Prefix is the module you are in (`rend`, `peak`, …). Read that module's
internal header for the real names. Patterns below use `mod` / `MOD`.

# Reduce surface first

Fewer failure points beat more checks.

- One allocator / one init path instead of N mallocs
- Assert invariants so later code can assume them
- Do not handle what a prior assert already forbids

Propose those cuts. Do not apply until the user agrees.

# Debug vs release

`MOD_DEBUG` on: crash at the fault.
`MOD_DEBUG` off: do not crash. User-visible failure is `false`, `NULL`, or a safe stub.

- Contract (caller broke the API) — `MASSERT`. Debug abort, release no-op.
- Recoverable (skip a frame) — return `false` / `NULL` / stub. Same in both.
- Non-recoverable (init failed, cannot continue) — debug crash, release `return false` / `NULL` so the caller can shut down.
- Unfinished path — `MOD_TODO`. Always abort. Do not ship.
- Do not add new always-`exit` paths. `MOD__CRASH` is leftover; prefer the pattern below.

# Macro pattern

    #if defined(MOD_DEBUG)
    #define MASSERT_N(_1, _2, N, ...) N
    #define MASSERT(...) MASSERT_N(__VA_ARGS__, MASSERT2, MASSERT1)(__VA_ARGS__)
    #define MASSERT1(a) assert(a)
    #define MASSERT2(a, s) assert((a) && (s))
    #else
    #define MASSERT(...) ((void)0)
    #endif

    #define MOD_TODO \
        do { \
            fprintf(stderr, "MOD TODO: %s() in %s:%d\n", __func__, __FILE__, __LINE__); \
            abort(); \
        } while (0)

    #define MOD__CRASH(...) \
        /* log */ \
        exit(1);

    #define MOD__WARN(...) /* log, continue */

Do not invent a second set. Reuse the module's existing macros.

# When to use what

- `MASSERT(p)` / `MASSERT(p, "why")` — caller contract. NULL handle, wrong phase, missing hook.
- `MOD__WARN(...)` — continue, but say so.
- Return `false` / `NULL` / stub — recoverable, or the release path of a non-recoverable failure.
- `MOD_TODO` — a path that must not run yet.

Do not wrap every call. Release still needs a path:

    MASSERT(obj && obj->ctx, "Invalid obj.");
    if (!obj)
        return;

Non-recoverable (init, cannot continue). Debug never reaches `return`:

    if (!ok) {
        MASSERT(0, "init failed");
        return false;
    }
