#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "rend.h"
#include "rend_internal.h"
#include "rend_vk14.c"

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

RendVTable rend_vtables[] = {
    [REND_BACKEND_VULKAN_14] = {
            .renderer_create = rend_vk14_renderer_create,
            .renderer_create_offscreen = rend_vk14_renderer_create_offscreen,
            .renderer_destroy = rend_vk14_renderer_destroy,
            .renderer_frame_begin = rend_vk14_renderer_frame_begin,
            .renderer_frame_end = rend_vk14_renderer_frame_end,
            .color_target = rend_vk14_color_target,

            .buffer_create_lifetime = rend_vk14_buffer_create_lifetime,
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
            .renderer_render_pass_begin_texture = rend_vk14_renderer_render_pass_begin_texture,
            .renderer_render_pass_end = rend_vk14_renderer_render_pass_end,
            .renderer_render_pass_end_texture = rend_vk14_renderer_render_pass_end_texture,

            .descriptor_write_buffer = rend_vk14_descriptor_write_buffer,
            .descriptor_write_texture = rend_vk14_descriptor_write_texture,
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
    if (!rend_vtables[rend->backend].renderer_create_offscreen) {
        rfree(rend);
        return NULL;
    }
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

    buffer = rend_vtables[renderer->backend].buffer_create_lifetime(renderer->context, size, type, gpu, REND_LIFETIME_PERMANENT);
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
    RendBackendType first;
    RendBackendType last;
    RendBackendType i;

    if (backend == REND_BACKEND_AUTO) {
        first = (RendBackendType)(REND_BACKEND_AUTO + 1);
        last = (RendBackendType)(REND_BACKEND_COUNT - 1);
    } else if (backend > REND_BACKEND_AUTO && backend < REND_BACKEND_COUNT) {
        first = last = backend;
    } else {
        REND__CRASH("Invalid backend type!");
        return false;
    }

    for (i = first; i <= last; i++) {
        if (i == REND_BACKEND_VULKAN_14 && rend_vk_init()) {
            rend_backend_vk_initialized = true;
            renderer->backend = REND_BACKEND_VULKAN_14;
            return true;
        }
    }
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
    renderer->staging = vt->buffer_create_lifetime(
        renderer->context, size, REND_BUFFER_TRANSFER, false, REND_LIFETIME_PERMANENT);
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
