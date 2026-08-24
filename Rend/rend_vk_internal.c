#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include "rend.h"
#include "rend_internal.h"

#define CHECK_VK_RESULT(r)                                                     \
{                                                                            \
	assert(r == VK_SUCCESS && __LINE__);                                       \
}

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
	int reserved;
} RendVkPage;

typedef struct RendVkPagedArena {
	RendVkPage *page_darr;
	uint32_t capacity;
	uint32_t elements;
} RendVkPagedArena;

struct RendVkArenaAllocator {
	VkAllocationCallbacks *allocator;
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

typedef struct RendVkAllocatorHeader {
	size_t size;
	size_t pad;
} RendVkAllocatorHeader;

/* Ginger Bill's linear arena: 
 * - https://www.gingerbill.org/article/2019/02/08/memory-allocation-strategies-002/ */
typedef struct RendVkHostArena {
	unsigned char *buf;
	size_t buf_len;
	size_t prev_offset;
	size_t curr_offset;
} RendVkHostArena;

#define REND_VK_HOST_ARENA_SIZE (1ull * 1024ull * 1024ull)
#define REND_VK_CMD_ARENA_SIZE  (1ull * 1024ull * 1024ull)
#define REND_VK_HOST_DEFAULT_ALIGNMENT (2 * sizeof(void *))

static bool rend_vk_is_power_of_two(uintptr_t x);
static uintptr_t rend_vk_align_forward(uintptr_t ptr, size_t align);
static void rend_vk_host_arena_init(void);
static void rend_vk_host_arena_destroy(void);
static void rend_vk_host_arena_init_buf(RendVkHostArena *a, void *backing_buffer, size_t backing_buffer_length);
static void *rend_vk_host_arena_alloc_align(RendVkHostArena *a, size_t size, size_t align);
static void rend_vk_host_arena_free(RendVkHostArena *a, void *ptr);
static void *rend_vk_host_arena_resize_align(RendVkHostArena *a, void *old_memory, size_t old_size, size_t new_size, size_t align);
static void rend_vk_host_arena_free_all(RendVkHostArena *a);

static void *rend_vk_allocator_alloc(void *pUserData, size_t size, size_t alignment, VkSystemAllocationScope allocationScope);
static void *rend_vk_allocator_realloc(void *pUserData, void *pOriginal, size_t size, size_t alignment, VkSystemAllocationScope allocationScope);
static void rend_vk_allocator_free(void *pUserData, void *pMemory);
static void rend_vk_allocator_internal_notification(void *pUserData, size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope);
static void rend_vk_allocator_free_notification(void *pUserData, size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope);

static RendVkHostArena vk_host_arena;
static RendVkHostArena vk_cmd_arena;
static size_t vk_cmd_live;
static int vk_host_arena_lock;

static VkAllocationCallbacks rend_vk_allocator = {
	.pUserData = &vk_host_arena,
	.pfnAllocation = rend_vk_allocator_alloc,
	.pfnReallocation = rend_vk_allocator_realloc,
	.pfnFree = rend_vk_allocator_free,
	.pfnInternalAllocation = rend_vk_allocator_internal_notification,
	.pfnInternalFree = rend_vk_allocator_free_notification,
};

static const char *rend_vk_allocator_scope_name[] = {
	[VK_SYSTEM_ALLOCATION_SCOPE_CACHE] = "Cache",
	[VK_SYSTEM_ALLOCATION_SCOPE_COMMAND] = "Command",
	[VK_SYSTEM_ALLOCATION_SCOPE_DEVICE] = "Device",
	[VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE] = "Instance",
	[VK_SYSTEM_ALLOCATION_SCOPE_OBJECT] = "Object",
};

static VkInstance vk_instance = 0;
static VkDebugUtilsMessengerEXT vk_debug_messenger = 0;
static RendVkDevice vk_device = {0};
static VkAllocationCallbacks *vk_allocator = &rend_vk_allocator;

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
	[REND_BUFFER_TRANSFER] = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
};

