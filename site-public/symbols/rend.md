# Rend symbols

Documentation: https://example.org/symbols/rend/
Description: Types, macros, and functions declared by Rend.h.


# Rend symbols

For setup and API workflows, read the [Rend guide](/docs/rend/). This vault lists individual declarations; it is not a tutorial.

<!-- BEGIN GENERATED API -->

## API reference

Generated from `Rend.h` by `tools/generate_docs.py`. Edit the header comments, not this section.

Declarations from all platform branches are listed; this is not a preprocessed API.

### Types

#### RendRenderer {#rendrenderer}

Source: `Rend.h:113`

```c
typedef struct rend_renderer_t* RendRenderer;
```

renderer target handle

#### RendPipeline {#rendpipeline}

Source: `Rend.h:114`

```c
typedef struct rend_pipeline_t* RendPipeline;
```

represents a baked shader + gpu pipeline state (blend mode, depth, vertex format)

#### RendMemory {#rendmemory}

Source: `Rend.h:116`

```c
typedef struct RendMemory RendMemory;
```

#### RendSpecs {#rendspecs}

Source: `Rend.h:117`

```c
typedef struct RendSpecs  RendSpecs;
```

#### RendBuffer {#rendbuffer}

Source: `Rend.h:118`

```c
typedef struct RendBuffer RendBuffer;
```

#### RendTexture {#rendtexture}

Source: `Rend.h:119`

```c
typedef struct RendTexture RendTexture;
```

#### RendMemory {#rendmemory-1}

Source: `Rend.h:123`

```c
struct RendMemory {
    void *host_mapped_memory; // pointer if memory is host visible
    uint64_t device_memory; // original device memory pointer
    uint64_t size; // size of the memory allocation AFTER THE OFFSET
    uint64_t offset; // we must sum the offset to device memory
    uint32_t heap_index; // heap index where the memory is located
    uint32_t id; // used by custom allocators
};
```

Unchanged legacy value layouts, needed by the public by-value API.
Renderer/pipeline handles and dispatch records remain implementation-only.

#### RendBuffer {#rendbuffer-1}

Source: `Rend.h:132`

```c
struct RendBuffer {
    RendMemory memory;
    void *mapped_memory; // pointer to offset memory
    void *allocator;
    void *logical_device;
    uint64_t handle;
    uint64_t gpu_address; // addresses must be buffer specific and cannot be generalized into memory because of how vulkan works
    uint32_t usage;
    uint32_t size;
    uint8_t backend;
};
```

#### RendTexture {#rendtexture-1}

Source: `Rend.h:144`

```c
struct RendTexture {
    
    RendMemory memory;
    void *ctx;

    uint64_t handle;
    uint64_t view;
    uint64_t sampler;

    uint64_t id;

    uint32_t width;
    uint32_t height;

    uint32_t format;

    uint32_t depth;
    uint32_t mip_levels;
    uint32_t layers;

    uint32_t img_type;
    uint32_t usage;
    uint32_t sample_count_flags;
    uint32_t sharing_mode;

    uint32_t layout;

    uint8_t backend;
    uint8_t borrowed; /* swapchain / offscreen default: do not destroy the image */
};
```

#### RendSpecs {#rendspecs-1}

Source: `Rend.h:175`

```c
struct RendSpecs {
    bool graphics;
    bool transfer;
    bool compute;
    bool present;
    bool sampler_anisotropy;
    bool discrete_gpu;
};
```

#### RendBackendType {#rendbackendtype}

Source: `Rend.h:185`

```c
typedef uint16_t RendBackendType;
```

Typedef enums as 16 bit unsigned integers

#### RendFormat {#rendformat}

Source: `Rend.h:186`

```c
typedef uint16_t RendFormat;
```

#### RendTopology {#rendtopology}

Source: `Rend.h:187`

```c
typedef uint16_t RendTopology;
```

#### RendCullMode {#rendcullmode}

Source: `Rend.h:188`

```c
typedef uint16_t RendCullMode;
```

#### RendPolygonMode {#rendpolygonmode}

Source: `Rend.h:189`

```c
typedef uint16_t RendPolygonMode;
```

#### RendBufferType {#rendbuffertype}

Source: `Rend.h:190`

```c
typedef uint16_t RendBufferType;
```

#### RendInputRate {#rendinputrate}

Source: `Rend.h:192`

