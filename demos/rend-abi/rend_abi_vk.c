/* Rend 2.0.0 native Vulkan shader/ABI feasibility; NOT the replacement renderer.
 * Explicit graphics-compute profile, selected device, fixed demo-owned storage.
 * Run ./build rend abi run from godstack. Vulkan 1.4.357+ headers are required.
 * Native/driver/loader/libc allocations are excluded from host-backing counts.
 * Copyright (c) 2026 Vasco Alves. MIT license, as in Rend/rend.h.
 */
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif

#include <vulkan/vulkan.h>
#include "rend_abi_shared.h"

#define ABI_DEVICE_MAX 16
#define ABI_EXTENSION_MAX 512
#define ABI_QUEUE_MAX 32
#define ABI_IMAGE_COUNT (ABI_FRAMES * 5 + 2)
#define ABI_BUFFER_COUNT 4
#define ABI_CODE_WORDS 16384
#define ABI_FRAME_BYTES 8192
#define ABI_DATA_BYTES (ABI_FRAME_BYTES * (ABI_FRAMES + 1))
#define ABI_ROUNDS 4
#define ABI_TIMEOUT_NS UINT64_C(10000000000)
#define ABI_ADDRESS_FLAGS (VK_ADDRESS_COMMAND_FULLY_BOUND_BIT_KHR | VK_ADDRESS_COMMAND_STORAGE_BUFFER_USAGE_BIT_KHR)
#define ABI_TRY(call) do { if (!abi_result((call), #call)) return 0; } while (0)
#define ABI_CREATE(call) do { if (!abi_created((call), #call)) return 0; } while (0)

typedef struct AbiBuffer {
	VkBuffer buffer;
	VkDeviceMemory memory;
	VkDeviceSize bytes, allocated;
	VkDeviceAddress gpu;
	unsigned char *cpu;
	VkMemoryPropertyFlags properties;
} AbiBuffer;

typedef struct AbiImage {
	VkImage image;
	VkImageView view;
	VkImageViewCreateInfo view_info;
	VkMemoryRequirements requirements;
	VkDeviceSize offset;
	VkExtent3D extent;
	VkImageAspectFlags aspect;
} AbiImage;

typedef struct AbiFrame {
	VkDeviceSize root[2], generated[2], nodes, values, vertices, outputs[2];
	VkDeviceSize upload[2], color_read, storage_read, generated_read;
	uint64_t completion;
	uint32_t tag, red[2], draw_red[2];
} AbiFrame;

typedef struct AbiTest {
	VkInstance instance;
	VkDebugUtilsMessengerEXT messenger;
	VkPhysicalDevice physical;
	VkPhysicalDeviceProperties properties;
	VkPhysicalDeviceMemoryProperties memory;
	VkPhysicalDeviceDescriptorHeapPropertiesEXT heaps;
	VkDevice device;
	VkQueue queues[2];
	uint32_t families[2], queue_count;
	VkCommandPool pools[2];
	VkCommandBuffer commands[ABI_FRAMES + 1], cross_command;
	VkSemaphore timeline, gate, cross_timeline;
	uint64_t sequence;
	VkShaderModule shaders[4];
	VkPipeline pipelines[3];
	uint32_t code[ABI_CODE_WORDS]; /* Reused only during shader-module creation. */
	AbiBuffer buffers[ABI_BUFFER_COUNT];
	AbiImage images[ABI_IMAGE_COUNT];
	VkDeviceMemory texture_memory;
	VkDeviceSize texture_bytes, gpu_live, gpu_peak;
	VkDeviceSize root_alignment, atom;
	VkBindHeapInfoEXT resource_bind, sampler_bind;
	AbiFrame frames[ABI_FRAMES];
	uint32_t allocation_attempts, allocation_count;
	uint32_t validation_errors, validation_warnings;
	double submit_ms, wait_ms;
	PFN_vkDestroyDebugUtilsMessengerEXT destroy_messenger;
	PFN_vkWriteResourceDescriptorsEXT write_resources;
	PFN_vkWriteSamplerDescriptorsEXT write_samplers;
	PFN_vkCmdBindResourceHeapEXT bind_resources;
	PFN_vkCmdBindSamplerHeapEXT bind_samplers;
	PFN_vkCmdPushDataEXT push_data;
	PFN_vkCmdCopyMemoryKHR copy_memory;
	PFN_vkCmdCopyMemoryToImageKHR copy_to_image;
	PFN_vkCmdCopyImageToMemoryKHR copy_from_image;
	PFN_vkCmdDispatchIndirect2KHR dispatch_indirect;
	PFN_vkCmdDrawIndirect2KHR draw_indirect;
} AbiTest;

#ifdef __linux__
void *__wrap_malloc(size_t bytes);
void *__wrap_calloc(size_t count, size_t bytes);
void *__wrap_realloc(void *ptr, size_t bytes);
void *__wrap_aligned_alloc(size_t alignment, size_t bytes);
int __wrap_posix_memalign(void **ptr, size_t alignment, size_t bytes);
#endif

static double abi_now(void);
static int abi_result(VkResult result, const char *operation);
static int abi_created(VkResult result, const char *operation);
static int abi_align(uint64_t value, uint64_t alignment, uint64_t *out);
static int abi_address(const AbiBuffer *buffer, uint64_t offset, uint64_t bytes, uint64_t alignment, VkDeviceAddress *out);
static int abi_reserve(uint64_t *cursor, uint64_t limit, uint64_t bytes, uint64_t alignment, uint64_t *offset);
static void abi_layout(void);
static VKAPI_ATTR VkBool32 VKAPI_CALL abi_validation(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT types, const VkDebugUtilsMessengerCallbackDataEXT *data, void *user);
static int abi_instance(AbiTest *test);
static int abi_capabilities(AbiTest *test, uint32_t index, int list, int full);
static int abi_device(AbiTest *test);
static int abi_memory_type(AbiTest *test, uint32_t bits, VkMemoryPropertyFlags required, uint32_t *index);
static int abi_allocate(AbiTest *test, VkDeviceSize bytes, uint32_t type, int address, VkDeviceMemory *memory);
static int abi_buffer_create(AbiTest *test, AbiBuffer *buffer, VkDeviceSize bytes, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, int mapped);
static int abi_images_create(AbiTest *test);
static int abi_pipeline_create(AbiTest *test);
static int abi_storage(AbiTest *test);
static int abi_cache(AbiTest *test, VkDeviceSize offset, VkDeviceSize bytes, int invalidate);
static void abi_barrier(VkCommandBuffer command, VkPipelineStageFlags2 src_stage, VkAccessFlags2 src_access, VkPipelineStageFlags2 dst_stage, VkAccessFlags2 dst_access);
static void abi_image_activate(VkCommandBuffer command, const AbiImage *image);
static void abi_copy(AbiTest *test, VkCommandBuffer command, VkDeviceAddress src, VkDeviceAddress dst, VkDeviceSize bytes);
static void abi_image_copy(AbiTest *test, VkCommandBuffer command, const AbiImage *image, VkDeviceAddress address, int upload);
static int abi_begin(VkCommandBuffer command);
static void abi_root(AbiTest *test, VkCommandBuffer command, VkDeviceAddress address);
static int abi_submit(AbiTest *test, uint32_t queue, VkCommandBuffer command, VkSemaphore wait, uint64_t wait_value, VkSemaphore signal, uint64_t signal_value);
static int abi_wait(AbiTest *test, uint64_t value);
static int abi_initialize_images(AbiTest *test);
static int abi_descriptors(AbiTest *test, uint32_t frame, uint32_t round);
static int abi_prepare(AbiTest *test, uint32_t frame, uint32_t round);
static int abi_record(AbiTest *test, uint32_t frame, int indirect);
static int abi_verify(AbiTest *test, uint32_t frame);
static int abi_aliases(AbiTest *test);
static int abi_cross_queue(AbiTest *test);
static int abi_work(AbiTest *test);
static int abi_cleanup(AbiTest *test);
static int abi_number(const char *text, uint32_t *number);

static AbiTest abi_test;
static uint32_t abi_create_step;
static uint32_t abi_backing_attempts;

/* Link-time wrappers count direct demo-owned backing calls, not allocations
 * inside dynamically linked native drivers, loaders or libc. Never grow storage.
 */
#ifdef __linux__
void *
__wrap_malloc(size_t bytes)
{
	(void)bytes;
	abi_backing_attempts++;
	return NULL;
}

void *
__wrap_calloc(size_t count, size_t bytes)
{
	(void)count;
	return __wrap_malloc(bytes);
}

void *
__wrap_realloc(void *ptr, size_t bytes)
{
	(void)ptr;
	return __wrap_malloc(bytes);
}

void *
__wrap_aligned_alloc(size_t alignment, size_t bytes)
{
	(void)alignment;
	return __wrap_malloc(bytes);
}

int
__wrap_posix_memalign(void **ptr, size_t alignment, size_t bytes)
{
	(void)ptr;
	(void)alignment;
	(void)bytes;
	abi_backing_attempts++;
	return ENOMEM;
}
#endif

double
abi_now(void)
{
#ifdef _WIN32
	LARGE_INTEGER ticks, frequency;
	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&ticks);
	return (double)ticks.QuadPart * 1000.0 / (double)frequency.QuadPart;
#else
	struct timespec time;
	clock_gettime(CLOCK_MONOTONIC, &time);
	return time.tv_sec * 1000.0 + time.tv_nsec / 1000000.0;
#endif
}

int
abi_result(VkResult result, const char *operation)
{
	if (result == VK_SUCCESS)
		return 1;
	fprintf(stderr, "rend_abi: %s failed: VkResult %d\n", operation, result);
	return 0;
}

