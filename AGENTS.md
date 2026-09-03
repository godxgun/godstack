# Agent Contract
## Files
- Header is a black box.
- Read `foo.h`. Call it.
- Do not open `foo.c` unless you are changing that library, or the header was used correctly and the process still dies.
- Vendored by copy. `-I` the library directory. Include `foo.h`, then `foo.c`. That `.c` pulls the rest of the TU.
## Survivor
One API, many backends. No plugin ABI. No `dlopen`. C99 floor. No threads.
- **Peak** — OS dirt. Brands live in `p_*.c`. Callers use `peak_*` and `PEAK_HANDLE`. Missing capability returns 0 / `PEAK_HANDLE_INVALID`, not `#error`.
- **Rend** — GPU dirt. `rend.c` vtable. Vulkan is optional (`PEAK_VULKAN`). CPU always compiles. `AUTO` falls back. Command path has no Win32/X11/Wayland.
- A backend lives while its file exists. Do not delete Win32/macOS/Wayland/CPU to tidy Linux/Vulkan.
- Do not break a public header function if a stub or no-op will do. Major version is the break.
- Callers test the result, not the OS. New OS `#ifdef` belongs in Peak (or a Rend backend), not in a Peak caller.
- VT never contains OS code. Every OS feature is a Peak function first.
## Navigation
- `rg` first.
- `read` with offset/limit.
- Never dump a library `.c` to learn an API — read the header first.
## Build
- `./build` in this directory (includes `Poof/poof.c`).
- `./build test` builds then runs.
- `./build peak` builds Peak demos + `tests/peak`.
- `tests/rend_cpu` is offscreen Rend CPU raster (no window, no Vulkan).
- `tests/peak` is Peak window tests (needs a display).
