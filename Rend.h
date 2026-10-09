/* ===========================================================================
 * REND - Renderer Library - Copyright (c) 2026 Vasco Alves
 *
 * PREFIX: REND (macros)  Rend (types)  rend_ (functions)
 *
 * DESCRIPTION:
 * - Send data to the GPU.
 * - Peak owns the window.
 * - Caller owns SPIR-V shader bytes.
 *
 * SURVIVOR:
 * - Self-contained legacy Rend header. Vulkan backend and RendVTable dispatch embedded.
 * - Define PEAK_VULKAN to enable rendering. AUTO selects Vulkan; no software fallback.
 * - No OS window APIs in the command path.
 *
 * BACKEND STATUS:
 * - Vulkan 1.4 supported
 * - DirectX 12 and Metal 4 are unavailable stubs; explicit selection fails.
 *
 * MACRO FLAGS:
 * - REND_IMPLEMENTATION      Emit implementation in exactly one translation unit.
 * - PEAK_VULKAN              Vulkan 1.4. Peak.h sets WSI. Required to create renderers.
 * - REND_DEBUG               Asserts and Peak debug log.
 * - REND_DEBUG_MEMORY        Explicit Peak driver-domain host backing diagnostics.
 *                            Without it, preserve consumer allocator macros.
 * - REND_VK_ARENA_GROW       Device-memory page grow (default 2). 1 = fit the alloc.
 * - REND_VK_ARENA_MIN        Device-memory page floor in bytes. 0 / unset = granularity*10.
 * - REND_VK_SWAPCHAIN_EXTRA  Images above minImageCount (default 2).
 * - REND_VK_COMPOSITE_PREFER_ALPHA  Prefer PRE/INHERIT over OPAQUE (transparent windows).
 *
 * USAGE:
 *     #define PEAK_VULKAN   // optional; without it renderer creation fails
 *     #define PEAK_IMPLEMENTATION
 *     #define REND_IMPLEMENTATION
 *     #include "Rend.h"
 *
 * Other translation units include "Rend.h" without implementation macros.
 * PEAK_IMPLEMENTATION is independent: Rend never defines it automatically.
 * A declarations-only include may precede the implementation include in one TU;
 * repeated includes do not emit either implementation again. Set configuration
 * flags before the first include, consistently across translation units. On
 * POSIX include Rend/Peak before system headers, or compile with
 * _POSIX_C_SOURCE=200809L. Do not also compile Rend/rend.c or mix with Rend2.h:
 * legacy Rend and Rend2 share names but are not API-compatible.
 *
 * DEPENDENCIES / PLATFORM BEHAVIOR:
 * - C99; root Peak.h supplies the platform declarations. Its implementation
 *   dependencies/link flags are documented there (Objective-C on macOS).
 * - PEAK_VULKAN additionally requires Vulkan 1.4 SDK headers and loader linkage
 *   (e.g. -lvulkan on Linux). VK_NO_PROTOTYPES is not supported.
 * - Legacy backend timing retains timespec/clock_gettime(CLOCK_MONOTONIC);
 *   Vulkan hosts must provide those APIs as before. No backend redesign here.
 * - AUTO selects Vulkan only. DirectX 12 / Metal 4 remain unavailable stubs.
 * - The active Rend/rend_internal.h, rend.c, rend_vk14.c and
 *   rend_vk_internal.c are embedded below. The five unreferenced historical
 *   files in Rend/unused code/ (buffer, command_queue, memory, pool, sbta) are
 *   intentionally not compiled or embedded; their original sources remain
 *   preserved in that directory, not needed by this distribution.
 *
 *     RendBindingInfo bind = {0};
 *     RendRenderer renderer = rend_renderer_create(win, REND_BACKEND_AUTO, NULL, true, &bind);
 */
#if 0
    if (rend_renderer_frame_begin(renderer)) {

        /* First pass: draw to a texture at in-game pixel-art size. */
        rend_cmd_render_begin_texture(renderer, &canvas); {
            rend_cmd_bind_pipeline(ui_pipeline);
            rend_cmd_push_constants(ui_pipeline, &push_constants, sizeof push_constants);
            rend_cmd_draw(ui_pipeline, vert_count, 1); /* verts via push constants */
        } rend_cmd_render_end_texture(renderer, &canvas);

        /* Second pass: post-process onto the swapchain. */
        rend_cmd_render_begin(renderer, 1.0, 0.5, 0.0, 1.0); { /* RGBA clear */
            rend_cmd_bind_pipeline(present_pipeline);
            rend_cmd_push_constants(present_pipeline, &present_pc, sizeof present_pc);
            rend_cmd_draw(present_pipeline, 4, 1); /* 4 verts, 1 instance */
        } rend_cmd_render_end(renderer);

        rend_renderer_frame_end(renderer, &delta);
    }
#endif
/* =========================================================================== */

#ifndef REND_H
#define REND_H

#define REND_MAJOR 2  // breaking API changes
#define REND_MINOR 0  // non-breaking features
#define REND_PATCH 0  // non-breaking patches and bug fixes

#ifndef REND_VK_ARENA_GROW
#define REND_VK_ARENA_GROW 2
#endif
#ifndef REND_VK_SWAPCHAIN_EXTRA
#define REND_VK_SWAPCHAIN_EXTRA 2
#endif

/* Do not auto-define PEAK_VULKAN: callers choose whether to compile Vulkan. */
#if defined(REND_DEBUG) && !defined(P_LOG_DEBUG_ENABLED)
#define P_LOG_DEBUG_ENABLED 1
#endif
#include "Peak.h"

typedef struct rend_renderer_t* RendRenderer; // renderer target handle
typedef struct rend_pipeline_t* RendPipeline; // represents a baked shader + gpu pipeline state (blend mode, depth, vertex format)

typedef struct RendMemory RendMemory;
typedef struct RendSpecs  RendSpecs;
typedef struct RendBuffer RendBuffer;
typedef struct RendTexture RendTexture;

/* Unchanged legacy value layouts, needed by the public by-value API.
 * Renderer/pipeline handles and dispatch records remain implementation-only. */
struct RendMemory {
    void *host_mapped_memory; // pointer if memory is host visible
    uint64_t device_memory; // original device memory pointer
    uint64_t size; // size of the memory allocation AFTER THE OFFSET
    uint64_t offset; // we must sum the offset to device memory
    uint32_t heap_index; // heap index where the memory is located
    uint32_t id; // used by custom allocators
};

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

struct RendSpecs {
    bool graphics;
    bool transfer;
    bool compute;
    bool present;
    bool sampler_anisotropy;
    bool discrete_gpu;
};

/* Typedef enums as 16 bit unsigned integers */
typedef uint16_t RendBackendType;
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
extern RendRenderer rend_renderer_create(PeakWindow*, RendBackendType backend, void* device, bool vsync, RendBindingInfo *bind_info); // Windowed renderer. First renderer creates the process device.
extern RendRenderer rend_renderer_create_offscreen(uint32_t width, uint32_t height, RendFormat format, RendBackendType backend, RendBindingInfo *bind_info); // No window. Default target is an owned color image. First renderer creates the process device.
extern void         rend_renderer_destroy(RendRenderer renderer); // Destroy renderer. Unless you need to freely create and destroy renderers, you can rely on rend_quit to clean up.
extern bool         rend_renderer_frame_begin(RendRenderer renderer); // May fail. Acquires backbuffer and starts recording commands!
extern void         rend_renderer_frame_end(RendRenderer renderer, float *delta); // Stops recording commands and presents the contents to the screen.
/* Borrowed default color. After frame_begin. */
extern RendTexture *rend_renderer_color_target(RendRenderer renderer);
extern uint32_t     rend_texture_width(const RendTexture *texture);
extern uint32_t     rend_texture_height(const RendTexture *texture);
extern RendFormat   rend_texture_format(const RendTexture *texture);
extern uint64_t     rend_texture_id(RendTexture *texture);
extern void         rend_renderer_read(RendRenderer renderer, void *dst, size_t size); // color_target to host. Tight-packed. Outside a frame. Offscreen only.

/* Write to Descriptor Sets */
extern void         rend_descriptor_write_ubo(RendRenderer, RendBuffer ubo, uint32_t binding, uint32_t slot);
extern void         rend_descriptor_write_ssbo(RendRenderer, RendBuffer ssbo, uint32_t binding, uint32_t slot);
extern void         rend_descriptor_write_texture(RendRenderer, RendTexture *texture, uint32_t binding, uint32_t slot); // Writes texture to a slot in the texture array. Does not need to be in the main render loop.

/* Buffers */
extern RendBuffer   rend_buffer_create(RendRenderer renderer, size_t size, RendBufferType type, bool gpu); // Create a buffer. The type flags define what usage is passed to the backend. Setting gpu to true will make the memory static, otherwise memory will be dynamic. 
extern void         rend_buffer_destroy(RendBuffer *buffer); // Destroy a buffer. Does not deallocate it's memory.
extern void         rend_buffer_write(RendRenderer renderer, RendBuffer *buffer, const void *data, size_t size, size_t offset); // Write to buffer. Works for dynamically and statically allocated buffers.
extern void         rend_buffer_copy(RendRenderer renderer, RendBuffer *dest, size_t dest_offset, RendBuffer *src, size_t src_offset, size_t bytes);
extern uint64_t     rend_buffer_address(RendBuffer *buffer); // Returns 64 bit address to buffer memory.
extern void        *rend_buffer_mapped(RendBuffer *buffer); // Host pointer if the buffer is host-visible, else NULL.

/* Textures */
extern RendTexture  rend_texture_create(RendRenderer renderer, uint32_t width, uint32_t height, uint32_t depth, uint32_t mip_levels, uint32_t layers, RendFormat format); // Create a texture.
extern RendTexture  rend_texture_create_from_data(RendRenderer renderer, const void *data, uint32_t width, uint32_t height, RendFormat format); // Create texture and copy data to it immediately.
extern void         rend_texture_destroy(RendRenderer renderer, RendTexture *texture); // Destroy texture. Does not deallocate it's memory from the bump allocator.
extern void         rend_texture_copy_data(RendRenderer renderer, RendTexture *texture, const void *data, size_t size); // Copy data to texture. MUST be the same size as the format expects (width x height x sizeof format).
extern void         rend_texture_copy_buffer(RendRenderer renderer, RendTexture *texture, RendBuffer *buffer); // Copy buffer to texture. Outside a frame: transfer-queue one-shot. Inside a frame: same as rend_cmd_copy_buffer_to_texture.
extern void         rend_texture_read(RendRenderer renderer, RendTexture *texture, void *dst, size_t size); // Copy texture to host. Tight-packed texels. Outside a frame and pass. MUST be width x height x sizeof format.

/* Create, Configure, and Destroy Rendering Pipelines */
extern RendPipeline rend_pipeline_create_graphics_spirv(RendRenderer renderer, uint8_t *vertex_bytes, size_t vertex_size, uint8_t *frag_bytes, size_t frag_size, const RendVertexBinding *vertex_bindings, uint32_t vertex_binding_count, const RendVertexAttributes *vertex_attributes, uint32_t vertex_attribute_count, const RendPushConstantInfo *push_constants, uint32_t push_constant_count,  RendPolygonMode polygon_mode, RendCullMode cull_mode, RendTopology topology, RendFormat color_format, bool depth_test_enable); // Create a pipeline for a renderer using a configuration handle.
extern RendPipeline rend_pipeline_create_graphics_bindless_spirv(RendRenderer renderer, uint8_t *vertex_bytes, size_t vertex_size, uint8_t *frag_bytes, size_t frag_size, const RendPushConstantInfo *push_constants, uint32_t push_constant_count,  RendPolygonMode polygon_mode, RendCullMode cull_mode, RendTopology topology, RendFormat color_format, bool depth_test_enable); // Create a pipeline for a renderer using a configuration handle.
extern RendPipeline rend_pipeline_create_meshlet_spirv(RendRenderer renderer, uint8_t *meshlet_bytes, size_t meshlet_size, uint8_t *frag_bytes, size_t frag_size, const RendPushConstantInfo *push_constants, uint32_t push_constant_count, RendPolygonMode polygon_mode, RendCullMode cull_mode, bool depth_test_enable); // Creates a meshlet rendering pipeline.
extern RendPipeline rend_pipeline_create_compute_spirv(RendRenderer renderer, const uint8_t *compute_bytes, size_t compute_size, const RendPushConstantInfo *push_constants, uint32_t push_constant_count); // Create a compute pipeline.
extern void         rend_pipeline_set_blend(RendPipeline pipeline, bool blend);

/* Commands */
extern void rend_cmd_render_begin(RendRenderer renderer, float r, float g, float b, float a); // Pass on color_target(). Clear to RGBA.
/* Resume the acquired default color after a completed pass/blit in this frame.
 * Inside a frame, outside a pass. Loads color, clears depth, transitions the
 * color target from transfer destination when needed. End with render_end.
 * Never loads a previous frame or a target from a failed acquire. */
extern void rend_cmd_render_begin_preserve(RendRenderer renderer);
extern void rend_cmd_render_begin_texture(RendRenderer renderer, RendTexture *texture); // Pass on a caller texture (not the borrowed color_target unless you mean to).
extern void rend_cmd_render_end(RendRenderer renderer); // End render pass.
extern void rend_cmd_render_end_texture(RendRenderer renderer, RendTexture *texture); // End pass on a caller texture. Ready to sample.
extern void rend_cmd_bind_pipeline(RendPipeline pipeline); // Bind the pipeline to this frame.
extern void rend_cmd_bind_vertex_buffer(RendPipeline pipeline, uint32_t binding, RendBuffer buffer, size_t offset); // Bind vertex buffer to this graphics pipeline.
extern void rend_cmd_bind_index_buffer(RendPipeline pipeline, RendBuffer buffer, size_t offset, RendIndexType index_type); // Bind index buffer to this graphics pipeline.
extern void rend_cmd_push_constants(RendPipeline pipeline, void *push_data, size_t size); // Send push constants to this pipeline / command buffer.
extern void rend_cmd_dispatch(RendPipeline pipeline, uint32_t x, uint32_t y, uint32_t z); // Dispatch compute commands to group with dimensions x, y, z!
extern void rend_cmd_draw(RendPipeline pipeline, size_t count, uint32_t instance_count); // Calls draw command on the pipeline.
extern void rend_cmd_draw_indexed(RendPipeline pipeline, uint32_t index_count, uint32_t first_index, int32_t vertex_offset, uint32_t instance_count); // Draw the pipeline using indexed rendering.
extern void rend_cmd_copy_buffer_to_texture(RendRenderer renderer, RendTexture *texture, RendBuffer *buffer); // Buffer to texture on the frame cmdbuf. Compute-write barrier, then copy. Leaves texture TRANSFER_DST. Inside a frame, outside a pass.
extern void rend_cmd_blit(RendRenderer renderer, RendTexture *src, RendTexture *dst, uint32_t src_x, uint32_t src_y, uint32_t src_w, uint32_t src_h, uint32_t dst_x, uint32_t dst_y, uint32_t dst_w, uint32_t dst_h); // Blit. dst may be color_target(). Inside a frame, outside a pass. color_target stays TRANSFER_DST for present.

enum RendBackendType_t {
    REND_BACKEND_AUTO = 0, 
    REND_BACKEND_VULKAN_14,
	REND_BACKEND_DIRECTX_12, /* Unavailable stub; creation returns NULL. */
	REND_BACKEND_METAL_4,    /* Unavailable stub; creation returns NULL. */
    REND_BACKEND_COUNT 
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
 * 1.0.4 - preserving default-color pass after transfer blits
 * 1.0.3 - @vasco - frame_begin no longer sticks in_frame or burns timeline on OUT_OF_DATE
 * 1.0.4 - @vasco - vulkan backend collapsed; renderer create fails cleanly
 * 1.0.5 - @vasco - vulkan host linear arena
 * 1.0.6 - @vasco - texture destroy waits idle; offscreen end transitions after pass
 * 1.0.7 - @vasco - resize recreates swapchain via oldSwapchain; surface extent before acquire
 * 1.0.8 - @vasco - offscreen pass clears new images; texture destroy not during a frame
 * 1.0.9 - @vasco - vulkan host alloc callbacks actually free (resize was exhausting 1MB arena)
 * 1.0.10 - @vasco - blit outside pass; texture layout after copy and blit
 * 1.1.0 - @vasco - rend_renderer_create_offscreen: no window, no swapchain
 * 1.1.1 - @vasco - rend_texture_read: image to host via staging buffer
 * 1.1.2 - @vasco - swapchain picks non-opaque composite alpha when offered
 * 1.2.0 - @vasco - rend_renderer_read: offscreen color target to host
 * 1.2.1 - @vasco - vsync, present queue, exclusive buffers, create cleanup, blend/indirect/delta
 * 1.3.0 - @vasco - rend_renderer_color_target: borrowed default color + extent/format
 * 1.4.0 - @vasco - rend_buffer_mapped; texture_id / set_blend; reuse staging
 * 1.5.0 - @vasco - in-frame buffer to texture; blit-only present
 * 1.5.1 - @vasco - no per-frame surface query; SUBOPTIMAL recreates once; host arena stays
 * 1.5.2 - @vasco - present on graphics family; extra swapchain image; OPAQUE composite first
 * 1.6.0 - @vasco - REND_BACKEND_CPU; AUTO falls back; create_graphics_c; PEAK_VULKAN not auto-defined
 * 1.6.1 - @vasco - CPU raster: axis-aligned quads, incremental edges, word clear
 * 1.6.2 - @vasco - CPU raster: runtime SSE/SSE2/AVX
 * 1.6.3 - @vasco - REND_VK_ARENA_GROW / MIN / SWAPCHAIN_EXTRA
 * 1.6.4 - @vasco - REND_VK_COMPOSITE_PREFER_ALPHA
 * 1.6.5 - @vasco - vulkan host alloc via peak_aligned_alloc
 * 1.6.6 - @vasco - prefer integrated GPU when surface is set
 * 1.6.7 - @vasco - grow-only pass depth so texture passes can exceed swapchain size
 * 1.6.8 - @vasco - exact-size swap/texture depths; NEAREST sampler; arena honors image alignment
 * 1.6.9 - @vasco - LINEAR mag/min sampler; CLAMP_TO_EDGE
 * 1.6.10 - @vasco - texture_destroy uses ctx without REND_DEBUG
 * 1.6.11 - @vasco - device score: vulkan 1.3 floor, implicit transfer queues (AMD)
 * 2.0.0 - Vulkan default host allocator; reclaimable depth memory; remove CPU backend and unused frame lifetime
 */

#endif /* REND_H */

#if defined(REND_IMPLEMENTATION) && !defined(REND_IMPLEMENTATION_INCLUDED)
#define REND_IMPLEMENTATION_INCLUDED

/* Honor independently selected Peak implementation on a delayed include. */
#include "Peak.h"
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* --- Embedded Rend/rend_internal.h: private records and diagnostics --- */

#if defined(REND_DEBUG)
#ifndef P_LOG_DEBUG_ENABLED
#define P_LOG_DEBUG_ENABLED 1
#endif
#define RASSERT_N(_1, _2, N, ...) N
#define RASSERT(...) RASSERT_N(__VA_ARGS__, RASSERT2, RASSERT1)(__VA_ARGS__)
#define RASSERT1(a) assert(a)
#define RASSERT2(a, s) assert((a) && (s))
#else
#define RASSERT(...) ((void)0)
#endif

/* Explicit opt-in preserves custom allocators in other consumers.
 * These calls bypass global malloc/free macros, hence exactly one event. */
#if defined(REND_DEBUG_MEMORY)
#define rmalloc(size) peak_debug_malloc_domain_impl((size), PEAK_MEMORY_DRIVER, __FILE__, __LINE__, __func__)
#define rrealloc(ptr, size) peak_debug_realloc_domain_impl((ptr), (size), PEAK_MEMORY_DRIVER, __FILE__, __LINE__, __func__)
#define rfree(ptr) peak_debug_free_domain_impl((ptr), PEAK_MEMORY_DRIVER, __FILE__, __LINE__, __func__)
#else
#define rmalloc malloc
#define rrealloc(ptr, size) realloc((ptr), (size))
#define rfree free
#endif

#define REND_TODO \
    do { \
        fprintf(stderr, "REND TODO: %s() in %s:%d\n", __func__, __FILE__, __LINE__); \
        abort(); \
    } while(0)

#define REND__CRASH(...)\
    PERROR(__VA_ARGS__);\
    exit(1);

#define REND__WARN(...) PWARN("[REND] "__VA_ARGS__);


#include <stdint.h>

typedef void* RendContextHandle;

enum RendPipelineType {
    REND__PIPELINE_GRAPHICS,
    REND__PIPELINE_COMPUTE,
    REND__PIPELINE_MESH,
};

typedef struct rend_pipeline_config_t {

    const RendVertexBinding *vertex_bindings;
    const RendVertexAttributes *vertex_attributes;
    const RendPushConstantInfo *push_constants;

    uint32_t vertex_binding_count;
    uint32_t vertex_attribute_count;
    uint32_t push_constant_count;
    
    uint16_t color_format;
    uint16_t depth_format;

    uint16_t polygon_mode;
    uint16_t cull_mode; 
    uint16_t topology;
    
    uint8_t depth_test_enable;
} Rend__PipelineConfig;

typedef struct {
    RendContextHandle (*renderer_create)(PeakWindow *window, RendBindingInfo *bind_info, bool vsync);
    RendContextHandle (*renderer_create_offscreen)(uint32_t width, uint32_t height, RendFormat format, RendBindingInfo *bind_info);

    void (*renderer_destroy)(RendContextHandle);

    bool (*renderer_frame_begin)(RendContextHandle);
    void (*renderer_frame_end)(RendContextHandle, float *delta);
    RendTexture *(*color_target)(RendContextHandle handle);

    void (*descriptor_write_buffer)(RendContextHandle handle, RendBuffer ubo, uint32_t binding, uint32_t slot, uint32_t offset, uint32_t size, bool is_ubo);
    void (*descriptor_write_texture)(RendContextHandle handle, RendTexture *texture, uint32_t binding, uint32_t slot);

    RendBuffer   (*buffer_create)(RendContextHandle handle, size_t size, RendBufferType type, bool gpu);
    void         (*buffer_destroy)(RendBuffer *buffer);
    void         (*buffer_copy)(RendContextHandle handle, RendBuffer *dest, size_t dest_offset, RendBuffer *src, size_t src_offset, size_t bytes);
    
    RendTexture  (*texture_create)(RendContextHandle handle, uint32_t width, uint32_t height, uint32_t depth, uint32_t mip_levels, uint32_t layers, RendFormat format);
    void         (*texture_destroy)(RendContextHandle handle, RendTexture *tex);
    void         (*texture_copy_buffer)(RendContextHandle handle, RendTexture *texture, RendBuffer *buffer);
    void         (*texture_copy_to_buffer)(RendContextHandle handle, RendTexture *texture, RendBuffer *buffer);
    void         (*texture_blit)(RendContextHandle handle, RendTexture *src, RendTexture *dst, uint32_t src_x, uint32_t src_y, uint32_t src_w, uint32_t src_h, uint32_t dst_x, uint32_t dst_y, uint32_t dst_w, uint32_t dst_h);

    bool (*pipeline_create)(RendContextHandle, RendPipeline, Rend__PipelineConfig, uint8_t type, const uint8_t *shader1, size_t bytes1, const uint8_t *shader2, size_t bytes2, const uint8_t *shader3, size_t bytes3);
    void (*pipeline_bind)(RendPipeline);
    void (*pipeline_push_constants)(RendPipeline pipeline, void *push_data, size_t size);

    void (*pipeline_bind_vertex_buffer)(RendPipeline pipeline, uint32_t binding, RendBuffer buffer, size_t offset);
    void (*pipeline_bind_index_buffer)(RendPipeline pipeline, RendBuffer buffer, size_t offset, RendIndexType index_type);

    void (*pipeline_dispatch)(RendPipeline pipeline, uint32_t x, uint32_t y, uint32_t z);
    void (*pipeline_draw)(RendPipeline, size_t count, uint32_t instance_count);
    void (*pipeline_draw_indexed)(RendPipeline pipeline, uint32_t index_count, uint32_t first_index, int32_t vertex_offset, uint32_t instance_count);
    void (*pipeline_set_blend)(RendPipeline, bool);

    void (*renderer_render_pass_begin)(RendContextHandle handle, float r, float g, float b, float a);
    void (*renderer_render_pass_begin_preserve)(RendContextHandle);
    void (*renderer_render_pass_begin_texture)(RendContextHandle, RendTexture*);
    void (*renderer_render_pass_end)(RendContextHandle handle);
    void (*renderer_render_pass_end_texture)(RendContextHandle handle, RendTexture*);
} RendVTable;


/* NOTE(vasco): Its simpler if backeds all use a uniform struct than
 * every single one having to basically redefine the same thing
 */





struct rend_pipeline_t {
    struct rend_pipeline_t *next;
    struct rend_pipeline_t *prev;
    void *backend_ctx;
    uint32_t idx; // index into renderers internal array of pipelines
    uint32_t frame_count;
    uint8_t backend;
    uint8_t type;
};

struct rend_renderer_t {
    struct rend_renderer_t *next;
    struct rend_renderer_t *prev;

