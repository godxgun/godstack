/* Snake's bounded Rend2 bootstrap and rendering implementation. */
#include "snake_render.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rend_vk.h"

#define SNAKE_DISCOVERY_FAMILY_CAPACITY 4096u
#define SNAKE_WAIT_NS UINT64_MAX

static uint64_t snake_align_up(uint64_t n, uint64_t alignment);
static int snake_select_class(RendBackend backend, RendHeapKind kind, uint32_t required, uint32_t *out_class);
static int snake_make_texture(SnakeRender *r);
static int snake_make_depth(SnakeRender *r);
static int snake_make_pipeline(SnakeRender *r, const uint8_t *vertex_spv, size_t vertex_bytes, const uint8_t *fragment_spv, size_t fragment_bytes);
static int snake_render_failure(SnakeRender *r, int drain);

static uint64_t
snake_align_up(uint64_t n, uint64_t alignment)
{
	if (!alignment || (alignment & (alignment - 1)) || n > UINT64_MAX - (alignment - 1))
		return 0;
	return (n + alignment - 1) & ~(alignment - 1);
}

static int
snake_select_class(RendBackend backend, RendHeapKind kind, uint32_t required,
                   uint32_t *out_class)
{
	RendHeapClassInfo classes[32];
	uint32_t count, i;

	if (!rend_heap_classes(backend, kind, NULL, 0, &count) || count > 32)
		return 0;
	if (!count || !rend_heap_classes(backend, kind, classes, count, &count))
		return 0;
	for (i = 0; i < count; ++i) {
		if ((classes[i].memory_properties & required) == required) {
			*out_class = classes[i].id;
			return 1;
		}
	}
	fprintf(stderr, "snake: device has no suitable memory class for heap kind %u (required properties 0x%x)\n",
	        (unsigned)kind, required);
	return 0;
}

static int
snake_make_texture(SnakeRender *r)
{
	RendTextureDesc desc;
	RendStorageRequirements req;
	RendHeapDesc heap_desc;
	RendTextureViewDesc view_desc;
	uint32_t memory_class;

	memset(&desc, 0, sizeof desc);
	desc.width = r->width;
	desc.height = r->height;
	desc.depth = 1;
	desc.mip_levels = 1;
	desc.array_layers = 1;
	desc.samples = 1;
	desc.format = REND_FORMAT_RGBA8_UNORM;
	desc.usage = REND_TEXTURE_COLOR_ATTACHMENT | REND_TEXTURE_TRANSFER_SRC;
	if (!snake_select_class(r->backend, REND_HEAP_TEXTURE_STORAGE,
	                        REND_MEMORY_DEVICE_LOCAL, &memory_class) ||
	    !rend_texture_memory_requirements(r->backend, &desc, memory_class, &req))
		return 0;
	memset(&heap_desc, 0, sizeof heap_desc);
	heap_desc.kind = REND_HEAP_TEXTURE_STORAGE;
	heap_desc.memory_class = memory_class;
	heap_desc.size = req.size;
	if (!rend_heap_create(r->backend, &heap_desc, &r->texture_heap) ||
	    !rend_texture_create(r->backend, &desc, r->texture_heap, 0, req.size,
	                         &r->offscreen_texture))
		return 0;
	memset(&view_desc, 0, sizeof view_desc);
	view_desc.format = REND_FORMAT_RGBA8_UNORM;
	view_desc.view_type = REND_VIEW_2D;
	view_desc.aspect = REND_ASPECT_COLOR;
	view_desc.mip_count = 1;
	view_desc.layer_count = 1;
	if (!rend_texture_view_create(r->backend, r->offscreen_texture, &view_desc,
	                              &r->offscreen_view))
		return 0;
	r->format = REND_FORMAT_RGBA8_UNORM;
	return 1;
}