int
abi_created(VkResult result, const char *operation)
{
	if (!abi_result(result, operation))
		return 0;
	abi_create_step++;
	(void)operation;
	return 1;
}

int
abi_align(uint64_t value, uint64_t alignment, uint64_t *out)
{
	if (!alignment || (alignment & (alignment - 1)) || value > UINT64_MAX - (alignment - 1))
		return 0;
	*out = (value + alignment - 1) & ~(alignment - 1);
	return 1;
}

int
abi_address(const AbiBuffer *buffer, uint64_t offset, uint64_t bytes, uint64_t alignment, VkDeviceAddress *out)
{
	*out = 0;
	if (!alignment || (alignment & (alignment - 1)) || offset > buffer->bytes || bytes > buffer->bytes - offset || offset > UINT64_MAX - buffer->gpu)
		return 0;
	if ((buffer->gpu + offset) & (alignment - 1))
		return 0;
	*out = buffer->gpu + offset;
	return *out != 0;
}

int
abi_reserve(uint64_t *cursor, uint64_t limit, uint64_t bytes, uint64_t alignment, uint64_t *offset)
{
	uint64_t start;

	if (!abi_align(*cursor, alignment, &start) || start > limit || bytes > limit - start)
		return 0;
	*offset = start;
	*cursor = start + bytes;
	return 1;
}

void
abi_layout(void)
{
#define ABI_FIELD(name) printf("\"%s\":{\"offset\":%zu,\"size\":%zu}", #name, offsetof(AbiRoot, name), sizeof(((AbiRoot *)0)->name))
	printf("{\"size\":%zu,\"alignment\":%zu,\"pointer_size\":%zu,\"fields\":{", sizeof(AbiRoot), offsetof(struct { char c; AbiRoot root; }, root), sizeof(uint64_t));
	ABI_FIELD(tag); printf(","); ABI_FIELD(vector); printf(","); ABI_FIELD(matrix); printf(",");
	ABI_FIELD(node); printf(","); ABI_FIELD(vertices); printf(","); ABI_FIELD(output); printf(",");
	ABI_FIELD(generated); printf(","); ABI_FIELD(probe); printf(","); ABI_FIELD(texture_first); printf(",");
	ABI_FIELD(sampler_first); printf(","); ABI_FIELD(storage_slot); printf(","); ABI_FIELD(count); printf(",");
	ABI_FIELD(draw_slot); printf(","); ABI_FIELD(reserved); puts("}}");
#undef ABI_FIELD
}

VKAPI_ATTR VkBool32 VKAPI_CALL
abi_validation(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT types, const VkDebugUtilsMessengerCallbackDataEXT *data, void *user)
{
	AbiTest *test = user;

	(void)types;
	if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		test->validation_errors++;
	if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		test->validation_warnings++;
	fprintf(stderr, "rend_abi validation: %s\n", data->pMessage);
	return VK_FALSE;
}

int
abi_instance(AbiTest *test)
{
	VkLayerProperties layers[64];
	VkExtensionProperties extensions[ABI_EXTENSION_MAX];
	uint32_t count = 0, i, version = 0;
	int layer = 0, debug = 0;
	const char *layer_name = "VK_LAYER_KHRONOS_validation";
	const char *extension_name = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
	VkValidationFeatureEnableEXT validation = VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT;
	VkValidationFeaturesEXT validations = { .sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT, .enabledValidationFeatureCount = 1, .pEnabledValidationFeatures = &validation };
	VkDebugUtilsMessengerCreateInfoEXT messenger = {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		.pNext = &validations,
		.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
		.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
		.pfnUserCallback = abi_validation, .pUserData = test,
	};
	VkApplicationInfo app = { .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .pApplicationName = "Rend 2.0.0 ABI feasibility", .apiVersion = VK_API_VERSION_1_4 };
	VkInstanceCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pNext = &messenger, .pApplicationInfo = &app,
		.enabledLayerCount = 1, .ppEnabledLayerNames = &layer_name,
		.enabledExtensionCount = 1, .ppEnabledExtensionNames = &extension_name,
	};
	PFN_vkCreateDebugUtilsMessengerEXT create;

	ABI_TRY(vkEnumerateInstanceVersion(&version));
	if (version < VK_API_VERSION_1_4) {
		fprintf(stderr, "rend_abi: Vulkan 1.4 loader required\n");
		return 0;
	}
	ABI_TRY(vkEnumerateInstanceLayerProperties(&count, NULL));
	if (count > 64)
		return 0;
	ABI_TRY(vkEnumerateInstanceLayerProperties(&count, layers));
	for (i = 0; i < count; i++)
		layer |= !strcmp(layers[i].layerName, layer_name);
	ABI_TRY(vkEnumerateInstanceExtensionProperties(NULL, &count, NULL));
	if (count > ABI_EXTENSION_MAX)
		return 0;
	ABI_TRY(vkEnumerateInstanceExtensionProperties(NULL, &count, extensions));
	for (i = 0; i < count; i++)
		debug |= !strcmp(extensions[i].extensionName, extension_name);
	if (!layer || !debug) {
		fprintf(stderr, "rend_abi: validation layer and VK_EXT_debug_utils required\n");
		return 0;
	}
	ABI_CREATE(vkCreateInstance(&info, NULL, &test->instance));
	create = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(test->instance, "vkCreateDebugUtilsMessengerEXT");
	test->destroy_messenger = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(test->instance, "vkDestroyDebugUtilsMessengerEXT");
	if (!create || !test->destroy_messenger)
		return 0;
	messenger.pNext = NULL;
	ABI_CREATE(create(test->instance, &messenger, NULL, &test->messenger));
	return 1;
}

int
abi_capabilities(AbiTest *test, uint32_t index, int list, int full)
{
	VkPhysicalDevice devices[ABI_DEVICE_MAX];
	VkExtensionProperties extensions[ABI_EXTENSION_MAX];
	VkQueueFamilyProperties queues[ABI_QUEUE_MAX];
	VkPhysicalDeviceDescriptorHeapFeaturesEXT heaps = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_FEATURES_EXT };
	VkPhysicalDeviceDeviceAddressCommandsFeaturesKHR addresses = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_ADDRESS_COMMANDS_FEATURES_KHR, .pNext = &heaps };
	VkPhysicalDeviceShaderUntypedPointersFeaturesKHR pointers = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_UNTYPED_POINTERS_FEATURES_KHR, .pNext = &addresses };
	VkPhysicalDeviceVulkan11Features f11 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES, .pNext = &pointers };
	VkPhysicalDeviceVulkan12Features f12 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, .pNext = &f11 };
	VkPhysicalDeviceVulkan13Features f13 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES, .pNext = &f12 };
	VkPhysicalDeviceFeatures2 features = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &f13 };
	VkPhysicalDeviceDriverProperties driver = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES };
	VkPhysicalDeviceProperties2 props = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &driver };
	const char *required[] = { VK_EXT_DESCRIPTOR_HEAP_EXTENSION_NAME, VK_KHR_DEVICE_ADDRESS_COMMANDS_EXTENSION_NAME, VK_KHR_SHADER_UNTYPED_POINTERS_EXTENSION_NAME };
	uint32_t count = 0, i, j, present[3] = {0}, mesh = 0;

	ABI_TRY(vkEnumeratePhysicalDevices(test->instance, &count, NULL));
	if (!count || count > ABI_DEVICE_MAX || (!list && index >= count)) {
		fprintf(stderr, "rend_abi: selected device %u unavailable (count %u, bound %u)\n", index, count, ABI_DEVICE_MAX);
		return 0;
	}
	ABI_TRY(vkEnumeratePhysicalDevices(test->instance, &count, devices));
	if (list) {
		for (i = 0; i < count; i++) {
			vkGetPhysicalDeviceProperties(devices[i], &test->properties);
			printf("device %u: %s (Vulkan %u.%u.%u)\n", i, test->properties.deviceName, VK_API_VERSION_MAJOR(test->properties.apiVersion), VK_API_VERSION_MINOR(test->properties.apiVersion), VK_API_VERSION_PATCH(test->properties.apiVersion));
		}
		return 1;
	}
	test->physical = devices[index];
	test->heaps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_PROPERTIES_EXT;
	driver.pNext = &test->heaps;
	vkGetPhysicalDeviceProperties2(test->physical, &props);
	test->properties = props.properties;
	vkGetPhysicalDeviceMemoryProperties(test->physical, &test->memory);
	printf("rend_abi: device %u: %s; Vulkan %u.%u.%u; %s %s\n", index, test->properties.deviceName, VK_API_VERSION_MAJOR(test->properties.apiVersion), VK_API_VERSION_MINOR(test->properties.apiVersion), VK_API_VERSION_PATCH(test->properties.apiVersion), driver.driverName, driver.driverInfo);
	ABI_TRY(vkEnumerateDeviceExtensionProperties(test->physical, NULL, &count, NULL));
	if (count > ABI_EXTENSION_MAX)
		return 0;
	ABI_TRY(vkEnumerateDeviceExtensionProperties(test->physical, NULL, &count, extensions));
	for (i = 0; i < count; i++) {
		for (j = 0; j < 3; j++)
			present[j] |= !strcmp(extensions[i].extensionName, required[j]);
		mesh |= !strcmp(extensions[i].extensionName, VK_EXT_MESH_SHADER_EXTENSION_NAME);
	}
	printf("rend_abi: explicit profile %s; mesh extension %s (task/mesh execution excluded)\n", full ? "full preflight" : "graphics-compute", mesh ? "advertised" : "absent");
	if (full) {
		fprintf(stderr, "rend_abi: full profile rejected: %s\n", mesh ? "full task/mesh workload not implemented by this feasibility client" : "missing VK_EXT_mesh_shader");
		return 0;
	}
	for (i = 0; i < 3; i++) {
		if (!present[i]) {
			fprintf(stderr, "rend_abi: selected device missing %s; no fallback\n", required[i]);
			return 0;
		}
	}
	vkGetPhysicalDeviceFeatures2(test->physical, &features);