static const VkShaderStageFlagBits vk_pipeline_stages[][3] = {
	[REND__PIPELINE_GRAPHICS] = { VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT },
	[REND__PIPELINE_COMPUTE]  = { VK_SHADER_STAGE_COMPUTE_BIT },
	[REND__PIPELINE_MESH]     = { VK_SHADER_STAGE_MESH_BIT_EXT, VK_SHADER_STAGE_FRAGMENT_BIT },
};

/* --- host allocator (linear arena) --- */

static bool
rend_vk_is_power_of_two(uintptr_t x)
{
	return (x & (x - 1)) == 0;
}

static uintptr_t
rend_vk_align_forward(uintptr_t ptr, size_t align)
{
	uintptr_t p, a, modulo;

	assert(rend_vk_is_power_of_two(align));

	p = ptr;
	a = (uintptr_t)align;
	modulo = p & (a - 1);

	if (modulo != 0) {
		p += a - modulo;
	}
	return p;
}

static void
rend_vk_host_lock(void)
{
	while (__sync_lock_test_and_set(&vk_host_arena_lock, 1))
		;
}

static void
rend_vk_host_unlock(void)
{
	__sync_lock_release(&vk_host_arena_lock);
}

static void
rend_vk_host_arena_init_buf(RendVkHostArena *a, void *backing_buffer, size_t backing_buffer_length)
{
	a->buf = (unsigned char *)backing_buffer;
	a->buf_len = backing_buffer_length;
	a->curr_offset = 0;
	a->prev_offset = 0;
}

static void *
rend_vk_host_arena_alloc_align(RendVkHostArena *a, size_t size, size_t align)
{
	uintptr_t curr_ptr;
	uintptr_t offset;
	void *ptr;

	curr_ptr = (uintptr_t)a->buf + (uintptr_t)a->curr_offset;
	offset = rend_vk_align_forward(curr_ptr, align);
	offset -= (uintptr_t)a->buf;

	if (offset + size <= a->buf_len) {
		ptr = &a->buf[offset];
		a->prev_offset = offset;
		a->curr_offset = offset + size;
		memset(ptr, 0, size);
		return ptr;
	}
	return NULL;
}

static void
rend_vk_host_arena_free(RendVkHostArena *a, void *ptr)
{
	(void)a;
	(void)ptr;
}

static void *
rend_vk_host_arena_resize_align(RendVkHostArena *a, void *old_memory, size_t old_size, size_t new_size, size_t align)
{
	unsigned char *old_mem;
	void *new_memory;
	size_t copy_size;

	old_mem = (unsigned char *)old_memory;

	assert(rend_vk_is_power_of_two(align));

	if (old_mem == NULL || old_size == 0) {
		return rend_vk_host_arena_alloc_align(a, new_size, align);
	} else if (a->buf <= old_mem && old_mem < a->buf + a->buf_len) {
		if (a->buf + a->prev_offset == old_mem) {
			if (a->prev_offset + new_size > a->buf_len) {
				return NULL;
			}
			a->curr_offset = a->prev_offset + new_size;
			if (new_size > old_size) {
				memset(&a->buf[a->prev_offset + old_size], 0, new_size - old_size);
			}
			return old_memory;
		} else {
			new_memory = rend_vk_host_arena_alloc_align(a, new_size, align);
			if (!new_memory) {
				return NULL;
			}
			copy_size = old_size < new_size ? old_size : new_size;
			memmove(new_memory, old_memory, copy_size);
			return new_memory;
		}
	} else {
		assert(0 && "Memory is out of bounds of the buffer in this arena");
		return NULL;
	}
}

static void
rend_vk_host_arena_free_all(RendVkHostArena *a)
{
	a->curr_offset = 0;
	a->prev_offset = 0;
}