static int
snake_make_depth(SnakeRender *r)
{
	RendTextureDesc desc;
	RendTextureViewDesc view;
	RendStorageRequirements req;
	RendHeapDesc heap;
	uint32_t memory_class;
	memset(&desc, 0, sizeof desc);
	desc.width = r->width;
	desc.height = r->height;
	desc.depth = 1;
	desc.mip_levels = 1;
	desc.array_layers = 1;
	desc.samples = 1;
	desc.format = REND_FORMAT_D32_FLOAT;
	desc.usage = REND_TEXTURE_DEPTH_ATTACHMENT;
	if (!snake_select_class(r->backend, REND_HEAP_TEXTURE_STORAGE, REND_MEMORY_DEVICE_LOCAL, &memory_class) ||
	    !rend_texture_memory_requirements(r->backend, &desc, memory_class, &req))
		return 0;
	memset(&heap, 0, sizeof heap);
	heap.kind = REND_HEAP_TEXTURE_STORAGE;
	heap.memory_class = memory_class;
	heap.size = req.size;
	if (!rend_heap_create(r->backend, &heap, &r->depth_heap) ||
	    !rend_texture_create(r->backend, &desc, r->depth_heap, 0, req.size, &r->depth_texture))
		return 0;
	memset(&view, 0, sizeof view);
	view.format = REND_FORMAT_D32_FLOAT;
	view.view_type = REND_VIEW_2D;
	view.aspect = REND_ASPECT_DEPTH;
	view.mip_count = 1;
	view.layer_count = 1;
	return rend_texture_view_create(r->backend, r->depth_texture, &view, &r->depth_view);
}

static int
snake_make_pipeline(SnakeRender *r, const uint8_t *vertex_spv, size_t vertex_bytes,
                    const uint8_t *fragment_spv, size_t fragment_bytes)
{
	RendGraphicsPipelineDesc desc;

	memset(&desc, 0, sizeof desc);
	desc.vertex.bytes = vertex_spv;
	desc.vertex.size = vertex_bytes;
	desc.fragment.bytes = fragment_spv;
	desc.fragment.size = fragment_bytes;
	desc.color_format = r->format;
	desc.depth_format = REND_FORMAT_D32_FLOAT;
	desc.samples = 1;
	desc.abi_tag = REND_SHADER_ABI_SLANG_TYPED_POINTER_ROOT_V1;
	if (!rend_graphics_pipeline_create(r->backend, &desc, &r->pipeline)) {
		fprintf(stderr, "snake: Rend2 pipeline creation failed for RGBA8 shader ABI\n");
		return 0;
	}
	return 1;
}