#define ABI_REQUIRE(condition) do { if (!(condition)) { fprintf(stderr, "rend_abi: missing feature/limit: %s\n", #condition); return 0; } } while (0)
	ABI_REQUIRE(test->properties.apiVersion >= VK_API_VERSION_1_4);
	ABI_REQUIRE(heaps.descriptorHeap);
	ABI_REQUIRE(addresses.deviceAddressCommands);
	ABI_REQUIRE(pointers.shaderUntypedPointers);
	ABI_REQUIRE(f11.shaderDrawParameters);
	ABI_REQUIRE(f12.bufferDeviceAddress);
	ABI_REQUIRE(f12.scalarBlockLayout);
	ABI_REQUIRE(f12.timelineSemaphore);
	ABI_REQUIRE(f13.synchronization2);
	ABI_REQUIRE(f13.dynamicRendering);
	printf("rend_abi: shaderInt64 %u; formatless storage read/write %u/%u\n", features.features.shaderInt64, features.features.shaderStorageImageReadWithoutFormat, features.features.shaderStorageImageWriteWithoutFormat);
	ABI_REQUIRE(features.features.shaderInt64);
	/* WTexture2D emits only StorageImageWriteWithoutFormat. The workload
	 * never reads a storage image; it samples/copies those pixels instead.
	 */
	ABI_REQUIRE(features.features.shaderStorageImageWriteWithoutFormat);
	ABI_REQUIRE(test->properties.limits.maxComputeWorkGroupInvocations >= ABI_ELEMENTS);
	ABI_REQUIRE(test->properties.limits.maxComputeWorkGroupSize[0] >= ABI_ELEMENTS);
	ABI_REQUIRE(test->heaps.maxPushDataSize >= sizeof(uint64_t));
	ABI_REQUIRE(test->heaps.imageDescriptorSize && test->heaps.samplerDescriptorSize);
#undef ABI_REQUIRE
	/* Depth/stencil dynamic commands used here are mandatory in Vulkan 1.3+.
	 * There is no EXT feature bit to enable for this promoted core subset.
	 */
	vkGetPhysicalDeviceQueueFamilyProperties(test->physical, &count, NULL);
	if (count > ABI_QUEUE_MAX)
		return 0;
	vkGetPhysicalDeviceQueueFamilyProperties(test->physical, &count, queues);
	test->families[0] = UINT32_MAX;
	for (i = 0; i < count; i++) {
		if (queues[i].queueCount && (queues[i].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) == (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) {
			test->families[0] = i;
			break;
		}
	}
	if (test->families[0] == UINT32_MAX) {
		fprintf(stderr, "rend_abi: graphics+compute queue required\n");
		return 0;
	}
	test->queue_count = 1;
	for (i = 0; i < count; i++) {
		if (i != test->families[0] && queues[i].queueCount && (queues[i].queueFlags & (VK_QUEUE_TRANSFER_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT))) {
			test->families[1] = i;
			test->queue_count = 2;
			break;
		}
	}
	test->atom = test->properties.limits.nonCoherentAtomSize;
	test->root_alignment = test->properties.limits.minUniformBufferOffsetAlignment;
	if (test->root_alignment < 16)
		test->root_alignment = 16;
	if (test->atom > ABI_FRAME_BYTES || test->root_alignment > ABI_FRAME_BYTES) {
		fprintf(stderr, "rend_abi: native cache/root alignment exceeds fixed frame range\n");
		return 0;
	}
	printf("rend_abi: root alignment %" PRIu64 "; cache atom %" PRIu64 "; queues %u\n", test->root_alignment, test->atom, test->queue_count);
	return 1;
}

int
abi_device(AbiTest *test)
{
	const char *extensions[] = { VK_EXT_DESCRIPTOR_HEAP_EXTENSION_NAME, VK_KHR_DEVICE_ADDRESS_COMMANDS_EXTENSION_NAME, VK_KHR_SHADER_UNTYPED_POINTERS_EXTENSION_NAME };
	float priority = 1;
	VkDeviceQueueCreateInfo queues[2];
	VkPhysicalDeviceDescriptorHeapFeaturesEXT heaps = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_FEATURES_EXT, .descriptorHeap = VK_TRUE };
	VkPhysicalDeviceDeviceAddressCommandsFeaturesKHR addresses = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_ADDRESS_COMMANDS_FEATURES_KHR, .pNext = &heaps, .deviceAddressCommands = VK_TRUE };
	VkPhysicalDeviceShaderUntypedPointersFeaturesKHR pointers = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_UNTYPED_POINTERS_FEATURES_KHR, .pNext = &addresses, .shaderUntypedPointers = VK_TRUE };
	VkPhysicalDeviceVulkan11Features f11 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES, .pNext = &pointers, .shaderDrawParameters = VK_TRUE };
	VkPhysicalDeviceVulkan12Features f12 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, .pNext = &f11, .bufferDeviceAddress = VK_TRUE, .scalarBlockLayout = VK_TRUE, .timelineSemaphore = VK_TRUE };
	VkPhysicalDeviceVulkan13Features f13 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES, .pNext = &f12, .synchronization2 = VK_TRUE, .dynamicRendering = VK_TRUE };
	VkPhysicalDeviceFeatures2 features = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &f13, .features = { .shaderInt64 = VK_TRUE, .shaderStorageImageWriteWithoutFormat = VK_TRUE } };
	VkDeviceCreateInfo info = { .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .pNext = &features, .queueCreateInfoCount = test->queue_count, .pQueueCreateInfos = queues, .enabledExtensionCount = 3, .ppEnabledExtensionNames = extensions };
	VkSemaphoreTypeCreateInfo type = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO, .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE };
	VkSemaphoreCreateInfo semaphore = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = &type };
	uint32_t i;

	for (i = 0; i < test->queue_count; i++)
		queues[i] = (VkDeviceQueueCreateInfo){ .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueFamilyIndex = test->families[i], .queueCount = 1, .pQueuePriorities = &priority };
	ABI_CREATE(vkCreateDevice(test->physical, &info, NULL, &test->device));
#define ABI_PROC(field, name) do { test->field = (PFN_##name)vkGetDeviceProcAddr(test->device, #name); if (!test->field) { fprintf(stderr, "rend_abi: missing native entry point %s\n", #name); return 0; } } while (0)
	ABI_PROC(write_resources, vkWriteResourceDescriptorsEXT);
	ABI_PROC(write_samplers, vkWriteSamplerDescriptorsEXT);
	ABI_PROC(bind_resources, vkCmdBindResourceHeapEXT);
	ABI_PROC(bind_samplers, vkCmdBindSamplerHeapEXT);
	ABI_PROC(push_data, vkCmdPushDataEXT);
	ABI_PROC(copy_memory, vkCmdCopyMemoryKHR);
	ABI_PROC(copy_to_image, vkCmdCopyMemoryToImageKHR);
	ABI_PROC(copy_from_image, vkCmdCopyImageToMemoryKHR);
	ABI_PROC(dispatch_indirect, vkCmdDispatchIndirect2KHR);
	ABI_PROC(draw_indirect, vkCmdDrawIndirect2KHR);
	if (!vkGetDeviceProcAddr(test->device, "vkCmdSetDepthCompareOp") || !vkGetDeviceProcAddr(test->device, "vkCmdSetStencilOp"))
		return 0;
#undef ABI_PROC
	for (i = 0; i < test->queue_count; i++) {
		VkCommandPoolCreateInfo pool = { .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, .queueFamilyIndex = test->families[i] };
		VkCommandBufferAllocateInfo commands = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = i ? 1 : ABI_FRAMES + 1 };
		vkGetDeviceQueue(test->device, test->families[i], 0, &test->queues[i]);
		ABI_CREATE(vkCreateCommandPool(test->device, &pool, NULL, &test->pools[i]));
		commands.commandPool = test->pools[i];
		ABI_TRY(vkAllocateCommandBuffers(test->device, &commands, i ? &test->cross_command : test->commands));
	}
	ABI_CREATE(vkCreateSemaphore(test->device, &semaphore, NULL, &test->timeline));
	ABI_CREATE(vkCreateSemaphore(test->device, &semaphore, NULL, &test->gate));
	ABI_CREATE(vkCreateSemaphore(test->device, &semaphore, NULL, &test->cross_timeline));
	return 1;
}

int
abi_memory_type(AbiTest *test, uint32_t bits, VkMemoryPropertyFlags required, uint32_t *index)
{
	uint32_t i;

	for (i = 0; i < test->memory.memoryTypeCount; i++) {
		if ((bits & (1u << i)) && (test->memory.memoryTypes[i].propertyFlags & required) == required) {
			*index = i;
			return 1;
		}
	}
	fprintf(stderr, "rend_abi: unsupported memory properties %#x, compatibility bits %#x\n", required, bits);
	return 0;
}

int
abi_allocate(AbiTest *test, VkDeviceSize bytes, uint32_t type, int address, VkDeviceMemory *memory)
{
	VkMemoryAllocateFlagsInfo flags = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO, .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT };
	VkMemoryAllocateInfo info = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .pNext = address ? &flags : NULL, .allocationSize = bytes, .memoryTypeIndex = type };
	VkResult result;

	test->allocation_attempts++;
	result = vkAllocateMemory(test->device, &info, NULL, memory);
	if (result == VK_SUCCESS) {
		test->allocation_count++;
		test->gpu_live += bytes;
		if (test->gpu_live > test->gpu_peak)
			test->gpu_peak = test->gpu_live;
	}
	return abi_created(result, "vkAllocateMemory");
}