static void
rend_vk_host_arena_create(RendVkHostArena *a, size_t size)
{
	void *backing;

	backing = malloc(size);
	assert(backing && "Vulkan host arena backing buffer");
	rend_vk_host_arena_init_buf(a, backing, size);
}

static void
rend_vk_host_arena_release(RendVkHostArena *a)
{
	if (!a->buf) {
		return;
	}
	free(a->buf);
	a->buf = NULL;
	a->buf_len = 0;
	rend_vk_host_arena_free_all(a);
}

static void
rend_vk_host_arena_init(void)
{
	if (vk_host_arena.buf) {
		return;
	}
	rend_vk_host_arena_create(&vk_host_arena, REND_VK_HOST_ARENA_SIZE);
	rend_vk_host_arena_create(&vk_cmd_arena, REND_VK_CMD_ARENA_SIZE);
	vk_cmd_live = 0;
}

static void
rend_vk_host_arena_destroy(void)
{
	rend_vk_host_arena_release(&vk_host_arena);
	rend_vk_host_arena_release(&vk_cmd_arena);
	vk_cmd_live = 0;
}

static int
rend_vk_host_arena_contains(const RendVkHostArena *a, const void *p)
{
	const unsigned char *c;

	if (!a->buf || !p) {
		return 0;
	}
	c = p;
	return c >= a->buf && c < a->buf + a->buf_len;
}

static RendVkHostArena *
rend_vk_host_arena_for_scope(VkSystemAllocationScope scope)
{
	if (scope == VK_SYSTEM_ALLOCATION_SCOPE_COMMAND) {
		return &vk_cmd_arena;
	}
	return &vk_host_arena;
}

static RendVkHostArena *
rend_vk_host_arena_for_ptr(void *p)
{
	if (rend_vk_host_arena_contains(&vk_cmd_arena, p)) {
		return &vk_cmd_arena;
	}
	return &vk_host_arena;
}

static void
rend_vk_cmd_release_one(void)
{
	if (vk_cmd_live == 0) {
		return;
	}
	vk_cmd_live--;
	if (vk_cmd_live == 0) {
		rend_vk_host_arena_free_all(&vk_cmd_arena);
		PTRACE("[VK_ARENA] command rewind");
	}
}

static size_t
rend_vk_host_header_pad(size_t alignment)
{
	return rend_vk_align_forward(sizeof(RendVkAllocatorHeader), alignment);
}

