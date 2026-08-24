#ifndef REND_INTERNAL_H
#define REND_INTERNAL_H

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

#if defined(REND_DEBUG_MEMORY)
#define rmalloc(size)  peak_debug_malloc_impl((size), __FILE__, __LINE__, __func__)
#define rrealloc(ptr, size) peak_debug_realloc_impl((ptr), (size), __FILE__, __LINE__, __func__)
#define rfree(ptr)     peak_debug_free_impl((ptr), __FILE__, __LINE__, __func__)
#else
#define rmalloc malloc
#define rrealloc(ptr, size) realloc((ptr), (size))
#define rfree   free
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

#include "rend.h"

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
    RendContextHandle (*renderer_create)(PeakWindow *window, RendBindingInfo *bind_info);

    void (*renderer_destroy)(RendContextHandle);

    bool (*renderer_frame_begin)(RendContextHandle);
    void (*renderer_frame_end)(RendContextHandle, float *delta);

    void (*descriptor_write_buffer)(RendContextHandle handle, RendBuffer ubo, uint32_t binding, uint32_t slot, uint32_t offset, uint32_t size, bool is_ubo);
    void (*descriptor_write_texture)(RendContextHandle handle, RendTexture *texture, uint32_t binding, uint32_t slot);

    RendBuffer   (*buffer_create_lifetime)(RendContextHandle handle, size_t size, RendBufferType type, bool gpu, int lifetime);
    void         (*buffer_destroy)(RendBuffer *buffer);
    void         (*buffer_copy)(RendContextHandle handle, RendBuffer *dest, size_t dest_offset, RendBuffer *src, size_t src_offset, size_t bytes);
    
    RendTexture  (*texture_create)(RendContextHandle handle, uint32_t width, uint32_t height, uint32_t depth, uint32_t mip_levels, uint32_t layers, RendFormat format);
    void         (*texture_destroy)(RendContextHandle handle, RendTexture *tex);
    void         (*texture_copy_buffer)(RendContextHandle handle, RendTexture *texture, RendBuffer *buffer);
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
    void (*renderer_render_pass_begin_texture)(RendContextHandle, RendTexture*);
    void (*renderer_render_pass_end)(RendContextHandle handle);
    void (*renderer_render_pass_end_texture)(RendContextHandle handle, RendTexture*);
} RendVTable;


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

    uint32_t texture_binding;
    uint32_t texture_count;
    uint32_t ubo_binding;
    uint32_t ubo_count;

    uint8_t backend;
    uint8_t in_frame;
    uint8_t in_pass;
    uint8_t vsync;
};


/* NOTE(vasco): Its simpler if backeds all use a uniform struct than
 * every single one having to basically redefine the same thing
 */

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
};

struct RendSpecs {
    bool graphics;
    bool transfer;
    bool compute;
    bool present;
    bool sampler_anisotropy;
    bool discrete_gpu;
};

#endif