int
abi_buffer_create(AbiTest *test, AbiBuffer *buffer, VkDeviceSize bytes, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, int mapped)
{
	VkBufferCreateInfo info = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = bytes, .usage = usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, .sharingMode = test->queue_count > 1 ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE, .queueFamilyIndexCount = test->queue_count > 1 ? test->queue_count : 0, .pQueueFamilyIndices = test->families };
	VkMemoryRequirements requirements;
	VkBufferDeviceAddressInfo address = { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
	uint32_t type;

	buffer->bytes = bytes;
	ABI_CREATE(vkCreateBuffer(test->device, &info, NULL, &buffer->buffer));
	vkGetBufferMemoryRequirements(test->device, buffer->buffer, &requirements);
	if (!abi_memory_type(test, requirements.memoryTypeBits, properties, &type))
		return 0;
	buffer->allocated = requirements.size;
	buffer->properties = test->memory.memoryTypes[type].propertyFlags;
	if (!abi_allocate(test, requirements.size, type, 1, &buffer->memory))
		return 0;
	ABI_TRY(vkBindBufferMemory(test->device, buffer->buffer, buffer->memory, 0));
	address.buffer = buffer->buffer;
	buffer->gpu = vkGetBufferDeviceAddress(test->device, &address);
	if (!buffer->gpu)
		return 0;
	if (mapped)
		ABI_TRY(vkMapMemory(test->device, buffer->memory, 0, VK_WHOLE_SIZE, 0, (void **)&buffer->cpu));
	printf("rend_abi: data/descriptor backing: %" PRIu64 " bytes, native %" PRIu64 ", properties %#x, mapped %d\n", bytes, requirements.size, buffer->properties, mapped);
	return 1;
}

int
abi_images_create(AbiTest *test)
{
	uint32_t i, bits = UINT32_MAX, type;
	uint64_t cursor = 0;
	VkFormatProperties color, depth;
	VkFormatFeatureFlags color_required = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT | VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
	VkMemoryDedicatedRequirements dedicated;

	vkGetPhysicalDeviceFormatProperties(test->physical, VK_FORMAT_R8G8B8A8_UNORM, &color);
	vkGetPhysicalDeviceFormatProperties(test->physical, VK_FORMAT_D32_SFLOAT_S8_UINT, &depth);
	if ((color.optimalTilingFeatures & color_required) != color_required || !(depth.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
		fprintf(stderr, "rend_abi: RGBA8 sampled/linear-filter/storage/attachment/transfer and D32S8 attachment formats required\n");
		return 0;
	}
	for (i = 0; i < ABI_IMAGE_COUNT; i++) {
		AbiImage *image = &test->images[i];
		uint32_t kind = i < ABI_FRAMES * 5 ? i % 5 : 0;
		VkImageCreateInfo info = { .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .flags = i >= ABI_FRAMES * 5 ? VK_IMAGE_CREATE_ALIAS_BIT : 0, .imageType = VK_IMAGE_TYPE_2D, .format = kind == 4 ? VK_FORMAT_D32_SFLOAT_S8_UINT : VK_FORMAT_R8G8B8A8_UNORM, .mipLevels = 1, .arrayLayers = 1, .samples = VK_SAMPLE_COUNT_1_BIT, .tiling = VK_IMAGE_TILING_OPTIMAL, .usage = kind == 4 ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, .sharingMode = test->queue_count > 1 ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE, .queueFamilyIndexCount = test->queue_count > 1 ? test->queue_count : 0, .pQueueFamilyIndices = test->families };
		VkImageMemoryRequirementsInfo2 query = { .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2 };
		VkMemoryRequirements2 requirements = { .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2, .pNext = &dedicated };
		image->extent = (VkExtent3D){ kind >= 2 ? ABI_WIDTH : i < ABI_FRAMES * 5 ? 2 : 1, kind >= 3 ? ABI_HEIGHT : 1, 1 };
		image->aspect = kind == 4 ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
		info.extent = image->extent;
		ABI_CREATE(vkCreateImage(test->device, &info, NULL, &image->image));
		dedicated = (VkMemoryDedicatedRequirements){ .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS };
		query.image = image->image;
		vkGetImageMemoryRequirements2(test->device, &query, &requirements);
		image->requirements = requirements.memoryRequirements;
		if (dedicated.requiresDedicatedAllocation) {
			fprintf(stderr, "rend_abi: texture %u requires dedicated storage; placement workload unsupported\n", i);
			return 0;
		}
		bits &= image->requirements.memoryTypeBits;
		if (i == ABI_IMAGE_COUNT - 1) {
			image->offset = test->images[i - 1].offset;
			if (image->requirements.size != test->images[i - 1].requirements.size || image->requirements.alignment != test->images[i - 1].requirements.alignment)
				return 0;
		} else if (!abi_reserve(&cursor, UINT64_MAX, image->requirements.size, image->requirements.alignment, &image->offset)) {
			return 0;
		}
	}
	if (!abi_memory_type(test, bits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &type))
		return 0;
	test->texture_bytes = cursor;
	if (!abi_allocate(test, cursor, type, 0, &test->texture_memory))
		return 0;
	for (i = 0; i < ABI_IMAGE_COUNT; i++) {
		AbiImage *image = &test->images[i];
		ABI_TRY(vkBindImageMemory(test->device, image->image, test->texture_memory, image->offset));
		image->view_info = (VkImageViewCreateInfo){ .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = image->image, .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = image->aspect == VK_IMAGE_ASPECT_COLOR_BIT ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_D32_SFLOAT_S8_UINT, .subresourceRange = { image->aspect, 0, 1, 0, 1 } };
		if (i < ABI_FRAMES * 5 && i % 5 >= 3)
			ABI_CREATE(vkCreateImageView(test->device, &image->view_info, NULL, &image->view));
	}
	printf("rend_abi: %u images, %" PRIu64 " texture-heap bytes; alias pair shares offset %" PRIu64 "\n", ABI_IMAGE_COUNT, cursor, test->images[ABI_IMAGE_COUNT - 1].offset);
	return 1;
}

int
abi_pipeline_create(AbiTest *test)
{
	const char *paths[] = { "bin/rend_abi.compute.spv", "bin/rend_abi.consume.spv", "bin/rend_abi.vertex.spv", "bin/rend_abi.fragment.spv" };
	const char *entries[] = { "computeMain", "consumeMain", "vertexMain", "fragmentMain" };
	VkDescriptorSetAndBindingMappingEXT root = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT, .descriptorSet = 0, .firstBinding = 0, .bindingCount = 1, .resourceMask = VK_SPIRV_RESOURCE_TYPE_UNIFORM_BUFFER_BIT_EXT, .source = VK_DESCRIPTOR_MAPPING_SOURCE_PUSH_ADDRESS_EXT, .sourceData.pushAddressOffset = 0 };
	VkShaderDescriptorSetAndBindingMappingInfoEXT mapping = { .sType = VK_STRUCTURE_TYPE_SHADER_DESCRIPTOR_SET_AND_BINDING_MAPPING_INFO_EXT, .mappingCount = 1, .pMappings = &root };
	VkPipelineCreateFlags2CreateInfo flags = { .sType = VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO, .flags = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT };
	VkPipelineShaderStageCreateInfo stages[4];
	uint32_t i;

	for (i = 0; i < 4; i++) {
		FILE *file;
		long size;
		VkShaderModuleCreateInfo info = { .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
		if (!(file = fopen(paths[i], "rb"))) {
			fprintf(stderr, "rend_abi: cannot open %s\n", paths[i]);
			return 0;
		}
		if (fseek(file, 0, SEEK_END) || (size = ftell(file)) < 20 || size % 4 || (uint64_t)size > sizeof(test->code) || fseek(file, 0, SEEK_SET) || fread(test->code, 1, (size_t)size, file) != (size_t)size) {
			fclose(file);
			fprintf(stderr, "rend_abi: invalid/oversized shader %s (capacity %zu)\n", paths[i], sizeof(test->code));
			return 0;
		}
		fclose(file);
		if (test->code[0] != UINT32_C(0x07230203))
			return 0;
		info.codeSize = (size_t)size;
		info.pCode = test->code;
		ABI_CREATE(vkCreateShaderModule(test->device, &info, NULL, &test->shaders[i]));
		stages[i] = (VkPipelineShaderStageCreateInfo){ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .pNext = &mapping, .stage = i < 2 ? VK_SHADER_STAGE_COMPUTE_BIT : i == 2 ? VK_SHADER_STAGE_VERTEX_BIT : VK_SHADER_STAGE_FRAGMENT_BIT, .module = test->shaders[i], .pName = entries[i] };
		if (i < 2) {
			VkComputePipelineCreateInfo pipeline = { .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, .pNext = &flags, .stage = stages[i] };
			ABI_CREATE(vkCreateComputePipelines(test->device, VK_NULL_HANDLE, 1, &pipeline, NULL, &test->pipelines[i]));
		}
	}
	{
		VkFormat color = VK_FORMAT_R8G8B8A8_UNORM;
		VkPipelineRenderingCreateInfo rendering = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO, .colorAttachmentCount = 1, .pColorAttachmentFormats = &color, .depthAttachmentFormat = VK_FORMAT_D32_SFLOAT_S8_UINT, .stencilAttachmentFormat = VK_FORMAT_D32_SFLOAT_S8_UINT };
		VkPipelineVertexInputStateCreateInfo input = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
		VkPipelineInputAssemblyStateCreateInfo assembly = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST };
		VkPipelineViewportStateCreateInfo viewport = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .scissorCount = 1 };
		VkPipelineRasterizationStateCreateInfo raster = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, .polygonMode = VK_POLYGON_MODE_FILL, .cullMode = VK_CULL_MODE_NONE, .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE, .lineWidth = 1 };
		VkPipelineMultisampleStateCreateInfo samples = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT };
		VkPipelineDepthStencilStateCreateInfo depth = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
		VkPipelineColorBlendAttachmentState attachment = { .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT };
		VkPipelineColorBlendStateCreateInfo blend = { .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, .attachmentCount = 1, .pAttachments = &attachment };
		VkDynamicState states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE, VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE, VK_DYNAMIC_STATE_DEPTH_COMPARE_OP, VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE, VK_DYNAMIC_STATE_STENCIL_OP, VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK, VK_DYNAMIC_STATE_STENCIL_WRITE_MASK, VK_DYNAMIC_STATE_STENCIL_REFERENCE };
		VkPipelineDynamicStateCreateInfo dynamic = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, .dynamicStateCount = sizeof(states) / sizeof(*states), .pDynamicStates = states };
		VkGraphicsPipelineCreateInfo pipeline = { .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, .pNext = &flags, .stageCount = 2, .pStages = &stages[2], .pVertexInputState = &input, .pInputAssemblyState = &assembly, .pViewportState = &viewport, .pRasterizationState = &raster, .pMultisampleState = &samples, .pDepthStencilState = &depth, .pColorBlendState = &blend, .pDynamicState = &dynamic };
		flags.pNext = &rendering;
		ABI_CREATE(vkCreateGraphicsPipelines(test->device, VK_NULL_HANDLE, 1, &pipeline, NULL, &test->pipelines[2]));
	}
	return 1;
}