```c
typedef enum RendInputRate { REND_INPUT_RATE_INSTANCE, REND_INPUT_RATE_VERTEX } RendInputRate;
```

#### RendIndexType {#rendindextype}

Source: `Rend.h:193`

```c
typedef enum RendIndexType { REND_INDEX_UINT16 = 16, REND_INDEX_UINT32 = 32 } RendIndexType;
```

#### RendVertexBinding {#rendvertexbinding}

Source: `Rend.h:195`

```c
typedef struct {
    uint64_t binding;
    uint64_t stride;
    uint8_t  input_rate;
} RendVertexBinding;
```

#### RendVertexAttributes {#rendvertexattributes}

Source: `Rend.h:201`

```c
typedef struct {
    uint64_t location;
    uint64_t binding;
    uint64_t offset;
    RendFormat format;
} RendVertexAttributes;
```

#### RendPushConstantInfo {#rendpushconstantinfo}

Source: `Rend.h:208`

```c
typedef struct {
    uint32_t offset;
    uint32_t size;
} RendPushConstantInfo;
```

#### RendBindingInfo {#rendbindinginfo}

Source: `Rend.h:215`

```c
typedef struct {
    uint32_t ubo_bindings[REND_MAX_BINDINGS];
    uint32_t ubo_array_sizes[REND_MAX_BINDINGS];
    uint32_t ubo_binding_count;

    uint32_t ssbo_bindings[REND_MAX_BINDINGS];
    uint32_t ssbo_array_sizes[REND_MAX_BINDINGS];
    uint32_t ssbo_binding_count;

    uint32_t texture_bindings[REND_MAX_BINDINGS];
    uint32_t texture_array_sizes[REND_MAX_BINDINGS];
    uint32_t texture_binding_count;
} RendBindingInfo;
```

the binding of rend: rebirth

#### RendBackendType_t {#rendbackendtype_t}

Source: `Rend.h:299`

```c
enum RendBackendType_t {
    REND_BACKEND_AUTO = 0, 
    REND_BACKEND_VULKAN_14,
	REND_BACKEND_DIRECTX_12, /* Unavailable stub; creation returns NULL. */
	REND_BACKEND_METAL_4,    /* Unavailable stub; creation returns NULL. */
    REND_BACKEND_WEBGL2, /* Emscripten WebGL 2 graphics backend. */
    REND_BACKEND_COUNT 
};
```

#### RendTopology_t {#rendtopology_t}

Source: `Rend.h:308`

```c
enum RendTopology_t {
    REND_TOPOLOGY_TRIANGLE_LIST = 0,
    REND_TOPOLOGY_TRIANGLE_STRIP,
    REND_TOPOLOGY_LINE_LIST,
    REND_TOPOLOGY_LINE_STRIP,
    REND_TOPOLOGY_POINT_LIST,
};
```

#### RendCullMode_t {#rendcullmode_t}

Source: `Rend.h:316`

```c
enum RendCullMode_t {
    REND_CULL_MODE_NONE = 0,
    REND_CULL_MODE_FRONT,
    REND_CULL_MODE_BACK,
    REND_CULL_MODE_FRONT_AND_BACK,
};
```

#### RendPolygonMode_t {#rendpolygonmode_t}

Source: `Rend.h:323`

```c
enum RendPolygonMode_t {
    REND_POLYGON_MODE_FILL = 0,
    REND_POLYGON_MODE_LINE,
    REND_POLYGON_MODE_POINT,
};
```

#### RendFormat_t {#rendformat_t}

Source: `Rend.h:329`

