+++
title = 'Rend'
description = 'Use legacy Rend with Peak on native Vulkan and browser WebGL2.'
weight = 60
[params]
  id = 'rend'
  product = 'Rend'
  category = 'Core Libraries'
  tags = ['C', 'Vulkan', 'WebGL2', 'graphics']
  access = 'free'
  audience = 'Core'
+++

# Rend

Rend provides one graphics API for its supported backends. `REND_BACKEND_AUTO` selects WebGL2
when compiled with Emscripten and Vulkan 1.4 on native builds that enable `PEAK_VULKAN`.

The supported graphics backends are Vulkan 1.4 and Emscripten WebGL2. DirectX 12 and Metal
selections are currently unavailable. Rend uses Peak for window creation and platform setup.

Put the implementation macros in exactly one C translation unit. Other translation units include
the headers without those macros. `PEAK_VULKAN` enables the native Vulkan backend and should be
omitted for Emscripten:

```c
#if !defined(__EMSCRIPTEN__)
#define PEAK_VULKAN
#endif
#define PEAK_IMPLEMENTATION
#include "Peak.h"
#define REND_IMPLEMENTATION
#include "Rend.h"
```

See the [Peak guide](/docs/peak/) for platform dependencies and window setup.

## Creating a Renderer

Create Peak first, then open a window and attach a Rend renderer to it. Pass `REND_BACKEND_AUTO`
to use the platform default. The renderer owns its frame and presentation state; destroy it
before closing its Peak window, then release Peak.

```c
PeakCtx *peak = peak_init();
if (!peak) return 1;

PeakWindow window = peak_window_open(peak, "Rend", 960, 540, 0);
if (!window.ctx) {
    peak_quit(peak);
    return 1;
}

RendBindingInfo bindings = {0};
RendRenderer renderer = rend_renderer_create(
    &window, REND_BACKEND_AUTO, NULL, true, &bindings);
if (!renderer) {
    peak_window_close(&window);
    peak_quit(peak);
    return 1;
}

/* Render frames here. */

rend_renderer_destroy(renderer);
rend_quit();
peak_window_close(&window);
peak_quit(peak);
```

`rend_renderer_frame_begin()` acquires a frame and can fail, for example when the window is
being resized. Record render commands only after it succeeds, then call
`rend_renderer_frame_end()` to submit and present. For headless work, use
`rend_renderer_create_offscreen()` instead of opening a window.

## Creating your first Pipeline

Graphics pipelines combine shader stages with vertex input and fixed state. Vulkan pipelines use
SPIR-V; WebGL2 pipelines use GLSL ES 3.00. `rend_pipeline_create_graphics_portable()` accepts both
shader forms and selects the one for the active backend. Supply equivalent shaders for both
backends: the GLSL below is the WebGL2 version, while Vulkan needs separately compiled SPIR-V.
The vertex binding gives the byte stride; attributes map shader locations to fields in each vertex.

This example uses a 2D position and a 16-byte push-constant color. The Vulkan SPIR-V shaders must
use the same vertex location and push-constant layout as the WebGL2 shaders:

```glsl
// Vertex shader
#version 300 es
layout(location=0) in vec2 position;
void main() { gl_Position = vec4(position, 0.0, 1.0); }
```

```glsl
// Fragment shader
#version 300 es
precision mediump float;
layout(std140) uniform RendPushConstants { vec4 color; };
out vec4 fragColor;
void main() { fragColor = color; }
```

Create the pipeline after creating the renderer. `REND_FORMAT_UNDEFINED` uses the renderer's
default color target format; depth testing is disabled here.

```c
static const RendVertexBinding vertex_binding = {
    0, sizeof(float) * 2, REND_INPUT_RATE_VERTEX
};
static const RendVertexAttributes position_attribute = {
    0, 0, 0, REND_FORMAT_2_SFLOAT32
};
static const RendPushConstantInfo color_constant = {0, sizeof(float) * 4};

RendPipeline pipeline = rend_pipeline_create_graphics_portable(
    renderer,
    vertex_spirv, vertex_spirv_size,
    fragment_spirv, fragment_spirv_size,
    vertex_glsl, fragment_glsl,
    &vertex_binding, 1,
    &position_attribute, 1,
    &color_constant, 1,
    REND_POLYGON_MODE_FILL, REND_CULL_MODE_NONE,
    REND_TOPOLOGY_TRIANGLE_LIST, REND_FORMAT_UNDEFINED, false);
if (!pipeline) return 1;
```

