/* ===========================================================================   
 * REND - Renderer Library - Copyright (c) 2026 Vasco Alves
 *
 * DESCRIPTION:
 * - High level graphics rendering API around Vulkan 1.4
 * - Pushes data to the GPU in a highly configurable and STABLE fashion.
 * - Is not responsible for initializing windows.
 * - Is not responsible for compiling shaders.
 * - Depends on Peak to be cross-platform.
 *
 * =========================================================================== */

#ifndef _REND_H_
#define _REND_H_

#define REND_MAJOR 1  // breaking API changes
#define REND_MINOR 0  // non-breaking features
#define REND_PATCH 6  // non-breaking patches and bug fixes
                    
#ifndef PEAK_VULKAN
#define PEAK_VULKAN
#endif
#if defined(REND_DEBUG) && !defined(P_LOG_DEBUG_ENABLED)
#define P_LOG_DEBUG_ENABLED 1
#endif
#include "peak.h"

typedef struct rend_renderer_t* RendRenderer; // renderer target handle
typedef struct rend_pipeline_t* RendPipeline; // represents a baked shader + gpu pipeline state (blend mode, depth, vertex format)

typedef struct RendMemory RendMemory;
typedef struct RendSpecs  RendSpecs;
typedef struct RendBuffer RendBuffer;
typedef struct RendTexture RendTexture; 

/* Typedef enums as 16 bit unsigned integers */
typedef uint16_t RendBackendType;
typedef uint16_t RendLifetime;
typedef uint16_t RendFormat;
typedef uint16_t RendTopology;
typedef uint16_t RendCullMode;
typedef uint16_t RendPolygonMode;
typedef uint16_t RendBufferType;

typedef enum RendInputRate { REND_INPUT_RATE_INSTANCE, REND_INPUT_RATE_VERTEX } RendInputRate;
typedef enum RendIndexType { REND_INDEX_UINT16 = 16, REND_INDEX_UINT32 = 32 } RendIndexType;

typedef struct {
    uint64_t binding;
    uint64_t stride;
    uint8_t  input_rate;
} RendVertexBinding;

typedef struct {
    uint64_t location;
    uint64_t binding;
    uint64_t offset;
    RendFormat format;
} RendVertexAttributes;

typedef struct {
    uint32_t offset;
    uint32_t size;
} RendPushConstantInfo;

#define REND_MAX_BINDINGS 8

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
} RendBindingInfo; // the binding of rend: rebirth

/* Clean up */
extern void rend_quit(void); // Will free ALL resources created by the library such as RendRenderer, RendPipeline, RendBuffer and RendTexture.

/* Renderer */
extern RendRenderer rend_renderer_create(PeakWindow*, RendBackendType backend, void* device, bool vsync, RendBindingInfo *bind_info); // Create Renderer that renders to a target window with 
extern void         rend_renderer_destroy(RendRenderer renderer); // Destroy renderer. Unless you need to freely create and destroy renderers, you can rely on rend_quit to clean up.
extern bool         rend_renderer_frame_begin(RendRenderer renderer); // May fail. Acquires backbuffer and starts recording commands!
extern void         rend_renderer_frame_end(RendRenderer renderer, float *delta); // Stops recording commands and presents the contents to the screen.

/* Write to Descriptor Sets */
extern void         rend_descriptor_write_ubo(RendRenderer, RendBuffer ubo, uint32_t binding, uint32_t slot); // 
extern void         rend_descriptor_write_ssbo(RendRenderer, RendBuffer ssbo, uint32_t binding, uint32_t slot);
extern void         rend_descriptor_write_texture(RendRenderer, RendTexture *texture, uint32_t binding, uint32_t slot); // Writes texture to a slot in the texture array. Does not need to be in the main render loop.