```c
enum RendFormat_t {
    REND_FORMAT_UNDEFINED = 0,
    REND_FORMAT_R8_UNORM,
    REND_FORMAT_R8G8_UNORM,
    REND_FORMAT_R8G8B8A8_UNORM,
    REND_FORMAT_B8G8R8A8_UNORM,

    REND_FORMAT_R8G8B8A8_SRGB,
    REND_FORMAT_B8G8R8A8_SRGB,

    REND_FORMAT_R32_SFLOAT,
    REND_FORMAT_R32G32_SFLOAT,
    REND_FORMAT_R32G32B32_SFLOAT,
    REND_FORMAT_R32G32B32A32_SFLOAT,

    /* useful aliases */
    REND_FORMAT_1_SFLOAT32 = REND_FORMAT_R32_SFLOAT,
    REND_FORMAT_2_SFLOAT32 = REND_FORMAT_R32G32_SFLOAT,
    REND_FORMAT_3_SFLOAT32 = REND_FORMAT_R32G32B32_SFLOAT,
    REND_FORMAT_4_SFLOAT32 = REND_FORMAT_R32G32B32A32_SFLOAT,
    
    REND_FORMAT_R16_SFLOAT,
    REND_FORMAT_R16G16_SFLOAT,
    REND_FORMAT_R16G16B16A16_SFLOAT,

    REND_FORMAT_R8G8B8A8_UINT,
    REND_FORMAT_R16G16B16A16_UINT,
    REND_FORMAT_R32_UINT,
    REND_FORMAT_R32_SINT,
    REND_FORMAT_R32G32B32A32_UINT,

    REND_FORMAT_D32_SFLOAT,
    REND_FORMAT_D24_UNORM_S8_UINT,
    REND_FORMAT_D32_SFLOAT_S8_UINT,

    REND_FORMAT_COUNT
};
```

#### RendBufferType_t {#rendbuffertype_t}

Source: `Rend.h:398`

```c
enum RendBufferType_t {
    REND_BUFFER_VERTEX   ,
    REND_BUFFER_INDEX    ,
    REND_BUFFER_UNIFORM  ,
    REND_BUFFER_STORAGE  ,
    REND_BUFFER_INDIRECT ,
    REND_BUFFER_TRANSFER ,
    REND_BUFFER_COUNT    ,
};
```

### Functions

#### rend_quit {#rend_quit}

Source: `Rend.h:230`

```c
extern void rend_quit(void);
```

Clean up
Will free ALL resources created by the library such as RendRenderer, RendPipeline, RendBuffer and RendTexture.

#### rend_renderer_create {#rend_renderer_create}

Source: `Rend.h:233`

```c
extern RendRenderer rend_renderer_create(PeakWindow*, RendBackendType backend, void* device, bool vsync, RendBindingInfo *bind_info);
```

Renderer
Windowed renderer. First renderer creates the process device.

#### rend_renderer_create_offscreen {#rend_renderer_create_offscreen}

Source: `Rend.h:234`

```c
extern RendRenderer rend_renderer_create_offscreen(uint32_t width, uint32_t height, RendFormat format, RendBackendType backend, RendBindingInfo *bind_info);
```

No window. Default target is an owned color image. First renderer creates the process device.

#### rend_renderer_destroy {#rend_renderer_destroy}

Source: `Rend.h:235`

```c
extern void         rend_renderer_destroy(RendRenderer renderer);
```

Destroy renderer. Unless you need to freely create and destroy renderers, you can rely on rend_quit to clean up.

#### rend_renderer_frame_begin {#rend_renderer_frame_begin}

Source: `Rend.h:236`

```c
extern bool         rend_renderer_frame_begin(RendRenderer renderer);
```

May fail. Acquires backbuffer and starts recording commands!

#### rend_renderer_frame_end {#rend_renderer_frame_end}

Source: `Rend.h:237`

```c
extern void         rend_renderer_frame_end(RendRenderer renderer, float *delta);
```

Stops recording commands and presents the contents to the screen.

#### rend_renderer_color_target {#rend_renderer_color_target}

Source: `Rend.h:239`

```c
extern RendTexture *rend_renderer_color_target(RendRenderer renderer);
```

Borrowed default color. After frame_begin.

#### rend_texture_width {#rend_texture_width}

Source: `Rend.h:240`

```c
extern uint32_t     rend_texture_width(const RendTexture *texture);
```

#### rend_texture_height {#rend_texture_height}

Source: `Rend.h:241`

```c
extern uint32_t     rend_texture_height(const RendTexture *texture);
```

#### rend_texture_format {#rend_texture_format}

Source: `Rend.h:242`

```c
extern RendFormat   rend_texture_format(const RendTexture *texture);
```

#### rend_texture_id {#rend_texture_id}

Source: `Rend.h:243`

```c
extern uint64_t     rend_texture_id(RendTexture *texture);
```

#### rend_renderer_read {#rend_renderer_read}

Source: `Rend.h:244`

```c
extern void         rend_renderer_read(RendRenderer renderer, void *dst, size_t size);
```

color_target to host. Tight-packed. Outside a frame. Offscreen only.

#### rend_descriptor_write_ubo {#rend_descriptor_write_ubo}

