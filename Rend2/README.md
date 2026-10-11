# Rend2 Vulkan implementation

The target contract is [SPEC.md](SPEC.md). This is an incomplete implementation of that target, separate from existing Rend. The current subset is not full Rend2 conformance. Snake now uses legacy `Rend.h`; see [its WebGL2 backend](../docs/rend-webgl2.md).

## Current subset

- Caller-backed discovery and context storage, explicit device/queue selection, fixed resource and command capacities.
- Explicit data heaps with CPU/GPU bases and cache operations; application suballocation supplies vertices, instances, indices, and root records.
- Shared C99/Slang shader data, typed GPU pointers, vertex pulling, and a per-draw root address. Pipelines accept prepared native shader artifacts tagged for the supported ABI; no runtime compiler or public vertex/resource layout.
- Application-placed color/depth textures and views, dynamic viewport/scissor/depth state, indexed instanced draws, and tightly packed RGBA8 readback.
- Explicit list/pool lifecycle and queue-produced timeline completion. Applications wait before overwriting mapped frame data.
- Native Vulkan presentation with borrowed acquired views. The host owns its native surface/window; presentation follows explicit work submission. Resize/error cleanup drains explicitly before destruction/recreation.

## Integration and ownership

Define `REND2_IMPLEMENTATION` and include `../Rend2.h` in one translation unit; native builds link the Vulkan loader. The `rend_vk_*` functions bridge host surface creation. Emscripten builds use the WebGL2 backend without Vulkan dependencies; see [WebGL2 backend](../docs/rend2-webgl2.md).

Discovery uses caller-owned aligned backing and separate family records, with fixed limits of 32 devices and 128 families per device (4096 family records cover the maximum). Discovery storage stays immutable/live through context teardown. Context placement copies configuration arrays; all placed backing stays live until successful teardown. Bootstrap management and diagnostics are caller-serialized.

The explicitly selected device must provide Vulkan 1.4 and the required descriptor-heap/address-command/shader features. Missing capabilities fail with diagnostics; there is no legacy binding fallback. The full task/mesh profile remains unsupported. Local support and executable shader ABI evidence must be validated separately from header declarations or feature bits.

## Build and checks

From the godstack directory:

```sh
./build demo rend2
demos/rend2/demo
./build demo rend-abi
python3 demos/rend_abi.py    # native typed-pointer/layout feasibility workload

```

The native ABI workload provides typed-pointer/layout feasibility evidence; it does
not establish conformance for every Rend2 operation.

## Remaining target work

The public subset does not implement the full specification: general compute/task/mesh pipelines and work, GPU-counted multi-draw, bindless descriptor writes/activation, general texture formats/views/transfers/aliasing, full attachment load/store/resolve operations, and all independent graphics state remain future work. DirectX 12 and Metal 4 are unimplemented. Cross-queue/hardware coverage, native failure injection, complete allocation/performance acceptance, and human window interaction remain separate validation obligations. See the header for the concrete supported descriptions and limits; do not infer support from a target requirement in the specification.
