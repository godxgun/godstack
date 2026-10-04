/* Rend backend-stub rejection and Vulkan depth lifetime regression. Requires Vulkan and validation layers.
 * Run from godstack with ./build rend test. No window is created.
 */
#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Peak sets Vulkan's platform macros before including Vulkan headers. */
#include "../Peak/peak.h"
#include "../Rend/rend.h"
#include <vulkan/vulkan.h>

#define TEST_MEMORY_MAX 256

typedef struct TestMemory {
	VkDeviceMemory handle;
	VkDeviceSize size;
} TestMemory;

static PeakCtx *peak_demo_ctx;

static VkResult test_memory_alloc(VkDevice device, const VkMemoryAllocateInfo *info, const VkAllocationCallbacks *allocator, VkDeviceMemory *memory);
static void test_memory_free(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks *allocator);
static VkResult test_memory_bind(VkDevice device, VkImage image, VkDeviceMemory memory, VkDeviceSize offset);
static VkResult test_image_view_create(VkDevice device, const VkImageViewCreateInfo *info, const VkAllocationCallbacks *allocator, VkImageView *view);
static VKAPI_ATTR VkBool32 VKAPI_CALL test_validation(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT types, const VkDebugUtilsMessengerCallbackDataEXT *data, void *user);
static void test_backend_stubs(void);

static TestMemory test_memory[TEST_MEMORY_MAX];
static uint32_t test_memory_count;
static VkDeviceSize test_memory_bytes;
static int test_fail_alloc;
static int test_fail_bind;
static int test_fail_view;
static int test_validation_errors;

#include "../Peak/peak.c"
/* Count Rend-owned device allocations, not the driver's host allocations. */
#define vkAllocateMemory(...) test_memory_alloc(__VA_ARGS__)
#define vkFreeMemory(...) test_memory_free(__VA_ARGS__)
#define vkBindImageMemory(...) test_memory_bind(__VA_ARGS__)
#define vkCreateImageView(...) test_image_view_create(__VA_ARGS__)
#include "../Rend/rend.c"
#undef vkAllocateMemory
#undef vkFreeMemory
#undef vkBindImageMemory
#undef vkCreateImageView

static VkResult
test_memory_alloc(VkDevice device, const VkMemoryAllocateInfo *info, const VkAllocationCallbacks *allocator, VkDeviceMemory *memory)
{
	VkResult result;

	assert(!allocator);
	if (test_fail_alloc) {
		test_fail_alloc = 0;
		return VK_ERROR_OUT_OF_DEVICE_MEMORY;
	}
	result = vkAllocateMemory(device, info, allocator, memory);
	if (result == VK_SUCCESS) {
		assert(test_memory_count < TEST_MEMORY_MAX);
		test_memory[test_memory_count++] = (TestMemory){*memory, info->allocationSize};
		test_memory_bytes += info->allocationSize;
	}
	return result;
}

static void
test_memory_free(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks *allocator)
{
	uint32_t i;

	assert(!allocator);
	for (i = 0; i < test_memory_count; i++) {
		if (test_memory[i].handle == memory)
			break;
	}
	assert(i < test_memory_count);
	test_memory_bytes -= test_memory[i].size;
	test_memory[i] = test_memory[--test_memory_count];
	vkFreeMemory(device, memory, allocator);
}

static VkResult
test_memory_bind(VkDevice device, VkImage image, VkDeviceMemory memory, VkDeviceSize offset)
{
	if (test_fail_bind) {
		test_fail_bind = 0;
		return VK_ERROR_OUT_OF_DEVICE_MEMORY;
	}
	return vkBindImageMemory(device, image, memory, offset);
}

