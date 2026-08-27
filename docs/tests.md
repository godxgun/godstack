# Tests

From this directory, after `./build`:

```
./build test
```

That builds the demos and `tests/rend_cpu`, then runs them. No display for
`tests/rend_cpu`. Rend demos take `--headless` (`--frames N`, `--ppm path`).
Peak window demos need a display; they are not in `./build test`.

Do not scrape a PTY or an X window. Offscreen CPU pixels go through
`rend_renderer_read`. Headless Vulkan demos go through `--headless`.

## Add

| Kind | Files | Wire |
|------|--------|------|
| Rend CPU raster | `tests/rend_cpu.c` | `build.c` `build_rend_cpu_test` + `run_tests` |
| Fuse | `Fuse/fuse_test.c` | already in `run_tests` |
| headless Rend demo | `demos/...` + `--headless` | `run_tests` |

`tests/rend_cpu` is offscreen `REND_BACKEND_CPU` only (no `PEAK_VULKAN`).
It checks a full-screen strip quad, a triangle-list corner, and a 1:1 blit.
It must pass with no window. Do not add a windowed test to `./build test`.