    struct rend_pipeline_t *pipeline_head;
    RendContextHandle context; // backend specific internal data
    PeakWindow *window;
    uint64_t frame_count;

    RendBindingInfo bind_info;
    RendBuffer staging;

    uint32_t texture_binding;
    uint32_t texture_count;
    uint32_t ubo_binding;
    uint32_t ubo_count;

    uint8_t backend;
    uint8_t in_frame;
    uint8_t in_pass;
    uint8_t vsync;
};

/* --- Embedded Rend/rend.c: dispatch (Vulkan precedes its table) --- */
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifdef PEAK_VULKAN
/* --- Embedded Rend/rend_vk14.c: Vulkan backend --- */
/*
 * Vulkan 1.4 backend. Per-renderer context is RendVk14Context.
 * Instance, device, and version-agnostic Vulkan helpers: rend_vk_internal.c.
 *
 * * 1.0.3 - @vasco - backend functions take RendContextHandle
 * * 1.0.7 - @vasco - resize: oldSwapchain recreate, surface extent, present OUT_OF_DATE
 * * 1.0.8 - @vasco - offscreen CLEAR on new images; texture destroy not in-frame
 * * 1.2.1 - @vasco - vsync, present queue, exclusive buffers, create cleanup, blend/indirect/delta
 * * 1.3.0 - @vasco - wrap swapchain/offscreen images as borrowed RendTexture
 * * 1.4.0 - @vasco - window pass uses color_target; drop renderer_read
 * * 1.5.0 - @vasco - in-frame copy_buffer; blit-only present barrier
 * * 1.5.1 - @vasco - no per-frame surface query; SUBOPTIMAL recreates once; host arena stays
 * * 1.5.2 - @vasco - present on graphics family; extra swapchain image; OPAQUE composite first
 * * 1.6.7 - @vasco - grow-only pass depth; texture pass no longer uses swapchain-sized depth
 * * 1.6.8 - @vasco - exact-size swap vs texture depth; NEAREST sampler; arena alignment
 * * 1.6.9 - @vasco - LINEAR mag/min sampler; CLAMP_TO_EDGE
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

/* --- Embedded Rend/rend_vk_internal.c: shared Vulkan helpers --- */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>


#define CHECK_VK_RESULT(r) do { \
	VkResult _rend_vk_r = (r); \
	if (_rend_vk_r != VK_SUCCESS) { \
		RASSERT(_rend_vk_r == VK_SUCCESS, "vulkan call failed"); \
		return false; \
	} \
} while (0)

typedef struct RendVkImage RendVkImage;
typedef struct RendVkArenaAllocator RendVkArenaAllocator;

typedef struct {
	VkSurfaceCapabilitiesKHR capabilities;
	VkSurfaceFormatKHR *format;
	VkPresentModeKHR *present_modes;
	uint32_t format_count;
	uint32_t present_mode_count;
} RendVkSwapchainSupport;

typedef struct {
	VkDevice logical_device;

	VkPhysicalDeviceProperties properties;
	VkPhysicalDeviceMemoryProperties memory;
	VkPhysicalDevice physical_device;
	VkPhysicalDeviceFeatures features;

	VkSurfaceKHR surface;
	RendVkSwapchainSupport swapchain_support;
	VkFormat depth_format;

	VkQueue graphics_queue;
	VkQueue present_queue;
	VkQueue compute_queue;
	VkQueue transfer_queue;
	uint32_t graphics_family_index;
	uint32_t present_family_index;
	uint32_t compute_family_index;
	uint32_t transfer_family_index;

	uint32_t host_index;
	uint32_t device_index;
} RendVkDevice;

typedef struct RendVkPage {
	RendMemory memory;
	size_t head;
} RendVkPage;

typedef struct RendVkPagedArena {
	RendVkPage *page_darr;
	uint32_t capacity;
	uint32_t elements;
} RendVkPagedArena;

struct RendVkArenaAllocator {
	RendVkPagedArena *mem_arenas;
	VkDevice logical_device;
	VkPhysicalDevice physical_device;
	VkDeviceSize gpu_alignment;
	VkDeviceSize block_min_size;
	uint32_t heap_index_count;
	VkPhysicalDeviceMemoryProperties properties;
};

struct RendVkImage {
	VkDevice logical_device;
	VkImage handle;
	VkImageView view;
	VkDeviceMemory owned_memory; /* depth images; other images borrow arena memory */

	VkMemoryRequirements requirements;

	VkImageType img_type;
	uint32_t width, height;
	VkFormat format;
	VkImageTiling tiling;
	VkImageUsageFlags usage;
	uint32_t depth;
	uint32_t mip_levels;
	uint32_t layers;
	VkSampleCountFlags sample_count_flags;
	VkSharingMode sharing_mode;

	RendMemory *memory;
};

static VkInstance vk_instance = 0;
static VkDebugUtilsMessengerEXT vk_debug_messenger = 0;
static RendVkDevice vk_device = {0};
/* Always use Vulkan's default host allocator, including on destruction. */
static const VkAllocationCallbacks *const vk_allocator = NULL;

static VkFormat vk_format_from_rend_format[] = {
	[REND_FORMAT_R8_UNORM]           = VK_FORMAT_R8_UNORM,
	[REND_FORMAT_R8G8_UNORM]         = VK_FORMAT_R8G8_UNORM,
	[REND_FORMAT_R8G8B8A8_UNORM]     = VK_FORMAT_R8G8B8A8_UNORM,
	[REND_FORMAT_B8G8R8A8_UNORM]     = VK_FORMAT_B8G8R8A8_UNORM,
	[REND_FORMAT_R8G8B8A8_SRGB]      = VK_FORMAT_R8G8B8A8_SRGB,
	[REND_FORMAT_B8G8R8A8_SRGB]      = VK_FORMAT_B8G8R8A8_SRGB,
	[REND_FORMAT_R32_SFLOAT]         = VK_FORMAT_R32_SFLOAT,
	[REND_FORMAT_R32G32_SFLOAT]      = VK_FORMAT_R32G32_SFLOAT,
	[REND_FORMAT_R32G32B32_SFLOAT]   = VK_FORMAT_R32G32B32_SFLOAT,
	[REND_FORMAT_R32G32B32A32_SFLOAT]= VK_FORMAT_R32G32B32A32_SFLOAT,
	[REND_FORMAT_R16_SFLOAT]         = VK_FORMAT_R16_SFLOAT,
	[REND_FORMAT_R16G16_SFLOAT]      = VK_FORMAT_R16G16_SFLOAT,
	[REND_FORMAT_R16G16B16A16_SFLOAT]= VK_FORMAT_R16G16B16A16_SFLOAT,
	[REND_FORMAT_R32_UINT]           = VK_FORMAT_R32_UINT,
	[REND_FORMAT_R32_SINT]           = VK_FORMAT_R32_SINT,
	[REND_FORMAT_R32G32B32A32_UINT]  = VK_FORMAT_R32G32B32A32_UINT,
	[REND_FORMAT_R16G16B16A16_UINT]  = VK_FORMAT_R16G16B16A16_UINT,
	[REND_FORMAT_R8G8B8A8_UINT]      = VK_FORMAT_R8G8B8A8_UINT,
	[REND_FORMAT_D32_SFLOAT]         = VK_FORMAT_D32_SFLOAT,
	[REND_FORMAT_D24_UNORM_S8_UINT]  = VK_FORMAT_D24_UNORM_S8_UINT,
	[REND_FORMAT_D32_SFLOAT_S8_UINT] = VK_FORMAT_D32_SFLOAT_S8_UINT,
};

static VkPrimitiveTopology vk_topology[] = {
	[REND_TOPOLOGY_TRIANGLE_LIST]   = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
	[REND_TOPOLOGY_TRIANGLE_STRIP]  = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
	[REND_TOPOLOGY_LINE_LIST]       = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
	[REND_TOPOLOGY_LINE_STRIP]      = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
	[REND_TOPOLOGY_POINT_LIST]      = VK_PRIMITIVE_TOPOLOGY_POINT_LIST
};

static VkPolygonMode vk_polymode[] = {
	[REND_POLYGON_MODE_FILL]  = VK_POLYGON_MODE_FILL,
	[REND_POLYGON_MODE_LINE]  = VK_POLYGON_MODE_LINE,
	[REND_POLYGON_MODE_POINT] = VK_POLYGON_MODE_POINT,
};

static VkCullModeFlags vk_cullflags[] = {
	[REND_CULL_MODE_NONE]           = VK_CULL_MODE_NONE,
	[REND_CULL_MODE_FRONT]          = VK_CULL_MODE_FRONT_BIT,
	[REND_CULL_MODE_BACK]           = VK_CULL_MODE_BACK_BIT,
	[REND_CULL_MODE_FRONT_AND_BACK] = VK_CULL_MODE_FRONT_AND_BACK,
};

static const VkBufferUsageFlags vk_buffer_usage[] = {
	[REND_BUFFER_VERTEX]   = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
	[REND_BUFFER_INDEX]    = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
	[REND_BUFFER_UNIFORM]  = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
	[REND_BUFFER_STORAGE]  = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
	[REND_BUFFER_INDIRECT] = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
	[REND_BUFFER_TRANSFER] = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
};

static const VkShaderStageFlagBits vk_pipeline_stages[][3] = {
	[REND__PIPELINE_GRAPHICS] = { VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT },
	[REND__PIPELINE_COMPUTE]  = { VK_SHADER_STAGE_COMPUTE_BIT },
	[REND__PIPELINE_MESH]     = { VK_SHADER_STAGE_MESH_BIT_EXT, VK_SHADER_STAGE_FRAGMENT_BIT },
};

/* --- device --- */

static bool
rend_vk_device_query_swapchain_support(RendVkDevice *device)
{
	RASSERT(device->physical_device, "Invalid device pointer.");

	CHECK_VK_RESULT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->physical_device, device->surface, &device->swapchain_support.capabilities));

	CHECK_VK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR(device->physical_device, device->surface, &device->swapchain_support.format_count, VK_NULL_HANDLE));
	if (device->swapchain_support.format_count != 0) {
		if (!device->swapchain_support.format) {
			device->swapchain_support.format = rmalloc(device->swapchain_support.format_count * sizeof(*device->swapchain_support.format));
		}
		CHECK_VK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR(device->physical_device, device->surface, &device->swapchain_support.format_count, device->swapchain_support.format));
	}

	CHECK_VK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(device->physical_device, device->surface, &device->swapchain_support.present_mode_count, VK_NULL_HANDLE));
	if (device->swapchain_support.present_mode_count != 0) {
		if (!device->swapchain_support.present_modes) {
			device->swapchain_support.present_modes = rmalloc(device->swapchain_support.present_mode_count * sizeof(*device->swapchain_support.present_modes));
		}
		CHECK_VK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(device->physical_device, device->surface, &device->swapchain_support.present_mode_count, device->swapchain_support.present_modes));
	}
	return true;
}

