#ifndef REND2_REND_INTERNAL_H
#define REND2_REND_INTERNAL_H
#include "rend.h"
#include <vulkan/vulkan.h>
#define REND_DISCOVERY_MAGIC UINT32_C(0x52443244)
#define REND_BACKEND_MAGIC UINT32_C(0x52443242)
#define REND_DISCOVERY_DEVICE_CAPACITY 32u
#define REND_QUEUE_FAMILY_CAPACITY 128u
#define REND_DISCOVERY_ALIGNMENT 16u
#define REND_CONTEXT_ALIGNMENT 64u
#define REND_MAX_MEMORY_TYPES 32u
enum
{
	REND_MISSING_DESCRIPTOR_HEAP = 1u << 0,
	REND_MISSING_ADDRESS_COMMANDS = 1u << 1,
	REND_MISSING_UNTYPED_POINTERS = 1u << 2,
	REND_MISSING_SHADER_DRAW_PARAMETERS = 1u << 0,
	REND_MISSING_BUFFER_DEVICE_ADDRESS = 1u << 1,
	REND_MISSING_SCALAR_BLOCK_LAYOUT = 1u << 2,
	REND_MISSING_TIMELINE_SEMAPHORE = 1u << 3,
	REND_MISSING_SYNCHRONIZATION2 = 1u << 4,
	REND_MISSING_DYNAMIC_RENDERING = 1u << 5,
	REND_MISSING_SHADER_INT64 = 1u << 6,
	REND_MISSING_STORAGE_IMAGE_WRITE = 1u << 7,
	REND_MISSING_PUSH_DATA_SIZE = 1u << 8,
	REND_MISSING_DESCRIPTOR_HEAP_FEATURE = 1u << 9,
	REND_MISSING_ADDRESS_COMMANDS_FEATURE = 1u << 10,
	REND_MISSING_UNTYPED_POINTERS_FEATURE = 1u << 11,
	REND_MISSING_ROOT_ALIGNMENT = 1u << 12,
	REND_MISSING_API_14 = 1u << 0,
	REND_MISSING_GRAPHICS_COMPUTE_QUEUE = 1u << 0
};
struct RendDeviceInfo
{
	VkPhysicalDevice physical;
	char name[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE];
	uint32_t family_offset, family_count;
	const RendQueueFamilyInfo *family_records;
	uint32_t api_version, graphics_compute, full_profile;
	uint32_t missing_extensions, missing_features, missing_queue_caps;
	uint32_t swapchain_extension_supported;
	struct RendDiscovery *owner;
	VkPhysicalDeviceMemoryProperties memory;
	VkPhysicalDeviceDescriptorHeapPropertiesEXT heap_properties;
	VkPhysicalDeviceProperties properties;
};
struct RendDiscovery
{
	uint32_t magic, device_count, family_capacity, family_count, context_count;
	RendQueueFamilyInfo *families;
	VkInstance instance;
	struct RendDeviceInfo devices[REND_DISCOVERY_DEVICE_CAPACITY];
};
typedef struct RendHeapState
{
	uint32_t live, refs;
	RendBackend backend;
	RendHeapKind kind;
	uint32_t memory_class, memory_type, properties;
	uint64_t size, allocated, cache_atom;
	VkDeviceMemory memory;
	VkBuffer buffer;
	VkDeviceAddress address;
	void *mapped;
	uint32_t descriptor_count;
	VkBindHeapInfoEXT bind_info;
} RendHeapRecord;
typedef struct RendTextureState
{
	uint32_t live, refs, initialized;
	RendBackend backend;
	RendHeap heap;
	RendTextureDesc desc;
	VkImage image;
	VkDeviceSize offset, requirements_size, requirements_alignment;
} RendTextureRecord;
typedef struct RendPresentationState RendPresentationRecord;
typedef struct RendTextureViewState
{
	uint32_t live, refs, external;
	RendBackend backend;
	RendTexture texture;
	VkImageView view;
	RendTextureViewDesc desc;
	uint32_t width, height;
	RendFormat format;
	RendPresentationRecord *presentation;
} RendTextureViewRecord;
typedef struct RendPipelineState
{
	uint32_t live, refs;
	RendBackend backend;
	VkPipeline pipeline;
	RendFormat format, depth_format;
} RendPipelineRecord;
struct RendPresentationState
{
	uint32_t live, acquired, submitted, needs_retire, image_count, width, height,
		queue_index;
	RendBackend backend;
	VkSurfaceKHR surface;
	VkSwapchainKHR swapchain;
	RendFormat format;
	VkImage images[8];
	VkImageView views[8];
	VkSemaphore acquire_semaphores[8], render_semaphores[8];
	RendTimeline acquire_timeline[8];
	uint64_t acquire_values[8];
	RendTextureViewRecord view_records[8];
	uint32_t image_index, acquire_slot, next_acquire_slot;
};
typedef struct RendTimelineState
{
	uint32_t live;
	RendBackend backend;
	RendQueue producer;
	VkSemaphore semaphore;
	uint64_t last_signal;
	uint32_t outstanding;
} RendTimelineRecord;
typedef struct RendQueueWaitRef
{
	RendTimeline timeline;
	uint64_t value;
} RendQueueWaitRef;
typedef struct RendQueueSubmission
{
	RendTimeline timeline;
	uint64_t value;
	uint32_t live, uncertain, wait_count;
} RendQueueSubmission;
typedef struct RendQueueState
{
	uint32_t live;
	RendBackend backend;
	VkQueue queue;
	uint32_t family, index, max_submit_lists, max_waits, submission_capacity,
		active_submissions;
	RendQueueSubmission *submissions;
	RendQueueWaitRef *wait_refs;
} RendQueueRecord;
typedef struct RendCommandRef
{
	uint32_t kind;
	void *object;
} RendCommandRef;
typedef struct RendCommandListState
{
	uint32_t state;
	RendBackend backend;
	RendCommandPool pool;
	VkCommandBuffer command;
	RendTimeline completion;
	uint64_t completion_value;
	uint32_t in_render;
	RendPipeline pipeline;
	RendTextureRecord *color_target, *depth_target;
	RendPresentationRecord *presentation;
	RendFormat active_color_format;
	uint32_t has_depth, viewport_set, scissor_set, depth_state_set;
	uint32_t render_width, render_height;
	uint32_t ref_count;
	RendCommandRef refs[16];
} RendCommandRecord;
typedef struct RendCommandPoolState
{
	uint32_t live;
	RendBackend backend;
	uint32_t family, count;
	VkCommandPool pool;
	RendCommandRecord *commands;
	void *scratch, *metadata;
	size_t scratch_bytes, metadata_bytes;
} RendPoolRecord;
struct RendBackendState
{
	uint32_t magic, terminal, drained;
	void *backing;
	size_t backing_size;
	void *scratch;
	size_t scratch_bytes;
	const RendDeviceInfo *device_info;
	VkDevice device;
	VkPhysicalDevice physical;
	VkPhysicalDeviceMemoryProperties memory;
	VkPhysicalDeviceDescriptorHeapPropertiesEXT heap_properties;
	VkDeviceSize atom;
	uint32_t root_alignment;
	uint32_t heap_count, texture_count, view_count, pipeline_count,
		timeline_count, queue_count, pool_count, presentation_count;
	RendHeapRecord *heaps;
	RendTextureRecord *textures;
	RendTextureViewRecord *views;
	RendPipelineRecord *pipelines;
	RendTimelineRecord *timelines;
	RendQueueRecord *queues;
	RendPoolRecord *pools;
	RendPresentationRecord *presentations;
	PFN_vkWriteResourceDescriptorsEXT write_resources;
	PFN_vkWriteSamplerDescriptorsEXT write_samplers;
	PFN_vkCmdBindResourceHeapEXT bind_resources;
	PFN_vkCmdBindSamplerHeapEXT bind_samplers;
	PFN_vkCmdPushDataEXT push_data;
	PFN_vkCmdCopyMemoryToImageKHR copy_to_image;
	PFN_vkCmdCopyImageToMemoryKHR copy_from_image;
	PFN_vkCmdDrawIndexedIndirect2KHR draw_indexed;
	PFN_vkCmdBindIndexBuffer3KHR bind_index;
};
static void rend_vk_set_diagnostic(RendDiagnosticCode code, int native_code,
				   const char *message);
int rend_vk_surface_support(const RendDeviceInfo *device, uint64_t surface,
			    uint32_t family, int *supported);
#endif
