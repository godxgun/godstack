/*
Ideas for Rend 2.0 aka NoGL
Inspiried by https://www.sebastianaaltonen.com/blog/no-graphics-api 

## Style Guide

### Order of Code

1. Includes.
2. Macros.
3. Types.
4. Function declarations.
5. Global variables.
6. Function definitions.

### Naming Conventions

1. Use snake_case for variables and functions.
2. Functions in the format `module_type_action`:
 - `ui_align_left`
 - `ui_button_create`
 - `pl_window_size`
 3. Types are in PascalCase, prefixed also by the module name.
 - `UI_Align`, `PL_Window`. 
 4. Macros are in UPPERCASE_SNAKE_CASE.

*/

#include <stddef.h>
#include <stdint.h>

typedef struct NOGL_CommandBufferSpan {
    struct NOGL_CommandBuffer* data;
    size_t                     count;
} NOGL_CommandBufferSpan;

typedef enum NOGL_Memory {
    NOGL_MEMORY_DEFAULT,
    NOGL_MEMORY_UPLOAD,
    NOGL_MEMORY_READBACK,
} NOGL_Memory;

typedef enum NOGL_Stage {
    NOGL_STAGE_NONE,
    NOGL_STAGE_TRANSFER,
    NOGL_STAGE_COMPUTE,
    NOGL_STAGE_VERTEX,
    NOGL_STAGE_PIXEL,
} NOGL_Stage;

typedef enum NOGL_Op {
    NOGL_OP_EQUAL,
    NOGL_OP_NOT_EQUAL,
    NOGL_OP_GREATER,
    NOGL_OP_GREATER_EQUAL,
    NOGL_OP_LESS,
    NOGL_OP_LESS_EQUAL,
} NOGL_Op;

typedef enum NOGL_Signal {
    NOGL_SIGNAL_FENCE,
    NOGL_SIGNAL_TIMELINE,
} NOGL_Signal;

typedef uint32_t NOGL_HazardFlags;

typedef struct NOGL_TextureDesc      NOGL_TextureDesc;
typedef struct NOGL_ViewDesc         NOGL_ViewDesc;
typedef struct NOGL_RasterDesc       NOGL_RasterDesc;
typedef struct NOGL_DepthStencilDesc NOGL_DepthStencilDesc;
typedef struct NOGL_BlendDesc        NOGL_BlendDesc;
typedef struct NOGL_RenderPassDesc   NOGL_RenderPassDesc;

typedef struct NOGL_TextureSizeAlign {
    size_t size;
    size_t align;
} NOGL_TextureSizeAlign;

typedef struct NOGL_Texture           { uint64_t handle; } NOGL_Texture;
typedef struct NOGL_TextureDescriptor { uint64_t handle; } NOGL_TextureDescriptor;
typedef struct NOGL_Pipeline          { uint64_t handle; } NOGL_Pipeline;
typedef struct NOGL_DepthStencilState { uint64_t handle; } NOGL_DepthStencilState;
typedef struct NOGL_BlendState        { uint64_t handle; } NOGL_BlendState;
typedef struct NOGL_Queue             { uint64_t handle; } NOGL_Queue;
typedef struct NOGL_CommandBuffer     { uint64_t handle; } NOGL_CommandBuffer;
typedef struct NOGL_Semaphore         { uint64_t handle; } NOGL_Semaphore;

void*  nogl_mem_alloc(size_t bytes, NOGL_Memory memory = NOGL_MEMORY_DEFAULT);
void*  nogl_mem_alloc_aligned(size_t bytes, size_t align, NOGL_Memory memory = NOGL_MEMORY_DEFAULT);
void   nogl_mem_free(void* ptr);
void*  nogl_mem_host_to_device_ptr(void* ptr);

NOGL_TextureSizeAlign  nogl_texture_size_align(NOGL_TextureDesc desc);
NOGL_Texture           nogl_texture_create(NOGL_TextureDesc desc, void* ptr_gpu);
NOGL_TextureDescriptor nogl_texture_view_descriptor(NOGL_Texture texture, NOGL_ViewDesc desc);
NOGL_TextureDescriptor nogl_texture_rw_view_descriptor(NOGL_Texture texture, NOGL_ViewDesc desc);