Source: `Rend.h:247`

```c
extern void         rend_descriptor_write_ubo(RendRenderer, RendBuffer ubo, uint32_t binding, uint32_t slot);
```

Write to Descriptor Sets

#### rend_descriptor_write_ssbo {#rend_descriptor_write_ssbo}

Source: `Rend.h:248`

```c
extern void         rend_descriptor_write_ssbo(RendRenderer, RendBuffer ssbo, uint32_t binding, uint32_t slot);
```

#### rend_descriptor_write_texture {#rend_descriptor_write_texture}

Source: `Rend.h:249`

```c
extern void         rend_descriptor_write_texture(RendRenderer, RendTexture *texture, uint32_t binding, uint32_t slot);
```

Writes texture to a slot in the texture array. Does not need to be in the main render loop.

#### rend_buffer_create {#rend_buffer_create}

Source: `Rend.h:252`

```c
extern RendBuffer   rend_buffer_create(RendRenderer renderer, size_t size, RendBufferType type, bool gpu);
```

Buffers
Create a buffer. The type flags define what usage is passed to the backend. Setting gpu to true will make the memory static, otherwise memory will be dynamic.

#### rend_buffer_destroy {#rend_buffer_destroy}

Source: `Rend.h:253`

```c
extern void         rend_buffer_destroy(RendBuffer *buffer);
```

Destroy a buffer. Does not deallocate it's memory.

#### rend_buffer_write {#rend_buffer_write}

Source: `Rend.h:254`

```c
extern void         rend_buffer_write(RendRenderer renderer, RendBuffer *buffer, const void *data, size_t size, size_t offset);
```

Write to buffer. Works for dynamically and statically allocated buffers.

#### rend_buffer_copy {#rend_buffer_copy}

Source: `Rend.h:255`

```c
extern void         rend_buffer_copy(RendRenderer renderer, RendBuffer *dest, size_t dest_offset, RendBuffer *src, size_t src_offset, size_t bytes);
```

#### rend_buffer_address {#rend_buffer_address}

Source: `Rend.h:256`

```c
extern uint64_t     rend_buffer_address(RendBuffer *buffer);
```

Returns 64 bit address to buffer memory.

#### rend_buffer_mapped {#rend_buffer_mapped}

Source: `Rend.h:257`

```c
extern void        *rend_buffer_mapped(RendBuffer *buffer);
```

Host pointer if the buffer is host-visible, else NULL.

#### rend_texture_create {#rend_texture_create}

Source: `Rend.h:260`

```c
extern RendTexture  rend_texture_create(RendRenderer renderer, uint32_t width, uint32_t height, uint32_t depth, uint32_t mip_levels, uint32_t layers, RendFormat format);
```

Textures
Create a texture.

#### rend_texture_create_from_data {#rend_texture_create_from_data}

Source: `Rend.h:261`

```c
extern RendTexture  rend_texture_create_from_data(RendRenderer renderer, const void *data, uint32_t width, uint32_t height, RendFormat format);
```

Create texture and copy data to it immediately.

#### rend_texture_destroy {#rend_texture_destroy}

Source: `Rend.h:262`

```c
extern void         rend_texture_destroy(RendRenderer renderer, RendTexture *texture);
```

Destroy texture. Does not deallocate it's memory from the bump allocator.

#### rend_texture_copy_data {#rend_texture_copy_data}

Source: `Rend.h:263`

```c
extern void         rend_texture_copy_data(RendRenderer renderer, RendTexture *texture, const void *data, size_t size);
```

Copy data to texture. MUST be the same size as the format expects (width x height x sizeof format).

#### rend_texture_copy_buffer {#rend_texture_copy_buffer}

Source: `Rend.h:264`

```c
extern void         rend_texture_copy_buffer(RendRenderer renderer, RendTexture *texture, RendBuffer *buffer);
```

Copy buffer to texture. Outside a frame: transfer-queue one-shot. Inside a frame: same as rend_cmd_copy_buffer_to_texture.

#### rend_texture_read {#rend_texture_read}

Source: `Rend.h:265`

```c
extern void         rend_texture_read(RendRenderer renderer, RendTexture *texture, void *dst, size_t size);
```

Copy texture to host. Tight-packed texels. Outside a frame and pass. MUST be width x height x sizeof format.

#### rend_pipeline_create_graphics_spirv {#rend_pipeline_create_graphics_spirv}

