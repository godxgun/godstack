#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <unistd.h>

#include "rend.h"
#include "rend_internal.h"
#include "rend_vk.c"

/* clean up functions */

static void rend__pipeline_free_list(RendPipeline pipeline);
static bool rend__renderer_init_recursive(RendRenderer renderer, RendBackendType backend, bool auto_pick);

static RendRenderer rend_renderers_head = NULL; 

// TODO: automatically clear buffers and textures
// static RendBuffer   rend_buffer_head = NULL; 
// static RendTexture  rend_texture_head = NULL; 

bool rend_backend_vk_initialized = false;

RendVTable rend_vtables[] = {
    [REND_BACKEND_VULKAN_14] = {
            .renderer_create = rend_vk_renderer_create,
            .renderer_destroy = rend_vk_renderer_destroy,
            .renderer_frame_begin = rend_vk_renderer_frame_begin,
            .renderer_frame_end = rend_vk_renderer_frame_end,

            .buffer_create_lifetime = rend_vk_buffer_create_lifetime,
            .buffer_destroy = rend_vk_buffer_destroy,
            .buffer_copy = rend_vk_buffer_copy,

            .texture_create = rend_vk_texture_create,
            .texture_destroy = rend_vk_texture_destroy,
            .texture_copy_buffer = rend_vk_texture_copy_buffer,
            .texture_blit = rend_vk_texture_blit,

            .pipeline_create = rend_vk_pipeline_create,
            .pipeline_bind = rend_vk_pipeline_bind,
            .pipeline_push_constants = rend_vk_pipeline_push_constants,

            .pipeline_bind_vertex_buffer = rend_vk_pipeline_bind_vertex_buffer,
            .pipeline_bind_index_buffer = rend_vk_pipeline_bind_index_buffer,

            .pipeline_dispatch = rend_vk_pipeline_dispatch,
            .pipeline_draw = rend_vk_pipeline_draw,
            .pipeline_draw_indexed = rend_vk_pipeline_draw_indexed,
            .pipeline_set_blend = rend_vk_pipeline_set_blend,

            .renderer_render_pass_begin = rend_vk_renderer_render_pass_begin,
            .renderer_render_pass_begin_texture = rend_vk_renderer_render_pass_begin_texture,
            .renderer_render_pass_end = rend_vk_renderer_render_pass_end,
            .renderer_render_pass_end_texture = rend_vk_renderer_render_pass_end_texture,

            .descriptor_write_buffer = rend_vk_descriptor_write_buffer,
            .descriptor_write_texture = rend_vk_descriptor_write_texture,
    },
};

extern void
rend_quit()
{
    while (rend_renderers_head)
        rend_renderer_destroy(rend_renderers_head);

    if (rend_backend_vk_initialized) {
        rend_vk_quit();
        rend_backend_vk_initialized = false;
    }
}

extern RendRenderer
rend_renderer_create(PeakWindow *target, RendBackendType backend, void* device, bool vsync, RendBindingInfo *bind_info)
{
    RendRenderer rend = rmalloc(sizeof *rend);
    if (!rend) return NULL;
    memset(rend, 0, sizeof *rend);

    if (!rend__renderer_init_recursive(rend, backend, false)) {
        rfree(rend);
        return NULL;
    }

    rend->vsync = vsync;
    rend->bind_info = *bind_info;
    rend->window = target;

    rend->context = rend_vtables[rend->backend].renderer_create(target, bind_info);
    if (!rend->context) {
        rfree(rend);
        return NULL;
    }

    rend->next = rend_renderers_head;
    rend->prev = NULL;
    rend_renderers_head = rend;
    if (rend->next)
        rend->next->prev = rend;

    return rend;
}