/* Buffers */
extern RendBuffer   rend_buffer_create(RendRenderer renderer, size_t size, RendBufferType type, bool gpu); // Create a buffer. The type flags define what usage is passed to the backend. Setting gpu to true will make the memory static, otherwise memory will be dynamic. 
extern void         rend_buffer_destroy(RendBuffer *buffer); // Destroy a buffer. Does not deallocate it's memory.
extern void         rend_buffer_write(RendRenderer renderer, RendBuffer *buffer, const void *data, size_t size, size_t offset); // Write to buffer. Works for dynamically and statically allocated buffers.
extern void         rend_buffer_copy(RendRenderer renderer, RendBuffer *dest, size_t dest_offset, RendBuffer *src, size_t src_offset, size_t bytes);
extern uint64_t     rend_buffer_address(RendBuffer *buffer); // Returns 64 bit address to buffer memory.

/* Textures */
extern RendTexture  rend_texture_create(RendRenderer renderer, uint32_t width, uint32_t height, uint32_t depth, uint32_t mip_levels, uint32_t layers, RendFormat format); // Create a texture.
extern RendTexture  rend_texture_create_from_data(RendRenderer renderer, const void *data, uint32_t width, uint32_t height, RendFormat format); // Create texture and copy data to it immediately.
extern void         rend_texture_destroy(RendRenderer renderer, RendTexture *texture); // Destroy texture. Does not deallocate it's memory from the bump allocator.
extern void         rend_texture_copy_data(RendRenderer renderer, RendTexture *texture, const void *data, size_t size); // Copy data to texture. MUST be the same size as the format expects (width x height x sizeof format).
extern void         rend_texture_copy_buffer(RendRenderer renderer, RendTexture *texture, RendBuffer *buffer); // Copy buffer to texture. 

/* Create, Configure, and Destroy Rendering Pipelines */
extern RendPipeline rend_pipeline_create_graphics_spirv(RendRenderer renderer, uint8_t *vertex_bytes, size_t vertex_size, uint8_t *frag_bytes, size_t frag_size, const RendVertexBinding *vertex_bindings, uint32_t vertex_binding_count, const RendVertexAttributes *vertex_attributes, uint32_t vertex_attribute_count, const RendPushConstantInfo *push_constants, uint32_t push_constant_count,  RendPolygonMode polygon_mode, RendCullMode cull_mode, RendTopology topology, RendFormat color_format, bool depth_test_enable); // Create a pipeline for a renderer using a configuration handle.
extern RendPipeline rend_pipeline_create_graphics_bindless_spirv(RendRenderer renderer, uint8_t *vertex_bytes, size_t vertex_size, uint8_t *frag_bytes, size_t frag_size, const RendPushConstantInfo *push_constants, uint32_t push_constant_count,  RendPolygonMode polygon_mode, RendCullMode cull_mode, RendTopology topology, RendFormat color_format, bool depth_test_enable); // Create a pipeline for a renderer using a configuration handle.
extern RendPipeline rend_pipeline_create_meshlet_spirv(RendRenderer renderer, uint8_t *meshlet_bytes, size_t meshlet_size, uint8_t *frag_bytes, size_t frag_size, const RendPushConstantInfo *push_constants, uint32_t push_constant_count, RendPolygonMode polygon_mode, RendCullMode cull_mode, bool depth_test_enable); // Creates a meshlet rendering pipeline.
extern RendPipeline rend_pipeline_create_compute_spirv(RendRenderer renderer, const uint8_t *compute_bytes, size_t compute_size, const RendPushConstantInfo *push_constants, uint32_t push_constant_count); // Create a compute pipeline.