int
snake_render_init(SnakeRender *r, int headless, uint32_t width, uint32_t height,
                  uint32_t device_index, PeakWindow *window, const uint8_t *vertex_spv,
                  size_t vertex_bytes, const uint8_t *fragment_spv, size_t fragment_bytes)
{
	RendMemoryRequirements req;
	RendDeviceInfo const *device;
	RendQueueParams queue_params;
	RendCommandPoolParams pool_params;
	RendParams params;
	RendHeapDesc heap_desc;
	RendStorageRequirements heap_req;
	RendDeviceLimits limits;
	RendPresentationInfo presentation_info;
	uint32_t extension_count, family_count, data_class, i;
	const char **extensions;
	void *base;
	uintptr_t aligned;
	int supported;
	uint64_t data_end;

	memset(r, 0, sizeof *r);
	r->headless = headless;
	r->window = window;
	r->vertex_spv = vertex_spv;
	r->fragment_spv = fragment_spv;
	r->vertex_bytes = vertex_bytes;
	r->fragment_bytes = fragment_bytes;
	r->width = width;
	r->height = height;
	if (!rend_discovery_memory_requirements(&req) ||
	    !(r->discovery_memory = malloc(req.size + req.alignment)))
		goto fail;
	aligned = ((uintptr_t)r->discovery_memory + req.alignment - 1) & ~(uintptr_t)(req.alignment - 1);
	base = (void *)aligned;
	extensions = NULL;
	extension_count = 0;
	if (!headless)
		extensions = peak_vulkan_get_extensions(&extension_count);
	if (!headless && !extensions)
		goto fail;
	if (!rend_discovery_create_with_extensions(base, req.size, r->family_storage,
	                                           SNAKE_DISCOVERY_FAMILY_CAPACITY, (const char *const *)extensions,
	                                           extension_count, &r->discovery))
		goto fail;
	if (!headless) {
		VkInstance instance;
		if (!rend_vk_discovery_instance(r->discovery, &instance) ||
		    !peak_vulkan_create_surface(window, instance, NULL, &r->surface))
			goto fail;
	}
	if (device_index >= rend_discovery_device_count(r->discovery)) {
		fprintf(stderr, "snake: --device %u is out of range (discovered %u device(s))\n",
		        device_index, rend_discovery_device_count(r->discovery));
		goto fail;
	}
	device = rend_discovery_device(r->discovery, device_index);
	if (!device || !rend_device_supports_profile(device, REND_PROFILE_GRAPHICS_COMPUTE)) {
		fprintf(stderr, "snake: selected device %u (%s) lacks Rend2 graphics/compute or typed-pointer shader ABI support\n",
		        device_index, device ? rend_device_name(device) : "unknown");
		goto fail;
	}
	family_count = 0;
	{
		const RendQueueFamilyInfo *device_families = rend_device_queue_families(device, &family_count);
		int selected = 0;
		for (i = 0; i < family_count; ++i) {
			if (!(device_families[i].capabilities & REND_QUEUE_GRAPHICS))
				continue;
			if (!headless) {
				if (!rend_device_surface_support(device, (uint64_t)(uintptr_t)r->surface, device_families[i].family_index, &supported))
					continue;
				if (!supported)
					continue;
			}
			queue_params.family_index = device_families[i].family_index;
			selected = 1;
			break;
		}
		if (!selected) {
			fprintf(stderr, "snake: selected device has no graphics queue%s\n",
			        headless ? "" : " that can present to this window");
			goto fail;
		}
	}
	queue_params.queue_index = 0;
	queue_params.submission_count = 1;
	queue_params.max_submit_command_lists = 1;
	queue_params.max_submit_waits = 0;
	memset(&pool_params, 0, sizeof pool_params);
	pool_params.family_index = queue_params.family_index;
	pool_params.command_list_count = 1;
	memset(&params, 0, sizeof params);
	params.profile = REND_PROFILE_GRAPHICS_COMPUTE;
	params.device = device;
	params.queues = &queue_params;
	params.command_pools = &pool_params;
	params.heap_count = headless ? 3 : 2;
	params.texture_count = headless ? 2 : 1;
	params.texture_view_count = headless ? 2 : 1;
	params.pipeline_count = 1;
	params.timeline_count = 1;
	params.presentation_count = headless ? 0 : 1;
	params.queue_count = 1;
	params.command_pool_count = 1;
	if (!rend_memory_requirements(&params, &req) ||
	    !(r->backend_memory = malloc(req.size + req.alignment)))
		goto fail;
	aligned = ((uintptr_t)r->backend_memory + req.alignment - 1) & ~(uintptr_t)(req.alignment - 1);
	if (!rend_place_in_memory((void *)aligned, req.size, &params, &r->backend))
		goto fail;
	if (!rend_queue_get(r->backend, 0, &r->queue) ||
	    !rend_command_pool_get(r->backend, 0, &r->pool) ||
	    !rend_backend_get_limits(r->backend, &limits))
		goto fail;
	if (limits.root_alignment == 0 || limits.max_push_bytes < sizeof(uint64_t)) {
		fprintf(stderr, "snake: device cannot push an aligned 64-bit root address (root record is %lu bytes)\n",
		        (unsigned long)sizeof(SnakeGpuRoot));
		goto fail;
	}
	if (!snake_select_class(r->backend, REND_HEAP_DATA,
	                        REND_MEMORY_HOST_VISIBLE, &data_class))
		goto fail;
	r->vertex_offset = 0;
	r->index_offset = snake_align_up(sizeof(SnakeGpuVertex) * SNAKE_GPU_VERTEX_COUNT, 2);
	r->instance_offset = snake_align_up(r->index_offset + sizeof(uint16_t) * 36, 16);
	r->root_offset = snake_align_up(r->instance_offset + sizeof(SnakeGpuInstance) * SNAKE_GPU_INSTANCE_CAPACITY,
	                                limits.root_alignment);
	r->readback_offset = snake_align_up(r->root_offset + sizeof(SnakeGpuRoot), 4);
	if (!r->index_offset || !r->instance_offset || !r->root_offset || !r->readback_offset ||
	    (headless && height && (uint64_t)width > (UINT64_MAX - r->readback_offset) / 4 / height))
		goto fail;
	data_end = r->readback_offset + (headless ? (uint64_t)width * height * 4 : 0);
	memset(&heap_desc, 0, sizeof heap_desc);
	heap_desc.kind = REND_HEAP_DATA;
	heap_desc.memory_class = data_class;
	heap_desc.size = data_end;
	if (!rend_heap_memory_requirements(r->backend, &heap_desc, &heap_req) ||
	    !rend_heap_create(r->backend, &heap_desc, &r->data_heap) ||
	    !rend_heap_get_info(r->backend, r->data_heap, &r->data_info) ||
	    !r->data_info.cpu_base || !r->data_info.gpu_base || r->data_info.size < data_end)
		goto fail;
	if (headless) {
		memset(&heap_desc, 0, sizeof heap_desc);
		heap_desc.kind = REND_HEAP_TEXTURE_STORAGE;
		if (!snake_make_texture(r))
			goto fail;
	} else {
		if (!rend_presentation_create(r->backend, (uint64_t)(uintptr_t)r->surface, width, height, &r->presentation) ||
		    !rend_presentation_get_info(r->presentation, &presentation_info))
			goto fail;
		r->width = presentation_info.width;
		r->height = presentation_info.height;
		r->format = presentation_info.format;
	}
	if (!snake_make_depth(r))
		goto fail;
	if (!snake_make_pipeline(r, vertex_spv, vertex_bytes, fragment_spv, fragment_bytes) ||
	    !rend_timeline_create(r->queue, 0, &r->timeline))
		goto fail;
	return 1;
fail: {
	RendDiagnostic diagnostic;
	if (rend_last_diagnostic(&diagnostic) && diagnostic.code != REND_DIAG_NONE)
		fprintf(stderr, "snake: initialization failed: %s (native %d)\n", diagnostic.message, diagnostic.native_code);
}
	if (!snake_render_destroy(r))
		fprintf(stderr, "snake: initialization cleanup failed; referenced storage retained\n");
	return 0;
}

