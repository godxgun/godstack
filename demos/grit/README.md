# Particle observatory — shared Peak C demo

**`demos/grit/demo.c`** is the default native and Emscripten program.
`visual.h` is the local demo-private Peak/raster helper shared by the native
and browser builds.
There is no EM_JS, custom Canvas drawing, JS simulation, or JS demo-step API.

Grit's actual pool, dynamic active array, ID hashmap, arena, LIFO stack, RNG
and vectors drive 1536 particles (4096 in stress). A fixed-step simulation
updates particles and recycles pool entries. Both simulation and rendering
verify ID lookups. `GritDraw` rasterizes gradients, grids, nearby connections,
particle glows and trails into a 720×420 AARRGGBB buffer. Row scratch measures
lit pixels/checksum and uploads to Peak. **Native keeps AARRGGBB; only web
swizzles red/blue to Peak ImageData's little-endian AABBGGRR.** All controls,
measured counts, scratch bytes, simulation steps and checksum are drawn in C.
No invented FPS. Tune `DEMO_GRIT_PARTICLES` / `DEMO_GRIT_MAX_PARTICLES` at build.

## Builds (from repository root)

```sh
cc -std=c99 -O2 -I. demos/grit/demo.c -lm -ldl -lpthread -lutil -o demos/grit/demo
./demos/grit/demo --paused
./demos/grit/demo --frames 120
mkdir -p demos/grit/.checks
cc -std=c99 -O1 -g -I. demos/grit/selftest.c -lm -ldl -lpthread -lutil -o demos/grit/.checks/selftest
./demos/grit/.checks/selftest
```

Other native platforms use existing `build_peak_native` link flags plus libm
where required. No extra renderer/font/window dependency beyond Peak is used.
For web compile the exact same `demo.c` with emcc and `-I. -lm`, modularized
`createDemo`, main export, web environment, memory growth and persistent runtime
or run `./build demo grit-web` to build the complete canvas runner.
Reduced-motion glue only sets `Module.arguments=['--paused']`; Peak handles
all presentation/events. Native title: **Particle observatory**.

Space pauses/resumes, R deterministically restarts, S toggles stress and
restarts, and Escape closes the window. Native runs accept `--frames N` to
close after N rendered loop frames, useful for bounded smoke runs. The program renders and presents a complete initial scene before
entering Peak's loop, including with `--paused`. Catch-up is bounded to three
fixed steps per tick. The workspace letterboxes into freshly acquired Peak
backbuffer dimensions on resize. Close/errors release Grit heap containers,
caller storage state and the Peak window/context.

`selftest.c` checks deterministic paused/restart frames, 120 normal/stress
simulation steps, pool recycling, ID consistency, scratch balance, native
color format and real raster changes without a display server. Run
`sha256sum -c workload.sha256` from `demos/grit` to verify the preserved
console workload and its captured output.

`workload.c` is the original console `demo.c`, unchanged. The original ASCII
scene and summary are in `workload.expected.txt`; `workload.sha256` records
source/output hashes. Run `sha256sum -c workload.sha256` from this directory.
Actual browser/native appearance, keyboard focus, resize and close handling
still need parent-builder or human real-window checks.