Store the GLSL examples in C strings named `vertex_glsl` and `fragment_glsl`. Load or embed the
separately compiled Vulkan artifacts as `vertex_spirv` and `fragment_spirv`; pass each artifact's
size in bytes. The push-constant range must match the data sent when drawing.

## Drawing a triangle

Create a vertex buffer, write three positions, and bind it while recording a frame. The push
constant color is passed to the shader before drawing.

```c
static const float triangle[] = {
     0.0f, -0.6f,
    -0.6f,  0.5f,
     0.6f,  0.5f,
};
float color[4] = {0.2f, 0.7f, 1.0f, 1.0f};

RendBuffer vertices = rend_buffer_create(
    renderer, sizeof triangle, REND_BUFFER_VERTEX, true);
if (!vertices.handle) return 1;
rend_buffer_write(renderer, &vertices, triangle, sizeof triangle, 0);

if (rend_renderer_frame_begin(renderer)) {
    rend_cmd_render_begin(renderer, 0.05f, 0.06f, 0.08f, 1.0f);
    rend_cmd_bind_pipeline(pipeline);
    rend_cmd_bind_vertex_buffer(pipeline, 0, vertices, 0);
    rend_cmd_push_constants(pipeline, color, sizeof color);
    rend_cmd_draw(pipeline, 3, 1);
    rend_cmd_render_end(renderer);
    rend_renderer_frame_end(renderer, NULL);
}

```

The draw count is the number of vertices; the final argument is the instance count. Keep buffers
alive while submitted GPU work may still reference them. Release them when they are no longer in
use, or let `rend_quit()` release all Rend objects, including buffers and pipelines.

## Drawing a Mesh

For indexed meshes, store vertex attributes in one or more vertex buffers and triangle indices
in an index buffer. Bind both buffers and issue `rend_cmd_draw_indexed()`. The vertex layout in
the pipeline must match the fields in your vertex structure.

```c
typedef struct MeshVertex { float position[2]; } MeshVertex;

static const MeshVertex mesh_vertices[] = {
    {{-0.6f, -0.5f}}, {{-0.6f, 0.5f}},
    {{ 0.6f, -0.5f}}, {{ 0.6f, 0.5f}},
};
static const uint16_t mesh_indices[] = {0, 1, 2, 2, 1, 3};

RendBuffer vertex_buffer = rend_buffer_create(
    renderer, sizeof mesh_vertices, REND_BUFFER_VERTEX, true);
RendBuffer index_buffer = rend_buffer_create(
    renderer, sizeof mesh_indices, REND_BUFFER_INDEX, true);
if (!vertex_buffer.handle || !index_buffer.handle) return 1;
rend_buffer_write(renderer, &vertex_buffer, mesh_vertices, sizeof mesh_vertices, 0);
rend_buffer_write(renderer, &index_buffer, mesh_indices, sizeof mesh_indices, 0);

if (rend_renderer_frame_begin(renderer)) {
    rend_cmd_render_begin(renderer, 0.05f, 0.06f, 0.08f, 1.0f);
    rend_cmd_bind_pipeline(pipeline);
    rend_cmd_bind_vertex_buffer(pipeline, 0, vertex_buffer, 0);
    rend_cmd_bind_index_buffer(pipeline, index_buffer, 0, REND_INDEX_UINT16);
    rend_cmd_draw_indexed(pipeline, 6, 0, 0, 1);
    rend_cmd_render_end(renderer);
    rend_renderer_frame_end(renderer, NULL);
}

```

This mesh reuses the position-only pipeline from the triangle example. For a richer vertex
structure, update the binding stride and add attributes with their matching shader locations and
`offsetof()` values. `first_index` selects the starting index, `vertex_offset` is added to each
index, and the final argument is the instance count. Keep both buffers alive while submitted GPU
work may reference them. The same buffers and draw commands work with Vulkan and WebGL2 when the
pipeline provides the matching shader for each backend.