Source: `Rend.h:268`

```c
extern RendPipeline rend_pipeline_create_graphics_spirv(RendRenderer renderer, uint8_t *vertex_bytes, size_t vertex_size, uint8_t *frag_bytes, size_t frag_size, const RendVertexBinding *vertex_bindings, uint32_t vertex_binding_count, const RendVertexAttributes *vertex_attributes, uint32_t vertex_attribute_count, const RendPushConstantInfo *push_constants, uint32_t push_constant_count,  RendPolygonMode polygon_mode, RendCullMode cull_mode, RendTopology topology, RendFormat color_format, bool depth_test_enable);
```

Create, Configure, and Destroy Rendering Pipelines
Create a pipeline for a renderer using a configuration handle.

#### rend_pipeline_create_graphics_bindless_spirv {#rend_pipeline_create_graphics_bindless_spirv}

Source: `Rend.h:269`

```c
extern RendPipeline rend_pipeline_create_graphics_bindless_spirv(RendRenderer renderer, uint8_t *vertex_bytes, size_t vertex_size, uint8_t *frag_bytes, size_t frag_size, const RendPushConstantInfo *push_constants, uint32_t push_constant_count,  RendPolygonMode polygon_mode, RendCullMode cull_mode, RendTopology topology, RendFormat color_format, bool depth_test_enable);
```

Create a pipeline for a renderer using a configuration handle.

#### rend_pipeline_create_meshlet_spirv {#rend_pipeline_create_meshlet_spirv}

Source: `Rend.h:270`

```c
extern RendPipeline rend_pipeline_create_meshlet_spirv(RendRenderer renderer, uint8_t *meshlet_bytes, size_t meshlet_size, uint8_t *frag_bytes, size_t frag_size, const RendPushConstantInfo *push_constants, uint32_t push_constant_count, RendPolygonMode polygon_mode, RendCullMode cull_mode, bool depth_test_enable);
```

Creates a meshlet rendering pipeline.

#### rend_pipeline_create_compute_spirv {#rend_pipeline_create_compute_spirv}

Source: `Rend.h:271`

```c
extern RendPipeline rend_pipeline_create_compute_spirv(RendRenderer renderer, const uint8_t *compute_bytes, size_t compute_size, const RendPushConstantInfo *push_constants, uint32_t push_constant_count);
```

Create a compute pipeline.

#### rend_pipeline_create_graphics_portable {#rend_pipeline_create_graphics_portable}

Source: `Rend.h:275`

```c
extern RendPipeline rend_pipeline_create_graphics_portable(RendRenderer renderer, const uint8_t *vertex_spirv, size_t vertex_size, const uint8_t *fragment_spirv, size_t fragment_size, const char *vertex_glsl, const char *fragment_glsl, const RendVertexBinding *vertex_bindings, uint32_t vertex_binding_count, const RendVertexAttributes *vertex_attributes, uint32_t vertex_attribute_count, const RendPushConstantInfo *push_constants, uint32_t push_constant_count, RendPolygonMode polygon_mode, RendCullMode cull_mode, RendTopology topology, RendFormat color_format, bool depth_test_enable);
```

Portable graphics: SPIR-V on Vulkan, GLSL ES 300 on WebGL2.
Web GLSL uses std140 block RendPushConstants; UBO bindings 0..6 use
RendUniforms_N blocks, textures use sampler uniforms rend_texture_N.

#### rend_pipeline_create_graphics_glsl {#rend_pipeline_create_graphics_glsl}

Source: `Rend.h:276`

```c
extern RendPipeline rend_pipeline_create_graphics_glsl(RendRenderer renderer, const char *vertex_glsl, const char *fragment_glsl, const RendVertexBinding *vertex_bindings, uint32_t vertex_binding_count, const RendVertexAttributes *vertex_attributes, uint32_t vertex_attribute_count, const RendPushConstantInfo *push_constants, uint32_t push_constant_count, RendPolygonMode polygon_mode, RendCullMode cull_mode, RendTopology topology, RendFormat color_format, bool depth_test_enable);
```

#### rend_pipeline_set_blend {#rend_pipeline_set_blend}

Source: `Rend.h:277`

```c
extern void         rend_pipeline_set_blend(RendPipeline pipeline, bool blend);
```

#### rend_cmd_render_begin {#rend_cmd_render_begin}

Source: `Rend.h:280`

