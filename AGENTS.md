# Agent Contract
## Files
- Header is a black box.
- Read `foo.h`. Call it.
- Do not open `foo.c` unless you are changing that library, or the header was used correctly and the process still dies.
- Vendored by copy. `-I` the library directory. Include `foo.h`, then `foo.c`. That `.c` pulls the rest of the TU.
## Navigation
- `rg` first.
- `read` with offset/limit.
- Never dump a library `.c` to learn an API — read the header first.
## Build
- `./build` in this directory (includes `Poof/poof.c`).
- `./build test` builds then runs.
- `tests/rend_cpu` is offscreen Rend CPU raster (no window, no Vulkan).