int
abi_storage(AbiTest *test)
{
	uint64_t resource_offset, sampler_offset, resource_bytes, sampler_bytes, cursor;
	uint32_t i, j;
	VkDeviceAddress address;
	VkBufferUsageFlags data = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

	if (test->heaps.imageDescriptorSize > UINT64_MAX / (ABI_FRAMES * 3) || test->heaps.samplerDescriptorSize > UINT64_MAX / (ABI_FRAMES * 2))
		return 0;
	if (!abi_align(ABI_FRAMES * 3 * test->heaps.imageDescriptorSize, test->heaps.resourceHeapAlignment, &resource_offset) || !abi_align(ABI_FRAMES * 2 * test->heaps.samplerDescriptorSize, test->heaps.samplerHeapAlignment, &sampler_offset) || resource_offset > UINT64_MAX - test->heaps.minResourceHeapReservedRange || sampler_offset > UINT64_MAX - test->heaps.minSamplerHeapReservedRange || !abi_align(resource_offset + test->heaps.minResourceHeapReservedRange, test->heaps.resourceHeapAlignment, &resource_bytes) || !abi_align(sampler_offset + test->heaps.minSamplerHeapReservedRange, test->heaps.samplerHeapAlignment, &sampler_bytes) || resource_bytes > test->heaps.maxResourceHeapSize || sampler_bytes > test->heaps.maxSamplerHeapSize)
		return 0;
	if (!abi_buffer_create(test, &test->buffers[0], ABI_DATA_BYTES, data, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 1) || !abi_buffer_create(test, &test->buffers[1], ABI_DATA_BYTES, data, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0) || !abi_buffer_create(test, &test->buffers[2], resource_bytes, VK_BUFFER_USAGE_DESCRIPTOR_HEAP_BIT_EXT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 1) || !abi_buffer_create(test, &test->buffers[3], sampler_bytes, VK_BUFFER_USAGE_DESCRIPTOR_HEAP_BIT_EXT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 1))
		return 0;
	if (!abi_address(&test->buffers[0], 0, ABI_DATA_BYTES, test->root_alignment, &address) || !abi_address(&test->buffers[1], 0, ABI_DATA_BYTES, test->root_alignment, &address))
		return 0;
	if (!abi_address(&test->buffers[2], 0, resource_bytes, test->heaps.resourceHeapAlignment, &address) || !abi_address(&test->buffers[3], 0, sampler_bytes, test->heaps.samplerHeapAlignment, &address))
		return 0;
	test->resource_bind = (VkBindHeapInfoEXT){ .sType = VK_STRUCTURE_TYPE_BIND_HEAP_INFO_EXT, .heapRange = { test->buffers[2].gpu, resource_bytes }, .reservedRangeOffset = resource_offset, .reservedRangeSize = test->heaps.minResourceHeapReservedRange };
	test->sampler_bind = (VkBindHeapInfoEXT){ .sType = VK_STRUCTURE_TYPE_BIND_HEAP_INFO_EXT, .heapRange = { test->buffers[3].gpu, sampler_bytes }, .reservedRangeOffset = sampler_offset, .reservedRangeSize = test->heaps.minSamplerHeapReservedRange };
	cursor = 0;
	for (i = 0; i < ABI_FRAMES; i++) {
		AbiFrame *frame = &test->frames[i];
		cursor = i * ABI_FRAME_BYTES;
#define ABI_RESERVE(field, bytes, alignment) do { if (!abi_reserve(&cursor, (i + 1) * ABI_FRAME_BYTES, (bytes), (alignment), &(field))) { fprintf(stderr, "rend_abi: fixed frame range exhausted\n"); return 0; } } while (0)
		for (j = 0; j < 2; j++) {
			ABI_RESERVE(frame->root[j], sizeof(AbiRoot), test->root_alignment);
			ABI_RESERVE(frame->generated[j], sizeof(AbiRoot), test->root_alignment);
			ABI_RESERVE(frame->outputs[j], (ABI_ELEMENTS + ABI_LAYOUT_WORDS) * sizeof(uint32_t), 16);
			ABI_RESERVE(frame->upload[j], 8, 4);
		}
		ABI_RESERVE(frame->nodes, sizeof(AbiNode) * 2, 16);
		ABI_RESERVE(frame->values, ABI_ELEMENTS * sizeof(uint32_t), 16);
		ABI_RESERVE(frame->vertices, sizeof(AbiVertex) * 3, 16);
		ABI_RESERVE(frame->color_read, ABI_WIDTH * ABI_HEIGHT * 4, 16);
		ABI_RESERVE(frame->storage_read, ABI_ELEMENTS * 4, 16);
		ABI_RESERVE(frame->generated_read, sizeof(AbiRoot) * 2, 16);
#undef ABI_RESERVE
		printf("rend_abi: frame %u uses %" PRIu64 "/%u bytes at offset %u\n", i, cursor - i * ABI_FRAME_BYTES, ABI_FRAME_BYTES, i * ABI_FRAME_BYTES);
	}
	memset(test->buffers[0].cpu, 0, ABI_DATA_BYTES);
	return abi_images_create(test) && abi_pipeline_create(test);
}

int
abi_cache(AbiTest *test, VkDeviceSize offset, VkDeviceSize bytes, int invalidate)
{
	AbiBuffer *buffer = &test->buffers[0];
	VkMappedMemoryRange range = { .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, .memory = buffer->memory };
	uint64_t end;

	if (offset > buffer->bytes || bytes > buffer->bytes - offset || !abi_align(offset + bytes, test->atom, &end) || end > buffer->allocated)
		return 0;
	if (buffer->properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
		return 1;
	range.offset = offset & ~(test->atom - 1);
	range.size = end - range.offset;
	ABI_TRY(invalidate ? vkInvalidateMappedMemoryRanges(test->device, 1, &range) : vkFlushMappedMemoryRanges(test->device, 1, &range));
	return 1;
}

void
abi_barrier(VkCommandBuffer command, VkPipelineStageFlags2 src_stage, VkAccessFlags2 src_access, VkPipelineStageFlags2 dst_stage, VkAccessFlags2 dst_access)
{
	VkMemoryBarrier2 barrier = { .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2, .srcStageMask = src_stage, .srcAccessMask = src_access, .dstStageMask = dst_stage, .dstAccessMask = dst_access };
	VkDependencyInfo dependency = { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .memoryBarrierCount = 1, .pMemoryBarriers = &barrier };
	vkCmdPipelineBarrier2(command, &dependency);
}

void
abi_image_activate(VkCommandBuffer command, const AbiImage *image)
{
	VkImageMemoryBarrier2 barrier = { .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2, .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT, .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED, .newLayout = VK_IMAGE_LAYOUT_GENERAL, .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .image = image->image, .subresourceRange = { image->aspect, 0, 1, 0, 1 } };
	VkDependencyInfo dependency = { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier };
	vkCmdPipelineBarrier2(command, &dependency);
}

void
abi_copy(AbiTest *test, VkCommandBuffer command, VkDeviceAddress src, VkDeviceAddress dst, VkDeviceSize bytes)
{
	VkDeviceMemoryCopyKHR region = { .sType = VK_STRUCTURE_TYPE_DEVICE_MEMORY_COPY_KHR, .srcRange = { src, bytes }, .srcFlags = ABI_ADDRESS_FLAGS, .dstRange = { dst, bytes }, .dstFlags = ABI_ADDRESS_FLAGS };
	VkCopyDeviceMemoryInfoKHR info = { .sType = VK_STRUCTURE_TYPE_COPY_DEVICE_MEMORY_INFO_KHR, .regionCount = 1, .pRegions = &region };
	test->copy_memory(command, &info);
}

void
abi_image_copy(AbiTest *test, VkCommandBuffer command, const AbiImage *image, VkDeviceAddress address, int upload)
{
	VkDeviceMemoryImageCopyKHR region = { .sType = VK_STRUCTURE_TYPE_DEVICE_MEMORY_IMAGE_COPY_KHR, .addressRange = { address, image->extent.width * image->extent.height * 4 }, .addressFlags = ABI_ADDRESS_FLAGS, .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 }, .imageLayout = VK_IMAGE_LAYOUT_GENERAL, .imageExtent = image->extent };
	VkCopyDeviceMemoryImageInfoKHR info = { .sType = VK_STRUCTURE_TYPE_COPY_DEVICE_MEMORY_IMAGE_INFO_KHR, .image = image->image, .regionCount = 1, .pRegions = &region };
	if (upload)
		test->copy_to_image(command, &info);
	else
		test->copy_from_image(command, &info);
}