```c
extern void rend_cmd_render_begin(RendRenderer renderer, float r, float g, float b, float a);
```

Commands
Pass on color_target(). Clear to RGBA.

#### rend_cmd_render_begin_preserve {#rend_cmd_render_begin_preserve}

Source: `Rend.h:285`

```c
extern void rend_cmd_render_begin_preserve(RendRenderer renderer);
```

Resume the acquired default color after a completed pass/blit in this frame.
Inside a frame, outside a pass. Loads color, clears depth, transitions the
color target from transfer destination when needed. End with render_end.
Never loads a previous frame or a target from a failed acquire.

#### rend_cmd_render_begin_texture {#rend_cmd_render_begin_texture}

Source: `Rend.h:286`

```c
extern void rend_cmd_render_begin_texture(RendRenderer renderer, RendTexture *texture);
```

Pass on a caller texture (not the borrowed color_target unless you mean to).

#### rend_cmd_render_end {#rend_cmd_render_end}

Source: `Rend.h:287`

```c
extern void rend_cmd_render_end(RendRenderer renderer);
```

End render pass.

#### rend_cmd_render_end_texture {#rend_cmd_render_end_texture}

Source: `Rend.h:288`

```c
extern void rend_cmd_render_end_texture(RendRenderer renderer, RendTexture *texture);
```

End pass on a caller texture. Ready to sample.

#### rend_cmd_bind_pipeline {#rend_cmd_bind_pipeline}

Source: `Rend.h:289`

```c
extern void rend_cmd_bind_pipeline(RendPipeline pipeline);
```

Bind the pipeline to this frame.

#### rend_cmd_bind_vertex_buffer {#rend_cmd_bind_vertex_buffer}

Source: `Rend.h:290`

```c
extern void rend_cmd_bind_vertex_buffer(RendPipeline pipeline, uint32_t binding, RendBuffer buffer, size_t offset);
```

Bind vertex buffer to this graphics pipeline.

#### rend_cmd_bind_index_buffer {#rend_cmd_bind_index_buffer}

Source: `Rend.h:291`

```c
extern void rend_cmd_bind_index_buffer(RendPipeline pipeline, RendBuffer buffer, size_t offset, RendIndexType index_type);
```

Bind index buffer to this graphics pipeline.

#### rend_cmd_push_constants {#rend_cmd_push_constants}

Source: `Rend.h:292`

```c
extern void rend_cmd_push_constants(RendPipeline pipeline, void *push_data, size_t size);
```

Send push constants to this pipeline / command buffer.

#### rend_cmd_dispatch {#rend_cmd_dispatch}

Source: `Rend.h:293`

```c
extern void rend_cmd_dispatch(RendPipeline pipeline, uint32_t x, uint32_t y, uint32_t z);
```

Dispatch compute commands to group with dimensions x, y, z!

#### rend_cmd_draw {#rend_cmd_draw}

Source: `Rend.h:294`

```c
extern void rend_cmd_draw(RendPipeline pipeline, size_t count, uint32_t instance_count);
```

Calls draw command on the pipeline.

#### rend_cmd_draw_indexed {#rend_cmd_draw_indexed}

Source: `Rend.h:295`

```c
extern void rend_cmd_draw_indexed(RendPipeline pipeline, uint32_t index_count, uint32_t first_index, int32_t vertex_offset, uint32_t instance_count);
```

Draw the pipeline using indexed rendering.

#### rend_cmd_copy_buffer_to_texture {#rend_cmd_copy_buffer_to_texture}

Source: `Rend.h:296`

```c
extern void rend_cmd_copy_buffer_to_texture(RendRenderer renderer, RendTexture *texture, RendBuffer *buffer);
```

Buffer to texture on the frame cmdbuf. Compute-write barrier, then copy. Leaves texture TRANSFER_DST. Inside a frame, outside a pass.

#### rend_cmd_blit {#rend_cmd_blit}

Source: `Rend.h:297`

```c
extern void rend_cmd_blit(RendRenderer renderer, RendTexture *src, RendTexture *dst, uint32_t src_x, uint32_t src_y, uint32_t src_w, uint32_t src_h, uint32_t dst_x, uint32_t dst_y, uint32_t dst_w, uint32_t dst_h);
```

Blit. dst may be color_target(). Inside a frame, outside a pass. color_target stays TRANSFER_DST for present.

<!-- END GENERATED API -->