int
snake_render_drain(SnakeRender *r)
{
	if (!r->backend)
		return 1;
	if (!rend_backend_wait_idle(r->backend)) {
		fprintf(stderr, "snake: explicit Rend2 GPU drain failed; potentially referenced storage retained\n");
		return 0;
	}
	r->submitted = 0;
	if (r->pool && !rend_command_pool_reset(r->pool)) {
		fprintf(stderr, "snake: command-pool retirement failed after explicit drain\n");
		return 0;
	}
	return 1;
}

int
snake_render_failure(SnakeRender *r, int drain)
{
	/* Cleanup operations may clear the diagnostic: preserve the work failure. */
	rend_last_diagnostic(&r->failure);
	if (drain && !snake_render_drain(r))
		rend_last_diagnostic(&r->failure);
	return 0;
}

int
snake_render_draw(SnakeRender *r, const SnakeGpuVertex *vertices,
                  const uint16_t *indices, uint32_t index_count, const SnakeGpuInstance *instances,
                  uint32_t instance_count, const SnakeGpuRoot *root, int readback)
{
	RendTextureView view;
	RendRenderAttachment attachment;
	RendRenderDesc render;
	RendCommandList commands;
	RendViewport viewport;
	RendScissor scissor;
	RendBarrier barrier;
	RendSubmitDesc submit;
	RendHeapRange range;
	uint64_t bytes;

	memset(&r->failure, 0, sizeof(r->failure));
	if (r->submitted) {
		if (!rend_timeline_wait(r->timeline, r->timeline_value, SNAKE_WAIT_NS))
			return snake_render_failure(r, 1);
		r->submitted = 0;
	}
	if (r->headless)
		view = r->offscreen_view;
	else if (!rend_presentation_acquire(r->presentation, SNAKE_WAIT_NS, &view))
		return snake_render_failure(r, 0);
	if (!rend_command_pool_reset(r->pool) ||
	    !rend_command_list_begin(r->pool, 0, &commands))
		goto acquired_fail;
	memcpy((uint8_t *)r->data_info.cpu_base + r->vertex_offset, vertices,
	       sizeof(SnakeGpuVertex) * SNAKE_GPU_VERTEX_COUNT);
	memcpy((uint8_t *)r->data_info.cpu_base + r->index_offset, indices,
	       sizeof(uint16_t) * index_count);
	memcpy((uint8_t *)r->data_info.cpu_base + r->instance_offset, instances,
	       sizeof(SnakeGpuInstance) * instance_count);
	{
		SnakeGpuRoot root_copy = *root;
		root_copy.vertices = r->data_info.gpu_base + r->vertex_offset;
		root_copy.instances = r->data_info.gpu_base + r->instance_offset;
		root_copy.instance_count = instance_count;
		memcpy((uint8_t *)r->data_info.cpu_base + r->root_offset, &root_copy, sizeof root_copy);
	}
	bytes = r->root_offset + sizeof *root - r->vertex_offset;
	if (!rend_heap_flush(r->backend, (RendHeapRange){r->data_heap, r->vertex_offset, bytes}))
		goto recording_fail;
	memset(&barrier, 0, sizeof barrier);
	barrier.producer_stages = REND_STAGE_HOST;
	barrier.producer_access = REND_ACCESS_WRITE;
	barrier.consumer_stages = REND_STAGE_VERTEX | REND_STAGE_INDEX_INPUT;
	barrier.consumer_access = REND_ACCESS_READ;
	if (!rend_cmd_barrier(commands, &barrier))
		goto recording_fail;
	memset(&attachment, 0, sizeof attachment);
	attachment.view = view;
	attachment.clear[0] = 0.04f;
	attachment.clear[1] = 0.05f;
	attachment.clear[2] = 0.06f;
	attachment.clear[3] = 1.0f;
	RendDepthAttachment depth_attachment;
	RendDepthState depth_state;
	memset(&depth_attachment, 0, sizeof depth_attachment);
	depth_attachment.view = r->depth_view;
	depth_attachment.clear_depth = 1.0f;
	memset(&render, 0, sizeof render);
	render.colors = &attachment;
	render.depth_attachment = &depth_attachment;
	render.color_count = 1;
	render.width = r->width;
	render.height = r->height;
	memset(&viewport, 0, sizeof viewport);
	viewport.width = (float)r->width;
	viewport.height = (float)r->height;
	viewport.max_depth = 1.0f;
	memset(&scissor, 0, sizeof scissor);
	scissor.width = r->width;
	scissor.height = r->height;
	memset(&depth_state, 0, sizeof depth_state);
	depth_state.test_enable = 1;
	depth_state.write_enable = 1;
	depth_state.compare = REND_COMPARE_LESS;
	if (!rend_cmd_begin_render(commands, &render) ||
	    !rend_cmd_set_pipeline(commands, r->pipeline) ||
	    !rend_cmd_set_depth_state(commands, &depth_state) ||
	    !rend_cmd_set_viewport(commands, &viewport) ||
	    !rend_cmd_set_scissor(commands, &scissor) ||
	    !rend_cmd_draw_indexed(commands, r->data_info.gpu_base + r->root_offset,
	                           r->data_info.gpu_base + r->index_offset, REND_INDEX_UINT16,
	                           index_count, instance_count, 0, 0, 0) ||
	    !rend_cmd_end_render(commands))
		goto recording_fail;
	if (readback) {
		memset(&barrier, 0, sizeof barrier);
		barrier.producer_stages = REND_STAGE_COLOR_OUTPUT;
		barrier.producer_access = REND_ACCESS_WRITE;
		barrier.consumer_stages = REND_STAGE_TRANSFER;
		barrier.consumer_access = REND_ACCESS_READ;
		if (!rend_cmd_barrier(commands, &barrier))
			goto recording_fail;
		bytes = (uint64_t)r->width * r->height * 4;
		range.heap = r->data_heap;
		range.offset = r->readback_offset;
		range.size = bytes;
		if (!rend_cmd_copy_texture_to_data(commands, r->offscreen_texture, range,
		                                   r->width, r->height))
			goto recording_fail;
		memset(&barrier, 0, sizeof barrier);
		barrier.producer_stages = REND_STAGE_TRANSFER;
		barrier.producer_access = REND_ACCESS_WRITE;
		barrier.consumer_stages = REND_STAGE_HOST;
		barrier.consumer_access = REND_ACCESS_READ;
		if (!rend_cmd_barrier(commands, &barrier))
			goto recording_fail;
	}
	if (!rend_command_list_end(commands))
		goto acquired_fail;
	memset(&submit, 0, sizeof submit);
	submit.commands = &commands;
	submit.command_count = 1;
	submit.completion.timeline = r->timeline;
	submit.completion.value = ++r->timeline_value;
	if (!rend_queue_submit(r->queue, &submit))
		/* Native submit failure may have accepted work; drain before reclamation. */
		return snake_render_failure(r, 1);
	r->submitted = 1;
	if (!r->headless && !rend_presentation_present(r->presentation, r->queue))
		return snake_render_failure(r, 1);
	return 1;
recording_fail:
acquired_fail:
	/* Reset discards failed recordings; presentation retirement needs an explicit drain. */
	return snake_render_failure(r, !r->headless);
}