int
abi_begin(VkCommandBuffer command)
{
	VkCommandBufferBeginInfo info = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
	ABI_TRY(vkResetCommandBuffer(command, 0));
	ABI_TRY(vkBeginCommandBuffer(command, &info));
	return 1;
}

void
abi_root(AbiTest *test, VkCommandBuffer command, VkDeviceAddress address)
{
	VkPushDataInfoEXT push = { .sType = VK_STRUCTURE_TYPE_PUSH_DATA_INFO_EXT, .data = { &address, sizeof(address) } };
	test->push_data(command, &push);
}

int
abi_submit(AbiTest *test, uint32_t queue, VkCommandBuffer command, VkSemaphore wait, uint64_t wait_value, VkSemaphore signal, uint64_t signal_value)
{
	VkCommandBufferSubmitInfo buffer = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO, .commandBuffer = command };
	VkSemaphoreSubmitInfo waiting = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO, .semaphore = wait, .value = wait_value, .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT };
	VkSemaphoreSubmitInfo signaling = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO, .semaphore = signal, .value = signal_value, .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT };
	VkSubmitInfo2 submit = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2, .waitSemaphoreInfoCount = wait ? 1 : 0, .pWaitSemaphoreInfos = &waiting, .commandBufferInfoCount = 1, .pCommandBufferInfos = &buffer, .signalSemaphoreInfoCount = 1, .pSignalSemaphoreInfos = &signaling };
	double start = abi_now();
	VkResult result = vkQueueSubmit2(test->queues[queue], 1, &submit, VK_NULL_HANDLE);
	test->submit_ms += abi_now() - start;
	return abi_result(result, "vkQueueSubmit2");
}

int
abi_wait(AbiTest *test, uint64_t value)
{
	VkSemaphoreWaitInfo info = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO, .semaphoreCount = 1, .pSemaphores = &test->timeline, .pValues = &value };
	double start = abi_now();
	VkResult result = vkWaitSemaphores(test->device, &info, ABI_TIMEOUT_NS);
	test->wait_ms += abi_now() - start;
	return abi_result(result, "vkWaitSemaphores (10-second timeout)");
}

int
abi_initialize_images(AbiTest *test)
{
	VkCommandBuffer command = test->commands[ABI_FRAMES];
	uint32_t i;

	if (!abi_begin(command))
		return 0;
	for (i = 0; i < ABI_FRAMES * 5; i++)
		abi_image_activate(command, &test->images[i]);
	ABI_TRY(vkEndCommandBuffer(command));
	return abi_submit(test, 0, command, VK_NULL_HANDLE, 0, test->timeline, ++test->sequence) && abi_wait(test, test->sequence);
}

int
abi_descriptors(AbiTest *test, uint32_t frame, uint32_t round)
{
	uint32_t i;

	for (i = 0; i < 3; i++) {
		VkImageDescriptorInfoEXT image = { .sType = VK_STRUCTURE_TYPE_IMAGE_DESCRIPTOR_INFO_EXT, .pView = &test->images[frame * 5 + (i < 2 ? i ^ (round & 1) : 2)].view_info, .layout = VK_IMAGE_LAYOUT_GENERAL };
		VkResourceDescriptorInfoEXT descriptor = { .sType = VK_STRUCTURE_TYPE_RESOURCE_DESCRIPTOR_INFO_EXT, .type = i < 2 ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .data.pImage = &image };
		VkHostAddressRangeEXT dst = { test->buffers[2].cpu + (frame * 3 + i) * test->heaps.imageDescriptorSize, test->heaps.imageDescriptorSize };
		ABI_TRY(test->write_resources(test->device, 1, &descriptor, &dst));
	}
	for (i = 0; i < 2; i++) {
		VkSamplerCreateInfo sampler = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .magFilter = (i ^ (round & 1)) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST, .minFilter = (i ^ (round & 1)) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST, .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST, .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, .maxLod = 0 };
		VkHostAddressRangeEXT dst = { test->buffers[3].cpu + (frame * 2 + i) * test->heaps.samplerDescriptorSize, test->heaps.samplerDescriptorSize };
		ABI_TRY(test->write_samplers(test->device, 1, &sampler, &dst));
	}
	return 1;
}

int
abi_prepare(AbiTest *test, uint32_t frame_index, uint32_t round)
{
	AbiFrame *frame = &test->frames[frame_index];
	AbiBuffer *host = &test->buffers[0], *gpu = &test->buffers[1];
	AbiRoot roots[2];
	AbiNode nodes[2];
	AbiVertex vertices[3] = { { {-1, -1}, 0 }, { {3, -1}, 0 }, { {-1, 3}, 0 } };
	uint32_t values[ABI_ELEMENTS], i;
	uint64_t completion = 0;

	ABI_TRY(vkGetSemaphoreCounterValue(test->device, test->timeline, &completion));
	if (completion < frame->completion) {
		fprintf(stderr, "rend_abi: attempted pending frame/descriptor reuse\n");
		return 0;
	}
	if (!abi_descriptors(test, frame_index, round))
		return 0;
	frame->tag = 10 + round * ABI_FRAMES + frame_index;
	for (i = 0; i < ABI_ELEMENTS; i++)
		values[i] = frame->tag * 2 + i;
	nodes[0] = (AbiNode){ gpu->gpu + frame->values, gpu->gpu + frame->nodes + sizeof(AbiNode), 5, 1 };
	nodes[1] = (AbiNode){ gpu->gpu + frame->values, 0, 9, 1 };
	memset(host->cpu + frame_index * ABI_FRAME_BYTES, 0, ABI_FRAME_BYTES);
	memcpy(host->cpu + frame->nodes, nodes, sizeof(nodes));
	memcpy(host->cpu + frame->values, values, sizeof(values));
	for (i = 0; i < 3; i++)
		vertices[i].depth = 0.125f * (frame_index + 1);
	memcpy(host->cpu + frame->vertices, vertices, sizeof(vertices));
	for (i = 0; i < 2; i++) {
		uint32_t red = 20 + round * 20 + frame_index * 2 + i * 40;
		unsigned char pixels[8] = { (unsigned char)(red - 10), 0, 0, 255, (unsigned char)(red + 10), 0, 0, 255 };
		/* At u=0.5, linear averages the pair; nearest selects the right
		 * texel. Different exact results prove sampler selection too.
		 */
		frame->red[i ^ (round & 1)] = red + (i ? 0 : 10);
		frame->draw_red[i ^ (round & 1)] = red + ((round & 1) ? 0 : 10);
		memcpy(host->cpu + frame->upload[i], pixels, sizeof(pixels));
		roots[i] = (AbiRoot){ .tag = frame->tag + i * 100, .vector = {1, 2, 3}, .matrix = { {{1, 2}, {3, 4}, {5, 6}} }, .node = gpu->gpu + frame->nodes, .vertices = host->gpu + frame->vertices, .output = host->gpu + frame->outputs[i], .generated = gpu->gpu + frame->generated[i], .probe = host->gpu + frame->root[i], .texture_first = frame_index * 3, .sampler_first = frame_index * 2, .storage_slot = frame_index * 3 + 2, .count = ABI_ELEMENTS, .draw_slot = frame_index * 3 + i };
		memcpy(host->cpu + frame->root[i], &roots[i], sizeof(AbiRoot));
	}
	return 1;
}