static VkResult
test_image_view_create(VkDevice device, const VkImageViewCreateInfo *info, const VkAllocationCallbacks *allocator, VkImageView *view)
{
	assert(!allocator);
	if (test_fail_view) {
		test_fail_view = 0;
		return VK_ERROR_OUT_OF_HOST_MEMORY;
	}
	return vkCreateImageView(device, info, allocator, view);
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
test_validation(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT types, const VkDebugUtilsMessengerCallbackDataEXT *data, void *user)
{
	(void)types;
	(void)data;
	(void)user;
	if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		test_validation_errors++;
	return VK_FALSE;
}

void
test_backend_stubs(void)
{
	const RendBackendType backends[] = {REND_BACKEND_DIRECTX_12, REND_BACKEND_METAL_4};
	RendRenderer head = rend_renderers_head;
	uint32_t count = test_memory_count;
	VkDeviceSize bytes = test_memory_bytes;
	int initialized = rend_backend_vk_initialized;
	size_t i;

	for (i = 0; i < sizeof(backends) / sizeof(backends[0]); i++) {
		assert(!rend_vtables[backends[i]].renderer_create);
		assert(!rend_vtables[backends[i]].renderer_create_offscreen);
		assert(!rend_renderer_create(NULL, backends[i], NULL, 0, NULL));
		assert(!rend_renderer_create_offscreen(64, 64, REND_FORMAT_R8G8B8A8_UNORM, backends[i], NULL));
		assert(rend_renderers_head == head);
		assert(rend_backend_vk_initialized == initialized);
		assert(test_memory_count == count && test_memory_bytes == bytes);
	}
}

int
main(void)
{
	RendRenderer renderer;
	RendVk14Context *ctx;
	RendTexture targets[2];
	VkDebugUtilsMessengerEXT messenger;
	VkDebugUtilsMessengerCreateInfoEXT debug;
	PFN_vkCreateDebugUtilsMessengerEXT create_messenger;
	PFN_vkDestroyDebugUtilsMessengerEXT destroy_messenger;
	VkDeviceSize baseline_bytes, size_bytes[2];
	VkImage old_image;
	VkDeviceMemory old_memory;
	uint32_t baseline_count;
	unsigned char pixels[64 * 64 * 4];
	int i, j;

	assert((peak_demo_ctx = peak_init_legacy()));
	puts("rend_vk: rejecting deferred backends before/after Vulkan creation (eight expected warnings)");
	test_backend_stubs();
	renderer = rend_renderer_create_offscreen(64, 64, REND_FORMAT_R8G8B8A8_UNORM, REND_BACKEND_AUTO, NULL);
	assert(renderer && renderer->backend == REND_BACKEND_VULKAN_14);
	ctx = renderer->context;
	assert(ctx->swap_depth.owned_memory);
	assert(!vk_allocator);
	test_backend_stubs();

	debug = (VkDebugUtilsMessengerCreateInfoEXT) {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
		.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
		.pfnUserCallback = test_validation,
	};
	create_messenger = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vk_instance, "vkCreateDebugUtilsMessengerEXT");
	destroy_messenger = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vk_instance, "vkDestroyDebugUtilsMessengerEXT");
	assert(create_messenger && destroy_messenger);
	assert(create_messenger(vk_instance, &debug, NULL, &messenger) == VK_SUCCESS);

	/* Window-depth replacement uses this same path, outside recording. */
	baseline_count = test_memory_count;
	for (i = 0; i < 64; i++) {
		assert(rend_vk14_depth_ensure_img(ctx, &ctx->swap_depth, i % 2 ? 96 : 64, 64));
		assert(test_memory_count == baseline_count);
		assert(ctx->stale_depth_count == 0);
		if (i < 2)
			size_bytes[i] = test_memory_bytes;
		else
			assert(test_memory_bytes == size_bytes[i % 2]);
	}
	assert(rend_vk14_depth_ensure_img(ctx, &ctx->swap_depth, 64, 64));
	baseline_bytes = test_memory_bytes;
	old_image = ctx->swap_depth.handle;
	old_memory = ctx->swap_depth.owned_memory;

	/* A failed replacement must clean up and leave the current image usable. */
	puts("rend_vk: injecting allocation/binding/view failures (three expected errors)");
	fflush(stdout);
	for (i = 0; i < 3; i++) {
		test_fail_alloc = i == 0;
		test_fail_bind = i == 1;
		test_fail_view = i == 2;
		assert(!rend_vk14_depth_ensure_img(ctx, &ctx->swap_depth, 128, 128));
		assert(ctx->swap_depth.handle == old_image);
		assert(ctx->swap_depth.owned_memory == old_memory);
		assert(test_memory_count == baseline_count && test_memory_bytes == baseline_bytes);
	}

	targets[0] = rend_texture_create(renderer, 32, 32, 1, 1, 1, REND_FORMAT_R8G8B8A8_UNORM);
	targets[1] = rend_texture_create(renderer, 64, 64, 1, 1, 1, REND_FORMAT_R8G8B8A8_UNORM);
	assert(targets[0].handle && targets[1].handle);
	baseline_count = test_memory_count;
	for (i = 0; i < 8; i++) {
		assert(rend_renderer_frame_begin(renderer));
		assert(ctx->stale_depth_count == 0);
		assert(test_memory_count == baseline_count + (i ? 1 : 0));
		/* More replacements than the former eight-entry retirement limit. */
		for (j = 0; j < 20; j++) {
			rend_cmd_render_begin_texture(renderer, &targets[j % 2]);
			rend_cmd_render_end_texture(renderer, &targets[j % 2]);
		}
		assert(ctx->stale_depth_count == (uint32_t)(i ? 20 : 19));
		assert(test_memory_count == baseline_count + (i ? 21 : 20));
		rend_cmd_render_begin(renderer, 1.f, 0.f, 0.f, 1.f);
		rend_cmd_render_end(renderer);
		rend_renderer_frame_end(renderer, NULL);
		if (i == 1)
			baseline_bytes = test_memory_bytes;
		else if (i > 1)
			assert(test_memory_bytes == baseline_bytes);
	}

	rend_renderer_read(renderer, pixels, sizeof(pixels));
	for (i = 0; i < 64 * 64; i++) {
		assert(pixels[i * 4] == 255 && pixels[i * 4 + 1] == 0);
		assert(pixels[i * 4 + 2] == 0 && pixels[i * 4 + 3] == 255);
	}
	/* Clear base -> transfer blit -> preserving foreground pass -> readback.
	 * Repeated loads use the existing default depth, not texture-pass backing. */
	/* targets[0] was cleared to black by its first texture pass above. */
	for (j = 0; j < 8; j++) {
		assert(rend_renderer_frame_begin(renderer));
		rend_cmd_render_begin(renderer, 1, 0, 0, 1);
		rend_cmd_render_end(renderer);
		rend_cmd_blit(renderer, &targets[0], rend_renderer_color_target(renderer),
			0, 0, 32, 32, 0, 0, 32, 32);
		rend_cmd_render_begin_preserve(renderer);
		rend_cmd_render_end(renderer);
		rend_cmd_render_begin_preserve(renderer); /* Also load after color writes. */
		rend_cmd_render_end(renderer);
		rend_renderer_frame_end(renderer, NULL);
		rend_renderer_read(renderer, pixels, sizeof(pixels));
		for (i = 0; i < 64 * 64; i++) {
			int black = i / 64 < 32 && i % 64 < 32;
			assert(pixels[i * 4] == (black ? 0 : 255));
			assert(pixels[i * 4 + 1] == 0 && pixels[i * 4 + 2] == 0);
			assert(pixels[i * 4 + 3] == (black ? 0 : 255));
		}
	}
	rend_texture_destroy(renderer, &targets[0]);
	rend_texture_destroy(renderer, &targets[1]);
	rend_renderer_destroy(renderer);
	assert(test_memory_count == 0 && test_memory_bytes == 0);
	assert(test_validation_errors == 0);
	destroy_messenger(vk_instance, messenger, NULL);
	rend_quit();
	peak_quit(peak_demo_ctx);
	puts("rend_vk: passed");
	return 0;
}