int
snake_render_minimize(SnakeRender *r)
{
	if (r->headless || !snake_render_drain(r))
		return 0;
	if (r->presentation && !rend_presentation_destroy(r->presentation))
		return 0;
	r->presentation = NULL;
	if (r->depth_view && !rend_texture_view_destroy(r->backend, r->depth_view))
		return 0;
	r->depth_view = NULL;
	if (r->depth_texture && !rend_texture_destroy(r->backend, r->depth_texture))
		return 0;
	r->depth_texture = NULL;
	if (r->depth_heap && !rend_heap_destroy(r->backend, r->depth_heap))
		return 0;
	r->depth_heap = NULL;
	r->width = r->height = 0;
	return 1;
}

int
snake_render_recreate_presentation(SnakeRender *r, uint32_t width, uint32_t height)
{
	RendPresentationInfo info;
	if (r->headless || !width || !height)
		return 0;
	if (!snake_render_drain(r))
		return 0;
	if (r->presentation && !rend_presentation_destroy(r->presentation))
		return 0;
	r->presentation = NULL;
	if (r->depth_view) {
		if (!rend_texture_view_destroy(r->backend, r->depth_view))
			return 0;
		r->depth_view = NULL;
	}
	if (r->depth_texture) {
		if (!rend_texture_destroy(r->backend, r->depth_texture))
			return 0;
		r->depth_texture = NULL;
	}
	if (r->depth_heap) {
		if (!rend_heap_destroy(r->backend, r->depth_heap))
			return 0;
		r->depth_heap = NULL;
	}
	if (!rend_presentation_create(r->backend, (uint64_t)(uintptr_t)r->surface, width, height, &r->presentation))
		return 0;
	if (!rend_presentation_get_info(r->presentation, &info))
		return 0;
	r->width = info.width;
	r->height = info.height;
	if (!snake_make_depth(r))
		return 0;
	if (r->format != info.format) {
		if (r->pipeline && !rend_pipeline_destroy(r->backend, r->pipeline))
			return 0;
		r->pipeline = NULL;
		r->format = info.format;
		if (!snake_make_pipeline(r, r->vertex_spv, r->vertex_bytes,
		                         r->fragment_spv, r->fragment_bytes))
			return 0;
		return 2;
	}
	return 1;
}