int
abi_record(AbiTest *test, uint32_t frame_index, int indirect)
{
	AbiFrame *frame = &test->frames[frame_index];
	AbiBuffer *host = &test->buffers[0], *gpu = &test->buffers[1];
	VkCommandBuffer command = test->commands[frame_index];
	uint32_t i;
	VkDeviceSize indirect_offset = (frame_index + 1) * ABI_FRAME_BYTES - 32;
	VkDispatchIndirectCommand dispatch = {1, 1, 1};
	VkDrawIndirectCommand draw = {3, 1, 0, 0};

	memcpy(host->cpu + indirect_offset, &dispatch, sizeof(dispatch));
	memcpy(host->cpu + indirect_offset + 16, &draw, sizeof(draw));
	if (!abi_begin(command))
		return 0;
	test->bind_resources(command, &test->resource_bind);
	test->bind_samplers(command, &test->sampler_bind);
	abi_barrier(command, VK_PIPELINE_STAGE_2_HOST_BIT | VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_HOST_WRITE_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT);
	abi_copy(test, command, host->gpu + frame->nodes, gpu->gpu + frame->nodes, sizeof(AbiNode) * 2);
	abi_copy(test, command, host->gpu + frame->values, gpu->gpu + frame->values, ABI_ELEMENTS * sizeof(uint32_t));
	for (i = 0; i < 2; i++)
		abi_image_copy(test, command, &test->images[frame_index * 5 + i], host->gpu + frame->upload[i], 1);
	abi_barrier(command, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_UNIFORM_READ_BIT);
	for (i = 0; i < 2; i++) {
		vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE, test->pipelines[0]);
		abi_root(test, command, host->gpu + frame->root[i]);
		if (indirect) {
			VkDispatchIndirect2InfoKHR info = { .sType = VK_STRUCTURE_TYPE_DISPATCH_INDIRECT_2_INFO_KHR, .addressRange = { host->gpu + indirect_offset, sizeof(dispatch) }, .addressFlags = ABI_ADDRESS_FLAGS };
			test->dispatch_indirect(command, &info);
		} else {
			vkCmdDispatch(command, 1, 1, 1);
		}
		abi_barrier(command, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_UNIFORM_READ_BIT | VK_ACCESS_2_TRANSFER_READ_BIT);
		vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE, test->pipelines[1]);
		abi_root(test, command, gpu->gpu + frame->generated[i]);
		vkCmdDispatch(command, 1, 1, 1);
		abi_barrier(command, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT);
		abi_copy(test, command, gpu->gpu + frame->generated[i], host->gpu + frame->generated_read + i * sizeof(AbiRoot), sizeof(AbiRoot));
	}
	{
		VkRenderingAttachmentInfo color = { .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, .imageView = test->images[frame_index * 5 + 3].view, .imageLayout = VK_IMAGE_LAYOUT_GENERAL, .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, .storeOp = VK_ATTACHMENT_STORE_OP_STORE, .clearValue.color = {{0, 0, 0, 1}} };
		VkRenderingAttachmentInfo depth = { .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, .imageView = test->images[frame_index * 5 + 4].view, .imageLayout = VK_IMAGE_LAYOUT_GENERAL, .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, .storeOp = VK_ATTACHMENT_STORE_OP_STORE, .clearValue.depthStencil = {1, 0} };
		VkRenderingInfo rendering = { .sType = VK_STRUCTURE_TYPE_RENDERING_INFO, .renderArea = {{0, 0}, {ABI_WIDTH, ABI_HEIGHT}}, .layerCount = 1, .colorAttachmentCount = 1, .pColorAttachments = &color, .pDepthAttachment = &depth, .pStencilAttachment = &depth };
		VkViewport viewport = {0, 0, ABI_WIDTH, ABI_HEIGHT, 0, 1};
		VkRect2D scissor = {{0, 0}, {ABI_WIDTH / 2, ABI_HEIGHT}};
		vkCmdBeginRendering(command, &rendering);
		vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, test->pipelines[2]);
		vkCmdSetViewport(command, 0, 1, &viewport);
		vkCmdSetScissor(command, 0, 1, &scissor);
		vkCmdSetDepthTestEnable(command, VK_TRUE);
		vkCmdSetDepthWriteEnable(command, VK_TRUE);
		vkCmdSetDepthCompareOp(command, VK_COMPARE_OP_LESS);
		vkCmdSetStencilTestEnable(command, VK_TRUE);
		vkCmdSetStencilCompareMask(command, VK_STENCIL_FACE_FRONT_AND_BACK, 255);
		vkCmdSetStencilWriteMask(command, VK_STENCIL_FACE_FRONT_AND_BACK, 255);
		vkCmdSetStencilReference(command, VK_STENCIL_FACE_FRONT_AND_BACK, 1);
		vkCmdSetStencilOp(command, VK_STENCIL_FACE_FRONT_AND_BACK, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS);
		abi_root(test, command, gpu->gpu + frame->generated[0]);
		vkCmdDraw(command, 3, 1, 0, 0);
		/* Same PSO: equality fails GREATER, and stencil ref 2 fails EQUAL.
		 * Both must retain the left-hand pixels written with root A.
		 */
		abi_root(test, command, host->gpu + frame->root[1]);
		vkCmdSetDepthCompareOp(command, VK_COMPARE_OP_GREATER);
		vkCmdDraw(command, 3, 1, 0, 0);
		vkCmdSetDepthTestEnable(command, VK_FALSE);
		vkCmdSetStencilReference(command, VK_STENCIL_FACE_FRONT_AND_BACK, 2);
		vkCmdSetStencilOp(command, VK_STENCIL_FACE_FRONT_AND_BACK, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_EQUAL);
		vkCmdDraw(command, 3, 1, 0, 0);
		viewport.x = ABI_WIDTH / 2;
		viewport.width = ABI_WIDTH / 2;
		scissor.extent.width = ABI_WIDTH;
		vkCmdSetViewport(command, 0, 1, &viewport);
		vkCmdSetScissor(command, 0, 1, &scissor);
		vkCmdSetDepthTestEnable(command, VK_TRUE);
		vkCmdSetDepthCompareOp(command, VK_COMPARE_OP_LESS);
		vkCmdSetStencilOp(command, VK_STENCIL_FACE_FRONT_AND_BACK, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS);
		if (indirect) {
			VkDrawIndirect2InfoKHR info = { .sType = VK_STRUCTURE_TYPE_DRAW_INDIRECT_2_INFO_KHR, .addressRange = { host->gpu + indirect_offset + 16, sizeof(draw), sizeof(draw) }, .addressFlags = ABI_ADDRESS_FLAGS, .drawCount = 1 };
			test->draw_indirect(command, &info);
		} else {
			vkCmdDraw(command, 3, 1, 0, 0);
		}
		vkCmdEndRendering(command);
	}
	abi_barrier(command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
	abi_image_copy(test, command, &test->images[frame_index * 5 + 3], host->gpu + frame->color_read, 0);
	abi_image_copy(test, command, &test->images[frame_index * 5 + 2], host->gpu + frame->storage_read, 0);
	abi_barrier(command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT);
	ABI_TRY(vkEndCommandBuffer(command));
	return 1;
}

int
abi_verify(AbiTest *test, uint32_t frame_index)
{
	AbiFrame *frame = &test->frames[frame_index];
	unsigned char *cpu = test->buffers[0].cpu;
	uint32_t expected_layout[ABI_LAYOUT_WORDS] = { sizeof(AbiRoot), sizeof(AbiNode), sizeof(AbiVertex), sizeof(uint64_t), offsetof(AbiRoot, tag), offsetof(AbiRoot, vector), offsetof(AbiRoot, matrix), offsetof(AbiRoot, node), offsetof(AbiRoot, vertices), offsetof(AbiRoot, output), offsetof(AbiRoot, generated), offsetof(AbiRoot, probe), offsetof(AbiRoot, texture_first), offsetof(AbiRoot, count), 321, 631, 1, 1 };
	uint32_t i, j;

	if (!abi_cache(test, frame_index * ABI_FRAME_BYTES, ABI_FRAME_BYTES, 1))
		return 0;
	for (j = 0; j < 2; j++) {
		uint32_t *output = (uint32_t *)(cpu + frame->outputs[j]);
		AbiRoot expected = *(AbiRoot *)(cpu + frame->root[j]);
		expected.tag += 7;
		if (memcmp(&expected, cpu + frame->generated_read + j * sizeof(AbiRoot), sizeof(expected))) {
			fprintf(stderr, "rend_abi: GPU-produced root mismatch frame %u root %u\n", frame_index, j);
			return 0;
		}
		for (i = 0; i < ABI_ELEMENTS; i++) {
			uint32_t value = 2 * (frame->tag + j * 100) + 12 + (frame->tag - 2) * 2 + i + frame->red[i & 1];
			if (output[i] != value) {
				fprintf(stderr, "rend_abi: pointer/root/nonuniform mismatch frame %u root %u element %u: %u != %u\n", frame_index, j, i, output[i], value);
				return 0;
			}
		}
		if (memcmp(output + ABI_ELEMENTS, expected_layout, sizeof(expected_layout))) {
			for (i = 0; i < ABI_LAYOUT_WORDS; i++)
				if (output[ABI_ELEMENTS + i] != expected_layout[i])
					fprintf(stderr, "rend_abi: GPU layout word %u: %u != %u\n", i, output[ABI_ELEMENTS + i], expected_layout[i]);
			return 0;
		}
	}
	for (i = 0; i < ABI_WIDTH * ABI_HEIGHT; i++) {
		unsigned char expected[4] = { (unsigned char)frame->draw_red[(i % ABI_WIDTH) >= ABI_WIDTH / 2], 0, 0, 255 };
		if (memcmp(cpu + frame->color_read + i * 4, expected, 4)) {
			fprintf(stderr, "rend_abi: vertex/root/dynamic-state pixel mismatch frame %u pixel %u (red %u expected %u)\n", frame_index, i, cpu[frame->color_read + i * 4], expected[0]);
			return 0;
		}
	}
	for (i = 0; i < ABI_ELEMENTS; i++) {
		unsigned char expected[4] = { (unsigned char)frame->red[i & 1], 0, 0, 255 };
		if (memcmp(cpu + frame->storage_read + i * 4, expected, 4)) {
			fprintf(stderr, "rend_abi: storage-texture pixel mismatch frame %u element %u\n", frame_index, i);
			return 0;
		}
	}
	return 1;
}

