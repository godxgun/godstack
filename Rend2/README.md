# Rend2 Vulkan implementation

The target contract is [SPEC.md](SPEC.md). This is an incomplete implementation of that target, separate from existing Rend. The current subset is designed for the [snake demo](../demos/snake/snake.c); it is not full Rend2 conformance.

## Snake path

- Caller-backed discovery and context storage, explicit device/queue selection, fixed resource and command capacities.
- Explicit data heaps with CPU/GPU bases and cache operations; application suballocation supplies vertices, instances, indices, and root records.
- Shared C99/Slang shader data, typed GPU pointers, vertex pulling, and a per-draw root address. Pipelines accept prepared native shader artifacts tagged for the supported ABI; no runtime compiler or public vertex/resource layout.
- Application-placed color/depth textures and views, dynamic viewport/scissor/depth state, indexed instanced draws, and tightly packed RGBA8 readback.
- Explicit list/pool lifecycle and queue-produced timeline completion. Snake waits before overwriting mapped frame data: this is demo policy, not an implicit Rend frame manager.
- Native Vulkan presentation with borrowed acquired views. The host owns its native surface/window; presentation follows explicit work submission. Resize/error cleanup drains explicitly before destruction/recreation.

## Integration and ownership

Include `rend.h`, then `rend.c` in one translation unit; link the Vulkan loader. `rend_vk.h` is the backend-specific native instance bridge for host surface creation, not a portable resource-binding interface.

Discovery uses caller-owned aligned backing and separate family records, with fixed limits of 32 devices and 128 families per device (4096 family records cover the maximum). Discovery storage stays immutable/live through context teardown. Context placement copies configuration arrays; all placed backing stays live until successful teardown. Bootstrap management and diagnostics are caller-serialized.

The explicitly selected device must provide Vulkan 1.4 and the required descriptor-heap/address-command/shader features. Missing capabilities fail with diagnostics; there is no legacy binding fallback. The full task/mesh profile remains unsupported. Local support and executable shader ABI evidence must be validated separately from header declarations or feature bits.

## Build and checks

From the godstack directory:

```sh
./build rend2 test    # discovery, placement, and lifecycle regressions
./build snake test    # compile/validate shaders, build demo, check offscreen output
./build rend abi test # separate native typed-pointer/layout feasibility workload

demos/snake/snake --headless --frames 2 --ppm /tmp/snake.ppm
demos/snake/snake --device 0
```

Snake keeps its keyboard controls (WASD/arrows, Escape, Space/Enter to restart). Windowed mode normally runs until close; an explicit `--frames N` bounds an automated window check. `--device N` selects a discovered device and does not silently skip an unsupported choice. Headless mode is deterministic and requires no window/desktop.

The render check verifies repeated fixed-frame pixels, a visible board/snake/apple scene, and reflected shared-root layout. It does not replace interactive UX acceptance, allocation instrumentation, failure injection, or representative performance measurements. The separate native ABI workload is feasibility evidence, not proof that every Rend2 operation conforms.

## Remaining target work

The public subset does not implement the full specification: general compute/task/mesh pipelines and work, GPU-counted multi-draw, bindless descriptor writes/activation, general texture formats/views/transfers/aliasing, full attachment load/store/resolve operations, and all independent graphics state remain future work. DirectX 12 and Metal 4 are unimplemented. Cross-queue/hardware coverage, native failure injection, complete allocation/performance acceptance, and human window interaction remain separate validation obligations. See the header for the concrete supported descriptions and limits; do not infer support from a target requirement in the specification.
