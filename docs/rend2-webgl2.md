# Rend2 WebGL2 backend (WIP)

Rend2 is work in progress and is not a complete implementation of `Rend2/SPEC.md`.
This document describes the bounded Emscripten/WebGL2 backend currently present in
`Rend2.h`; it is not a conformance or production-readiness claim.

## Common API and shader ABI

The backend implements the common `rend_*` interface, selected with
`RendParams.backend = REND_BACKEND_WEBGL2` (also the Emscripten default). The
portable `REND_SHADER_ABI_PORTABLE_V1` path is shared with native Vulkan: both
backends use the same pipeline, heap, texture/view, command, uniform, draw,
submission, timeline, and readback handles/functions. Native pipeline input is
SPIR-V; WebGL2 pipeline input is explicit-length GLSL ES 300 source. The shaders
must agree on attributes/uniforms. WebGL2 vertex shaders convert common Vulkan
clip coordinates to GL coordinates (`y = -y`, `z = 2*z-w`); readback and
viewport/scissor coordinates use top-left origins.

The current WebGL2 profile supports one graphics queue and one command pool,
RGBA8 offscreen color attachments, D32 depth attachment storage, bounded data
heaps, float vertex/instance streams, vec4/mat4 named uniforms, triangle-list
instanced draws, and synchronous RGBA8 readback during submission. Each command
list stores at most 64 operations. CPU data writes become visible after
`rend_heap_flush`; timeline waits can time out until the browser event loop
advances, and `rend_backend_wait_idle` provides an explicit synchronous drain.

Pointer-root/indexed device-address draws, descriptor heaps and samplers,
sampled/storage images, compute/task/mesh work, native surfaces and Vulkan swapchain presentation
are not supported by WebGL2 and fail explicitly. These limits are not hidden
fallbacks. Context loss is terminal and requires draining/retiring resources and
recreating the backend. `rend_webgl2_present_texture` blits a submitted RGBA8
color texture to the backend canvas on the GPU; yield to the browser event loop
after presenting. Caller storage and recorded resource lifetimes remain
explicit on both backends.

## Smoke checks

`tools/webgl2-smoke/smoke.c` exercises the same portable `rend_*` operations on
both backends, including real instanced rendering and readback. For a browser
build, compile with Emscripten WebGL2 flags and serve the generated files; the
page reports `COMMON_SMOKE_PASS` after pixel validation. For native Vulkan,
compile `portable.vert` and `portable.frag` to SPIR-V with `glslangValidator`,
then pass those two artifacts to the native smoke executable. Device support is
runtime-dependent; the test reports a failure rather than claiming a skipped
render when no suitable device is available.

These smoke tests cover this bounded Rend2 path. The current Snake demo uses
legacy `Rend.h`, with a single shared renderer in `snake.c`; see
[rend-webgl2.md](rend-webgl2.md) for its backend, build and validation instructions.