int
abi_aliases(AbiTest *test)
{
	VkCommandBuffer command = test->commands[ABI_FRAMES];
	AbiBuffer *host = &test->buffers[0];
	VkDeviceSize offset = ABI_FRAMES * ABI_FRAME_BYTES;
	VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	uint32_t i;

	if (!abi_begin(command))
		return 0;
	for (i = 0; i < 2; i++) {
		VkClearColorValue color = {{ i ? 0 : 1, i ? 1 : 0, 0, 1 }};
		abi_barrier(command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT);
		abi_image_activate(command, &test->images[ABI_FRAMES * 5 + i]);
		vkCmdClearColorImage(command, test->images[ABI_FRAMES * 5 + i].image, VK_IMAGE_LAYOUT_GENERAL, &color, 1, &range);
		abi_barrier(command, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
		abi_image_copy(test, command, &test->images[ABI_FRAMES * 5 + i], host->gpu + offset + i * 4, 0);
	}
	abi_barrier(command, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT);
	ABI_TRY(vkEndCommandBuffer(command));
	if (!abi_submit(test, 0, command, VK_NULL_HANDLE, 0, test->timeline, ++test->sequence) || !abi_wait(test, test->sequence) || !abi_cache(test, offset, test->atom, 1))
		return 0;
	if (memcmp(host->cpu + offset, "\377\0\0\377\0\377\0\377", 8)) {
		fprintf(stderr, "rend_abi: ordered alias discard/readback mismatch\n");
		return 0;
	}
	puts("rend_abi: ordered texture alias handoff passed");
	return 1;
}

int
abi_cross_queue(AbiTest *test)
{
	VkDeviceSize offset = ABI_FRAMES * ABI_FRAME_BYTES + 16;
	uint32_t expected = UINT32_C(0x76543210);
	VkCommandBuffer command = test->commands[ABI_FRAMES];

	if (test->queue_count < 2) {
		puts("rend_abi: cross-family execution NOT TESTED (no second queue family)");
		return 1;
	}
	memcpy(test->buffers[0].cpu + offset, &expected, 4);
	if (!abi_cache(test, ABI_FRAMES * ABI_FRAME_BYTES, ABI_FRAME_BYTES, 0) || !abi_begin(test->cross_command))
		return 0;
	abi_copy(test, test->cross_command, test->buffers[0].gpu + offset, test->buffers[1].gpu + offset, 4);
	ABI_TRY(vkEndCommandBuffer(test->cross_command));
	if (!abi_submit(test, 1, test->cross_command, VK_NULL_HANDLE, 0, test->cross_timeline, 1) || !abi_begin(command))
		return 0;
	abi_copy(test, command, test->buffers[1].gpu + offset, test->buffers[0].gpu + offset + 4, 4);
	abi_barrier(command, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT);
	ABI_TRY(vkEndCommandBuffer(command));
	if (!abi_submit(test, 0, command, test->cross_timeline, 1, test->timeline, ++test->sequence) || !abi_wait(test, test->sequence) || !abi_cache(test, ABI_FRAMES * ABI_FRAME_BYTES, ABI_FRAME_BYTES, 1))
		return 0;
	if (memcmp(test->buffers[0].cpu + offset + 4, &expected, 4))
		return 0;
	puts("rend_abi: cross-family timeline/concurrent-sharing copy passed");
	return 1;
}

int
abi_work(AbiTest *test)
{
	uint32_t round, i;

	if (!abi_initialize_images(test))
		return 0;
	for (round = 0; round < ABI_ROUNDS; round++) {
		uint64_t previous = test->sequence, counter;
		VkSemaphoreSignalInfo release = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO, .semaphore = test->gate, .value = round + 1 };
		VkSemaphoreWaitInfo timeout = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO, .semaphoreCount = 1, .pSemaphores = &test->timeline, .pValues = &test->sequence };
		for (i = 0; i < ABI_FRAMES; i++) {
			if (!abi_prepare(test, i, round) || !abi_record(test, i, round & 1))
				return 0;
			/* A pre-submit update proves recording captured an address, not
			 * a root snapshot. Never modify the record while GPU work is pending.
			 */
			((AbiRoot *)(test->buffers[0].cpu + test->frames[i].root[0]))->tag += 2;
			((AbiRoot *)(test->buffers[0].cpu + test->frames[i].root[1]))->tag += 2;
			test->frames[i].tag += 2;
			/* The nested values were prepared with the original tag. */
			if (!abi_cache(test, i * ABI_FRAME_BYTES, ABI_FRAME_BYTES, 0))
				return 0;
			test->frames[i].completion = ++test->sequence;
			if (!abi_submit(test, 0, test->commands[i], test->gate, round + 1, test->timeline, test->sequence))
				return 0;
		}
		ABI_TRY(vkGetSemaphoreCounterValue(test->device, test->timeline, &counter));
		if (counter != previous || vkWaitSemaphores(test->device, &timeout, 0) != VK_TIMEOUT) {
			fprintf(stderr, "rend_abi: pending-submission/timeout proof failed\n");
			return 0;
		}
		ABI_TRY(vkSignalSemaphore(test->device, &release));
		if (!abi_wait(test, test->sequence))
			return 0;
		for (i = 0; i < ABI_FRAMES; i++)
			if (!abi_verify(test, i))
				return 0;
		printf("rend_abi: batch %u: %u proven pending submissions; %s address commands; exact roots/layout/pixels passed\n", round, ABI_FRAMES, (round & 1) ? "indirect" : "direct");
	}
	return abi_aliases(test) && abi_cross_queue(test);
}

int
abi_cleanup(AbiTest *test)
{
	uint32_t i;
	int ok = 1;

	if (test->device) {
		/* Explicit shutdown drain only. Release a held workload gate even when a
		 * recording/submission fails partway through the pending-work proof.
		 */
		if (test->gate) {
			VkSemaphoreSignalInfo release = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO, .semaphore = test->gate, .value = ABI_ROUNDS + 1 };
			ok &= abi_result(vkSignalSemaphore(test->device, &release), "shutdown gate release");
		}
		ok &= abi_result(vkDeviceWaitIdle(test->device), "explicit shutdown vkDeviceWaitIdle");
		for (i = 0; i < 3; i++)
			if (test->pipelines[i]) vkDestroyPipeline(test->device, test->pipelines[i], NULL);
		for (i = 0; i < 4; i++)
			if (test->shaders[i]) vkDestroyShaderModule(test->device, test->shaders[i], NULL);
		for (i = 0; i < ABI_IMAGE_COUNT; i++) {
			if (test->images[i].view) vkDestroyImageView(test->device, test->images[i].view, NULL);
			if (test->images[i].image) vkDestroyImage(test->device, test->images[i].image, NULL);
		}
		if (test->texture_memory) {
			vkFreeMemory(test->device, test->texture_memory, NULL);
			test->gpu_live -= test->texture_bytes;
			test->allocation_count--;
		}
		for (i = 0; i < ABI_BUFFER_COUNT; i++) {
			AbiBuffer *buffer = &test->buffers[i];
			if (buffer->cpu) vkUnmapMemory(test->device, buffer->memory);
			if (buffer->buffer) vkDestroyBuffer(test->device, buffer->buffer, NULL);
			if (buffer->memory) {
				vkFreeMemory(test->device, buffer->memory, NULL);
				test->gpu_live -= buffer->allocated;
				test->allocation_count--;
			}
		}
		if (test->timeline) vkDestroySemaphore(test->device, test->timeline, NULL);
		if (test->gate) vkDestroySemaphore(test->device, test->gate, NULL);
		if (test->cross_timeline) vkDestroySemaphore(test->device, test->cross_timeline, NULL);
		for (i = 0; i < 2; i++)
			if (test->pools[i]) vkDestroyCommandPool(test->device, test->pools[i], NULL);
		vkDestroyDevice(test->device, NULL);
	}
	if (test->messenger) test->destroy_messenger(test->instance, test->messenger, NULL);
	if (test->instance) vkDestroyInstance(test->instance, NULL);
	if (test->gpu_live || test->allocation_count || test->validation_errors || abi_backing_attempts)
		ok = 0;
	printf("rend_abi: cleanup: GPU bytes/live allocations %" PRIu64 "/%u; validation errors/warnings %u/%u; direct host backing attempts %u\n", test->gpu_live, test->allocation_count, test->validation_errors, test->validation_warnings, abi_backing_attempts);
#ifndef __linux__
	puts("rend_abi: direct host backing instrumentation NOT AVAILABLE on this build");
#endif
	return ok;
}

int
abi_number(const char *text, uint32_t *number)
{
	char *end;
	unsigned long value;

	errno = 0;
	value = strtoul(text, &end, 10);
	if (errno || !*text || *end || value > UINT32_MAX || *text == '-')
		return 0;
	*number = (uint32_t)value;
	return 1;
}

int
main(int argc, char **argv)
{
	uint32_t device = 0;
	int profile = 0, full = 0, list = 0, result = 0, i;
	double start, initialized;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--layout")) {
			abi_layout();
			return 0;
		} else if (!strcmp(argv[i], "--list")) {
			list = 1;
		} else if (!strcmp(argv[i], "--device") && i + 1 < argc) {
			if (!abi_number(argv[++i], &device)) goto usage;
		} else if (!strcmp(argv[i], "--profile") && i + 1 < argc) {
			i++;
			if (strcmp(argv[i], "graphics-compute") && strcmp(argv[i], "full")) goto usage;
			profile = 1;
			full = !strcmp(argv[i], "full");
		} else {
			goto usage;
		}
	}
	if (!profile && !list)
		goto usage;
	start = abi_now();
	if (!abi_instance(&abi_test) || !abi_capabilities(&abi_test, device, list, full))
		goto done;
	if (list) {
		result = 1;
		goto done;
	}
	if (!abi_device(&abi_test) || !abi_storage(&abi_test))
		goto done;
	initialized = abi_now() - start;
	if (!abi_work(&abi_test))
		goto done;
	printf("rend_abi: measurements: initialize %.3f ms; submits %.3f ms total; completion waits %.3f ms total; peak native GPU bytes %" PRIu64 "; native allocation attempts %u; native acquisition calls %u; retained demo host capacity %zu bytes\n", initialized, abi_test.submit_ms, abi_test.wait_ms, abi_test.gpu_peak, abi_test.allocation_attempts, abi_create_step, sizeof(abi_test));
	result = 1;
	done:
	if (!abi_cleanup(&abi_test))
		return 1;
	if (result && !list)
		puts("rend_abi: Vulkan graphics-compute feasibility passed; NOT full Rend conformance");
	return result ? 0 : 1;
	usage:
	fprintf(stderr, "usage: %s --layout | --list | --device INDEX --profile graphics-compute|full\n", argv[0]);
	return 2;
}
