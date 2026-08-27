/*
 * Vulkan 1.4 backend. Per-renderer context is RendVk14Context.
 * Instance, allocator, device, and version-agnostic Vulkan helpers: rend_vk_internal.c.
 *
 * * 1.0.3 - @vasco - backend functions take RendContextHandle
 * * 1.0.7 - @vasco - resize: oldSwapchain recreate, surface extent, present OUT_OF_DATE
 * * 1.0.8 - @vasco - offscreen CLEAR on new images; texture destroy not in-frame
 * * 1.2.1 - @vasco - vsync, present queue, exclusive buffers, create cleanup, blend/indirect/delta
 * * 1.3.0 - @vasco - wrap swapchain/offscreen images as borrowed RendTexture
 * * 1.4.0 - @vasco - window pass uses color_target; drop renderer_read
 * * 1.5.0 - @vasco - in-frame copy_buffer; blit-only present barrier
 * * 1.5.1 - @vasco - no per-frame surface query; SUBOPTIMAL recreates once; host arena stays
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include "rend.h"
#include "rend_internal.h"
#include "rend_vk_internal.c"

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

	RendVkImage depth_attachment;
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
 * such as the instance, the allocator, the devices etc...
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

	RendVkArenaAllocator arena_persistent; /* magical arena allocator for every type of memory */
	RendVkArenaAllocator arena_frame;

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

	/* we must create arena before swapchain */
	ctx->arena_persistent = rend_vk_arena_create(
			vk_device.logical_device,
			vk_device.physical_device,
			vk_device.properties.limits,
			vk_allocator);

	ctx->arena_frame = rend_vk_arena_create(
			vk_device.logical_device,
			vk_device.physical_device,
			vk_device.properties.limits,
			vk_allocator);

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
			vk_device.properties.limits,
			vk_allocator);

	ctx->arena_frame = rend_vk_arena_create(
			vk_device.logical_device,
			vk_device.physical_device,
			vk_device.properties.limits,
			vk_allocator);

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
	rend_vk_arena_destroy(&ctx->arena_frame);
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

		.image = ctx->swapchain.depth_attachment.handle,

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

	rend_vk_arena_clear_all(&ctx->arena_frame);
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
	rend_vk14__renderer_render_pass_begin_internal(
			handle,
			r, g, b, a,
			color->view,
			(uint64_t)ctx->swapchain.depth_attachment.view,
			0, 0,
			color->width, color->height,
			VK_ATTACHMENT_LOAD_OP_CLEAR
	);
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
		rend_vk14__renderer_render_pass_begin_internal(
				handle,
				0, 0, 0, 0,
				(uint64_t)texture->view,
				(uint64_t)ctx->swapchain.depth_attachment.view,
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
rend_vk14_buffer_create_lifetime(RendContextHandle handle, size_t size, RendBufferType type, bool gpu, int lifetime)
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

	RendVkArenaAllocator *arena = (lifetime == REND_LIFETIME_FRAME) ? &ctx->arena_frame : &ctx->arena_persistent;
	RendMemory vk_memory = rend_vk_arena_alloc(arena, mem_reqs.size, index);

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
	tex.memory = rend_vk_arena_alloc(&ctx->arena_persistent, mem_requirements.size, index);

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
		.magFilter    = VK_FILTER_LINEAR, /* force sharp upscaling */
		.minFilter    = VK_FILTER_LINEAR, /* TODO: add filter setting to texture */
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,

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
	RASSERT(!ctx->in_frame, "must not destroy textures during a frame");
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
		depth_stencil.depthWriteEnable = VK_TRUE;
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
	RendMemory depth_mem;
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
	color_mem = rend_vk_arena_alloc(&ctx->arena_persistent, color.requirements.size, heap);
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

	if (!rend_vk_device_detect_depth_format(&vk_device)) {
		vk_device.depth_format = VK_FORMAT_UNDEFINED;
		PERROR("Failed to find a supported depth buffer format!");
		rend_vk14_offscreen_destroy(ctx);
		return false;
	}

	ctx->swapchain.depth_attachment = rend_vk_image_create(
			vk_device.logical_device,
			VK_IMAGE_TYPE_2D,
			width, height,
			vk_device.depth_format,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			1, 1, 1,
			VK_SAMPLE_COUNT_1_BIT,
			VK_SHARING_MODE_EXCLUSIVE);
	if (!ctx->swapchain.depth_attachment.handle) {
		PERROR("Failed to create offscreen depth image.");
		rend_vk14_offscreen_destroy(ctx);
		return false;
	}

	mem_type = rend_vk_image_required_memory_type(&ctx->swapchain.depth_attachment);
	heap = rend_vk_get_heap_index(mem_type, vk_device.device_index);
	depth_mem = rend_vk_arena_alloc(&ctx->arena_persistent, ctx->swapchain.depth_attachment.requirements.size, heap);
	rend_vk_image_bind_memory(&ctx->swapchain.depth_attachment, &depth_mem);
	rend_vk_image_view_create(
			&ctx->swapchain.depth_attachment,
			VK_IMAGE_VIEW_TYPE_2D,
			VK_IMAGE_ASPECT_DEPTH_BIT);
	if (!ctx->swapchain.depth_attachment.view) {
		PERROR("Failed to create offscreen depth view.");
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
	rend_vk_image_destroy(&ctx->swapchain.depth_attachment);
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

	/* vsync: MAILBOX else FIFO. no vsync: IMMEDIATE else MAILBOX else FIFO. */
	VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
	VkPresentModeKHR preferred = ctx->vsync ? VK_PRESENT_MODE_MAILBOX_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;
	VkPresentModeKHR second = ctx->vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_MAILBOX_KHR;
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

	uint32_t img_count = surface_caps.minImageCount + 1;
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
			VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
			VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
			VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
		};
		uint32_t ai;

		swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		for (ai = 0; ai < sizeof alpha_pref / sizeof alpha_pref[0]; ai++) {
			if (surface_caps.supportedCompositeAlpha & alpha_pref[ai]) {
				swapchain_create_info.compositeAlpha = alpha_pref[ai];
				break;
			}
		}
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

	/* depth resources */
	if (!rend_vk_device_detect_depth_format(&vk_device)) {
		vk_device.depth_format = VK_FORMAT_UNDEFINED;
		PERROR("Failed to find a supported depth buffer format!");
		rend_vk14_swapchain_destroy(ctx, swapchain);
		return false;
	}

	swapchain->depth_attachment = rend_vk_image_create(
			vk_device.logical_device,
			VK_IMAGE_TYPE_2D,
			swapchain_extent.width, swapchain_extent.height,
			vk_device.depth_format,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			1, /* depth */
			1, /* mip level */
			1, /* layers */
			VK_SAMPLE_COUNT_1_BIT,
			VK_SHARING_MODE_EXCLUSIVE
			);

	uint32_t mem_type = rend_vk_image_required_memory_type(&swapchain->depth_attachment);
	uint32_t depth_index = rend_vk_get_heap_index(mem_type, vk_device.device_index);

	RASSERT(ctx);
	RendMemory depth_mem = rend_vk_arena_alloc(&ctx->arena_persistent, swapchain->depth_attachment.requirements.size, depth_index);

	rend_vk_image_bind_memory(&swapchain->depth_attachment, &depth_mem);

	rend_vk_image_view_create(
			&swapchain->depth_attachment,
			VK_IMAGE_VIEW_TYPE_2D,
			VK_IMAGE_ASPECT_DEPTH_BIT
			);

	swapchain->extent = swapchain_extent;
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

	rend_vk_image_destroy(&swapchain->depth_attachment);

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