extern void
rend_renderer_destroy(RendRenderer renderer)
{
    RASSERT(renderer && renderer->context, "Invalid renderer.");
    if (!renderer) return;

    rend__pipeline_free_list(renderer->pipeline_head);
    renderer->pipeline_head = NULL;

    if (renderer->backend != 0 && renderer->context) {
        rend_vtables[renderer->backend].renderer_destroy(renderer->context);
        renderer->context = NULL;
    }

    if (renderer->prev) renderer->prev->next = renderer->next;
    else rend_renderers_head = renderer->next;

    if (renderer->next) renderer->next->prev = renderer->prev;

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

extern void
rend_renderer_render_pass_begin(RendRenderer renderer, float r, float g ,float b, float a)
{
    renderer->in_pass = 1;
    rend_vtables[renderer->backend].renderer_render_pass_begin(renderer->context, r, g, b, a);
}

extern void
rend_renderer_render_pass_begin_texture(RendRenderer renderer, RendTexture *texture)
{
    renderer->in_pass = 1;
    rend_vtables[renderer->backend].renderer_render_pass_begin_texture(renderer->context, texture);
}

extern void
rend_renderer_render_pass_end_texture(RendRenderer renderer, RendTexture *texture)
{
    renderer->in_pass = 0;
    rend_vtables[renderer->backend].renderer_render_pass_end_texture(renderer->context, texture);
}


extern void
rend_renderer_render_pass_end(RendRenderer renderer)
{
    renderer->in_pass = 0;
    rend_vtables[renderer->backend].renderer_render_pass_end(renderer->context);
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
rend_buffer_create(RendRenderer renderer, size_t size, RendBufferType type, bool gpu) {
    RendBuffer buffer = rend_vtables[renderer->backend].buffer_create_lifetime(renderer->context, size, type, gpu, REND_LIFETIME_PERMANENT);
    buffer.backend = renderer->backend;
    return buffer;
}

extern void
rend_buffer_destroy(RendBuffer *buffer) {
    rend_vtables[buffer->backend].buffer_destroy(buffer);
}

extern void
rend_buffer_write(RendRenderer renderer, RendBuffer *buffer, const void *data, size_t size, size_t offset)
{
    if (buffer->mapped_memory) {
        uint8_t *ptr = buffer->mapped_memory;
        memcpy(ptr + offset, data, size);
    } else {
        /* if the memory is bound to the device we must create and deallocate a transfer buffer */
        RendBuffer transfer_buffer = rend_vtables[buffer->backend].buffer_create_lifetime(renderer->context, size, REND_BUFFER_TRANSFER, false, REND_LIFETIME_FRAME);
        memcpy(transfer_buffer.mapped_memory, data, size);
        rend_vtables[buffer->backend].buffer_copy(renderer->context, buffer, offset, &transfer_buffer, 0, size);
        rend_vtables[buffer->backend].buffer_destroy(&transfer_buffer);
    }
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
    return buffer->gpu_address;
}

extern RendTexture
rend_texture_create(RendRenderer renderer, uint32_t width, uint32_t height, uint32_t depth, uint32_t mip_levels, uint32_t layers, RendFormat format)
{
    RendTexture tex = rend_vtables[renderer->backend].texture_create(renderer->context, width, height, depth, mip_levels, layers, format);
    tex.backend = renderer->backend;
    return tex;
}

extern RendTexture
rend_texture_create_from_data(RendRenderer renderer, const void *data, uint32_t width, uint32_t height, RendFormat format)
{
    RendTexture tex = rend_vtables[renderer->backend].texture_create(renderer->context, width, height, 1, 1, 1, format);
    tex.backend = renderer->backend;

    uint32_t size = width * height * rend_format_size[format];

    RendBuffer staging_buffer = rend_vtables[renderer->backend].buffer_create_lifetime(renderer->context, size, REND_BUFFER_TRANSFER, false, REND_LIFETIME_FRAME);
    memcpy(staging_buffer.mapped_memory, data, size);
    rend_vtables[renderer->backend].texture_copy_buffer(renderer->context, &tex, &staging_buffer);
    rend_vtables[renderer->backend].buffer_destroy(&staging_buffer);
    return tex;
}

extern void
rend_texture_copy_data(RendRenderer renderer, RendTexture *texture, const void *data, size_t size)
{
    RendBuffer staging_buffer = rend_vtables[renderer->backend].buffer_create_lifetime(renderer->context, size, REND_BUFFER_TRANSFER, false, REND_LIFETIME_FRAME);
    memcpy(staging_buffer.mapped_memory, data, size);
    rend_vtables[renderer->backend].texture_copy_buffer(renderer->context, texture, &staging_buffer);
    rend_vtables[renderer->backend].buffer_destroy(&staging_buffer);
}


extern void 
rend_texture_copy_buffer(RendRenderer renderer, RendTexture *texture, RendBuffer *buffer)
{
    rend_vtables[renderer->backend].texture_copy_buffer(renderer->context, texture, buffer);
}

extern void
rend_texture_blit(RendRenderer renderer, RendTexture *src, RendTexture *dst, uint32_t src_x, uint32_t src_y, uint32_t src_w, uint32_t src_h, uint32_t dst_x, uint32_t dst_y, uint32_t dst_w, uint32_t dst_h)
{
    rend_vtables[renderer->backend].texture_blit(renderer->context, src, dst, src_x, src_y, src_w, src_h, dst_x, dst_y, dst_w, dst_h);
}

extern uint64_t 
rend_texture_id(RendTexture *texture)
{
    return texture->id;
}

extern void 
rend_texture_destroy(RendRenderer renderer, RendTexture *tex)
{
    rend_vtables[renderer->backend].texture_destroy(renderer->context, tex);
}

extern void
rend_pipeline_push_constants(RendPipeline pipeline, void *push_data, size_t size)
{
    RASSERT(pipeline != NULL && "Pipeline handle is NULL!");
    RASSERT(rend_vtables[pipeline->backend].pipeline_push_constants && "Backend function not implemented!");
    rend_vtables[pipeline->backend].pipeline_push_constants(pipeline, push_data, size);
}

extern void
rend_pipeline_bind_vertex_buffer(RendPipeline pipeline, uint32_t binding, RendBuffer buffer, size_t offset)
{
    rend_vtables[pipeline->backend].pipeline_bind_vertex_buffer(pipeline, binding, buffer, offset);
}

extern void
rend_pipeline_bind_index_buffer(RendPipeline pipeline, RendBuffer buffer, size_t offset, RendIndexType index_type)
{
    rend_vtables[pipeline->backend].pipeline_bind_index_buffer(pipeline, buffer, offset, index_type);
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
    RendPipeline pipeline = rmalloc(sizeof *pipeline);

    Rend__PipelineConfig config = {
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

    if (rend_vtables[renderer->backend].pipeline_create(renderer->context, pipeline, config, REND__PIPELINE_GRAPHICS, vertex_bytes, vertex_size, frag_bytes, frag_size, NULL, 0)) {
        pipeline->backend = renderer->backend;

        pipeline->next = renderer->pipeline_head;
        pipeline->prev = NULL;
        renderer->pipeline_head = pipeline;
        pipeline->type = REND__PIPELINE_GRAPHICS;
        if(pipeline->next) {
            pipeline->next->prev = pipeline;
        }

        return pipeline;
    } 
    rfree(pipeline);
    return NULL;
}

extern RendPipeline
rend_pipeline_create_graphics_bindless_spirv(RendRenderer renderer, uint8_t *vertex_bytes, size_t vertex_size, uint8_t *frag_bytes, size_t frag_size, const RendPushConstantInfo *push_constants, uint32_t push_constant_count,  RendPolygonMode polygon_mode, RendCullMode cull_mode, RendTopology topology, RendFormat color_format, bool depth_test_enable)
{
    RendPipeline pipeline = rmalloc(sizeof *pipeline);

    Rend__PipelineConfig config = {
        .vertex_bindings = NULL,
        .vertex_binding_count = 0,
        .vertex_attributes = NULL,
        .vertex_attribute_count = 0,
        .push_constants = push_constants,
        .push_constant_count = push_constant_count,
        .polygon_mode = polygon_mode,
        .cull_mode = cull_mode,
        .topology = topology,
        .depth_test_enable = depth_test_enable,

        .color_format = color_format,
    };

    if (rend_vtables[renderer->backend].pipeline_create(renderer->context, pipeline, config, REND__PIPELINE_GRAPHICS, vertex_bytes, vertex_size, frag_bytes, frag_size, NULL, 0)) {
        pipeline->backend = renderer->backend;

        pipeline->next = renderer->pipeline_head;
        pipeline->prev = NULL;
        renderer->pipeline_head = pipeline;
        pipeline->type = REND__PIPELINE_GRAPHICS;
        if(pipeline->next) {
            pipeline->next->prev = pipeline;
        }

        return pipeline;
    } 
    rfree(pipeline);
    return NULL;
}

extern RendPipeline
rend_pipeline_create_compute_spirv(RendRenderer renderer, const uint8_t *compute_bytes, size_t compute_size, const RendPushConstantInfo *push_constants, uint32_t push_constant_count)
{
    RendPipeline pipeline = rmalloc(sizeof *pipeline);
    Rend__PipelineConfig config = {
        .push_constants = push_constants,
        .push_constant_count = push_constant_count,
    };

    if (rend_vtables[renderer->backend].pipeline_create(renderer->context, pipeline, config, REND__PIPELINE_COMPUTE, compute_bytes, compute_size, NULL, 0, NULL, 0)) {
        pipeline->backend = renderer->backend;
        pipeline->type = REND__PIPELINE_COMPUTE;

        pipeline->next = renderer->pipeline_head;
        pipeline->prev = NULL;
        renderer->pipeline_head = pipeline;
        if(pipeline->next) {
            pipeline->next->prev = pipeline;
        }

        return pipeline;
    }
    rfree(pipeline);
    return NULL;
}

extern RendPipeline
rend_pipeline_create_meshlet_spirv(RendRenderer renderer, uint8_t *meshlet_bytes, size_t meshlet_size, uint8_t *frag_bytes, size_t frag_size, const RendPushConstantInfo *push_constants, uint32_t push_constant_count, RendPolygonMode polygon_mode, RendCullMode cull_mode, bool depth_test_enable)
{
    RendPipeline pipeline = rmalloc(sizeof *pipeline);
    Rend__PipelineConfig config = {
        .push_constants = push_constants,
        .push_constant_count = push_constant_count,
        .polygon_mode = polygon_mode,
        .cull_mode = cull_mode,
        .depth_test_enable = depth_test_enable
    };
    if (rend_vtables[renderer->backend].pipeline_create(renderer->context, pipeline, config, REND__PIPELINE_MESH, meshlet_bytes, meshlet_size, frag_bytes, frag_size, NULL, 0)) {
        pipeline->backend = renderer->backend;
        pipeline->type = REND__PIPELINE_MESH;

        pipeline->next = renderer->pipeline_head;
        pipeline->prev = NULL;
        renderer->pipeline_head = pipeline;
        if(pipeline->next) {
            pipeline->next->prev = pipeline;
        }

        return pipeline;
    }
    rfree(pipeline);
    return NULL;
}

extern void
rend_pipeline_bind(RendPipeline pipeline)
{
    rend_vtables[pipeline->backend].pipeline_bind(pipeline);
}

extern void
rend_pipeline_dispatch(RendPipeline pipeline, uint32_t x, uint32_t y, uint32_t z)
{
    rend_vtables[pipeline->backend].pipeline_dispatch(pipeline, x, y, z);
}

extern void
rend_pipeline_draw(RendPipeline pipeline, size_t count, uint32_t instance_count)
{
    rend_vtables[pipeline->backend].pipeline_draw(pipeline, count, instance_count);
}

extern void
rend_pipeline_draw_indexed(RendPipeline pipeline, uint32_t index_count, uint32_t first_index, int32_t vertex_offset, uint32_t instance_count)
{
    rend_vtables[pipeline->backend].pipeline_draw_indexed(pipeline, index_count, first_index, vertex_offset, instance_count);
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
    assert(pipeline != NULL && "Pipeline handle is NULL!");
    assert(rend_vtables[pipeline->backend].pipeline_push_constants && "Backend function not implemented!");
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
rend_cmd_blit(RendRenderer renderer, RendTexture *src, RendTexture *dst, uint32_t src_x, uint32_t src_y, uint32_t src_w, uint32_t src_h, uint32_t dst_x, uint32_t dst_y, uint32_t dst_w, uint32_t dst_h)
{
    RASSERT(renderer->in_frame && "Must be called while rendering a frame!");
    RASSERT(!renderer->in_pass && "Must be called outside a render pass!");
    rend_vtables[renderer->backend].texture_blit(renderer->context, src, dst, src_x, src_y, src_w, src_h, dst_x, dst_y, dst_w, dst_h);
}

static bool
rend__renderer_init_recursive(RendRenderer renderer, RendBackendType backend, bool auto_pick)
{
    /* set autopick to true and set backend to first backend*/
    if (backend == REND_BACKEND_AUTO) {
        backend++;
        auto_pick = true;
    }

    switch (backend) {
        default:
            /* unless using auto pick to pick renderers, crash */
            if (!auto_pick) REND__CRASH("Invalid backend type!");

        case REND_BACKEND_VULKAN_14:
            if (rend_vk_init()) {
                rend_backend_vk_initialized = true;
                renderer->backend = REND_BACKEND_VULKAN_14;
                return true;
            }
            break;
    }

    if (auto_pick) {
        backend++;
        if (backend >= REND_BACKEND_COUNT) {
            return false;
        }

        /* recursively try next backend */
        return rend__renderer_init_recursive(renderer, backend, auto_pick);
    }

    return false;
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