/* Commands */
extern void rend_cmd_render_begin(RendRenderer renderer, float r, float g, float b, float a); // Begin render pass to default target the window.
extern void rend_cmd_render_begin_texture(RendRenderer renderer, RendTexture *texture); // Begin render pass with a texture as the target.
extern void rend_cmd_render_end(RendRenderer renderer); // End render pass.
extern void rend_cmd_render_end_texture(RendRenderer renderer, RendTexture *texture); // End render pass that targets texture. Transition texture to read optimal format.
extern void rend_cmd_bind_pipeline(RendPipeline pipeline); // Bind the pipeline to this frame.
extern void rend_cmd_bind_vertex_buffer(RendPipeline pipeline, uint32_t binding, RendBuffer buffer, size_t offset); // Bind vertex buffer to this graphics pipeline.
extern void rend_cmd_bind_index_buffer(RendPipeline pipeline, RendBuffer buffer, size_t offset, RendIndexType index_type); // Bind index buffer to this graphics pipeline.
extern void rend_cmd_push_constants(RendPipeline pipeline, void *push_data, size_t size); // Send push constants to this pipeline / command buffer.
extern void rend_cmd_dispatch(RendPipeline pipeline, uint32_t x, uint32_t y, uint32_t z); // Dispatch compute commands to group with dimensions x, y, z!
extern void rend_cmd_draw(RendPipeline pipeline, size_t count, uint32_t instance_count); // Calls draw command on the pipeline.
extern void rend_cmd_draw_indexed(RendPipeline pipeline, uint32_t index_count, uint32_t first_index, int32_t vertex_offset, uint32_t instance_count); // Draw the pipeline using indexed rendering.
extern void rend_cmd_blit(RendRenderer renderer, RendTexture *src, RendTexture *dst, uint32_t src_x, uint32_t src_y, uint32_t src_w, uint32_t src_h, uint32_t dst_x, uint32_t dst_y, uint32_t dst_w, uint32_t dst_h); // Blit a section of one texture onto another texture.

enum RendBackendType_t {
    REND_BACKEND_AUTO = 0, 
    REND_BACKEND_VULKAN_14, 
    REND_BACKEND_COUNT 
};

enum RendLifetime_t {
    REND_LIFETIME_FRAME = 0,
    REND_LIFETIME_PERMANENT
};

enum RendTopology_t {
    REND_TOPOLOGY_TRIANGLE_LIST = 0,
    REND_TOPOLOGY_TRIANGLE_STRIP,
    REND_TOPOLOGY_LINE_LIST,
    REND_TOPOLOGY_LINE_STRIP,
    REND_TOPOLOGY_POINT_LIST,
};

enum RendCullMode_t {
    REND_CULL_MODE_NONE = 0,
    REND_CULL_MODE_FRONT,
    REND_CULL_MODE_BACK,
    REND_CULL_MODE_FRONT_AND_BACK,
};

enum RendPolygonMode_t {
    REND_POLYGON_MODE_FILL = 0,
    REND_POLYGON_MODE_LINE,
    REND_POLYGON_MODE_POINT,
}; 

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

static size_t rend_format_size[REND_FORMAT_COUNT] = {
    [REND_FORMAT_UNDEFINED]            = 0,

    [REND_FORMAT_R8_UNORM]             = 1,
    [REND_FORMAT_R8G8_UNORM]           = 2,
    [REND_FORMAT_R8G8B8A8_UNORM]       = 4,
    [REND_FORMAT_B8G8R8A8_UNORM]       = 4,

    [REND_FORMAT_R8G8B8A8_SRGB]        = 4,
    [REND_FORMAT_B8G8R8A8_SRGB]        = 4,

    [REND_FORMAT_R32_SFLOAT]           = 4,
    [REND_FORMAT_R32G32_SFLOAT]        = 8,
    [REND_FORMAT_R32G32B32_SFLOAT]     = 12,
    [REND_FORMAT_R32G32B32A32_SFLOAT]  = 16,

    [REND_FORMAT_R16_SFLOAT]           = 2,
    [REND_FORMAT_R16G16_SFLOAT]        = 4,
    [REND_FORMAT_R16G16B16A16_SFLOAT]  = 8,

    [REND_FORMAT_R8G8B8A8_UINT]        = 4,
    [REND_FORMAT_R16G16B16A16_UINT]    = 8,
    [REND_FORMAT_R32_UINT]             = 4,
    [REND_FORMAT_R32_SINT]             = 4,
    [REND_FORMAT_R32G32B32A32_UINT]    = 16,

    [REND_FORMAT_D32_SFLOAT]           = 4,
    [REND_FORMAT_D24_UNORM_S8_UINT]    = 4,
    [REND_FORMAT_D32_SFLOAT_S8_UINT]   = 8, // typically padded to 64-bit alignment by GPU drivers
};