static uint32_t
rend_vk_device_score_default(RendVkDevice *device, RendSpecs minimum_specs, const char **required_extensions, uint32_t required_extension_count)
{
	uint32_t score;
	uint32_t q_family_count;
	uint8_t min_transfer_score;
	uint32_t i;

	score = 0;

	/* Backend uses 1.3 core (dynamic rendering, sync2). Instance is 1.4.
	 * Vega / older RADV still advertise 1.3 on the device. */
	if (device->properties.apiVersion < VK_API_VERSION_1_3) {
		PDEBUG("reject %s: vulkan %u.%u (need 1.3)",
			device->properties.deviceName,
			VK_API_VERSION_MAJOR(device->properties.apiVersion),
			VK_API_VERSION_MINOR(device->properties.apiVersion));
		return 0;
	}
	if (!device->features.shaderInt64) {
		PDEBUG("reject %s: no shaderInt64", device->properties.deviceName);
		return 0;
	}

	device->graphics_family_index = UINT32_MAX;
	device->present_family_index = UINT32_MAX;
	device->compute_family_index = UINT32_MAX;
	device->transfer_family_index = UINT32_MAX;

	if (minimum_specs.discrete_gpu) {
		if (device->properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
			return 0;
		}
	}

	q_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device->physical_device, &q_family_count, VK_NULL_HANDLE);

	{
		VkQueueFamilyProperties q_family[q_family_count];

		vkGetPhysicalDeviceQueueFamilyProperties(device->physical_device, &q_family_count, q_family);

		min_transfer_score = 255;
		for (i = 0; i < q_family_count; i++) {
			uint8_t transfer_score;
			VkBool32 supports_present;

			transfer_score = 0;
			if (q_family[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				device->graphics_family_index = i;
				transfer_score++;
			}

			if (q_family[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
				device->compute_family_index = i;
				transfer_score++;
			}

			/* TRANSFER_BIT is optional on graphics/compute families (Vulkan spec).
			 * AMD often omits it; NVIDIA usually sets it. */
			if (q_family[i].queueFlags & (VK_QUEUE_TRANSFER_BIT | VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) {
				if (transfer_score <= min_transfer_score) {
					min_transfer_score = transfer_score;
					device->transfer_family_index = i;
				}
			}

			if (device->surface) {
				supports_present = VK_FALSE;
				CHECK_VK_RESULT(vkGetPhysicalDeviceSurfaceSupportKHR(device->physical_device, i, device->surface, &supports_present));
				if (supports_present) {
					if (device->present_family_index == UINT32_MAX)
						device->present_family_index = i;
					if (q_family[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
						device->present_family_index = i;
				}
			}
		}
	}

	if ((minimum_specs.graphics && device->graphics_family_index == UINT32_MAX) ||
			(minimum_specs.present && device->present_family_index == UINT32_MAX) ||
			(minimum_specs.compute && device->compute_family_index == UINT32_MAX) ||
			(minimum_specs.transfer && device->transfer_family_index == UINT32_MAX)) {
		PDEBUG("reject %s: missing queue (g=%u p=%u c=%u t=%u)",
			device->properties.deviceName,
			device->graphics_family_index,
			device->present_family_index,
			device->compute_family_index,
			device->transfer_family_index);
		return 0;
	}

	if (device->surface) {
		if (!rend_vk_device_query_swapchain_support(device)) {
			PDEBUG("reject %s: swapchain query failed", device->properties.deviceName);
			return 0;
		}
		if (device->present_family_index == UINT32_MAX) {
			PDEBUG("reject %s: no present queue", device->properties.deviceName);
			return 0;
		}
		if (device->swapchain_support.format_count < 1 || device->swapchain_support.present_mode_count < 1) {
			PDEBUG("reject %s: no swapchain formats/modes", device->properties.deviceName);
			return 0;
		}
	} else {
		device->present_family_index = device->graphics_family_index;
	}

	if (required_extensions) {
		uint32_t available_extentions_count;
		VkExtensionProperties *available_extentions;

		available_extentions_count = 0;
		available_extentions = NULL;
		CHECK_VK_RESULT(vkEnumerateDeviceExtensionProperties(device->physical_device, VK_NULL_HANDLE, &available_extentions_count, VK_NULL_HANDLE));

		if (available_extentions_count != 0) {
			bool overall_found;
			uint32_t j;

			available_extentions = rmalloc(available_extentions_count * sizeof(*available_extentions));
			if (!available_extentions)
				return 0;
			if (vkEnumerateDeviceExtensionProperties(device->physical_device, VK_NULL_HANDLE, &available_extentions_count, available_extentions) != VK_SUCCESS) {
				rfree(available_extentions);
				return 0;
			}

			overall_found = true;
			for (i = 0; i < required_extension_count; ++i) {
				bool found;

				found = false;
				for (j = 0; j < available_extentions_count; ++j) {
					if (strcmp(required_extensions[i], available_extentions[j].extensionName) == 0) {
						found = true;
						break;
					}
				}
				if (!found) {
					overall_found = false;
					break;
				}
			}

			rfree(available_extentions);
			if (!overall_found) {
				PDEBUG("reject %s: missing device extension", device->properties.deviceName);
				return 0;
			}
		}
	}

	if (minimum_specs.sampler_anisotropy && !device->features.samplerAnisotropy) {
		PDEBUG("reject %s: no samplerAnisotropy", device->properties.deviceName);
		return 0;
	}

	score = 10;
	if (device->surface) {
		if (device->properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
			score += 1000;
	} else if (device->properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		score += 1000;

	return score;
}

static bool
rend_vk_device_create(VkSurfaceKHR surface, RendSpecs specs, RendVkDevice *out_device)
{
	uint32_t device_count;
	const char *extension_names[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	uint32_t best_score;
	RendVkDevice best_device;
	uint32_t i;
	bool present_shares_graphics_q;
	bool transfer_shares_graphics_q;
	uint32_t index_count;
	uint8_t index;
	float queue_priority[2];
	VkPhysicalDeviceFeatures device_features;
	VkPhysicalDeviceVulkan13Features vk13_features;
	VkPhysicalDeviceVulkan12Features vk12_features;
	VkPhysicalDeviceVulkan11Features vk11_features;
	VkDeviceCreateInfo device_create_info;
	const char *extention_names;
	VkPhysicalDeviceMemoryProperties mem_props;
	VkMemoryPropertyFlags host_flags;

	assert(out_device);

	device_count = 0;
	CHECK_VK_RESULT(vkEnumeratePhysicalDevices(vk_instance, &device_count, VK_NULL_HANDLE));
	if (device_count == 0) {
		PFATAL("No GPU with Vulkan support found!");
		return false;
	}

	{
		VkPhysicalDevice physical_devices[device_count];

		CHECK_VK_RESULT(vkEnumeratePhysicalDevices(vk_instance, &device_count, physical_devices));

		best_score = 1;
		best_device = (RendVkDevice){0};

		PDEBUG("DEVICE                SCORE    API");
		for (i = 0; i < device_count; i++) {
			RendVkDevice scoring;
			uint32_t dev_score;

			scoring = (RendVkDevice){0};
			scoring.surface = surface;
			scoring.physical_device = physical_devices[i];
			vkGetPhysicalDeviceProperties(physical_devices[i], &scoring.properties);
			vkGetPhysicalDeviceFeatures(physical_devices[i], &scoring.features);
			vkGetPhysicalDeviceMemoryProperties(physical_devices[i], &scoring.memory);

			dev_score = rend_vk_device_score_default(&scoring, specs, extension_names, 1);
			PDEBUG("%-20.20s  %5d  %u.%u.%u", scoring.properties.deviceName, dev_score,
				VK_API_VERSION_MAJOR(scoring.properties.apiVersion),
				VK_API_VERSION_MINOR(scoring.properties.apiVersion),
				VK_API_VERSION_PATCH(scoring.properties.apiVersion));
			if (dev_score >= best_score) {
				if (best_device.swapchain_support.format) {
					rfree(best_device.swapchain_support.format);
				}
				if (best_device.swapchain_support.present_modes) {
					rfree(best_device.swapchain_support.present_modes);
				}
				best_score = dev_score;
				best_device = scoring;
			} else {
				if (scoring.swapchain_support.format) {
					rfree(scoring.swapchain_support.format);
				}
				if (scoring.swapchain_support.present_modes) {
					rfree(scoring.swapchain_support.present_modes);
				}
			}
		}
	}

	if (best_score > 1) {
		PDEBUG("Driver version %d.%d.%d", VK_VERSION_MAJOR(best_device.properties.driverVersion), VK_VERSION_MINOR(best_device.properties.driverVersion), VK_VERSION_PATCH(best_device.properties.driverVersion));
		PDEBUG("Vulkan API version %d.%d.%d", VK_VERSION_MAJOR(best_device.properties.apiVersion), VK_VERSION_MINOR(best_device.properties.apiVersion), VK_VERSION_PATCH(best_device.properties.apiVersion));
	}

	if (!best_device.physical_device) {
		PERROR("No physical devices were found that meet specs!");
		return false;
	}

	*out_device = best_device;

	PDEBUG("Graphics Family Index: %u", out_device->graphics_family_index);
	PDEBUG("Present Family Index: %u", out_device->present_family_index);
	PDEBUG("Compute Family Index: %u", out_device->compute_family_index);
	PDEBUG("Transfer Family Index: %u", out_device->transfer_family_index);

	present_shares_graphics_q = out_device->present_family_index == out_device->graphics_family_index;
	transfer_shares_graphics_q = out_device->transfer_family_index == out_device->graphics_family_index;

	index_count = 1;
	if (!present_shares_graphics_q)
		index_count++;
	if (!transfer_shares_graphics_q)
		index_count++;

	{
		uint32_t indices[index_count];
		VkDeviceQueueCreateInfo q_create_info[index_count];

		index = 0;
		indices[index++] = out_device->graphics_family_index;
		if (!present_shares_graphics_q) {
			indices[index++] = out_device->present_family_index;
		}
		if (!transfer_shares_graphics_q) {
			indices[index++] = out_device->transfer_family_index;
		}

		queue_priority[0] = 1.0f;
		queue_priority[1] = 1.0f;
		for (i = 0; i < index_count; i++) {
			q_create_info[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			q_create_info[i].queueFamilyIndex = indices[i];
			q_create_info[i].queueCount = 1;
			q_create_info[i].flags = 0;
			q_create_info[i].pNext = 0;
			q_create_info[i].pQueuePriorities = queue_priority;
		}

		device_features = (VkPhysicalDeviceFeatures){0};
		device_features.samplerAnisotropy = specs.sampler_anisotropy && out_device->features.samplerAnisotropy;
		device_features.fillModeNonSolid = out_device->features.fillModeNonSolid;
		device_features.shaderInt64 = out_device->features.shaderInt64;

		vk13_features = (VkPhysicalDeviceVulkan13Features){0};
		vk13_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
		vk13_features.dynamicRendering = VK_TRUE;
		vk13_features.synchronization2 = VK_TRUE;

		vk12_features = (VkPhysicalDeviceVulkan12Features){0};
		vk12_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
		vk12_features.timelineSemaphore = VK_TRUE;
		vk12_features.descriptorBindingPartiallyBound = VK_TRUE;
		vk12_features.bufferDeviceAddress = VK_TRUE;
		vk12_features.scalarBlockLayout = VK_TRUE;
		vk12_features.pNext = &vk13_features;

		vk11_features = (VkPhysicalDeviceVulkan11Features){0};
		vk11_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
		vk11_features.shaderDrawParameters = VK_TRUE;
		vk11_features.pNext = &vk12_features;

		device_create_info = (VkDeviceCreateInfo){ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
		device_create_info.pNext = &vk11_features;
		device_create_info.queueCreateInfoCount = index_count;
		device_create_info.pQueueCreateInfos = q_create_info;
		device_create_info.pEnabledFeatures = &device_features;
		device_create_info.enabledExtensionCount = 1;

		extention_names = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
		device_create_info.ppEnabledExtensionNames = &extention_names;

		device_create_info.enabledLayerCount = 0;
		device_create_info.ppEnabledLayerNames = 0;

		CHECK_VK_RESULT(vkCreateDevice(out_device->physical_device, &device_create_info, vk_allocator, &out_device->logical_device));
	}

	vkGetDeviceQueue(out_device->logical_device, out_device->graphics_family_index, 0, &out_device->graphics_queue);
	vkGetDeviceQueue(out_device->logical_device, out_device->present_family_index, 0, &out_device->present_queue);
	vkGetDeviceQueue(out_device->logical_device, out_device->transfer_family_index, 0, &out_device->transfer_queue);

	PDEBUG("GRAPHICS | PRESENT | COMPUTE | TRANSFER | DEVICE");
	PDEBUG("      %02d |      %02d |      %02d |       %02d | %s",
			out_device->graphics_family_index != UINT32_MAX,
			out_device->present_family_index != UINT32_MAX,
			out_device->compute_family_index != UINT32_MAX,
			out_device->transfer_family_index != UINT32_MAX,
			out_device->properties.deviceName);

	mem_props = out_device->memory;
	out_device->device_index = UINT32_MAX;
	for (i = 0; i < mem_props.memoryTypeCount; i++) {
		if ((mem_props.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
			out_device->device_index = i;
			break;
		}
	}

	host_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	out_device->host_index = UINT32_MAX;
	for (i = 0; i < mem_props.memoryTypeCount; i++) {
		if ((mem_props.memoryTypes[i].propertyFlags & host_flags) == host_flags) {
			out_device->host_index = i;
			break;
		}
	}

	PDEBUG("Device local heap: %2u", out_device->device_index);
	PDEBUG("Host mapped heap:  %2u", out_device->host_index);
	return true;
}

static void
rend_vk_device_destroy(RendVkDevice *device)
{
	RASSERT(device && device->logical_device, "Invalid or uninitialized device.");

	vkDeviceWaitIdle(device->logical_device);
	vkDestroyDevice(device->logical_device, vk_allocator);
	if (device->swapchain_support.format)
		rfree(vk_device.swapchain_support.format);
	if (device->swapchain_support.present_modes)
		rfree(vk_device.swapchain_support.present_modes);

	*device = (RendVkDevice){0};
}

static bool
rend_vk_device_detect_depth_format(RendVkDevice *device)
{
	const uint64_t candidate_count = 3;
	VkFormat candidates[] = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
	uint32_t flags;
	uint32_t i;
	VkFormatProperties properties;

	flags = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
	for (i = 0; i < candidate_count; i++) {
		vkGetPhysicalDeviceFormatProperties(device->physical_device, candidates[i], &properties);
		if ((properties.linearTilingFeatures & flags) == flags) {
			device->depth_format = candidates[i];
			return true;
		} else if ((properties.optimalTilingFeatures & flags) == flags) {
			device->depth_format = candidates[i];
			return true;
		}
	}

	return false;
}

/* --- arena --- */

static RendVkArenaAllocator
rend_vk_arena_create(VkDevice logical_device, VkPhysicalDevice physical_device, VkPhysicalDeviceLimits device_limits)
{
	RendVkArenaAllocator arena;
	VkPhysicalDeviceMemoryProperties mem_properties;
	VkDeviceSize alignment;
	uint32_t count;
	uint32_t u;

	arena = (RendVkArenaAllocator){
		.logical_device = logical_device,
		.physical_device = physical_device,
		.gpu_alignment = 0,
		.block_min_size = 0,
		.heap_index_count = 0,
		.mem_arenas = 0,
	};

	vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);
	arena.properties = mem_properties;

	count = mem_properties.memoryTypeCount;
	arena.heap_index_count = count;

	arena.mem_arenas = rmalloc(count * sizeof(*arena.mem_arenas));
	for (u = 0; u < count; ++u) {
		arena.mem_arenas[u].capacity = 2;
		arena.mem_arenas[u].elements = 0;
		arena.mem_arenas[u].page_darr = rmalloc(2 * sizeof(*arena.mem_arenas[u].page_darr));
		memset(arena.mem_arenas[u].page_darr, 0, 2 * sizeof(*arena.mem_arenas[u].page_darr));
	}

	alignment = device_limits.bufferImageGranularity;
	if (device_limits.nonCoherentAtomSize > alignment) {
		alignment = device_limits.nonCoherentAtomSize;
	}

	arena.gpu_alignment = alignment;
#ifdef REND_VK_ARENA_MIN
	arena.block_min_size = (VkDeviceSize)REND_VK_ARENA_MIN
		? (VkDeviceSize)REND_VK_ARENA_MIN : arena.gpu_alignment * 10;
#else
	arena.block_min_size = arena.gpu_alignment * 10;
#endif

	return arena;
}

static uint32_t
rend_vk_arena_add_page(RendVkArenaAllocator *arena, VkDeviceSize size, uint32_t heap_index, bool fit_to_alloc)
{
	VkDeviceSize new_arena_size;
	VkMemoryPropertyFlags properties;
	RendVkPage page;
	VkMemoryAllocateFlagsInfo flags_info;
	VkMemoryAllocateInfo alloc_info;
	VkResult res;
	RendVkPagedArena *mem_arena;
	uint32_t page_index;
	uint32_t new_capacity;
	void *new_darr;

	new_arena_size = fit_to_alloc ? size : size * (VkDeviceSize)REND_VK_ARENA_GROW;
	if (new_arena_size < size)
		new_arena_size = size;
	new_arena_size = (new_arena_size < arena->block_min_size) ? arena->block_min_size : new_arena_size;

	properties = arena->properties.memoryTypes[heap_index].propertyFlags;

	page = (RendVkPage){0};
	page.head = 0;

	flags_info = (VkMemoryAllocateFlagsInfo){
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
		.pNext = NULL,
		.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
		.deviceMask = 0
	};

	alloc_info = (VkMemoryAllocateInfo){
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = new_arena_size,
		.pNext = &flags_info,
		.memoryTypeIndex = heap_index
	};

	page.memory = (RendMemory){0};
	page.memory.offset = 0;
	page.memory.size = new_arena_size;

	res = vkAllocateMemory(arena->logical_device, &alloc_info, vk_allocator, (VkDeviceMemory *)&page.memory.device_memory);
	if (res != VK_SUCCESS) {
		return UINT32_MAX;
	}

	if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
		vkMapMemory(
				arena->logical_device,
				(VkDeviceMemory)page.memory.device_memory,
				0,
				new_arena_size,
				0,
				&page.memory.host_mapped_memory
				);
	}

	mem_arena = &arena->mem_arenas[heap_index];

	if (mem_arena->elements + 1 >= mem_arena->capacity) {
		new_capacity = mem_arena->capacity * 2;
		new_darr = rrealloc(mem_arena->page_darr, new_capacity * sizeof(*mem_arena->page_darr));
		if (!new_darr) {
			vkFreeMemory(arena->logical_device, (VkDeviceMemory)page.memory.device_memory, vk_allocator);
			return UINT32_MAX;
		}
		mem_arena->page_darr = new_darr;
		mem_arena->capacity = new_capacity;
	}

	page_index = mem_arena->elements;
	mem_arena->page_darr[mem_arena->elements++] = page;

	return page_index;
}

static RendMemory
rend_vk_arena_alloc(RendVkArenaAllocator *arena, VkDeviceSize size, VkDeviceSize alignment, uint32_t heap_index)
{
	RendVkPagedArena *mem_arena;
	VkMemoryPropertyFlags properties;
	VkDeviceSize align;
	VkDeviceSize aligned_size;
	VkDeviceSize aligned_head;
	int whole_page;
	uint32_t page_idx;
	uint32_t u;
	RendVkPage page;
	int valid;
	RendMemory memory;

	assert(heap_index < 32 && "Unusual heap index. Did you pass the memory type instead?");
	mem_arena = &arena->mem_arenas[heap_index];

	properties = arena->properties.memoryTypes[heap_index].propertyFlags;

	align = arena->gpu_alignment;
	if (alignment > align)
		align = alignment;
	if (align < 1)
		align = 1;
	aligned_size = (size + align - 1) & ~(align - 1);

	whole_page = !(properties & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	page_idx = UINT32_MAX;
	aligned_head = 0;
	for (u = 0; u < mem_arena->elements; ++u) {
		page = mem_arena->page_darr[u];
		valid = (whole_page) ? (page.head == 0) : 1;
		aligned_head = (page.head + align - 1) & ~(align - 1);
		if ((aligned_head + aligned_size <= page.memory.size) && valid) {
			page_idx = u;
			break;
		}
	}

	if (page_idx == UINT32_MAX) {
		page_idx = rend_vk_arena_add_page(arena, aligned_size + align, heap_index, whole_page);
		if (page_idx == UINT32_MAX) {
			REND__CRASH("[REND_VK] Arena page allocation failed!");
		}
		aligned_head = (mem_arena->page_darr[page_idx].head + align - 1) & ~(align - 1);
	}

	memory = (RendMemory){
		.device_memory = mem_arena->page_darr[page_idx].memory.device_memory,
		.size = aligned_size,
		.offset = aligned_head,
		.host_mapped_memory = 0,
		.heap_index = heap_index,
		.id = page_idx,
	};

	if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
		memory.host_mapped_memory = mem_arena->page_darr[page_idx].memory.host_mapped_memory + memory.offset;
	}

	mem_arena->page_darr[page_idx].head = aligned_head + aligned_size;
	return memory;
}

static void
rend_vk_arena_destroy(RendVkArenaAllocator *arena)
{
	size_t u;
	uint32_t p;
	RendVkPagedArena *mem_arena;
	RendMemory memory;

	if (arena->mem_arenas) {
		for (u = 0; u < arena->heap_index_count; ++u) {
			mem_arena = &arena->mem_arenas[u];
			for (p = 0; p < mem_arena->elements; ++p) {
				memory = mem_arena->page_darr[p].memory;
				if (memory.offset == 0) {
					if (memory.host_mapped_memory) {
						vkUnmapMemory(arena->logical_device, (VkDeviceMemory)memory.device_memory);
					}
					vkFreeMemory(arena->logical_device, (VkDeviceMemory)memory.device_memory, vk_allocator);
				} else {
					PWARN("[REND_VK] Attempted to free memory with an offset!");
				}
			}
			if (mem_arena->page_darr) {
				rfree(mem_arena->page_darr);
				mem_arena->page_darr = 0;
			}
		}
		rfree(arena->mem_arenas);
		arena->mem_arenas = 0;
	}
	memset(arena, 0, sizeof(*arena));
}

/* --- image --- */

static RendVkImage
rend_vk_image_create(VkDevice logical_device, VkImageType img_type, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
		VkImageUsageFlags usage, uint32_t depth, uint32_t mip_levels, uint32_t layers,
		VkSampleCountFlags sample_count_flags, VkSharingMode sharing_mode)
{
	RendVkImage image = {
		.handle = VK_NULL_HANDLE,
		.memory = VK_NULL_HANDLE,
		.logical_device = logical_device,
		.img_type = img_type,
		.width = width,
		.height = height,
		.format = format,
		.tiling = tiling,
		.usage = usage,
		.depth = depth,
		.mip_levels = mip_levels,
		.layers = layers,
		.sample_count_flags = sample_count_flags,
		.sharing_mode = sharing_mode
	};

	VkImageCreateInfo img_create_info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
	img_create_info.imageType = img_type;
	img_create_info.extent.width = width;
	img_create_info.extent.height = height;
	img_create_info.extent.depth = depth;
	img_create_info.mipLevels = mip_levels;
	img_create_info.arrayLayers = layers;
	img_create_info.format = format;
	img_create_info.tiling = tiling;
	img_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	img_create_info.usage = usage;
	img_create_info.samples = sample_count_flags;
	img_create_info.sharingMode = sharing_mode;

	if (vkCreateImage(logical_device, &img_create_info, vk_allocator, &image.handle) != VK_SUCCESS) {
		return image;
	}

	return image;
}

static uint32_t
rend_vk_image_required_memory_type(RendVkImage *img)
{
	VkMemoryRequirements memory_requirements;

	assert(img->memory == NULL && "Image already bound to memory");
	memory_requirements = (VkMemoryRequirements){0};
	vkGetImageMemoryRequirements(img->logical_device, img->handle, &memory_requirements);
	img->requirements = memory_requirements;
	return memory_requirements.memoryTypeBits;
}

static void
rend_vk_image_bind_memory(RendVkImage *img, RendMemory *memory)
{
	assert(img->memory == NULL && "Image already bound to memory");
	vkBindImageMemory(img->logical_device, img->handle, (VkDeviceMemory) memory->device_memory, memory->offset);
}

static void
rend_vk_image_destroy(RendVkImage *img)
{
	if (img->view) {
		vkDestroyImageView(img->logical_device, img->view, vk_allocator);
		img->view = 0;
	}

	if (img->handle) {
		vkDestroyImage(img->logical_device, img->handle, vk_allocator);
		img->handle = 0;
	}
	if (img->owned_memory) {
		vkFreeMemory(img->logical_device, img->owned_memory, vk_allocator);
		img->owned_memory = 0;
	}
}

static void
rend_vk_image_view_create(RendVkImage *image, VkImageViewType view_type, VkImageAspectFlags view_aspect_flags)
{
	VkImageViewCreateInfo view_create_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = image->handle,
		.format = image->format,
		.viewType = view_type,
		.subresourceRange.aspectMask = view_aspect_flags,
		.subresourceRange.baseMipLevel = 0,
		.subresourceRange.levelCount = image->mip_levels,
		.subresourceRange.baseArrayLayer = 0,
		.subresourceRange.layerCount = image->layers,
	};

	vkCreateImageView(image->logical_device, &view_create_info, vk_allocator, &image->view);
}

/* Version-agnostic Vulkan helpers (shared; not the 1.4 renderer backend). */

VKAPI_ATTR VkBool32 VKAPI_CALL
rend_vk_debug_func(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity, VkDebugUtilsMessageTypeFlagsEXT message_types, const VkDebugUtilsMessengerCallbackDataEXT *callback_data, void *user_data)
{
	switch (message_severity) {
		default:
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
			PERROR(callback_data->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
			PWARN(callback_data->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
			PINFO(callback_data->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
			PTRACE(callback_data->pMessage);
			break;
	}
	return VK_FALSE;
}

bool
rend_vk_init(void)
{
	RASSERT(sizeof(void*) == 8 && sizeof(int64_t) == 8 && sizeof(double) == 8, "SPEC: Host device 64-bit integer and floating-point types.");

	/* trying to init another renderer with the same backend */
	if (vk_instance) {
		return true;
	}

	/* create instance if does not exist */
	VkApplicationInfo vk_app_info = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
	vk_app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	vk_app_info.pApplicationName = "REND";
	vk_app_info.applicationVersion = VK_MAKE_VERSION(REND_MAJOR, REND_MINOR, REND_PATCH);
	vk_app_info.apiVersion = VK_API_VERSION_1_4;
	vk_app_info.pEngineName = "REND Renderer";
	vk_app_info.engineVersion = VK_MAKE_VERSION(REND_MAJOR, REND_MINOR, REND_PATCH);

	uint32_t peak_ext_count = 0;
	const char **peak_exts = peak_vulkan_get_extensions(&peak_ext_count);
	const char *extensions[8];
	uint32_t ext_count = 0;
	uint32_t ei;
	for (ei = 0; ei < peak_ext_count && ext_count < 8; ei++)
		extensions[ext_count++] = peak_exts[ei];

#ifdef REND_DEBUG
	if (ext_count < 8)
		extensions[ext_count++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
	PDEBUG("Vulkan Extensions: ");
	for (ei = 0; ei < ext_count; ei++)
		PDEBUG("%s", extensions[ei]);
#endif

#ifdef REND_DEBUG
	const char *required_validation_layers[] = { "VK_LAYER_KHRONOS_validation" };
	uint32_t layer_count = 1;

	PDEBUG("Required Validation Layers: ");
	for (ei = 0; ei < layer_count; ei++) {
		PDEBUG(" - %s", required_validation_layers[ei]);
	}

	VkValidationFeaturesEXT validation_features = {
		.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
		.pNext = NULL,
		/* GPU-assisted validation: VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT */
	};
#endif

	VkInstanceCreateInfo vk_create_info = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
	vk_create_info.pApplicationInfo = &vk_app_info;
	vk_create_info.ppEnabledExtensionNames = extensions;
	vk_create_info.enabledExtensionCount = ext_count;
	vk_create_info.pApplicationInfo = &vk_app_info;
#ifdef REND_DEBUG
	vk_create_info.enabledLayerCount = layer_count;
	vk_create_info.ppEnabledLayerNames = required_validation_layers;
	vk_create_info.pNext = &validation_features;
#else
	vk_create_info.enabledLayerCount = 0;
	vk_create_info.ppEnabledLayerNames = NULL;
#endif

	VkResult res = vkCreateInstance(&vk_create_info, vk_allocator, &vk_instance);
	CHECK_VK_RESULT(res);

	PDEBUG("Vulkan instance created!");

#ifdef REND_DEBUG
	uint32_t log_severity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
	VkDebugUtilsMessengerCreateInfoEXT debug_create_info = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
	debug_create_info.messageSeverity = log_severity;
	debug_create_info.messageType =
		VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
	debug_create_info.pfnUserCallback = rend_vk_debug_func;

	PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vk_instance, "vkCreateDebugUtilsMessengerEXT");
	if (!func) {
		PDEBUG("Failed to create vulkan debug messenger!");
		return false;
	}
	func(vk_instance, &debug_create_info, vk_allocator, &vk_debug_messenger);
#endif
	return true;
}

void
rend_vk_quit(void)
{
	PDEBUG("[REND_VK14] Destroying device.");

	/* we destroy the device on quit */
	if (vk_device.logical_device != 0) {
		rend_vk_device_destroy(&vk_device);
	}

	if (vk_debug_messenger) {
		PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vk_instance, "vkDestroyDebugUtilsMessengerEXT");
		func(vk_instance, vk_debug_messenger, vk_allocator);
		vk_debug_messenger = 0;
	}

	vkDestroyInstance(vk_instance, vk_allocator);
	vk_instance = 0;
}

static uint32_t
rend_vk_get_heap_index(uint32_t memory_type_bits, uint32_t preferred_index)
{
	uint32_t i;
	if (memory_type_bits & (1u << preferred_index)) {
		return preferred_index;
	}

	for (i = 0; i < 32; i++) {
		if (memory_type_bits & (1u << i)) {
			return i;
		}
	}

	return UINT32_MAX;
}

static VkShaderModule
rend_vk_shader_module_create(const void *data, size_t size)
{

	VkShaderModuleCreateInfo create_info = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
	create_info.codeSize = size;
	create_info.pCode = (const uint32_t *)data;

	VkShaderModule module;
	VkResult res = vkCreateShaderModule(vk_device.logical_device, &create_info, vk_allocator, &module);

	if (res != VK_SUCCESS) {
		PWARN("Failed to create shader module for %p", data);
		return VK_NULL_HANDLE;
	}

	return module;
}

static VkCommandBuffer
rend_vk_cmdbuffer_single_use_begin(VkCommandPool pool)
{
	VkCommandBufferAllocateInfo alloc_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandPool = pool,
		.commandBufferCount = 1,
	};

	VkCommandBuffer cmd;
	vkAllocateCommandBuffers(vk_device.logical_device, &alloc_info, &cmd);

	VkCommandBufferBeginInfo begin = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};

	vkBeginCommandBuffer(cmd, &begin);

	return cmd;
}

static void
rend_vk_cmdbuffer_single_use_end(VkCommandPool pool, VkCommandBuffer cmd, VkQueue q)
{
	vkEndCommandBuffer(cmd);

	VkSubmitInfo submit_info = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &cmd,
	};

	vkQueueSubmit(q, 1, &submit_info, VK_NULL_HANDLE);
	vkQueueWaitIdle(q);

	vkFreeCommandBuffers(vk_device.logical_device, pool, 1, &cmd);
}

static void
rend_vk_texture_barrier(VkCommandBuffer cmd, RendTexture *texture, VkImageLayout new_layout, uint32_t src_family, uint32_t dst_family, VkAccessFlags2 src_access, VkAccessFlags2 dst_access, VkPipelineStageFlags2 src_stage, VkPipelineStageFlags2 dst_stage)
{
	VkImageMemoryBarrier2 barrier = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.oldLayout = texture->layout,
		.newLayout = new_layout,
		.srcQueueFamilyIndex = src_family,
		.dstQueueFamilyIndex = dst_family,
		.image = (VkImage)texture->handle,
		.srcAccessMask = src_access,
		.dstAccessMask = dst_access,
		.srcStageMask = src_stage,
		.dstStageMask = dst_stage,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = texture->mip_levels,
			.baseArrayLayer = 0,
			.layerCount = texture->layers,
		},
	};
	VkDependencyInfo dep = {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.pImageMemoryBarriers = &barrier,
		.imageMemoryBarrierCount = 1,
	};
	texture->layout = new_layout;
	vkCmdPipelineBarrier2(cmd, &dep);
}

void
rend_vk_texture_transition_layout(RendContextHandle handle, VkCommandBuffer cmd, RendTexture *texture, VkImageLayout new_layout)
{
	(void)handle;
	if (texture->layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		rend_vk_texture_barrier(cmd, texture, new_layout, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
			0, VK_ACCESS_2_TRANSFER_WRITE_BIT,
			VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT);
	} else if (texture->layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		rend_vk_texture_barrier(cmd, texture, new_layout, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
			VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT,
			VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
	} else {
		rend_vk_texture_barrier(cmd, texture, new_layout, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
			VK_ACCESS_2_MEMORY_WRITE_BIT, VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
			VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
	}
}

void
rend_vk_texture_transfer_ownership_release(RendContextHandle handle, VkCommandBuffer cmd, RendTexture *texture, uint32_t src_family, uint32_t dst_family, VkImageLayout new_layout)
{
	(void)handle;
	if (texture->layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		rend_vk_texture_barrier(cmd, texture, new_layout, src_family, dst_family,
			VK_ACCESS_2_TRANSFER_WRITE_BIT, 0,
			VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);
	} else {
		rend_vk_texture_barrier(cmd, texture, new_layout, src_family, dst_family,
			VK_ACCESS_2_MEMORY_WRITE_BIT, 0,
			VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);
	}
}

void
rend_vk_texture_transfer_ownership_acquire(RendContextHandle handle, VkCommandBuffer cmd, RendTexture *texture, uint32_t src_family, uint32_t dst_family, VkImageLayout new_layout)
{
	(void)handle;
	if (texture->layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		rend_vk_texture_barrier(cmd, texture, new_layout, src_family, dst_family,
			0, VK_ACCESS_2_SHADER_READ_BIT,
			VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
	} else {
		rend_vk_texture_barrier(cmd, texture, new_layout, src_family, dst_family,
			0, VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
			VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
	}
}


/* --- Rend/rend_vk14.c: renderer backend (continued) --- */

#define REND_MIN_FRAMES_IN_FLIGHT 2 /* double buffering! */
#define REND_MAX_FRAMES_IN_FLIGHT 4 /* quadruple buffering! */

#define REND_VK_MAX_PIPELINES 100

typedef struct {
	VkSwapchainKHR handle;
	VkSurfaceFormatKHR format;
	VkExtent2D extent;

	uint32_t image_count;
	VkImage *images;
	VkImageView *views;
	VkSemaphore *present_semaphores;

} RendVkSwapchain;

typedef struct {
	VkCommandPool command_pool;
	VkCommandBuffer command_buffer;
	VkSemaphore image_acquired_semaphore;
} RendVkFrameResources;

typedef struct RendVkPipeline {
	VkPipeline handle;
	VkPipeline handle_blend;
	VkPipelineLayout layout;
	uint32_t push_constants_range;
	bool blend_enable;
} RendVkPipeline;

/*
 * per renderer context, separate from global vk_ variables
 * such as the instance and device.
 */
typedef struct RendVk14Context {
	PeakWindow *window;
	VkSurfaceKHR surface;
	RendVkPipeline pipelines[REND_VK_MAX_PIPELINES];
	RendVkFrameResources frame_resources[REND_MAX_FRAMES_IN_FLIGHT];
	RendVkSwapchain swapchain;
	VkSemaphore timeline_semaphore;

	VkCommandPool upload_command_pool;
	VkCommandPool graphics_command_pool;

	RendVkArenaAllocator arena_persistent; /* renderer-lifetime buffer and color memory */

	uint64_t frame;
	uint64_t frame_index;
	uint64_t signal_value;
	uint64_t next_signal_value;
	uint64_t max_frames_in_flight;
	uint32_t pipeline_count;
	uint32_t image_index;

	bool require_swapchain_recreation;
	bool swapchain_suboptimal;
	uint32_t window_w;
	uint32_t window_h;

	RendVkImage swap_depth;
	RendVkImage tex_depth;
	RendVkImage *stale_depth; /* replaced during recording; retire after device idle */
	uint32_t stale_depth_count;
	uint32_t stale_depth_capacity;

	bool vsync;
	bool in_frame;
	bool has_frame_time;
	struct timespec frame_time;
	bool offscreen;
	RendVkImage offscreen_color;
	RendTexture *color_targets;
	uint32_t color_target_count;
	RendFormat color_rend_format;

	VkDescriptorPool        descriptor_pool;
	VkDescriptorSet         desc_set;
	VkDescriptorSetLayout   desc_layout;

} RendVk14Context;

static void rend_vk14_pipeline_destroy(RendVkPipeline *pipeline);
void rend_vk14_renderer_destroy(RendContextHandle handle);
static VkExtent2D rend_vk14_surface_extent(RendVk14Context *ctx, const VkSurfaceCapabilitiesKHR *caps);
static bool rend_vk14_swapchain_create(RendVk14Context *ctx, RendVkSwapchain *swapchain, VkSwapchainKHR old_swapchain);
static void rend_vk14_swapchain_destroy(RendVk14Context *ctx, RendVkSwapchain *swapchain);
static bool rend_vk14_swapchain_recreate(RendVk14Context *ctx);
static bool rend_vk14_offscreen_create(RendVk14Context *ctx, uint32_t width, uint32_t height, RendFormat format);
static void rend_vk14_offscreen_destroy(RendVk14Context *ctx);
static bool rend_vk14_renderer_init_sync_and_descriptors(RendVk14Context *ctx, RendBindingInfo *bind_info);
static RendFormat rend_vk14_format_from_vk(VkFormat fmt);
static void rend_vk14_color_targets_free(RendVk14Context *ctx);
static bool rend_vk14_color_targets_rebuild(RendVk14Context *ctx);
static RendTexture *rend_vk14_color_target_at(RendVk14Context *ctx);
RendTexture *rend_vk14_color_target(RendContextHandle handle);
static void rend_vk14_depth_barrier_img(RendVkImage *img, VkCommandBuffer cmd);
static void rend_vk14_depth_flush_stale(RendVk14Context *ctx);
static bool rend_vk14_depth_ensure_img(RendVk14Context *ctx, RendVkImage *img, uint32_t w, uint32_t h);
static void rend_vk14_depth_destroy(RendVk14Context *ctx);

RendContextHandle
rend_vk14_renderer_create(PeakWindow *window, RendBindingInfo *bind_info, bool vsync)
{
	RendVk14Context *ctx = rmalloc(sizeof(*ctx));
	if (!ctx) {
		PERROR("Failed to allocate internal vulkan context.");
		return NULL;
	}

	memset(ctx, 0, sizeof(*ctx));
	ctx->window = window;
	ctx->vsync = vsync;

	/* get surface from window */
	if (!peak_vulkan_create_surface(window, vk_instance, vk_allocator, &ctx->surface)) {
		PERROR("Failed to create vulkan surface!");
		rfree(ctx);
		return NULL;
	}

	RendSpecs specs = {
		.sampler_anisotropy = true,
		.graphics = true,
		.present = true,
		.transfer = true,
		.discrete_gpu = false, /* even though its not a requirement, I expect discrete gpu to be picked */
	};

	/* we lazily create the logical device only after creating the first renderer
	 * because we need the surface first */
	if (vk_device.logical_device == 0) {
		if (!rend_vk_device_create(ctx->surface, specs, &vk_device)) {
			PERROR("Failed to create vulkan device.");
			vkDestroySurfaceKHR(vk_instance, ctx->surface, vk_allocator);
			rfree(ctx);
			return NULL;
		}
	}

	ctx->arena_persistent = rend_vk_arena_create(
			vk_device.logical_device,
			vk_device.physical_device,
			vk_device.properties.limits);

	/* create swapchain */
	if (!rend_vk14_swapchain_create(ctx, &ctx->swapchain, VK_NULL_HANDLE)) {
		PERROR("Failed to create swapchain!");
		rend_vk14_renderer_destroy(ctx);
		return NULL;
	}
	ctx->color_rend_format = rend_vk14_format_from_vk(ctx->swapchain.format.format);
	if (!rend_vk14_color_targets_rebuild(ctx)) {
		PERROR("Failed to wrap swapchain images.");
		rend_vk14_renderer_destroy(ctx);
		return NULL;
	}

	if (!rend_vk14_renderer_init_sync_and_descriptors(ctx, bind_info)) {
		rend_vk14_renderer_destroy(ctx);
		return NULL;
	}

	return ctx;
}

RendContextHandle
rend_vk14_renderer_create_offscreen(uint32_t width, uint32_t height, RendFormat format, RendBindingInfo *bind_info)
{
	RendVk14Context *ctx;
	RendSpecs specs;

	if (width == 0 || height == 0) {
		PERROR("Offscreen renderer needs a non-zero extent.");
		return NULL;
	}

	ctx = rmalloc(sizeof(*ctx));
	if (!ctx) {
		PERROR("Failed to allocate internal vulkan context.");
		return NULL;
	}
	memset(ctx, 0, sizeof(*ctx));
	ctx->offscreen = true;

	specs = (RendSpecs) {
		.sampler_anisotropy = true,
		.graphics = true,
		.transfer = true,
		.present = false,
		.discrete_gpu = false,
	};

	if (vk_device.logical_device == 0) {
		if (!rend_vk_device_create(VK_NULL_HANDLE, specs, &vk_device)) {
			PERROR("Failed to create vulkan device.");
			rfree(ctx);
			return NULL;
		}
	}

	ctx->arena_persistent = rend_vk_arena_create(
			vk_device.logical_device,
			vk_device.physical_device,
			vk_device.properties.limits);

	if (!rend_vk14_offscreen_create(ctx, width, height, format)) {
		PERROR("Failed to create offscreen target!");
		rend_vk14_renderer_destroy(ctx);
		return NULL;
	}

	if (!rend_vk14_renderer_init_sync_and_descriptors(ctx, bind_info)) {
		rend_vk14_renderer_destroy(ctx);
		return NULL;
	}

	return ctx;
}

static bool
rend_vk14_renderer_init_sync_and_descriptors(RendVk14Context *ctx, RendBindingInfo *bind_info)
{
	uint32_t u;
	uint32_t i;

	/* timeline semaphore */
	VkSemaphoreTypeCreateInfo timeline_type_info = {VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
	timeline_type_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
	timeline_type_info.initialValue = ctx->max_frames_in_flight;

	VkSemaphoreCreateInfo timeline_semaphore_info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
	timeline_semaphore_info.pNext = &timeline_type_info;

	if (vkCreateSemaphore(vk_device.logical_device, &timeline_semaphore_info, vk_allocator, &ctx->timeline_semaphore) != VK_SUCCESS) {
		PERROR("Unable to create the timeline semaphore for the renderer!");
		return false;
	}

	/* create per frame semaphores */
	VkSemaphoreCreateInfo frame_semaphore_info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
	for (u = 0; u < REND_MAX_FRAMES_IN_FLIGHT; ++u) {
		if (vkCreateSemaphore(
					vk_device.logical_device,
					&frame_semaphore_info,
					vk_allocator,
					&ctx->frame_resources[u].image_acquired_semaphore) != VK_SUCCESS) {
			PERROR("Unable to create semaphore for frame #%u!", u);
			return false;
		}

		VkCommandPoolCreateInfo pool_info = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
		pool_info.queueFamilyIndex = vk_device.graphics_family_index;
		if (vkCreateCommandPool(vk_device.logical_device, &pool_info, vk_allocator, &ctx->frame_resources[u].command_pool) != VK_SUCCESS) {
			PERROR("Unable to create command pool for frame #%u!", u);
			return false;
		}

		VkCommandBufferAllocateInfo cmdbuf_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
		cmdbuf_info.commandPool = ctx->frame_resources[u].command_pool;
		cmdbuf_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmdbuf_info.commandBufferCount = 1;
		if (vkAllocateCommandBuffers(vk_device.logical_device, &cmdbuf_info, &ctx->frame_resources[u].command_buffer) != VK_SUCCESS) {
			PERROR("Unable to create command buffer for frame #%u!", u);
			return false;
		}

	}

	VkCommandPoolCreateInfo pool_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.queueFamilyIndex = vk_device.transfer_family_index,
		.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
	};

	if (vkCreateCommandPool(vk_device.logical_device, &pool_info, vk_allocator, &ctx->upload_command_pool) != VK_SUCCESS) {
		PERROR("Unable to create upload command pool!");
		return false;
	}

	pool_info.queueFamilyIndex = vk_device.graphics_family_index;
	if (vkCreateCommandPool(vk_device.logical_device, &pool_info, vk_allocator, &ctx->graphics_command_pool) != VK_SUCCESS) {
		PERROR("Unable to create graphics command pool!");
		return false;
	}

	ctx->frame = 0;
	ctx->frame_index = 0;
	ctx->next_signal_value = ctx->max_frames_in_flight + 1; /* start at frame zero */

	/*
	 * Descriptor Pool
	 */
	{
		RendBindingInfo bind_info_local = {0};
		if (bind_info) bind_info_local = *bind_info;
		RendBindingInfo bind_info = bind_info_local;

		uint32_t total_ubos = 0;
		for (i = 0; i < bind_info.ubo_binding_count; ++i) {
			total_ubos += bind_info.ubo_array_sizes[i];
		}

		uint32_t total_ssbos = 0;
		for (i = 0; i < bind_info.ssbo_binding_count; ++i) {
			total_ssbos += bind_info.ssbo_array_sizes[i];
		}

		uint32_t total_textures = 0;
		for (i = 0; i < bind_info.texture_binding_count; ++i) {
			total_textures += bind_info.texture_array_sizes[i];
		}

		const uint32_t pool_count = 3;
		VkDescriptorPoolSize pool_sizes[3] = {
			{ .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         .descriptorCount = (total_ubos > 0) ? total_ubos : 1 },
			{ .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         .descriptorCount = (total_ssbos > 0) ? total_ssbos : 1 },
			{ .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = (total_textures > 0) ? total_textures : 1 }
		};

		VkDescriptorPoolCreateInfo pool_info = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.flags = 0,
			.maxSets = 1,
			.poolSizeCount = pool_count,
			.pPoolSizes = pool_sizes,
			.pNext = NULL,
		};

		VkResult result = vkCreateDescriptorPool(vk_device.logical_device, &pool_info, vk_allocator, &ctx->descriptor_pool);
		if (result != VK_SUCCESS) {
			return false;
		}

		/*
		 * Descriptor Sets!!!!!!
		 */

		const uint32_t binding_count = bind_info.ubo_binding_count + bind_info.ssbo_binding_count + bind_info.texture_binding_count;
		VkDescriptorSetLayoutBinding binding_array[binding_count];

		for (i = 0; i < bind_info.ubo_binding_count; ++i) {
			binding_array[i] = (VkDescriptorSetLayoutBinding) {
				.binding         = bind_info.ubo_bindings[i],
				.descriptorCount = bind_info.ubo_array_sizes[i],
				.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.stageFlags      = VK_SHADER_STAGE_ALL,
				.pImmutableSamplers = 0,
			};
		};

		uint32_t offset = bind_info.ubo_binding_count;
		for (i = 0; i < bind_info.ssbo_binding_count; ++i) {
			binding_array[i + offset] = (VkDescriptorSetLayoutBinding) {
				.binding           = bind_info.ssbo_bindings[i],
				.descriptorCount   = bind_info.ssbo_array_sizes[i],
				.descriptorType    = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.stageFlags        = VK_SHADER_STAGE_ALL,
				.pImmutableSamplers = 0,
			};
		};

		offset += bind_info.ssbo_binding_count;
		for (i = 0; i < bind_info.texture_binding_count; ++i) {
			binding_array[i + offset] = (VkDescriptorSetLayoutBinding) {
				.binding           = bind_info.texture_bindings[i],
				.descriptorCount   = bind_info.texture_array_sizes[i],
				.descriptorType    = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.stageFlags        = VK_SHADER_STAGE_ALL,
				.pImmutableSamplers = 0,
			};
		};

		VkDescriptorBindingFlags binding_flags[binding_count];
		for (u = 0; u < binding_count; ++u) {
			binding_flags[u] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
		}

		VkDescriptorSetLayoutBindingFlagsCreateInfo desc_flags_info = {
			.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
			.bindingCount  = binding_count,
			.pBindingFlags = binding_flags
		};

		VkDescriptorSetLayoutCreateInfo desc_layout_info = {
			.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.flags        = 0,
			.bindingCount = binding_count,
			.pBindings    = binding_array,
			.pNext        = &desc_flags_info,
		};

		result = vkCreateDescriptorSetLayout(vk_device.logical_device, &desc_layout_info, vk_allocator, &ctx->desc_layout);
		if (result != VK_SUCCESS) {
			PERROR("Failed to create descriptor set layout!");
			return false;
		}

		VkDescriptorSetAllocateInfo alloc_info = {
			.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool     = ctx->descriptor_pool,
			.descriptorSetCount = 1, /* ONLY ONE DESCRIPTOR SET */
			.pSetLayouts        = &ctx->desc_layout,
			.pNext              = NULL,
		};

		result = vkAllocateDescriptorSets(vk_device.logical_device, &alloc_info, &ctx->desc_set);
		if (result != VK_SUCCESS) {
			PERROR("Failed to allocate descriptors!!!");
			return false;
		}
	}

	return true;
}

void
rend_vk14_renderer_destroy(RendContextHandle handle)
{
	uint32_t u;
	RendVk14Context *ctx;
	VkDevice dev;

	RASSERT(handle, "Invalid context handle.");
	if (!handle)
		return;

	ctx = (RendVk14Context *)handle;
	dev = vk_device.logical_device;

	if (dev) {
		PDEBUG("[REND] Waiting for device...");
		vkDeviceWaitIdle(dev);

		PDEBUG("[REND] Destroying renderer...");
		rend_vk14_depth_destroy(ctx);
		vkDestroySemaphore(dev, ctx->timeline_semaphore, vk_allocator);

		for (u = 0; u < REND_MAX_FRAMES_IN_FLIGHT; ++u) {
			vkDestroySemaphore(dev, ctx->frame_resources[u].image_acquired_semaphore, vk_allocator);
			vkDestroyCommandPool(dev, ctx->frame_resources[u].command_pool, vk_allocator);
			ctx->frame_resources[u].image_acquired_semaphore = 0;
			ctx->frame_resources[u].command_pool = 0;
			ctx->frame_resources[u].command_buffer = 0;
		}

		vkDestroyCommandPool(dev, ctx->upload_command_pool, vk_allocator);
		vkDestroyCommandPool(dev, ctx->graphics_command_pool, vk_allocator);
		vkDestroyDescriptorSetLayout(dev, ctx->desc_layout, vk_allocator);
		vkDestroyDescriptorPool(dev, ctx->descriptor_pool, vk_allocator);

		PDEBUG("[REND] Destroying pipelines...");
		for (u = 0; u < ctx->pipeline_count; ++u) {
			rend_vk14_pipeline_destroy(&ctx->pipelines[u]);
		}
	}

	rend_vk_arena_destroy(&ctx->arena_persistent);
	rend_vk14_color_targets_free(ctx);

	if (ctx->offscreen) {
		PDEBUG("[REND] Destroying offscreen target...");
		rend_vk14_offscreen_destroy(ctx);
	} else {
		PDEBUG("[REND] Destroying swapchain...");
		rend_vk14_swapchain_destroy(ctx, &ctx->swapchain);
		ctx->swapchain.handle = 0;

		if (vk_instance && ctx->surface) {
			PDEBUG("[REND] Destroying surface...");
			vkDestroySurfaceKHR(vk_instance, ctx->surface, vk_allocator);
			ctx->surface = 0;
		}
	}

	rfree(handle);
}

bool
rend_vk14_renderer_frame_begin(RendContextHandle handle)
{
	RendVk14Context *ctx;
	VkDevice dev;
	uint32_t retries;

	RASSERT(handle, "Invalid handle.");

	ctx = (RendVk14Context *)handle;
	dev = vk_device.logical_device;

	for (retries = 0; retries < 4; retries++) {
		uint64_t frame_res_index;
		uint64_t wait_value;
		VkResult acquire_image;
		RendVkFrameResources frame_resource;
		VkSemaphoreWaitInfo timeline_wait_info;

		if (ctx->window && (ctx->window->width == 0 || ctx->window->height == 0))
			return false;

		if (!ctx->offscreen && ctx->window &&
				(ctx->window->width != ctx->window_w ||
				 ctx->window->height != ctx->window_h))
			ctx->require_swapchain_recreation = true;

		if (!ctx->offscreen && ctx->require_swapchain_recreation) {
			PDEBUG("[REND] Awaiting device...");
			vkDeviceWaitIdle(vk_device.logical_device);
			PDEBUG("[REND] Recreating swapchain...");
			if (!rend_vk14_swapchain_recreate(ctx))
				return false;
			ctx->require_swapchain_recreation = false;
		}

		rend_vk14_depth_flush_stale(ctx);
		if (!rend_vk14_depth_ensure_img(ctx, &ctx->swap_depth,
					ctx->swapchain.extent.width, ctx->swapchain.extent.height))
			return false;

		if (ctx->max_frames_in_flight == 0)
			return false;

		/* wait on timeline semaphore */
		frame_res_index = ctx->frame % ctx->max_frames_in_flight;
		ctx->frame_index = frame_res_index;

		wait_value = ctx->next_signal_value - ctx->max_frames_in_flight;

		timeline_wait_info = (VkSemaphoreWaitInfo) {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
			.semaphoreCount = 1,
			.pSemaphores = &ctx->timeline_semaphore,
			.pValues = &wait_value
		};

		vkWaitSemaphores(vk_device.logical_device, &timeline_wait_info, UINT64_MAX);

		frame_resource = ctx->frame_resources[frame_res_index];
		vkResetCommandPool(dev, frame_resource.command_pool, 0);

		if (ctx->offscreen) {
			ctx->image_index = 0;
		} else {
			/* acquire next image */
			acquire_image = vkAcquireNextImageKHR(
					vk_device.logical_device,
					ctx->swapchain.handle,
					UINT64_MAX,
					frame_resource.image_acquired_semaphore,
					VK_NULL_HANDLE,
					&ctx->image_index);

			if (acquire_image == VK_ERROR_OUT_OF_DATE_KHR) {
				ctx->require_swapchain_recreation = true;
				continue;
			}
			if (acquire_image == VK_SUBOPTIMAL_KHR) {
				if (!ctx->swapchain_suboptimal) {
					ctx->require_swapchain_recreation = true;
					ctx->swapchain_suboptimal = true;
				}
			} else if (acquire_image != VK_SUCCESS) {
				return false;
			} else {
				ctx->swapchain_suboptimal = false;
			}
		}

		ctx->signal_value = ctx->next_signal_value++;

		{
	VkCommandBufferBeginInfo cmd_begin_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	};

	vkBeginCommandBuffer(frame_resource.command_buffer, &cmd_begin_info);

	static const size_t NUM_LAYOUT_BARRIERS = 2;
	VkImageMemoryBarrier2 layout_barriers[NUM_LAYOUT_BARRIERS];
	layout_barriers[0] = (VkImageMemoryBarrier2) {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,

		.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_2_TRANSFER_BIT,
		.srcAccessMask = 0,

		.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_2_TRANSFER_BIT,
		.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT,

		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,

		.image = ctx->swapchain.images[ctx->image_index],

		.subresourceRange = (VkImageSubresourceRange) {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1
		},

		/* .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, */
		/* .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, */
	};
	layout_barriers[1] = (VkImageMemoryBarrier2) {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,

		.srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
		.srcAccessMask = 0,

		.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
		.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,

		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,

		.image = ctx->swap_depth.handle,

		.subresourceRange = (VkImageSubresourceRange) {
			.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1
		},
	};

	VkMemoryBarrier2 memory_barrier =  {
		.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
		.srcStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
		.srcAccessMask = VK_ACCESS_2_HOST_WRITE_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
	};

	VkDependencyInfo dep_info = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
	dep_info.memoryBarrierCount = 1;
	dep_info.pMemoryBarriers = &memory_barrier;
	dep_info.imageMemoryBarrierCount = (uint32_t) NUM_LAYOUT_BARRIERS;
	dep_info.pImageMemoryBarriers = layout_barriers;

	vkCmdPipelineBarrier2(frame_resource.command_buffer, &dep_info);
		}

		{
			RendTexture *color;

			color = rend_vk14_color_target_at(ctx);
			if (color)
				color->layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}

		ctx->in_frame = true;
		return true;
	}

	return false;
}

void
rend_vk14_renderer_frame_end(RendContextHandle handle, float *delta)
{
	RendVk14Context *ctx = (RendVk14Context *)handle;
	RendTexture *color_end;

	RASSERT(ctx, "Uninitialized renderer.");

	RendVkFrameResources res = ctx->frame_resources[ctx->frame_index];
	color_end = rend_vk14_color_target_at(ctx);

	VkImageMemoryBarrier2 present_barrier;
	present_barrier = (VkImageMemoryBarrier2) {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,

		.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_2_TRANSFER_BIT,
		.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT,

		.dstStageMask = VK_PIPELINE_STAGE_2_NONE,
		.dstAccessMask = 0,

		.oldLayout = color_end && color_end->layout
			? (VkImageLayout)color_end->layout
			: VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.newLayout = ctx->offscreen ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,

		.image = ctx->swapchain.images[ctx->image_index],

		.subresourceRange = (VkImageSubresourceRange) {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1
		}

	};

	VkDependencyInfo dep_info = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
	dep_info.imageMemoryBarrierCount = 1;
	dep_info.pImageMemoryBarriers = &present_barrier;

	vkCmdPipelineBarrier2(res.command_buffer, &dep_info);
	if (color_end) {
		color_end->layout = ctx->offscreen
			? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
			: VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	}
	vkEndCommandBuffer(res.command_buffer);

	{
		VkSemaphoreSubmitInfo image_acquire_await_info;
		VkSemaphoreSubmitInfo semaphore_signals[2];
		VkCommandBufferSubmitInfo cmd_submit_info;
		VkSubmitInfo2 submit_info;
		uint32_t wait_count;
		uint32_t signal_count;

		image_acquire_await_info = (VkSemaphoreSubmitInfo) {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = res.image_acquired_semaphore,
			.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_2_TRANSFER_BIT
		};

		semaphore_signals[0] = (VkSemaphoreSubmitInfo) {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = ctx->timeline_semaphore,
			.value = ctx->signal_value,
			.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT
		};
		signal_count = 1;
		wait_count = 0;

		if (!ctx->offscreen) {
			semaphore_signals[1] = semaphore_signals[0];
			semaphore_signals[0] = (VkSemaphoreSubmitInfo) {
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
				.semaphore = ctx->swapchain.present_semaphores[ctx->image_index],
				.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT
			};
			signal_count = 2;
			wait_count = 1;
		}

		cmd_submit_info = (VkCommandBufferSubmitInfo) {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
			.commandBuffer = res.command_buffer,
		};

		submit_info = (VkSubmitInfo2) {
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
			.waitSemaphoreInfoCount = wait_count,
			.pWaitSemaphoreInfos = wait_count ? &image_acquire_await_info : NULL,
			.commandBufferInfoCount = 1,
			.pCommandBufferInfos = &cmd_submit_info,
			.signalSemaphoreInfoCount = signal_count,
			.pSignalSemaphoreInfos = semaphore_signals
		};

		vkQueueSubmit2(vk_device.graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
	}

	if (!ctx->offscreen) {
		VkPresentInfoKHR present_info = {
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &ctx->swapchain.present_semaphores[ctx->image_index],
			.swapchainCount = 1,
			.pSwapchains = &ctx->swapchain.handle,
			.pImageIndices = &ctx->image_index,
			.pResults = NULL,
		};
		VkResult present_res;

		present_res = vkQueuePresentKHR(vk_device.present_queue, &present_info);
		if (present_res == VK_ERROR_OUT_OF_DATE_KHR)
			ctx->require_swapchain_recreation = true;
		else if (present_res == VK_SUBOPTIMAL_KHR && !ctx->swapchain_suboptimal) {
			ctx->require_swapchain_recreation = true;
			ctx->swapchain_suboptimal = true;
		}
	}

	ctx->frame++;
	ctx->in_frame = false;

	if (delta) {
		struct timespec now;
		float dt;

		clock_gettime(CLOCK_MONOTONIC, &now);
		if (ctx->has_frame_time) {
			dt = (float)(now.tv_sec - ctx->frame_time.tv_sec) +
				(float)(now.tv_nsec - ctx->frame_time.tv_nsec) * 1e-9f;
		} else {
			dt = 0.f;
		}
		ctx->frame_time = now;
		ctx->has_frame_time = true;
		*delta = dt;
	}
}

static inline void
rend_vk14__renderer_render_pass_begin_internal(RendContextHandle handle, float r, float g, float b, float a, uint64_t view_handle, uint64_t depth_attachment_view_handle, uint32_t offset_x, uint32_t offset_y, uint32_t width, uint32_t height, VkAttachmentLoadOp color_load)
{
	RendVk14Context *ctx = (RendVk14Context *)handle;
	RASSERT(ctx->in_frame && "must begin render pass inside a frame");

	RendVkFrameResources frame_resource = ctx->frame_resources[ctx->frame_index];

	VkRenderingAttachmentInfo color_attachment = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = (VkImageView)view_handle,
		.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.loadOp = color_load,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.clearValue = {
			.color = {{r, g, b, a}},
		}
	};

	VkRenderingAttachmentInfo depth_attachment = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = (VkImageView)depth_attachment_view_handle,
		.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, /* clear depth data */
		.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, /* don't care after rendering */
		.clearValue = (VkClearValue) {
			.depthStencil = (VkClearDepthStencilValue) {1.0f, 0},
		},
	};

	VkRenderingInfo rendering_info = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea.offset = (VkOffset2D) {offset_x, offset_y},
		.renderArea.extent = (VkExtent2D) {width, height},
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = &color_attachment,
		.pDepthAttachment = &depth_attachment,
	};

	vkCmdBeginRendering(frame_resource.command_buffer, &rendering_info);

	VkViewport viewport = {
		.x = offset_x,
		.y = offset_y,
		.width = width,
		.height = height,
		.minDepth = 0.0f,
		.maxDepth = 1.0f
	};

	vkCmdSetViewport(frame_resource.command_buffer, 0, 1, &viewport);

	VkRect2D scissor = {{offset_x, offset_y}, {width, height}};
	vkCmdSetScissor(frame_resource.command_buffer, 0, 1, &scissor);
}

void
rend_vk14_renderer_render_pass_begin(RendContextHandle handle, float r, float g, float b, float a)
{
	RendVk14Context *ctx = (RendVk14Context *)handle;
	RendTexture *color = rend_vk14_color_target_at(ctx);

	RASSERT(color, "No color target.");
	if (!color)
		return;
	if (!rend_vk14_depth_ensure_img(ctx, &ctx->swap_depth, color->width, color->height))
		return;
	rend_vk14__renderer_render_pass_begin_internal(
			handle,
			r, g, b, a,
			color->view,
			(uint64_t)ctx->swap_depth.view,
			0, 0,
			color->width, color->height,
			VK_ATTACHMENT_LOAD_OP_CLEAR
	);
}

void
rend_vk14_renderer_render_pass_begin_preserve(RendContextHandle handle)
{
	RendVk14Context *ctx = (RendVk14Context *)handle;
	RendTexture *color = rend_vk14_color_target_at(ctx);

	RASSERT(ctx->in_frame && color, "No acquired color target.");
	if (!ctx->in_frame || !color)
		return;
	rend_vk_texture_transition_layout(handle, ctx->frame_resources[ctx->frame_index].command_buffer,
		color, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	rend_vk14_depth_barrier_img(&ctx->swap_depth, ctx->frame_resources[ctx->frame_index].command_buffer);
	rend_vk14__renderer_render_pass_begin_internal(handle, 0, 0, 0, 0,
		color->view, (uint64_t)ctx->swap_depth.view, 0, 0,
		color->width, color->height, VK_ATTACHMENT_LOAD_OP_LOAD);
}

void
rend_vk14_renderer_render_pass_begin_texture(RendContextHandle handle, RendTexture *texture)
{
	RendVk14Context *ctx = (RendVk14Context *)handle;

	{
		uint32_t width;
		uint32_t height;
		VkAttachmentLoadOp load;

		load = (texture->layout == VK_IMAGE_LAYOUT_UNDEFINED)
			? VK_ATTACHMENT_LOAD_OP_CLEAR
			: VK_ATTACHMENT_LOAD_OP_LOAD;
		rend_vk_texture_transition_layout(handle, ctx->frame_resources[ctx->frame_index].command_buffer, texture, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		width = texture->width;
		height = texture->height;
		if (!rend_vk14_depth_ensure_img(ctx, &ctx->tex_depth, width, height))
			return;
		rend_vk14_depth_barrier_img(&ctx->tex_depth, ctx->frame_resources[ctx->frame_index].command_buffer);
		rend_vk14__renderer_render_pass_begin_internal(
				handle,
				0, 0, 0, 0,
				(uint64_t)texture->view,
				(uint64_t)ctx->tex_depth.view,
				0, 0,
				width, height,
				load
		);
	}
}

void
rend_vk14_renderer_render_pass_end(RendContextHandle handle)
{
	RendVk14Context *ctx = (RendVk14Context *)handle;
	RASSERT(ctx->in_frame && "must end render pass inside a frame");
	RendVkFrameResources res = ctx->frame_resources[ctx->frame_index];
	vkCmdEndRendering(res.command_buffer);
}

void
rend_vk14_renderer_render_pass_end_texture(RendContextHandle handle, RendTexture *texture)
{
	RendVk14Context *ctx = (RendVk14Context *)handle;
	rend_vk14_renderer_render_pass_end(handle);
	rend_vk_texture_transition_layout(handle, ctx->frame_resources[ctx->frame_index].command_buffer, texture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void
rend_vk14_descriptor_write_buffer(RendContextHandle handle, RendBuffer ubo, uint32_t binding, uint32_t slot, uint32_t offset, uint32_t size, bool is_ubo)
{
	RendVk14Context *ctx = (RendVk14Context *)handle;

	VkDescriptorBufferInfo buffer_info = {
		.buffer = (VkBuffer)ubo.handle,
		.offset = offset,
		.range = size,
	};

	VkWriteDescriptorSet descriptor_write = {
		.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet          = ctx->desc_set,
		.dstBinding      = binding,
		.dstArrayElement = slot, /* write texture to slot */
		.descriptorCount = 1,
		.descriptorType  = (is_ubo) ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.pBufferInfo = &buffer_info,
	};

	vkUpdateDescriptorSets(vk_device.logical_device, 1, &descriptor_write, 0, NULL);
}

RendBuffer
rend_vk14_buffer_create(RendContextHandle handle, size_t size, RendBufferType type, bool gpu)
{
	RendVk14Context *ctx = (RendVk14Context *)handle;
	int32_t index = (gpu) ? vk_device.device_index : vk_device.host_index;

	if (type >= REND_BUFFER_COUNT || !vk_buffer_usage[type]) {
		REND__CRASH("Invalid buffer type!");
	}
	VkBufferUsageFlags vk_usage = vk_buffer_usage[type];

	RendBuffer buffer = {0};
	buffer.usage = vk_usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

	bool is_concurrent = (gpu && (vk_device.graphics_family_index != vk_device.transfer_family_index));
	uint32_t family[] = { vk_device.graphics_family_index, vk_device.transfer_family_index  };

	VkBufferCreateInfo buffer_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = buffer.usage,
		.sharingMode            = is_concurrent ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount  = is_concurrent ? 2 : 0,
		.pQueueFamilyIndices    = is_concurrent ? family : NULL,
	};

	if (vkCreateBuffer(vk_device.logical_device, &buffer_info, vk_allocator, (VkBuffer *) &buffer.handle) != VK_SUCCESS) {
		REND__CRASH("Failed to create VkBuffer!");
	}

	VkMemoryRequirements mem_reqs;
	vkGetBufferMemoryRequirements(vk_device.logical_device, (VkBuffer)buffer.handle, &mem_reqs);

	RASSERT((mem_reqs.memoryTypeBits & (1u << index)) && "Buffer incompatible with chosen memory type!");

	RendMemory vk_memory = rend_vk_arena_alloc(&ctx->arena_persistent, mem_reqs.size, mem_reqs.alignment, index);

	if (vkBindBufferMemory(vk_device.logical_device, (VkBuffer)buffer.handle, (VkDeviceMemory)vk_memory.device_memory, vk_memory.offset) != VK_SUCCESS) {
		REND__CRASH("Failed to bind VkBuffer memory!");
	}

	buffer.memory = vk_memory;

	VkBufferDeviceAddressInfo address_info = {
		.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
		.buffer = (VkBuffer)buffer.handle,
	};

	if (!gpu && vk_memory.host_mapped_memory) {
		buffer.mapped_memory = vk_memory.host_mapped_memory;
	} else {
		buffer.mapped_memory = NULL;
	}

	buffer.gpu_address = vkGetBufferDeviceAddress(vk_device.logical_device, &address_info);
	buffer.size = size;
	return buffer;
}

void
rend_vk14_buffer_destroy(RendBuffer *buffer)
{
	/* NOTE: this function is meant to be called by the user */
	/* when freeing his buffers mid frame, there may be a better */
	/* way using fences perhaps? or by checking the timeline semaphore? */
	vkDeviceWaitIdle(vk_device.logical_device);
	vkDestroyBuffer(vk_device.logical_device, (VkBuffer)buffer->handle, vk_allocator);
	memset(buffer, 0xC0FFEE, sizeof(*buffer)); /* fill buffer with coffee */
}

void
rend_vk14_buffer_copy(RendContextHandle handle, RendBuffer *dest, size_t dest_offset, RendBuffer *src, size_t src_offset, size_t bytes)
{
	RASSERT(src && dest); /* check that im not sending null pointers */
	RASSERT(src->usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT); /* source buffer must be marked as transfer src */
	RASSERT(dest->usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT); /* dest buffer must be marked as transfer dest */

	RendVk14Context *ctx = (RendVk14Context *)handle;
	VkBuffer src_vk  = (VkBuffer)(uintptr_t)src->handle;
	VkBuffer dest_vk = (VkBuffer)(uintptr_t)dest->handle;

	VkCommandBuffer transfer_cmd = VK_NULL_HANDLE;

	VkCommandBufferAllocateInfo alloc_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandBufferCount = 1,
		.commandPool = ctx->upload_command_pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.pNext = NULL,
	};

	if (vkAllocateCommandBuffers(vk_device.logical_device, &alloc_info, &transfer_cmd) != VK_SUCCESS) {
		REND__CRASH("Failed to allocate transfer command buffer!");
	}

	VkCommandBufferBeginInfo begin_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	vkBeginCommandBuffer(transfer_cmd, &begin_info);

	VkBufferCopy buffer_copy = {
		.srcOffset = (VkDeviceSize)src_offset,
		.dstOffset = (VkDeviceSize)dest_offset,
		.size      = (VkDeviceSize)bytes,
	};

	vkCmdCopyBuffer(transfer_cmd, src_vk, dest_vk, 1, &buffer_copy);
	vkEndCommandBuffer(transfer_cmd);

	VkCommandBufferSubmitInfo cmd_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
		.commandBuffer = transfer_cmd,
		.deviceMask = 0,
	};

	VkSubmitInfo2 submit_info = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
		.commandBufferInfoCount = 1,
		.pCommandBufferInfos = &cmd_info,
	};

	vkQueueSubmit2(vk_device.transfer_queue, 1, &submit_info, VK_NULL_HANDLE);

	vkQueueWaitIdle(vk_device.transfer_queue);
	vkFreeCommandBuffers(vk_device.logical_device, ctx->upload_command_pool, 1, &transfer_cmd);
}

RendTexture
rend_vk14_texture_create(RendContextHandle handle, uint32_t width, uint32_t height, uint32_t depth, uint32_t mip_levels, uint32_t layers, RendFormat format)
{
	RASSERT(format < REND_FORMAT_COUNT && "Invalid format!");

	RendVk14Context *ctx = (RendVk14Context *)handle;
	VkFormat vk_format = vk_format_from_rend_format[format];

	uint32_t safe_depth = (depth > 0) ? depth : 1;
	uint32_t safe_mips  = (mip_levels > 0) ? mip_levels : 1;
	uint32_t safe_layers = (layers > 0) ? layers : 1;

	RendTexture tex = {
		.handle     = 0,
		.width      = width,
		.height     = height,
		.depth      = safe_depth,
		.mip_levels = safe_mips,
		.layers     = safe_layers,
		.format     = format,
		.layout     = VK_IMAGE_LAYOUT_UNDEFINED,
	};

	VkImageCreateInfo image_info = {
		.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType     = (safe_depth > 1) ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D,
		.extent        = { .width = width, .height = height, .depth = safe_depth },
		.mipLevels     = tex.mip_levels,
		.arrayLayers   = tex.layers,
		.format        = vk_format,
		.tiling        = VK_IMAGE_TILING_OPTIMAL,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		.sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
		.samples       = VK_SAMPLE_COUNT_1_BIT,
		.flags         = 0,
	};

	if (vkCreateImage(vk_device.logical_device, &image_info, vk_allocator, (VkImage *)&tex.handle) != VK_SUCCESS) {
		REND__CRASH("failed to create image!");
		return tex;
	}

	VkMemoryRequirements mem_requirements;
	vkGetImageMemoryRequirements(vk_device.logical_device, (VkImage)tex.handle, &mem_requirements);

	uint32_t index = rend_vk_get_heap_index(mem_requirements.memoryTypeBits, vk_device.device_index);
	tex.memory = rend_vk_arena_alloc(&ctx->arena_persistent, mem_requirements.size, mem_requirements.alignment, index);

	vkBindImageMemory(vk_device.logical_device, (VkImage)tex.handle, (VkDeviceMemory)tex.memory.device_memory, (VkDeviceSize)tex.memory.offset);

	/* Correct ImageView creation using strict VkImageViewType and derived format */
	VkImageViewCreateInfo view_info = {
		.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image    = (VkImage)tex.handle,
		.viewType = (safe_depth > 1) ? VK_IMAGE_VIEW_TYPE_3D : VK_IMAGE_VIEW_TYPE_2D,
		.format   = vk_format,
		.subresourceRange = {
			.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel   = 0,
			.levelCount     = tex.mip_levels,
			.baseArrayLayer = 0,
			.layerCount     = tex.layers,
		},
	};

	if (vkCreateImageView(vk_device.logical_device, &view_info, vk_allocator, (VkImageView *)&tex.view) != VK_SUCCESS) {
		REND__CRASH("failed to create image view!");
		return tex;
	}

	/* per-texture sampler */
	VkSamplerCreateInfo sampler_info = {
		.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter    = VK_FILTER_LINEAR,
		.minFilter    = VK_FILTER_LINEAR,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,

		.mipmapMode   = (tex.mip_levels > 1) ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST,
		.minLod       = 0.0f,
		.maxLod       = (tex.mip_levels > 1) ? (float)tex.mip_levels : 0.0f,

		.anisotropyEnable = (tex.mip_levels > 1 && vk_device.features.samplerAnisotropy) ? VK_TRUE : VK_FALSE,
		.maxAnisotropy    = 8.0f,
	};

	if (vkCreateSampler(vk_device.logical_device, &sampler_info, vk_allocator, (VkSampler *)&tex.sampler) != VK_SUCCESS) {
		REND__CRASH("failed to create sampler!");
	}

	return tex;
}

void
rend_vk14_texture_destroy(RendContextHandle handle, RendTexture *tex)
{
	RendVk14Context *ctx;

	RASSERT(handle && tex);
	ctx = (RendVk14Context *)handle;
	if (ctx->in_frame) {
		RASSERT(0 && "must not destroy textures during a frame");
		return;
	}
	if (tex->borrowed) {
		RASSERT(0 && "do not destroy color_target");
		return;
	}

	/* in-flight CBs may still refer to this image */
	vkDeviceWaitIdle(vk_device.logical_device);

	if (tex->view) {
		vkDestroyImageView(vk_device.logical_device, (VkImageView)tex->view, vk_allocator);
		tex->view = 0;
	}

	if (tex->sampler) {
		vkDestroySampler(vk_device.logical_device, (VkSampler)tex->sampler, vk_allocator);
		tex->sampler = 0;
	}

	if (tex->handle) {
		vkDestroyImage(vk_device.logical_device, (VkImage)tex->handle, vk_allocator);
		tex->handle = 0;
	}

	/* memset(tex, 0xBABE, sizeof(*tex)); */
}

void
rend_vk14_texture_copy_buffer(RendContextHandle handle, RendTexture *texture, RendBuffer *buffer)
{
	RendVk14Context *ctx = (RendVk14Context *)handle;
	VkBufferImageCopy region;

	region = (VkBufferImageCopy) {0};
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.layerCount = texture->layers ? texture->layers : 1;
	region.imageExtent.width = texture->width;
	region.imageExtent.height = texture->height;
	region.imageExtent.depth = texture->depth ? texture->depth : 1;

	if (ctx->in_frame) {
		VkCommandBuffer cmd;
		VkMemoryBarrier2 mem;
		VkDependencyInfo dep;

		cmd = ctx->frame_resources[ctx->frame_index].command_buffer;
		mem = (VkMemoryBarrier2) {
			.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
			.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
			.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
		};
		dep = (VkDependencyInfo) {
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.memoryBarrierCount = 1,
			.pMemoryBarriers = &mem,
		};
		vkCmdPipelineBarrier2(cmd, &dep);
		rend_vk_texture_transition_layout(handle, cmd, texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		vkCmdCopyBufferToImage(cmd, (VkBuffer)buffer->handle, (VkImage)texture->handle,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
		return;
	}

	VkCommandBuffer cmd_transfer = rend_vk_cmdbuffer_single_use_begin(ctx->upload_command_pool); {
		rend_vk_texture_transition_layout(handle, cmd_transfer, texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		VkBufferImageCopy region = {
			.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.imageSubresource.layerCount = texture->layers,
			.imageExtent = (VkExtent3D) { texture->width, texture->height, texture->depth },
		};

		vkCmdCopyBufferToImage(cmd_transfer, (VkBuffer)buffer->handle, (VkImage)texture->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
		rend_vk_texture_transfer_ownership_release(handle, cmd_transfer, texture, vk_device.transfer_family_index, vk_device.graphics_family_index, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	} rend_vk_cmdbuffer_single_use_end(ctx->upload_command_pool, cmd_transfer, vk_device.transfer_queue);

	VkCommandBuffer cmd_graphics = rend_vk_cmdbuffer_single_use_begin(ctx->graphics_command_pool); {
		rend_vk_texture_transfer_ownership_acquire(handle, cmd_graphics, texture, vk_device.transfer_family_index, vk_device.graphics_family_index, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	} rend_vk_cmdbuffer_single_use_end(ctx->graphics_command_pool, cmd_graphics, vk_device.graphics_queue);
}

void
rend_vk14_texture_copy_to_buffer(RendContextHandle handle, RendTexture *texture, RendBuffer *buffer)
{
	RendVk14Context *ctx;
	VkCommandBuffer cmd;
	VkBufferImageCopy region;
	VkMemoryBarrier2 mem_barrier;
	VkDependencyInfo dep;
	uint32_t layers;
	uint32_t depth;

	ctx = (RendVk14Context *)handle;
	RASSERT(ctx && texture && buffer, "Invalid texture read.");
	RASSERT(!ctx->in_frame, "Must be called outside a frame.");
	if (!ctx || !texture || !buffer)
		return;

	layers = texture->layers ? texture->layers : 1;
	depth = texture->depth ? texture->depth : 1;

	vkQueueWaitIdle(vk_device.graphics_queue);
	cmd = rend_vk_cmdbuffer_single_use_begin(ctx->graphics_command_pool);

	rend_vk_texture_transition_layout(handle, cmd, texture, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

	region = (VkBufferImageCopy) {0};
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.layerCount = layers;
	region.imageExtent.width = texture->width;
	region.imageExtent.height = texture->height;
	region.imageExtent.depth = depth;

	vkCmdCopyImageToBuffer(
			cmd,
			(VkImage)texture->handle,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			(VkBuffer)buffer->handle,
			1, &region);

	mem_barrier = (VkMemoryBarrier2) {
		.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
		.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
		.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
		.dstAccessMask = VK_ACCESS_2_HOST_READ_BIT,
	};
	dep = (VkDependencyInfo) {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.memoryBarrierCount = 1,
		.pMemoryBarriers = &mem_barrier,
	};
	vkCmdPipelineBarrier2(cmd, &dep);

	rend_vk_texture_transition_layout(handle, cmd, texture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	rend_vk_cmdbuffer_single_use_end(ctx->graphics_command_pool, cmd, vk_device.graphics_queue);
}

void
rend_vk14_texture_blit(RendContextHandle handle, RendTexture *src, RendTexture *dst, uint32_t src_x, uint32_t src_y, uint32_t src_w, uint32_t src_h, uint32_t dst_x, uint32_t dst_y, uint32_t dst_w, uint32_t dst_h)
{
	RendVk14Context *ctx = (RendVk14Context *)handle;
	VkCommandBuffer cmd;
	int standalone;

	standalone = !ctx->in_frame;
	if (standalone) {
		vkQueueWaitIdle(vk_device.graphics_queue);
		cmd = rend_vk_cmdbuffer_single_use_begin(ctx->graphics_command_pool);
	} else {
		cmd = ctx->frame_resources[ctx->frame_index].command_buffer;
	}

	rend_vk_texture_transition_layout(handle, cmd, src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	rend_vk_texture_transition_layout(handle, cmd, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	VkImageBlit2 blit_region = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
		.srcSubresource = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.mipLevel = 0,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
		.srcOffsets = {
			{ src_x, src_y, 0 },
			{ src_x + src_w, src_y + src_h, 1 }
		},
		.dstSubresource = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.mipLevel = 0,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
		.dstOffsets = {
			{ dst_x, dst_y, 0 },
			{ dst_x + dst_w, dst_y + dst_h, 1 }
		}
	};

	VkBlitImageInfo2 blit_info = {
		.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
		.srcImage = (VkImage)src->handle,
		.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		.dstImage = (VkImage)dst->handle,
		.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.regionCount = 1,
		.pRegions = &blit_region,

		.filter = VK_FILTER_NEAREST /* crispy */
	};

	vkCmdBlitImage2(cmd, &blit_info);
	rend_vk_texture_transition_layout(handle, cmd, src, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	if (!dst->borrowed)
		rend_vk_texture_transition_layout(handle, cmd, dst, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	if (standalone) {
		rend_vk_cmdbuffer_single_use_end(ctx->graphics_command_pool, cmd, vk_device.graphics_queue);
	}
}

bool
rend_vk14_pipeline_create(RendContextHandle handle, RendPipeline pipeline, Rend__PipelineConfig config, uint8_t type, const uint8_t *shader1, size_t bytes1, const uint8_t *shader2, size_t bytes2, const uint8_t *shader3, size_t bytes3)
{
	uint32_t u;
	uint32_t i;
	RendVk14Context *ctx = (RendVk14Context *)handle;
	RendVkPipeline *vk_pipeline;

	if (ctx->pipeline_count >= REND_VK_MAX_PIPELINES) {
		RASSERT(0, "pipeline limit");
		return false;
	}
	pipeline->idx = ctx->pipeline_count;
	pipeline->backend_ctx = ctx; /* useful for when we only have RendPipeline as an argument */

	vk_pipeline = &ctx->pipelines[pipeline->idx];
	memset(vk_pipeline, 0, sizeof *vk_pipeline);

	/*
	 * set color and depth format
	 */
	VkFormat color_format = (config.color_format != REND_FORMAT_UNDEFINED)
		? vk_format_from_rend_format[config.color_format]
		: ctx->swapchain.format.format;

	VkFormat depth_format = (config.depth_format != REND_FORMAT_UNDEFINED)
		? vk_format_from_rend_format[config.depth_format]
		: vk_device.depth_format;

	/*
	 * Pipeline Layout
	 */

	uint32_t total_size = 0;
	for (u = 0; u < config.push_constant_count; ++u) {
		total_size += config.push_constants[u].size;
	}
	vk_pipeline->push_constants_range = total_size;

	VkPushConstantRange pc_range = {
		.offset     = 0,
		.size       = total_size,
		.stageFlags = VK_SHADER_STAGE_ALL,
	};

	VkPipelineLayoutCreateInfo layout_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pSetLayouts = &ctx->desc_layout,
		.setLayoutCount = 1,
		.pPushConstantRanges    = (total_size > 0) ? &pc_range : NULL,
		.pushConstantRangeCount = (total_size > 0) ? 1 : 0,
	};

	CHECK_VK_RESULT(vkCreatePipelineLayout(vk_device.logical_device, &layout_info, vk_allocator, &vk_pipeline->layout));

	VkPipelineShaderStageCreateInfo shader_stages[3] = {0};
	VkShaderModule shader_modules[3] = {0};
	const uint8_t *shader_bytes[3] = { shader1, shader2, shader3 };
	size_t shader_sizes[3] = { bytes1, bytes2, bytes3 };
	uint32_t shader_count = 0;

	if (type != REND__PIPELINE_GRAPHICS && type != REND__PIPELINE_MESH && type != REND__PIPELINE_COMPUTE) {
		REND__CRASH("Invalid pipeline type");
	}
	for (i = 0; i < 3 && vk_pipeline_stages[type][i]; ++i) {
		shader_modules[i] = rend_vk_shader_module_create(shader_bytes[i], shader_sizes[i]);
		shader_stages[i] = (VkPipelineShaderStageCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = vk_pipeline_stages[type][i],
			.module = shader_modules[i],
			.pName = "main"
		};
		shader_count++;
	}

	/*
	 * Compute Pipeline Branch
	 */
	if (type == REND__PIPELINE_COMPUTE) {
		VkComputePipelineCreateInfo compute_pipeline_info = {
			.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
			.stage = shader_stages[0],
			.layout = vk_pipeline->layout,
			.basePipelineHandle = VK_NULL_HANDLE,
			.basePipelineIndex = -1,
		};

		if (vkCreateComputePipelines(vk_device.logical_device, VK_NULL_HANDLE, 1, &compute_pipeline_info, vk_allocator, &vk_pipeline->handle) != VK_SUCCESS) {
			RASSERT(0, "vkCreateComputePipelines failed");
			for (i = 0; i < shader_count; ++i) {
				if (shader_modules[i] != VK_NULL_HANDLE)
					vkDestroyShaderModule(vk_device.logical_device, shader_modules[i], vk_allocator);
			}
			rend_vk14_pipeline_destroy(vk_pipeline);
			return false;
		}

		PINFO("Successfully created compute pipeline!");
	}
	/*
	 * Graphics / Mesh Pipeline Branch
	 */
	else {
		VkPipelineRenderingCreateInfo rendering_info = {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
		rendering_info.colorAttachmentCount = 1;
		rendering_info.pColorAttachmentFormats = &color_format;
		rendering_info.depthAttachmentFormat = depth_format;

		VkPipelineViewportStateCreateInfo viewport_state = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
		viewport_state.viewportCount = 1;
		viewport_state.scissorCount = 1;

		VkPipelineRasterizationStateCreateInfo rasterizer = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.rasterizerDiscardEnable = VK_FALSE;
		rasterizer.polygonMode = vk_polymode[config.polygon_mode];
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = vk_cullflags[config.cull_mode];
		rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
		rasterizer.depthBiasEnable = VK_FALSE;

		VkPipelineMultisampleStateCreateInfo multisampling = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		VkPipelineColorBlendAttachmentState color_blend_attachment = {0};
		uint32_t blend;
		VkPipeline *pipe_out[2];

		color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		color_blend_attachment.blendEnable = VK_FALSE;
		color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
		color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

		VkPipelineColorBlendStateCreateInfo color_blending = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
		color_blending.logicOpEnable = VK_FALSE;
		color_blending.logicOp = VK_LOGIC_OP_COPY;
		color_blending.attachmentCount = 1;
		color_blending.pAttachments = &color_blend_attachment;

		VkPipelineDepthStencilStateCreateInfo depth_stencil = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
		depth_stencil.depthTestEnable = config.depth_test_enable;
		depth_stencil.depthWriteEnable = config.depth_test_enable ? VK_TRUE : VK_FALSE;
		depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
		depth_stencil.depthBoundsTestEnable = VK_FALSE;
		depth_stencil.stencilTestEnable = VK_FALSE;

		VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
		VkPipelineDynamicStateCreateInfo dynamic_state = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
		dynamic_state.dynamicStateCount = 2;
		dynamic_state.pDynamicStates = dynamic_states;

		VkPipelineVertexInputStateCreateInfo vertex_input_info = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
		VkPipelineInputAssemblyStateCreateInfo input_assembly = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
		VkVertexInputBindingDescription* vk_bindings = NULL;
		VkVertexInputAttributeDescription* vk_attributes = NULL;

		if (type == REND__PIPELINE_GRAPHICS) {
			uint32_t binding_count = config.vertex_binding_count;
			uint32_t attribute_count = config.vertex_attribute_count;

			/*
			 * Bind Vertex Attributes
			 */
			if (binding_count > 0) {
				vk_bindings = rmalloc(sizeof(VkVertexInputBindingDescription) * binding_count);
				for (i = 0; i < binding_count; i++) {
					RendVertexBinding rb = config.vertex_bindings[i];
					vk_bindings[i].binding   = rb.binding;
					vk_bindings[i].stride    = rb.stride;
					vk_bindings[i].inputRate = (rb.input_rate == REND_INPUT_RATE_INSTANCE) ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
				}
			}

			if (attribute_count > 0) {
				vk_attributes = rmalloc(sizeof(VkVertexInputAttributeDescription) * attribute_count);
				for (i = 0; i < attribute_count; i++) {
					RendVertexAttributes ra = config.vertex_attributes[i];
					vk_attributes[i].binding  = ra.binding;
					vk_attributes[i].location = ra.location;
					vk_attributes[i].offset   = ra.offset;
					vk_attributes[i].format   = vk_format_from_rend_format[ra.format];
				}
			}

			vertex_input_info.vertexBindingDescriptionCount = binding_count;
			vertex_input_info.pVertexBindingDescriptions = vk_bindings;
			vertex_input_info.vertexAttributeDescriptionCount = attribute_count;
			vertex_input_info.pVertexAttributeDescriptions = vk_attributes;

			input_assembly.topology = vk_topology[config.topology];
			input_assembly.primitiveRestartEnable = VK_FALSE;
		}

		VkGraphicsPipelineCreateInfo pipeline_info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
		pipeline_info.pNext = &rendering_info;
		pipeline_info.stageCount = shader_count;
		pipeline_info.pStages = shader_stages;
		pipeline_info.pVertexInputState = &vertex_input_info;
		pipeline_info.pInputAssemblyState = &input_assembly;
		pipeline_info.pViewportState = &viewport_state;
		pipeline_info.pRasterizationState = &rasterizer;
		pipeline_info.pMultisampleState = &multisampling;
		pipeline_info.pColorBlendState = &color_blending;
		pipeline_info.pDepthStencilState = &depth_stencil;
		pipeline_info.pDynamicState = &dynamic_state;
		pipeline_info.layout = vk_pipeline->layout;
		pipeline_info.renderPass = VK_NULL_HANDLE;

		pipe_out[0] = &vk_pipeline->handle;
		pipe_out[1] = &vk_pipeline->handle_blend;
		for (blend = 0; blend < 2; blend++) {
			color_blend_attachment.blendEnable = blend ? VK_TRUE : VK_FALSE;
			if (vkCreateGraphicsPipelines(vk_device.logical_device, VK_NULL_HANDLE, 1, &pipeline_info, vk_allocator, pipe_out[blend]) != VK_SUCCESS) {
				RASSERT(0, "vkCreateGraphicsPipelines failed");
				if (vk_bindings) rfree(vk_bindings);
				if (vk_attributes) rfree(vk_attributes);
				for (i = 0; i < shader_count; ++i) {
					if (shader_modules[i] != VK_NULL_HANDLE)
						vkDestroyShaderModule(vk_device.logical_device, shader_modules[i], vk_allocator);
				}
				rend_vk14_pipeline_destroy(vk_pipeline);
				return false;
			}
		}

		if (vk_bindings) rfree(vk_bindings);
		if (vk_attributes) rfree(vk_attributes);

	}

	/* destroy shader modules */
	for (i = 0; i < shader_count; ++i) {
		if (shader_modules[i] != VK_NULL_HANDLE) {
			vkDestroyShaderModule(vk_device.logical_device, shader_modules[i], vk_allocator);
		}
	}

	ctx->pipeline_count++;
	return true;
}

void
rend_vk14_pipeline_bind(RendPipeline pipeline)
{
	RendVk14Context *ctx = pipeline->backend_ctx;
	RendVkPipeline vk_pipeline = ctx->pipelines[pipeline->idx];
	VkCommandBuffer cmd = ctx->frame_resources[ctx->frame_index].command_buffer;
	VkPipelineBindPoint bind_point = (pipeline->type == REND__PIPELINE_COMPUTE) ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;
	VkPipeline handle = (vk_pipeline.blend_enable && vk_pipeline.handle_blend) ? vk_pipeline.handle_blend : vk_pipeline.handle;
	vkCmdBindPipeline(cmd, bind_point, handle);
	vkCmdBindDescriptorSets(cmd, bind_point, vk_pipeline.layout, 0, 1, &ctx->desc_set, 0, NULL);
}

void
rend_vk14_pipeline_push_constants(RendPipeline pipeline, void *push_data, size_t size)
{
	RASSERT(pipeline && push_data);

	RendVk14Context *ctx = pipeline->backend_ctx;
	RendVkPipeline p = ctx->pipelines[pipeline->idx];
	RASSERT(size <= p.push_constants_range && "Size exceeds bound push constant range!");
	vkCmdPushConstants(ctx->frame_resources[ctx->frame_index].command_buffer, p.layout, VK_SHADER_STAGE_ALL, 0, size, push_data);
}

void
rend_vk14_pipeline_bind_vertex_buffer(RendPipeline pipeline, uint32_t binding, RendBuffer buffer, size_t offset)
{
	RendVk14Context *ctx = pipeline->backend_ctx;
	VkBuffer buf = (VkBuffer)(uintptr_t)buffer.handle;
	VkCommandBuffer cmd = ctx->frame_resources[ctx->frame_index].command_buffer;

	VkDeviceSize vk_offset = (VkDeviceSize)offset;
	vkCmdBindVertexBuffers(cmd, binding, 1, &buf, &vk_offset);
}

void
rend_vk14_pipeline_bind_index_buffer(RendPipeline pipeline, RendBuffer buffer, size_t offset, RendIndexType index_type)
{
	RendVk14Context *ctx = pipeline->backend_ctx;
	VkBuffer buf = (VkBuffer)(uintptr_t)buffer.handle;
	VkCommandBuffer cmd = ctx->frame_resources[ctx->frame_index].command_buffer;

	VkIndexType vk_index_type = (index_type == REND_INDEX_UINT16) ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;

	vkCmdBindIndexBuffer(cmd, buf, (VkDeviceSize)offset, vk_index_type);
}

void
rend_vk14_descriptor_write_texture(RendContextHandle handle, RendTexture *texture, uint32_t binding, uint32_t slot)
{
	RendVk14Context *vk_ctx = (RendVk14Context *)handle;

	VkDescriptorImageInfo image_info = {
		.sampler     = (VkSampler)texture->sampler,
		.imageView   = (VkImageView)texture->view,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};

	VkWriteDescriptorSet descriptor_write = {
		.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet          = vk_ctx->desc_set,
		.dstBinding      = binding,
		.dstArrayElement = slot, /* write texture to slot */
		.descriptorCount = 1,
		.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.pImageInfo      = &image_info,
	};

	vkUpdateDescriptorSets(vk_device.logical_device, 1, &descriptor_write, 0, NULL);
}

void
rend_vk14_pipeline_dispatch(RendPipeline pipeline, uint32_t x, uint32_t y, uint32_t z)
{
	RendVk14Context *ctx = pipeline->backend_ctx;
	VkCommandBuffer cmd = ctx->frame_resources[ctx->frame_index].command_buffer;
	vkCmdDispatch(cmd, x, y, z);
}

void
rend_vk14_pipeline_draw(RendPipeline pipeline, size_t count, uint32_t instance_count)
{
	RendVk14Context *ctx = pipeline->backend_ctx;
	VkCommandBuffer cmd = ctx->frame_resources[ctx->frame_index].command_buffer;
	vkCmdDraw(cmd, count, instance_count, 0, 0);
}

void
rend_vk14_pipeline_draw_indexed(RendPipeline pipeline, uint32_t index_count, uint32_t first_index, int32_t vertex_offset, uint32_t instance_count)
{
	RendVk14Context *ctx = pipeline->backend_ctx;
	VkCommandBuffer cmd = ctx->frame_resources[ctx->frame_index].command_buffer;
	vkCmdDrawIndexed(cmd, index_count, instance_count, first_index, vertex_offset, 0);
}

void
rend_vk14_pipeline_set_blend(RendPipeline pipeline, bool blend)
{
	RendVk14Context *ctx = pipeline->backend_ctx;
	RendVkPipeline *vk_pipeline = &ctx->pipelines[pipeline->idx];

	vk_pipeline->blend_enable = blend;
	if (ctx->in_frame && pipeline->type != REND__PIPELINE_COMPUTE)
		rend_vk14_pipeline_bind(pipeline);
}

static void
rend_vk14_pipeline_destroy(RendVkPipeline *pipeline)
{
	VkDevice dev = vk_device.logical_device;

	if (pipeline->handle != VK_NULL_HANDLE) {
		vkDestroyPipeline(dev, pipeline->handle, vk_allocator);
		pipeline->handle = VK_NULL_HANDLE;
	}
	if (pipeline->handle_blend != VK_NULL_HANDLE) {
		vkDestroyPipeline(dev, pipeline->handle_blend, vk_allocator);
		pipeline->handle_blend = VK_NULL_HANDLE;
	}
	if (pipeline->layout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(dev, pipeline->layout, vk_allocator);
		pipeline->layout = VK_NULL_HANDLE;
	}
}

static void
rend_vk14_depth_barrier_img(RendVkImage *img, VkCommandBuffer cmd)
{
	VkImageMemoryBarrier2 barrier;
	VkDependencyInfo dep;

	if (!img || !img->handle || !cmd)
		return;

	barrier = (VkImageMemoryBarrier2) {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
		.srcAccessMask = 0,
		.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
		.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
		.image = img->handle,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1
		},
	};
	dep = (VkDependencyInfo){VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
	dep.imageMemoryBarrierCount = 1;
	dep.pImageMemoryBarriers = &barrier;
	vkCmdPipelineBarrier2(cmd, &dep);
}

static void
rend_vk14_depth_flush_stale(RendVk14Context *ctx)
{
	uint32_t i;

	if (!ctx || ctx->stale_depth_count == 0)
		return;
	if (vk_device.logical_device)
		vkDeviceWaitIdle(vk_device.logical_device);
	for (i = 0; i < ctx->stale_depth_count; i++)
		rend_vk_image_destroy(&ctx->stale_depth[i]);
	ctx->stale_depth_count = 0;
}

static bool
rend_vk14_depth_ensure_img(RendVk14Context *ctx, RendVkImage *slot, uint32_t w, uint32_t h)
{
	RendVkImage img;
	VkMemoryDedicatedAllocateInfo dedicated;
	VkMemoryAllocateInfo alloc;
	uint32_t mem_type;

	RASSERT(ctx && slot && "No context provided.");
	if (w < 1)
		w = 1;
	if (h < 1)
		h = 1;
	if (slot->handle && slot->width == w && slot->height == h)
		return true;

	/* Recorded commands can still reference every previous depth image. */
	if (slot->handle && ctx->in_frame && ctx->stale_depth_count == ctx->stale_depth_capacity) {
		uint32_t capacity = ctx->stale_depth_capacity ? ctx->stale_depth_capacity * 2 : 8;
		RendVkImage *stale = rrealloc(ctx->stale_depth, capacity * sizeof(*stale));

		if (!stale)
			return false;
		ctx->stale_depth = stale;
		ctx->stale_depth_capacity = capacity;
	}

	if (!rend_vk_device_detect_depth_format(&vk_device)) {
		vk_device.depth_format = VK_FORMAT_UNDEFINED;
		PERROR("Failed to find a supported depth buffer format!");
		return false;
	}

	img = rend_vk_image_create(
			vk_device.logical_device,
			VK_IMAGE_TYPE_2D,
			w, h,
			vk_device.depth_format,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			1, 1, 1,
			VK_SAMPLE_COUNT_1_BIT,
			VK_SHARING_MODE_EXCLUSIVE);
	if (!img.handle) {
		PERROR("Failed to create depth image.");
		return false;
	}

	mem_type = rend_vk_image_required_memory_type(&img);
	dedicated = (VkMemoryDedicatedAllocateInfo) {
		.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
		.image = img.handle,
	};
	alloc = (VkMemoryAllocateInfo) {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = &dedicated,
		.allocationSize = img.requirements.size,
		.memoryTypeIndex = rend_vk_get_heap_index(mem_type, vk_device.device_index),
	};
	if (vkAllocateMemory(vk_device.logical_device, &alloc, vk_allocator, &img.owned_memory) != VK_SUCCESS) {
		PERROR("Failed to allocate depth memory.");
		goto fail;
	}
	if (vkBindImageMemory(vk_device.logical_device, img.handle, img.owned_memory, 0) != VK_SUCCESS) {
		PERROR("Failed to bind depth memory.");
		goto fail;
	}
	rend_vk_image_view_create(&img, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_DEPTH_BIT);
	if (!img.view) {
		PERROR("Failed to create depth view.");
		goto fail;
	}

	if (slot->handle) {
		if (ctx->in_frame) {
			ctx->stale_depth[ctx->stale_depth_count++] = *slot;
		} else {
			vkDeviceWaitIdle(vk_device.logical_device);
			rend_vk_image_destroy(slot);
		}
	}
	*slot = img;

	if (ctx->in_frame)
		rend_vk14_depth_barrier_img(slot, ctx->frame_resources[ctx->frame_index].command_buffer);
	return true;

fail:
	rend_vk_image_destroy(&img);
	return false;
}

static void
rend_vk14_depth_destroy(RendVk14Context *ctx)
{
	if (!ctx)
		return;
	rend_vk14_depth_flush_stale(ctx);
	rend_vk_image_destroy(&ctx->swap_depth);
	rend_vk_image_destroy(&ctx->tex_depth);
	rfree(ctx->stale_depth);
	ctx->stale_depth = NULL;
	ctx->stale_depth_capacity = 0;
}

static VkExtent2D
rend_vk14_surface_extent(RendVk14Context *ctx, const VkSurfaceCapabilitiesKHR *caps)
{
	VkExtent2D extent;
	VkExtent2D min;
	VkExtent2D max;

	extent.width = caps->currentExtent.width;
	extent.height = caps->currentExtent.height;
	if (extent.width == 0xffffffffu || extent.height == 0xffffffffu) {
		extent.width = ctx->window ? ctx->window->width : 1;
		extent.height = ctx->window ? ctx->window->height : 1;
	}
	min = caps->minImageExtent;
	max = caps->maxImageExtent;
	if (extent.width < min.width) extent.width = min.width;
	if (extent.height < min.height) extent.height = min.height;
	if (max.width && extent.width > max.width) extent.width = max.width;
	if (max.height && extent.height > max.height) extent.height = max.height;
	if (extent.width == 0) extent.width = 1;
	if (extent.height == 0) extent.height = 1;
	return extent;
}

static bool
rend_vk14_offscreen_create(RendVk14Context *ctx, uint32_t width, uint32_t height, RendFormat format)
{
	RendVkImage color;
	RendMemory color_mem;
	uint32_t mem_type;
	uint32_t heap;
	VkFormat vk_format;

	if (format == REND_FORMAT_UNDEFINED)
		format = REND_FORMAT_B8G8R8A8_UNORM;
	if (format >= REND_FORMAT_COUNT)
		return false;

	vk_format = vk_format_from_rend_format[format];
	ctx->max_frames_in_flight = REND_MIN_FRAMES_IN_FLIGHT;
	ctx->image_index = 0;

	color = rend_vk_image_create(
			vk_device.logical_device,
			VK_IMAGE_TYPE_2D,
			width, height,
			vk_format,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			1, 1, 1,
			VK_SAMPLE_COUNT_1_BIT,
			VK_SHARING_MODE_EXCLUSIVE);
	if (!color.handle) {
		PERROR("Failed to create offscreen color image.");
		return false;
	}

	mem_type = rend_vk_image_required_memory_type(&color);
	heap = rend_vk_get_heap_index(mem_type, vk_device.device_index);
	color_mem = rend_vk_arena_alloc(&ctx->arena_persistent, color.requirements.size, color.requirements.alignment, heap);
	rend_vk_image_bind_memory(&color, &color_mem);
	rend_vk_image_view_create(&color, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT);
	if (!color.view) {
		PERROR("Failed to create offscreen color view.");
		rend_vk_image_destroy(&color);
		return false;
	}

	ctx->offscreen_color = color;
	ctx->swapchain.handle = VK_NULL_HANDLE;
	ctx->swapchain.image_count = 1;
	ctx->swapchain.images = rmalloc(sizeof *ctx->swapchain.images);
	ctx->swapchain.views = rmalloc(sizeof *ctx->swapchain.views);
	if (!ctx->swapchain.images || !ctx->swapchain.views) {
		rend_vk_image_destroy(&ctx->offscreen_color);
		rfree(ctx->swapchain.images);
		rfree(ctx->swapchain.views);
		ctx->swapchain.images = 0;
		ctx->swapchain.views = 0;
		return false;
	}
	ctx->swapchain.images[0] = color.handle;
	ctx->swapchain.views[0] = color.view;
	ctx->swapchain.format.format = vk_format;
	ctx->swapchain.format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	ctx->swapchain.extent.width = width;
	ctx->swapchain.extent.height = height;
	ctx->color_rend_format = format;
	if (!rend_vk14_color_targets_rebuild(ctx)) {
		rend_vk14_offscreen_destroy(ctx);
		return false;
	}

	if (!rend_vk14_depth_ensure_img(ctx, &ctx->swap_depth, width, height)) {
		rend_vk14_offscreen_destroy(ctx);
		return false;
	}

	return true;
}

static void
rend_vk14_offscreen_destroy(RendVk14Context *ctx)
{
	if (!ctx)
		return;

	rend_vk14_color_targets_free(ctx);
	if (ctx->swapchain.views) {
		rfree(ctx->swapchain.views);
		ctx->swapchain.views = 0;
	}
	if (ctx->swapchain.images) {
		rfree(ctx->swapchain.images);
		ctx->swapchain.images = 0;
	}
	ctx->swapchain.image_count = 0;
	rend_vk_image_destroy(&ctx->offscreen_color);
}

static bool
rend_vk14_swapchain_create(RendVk14Context *ctx, RendVkSwapchain *swapchain, VkSwapchainKHR old_swapchain)
{
	uint32_t i;
	RASSERT(ctx && "No context provided.");
	RASSERT(swapchain && "No swapchain provided.");

	VkSurfaceCapabilitiesKHR surface_caps;
	VkExtent2D swapchain_extent;
	VkResult sc_res;
	uint32_t queue_family_indices[2];

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_device.physical_device, ctx->surface, &surface_caps);
	swapchain_extent = rend_vk14_surface_extent(ctx, &surface_caps);

	if (ctx->max_frames_in_flight == 0) {
		ctx->max_frames_in_flight = REND_MIN_FRAMES_IN_FLIGHT;

		if (surface_caps.minImageCount > ctx->max_frames_in_flight) {
			ctx->max_frames_in_flight = surface_caps.minImageCount;
		}

		/* maxImageCount == 0 means there is no maximum */
		if (surface_caps.maxImageCount > 0 && surface_caps.maxImageCount < ctx->max_frames_in_flight) {
			ctx->max_frames_in_flight = surface_caps.maxImageCount;
		}
		if (ctx->max_frames_in_flight > REND_MAX_FRAMES_IN_FLIGHT) {
			ctx->max_frames_in_flight = REND_MAX_FRAMES_IN_FLIGHT;
		}
		if (ctx->max_frames_in_flight == 0) {
			ctx->max_frames_in_flight = 1;
		}
	}

	bool found = false;
	for (i = 0; i < vk_device.swapchain_support.format_count; i++) {
		VkSurfaceFormatKHR format = vk_device.swapchain_support.format[i];

		/* preferred format */
		if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
				format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			swapchain->format = format;
			found = true;
			break;
		}
	}

	/* default format */
	if (!found) {
		swapchain->format = vk_device.swapchain_support.format[0];
	}

	/* NOTE: mailbox is probably the best for most applications
	 * but I may want the ability to pick a different mode in
	 * very niche circumstances.
	 *
	 * We also may want immediate mode if we want to disable VSYNC.
	 */

	/* vsync: MAILBOX else FIFO. no vsync: MAILBOX else IMMEDIATE else FIFO.
	 * IMMEDIATE on X11 ARGB blocks in vkQueuePresentKHR; MAILBOX does not. */
	VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
	VkPresentModeKHR preferred = VK_PRESENT_MODE_MAILBOX_KHR;
	VkPresentModeKHR second = ctx->vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;
	int have_pref = 0;
	int have_second = 0;
	const char *mode_name = "FIFO";
	for (i = 0; i < vk_device.swapchain_support.present_mode_count; i++) {
		VkPresentModeKHR pres = vk_device.swapchain_support.present_modes[i];
		if (pres == preferred)
			have_pref = 1;
		if (pres == second)
			have_second = 1;
	}
	if (have_pref)
		present_mode = preferred;
	else if (have_second)
		present_mode = second;
	if (present_mode == VK_PRESENT_MODE_IMMEDIATE_KHR)
		mode_name = "IMMEDIATE";
	else if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
		mode_name = "MAILBOX";
	else if (present_mode == VK_PRESENT_MODE_FIFO_RELAXED_KHR)
		mode_name = "FIFO_RELAXED";
	PINFO("Present mode %s", mode_name);

	uint32_t img_count = surface_caps.minImageCount + (uint32_t)REND_VK_SWAPCHAIN_EXTRA;
	if (img_count < ctx->max_frames_in_flight + 1)
		img_count = (uint32_t)ctx->max_frames_in_flight + 1;
	if (surface_caps.maxImageCount > 0 && img_count > surface_caps.maxImageCount) {
		img_count = surface_caps.maxImageCount;
	}

	VkSwapchainCreateInfoKHR swapchain_create_info = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
	swapchain_create_info.surface = ctx->surface;
	swapchain_create_info.minImageCount = img_count;
	swapchain_create_info.imageFormat = swapchain->format.format;
	swapchain_create_info.imageColorSpace = swapchain->format.colorSpace;
	swapchain_create_info.imageExtent = swapchain_extent;
	swapchain_create_info.imageArrayLayers = 1;
	swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	/* index sharing */
	if (vk_device.graphics_family_index != vk_device.present_family_index) {
		queue_family_indices[0] = vk_device.graphics_family_index;
		queue_family_indices[1] = vk_device.present_family_index;
		swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapchain_create_info.queueFamilyIndexCount = 2;
		swapchain_create_info.pQueueFamilyIndices = queue_family_indices;
	} else {
		swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchain_create_info.queueFamilyIndexCount = 0;
		swapchain_create_info.pQueueFamilyIndices = 0;
	}

	swapchain_create_info.preTransform = surface_caps.currentTransform;
	{
		static const VkCompositeAlphaFlagBitsKHR alpha_pref[] = {
#ifdef REND_VK_COMPOSITE_PREFER_ALPHA
			VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
			VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
			VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
			VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
#else
			VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
			VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
			VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
#endif
		};
		uint32_t ai;

		swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		for (ai = 0; ai < sizeof alpha_pref / sizeof alpha_pref[0]; ai++) {
			if (surface_caps.supportedCompositeAlpha & alpha_pref[ai]) {
				swapchain_create_info.compositeAlpha = alpha_pref[ai];
				break;
			}
		}
		PINFO("Composite alpha %s",
			swapchain_create_info.compositeAlpha == VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR ? "OPAQUE" :
			swapchain_create_info.compositeAlpha == VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR ? "PRE" :
			swapchain_create_info.compositeAlpha == VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR ? "POST" : "INHERIT");
	}
	swapchain_create_info.presentMode = present_mode;
	swapchain_create_info.clipped = VK_TRUE;
	swapchain_create_info.oldSwapchain = old_swapchain;

	sc_res = vkCreateSwapchainKHR(vk_device.logical_device, &swapchain_create_info, vk_allocator, &swapchain->handle);
	if (sc_res != VK_SUCCESS) {
		PERROR("vkCreateSwapchainKHR failed (%d)", (int)sc_res);
		return false;
	}

	swapchain->image_count = 0;
	if (vkGetSwapchainImagesKHR(vk_device.logical_device, swapchain->handle, &swapchain->image_count, 0) != VK_SUCCESS) {
		vkDestroySwapchainKHR(vk_device.logical_device, swapchain->handle, vk_allocator);
		swapchain->handle = VK_NULL_HANDLE;
		return false;
	}

	swapchain->images = rmalloc(swapchain->image_count * sizeof(*swapchain->images));
	swapchain->views = rmalloc(swapchain->image_count * sizeof(*swapchain->views));
	swapchain->present_semaphores = rmalloc(swapchain->image_count * sizeof(*swapchain->present_semaphores));
	if (!swapchain->images || !swapchain->views || !swapchain->present_semaphores) {
		rfree(swapchain->images);
		rfree(swapchain->views);
		rfree(swapchain->present_semaphores);
		swapchain->images = 0;
		swapchain->views = 0;
		swapchain->present_semaphores = 0;
		vkDestroySwapchainKHR(vk_device.logical_device, swapchain->handle, vk_allocator);
		swapchain->handle = VK_NULL_HANDLE;
		return false;
	}
	memset(swapchain->present_semaphores, 0, swapchain->image_count * sizeof(*swapchain->present_semaphores));

	if (vkGetSwapchainImagesKHR(vk_device.logical_device, swapchain->handle, &swapchain->image_count, swapchain->images) != VK_SUCCESS) {
		rfree(swapchain->images);
		rfree(swapchain->views);
		rfree(swapchain->present_semaphores);
		swapchain->images = 0;
		swapchain->views = 0;
		swapchain->present_semaphores = 0;
		vkDestroySwapchainKHR(vk_device.logical_device, swapchain->handle, vk_allocator);
		swapchain->handle = VK_NULL_HANDLE;
		return false;
	}

	for (i = 0; i < swapchain->image_count; i++) {
		VkSemaphoreCreateInfo sem_info;
		VkImageViewCreateInfo view_info = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};

		view_info.image = swapchain->images[i];
		view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view_info.format = swapchain->format.format;
		view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view_info.subresourceRange.baseMipLevel = 0;
		view_info.subresourceRange.levelCount = 1;
		view_info.subresourceRange.baseArrayLayer = 0;
		view_info.subresourceRange.layerCount = 1;

		if (vkCreateImageView(vk_device.logical_device, &view_info, vk_allocator, &swapchain->views[i]) != VK_SUCCESS) {
			uint32_t j;
			for (j = 0; j < i; j++) {
				vkDestroyImageView(vk_device.logical_device, swapchain->views[j], vk_allocator);
				vkDestroySemaphore(vk_device.logical_device, swapchain->present_semaphores[j], vk_allocator);
			}
			rfree(swapchain->images);
			rfree(swapchain->views);
			rfree(swapchain->present_semaphores);
			swapchain->images = 0;
			swapchain->views = 0;
			swapchain->present_semaphores = 0;
			vkDestroySwapchainKHR(vk_device.logical_device, swapchain->handle, vk_allocator);
			swapchain->handle = VK_NULL_HANDLE;
			return false;
		}

		sem_info = (VkSemaphoreCreateInfo){ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		if (vkCreateSemaphore(vk_device.logical_device, &sem_info, vk_allocator, &swapchain->present_semaphores[i]) != VK_SUCCESS) {
			uint32_t j;
			PERROR("Unable to create present semaphore #%u!", i);
			vkDestroyImageView(vk_device.logical_device, swapchain->views[i], vk_allocator);
			for (j = 0; j < i; j++) {
				vkDestroyImageView(vk_device.logical_device, swapchain->views[j], vk_allocator);
				vkDestroySemaphore(vk_device.logical_device, swapchain->present_semaphores[j], vk_allocator);
			}
			rfree(swapchain->images);
			rfree(swapchain->views);
			rfree(swapchain->present_semaphores);
			swapchain->images = 0;
			swapchain->views = 0;
			swapchain->present_semaphores = 0;
			vkDestroySwapchainKHR(vk_device.logical_device, swapchain->handle, vk_allocator);
			swapchain->handle = VK_NULL_HANDLE;
			return false;
		}
	}

	swapchain->extent = swapchain_extent;
	if (!rend_vk14_depth_ensure_img(ctx, &ctx->swap_depth, swapchain_extent.width, swapchain_extent.height)) {
		rend_vk14_swapchain_destroy(ctx, swapchain);
		return false;
	}
	if (ctx->window) {
		ctx->window_w = ctx->window->width;
		ctx->window_h = ctx->window->height;
	}
	return true;
}

static void
rend_vk14_swapchain_destroy(RendVk14Context *ctx, RendVkSwapchain *swapchain)
{
	uint32_t i;

	(void)ctx;
	if (!swapchain || swapchain->handle == VK_NULL_HANDLE) return;
	RASSERT(vk_device.logical_device);

	if (swapchain->present_semaphores) {
		for (i = 0; i < swapchain->image_count; i++) {
			if (swapchain->present_semaphores[i])
				vkDestroySemaphore(vk_device.logical_device, swapchain->present_semaphores[i], vk_allocator);
		}
		rfree(swapchain->present_semaphores);
		swapchain->present_semaphores = 0;
	}

	if (swapchain->views) {
		for (i = 0; i < swapchain->image_count; i++) {
			vkDestroyImageView(vk_device.logical_device, swapchain->views[i], vk_allocator);
		}
		rfree(swapchain->views);
		swapchain->views = 0;
	}

	if (swapchain->images) {
		rfree(swapchain->images);
		swapchain->images = 0;
	}

	swapchain->image_count = 0;
	vkDestroySwapchainKHR(vk_device.logical_device, swapchain->handle, vk_allocator);
	swapchain->handle = VK_NULL_HANDLE;
}

static bool
rend_vk14_swapchain_recreate(RendVk14Context *ctx)
{
	RendVkSwapchain old;
	RendVkSwapchain created;

	old = ctx->swapchain;
	memset(&created, 0, sizeof created);
	if (!rend_vk14_swapchain_create(ctx, &created, old.handle))
		return false;
	rend_vk14_swapchain_destroy(ctx, &old);
	ctx->swapchain = created;
	ctx->color_rend_format = rend_vk14_format_from_vk(created.format.format);
	return rend_vk14_color_targets_rebuild(ctx);
}

static RendFormat
rend_vk14_format_from_vk(VkFormat fmt)
{
	uint32_t i;

	if (fmt == VK_FORMAT_UNDEFINED)
		return REND_FORMAT_UNDEFINED;
	for (i = 1; i < REND_FORMAT_COUNT; i++) {
		if (vk_format_from_rend_format[i] == fmt)
			return (RendFormat)i;
	}
	return REND_FORMAT_UNDEFINED;
}

static void
rend_vk14_color_targets_free(RendVk14Context *ctx)
{
	if (!ctx)
		return;
	rfree(ctx->color_targets);
	ctx->color_targets = NULL;
	ctx->color_target_count = 0;
}

static bool
rend_vk14_color_targets_rebuild(RendVk14Context *ctx)
{
	uint32_t i;
	RendTexture *targets;

	rend_vk14_color_targets_free(ctx);
	if (!ctx->swapchain.image_count || !ctx->swapchain.images || !ctx->swapchain.views)
		return false;

	targets = rmalloc(ctx->swapchain.image_count * sizeof *targets);
	if (!targets)
		return false;
	memset(targets, 0, ctx->swapchain.image_count * sizeof *targets);

	for (i = 0; i < ctx->swapchain.image_count; i++) {
		targets[i].handle = (uint64_t)ctx->swapchain.images[i];
		targets[i].view = (uint64_t)ctx->swapchain.views[i];
		targets[i].width = ctx->swapchain.extent.width;
		targets[i].height = ctx->swapchain.extent.height;
		targets[i].depth = 1;
		targets[i].mip_levels = 1;
		targets[i].layers = 1;
		targets[i].format = ctx->color_rend_format;
		targets[i].layout = VK_IMAGE_LAYOUT_UNDEFINED;
		targets[i].backend = REND_BACKEND_VULKAN_14;
		targets[i].borrowed = 1;
		targets[i].ctx = ctx;
		targets[i].usage = ctx->offscreen
			? (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
			   VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
			: (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	}

	ctx->color_targets = targets;
	ctx->color_target_count = ctx->swapchain.image_count;
	return true;
}

static RendTexture *
rend_vk14_color_target_at(RendVk14Context *ctx)
{
	if (!ctx || !ctx->color_targets || ctx->image_index >= ctx->color_target_count)
		return NULL;
	return &ctx->color_targets[ctx->image_index];
}

RendTexture *
rend_vk14_color_target(RendContextHandle handle)
{
	return rend_vk14_color_target_at((RendVk14Context *)handle);
}


#endif

static void rend__pipeline_free_list(RendPipeline pipeline);
static bool rend__renderer_init(RendRenderer renderer, RendBackendType backend);
static RendRenderer rend__renderer_alloc(RendBackendType backend);
static void rend__renderer_link(RendRenderer rend);
static RendPipeline rend__pipeline_alloc(void);
static RendPipeline rend__pipeline_create(RendRenderer renderer, Rend__PipelineConfig config, uint8_t type,
    const uint8_t *a, size_t a_size, const uint8_t *b, size_t b_size);
static void *rend__staging_map(RendRenderer renderer, size_t size);
static void rend__staging_to_buffer(RendRenderer renderer, RendBuffer *dest, size_t dest_offset, const void *data, size_t size);
static void rend__staging_to_texture(RendRenderer renderer, RendTexture *texture, const void *data, size_t size);
static void rend__staging_from_texture(RendRenderer renderer, RendTexture *texture, void *dst, size_t size);

static RendRenderer rend_renderers_head = NULL;

bool rend_backend_vk_initialized = false;

RendVTable rend_vtables[REND_BACKEND_COUNT] = {
    [REND_BACKEND_AUTO] = {0},
	/* Deferred backends publish no callable entry points or contexts. */
	[REND_BACKEND_DIRECTX_12] = {0},
	[REND_BACKEND_METAL_4] = {0},
#ifdef PEAK_VULKAN
    [REND_BACKEND_VULKAN_14] = {
            .renderer_create = rend_vk14_renderer_create,
            .renderer_create_offscreen = rend_vk14_renderer_create_offscreen,
            .renderer_destroy = rend_vk14_renderer_destroy,
            .renderer_frame_begin = rend_vk14_renderer_frame_begin,
            .renderer_frame_end = rend_vk14_renderer_frame_end,
            .color_target = rend_vk14_color_target,

            .buffer_create = rend_vk14_buffer_create,
            .buffer_destroy = rend_vk14_buffer_destroy,
            .buffer_copy = rend_vk14_buffer_copy,

            .texture_create = rend_vk14_texture_create,
            .texture_destroy = rend_vk14_texture_destroy,
            .texture_copy_buffer = rend_vk14_texture_copy_buffer,
            .texture_copy_to_buffer = rend_vk14_texture_copy_to_buffer,
            .texture_blit = rend_vk14_texture_blit,

            .pipeline_create = rend_vk14_pipeline_create,
            .pipeline_bind = rend_vk14_pipeline_bind,
            .pipeline_push_constants = rend_vk14_pipeline_push_constants,

            .pipeline_bind_vertex_buffer = rend_vk14_pipeline_bind_vertex_buffer,
            .pipeline_bind_index_buffer = rend_vk14_pipeline_bind_index_buffer,

            .pipeline_dispatch = rend_vk14_pipeline_dispatch,
            .pipeline_draw = rend_vk14_pipeline_draw,
            .pipeline_draw_indexed = rend_vk14_pipeline_draw_indexed,
            .pipeline_set_blend = rend_vk14_pipeline_set_blend,

            .renderer_render_pass_begin = rend_vk14_renderer_render_pass_begin,
            .renderer_render_pass_begin_preserve = rend_vk14_renderer_render_pass_begin_preserve,
            .renderer_render_pass_begin_texture = rend_vk14_renderer_render_pass_begin_texture,
            .renderer_render_pass_end = rend_vk14_renderer_render_pass_end,
            .renderer_render_pass_end_texture = rend_vk14_renderer_render_pass_end_texture,

            .descriptor_write_buffer = rend_vk14_descriptor_write_buffer,
            .descriptor_write_texture = rend_vk14_descriptor_write_texture,
    },
#endif
};

extern void
rend_quit()
{
    while (rend_renderers_head)
        rend_renderer_destroy(rend_renderers_head);

#ifdef PEAK_VULKAN
    if (rend_backend_vk_initialized) {
        rend_vk_quit();
        rend_backend_vk_initialized = false;
    }
#endif
}

extern RendRenderer
rend_renderer_create(PeakWindow *target, RendBackendType backend, void *device, bool vsync, RendBindingInfo *bind_info)
{
    RendRenderer rend;

    (void)device;
    rend = rend__renderer_alloc(backend);
    if (!rend)
        return NULL;
    rend->vsync = vsync;
    if (bind_info)
        rend->bind_info = *bind_info;
    rend->window = target;
    if (rend_vtables[rend->backend].renderer_create)
        rend->context = rend_vtables[rend->backend].renderer_create(target, bind_info, vsync);
    if (!rend->context) {
        rfree(rend);
        return NULL;
    }
    rend__renderer_link(rend);
    return rend;
}

extern RendRenderer
rend_renderer_create_offscreen(uint32_t width, uint32_t height, RendFormat format, RendBackendType backend, RendBindingInfo *bind_info)
{
    RendRenderer rend;

    rend = rend__renderer_alloc(backend);
    if (!rend)
        return NULL;
    rend->vsync = false;
    if (bind_info)
        rend->bind_info = *bind_info;
    rend->window = NULL;
    if (rend_vtables[rend->backend].renderer_create_offscreen)
        rend->context = rend_vtables[rend->backend].renderer_create_offscreen(width, height, format, bind_info);
    if (!rend->context) {
        rfree(rend);
        return NULL;
    }
    rend__renderer_link(rend);
    return rend;
}

extern void
rend_renderer_destroy(RendRenderer renderer)
{
    RASSERT(renderer && renderer->context, "Invalid renderer.");
    if (!renderer)
        return;

    rend__pipeline_free_list(renderer->pipeline_head);
    renderer->pipeline_head = NULL;

    if (renderer->staging.handle)
        rend_vtables[renderer->backend].buffer_destroy(&renderer->staging);
    memset(&renderer->staging, 0, sizeof renderer->staging);

    if (renderer->backend != 0 && renderer->context) {
        rend_vtables[renderer->backend].renderer_destroy(renderer->context);
        renderer->context = NULL;
    }

    if (renderer->prev)
        renderer->prev->next = renderer->next;
    else
        rend_renderers_head = renderer->next;

    if (renderer->next)
        renderer->next->prev = renderer->prev;

    renderer->next = NULL;
    renderer->prev = NULL;
    rfree(renderer);
}

extern bool
rend_renderer_frame_begin(RendRenderer renderer)
{
    bool ok;

    RASSERT(renderer && (uintptr_t)renderer != 0xffffffff00000000 && "Invalid renderer. (Possible stack corruption.)");
    RASSERT(renderer->context, "Uninitialized renderer.");
    RASSERT(!renderer->in_frame, "Must not be called inside a frame.");
    RASSERT(!renderer->in_pass, "Must not be called inside a render pass.");

    ok = rend_vtables[renderer->backend].renderer_frame_begin(renderer->context);
    renderer->in_frame = ok ? 1 : 0;
    return ok;
}

extern void
rend_renderer_frame_end(RendRenderer renderer, float *delta)
{
    RASSERT(renderer && (uintptr_t)renderer != 0xffffffff00000000 && "Invalid renderer. (Possible stack corruption.)");
    RASSERT(renderer->context, "Uninitialized renderer.");
    RASSERT(renderer->in_frame, "Must be called inside a frame.");
    RASSERT(!renderer->in_pass, "Must not be called inside a render pass.");

    renderer->in_frame = 0;
    rend_vtables[renderer->backend].renderer_frame_end(renderer->context, delta);
    renderer->frame_count++;
}

extern RendTexture *
rend_renderer_color_target(RendRenderer renderer)
{
    RendTexture *tex;

    RASSERT(renderer && renderer->context, "Invalid renderer.");
    RASSERT(renderer->in_frame, "Must be called inside a frame.");
    if (!renderer || !renderer->context || !renderer->in_frame)
        return NULL;
    if (!rend_vtables[renderer->backend].color_target)
        return NULL;
    tex = rend_vtables[renderer->backend].color_target(renderer->context);
    return tex;
}

extern uint32_t
rend_texture_width(const RendTexture *texture)
{
    RASSERT(texture, "Invalid texture.");
    return texture ? texture->width : 0;
}

extern uint32_t
rend_texture_height(const RendTexture *texture)
{
    RASSERT(texture, "Invalid texture.");
    return texture ? texture->height : 0;
}

extern RendFormat
rend_texture_format(const RendTexture *texture)
{
    RASSERT(texture, "Invalid texture.");
    return texture ? (RendFormat)texture->format : REND_FORMAT_UNDEFINED;
}

extern uint64_t
rend_texture_id(RendTexture *texture)
{
    RASSERT(texture, "Invalid texture.");
    return texture ? texture->id : 0;
}

extern void
rend_renderer_read(RendRenderer renderer, void *dst, size_t size)
{
    RendTexture *tex;
    size_t need;

    RASSERT(renderer && renderer->context, "Invalid renderer.");
    RASSERT(dst, "Invalid destination.");
    RASSERT(!renderer->window, "renderer_read is for offscreen.");
    RASSERT(!renderer->in_frame, "Must be called outside a frame.");
    RASSERT(!renderer->in_pass, "Must be called outside a render pass.");
    if (!renderer || !renderer->context || !dst || !size)
        return;
    if (renderer->window || renderer->in_frame || renderer->in_pass)
        return;
    if (!rend_vtables[renderer->backend].color_target)
        return;
    tex = rend_vtables[renderer->backend].color_target(renderer->context);
    if (!tex)
        return;
    need = (size_t)tex->width * tex->height * rend_format_size[tex->format];
    RASSERT(size >= need, "renderer_read destination too small.");
    if (size < need)
        return;
    rend__staging_from_texture(renderer, tex, dst, need);
}

extern void
rend_renderer_render_pass_begin(RendRenderer renderer, float r, float g, float b, float a)
{
    rend_cmd_render_begin(renderer, r, g, b, a);
}

extern void
rend_renderer_render_pass_begin_texture(RendRenderer renderer, RendTexture *texture)
{
    rend_cmd_render_begin_texture(renderer, texture);
}

extern void
rend_renderer_render_pass_end_texture(RendRenderer renderer, RendTexture *texture)
{
    rend_cmd_render_end_texture(renderer, texture);
}

extern void
rend_renderer_render_pass_end(RendRenderer renderer)
{
    rend_cmd_render_end(renderer);
}

extern void
rend_descriptor_write_ubo(RendRenderer renderer, RendBuffer ubo, uint32_t binding, uint32_t slot)
{
    rend_vtables[renderer->backend].descriptor_write_buffer(renderer->context, ubo, binding, slot, 0, ubo.size, true);
}

extern void
rend_descriptor_write_ssbo(RendRenderer renderer, RendBuffer ssbo, uint32_t binding, uint32_t slot)
{
    rend_vtables[renderer->backend].descriptor_write_buffer(renderer->context, ssbo, binding, slot, 0, ssbo.size, false);
}

extern RendBuffer
rend_buffer_create(RendRenderer renderer, size_t size, RendBufferType type, bool gpu)
{
    RendBuffer buffer;

    buffer = rend_vtables[renderer->backend].buffer_create(renderer->context, size, type, gpu);
    buffer.backend = renderer->backend;
    return buffer;
}

extern void
rend_buffer_destroy(RendBuffer *buffer)
{
    rend_vtables[buffer->backend].buffer_destroy(buffer);
}

extern void
rend_buffer_write(RendRenderer renderer, RendBuffer *buffer, const void *data, size_t size, size_t offset)
{
    uint8_t *ptr;

    RASSERT(buffer && data, "Invalid buffer write.");
    RASSERT(offset <= buffer->size && size <= buffer->size - offset, "Write past end of buffer.");
    if (!buffer || !data || !size)
        return;
    if (offset > buffer->size || size > buffer->size - offset)
        return;
    if (buffer->mapped_memory) {
        ptr = buffer->mapped_memory;
        memcpy(ptr + offset, data, size);
        return;
    }
    rend__staging_to_buffer(renderer, buffer, offset, data, size);
}

extern void
rend_buffer_copy(RendRenderer renderer, RendBuffer *dest, size_t dest_offset, RendBuffer *src, size_t src_offset, size_t bytes)
{
    rend_vtables[renderer->backend].buffer_copy(renderer->context, dest, dest_offset, src, src_offset, bytes);
}

extern uint64_t
rend_buffer_address(RendBuffer *buffer)
{
    RASSERT(buffer != NULL);
    return buffer ? buffer->gpu_address : 0;
}

extern void *
rend_buffer_mapped(RendBuffer *buffer)
{
    RASSERT(buffer != NULL);
    return buffer ? buffer->mapped_memory : NULL;
}

extern RendTexture
rend_texture_create(RendRenderer renderer, uint32_t width, uint32_t height, uint32_t depth, uint32_t mip_levels, uint32_t layers, RendFormat format)
{
    RendTexture tex;

    tex = rend_vtables[renderer->backend].texture_create(renderer->context, width, height, depth, mip_levels, layers, format);
    tex.backend = renderer->backend;
    return tex;
}

extern RendTexture
rend_texture_create_from_data(RendRenderer renderer, const void *data, uint32_t width, uint32_t height, RendFormat format)
{
    RendTexture tex;
    uint32_t size;

    tex = rend_vtables[renderer->backend].texture_create(renderer->context, width, height, 1, 1, 1, format);
    tex.backend = renderer->backend;
    size = width * height * rend_format_size[format];
    rend__staging_to_texture(renderer, &tex, data, size);
    return tex;
}

extern void
rend_texture_copy_data(RendRenderer renderer, RendTexture *texture, const void *data, size_t size)
{
    rend__staging_to_texture(renderer, texture, data, size);
}

extern void
rend_texture_copy_buffer(RendRenderer renderer, RendTexture *texture, RendBuffer *buffer)
{
    rend_vtables[renderer->backend].texture_copy_buffer(renderer->context, texture, buffer);
}

extern void
rend_texture_read(RendRenderer renderer, RendTexture *texture, void *dst, size_t size)
{
    size_t need;

    RASSERT(renderer && renderer->context, "Invalid renderer.");
    RASSERT(texture, "Invalid texture.");
    RASSERT(dst, "Invalid destination.");
    RASSERT(!renderer->in_frame, "Must be called outside a frame.");
    RASSERT(!renderer->in_pass, "Must be called outside a render pass.");
    if (!renderer || !renderer->context || !texture || !dst || !size)
        return;
    if (renderer->in_frame || renderer->in_pass)
        return;
    need = (size_t)texture->width * texture->height * rend_format_size[texture->format];
    RASSERT(size >= need, "texture_read destination too small.");
    if (size < need)
        return;
    rend__staging_from_texture(renderer, texture, dst, need);
}

extern void
rend_texture_blit(RendRenderer renderer, RendTexture *src, RendTexture *dst, uint32_t src_x, uint32_t src_y, uint32_t src_w, uint32_t src_h, uint32_t dst_x, uint32_t dst_y, uint32_t dst_w, uint32_t dst_h)
{
    rend_vtables[renderer->backend].texture_blit(renderer->context, src, dst, src_x, src_y, src_w, src_h, dst_x, dst_y, dst_w, dst_h);
}

extern void
rend_texture_destroy(RendRenderer renderer, RendTexture *tex)
{
    rend_vtables[renderer->backend].texture_destroy(renderer->context, tex);
}

extern void
rend_pipeline_push_constants(RendPipeline pipeline, void *push_data, size_t size)
{
    rend_cmd_push_constants(pipeline, push_data, size);
}

extern void
rend_pipeline_bind_vertex_buffer(RendPipeline pipeline, uint32_t binding, RendBuffer buffer, size_t offset)
{
    rend_cmd_bind_vertex_buffer(pipeline, binding, buffer, offset);
}

extern void
rend_pipeline_bind_index_buffer(RendPipeline pipeline, RendBuffer buffer, size_t offset, RendIndexType index_type)
{
    rend_cmd_bind_index_buffer(pipeline, buffer, offset, index_type);
}

extern void
rend_pipeline_bind_texture(RendPipeline pipeline, RendTexture *texture, uint32_t binding, uint32_t slot)
{
    rend_vtables[pipeline->backend].descriptor_write_texture(pipeline->backend_ctx, texture, binding, slot);
}

extern void
rend_descriptor_write_texture(RendRenderer renderer, RendTexture *texture, uint32_t binding, uint32_t slot)
{
    rend_vtables[renderer->backend].descriptor_write_texture(renderer->context, texture, binding, slot);
}

extern RendPipeline
rend_pipeline_create_graphics_spirv(RendRenderer renderer, uint8_t *vertex_bytes, size_t vertex_size, uint8_t *frag_bytes, size_t frag_size, const RendVertexBinding *vertex_bindings, uint32_t vertex_binding_count, const RendVertexAttributes *vertex_attributes, uint32_t vertex_attribute_count, const RendPushConstantInfo *push_constants, uint32_t push_constant_count, RendPolygonMode polygon_mode, RendCullMode cull_mode, RendTopology topology, RendFormat color_format, bool depth_test_enable)
{
    Rend__PipelineConfig config;

    config = (Rend__PipelineConfig) {
        .vertex_bindings = vertex_bindings,
        .vertex_binding_count = vertex_binding_count,
        .vertex_attributes = vertex_attributes,
        .vertex_attribute_count = vertex_attribute_count,
        .push_constants = push_constants,
        .push_constant_count = push_constant_count,
        .polygon_mode = polygon_mode,
        .cull_mode = cull_mode,
        .topology = topology,
        .depth_test_enable = depth_test_enable,
        .color_format = color_format,
    };
    return rend__pipeline_create(renderer, config, REND__PIPELINE_GRAPHICS, vertex_bytes, vertex_size, frag_bytes, frag_size);
}

extern RendPipeline
rend_pipeline_create_graphics_bindless_spirv(RendRenderer renderer, uint8_t *vertex_bytes, size_t vertex_size, uint8_t *frag_bytes, size_t frag_size, const RendPushConstantInfo *push_constants, uint32_t push_constant_count, RendPolygonMode polygon_mode, RendCullMode cull_mode, RendTopology topology, RendFormat color_format, bool depth_test_enable)
{
    return rend_pipeline_create_graphics_spirv(renderer, vertex_bytes, vertex_size, frag_bytes, frag_size,
        NULL, 0, NULL, 0, push_constants, push_constant_count, polygon_mode, cull_mode, topology, color_format, depth_test_enable);
}

extern RendPipeline
rend_pipeline_create_compute_spirv(RendRenderer renderer, const uint8_t *compute_bytes, size_t compute_size, const RendPushConstantInfo *push_constants, uint32_t push_constant_count)
{
    Rend__PipelineConfig config;

    config = (Rend__PipelineConfig) {
        .push_constants = push_constants,
        .push_constant_count = push_constant_count,
    };
    return rend__pipeline_create(renderer, config, REND__PIPELINE_COMPUTE, compute_bytes, compute_size, NULL, 0);
}

extern RendPipeline
rend_pipeline_create_meshlet_spirv(RendRenderer renderer, uint8_t *meshlet_bytes, size_t meshlet_size, uint8_t *frag_bytes, size_t frag_size, const RendPushConstantInfo *push_constants, uint32_t push_constant_count, RendPolygonMode polygon_mode, RendCullMode cull_mode, bool depth_test_enable)
{
    Rend__PipelineConfig config;

    config = (Rend__PipelineConfig) {
        .push_constants = push_constants,
        .push_constant_count = push_constant_count,
        .polygon_mode = polygon_mode,
        .cull_mode = cull_mode,
        .depth_test_enable = depth_test_enable,
    };
    return rend__pipeline_create(renderer, config, REND__PIPELINE_MESH, meshlet_bytes, meshlet_size, frag_bytes, frag_size);
}

extern void
rend_pipeline_bind(RendPipeline pipeline)
{
    rend_cmd_bind_pipeline(pipeline);
}

extern void
rend_pipeline_dispatch(RendPipeline pipeline, uint32_t x, uint32_t y, uint32_t z)
{
    rend_cmd_dispatch(pipeline, x, y, z);
}

extern void
rend_pipeline_draw(RendPipeline pipeline, size_t count, uint32_t instance_count)
{
    rend_cmd_draw(pipeline, count, instance_count);
}

extern void
rend_pipeline_draw_indexed(RendPipeline pipeline, uint32_t index_count, uint32_t first_index, int32_t vertex_offset, uint32_t instance_count)
{
    rend_cmd_draw_indexed(pipeline, index_count, first_index, vertex_offset, instance_count);
}

extern void
rend_pipeline_set_blend(RendPipeline pipeline, bool blend)
{
    rend_vtables[pipeline->backend].pipeline_set_blend(pipeline, blend);
}

extern void
rend_cmd_render_begin(RendRenderer renderer, float r, float g, float b, float a)
{
    renderer->in_pass = 1;
    rend_vtables[renderer->backend].renderer_render_pass_begin(renderer->context, r, g, b, a);
}

extern void
rend_cmd_render_begin_preserve(RendRenderer renderer)
{
    RASSERT(renderer && renderer->in_frame && !renderer->in_pass, "Must resume inside a frame, outside a pass.");
    if (!renderer || !renderer->in_frame || renderer->in_pass)
        return;
    renderer->in_pass = 1;
    rend_vtables[renderer->backend].renderer_render_pass_begin_preserve(renderer->context);
}

extern void
rend_cmd_render_begin_texture(RendRenderer renderer, RendTexture *texture)
{
    renderer->in_pass = 1;
    rend_vtables[renderer->backend].renderer_render_pass_begin_texture(renderer->context, texture);
}

extern void
rend_cmd_render_end(RendRenderer renderer)
{
    renderer->in_pass = 0;
    rend_vtables[renderer->backend].renderer_render_pass_end(renderer->context);
}

extern void
rend_cmd_render_end_texture(RendRenderer renderer, RendTexture *texture)
{
    renderer->in_pass = 0;
    rend_vtables[renderer->backend].renderer_render_pass_end_texture(renderer->context, texture);
}

extern void
rend_cmd_bind_pipeline(RendPipeline pipeline)
{
    rend_vtables[pipeline->backend].pipeline_bind(pipeline);
}

extern void
rend_cmd_bind_vertex_buffer(RendPipeline pipeline, uint32_t binding, RendBuffer buffer, size_t offset)
{
    rend_vtables[pipeline->backend].pipeline_bind_vertex_buffer(pipeline, binding, buffer, offset);
}

extern void
rend_cmd_bind_index_buffer(RendPipeline pipeline, RendBuffer buffer, size_t offset, RendIndexType index_type)
{
    rend_vtables[pipeline->backend].pipeline_bind_index_buffer(pipeline, buffer, offset, index_type);
}

extern void
rend_cmd_push_constants(RendPipeline pipeline, void *push_data, size_t size)
{
    RASSERT(pipeline != NULL && "Pipeline handle is NULL!");
    RASSERT(rend_vtables[pipeline->backend].pipeline_push_constants && "Backend function not implemented!");
    rend_vtables[pipeline->backend].pipeline_push_constants(pipeline, push_data, size);
}

extern void
rend_cmd_dispatch(RendPipeline pipeline, uint32_t x, uint32_t y, uint32_t z)
{
    rend_vtables[pipeline->backend].pipeline_dispatch(pipeline, x, y, z);
}

extern void
rend_cmd_draw(RendPipeline pipeline, size_t count, uint32_t instance_count)
{
    rend_vtables[pipeline->backend].pipeline_draw(pipeline, count, instance_count);
}

extern void
rend_cmd_draw_indexed(RendPipeline pipeline, uint32_t index_count, uint32_t first_index, int32_t vertex_offset, uint32_t instance_count)
{
    rend_vtables[pipeline->backend].pipeline_draw_indexed(pipeline, index_count, first_index, vertex_offset, instance_count);
}

extern void
rend_cmd_copy_buffer_to_texture(RendRenderer renderer, RendTexture *texture, RendBuffer *buffer)
{
    RASSERT(renderer && renderer->context, "Invalid renderer.");
    RASSERT(texture, "Invalid texture.");
    RASSERT(buffer, "Invalid buffer.");
    RASSERT(renderer->in_frame && "Must be called while rendering a frame!");
    RASSERT(!renderer->in_pass && "Must be called outside a render pass!");
    if (!renderer || !renderer->context || !texture || !buffer)
        return;
    if (!renderer->in_frame || renderer->in_pass)
        return;
    rend_vtables[renderer->backend].texture_copy_buffer(renderer->context, texture, buffer);
}

extern void
rend_cmd_blit(RendRenderer renderer, RendTexture *src, RendTexture *dst, uint32_t src_x, uint32_t src_y, uint32_t src_w, uint32_t src_h, uint32_t dst_x, uint32_t dst_y, uint32_t dst_w, uint32_t dst_h)
{
    RASSERT(renderer->in_frame && "Must be called while rendering a frame!");
    RASSERT(!renderer->in_pass && "Must be called outside a render pass!");
    rend_vtables[renderer->backend].texture_blit(renderer->context, src, dst, src_x, src_y, src_w, src_h, dst_x, dst_y, dst_w, dst_h);
}

static bool
rend__renderer_init(RendRenderer renderer, RendBackendType backend)
{
	switch (backend) {
	case REND_BACKEND_AUTO:
	case REND_BACKEND_VULKAN_14:
		break;
	case REND_BACKEND_DIRECTX_12:
		REND__WARN("DirectX 12 backend is not implemented (deferred stub); no fallback.");
		return 0;
	case REND_BACKEND_METAL_4:
		REND__WARN("Metal 4 backend is not implemented (deferred stub); no fallback.");
		return 0;
	default:
		REND__WARN("Invalid backend type!");
		return 0;
	}
#ifdef PEAK_VULKAN
    if (rend_vk_init()) {
        rend_backend_vk_initialized = true;
        renderer->backend = REND_BACKEND_VULKAN_14;
        return true;
    }
#else
    (void)renderer;
#endif
    return false;
}

static RendRenderer
rend__renderer_alloc(RendBackendType backend)
{
    RendRenderer rend;

    rend = rmalloc(sizeof *rend);
    if (!rend)
        return NULL;
    memset(rend, 0, sizeof *rend);
    if (!rend__renderer_init(rend, backend)) {
        rfree(rend);
        return NULL;
    }
    return rend;
}

static void
rend__renderer_link(RendRenderer rend)
{
    rend->next = rend_renderers_head;
    rend->prev = NULL;
    rend_renderers_head = rend;
    if (rend->next)
        rend->next->prev = rend;
}

static RendPipeline
rend__pipeline_alloc(void)
{
    RendPipeline pipeline;

    pipeline = rmalloc(sizeof *pipeline);
    if (!pipeline)
        return NULL;
    memset(pipeline, 0, sizeof *pipeline);
    return pipeline;
}

static RendPipeline
rend__pipeline_create(RendRenderer renderer, Rend__PipelineConfig config, uint8_t type,
    const uint8_t *a, size_t a_size, const uint8_t *b, size_t b_size)
{
    RendPipeline pipeline;

    RASSERT(renderer && renderer->context, "Invalid renderer.");
    if (!renderer || !renderer->context)
        return NULL;
    pipeline = rend__pipeline_alloc();
    if (!pipeline)
        return NULL;
    if (!rend_vtables[renderer->backend].pipeline_create(
            renderer->context, pipeline, config, type, a, a_size, b, b_size, NULL, 0)) {
        rfree(pipeline);
        return NULL;
    }
    pipeline->backend = renderer->backend;
    pipeline->type = type;
    pipeline->next = renderer->pipeline_head;
    pipeline->prev = NULL;
    renderer->pipeline_head = pipeline;
    if (pipeline->next)
        pipeline->next->prev = pipeline;
    return pipeline;
}

static void
rend__pipeline_free_list(RendPipeline pipeline)
{
    while (pipeline) {
        RendPipeline next = pipeline->next;
        pipeline->next = NULL;
        pipeline->prev = NULL;
        rfree(pipeline);
        pipeline = next;
    }
}

static void *
rend__staging_map(RendRenderer renderer, size_t size)
{
    RendVTable *vt;

    RASSERT(renderer && renderer->context, "Invalid renderer.");
    if (!renderer || !renderer->context || !size)
        return NULL;
    if (renderer->staging.mapped_memory && renderer->staging.size >= size)
        return renderer->staging.mapped_memory;
    vt = &rend_vtables[renderer->backend];
    if (renderer->staging.handle) {
        vt->buffer_destroy(&renderer->staging);
        memset(&renderer->staging, 0, sizeof renderer->staging);
    }
    renderer->staging = vt->buffer_create(
        renderer->context, size, REND_BUFFER_TRANSFER, false);
    renderer->staging.backend = renderer->backend;
    return renderer->staging.mapped_memory;
}

static void
rend__staging_to_buffer(RendRenderer renderer, RendBuffer *dest, size_t dest_offset, const void *data, size_t size)
{
    void *map;

    map = rend__staging_map(renderer, size);
    if (!map)
        return;
    memcpy(map, data, size);
    rend_vtables[renderer->backend].buffer_copy(
        renderer->context, dest, dest_offset, &renderer->staging, 0, size);
}

static void
rend__staging_to_texture(RendRenderer renderer, RendTexture *texture, const void *data, size_t size)
{
    void *map;

    map = rend__staging_map(renderer, size);
    if (!map)
        return;
    memcpy(map, data, size);
    rend_vtables[renderer->backend].texture_copy_buffer(renderer->context, texture, &renderer->staging);
}

static void
rend__staging_from_texture(RendRenderer renderer, RendTexture *texture, void *dst, size_t size)
{
    void *map;

    map = rend__staging_map(renderer, size);
    if (!map)
        return;
    rend_vtables[renderer->backend].texture_copy_to_buffer(renderer->context, texture, &renderer->staging);
    memcpy(dst, map, size);
}

#endif /* REND_IMPLEMENTATION && !REND_IMPLEMENTATION_INCLUDED */

/*
------------------------------------------------------------------------------
MIT License
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
*/
