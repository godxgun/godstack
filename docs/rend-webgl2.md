# Legacy Rend.h on WebGL2

`Rend.h` has a WebGL2 backend for Emscripten, alongside its native Vulkan
backend. Snake uses the same `demos/snake/snake.c` game, buffers, draw calls,
frame lifecycle and renderer API on both platforms. There is no separate
browser renderer in the demo.

Define `PEAK_IMPLEMENTATION` and `REND_IMPLEMENTATION` in one translation
unit before including `Rend.h`. For native Vulkan builds also define
`PEAK_VULKAN`. Emscripten builds do not require Vulkan headers or libraries.
`REND_BACKEND_AUTO` selects WebGL2 in an Emscripten build; explicit native
backend selections fail there. `REND_BACKEND_WEBGL2` selects the browser
backend explicitly.

The windowed renderer uses Peak's canvas and follows its backing dimensions
at `rend_renderer_frame_begin`. CSS size and backing dimensions are different:
the page should update the canvas width and height when its available space
changes. Use the renderer's borrowed color target for the current framebuffer
extent. Offscreen renderers use a private canvas and GPU color attachment.

## Shaders

WebGL2 consumes GLSL ES 3.00, while Vulkan consumes SPIR-V. The portable graphics
pipeline constructor accepts both representations and selects the appropriate
one for the backend. `rend_pipeline_create_graphics_glsl` is a convenience
constructor for WebGL2. Passing SPIR-V alone to the browser is unsupported;
the backend does not silently translate it.

Vertex locations and binding strides use the existing `RendVertexBinding` and
`RendVertexAttributes` declarations. Instance bindings use
`REND_INPUT_RATE_INSTANCE`. Push data is exposed in GLSL through a `std140`
uniform block named `RendPushConstants`. Its layout must match the caller's
push constant data. Texture descriptor binding N maps to the GLSL sampler
uniform `rend_texture_N`. Uniform buffer binding N maps to the GLSL `std140`
block `RendUniforms_N`; binding 7 is reserved for push constants. Uniform
buffer and texture descriptor arrays are unsupported. For mapped buffers, vertex/index
binding or a uniform descriptor write uploads the current host contents.

WebGL2 does not expose Vulkan buffer device addresses, compute shaders,
mesh shaders, storage buffers or bindless SPIR-V. Applications that rely on
those features need a portable graphics path. Nonzero indexed base-vertex
offsets and polygon line/point modes are unsupported. Texture formats and
renderable float formats are constrained by WebGL2 and browser extensions.

## Repeatable browser verification

Install Emscripten, Node 22 or newer, Python 3 and Chromium, then run:

```sh
python3 tools/test_rend_web.py
```

Set `CHROMIUM` to another Chromium executable if necessary. The test compiles
`tools/rend-webgl2-smoke/smoke.c` with Emscripten and runs it in headless Chromium
through CDP, with no npm dependencies. Generated binaries, browser profiles and
Emscripten cache writes stay in temporary directories.

The test checks GPU indexed instancing, depth occlusion, push constant upload,
buffer copying, exact RGBA readback, texture upload and buffer transfers,
partial blits followed by preserving passes, texture descriptor sampling and
caller texture render passes. It also checks unsupported backend/shader
selection, browser canvas resize with actual default framebuffer pixels,
resource teardown and renderer recreation. Browser runs use Chromium's ANGLE
SwiftShader WebGL2 implementation, so they verify the GPU API path rather than
hardware-driver coverage.

## Snake builds and controls

Build and run the native demo from the repository root:

```sh
./build demo snake
demos/snake/snake
```

Build the browser artifact with `./build demo snake-web`. The site Snake page
uses the site's header and theme, followed by a small row containing the
title and GitHub source link. The game canvas fills the remaining viewport
height and resizes with that space. The title and source link stay above the
canvas.

Use arrow keys or WASD to steer, Space or Enter to restart, and Escape to stop.
For deterministic native/browser rendering comparison:

```sh
demos/snake/snake --headless --frames 2 --ppm /tmp/snake-native.ppm
python3 tools/test_snake_web.py --native-ppm /tmp/snake-native.ppm
```

The Snake test checks a visible rendered scene, fixed-frame native/browser
pixel agreement, keyboard steering, backing-size changes, collision/restart,
Escape cleanup and the published browser runner. It also retains the separate
Rend2 smoke test, so both headers have independent browser coverage.

Legacy `Rend.h` and `Rend2.h` expose different APIs with overlapping names.
Do not include both in the same translation unit or substitute one for the
other. The `tools/webgl2-smoke` test exercises Rend2; this test exercises the
legacy header.