enum RendBufferType_t {
    REND_BUFFER_VERTEX   ,
    REND_BUFFER_INDEX    ,
    REND_BUFFER_UNIFORM  ,
    REND_BUFFER_STORAGE  ,
    REND_BUFFER_INDIRECT ,
    REND_BUFFER_TRANSFER ,
    REND_BUFFER_COUNT    ,
};



/* CHANGE LOG 
 * 0.1.0 - @vasco - vulkan instance
 * 0.1.1 - @vasco - swapchain
 * 0.1.2 - @vasco - command buffers
 * 0.2.0 - @vasco - push basic vertex data to the gpu 
 * 0.3.1 - @vasco - Fixed rend_quit not freeing all objects.
 * 0.4.0 - @vasco - Added resource sets.
 * 0.4.1 - @vasco - Fixed binding descriptor sets.
 * 0.4.2 - @vasco - Fixed capped framerate due to FIFO being always enabled and added vsync option.
 * 0.4.3 - @vasco - Replaced bad fence based synchronization with a single timeline semaphore.
 * 0.4.4 - @vasco - Deprecated pipeline destruction and clearing because it doesnt make any sense.
 * 0.4.5 - @vasco - Exposed depth testing.
 * 0.5.0 - @vasco - Host-visible buffers
 * 0.5.1 - @vasco - Removed RendMemProperties from public API. 
 * 0.5.2 - @vasco - push_data is now push_vertices_and_draw and pipeline_draw not takes vertex count
 * 0.6.0 - @vasco - indexed rendering
 * 0.6.1 - @vasco - cool beans
 * 0.6.2 - @vasco - Removed RendMemProperties is back.
 * 0.6.3 - @vasco - push_data removed in favor of making the pipeline more low level. A prebuilt "gfx" pipeline can be added in the future.
 * 0.6.4 - @vasco - Moved to push constants and bindless descriptors unde the hood.
 * 0.7.0 - @vasco - Low level buffer creation API if you want device-local data.
 * 0.7.1 - @vasco - typedefs for ease of use
 * 0.8.0 - @vasco - bindless resources
 * 0.8.1 - @vasco - remove old binding code from backend
 * 0.8.2 - @vasco - nothing works!!!
 * 0.8.3 - @vasco - pool allocator
 * 0.8.4 - @vasco - buffers 2.0
 * 0.8.5 - @vasco - memory 2.0
 * 0.8.6 - @vasco - arena allocator
 * 0.8.7 - @vasco - everything works!!!
 * 0.8.8 - @vasco - fix device selection
 * 0.8.9 - @vasco - images 2.0 
 * 0.9.0 - @vasco - textures!!!!!
 * 0.9.1 - @vasco - clear to color
 * 0.10.0 - @vasco - instanced rendering
 * 0.10.1 - @vasco - API clean up pt. 1 (remove RendShader in favor of pointers to data)
 * 0.10.2 - @vasco - API clean up pt. 2 (remove RendPipelineConfig in favor of large functions)
 * 0.11.0 - @vasco - compute shaders, dispatch command and render pass is now separate
 * 0.11.1 - @vasco - better descriptor binding, multiple ubo, ssbo and texture arrays
 * 0.11.2 - @vasco - cool beans
 * 1.0.0 - @vasco -  finished API release
 * 1.0.1 - @vasco - render pass that targets textures 
 * 1.0.2 - @vasco - Peak instead of Podium
 * 1.0.3 - @vasco - frame_begin no longer sticks in_frame or burns timeline on OUT_OF_DATE
 * 1.0.4 - @vasco - vulkan backend collapsed; renderer create fails cleanly
 * 1.0.5 - @vasco - vulkan host linear arena
 * 1.0.6 - @vasco - texture destroy waits idle; offscreen end transitions after pass
 *
 * 1.0.0 finished API release
 *
 * -------------------------------------------
 *
 * 1.1.0 shader hot reloading plugin (need settings management and dll loading in Peak)
 */

/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2026 Vasco Alves
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/

#endif