static void *
rend_vk_allocator_alloc(void *pUserData, size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
{
	RendVkHostArena *arena;
	size_t pad;
	unsigned char *block;
	unsigned char *user;
	RendVkAllocatorHeader *header;

	(void)pUserData;
	if (size == 0) {
		return NULL;
	}
	if (alignment < REND_VK_HOST_DEFAULT_ALIGNMENT) {
		alignment = REND_VK_HOST_DEFAULT_ALIGNMENT;
	}

	arena = rend_vk_host_arena_for_scope(allocationScope);
	pad = rend_vk_host_header_pad(alignment);

	rend_vk_host_lock();
	block = rend_vk_host_arena_alloc_align(arena, pad + size, alignment);
	if (block && arena == &vk_cmd_arena) {
		vk_cmd_live++;
	}
	rend_vk_host_unlock();
	if (!block) {
		return NULL;
	}

	user = block + pad;
	header = (RendVkAllocatorHeader *)user - 1;
	header->size = size;
	header->pad = pad;

	return user;
}

static void *
rend_vk_allocator_realloc(void *pUserData, void *pOriginal, size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
{
	RendVkHostArena *arena;
	RendVkAllocatorHeader *header;
	size_t old_size;
	size_t old_pad;
	size_t new_pad;
	unsigned char *old_block;
	unsigned char *new_block;
	unsigned char *user;
	void *result;

	if (!pOriginal) {
		return rend_vk_allocator_alloc(pUserData, size, alignment, allocationScope);
	}
	if (size == 0) {
		rend_vk_allocator_free(pUserData, pOriginal);
		return NULL;
	}
	if (alignment < REND_VK_HOST_DEFAULT_ALIGNMENT) {
		alignment = REND_VK_HOST_DEFAULT_ALIGNMENT;
	}

	(void)pUserData;
	arena = rend_vk_host_arena_for_ptr(pOriginal);
	header = (RendVkAllocatorHeader *)pOriginal - 1;
	old_size = header->size;
	old_pad = header->pad;
	old_block = (unsigned char *)pOriginal - old_pad;
	new_pad = rend_vk_host_header_pad(alignment);

	rend_vk_host_lock();
	if (old_pad == new_pad) {
		new_block = rend_vk_host_arena_resize_align(arena, old_block, old_pad + old_size, new_pad + size, alignment);
	} else {
		new_block = rend_vk_host_arena_alloc_align(arena, new_pad + size, alignment);
		if (new_block) {
			memmove(new_block + new_pad, pOriginal, old_size < size ? old_size : size);
		}
	}
	rend_vk_host_unlock();
	if (!new_block) {
		return NULL;
	}

	user = new_block + new_pad;
	header = (RendVkAllocatorHeader *)user - 1;
	header->size = size;
	header->pad = new_pad;
	result = user;

	return result;
}

static void
rend_vk_allocator_free(void *pUserData, void *pMemory)
{
	(void)pUserData;
	if (!pMemory) {
		return;
	}
	rend_vk_host_lock();
	if (rend_vk_host_arena_contains(&vk_cmd_arena, pMemory)) {
		rend_vk_cmd_release_one();
	} else {
		rend_vk_host_arena_free(&vk_host_arena, pMemory);
	}
	rend_vk_host_unlock();
}

static void
rend_vk_allocator_internal_notification(void *pUserData, size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope)
{
	(void)pUserData;
	(void)allocationType;
	PDEBUG("[VK_ALLOC_INTERNAL] bytes %lu - scope %s", (unsigned long)size, rend_vk_allocator_scope_name[allocationScope]);
}

static void
rend_vk_allocator_free_notification(void *pUserData, size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope)
{
	(void)pUserData;
	(void)allocationType;
	PDEBUG("[VK_FREE_INTERNAL] bytes %lu - scope %s",
			(unsigned long)size, rend_vk_allocator_scope_name[allocationScope]);
}

/* --- device --- */

static void
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
}

static uint32_t
rend_vk_device_score_default(RendVkDevice *device, RendSpecs minimum_specs, const char **required_extensions, uint32_t required_extension_count)
{
	uint32_t score;
	uint32_t q_family_count;
	uint8_t min_transfer_score;
	uint32_t i;

	score = 0;

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

			if (q_family[i].queueFlags & VK_QUEUE_TRANSFER_BIT) {
				if (transfer_score <= min_transfer_score) {
					min_transfer_score = transfer_score;
					device->transfer_family_index = i;
				}
			}

			supports_present = VK_FALSE;
			CHECK_VK_RESULT(vkGetPhysicalDeviceSurfaceSupportKHR(device->physical_device, i, device->surface, &supports_present));
			if (supports_present) {
				device->present_family_index = i;
			}
		}
	}

	if ((minimum_specs.graphics && device->graphics_family_index == UINT32_MAX) ||
			(minimum_specs.present && device->present_family_index == UINT32_MAX) ||
			(minimum_specs.compute && device->compute_family_index == UINT32_MAX) ||
			(minimum_specs.transfer && device->transfer_family_index == UINT32_MAX)) {
		return 0;
	}

	rend_vk_device_query_swapchain_support(device);

	if (device->swapchain_support.format_count < 1 || device->swapchain_support.present_mode_count < 1) {
		return 0;
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
			CHECK_VK_RESULT(vkEnumerateDeviceExtensionProperties(device->physical_device, VK_NULL_HANDLE, &available_extentions_count, available_extentions));

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
				return 0;
			}
		}
	}

	if (minimum_specs.sampler_anisotropy && !device->features.samplerAnisotropy) {
		return 0;
	}

	score = 10;
	if (device->properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
		score += 1000;
	}

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

		PDEBUG("DEVICE                SCORE");
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
			PDEBUG("%-20.20s  %5d", scoring.properties.deviceName, dev_score);
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
		device_features.samplerAnisotropy = specs.sampler_anisotropy;
		device_features.fillModeNonSolid = VK_TRUE;
		device_features.shaderInt64 = VK_TRUE;

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
rend_vk_arena_create(VkDevice logical_device, VkPhysicalDevice physical_device, VkPhysicalDeviceLimits device_limits, VkAllocationCallbacks *allocator)
{
	RendVkArenaAllocator arena;
	VkPhysicalDeviceMemoryProperties mem_properties;
	VkDeviceSize alignment;
	uint32_t count;
	uint32_t u;

	arena = (RendVkArenaAllocator){
		.allocator = allocator,
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
	arena.block_min_size = arena.gpu_alignment * 10;

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

	new_arena_size = fit_to_alloc ? size : (size * 2);
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

	res = vkAllocateMemory(arena->logical_device, &alloc_info, arena->allocator, (VkDeviceMemory *)&page.memory.device_memory);
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
			vkFreeMemory(arena->logical_device, (VkDeviceMemory)page.memory.device_memory, arena->allocator);
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
rend_vk_arena_alloc(RendVkArenaAllocator *arena, VkDeviceSize size, uint32_t heap_index)
{
	RendVkPagedArena *mem_arena;
	VkMemoryPropertyFlags properties;
	VkDeviceSize align;
	VkDeviceSize aligned_size;
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
	aligned_size = (size + align - 1) & ~(align - 1);

	whole_page = !(properties & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	page_idx = UINT32_MAX;
	for (u = 0; u < mem_arena->elements; ++u) {
		page = mem_arena->page_darr[u];
		valid = (whole_page) ? (page.head == 0) : 1;
		if ((page.head + aligned_size <= page.memory.size) && valid) {
			page_idx = u;
			break;
		}
	}

	if (page_idx == UINT32_MAX) {
		page_idx = rend_vk_arena_add_page(arena, aligned_size, heap_index, whole_page);
		if (page_idx == UINT32_MAX) {
			REND__CRASH("[REND_VK] Arena page allocation failed!");
		}
	}

	mem_arena->page_darr[page_idx].reserved = whole_page;

	memory = (RendMemory){
		.device_memory = mem_arena->page_darr[page_idx].memory.device_memory,
		.size = aligned_size,
		.offset = mem_arena->page_darr[page_idx].head,
		.host_mapped_memory = 0,
		.heap_index = heap_index,
		.id = page_idx,
	};

	if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
		memory.host_mapped_memory = mem_arena->page_darr[page_idx].memory.host_mapped_memory + memory.offset;
	}

	mem_arena->page_darr[page_idx].head += aligned_size;
	return memory;
}

static void
rend_vk_arena_clear(RendVkArenaAllocator *arena, uint32_t heap_index)
{
	RendVkPagedArena mem_arena;
	uint32_t u;

	assert(heap_index < 32 && "Unusual heap index. Did you pass the memory type instead?");
	mem_arena = arena->mem_arenas[heap_index];
	if (mem_arena.page_darr) {
		for (u = 0; u < mem_arena.elements; ++u) {
			mem_arena.page_darr[u].head = 0;
			mem_arena.page_darr[u].reserved = 0;
		}
	}
}

static void
rend_vk_arena_clear_all(RendVkArenaAllocator *arena)
{
	uint32_t u;

	for (u = 0; u < arena->properties.memoryTypeCount; ++u) {
		rend_vk_arena_clear(arena, u);
	}
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
					vkFreeMemory(arena->logical_device, (VkDeviceMemory)memory.device_memory, arena->allocator);
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