int
snake_render_capture(SnakeRender *r, const char *ppm)
{
	FILE *f;
	uint8_t *pixels;
	uint32_t x, y;
	size_t n;
	int any = 0;
	if (!r->submitted || !rend_timeline_wait(r->timeline, r->timeline_value, SNAKE_WAIT_NS))
		return 0;
	r->submitted = 0;
	n = (size_t)r->width * r->height * 4;
	if (!rend_heap_invalidate(r->backend, (RendHeapRange){r->data_heap, r->readback_offset, n}))
		return 0;
	pixels = (uint8_t *)r->data_info.cpu_base + r->readback_offset;
	for (x = 0; x < n; ++x)
		any |= pixels[x] != 0;
	if (ppm) {
		f = fopen(ppm, "wb");
		if (!f)
			return 0;
		fprintf(f, "P6\n%u %u\n255\n", r->width, r->height);
		for (y = 0; y < r->height; ++y) {
			for (x = 0; x < r->width; ++x) {
				if (fwrite(pixels + ((size_t)y * r->width + x) * 4, 1, 3, f) != 3) {
					fclose(f);
					return 0;
				}
			}
		}
		if (fclose(f) != 0)
			return 0;
	}
	return any;
}

int
snake_render_destroy(SnakeRender *r)
{
	VkInstance instance;
	if (!r)
		return 1;
	if (r->backend && !snake_render_drain(r))
		return 0;
	if (r->presentation) {
		if (!rend_presentation_destroy(r->presentation))
			return 0;
		r->presentation = NULL;
	}
	if (r->offscreen_view) {
		if (!rend_texture_view_destroy(r->backend, r->offscreen_view))
			return 0;
		r->offscreen_view = NULL;
	}
	if (r->depth_view) {
		if (!rend_texture_view_destroy(r->backend, r->depth_view))
			return 0;
		r->depth_view = NULL;
	}
	if (r->pipeline) {
		if (!rend_pipeline_destroy(r->backend, r->pipeline))
			return 0;
		r->pipeline = NULL;
	}
	if (r->offscreen_texture) {
		if (!rend_texture_destroy(r->backend, r->offscreen_texture))
			return 0;
		r->offscreen_texture = NULL;
	}
	if (r->depth_texture) {
		if (!rend_texture_destroy(r->backend, r->depth_texture))
			return 0;
		r->depth_texture = NULL;
	}
	if (r->depth_heap) {
		if (!rend_heap_destroy(r->backend, r->depth_heap))
			return 0;
		r->depth_heap = NULL;
	}
	if (r->texture_heap) {
		if (!rend_heap_destroy(r->backend, r->texture_heap))
			return 0;
		r->texture_heap = NULL;
	}
	if (r->timeline) {
		if (!rend_timeline_destroy(r->timeline))
			return 0;
		r->timeline = NULL;
	}
	if (r->data_heap) {
		if (!rend_heap_destroy(r->backend, r->data_heap))
			return 0;
		r->data_heap = NULL;
	}
	if (r->backend) {
		if (!rend_backend_destroy(r->backend))
			return 0;
		r->backend = NULL;
	}
	if (r->surface && r->discovery) {
		if (!rend_vk_discovery_instance(r->discovery, &instance))
			return 0;
		vkDestroySurfaceKHR(instance, r->surface, NULL);
		r->surface = VK_NULL_HANDLE;
	}
	if (r->discovery) {
		if (!rend_discovery_destroy(r->discovery))
			return 0;
		r->discovery = NULL;
	}
	free(r->backend_memory);
	free(r->discovery_memory);
	memset(r, 0, sizeof *r);
	return 1;
}