NOGL_Pipeline nogl_pipeline_create_compute_spirv(uint8_t *bytes, size_t size);
NOGL_Pipeline nogl_pipeline_create_graphics_spirv(uint8_t *vertex_bytes, size_t vertex_size, uint8_t *frag_bytes, size_t frag_size, NOGL_RasterDesc desc);
NOGL_Pipeline nogl_pipeline_create_meshlet_spirv(uint8_t *meshlet_bytes, size_t meshlet_size, uint8_t *frag_bytes, size_t frag_size, NOGL_RasterDesc desc);
void          nogl_pipeline_destroy(NOGL_Pipeline pipeline);

NOGL_DepthStencilState nogl_depth_stencil_state_create(NOGL_DepthStencilDesc desc);
NOGL_BlendState        nogl_blend_state_create(NOGL_BlendDesc desc);
void                   nogl_depth_stencil_state_free(NOGL_DepthStencilState state);
void                   nogl_blend_state_free(NOGL_BlendState state);

NOGL_Queue         nogl_queue_create(/* device & queue creation details omitted */);
NOGL_CommandBuffer nogl_command_buffer_start(NOGL_Queue queue);
void               nogl_queue_submit(NOGL_Queue queue, NOGL_CommandBufferSpan command_buffers);

NOGL_Semaphore nogl_semaphore_create(uint64_t init_value);
void           nogl_semaphore_wait(NOGL_Semaphore sema, uint64_t value);
void           nogl_semaphore_destroy(NOGL_Semaphore sema);

void nogl_cmd_mem_copy(NOGL_CommandBuffer cb, void* dest_gpu, void* src_gpu, size_t bytes);
void nogl_cmd_copy_to_texture(NOGL_CommandBuffer cb, void* dest_gpu, void* src_gpu, NOGL_Texture texture);
void nogl_cmd_copy_from_texture(NOGL_CommandBuffer cb, void* dest_gpu, void* src_gpu, NOGL_Texture texture);

void nogl_cmd_set_active_texture_heap_ptr(NOGL_CommandBuffer cb, void* ptr_gpu);

void nogl_cmd_barrier(NOGL_CommandBuffer cb, NOGL_Stage before, NOGL_Stage after, NOGL_HazardFlags hazards = 0);
void nogl_cmd_signal_after(NOGL_CommandBuffer cb, NOGL_Stage before, void* ptr_gpu, uint64_t value, NOGL_Signal signal);
void nogl_cmd_wait_before(NOGL_CommandBuffer cb, NOGL_Stage after, void* ptr_gpu, uint64_t value, NOGL_Op op, NOGL_HazardFlags hazards, uint64_t mask);

void nogl_cmd_set_pipeline(NOGL_CommandBuffer cb, NOGL_Pipeline pipeline);
void nogl_cmd_set_depth_stencil_state(NOGL_CommandBuffer cb, NOGL_DepthStencilState state);
void nogl_cmd_set_blend_state(NOGL_CommandBuffer cb, NOGL_BlendState state);

void nogl_cmd_dispatch(NOGL_CommandBuffer cb, void* data_gpu, uint32_t count_x, uint32_t count_y, uint32_t count_z);
void nogl_cmd_dispatch_indirect(NOGL_CommandBuffer cb, void* data_gpu, void* grid_dimensions_gpu);

void nogl_cmd_render_pass_begin(NOGL_CommandBuffer cb, NOGL_RenderPassDesc desc);
void nogl_cmd_render_pass_end(NOGL_CommandBuffer cb);

void nogl_cmd_draw_indexed_instanced(NOGL_CommandBuffer cb, void* vertex_data_gpu, void* pixel_data_gpu, void* indices_gpu, uint32_t index_count, uint32_t instance_count);
void nogl_cmd_draw_indexed_instanced_indirect(NOGL_CommandBuffer cb, void* vertex_data_gpu, void* pixel_data_gpu, void* indices_gpu, void* args_gpu);
void nogl_cmd_draw_indexed_instanced_indirect_multi(NOGL_CommandBuffer cb, void* data_vx_gpu, uint32_t vx_stride, void* data_px_gpu, uint32_t px_stride, void* args_gpu, void* draw_count_gpu);

void nogl_cmd_draw_meshlets(NOGL_CommandBuffer cb, void* meshlet_data_gpu, void* pixel_data_gpu, uint32_t count_x, uint32_t count_y, uint32_t count_z);
void nogl_cmd_draw_meshlets_indirect(NOGL_CommandBuffer cb, void* meshlet_data_gpu, void* pixel_data_gpu, void* dim_gpu);
