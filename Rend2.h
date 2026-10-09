/* ===========================================================================
 * REND2 - Bounded Vulkan rendering core. Copyright (c) 2026 Vasco Alves.
 * MIT license; see the complete license at the end of this file.
 *
 * Rend2.h is the self-contained distribution of the existing Rend2 C99 unity
 * module. The rend_* API, caller-backed storage, explicit capacities, native
 * resource ownership, diagnostics and completion behavior are unchanged.
 * No Peak, Grit, or original Rend2 source files are needed to use this header.
 *
 * USAGE:
 *     #define REND2_IMPLEMENTATION
 *     #include "Rend2.h"
 * Define REND2_IMPLEMENTATION in exactly one translation unit. Other translation
 * units include this header without the macro. Link the Vulkan loader (e.g.
 * cc -std=c99 app.c -lvulkan -lm). Repeated inclusion, including declaration-only
 * inclusion followed by implementation inclusion, is supported in one TU.
 * Do not also compile Rend2/rend.c or mix this API with legacy Rend/rend.h:
 * those modules share the rend_* namespace and are not API-compatible.
 *
 * DEPENDENCIES:
 * C99 standard headers and Vulkan SDK headers declaring Vulkan 1.4,
 * VK_EXT_descriptor_heap, VK_KHR_device_address_commands, and
 * VK_KHR_shader_untyped_pointers. Vulkan headers are included for the native
 * rend_vk_discovery_instance bridge. VK_NO_PROTOTYPES is not supported by the
 * implementation; it calls the Vulkan loader directly and loads extension
 * device entrypoints with vkGetDeviceProcAddr.
 * Prepared SPIR-V shaders must match the supported Slang typed-pointer root ABI
 * (vertMain/fragMain). No shader compiler is embedded in the library.
 *
 * OWNERSHIP AND SUPPORTED SUBSET:
 * Discovery backing/family records stay live through backend destruction;
 * context backing stays live through successful teardown. Native surfaces are
 * caller-owned and outlive presentation objects. Bootstrap/diagnostics are
 * caller-serialized. Wait for GPU completion before reusing pointed-to data,
 * reset pools to release recorded references, and explicitly drain on terminal
 * errors before retiring dependent objects/backing.
 * Discovery is bounded to 32 devices and 128 queue families per device. The
 * graphics-compute profile currently accepts one graphics+compute family,
 * queue index zero, fixed object/work capacities and eight presentation images.
 * The snake subset provides data/image heaps, RGBA8/BGRA8 color and D32 depth,
 * indexed graphics with pointer roots, RGBA8 readback and Vulkan presentation.
 * Samplers/descriptor heaps, general compute/task/mesh work, and DirectX/Metal
 * backends are not implemented. Version 2.0.0 denotes the Rend2 API generation,
 * not full conformance to Rend2/SPEC.md. Missing capabilities fail explicitly;
 * there is no legacy binding fallback. This distribution does not add C++
 * implementation support or change the existing external C linkage.
 * =========================================================================== */

#ifndef REND2_H
#define REND2_H

#define REND2_MAJOR 2
#define REND2_MINOR 0
#define REND2_PATCH 0

/* CHANGE LOG
 * 2.0.0 - Single-header distribution of the bounded Rend2 Vulkan subset.
 */

/* Rend 2 bounded Vulkan rendering core. Copyright (c) 2026, MIT license. */
#ifndef REND2_REND_H
#define REND2_REND_H

#include <stddef.h>
#include <stdint.h>

typedef enum RendProfile
{
	REND_PROFILE_NONE = 0,
	REND_PROFILE_FULL,
	REND_PROFILE_GRAPHICS_COMPUTE
} RendProfile;
typedef enum RendBackendKind
{
	REND_BACKEND_NONE = 0,
	REND_BACKEND_VULKAN
} RendBackendKind;
typedef enum RendDiagnosticCode
{
	REND_DIAG_NONE = 0,
	REND_DIAG_INVALID_ARGUMENT,
	REND_DIAG_CAPACITY,
	REND_DIAG_NATIVE,
	REND_DIAG_UNSUPPORTED,
	REND_DIAG_OVERFLOW,
	REND_DIAG_TIMEOUT,
	REND_DIAG_OUT_OF_DATE,
	REND_DIAG_TERMINAL
} RendDiagnosticCode;
typedef struct RendDiagnostic
{
	RendDiagnosticCode code;
	int32_t native_code;
	char message[192];
} RendDiagnostic;
typedef struct RendQueueFamilyInfo
{
	uint32_t family_index, queue_count, capabilities;
} RendQueueFamilyInfo;
enum
{
	REND_QUEUE_GRAPHICS = 1u,
	REND_QUEUE_COMPUTE = 2u,
	REND_QUEUE_TRANSFER = 4u
};
typedef struct RendDeviceInfo RendDeviceInfo;
typedef struct RendDiscovery RendDiscovery;
typedef struct RendBackendState *RendBackend;
typedef struct RendHeapState *RendHeap;
typedef struct RendTextureState *RendTexture;
typedef struct RendTextureViewState *RendTextureView;
typedef struct RendPipelineState *RendPipeline;
typedef struct RendQueueState *RendQueue;
typedef struct RendCommandPoolState *RendCommandPool;
typedef struct RendCommandListState *RendCommandList;
typedef struct RendTimelineState *RendTimeline;
typedef struct RendPresentationState *RendPresentation;

typedef struct RendQueueParams
{
	uint32_t family_index, queue_index, submission_count,
		max_submit_command_lists, max_submit_waits;
} RendQueueParams;
typedef struct RendCommandPoolParams
{
	uint32_t family_index, command_list_count;
	size_t scratch_bytes, metadata_bytes;
} RendCommandPoolParams;
/* Graphics-compute subset currently accepts one graphics+compute queue family,
 * queue index zero, and fixed native per-presentation capacity of eight images.
 * Nonzero sampler_count and descriptor heaps are rejected. */
typedef struct RendParams
{
	RendProfile profile;
	const RendDeviceInfo *device;
	const RendQueueParams *queues;
	const RendCommandPoolParams *command_pools;
	uint32_t heap_count, texture_count, texture_view_count, sampler_count,
		pipeline_count, timeline_count, presentation_count;
	uint32_t queue_count, command_pool_count;
	size_t scratch_bytes;
} RendParams;
typedef struct RendMemoryRequirements
{
	size_t size, alignment;
} RendMemoryRequirements;
typedef struct RendDeviceLimits
{
	uint32_t root_alignment, max_push_bytes;
	uint64_t cache_atom_size;
} RendDeviceLimits;
typedef struct RendStorageRequirements
{
	uint64_t size, alignment;
} RendStorageRequirements;
typedef struct RendHeapClassInfo
{
	uint32_t id, memory_properties;
} RendHeapClassInfo;
enum
{
	REND_MEMORY_DEVICE_LOCAL = 1u,
	REND_MEMORY_HOST_VISIBLE = 2u,
	REND_MEMORY_HOST_COHERENT = 4u,
	REND_MEMORY_HOST_CACHED = 8u
};
typedef enum RendHeapKind
{
	REND_HEAP_NONE = 0,
	REND_HEAP_DATA,
	REND_HEAP_TEXTURE_STORAGE,
	REND_HEAP_TEXTURE_DESCRIPTORS,
	REND_HEAP_SAMPLER_DESCRIPTORS
} RendHeapKind;
typedef struct RendHeapDesc
{
	RendHeapKind kind;
	uint32_t memory_class;
	uint64_t size;
	uint32_t descriptor_count;
} RendHeapDesc;
typedef struct RendHeapInfo
{
	RendHeapKind kind;
	uint32_t memory_class;
	uint64_t size;
	void *cpu_base;
	uint64_t gpu_base, cache_atom_size;
	uint32_t memory_properties, descriptor_count;
} RendHeapInfo;
typedef struct RendHeapRange
{
	RendHeap heap;
	uint64_t offset, size;
} RendHeapRange;
typedef struct RendTextureDesc
{
	uint32_t width, height, depth, mip_levels, array_layers, samples;
	uint32_t format, usage;
} RendTextureDesc;
typedef enum RendFormat
{
	REND_FORMAT_NONE = 0,
	REND_FORMAT_RGBA8_UNORM = 1,
	REND_FORMAT_BGRA8_UNORM = 2,
	REND_FORMAT_D32_FLOAT = 3
} RendFormat;
typedef enum RendViewType
{
	REND_VIEW_NONE = 0,
	REND_VIEW_2D = 1
} RendViewType;
typedef enum RendAspect
{
	REND_ASPECT_NONE = 0,
	REND_ASPECT_COLOR = 1,
	REND_ASPECT_DEPTH = 2
} RendAspect;
typedef struct RendTextureViewDesc
{
	RendFormat format;
	RendViewType view_type;
	RendAspect aspect;
	uint32_t base_mip, mip_count, base_layer, layer_count;
} RendTextureViewDesc;
enum
{
	REND_TEXTURE_SAMPLED = 1u,
	REND_TEXTURE_STORAGE = 2u,
	REND_TEXTURE_COLOR_ATTACHMENT = 4u,
	REND_TEXTURE_TRANSFER_SRC = 8u,
	REND_TEXTURE_TRANSFER_DST = 16u,
	REND_TEXTURE_DEPTH_ATTACHMENT = 32u
};
typedef struct RendShaderCode
{
	const void *bytes;
	size_t size;
} RendShaderCode;
typedef struct RendGraphicsPipelineDesc
{
	RendShaderCode vertex, fragment;
	RendFormat color_format, depth_format;
	uint32_t samples;
	/* Required artifact identity: validated snake Slang pointer-root/SPIR-V
	 * convention. */
	uint32_t abi_tag;
} RendGraphicsPipelineDesc;
enum
{
	REND_SHADER_ABI_SLANG_TYPED_POINTER_ROOT_V1 = 0x52530101u
};
typedef struct RendRenderAttachment
{
	RendTextureView view;
	float clear[4];
} RendRenderAttachment;
typedef struct RendDepthAttachment
{
	RendTextureView view;
	float clear_depth;
} RendDepthAttachment;
typedef struct RendRenderDesc
{
	const RendRenderAttachment *colors;
	uint32_t color_count;
	const RendDepthAttachment *depth_attachment;
	uint32_t width, height;
} RendRenderDesc;
typedef struct RendViewport
{
	float x, y, width, height, min_depth, max_depth;
} RendViewport;
typedef struct RendScissor
{
	int32_t x, y;
	uint32_t width, height;
} RendScissor;
typedef enum RendCompareOp
{
	REND_COMPARE_NONE = 0,
	REND_COMPARE_LESS,
	REND_COMPARE_ALWAYS
} RendCompareOp;
typedef struct RendDepthState
{
	uint32_t test_enable, write_enable;
	RendCompareOp compare;
} RendDepthState;
typedef struct RendBarrier
{
	uint32_t producer_stages, producer_access, consumer_stages, consumer_access;
} RendBarrier;
enum
{
	REND_STAGE_NONE = 0,
	REND_STAGE_HOST = 1u,
	REND_STAGE_TRANSFER = 2u,
	REND_STAGE_COMPUTE = 4u,
	REND_STAGE_INDEX_INPUT = 8u,
	REND_STAGE_VERTEX = 16u,
	REND_STAGE_TASK = 32u,
	REND_STAGE_MESH = 64u,
	REND_STAGE_FRAGMENT = 128u,
	REND_STAGE_EARLY_DEPTH_STENCIL = 256u,
	REND_STAGE_LATE_DEPTH_STENCIL = 512u,
	REND_STAGE_COLOR_OUTPUT = 1024u,
	REND_STAGE_INDIRECT = 2048u,
	REND_STAGE_GRAPHICS = 4096u
};
enum
{
	REND_ACCESS_NONE = 0,
	REND_ACCESS_READ = 1u,
	REND_ACCESS_WRITE = 2u
};
typedef struct RendTimelinePoint
{
	RendTimeline timeline;
	uint64_t value;
} RendTimelinePoint;
typedef struct RendTimelineWait
{
	RendTimelinePoint point;
	uint32_t consumer_stages;
} RendTimelineWait;
typedef struct RendSubmitDesc
{
	const RendCommandList *commands;
	uint32_t command_count;
	const RendTimelineWait *waits;
	uint32_t wait_count;
	RendTimelinePoint completion;
} RendSubmitDesc;

int rend_discovery_memory_requirements(RendMemoryRequirements *out);
int rend_discovery_create(void *storage, size_t storage_size,
			  RendQueueFamilyInfo *families,
			  uint32_t family_capacity, RendDiscovery **out);
int rend_discovery_create_with_extensions(void *storage, size_t storage_size,
					  RendQueueFamilyInfo *families,
					  uint32_t family_capacity,
					  const char *const *extensions,
					  uint32_t extension_count,
					  RendDiscovery **out);
int rend_discovery_destroy(RendDiscovery *discovery);
uint32_t rend_discovery_device_count(const RendDiscovery *discovery);
const RendDeviceInfo *rend_discovery_device(const RendDiscovery *discovery,
					    uint32_t index);
const RendQueueFamilyInfo *
rend_device_queue_families(const RendDeviceInfo *device, uint32_t *out_count);
RendBackendKind rend_device_backend(const RendDeviceInfo *device);
const char *rend_device_name(const RendDeviceInfo *device);
int rend_device_supports_profile(const RendDeviceInfo *device,
				 RendProfile profile);
int rend_last_diagnostic(RendDiagnostic *out_diagnostic);
int rend_memory_requirements(const RendParams *params,
			     RendMemoryRequirements *out);
int rend_place_in_memory(void *memory, size_t memory_size,
			 const RendParams *params, RendBackend *out_backend);
/* wait_idle is the explicit terminal-error/recreation drain; no call waits
 * implicitly. Reset command pools and destroy dependent objects before backend. */
int rend_backend_destroy(RendBackend backend);
int rend_backend_wait_idle(RendBackend backend);
int rend_backend_get_limits(RendBackend backend, RendDeviceLimits *out);
int rend_device_surface_support(const RendDeviceInfo *device,
				uint64_t native_surface, uint32_t family_index,
				int *out_supported);
int rend_queue_get(RendBackend backend, uint32_t index, RendQueue *out);
int rend_command_pool_get(RendBackend backend, uint32_t index,
			  RendCommandPool *out);
int rend_heap_classes(RendBackend backend, RendHeapKind kind,
		      RendHeapClassInfo *classes, uint32_t capacity,
		      uint32_t *out_count);
int rend_heap_memory_requirements(RendBackend backend, const RendHeapDesc *desc,
				  RendStorageRequirements *out);
int rend_heap_create(RendBackend backend, const RendHeapDesc *desc,
		     RendHeap *out);
int rend_heap_get_info(RendBackend backend, RendHeap heap, RendHeapInfo *out);
int rend_heap_flush(RendBackend backend, RendHeapRange range);
int rend_heap_invalidate(RendBackend backend, RendHeapRange range);
int rend_heap_destroy(RendBackend backend, RendHeap heap);
int rend_texture_memory_requirements(RendBackend backend,
				     const RendTextureDesc *desc,
				     uint32_t memory_class,
				     RendStorageRequirements *out);
int rend_texture_create(RendBackend backend, const RendTextureDesc *desc,
			RendHeap heap, uint64_t offset, uint64_t size,
			RendTexture *out);
int rend_texture_view_create(RendBackend backend, RendTexture texture,
			     const RendTextureViewDesc *desc,
			     RendTextureView *out);
int rend_texture_destroy(RendBackend backend, RendTexture texture);
int rend_texture_view_destroy(RendBackend backend, RendTextureView view);
int rend_graphics_pipeline_create(RendBackend backend,
				  const RendGraphicsPipelineDesc *desc,
				  RendPipeline *out);
int rend_pipeline_destroy(RendBackend backend, RendPipeline pipeline);
int rend_command_list_begin(RendCommandPool pool, uint32_t slot,
			    RendCommandList *out);
int rend_command_list_end(RendCommandList commands);
int rend_command_pool_reset(RendCommandPool pool);
int rend_cmd_barrier(RendCommandList commands, const RendBarrier *barrier);
int rend_cmd_begin_render(RendCommandList commands, const RendRenderDesc *desc);
int rend_cmd_end_render(RendCommandList commands);
int rend_cmd_set_pipeline(RendCommandList commands, RendPipeline pipeline);
int rend_cmd_set_viewport(RendCommandList commands,
			  const RendViewport *viewport);
int rend_cmd_set_scissor(RendCommandList commands, const RendScissor *scissor);
/* Dynamic Vulkan 1.3 depth test/write/compare state. Snake subset supports
 * NONE, LESS, ALWAYS. */
int rend_cmd_set_depth_state(RendCommandList commands,
			     const RendDepthState *state);
typedef enum RendIndexType
{
	REND_INDEX_NONE = 0,
	REND_INDEX_UINT16 = 1,
	REND_INDEX_UINT32 = 2
} RendIndexType;
/* Raw root/index addresses are caller-owned and must remain valid through GPU
 * completion; Rend tracks known Rend objects but cannot trace shader pointers. */
int rend_cmd_draw_indexed(RendCommandList commands, uint64_t root_address,
			  uint64_t index_address, RendIndexType index_type,
			  uint32_t index_count, uint32_t instance_count,
			  uint32_t first_index, int32_t vertex_offset,
			  uint32_t first_instance);
/* Snake subset: RGBA8 only, tightly packed rows (width*4), width/height within
 * texture; destination offset must be 4-byte aligned and range must cover
 * width*height*4. */
int rend_cmd_copy_texture_to_data(RendCommandList commands, RendTexture texture,
				  RendHeapRange destination, uint32_t width,
				  uint32_t height);
int rend_queue_submit(RendQueue queue, const RendSubmitDesc *desc);
int rend_timeline_create(RendQueue producer, uint64_t initial_value,
			 RendTimeline *out);
int rend_timeline_query(RendTimeline timeline, uint64_t *out_value);
int rend_timeline_wait(RendTimeline timeline, uint64_t value,
		       uint64_t timeout_ns);
int rend_timeline_destroy(RendTimeline timeline);
typedef struct RendPresentationInfo
{
	uint32_t width, height;
	RendFormat format;
	uint32_t image_count;
} RendPresentationInfo;
int rend_presentation_get_info(RendPresentation presentation,
			       RendPresentationInfo *out);
int rend_presentation_create(RendBackend backend, uint64_t native_surface,
			     uint32_t width, uint32_t height,
			     RendPresentation *out);
/* Acquire never waits except for the supplied WSI timeout; no free acquire
 * semaphore returns REND_DIAG_TIMEOUT. Drain before retirement/recreation. */
int rend_presentation_acquire(RendPresentation presentation,
			      uint64_t timeout_ns, RendTextureView *out_view);
int rend_presentation_present(RendPresentation presentation, RendQueue queue);
int rend_presentation_destroy(RendPresentation presentation);

#endif

/* Vulkan-only native bridge. Define any required VK_USE_PLATFORM_* macros
 * before including Rend2.h (or include the host platform header first). */
#if !defined(REND2_REND_VK_H) && !defined(REND2_REND_VULKAN_H)
#define REND2_REND_VK_H
#define REND2_REND_VULKAN_H
#include <vulkan/vulkan.h>

/* Surface lifetime is caller-owned and must outlive its RendPresentation. */
int rend_vk_discovery_instance(const RendDiscovery *discovery,
			       VkInstance *out_instance);
#endif

#endif /* REND2_H */

#if defined(REND2_IMPLEMENTATION) && !defined(REND2_IMPLEMENTATION_INCLUDED)
#define REND2_IMPLEMENTATION_INCLUDED

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <vulkan/vulkan.h>

/* ---- Amalgamated from Rend2/rend_internal.h ---- */
#ifndef REND2_REND_INTERNAL_H
#define REND2_REND_INTERNAL_H
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

/* ---- Amalgamated from Rend2/rend.c ---- */
/* Rend 2 bounded host-side bootstrap. Copyright (c) 2026, MIT license. */


static RendDiagnostic rend_diagnostic;

void
rend_vk_set_diagnostic(RendDiagnosticCode code, int native_code,
		       const char *message)
{
	rend_diagnostic.code = code;
	rend_diagnostic.native_code = native_code;
	if (!message)
		message = "";
	snprintf(rend_diagnostic.message, sizeof(rend_diagnostic.message), "%s",
		 message);
}

int
rend_last_diagnostic(RendDiagnostic *out)
{
	if (!out)
		return 0;
	*out = rend_diagnostic;
	return 1;
}

int
rend_discovery_memory_requirements(RendMemoryRequirements *out)
{
	if (!out)
	{
		rend_vk_set_diagnostic(REND_DIAG_INVALID_ARGUMENT, 0,
				       "null requirements output");
		return 0;
	}
	out->size = sizeof(RendDiscovery);
	out->alignment = REND_DISCOVERY_ALIGNMENT;
	rend_vk_set_diagnostic(REND_DIAG_NONE, 0, "");
	return 1;
}

uint32_t
rend_discovery_device_count(const RendDiscovery *discovery)
{
	return discovery && discovery->magic == REND_DISCOVERY_MAGIC
		       ? discovery->device_count
		       : 0;
}

const RendDeviceInfo *
rend_discovery_device(const RendDiscovery *discovery,
		      uint32_t index)
{
	if (!discovery || discovery->magic != REND_DISCOVERY_MAGIC ||
	    index >= discovery->device_count)
		return NULL;
	return &discovery->devices[index];
}

const RendQueueFamilyInfo *
rend_device_queue_families(const RendDeviceInfo *device, uint32_t *out_count)
{
	if (out_count)
		*out_count = device ? device->family_count : 0;
	return device ? device->family_records : NULL;
}

RendBackendKind
rend_device_backend(const RendDeviceInfo *device)
{
	return device ? REND_BACKEND_VULKAN : REND_BACKEND_NONE;
}

const char *
rend_device_name(const RendDeviceInfo *device)
{
	return device ? device->name : NULL;
}

static void
rend_profile_reason_add(char *message, size_t capacity,
			const char *reason)
{
	size_t used = strlen(message);
	if (used < capacity - 1)
		snprintf(message + used, capacity - used, "%s%s", used ? ", " : "",
			 reason);
}
int
rend_device_supports_profile(const RendDeviceInfo *device,
			     RendProfile profile)
{
	char message[192] = "selected Vulkan device lacks required graphics-compute capability: ";
	const size_t prefix = sizeof("selected Vulkan device lacks required graphics-compute capability: ") - 1;
	if (!device || (profile != REND_PROFILE_GRAPHICS_COMPUTE &&
			profile != REND_PROFILE_FULL))
	{
		rend_vk_set_diagnostic(REND_DIAG_INVALID_ARGUMENT, 0,
				       "invalid device/profile query");
		return 0;
	}
	if (profile == REND_PROFILE_GRAPHICS_COMPUTE && device->graphics_compute)
	{
		rend_vk_set_diagnostic(REND_DIAG_NONE, 0, "");
		return 1;
	}
	if (profile == REND_PROFILE_FULL)
	{
		rend_vk_set_diagnostic(REND_DIAG_UNSUPPORTED, 0,
				       "full task/mesh profile is not implemented");
		return 0;
	}
#define REND_REASON(mask, bit, text)                                             \
	do                                                                       \
	{                                                                        \
		if ((mask) & (bit))                                              \
			rend_profile_reason_add(message, sizeof(message), text); \
	} while (0)
	message[prefix] = '\0';
	REND_REASON(device->missing_extensions, REND_MISSING_DESCRIPTOR_HEAP,
		    "VK_EXT_descriptor_heap");
	REND_REASON(device->missing_extensions, REND_MISSING_ADDRESS_COMMANDS,
		    "VK_KHR_device_address_commands");
	REND_REASON(device->missing_extensions, REND_MISSING_UNTYPED_POINTERS,
		    "VK_KHR_shader_untyped_pointers");
	REND_REASON(device->missing_features, REND_MISSING_API_14, "Vulkan 1.4");
	REND_REASON(device->missing_features, REND_MISSING_DESCRIPTOR_HEAP_FEATURE,
		    "descriptorHeap feature");
	REND_REASON(device->missing_features, REND_MISSING_ADDRESS_COMMANDS_FEATURE,
		    "deviceAddressCommands feature");
	REND_REASON(device->missing_features, REND_MISSING_UNTYPED_POINTERS_FEATURE,
		    "shaderUntypedPointers feature");
	REND_REASON(device->missing_features, REND_MISSING_SHADER_DRAW_PARAMETERS,
		    "shaderDrawParameters");
	REND_REASON(device->missing_features, REND_MISSING_BUFFER_DEVICE_ADDRESS,
		    "bufferDeviceAddress");
	REND_REASON(device->missing_features, REND_MISSING_SCALAR_BLOCK_LAYOUT,
		    "scalarBlockLayout");
	REND_REASON(device->missing_features, REND_MISSING_TIMELINE_SEMAPHORE,
		    "timelineSemaphore");
	REND_REASON(device->missing_features, REND_MISSING_SYNCHRONIZATION2,
		    "synchronization2");
	REND_REASON(device->missing_features, REND_MISSING_DYNAMIC_RENDERING,
		    "dynamicRendering");
	REND_REASON(device->missing_features, REND_MISSING_SHADER_INT64,
		    "shaderInt64");
	REND_REASON(device->missing_features, REND_MISSING_STORAGE_IMAGE_WRITE,
		    "shaderStorageImageWriteWithoutFormat");
	REND_REASON(device->missing_features, REND_MISSING_PUSH_DATA_SIZE,
		    "maxPushDataSize >= 8");
	REND_REASON(device->missing_features, REND_MISSING_ROOT_ALIGNMENT,
		    "root uniform-address alignment fits Rend limit");
	REND_REASON(device->missing_queue_caps, REND_MISSING_GRAPHICS_COMPUTE_QUEUE,
		    "graphics+compute queue family");
#undef REND_REASON
	if (!message[prefix])
		snprintf(message, sizeof(message), "selected Vulkan device does not implement this profile");
	rend_vk_set_diagnostic(REND_DIAG_UNSUPPORTED, 0, message);
	return 0;
}

static int
rend_size_add(size_t *size, size_t count, size_t item)
{
	if (count && item > (SIZE_MAX - *size) / count) {
		rend_vk_set_diagnostic(REND_DIAG_OVERFLOW, 0, "context storage size overflow");
		return 0;
	}
	*size += count * item;
	return 1;
}

#define REND_C99_ALIGNOF(type)      \
	offsetof(                   \
		struct {            \
			char c;     \
			type value; \
		},                  \
		value)
static int
rend_size_array(size_t *size, size_t count, size_t item,
		size_t alignment)
{
	if (!count)
		return 1;
	return rend_size_add(size, 1, alignment - 1) &&
	       rend_size_add(size, count, item);
}
static int
rend_context_size(const RendParams *p, size_t *out)
{
	size_t n = sizeof(struct RendBackendState);
	uint32_t i;
	if (!p || !out || !p->device || !p->queues || p->queue_count != 1 ||
	    (!p->command_pools && p->command_pool_count) || p->sampler_count)
		return 0;
	if (p->profile != REND_PROFILE_GRAPHICS_COMPUTE)
	{
		rend_device_supports_profile(p->device, p->profile);
		return 0;
	}
	if (!rend_device_supports_profile(p->device, p->profile))
		return 0;
	if (p->presentation_count && !p->device->swapchain_extension_supported)
	{
		rend_vk_set_diagnostic(REND_DIAG_UNSUPPORTED, 0,
				       "presentation requested but VK_KHR_swapchain is unavailable");
		return 0;
	}
	if (!rend_size_array(&n, p->heap_count, sizeof(RendHeapRecord),
			     REND_C99_ALIGNOF(RendHeapRecord)) ||
	    !rend_size_array(&n, p->texture_count, sizeof(RendTextureRecord),
			     REND_C99_ALIGNOF(RendTextureRecord)) ||
	    !rend_size_array(&n, p->texture_view_count, sizeof(RendTextureViewRecord),
			     REND_C99_ALIGNOF(RendTextureViewRecord)) ||
	    !rend_size_array(&n, p->pipeline_count, sizeof(RendPipelineRecord),
			     REND_C99_ALIGNOF(RendPipelineRecord)) ||
	    !rend_size_array(&n, p->timeline_count, sizeof(RendTimelineRecord),
			     REND_C99_ALIGNOF(RendTimelineRecord)) ||
	    !rend_size_array(&n, p->queue_count, sizeof(RendQueueRecord),
			     REND_C99_ALIGNOF(RendQueueRecord)) ||
	    !rend_size_array(&n, p->command_pool_count, sizeof(RendPoolRecord),
			     REND_C99_ALIGNOF(RendPoolRecord)) ||
	    !rend_size_array(&n, p->presentation_count,
			     sizeof(RendPresentationRecord),
			     REND_C99_ALIGNOF(RendPresentationRecord)))
		return 0;
	if (!p->device->family_records ||
	    p->queues[0].family_index >= p->device->family_count ||
	    (p->device->family_records[p->queues[0].family_index].capabilities &
	     (REND_QUEUE_GRAPHICS | REND_QUEUE_COMPUTE)) !=
		    (REND_QUEUE_GRAPHICS | REND_QUEUE_COMPUTE) ||
	    !p->device->family_records[p->queues[0].family_index].queue_count)
		return 0;
	for (i = 0; i < p->queue_count; i++)
	{
		size_t wait_refs;
		if (p->queues[i].max_submit_waits &&
		    p->queues[i].submission_count >
			    SIZE_MAX / p->queues[i].max_submit_waits)
			return 0;
		wait_refs =
			(size_t)p->queues[i].submission_count * p->queues[i].max_submit_waits;
		if (!p->queues[i].submission_count ||
		    !p->queues[i].max_submit_command_lists ||
		    p->queues[i].max_submit_command_lists > 64 ||
		    p->queues[i].max_submit_waits > 63 || p->queues[i].queue_index ||
		    !rend_size_array(&n, p->queues[i].submission_count,
				     sizeof(RendQueueSubmission),
				     REND_C99_ALIGNOF(RendQueueSubmission)) ||
		    !rend_size_array(&n, wait_refs, sizeof(RendQueueWaitRef),
				     REND_C99_ALIGNOF(RendQueueWaitRef)))
			return 0;
	}
	for (i = 0; i < p->command_pool_count; i++)
	{
		const RendCommandPoolParams *q = &p->command_pools[i];
		if (!q->command_list_count ||
		    q->family_index != p->queues[0].family_index ||
		    !rend_size_array(&n, q->command_list_count, sizeof(RendCommandRecord),
				     REND_C99_ALIGNOF(RendCommandRecord)) ||
		    !rend_size_array(&n, q->scratch_bytes, 1, REND_CONTEXT_ALIGNMENT) ||
		    !rend_size_array(&n, q->metadata_bytes, 1, REND_CONTEXT_ALIGNMENT))
			return 0;
	}
	if (!rend_size_array(&n, p->scratch_bytes, 1, REND_CONTEXT_ALIGNMENT))
		return 0;

	if (n > SIZE_MAX - (REND_CONTEXT_ALIGNMENT - 1))
		return 0;
	*out =
		(n + REND_CONTEXT_ALIGNMENT - 1) & ~(size_t)(REND_CONTEXT_ALIGNMENT - 1);
	return 1;
}
#undef REND_C99_ALIGNOF

int
rend_memory_requirements(const RendParams *params,
			 RendMemoryRequirements *out)
{
	size_t bytes;
	if (out)
		memset(out, 0, sizeof(*out));
	if (!params || !out)
	{
		rend_vk_set_diagnostic(REND_DIAG_INVALID_ARGUMENT, 0,
				       "null context sizing argument");
		return 0;
	}
	rend_vk_set_diagnostic(REND_DIAG_INVALID_ARGUMENT, 0,
			       "invalid context configuration");
	if (!rend_context_size(params, &bytes)) {
		if (rend_diagnostic.code == REND_DIAG_NONE)
			rend_vk_set_diagnostic(REND_DIAG_INVALID_ARGUMENT, 0, "invalid context configuration");
		return 0;
	}
	out->size = bytes;
	out->alignment = REND_CONTEXT_ALIGNMENT;
	rend_vk_set_diagnostic(REND_DIAG_NONE, 0, "");
	return 1;
}

static void *
rend_arena_take(unsigned char **cursor, size_t *left, size_t bytes,
		size_t alignment)
{
	uintptr_t address = (uintptr_t)*cursor,
		  aligned = (address + alignment - 1) & ~(uintptr_t)(alignment - 1);
	size_t pad = (size_t)(aligned - address);
	if (pad > *left || bytes > *left - pad)
		return NULL;
	*cursor = (unsigned char *)(aligned + bytes);
	*left -= pad + bytes;
	return (void *)aligned;
}

int
rend_place_in_memory(void *memory, size_t memory_size, const RendParams *p,
		     RendBackend *out)
{
	RendMemoryRequirements req;
	struct RendBackendState *b;
	unsigned char *cursor;
	size_t left;
	uint32_t i;
	VkDeviceQueueCreateInfo qci[16];
	float priorities[16];
	uint32_t families[16], family_count = 0;
	VkDeviceCreateInfo dci;
	VkPhysicalDeviceDescriptorHeapFeaturesEXT heapf;
	VkPhysicalDeviceDeviceAddressCommandsFeaturesKHR addressf;
	VkPhysicalDeviceShaderUntypedPointersFeaturesKHR pointerf;
	VkPhysicalDeviceVulkan11Features f11;
	VkPhysicalDeviceVulkan12Features f12;
	VkPhysicalDeviceVulkan13Features f13;
	VkPhysicalDeviceFeatures2 f2;
	const char *exts[] = {VK_EXT_DESCRIPTOR_HEAP_EXTENSION_NAME,
			      VK_KHR_DEVICE_ADDRESS_COMMANDS_EXTENSION_NAME,
			      VK_KHR_SHADER_UNTYPED_POINTERS_EXTENSION_NAME,
			      VK_KHR_SWAPCHAIN_EXTENSION_NAME};
	if (out)
		*out = NULL;
	if (!out || !memory)
	{
		rend_vk_set_diagnostic(REND_DIAG_INVALID_ARGUMENT, 0,
				       "null context backing/output");
		return 0;
	}
	if (!rend_memory_requirements(p, &req))
		return 0;
	if (memory_size < req.size || (uintptr_t)memory % req.alignment)
	{
		rend_vk_set_diagnostic(REND_DIAG_INVALID_ARGUMENT, 0,
				       "context backing is undersized or misaligned");
		return 0;
	}
	memset(memory, 0, req.size);
	cursor = memory;
	left = memory_size;
	b = rend_arena_take(&cursor, &left, sizeof(*b),
			    offsetof(
				    struct {
					    char c;
					    struct RendBackendState value;
				    },
				    value));
	if (!b)
	{
		rend_vk_set_diagnostic(REND_DIAG_CAPACITY, 0, "context backing exhausted");
		return 0;
	}
	b->backing = memory;
	b->backing_size = memory_size;
	b->device_info = p->device;
	b->physical = p->device->physical;
	b->memory = p->device->memory;
	b->heap_properties = p->device->heap_properties;
	b->atom = p->device->properties.limits.nonCoherentAtomSize;
	b->root_alignment =
		(uint32_t)(p->device->properties.limits.minUniformBufferOffsetAlignment >
					   16
				   ? p->device->properties.limits
					     .minUniformBufferOffsetAlignment
				   : 16);
	b->heap_count = p->heap_count;
	b->texture_count = p->texture_count;
	b->view_count = p->texture_view_count;
	b->pipeline_count = p->pipeline_count;
	b->timeline_count = p->timeline_count;
	b->queue_count = p->queue_count;
	b->pool_count = p->command_pool_count;
	b->presentation_count = p->presentation_count;
#define REND_ALIGNOF(type)          \
	offsetof(                   \
		struct {            \
			char c;     \
			type value; \
		},                  \
		value)
#define TAKE(field, count, type)                                                          \
	do                                                                                \
	{                                                                                 \
		if (count && !(b->field = rend_arena_take(&cursor, &left,                 \
							  (size_t)(count) * sizeof(type), \
							  REND_ALIGNOF(type))))           \
			goto oom;                                                         \
	} while (0)
	TAKE(heaps, p->heap_count, RendHeapRecord);
	TAKE(textures, p->texture_count, RendTextureRecord);
	TAKE(views, p->texture_view_count, RendTextureViewRecord);
	TAKE(pipelines, p->pipeline_count, RendPipelineRecord);
	TAKE(timelines, p->timeline_count, RendTimelineRecord);
	TAKE(queues, p->queue_count, RendQueueRecord);
	TAKE(pools, p->command_pool_count, RendPoolRecord);
	TAKE(presentations, p->presentation_count, RendPresentationRecord);
	for (i = 0; i < p->queue_count; i++)
	{
		b->queues[i].submission_capacity = p->queues[i].submission_count;
		b->queues[i].submissions = rend_arena_take(
			&cursor, &left,
			(size_t)p->queues[i].submission_count * sizeof(RendQueueSubmission),
			REND_ALIGNOF(RendQueueSubmission));
		if (!b->queues[i].submissions)
			goto oom;
		if (p->queues[i].max_submit_waits)
		{
			size_t wait_count =
				(size_t)p->queues[i].submission_count * p->queues[i].max_submit_waits;
			b->queues[i].wait_refs =
				rend_arena_take(&cursor, &left, wait_count * sizeof(RendQueueWaitRef),
						REND_ALIGNOF(RendQueueWaitRef));
			if (!b->queues[i].wait_refs)
				goto oom;
		}
	}
	for (i = 0; i < p->command_pool_count; i++)
	{
		b->pools[i].commands =
			rend_arena_take(&cursor, &left,
					(size_t)p->command_pools[i].command_list_count *
						sizeof(RendCommandRecord),
					REND_ALIGNOF(RendCommandRecord));
		if (!b->pools[i].commands)
			goto oom;
		b->pools[i].scratch_bytes = p->command_pools[i].scratch_bytes;
		b->pools[i].metadata_bytes = p->command_pools[i].metadata_bytes;
		if (b->pools[i].scratch_bytes &&
		    !(b->pools[i].scratch = rend_arena_take(&cursor, &left, b->pools[i].scratch_bytes, REND_CONTEXT_ALIGNMENT)))
			goto oom;
		if (b->pools[i].metadata_bytes &&
		    !(b->pools[i].metadata = rend_arena_take(&cursor, &left, b->pools[i].metadata_bytes, REND_CONTEXT_ALIGNMENT)))
			goto oom;
	}
	b->scratch_bytes = p->scratch_bytes;
	if (b->scratch_bytes && !(b->scratch = rend_arena_take(&cursor, &left, b->scratch_bytes, REND_CONTEXT_ALIGNMENT)))
		goto oom;
#undef TAKE
#undef REND_ALIGNOF
	for (i = 0; i < p->queue_count; i++)
	{
		uint32_t j, found = 0;
		for (j = 0; j < i; j++)
			if (p->queues[j].family_index == p->queues[i].family_index &&
			    p->queues[j].queue_index == p->queues[i].queue_index)
				goto unsupported;
		for (j = 0; j < family_count; j++)
			found |= families[j] == p->queues[i].family_index;
		if (!found)
			families[family_count++] = p->queues[i].family_index;
		priorities[i] = 1.0f;
		memset(&qci[i], 0, sizeof(qci[i]));
		qci[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		qci[i].queueFamilyIndex = p->queues[i].family_index;
		qci[i].queueCount = p->queues[i].queue_index + 1;
		qci[i].pQueuePriorities = &priorities[i];
		b->queues[i].backend = b;
		b->queues[i].family = p->queues[i].family_index;
		b->queues[i].index = p->queues[i].queue_index;
		b->queues[i].max_submit_lists = p->queues[i].max_submit_command_lists;
		b->queues[i].max_waits = p->queues[i].max_submit_waits;
	}
	/* One queue-create record per distinct family; reject multiple queues in one
	 * family in this minimal backend. */
	for (i = 0; i < p->queue_count; i++)
		for (uint32_t j = 0; j < i; j++)
			if (p->queues[i].family_index == p->queues[j].family_index)
				goto unsupported;
	memset(&heapf, 0, sizeof(heapf));
	heapf.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_FEATURES_EXT;
	heapf.descriptorHeap = VK_TRUE;
	memset(&addressf, 0, sizeof(addressf));
	addressf.sType =
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_ADDRESS_COMMANDS_FEATURES_KHR;
	addressf.pNext = &heapf;
	addressf.deviceAddressCommands = VK_TRUE;
	memset(&pointerf, 0, sizeof(pointerf));
	pointerf.sType =
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_UNTYPED_POINTERS_FEATURES_KHR;
	pointerf.pNext = &addressf;
	pointerf.shaderUntypedPointers = VK_TRUE;
	memset(&f11, 0, sizeof(f11));
	f11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
	f11.pNext = &pointerf;
	f11.shaderDrawParameters = VK_TRUE;
	memset(&f12, 0, sizeof(f12));
	f12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	f12.pNext = &f11;
	f12.bufferDeviceAddress = VK_TRUE;
	f12.scalarBlockLayout = VK_TRUE;
	f12.timelineSemaphore = VK_TRUE;
	memset(&f13, 0, sizeof(f13));
	f13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	f13.pNext = &f12;
	f13.synchronization2 = VK_TRUE;
	f13.dynamicRendering = VK_TRUE;
	memset(&f2, 0, sizeof(f2));
	f2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	f2.pNext = &f13;
	f2.features.shaderInt64 = VK_TRUE;
	f2.features.shaderStorageImageWriteWithoutFormat = VK_TRUE;
	memset(&dci, 0, sizeof(dci));
	dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	dci.pNext = &f2;
	dci.queueCreateInfoCount = p->queue_count;
	dci.pQueueCreateInfos = qci;
	dci.enabledExtensionCount = p->presentation_count ? 4 : 3;
	dci.ppEnabledExtensionNames = exts;
	{
		VkResult r = vkCreateDevice(b->physical, &dci, NULL, &b->device);
		if (r != VK_SUCCESS)
		{
			rend_vk_set_diagnostic(REND_DIAG_NATIVE, r,
					       "vkCreateDevice with descriptor "
					       "heap/address/untyped-pointer features failed");
			return 0;
		}
	}
#define LOAD(field, name)                                             \
	b->field = (PFN_##name)vkGetDeviceProcAddr(b->device, #name); \
	if (!b->field)                                                \
	goto device_fail
	LOAD(write_resources, vkWriteResourceDescriptorsEXT);
	LOAD(write_samplers, vkWriteSamplerDescriptorsEXT);
	LOAD(bind_resources, vkCmdBindResourceHeapEXT);
	LOAD(bind_samplers, vkCmdBindSamplerHeapEXT);
	LOAD(push_data, vkCmdPushDataEXT);
	LOAD(copy_to_image, vkCmdCopyMemoryToImageKHR);
	LOAD(copy_from_image, vkCmdCopyImageToMemoryKHR);
	LOAD(bind_index, vkCmdBindIndexBuffer3KHR);
#undef LOAD
	for (i = 0; i < p->queue_count; i++)
	{
		vkGetDeviceQueue(b->device, b->queues[i].family, b->queues[i].index,
				 &b->queues[i].queue);
		b->queues[i].live = 1;
	}
	for (i = 0; i < p->command_pool_count; i++)
	{
		VkCommandPoolCreateInfo ci;
		uint32_t j;
		b->pools[i].backend = b;
		b->pools[i].family = p->command_pools[i].family_index;
		b->pools[i].count = p->command_pools[i].command_list_count;
		memset(&ci, 0, sizeof(ci));
		ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		ci.queueFamilyIndex = b->pools[i].family;
		if (vkCreateCommandPool(b->device, &ci, NULL, &b->pools[i].pool) !=
		    VK_SUCCESS)
			goto device_fail;
		for (j = 0; j < b->pools[i].count; j++)
		{
			VkCommandBufferAllocateInfo ai;
			memset(&ai, 0, sizeof(ai));
			ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			ai.commandPool = b->pools[i].pool;
			ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			ai.commandBufferCount = 1;
			if (vkAllocateCommandBuffers(
				    b->device, &ai, &b->pools[i].commands[j].command) != VK_SUCCESS)
				goto device_fail;
			b->pools[i].commands[j].backend = b;
			b->pools[i].commands[j].pool = &b->pools[i];
		}
		b->pools[i].live = 1;
	}
	b->magic = REND_BACKEND_MAGIC;
	*out = b;
	if (p->device->owner)
		p->device->owner->context_count++;
	rend_vk_set_diagnostic(REND_DIAG_NONE, 0, "");
	return 1;
device_fail:
	for (i = 0; i < p->command_pool_count; i++)
		if (b->pools[i].pool)
			vkDestroyCommandPool(b->device, b->pools[i].pool, NULL);
	vkDestroyDevice(b->device, NULL);
	b->device = VK_NULL_HANDLE;
	rend_vk_set_diagnostic(
		REND_DIAG_NATIVE, 0,
		"native device entrypoint, queue, or command-pool creation failed");
	return 0;
unsupported:
	rend_vk_set_diagnostic(
		REND_DIAG_UNSUPPORTED, 0,
		"duplicate family, queue policy, or unsupported queue submission limits");
	return 0;
oom:
	rend_vk_set_diagnostic(REND_DIAG_CAPACITY, 0,
			       "context caller backing exhausted");
	return 0;
}

/* ---- Amalgamated from Rend2/rend_backend_vulkan.c ---- */
/* Vulkan device and queue-family discovery only. */


int
rend_discovery_create(void *storage, size_t storage_size,
		      RendQueueFamilyInfo *families,
		      uint32_t family_capacity, RendDiscovery **out)
{
	return rend_discovery_create_with_extensions(storage, storage_size, families,
						     family_capacity, NULL, 0, out);
}

int
rend_discovery_create_with_extensions(void *storage, size_t storage_size,
				      RendQueueFamilyInfo *families,
				      uint32_t family_capacity,
				      const char *const *extensions,
				      uint32_t extension_count,
				      RendDiscovery **out)
{
	RendDiscovery *d;
	VkApplicationInfo app;
	VkInstanceCreateInfo ici;
	VkPhysicalDevice physical[REND_DISCOVERY_DEVICE_CAPACITY];
	uint32_t count = 0, total_families = 0, i;
	VkResult result;

	if (out)
		*out = NULL;
	if (!storage || !out || storage_size < sizeof(RendDiscovery) ||
	    (uintptr_t)storage % REND_DISCOVERY_ALIGNMENT ||
	    (!families && family_capacity) || extension_count > 16 ||
	    (!extensions && extension_count))
	{
		rend_vk_set_diagnostic(
			REND_DIAG_INVALID_ARGUMENT, 0,
			"invalid or insufficient discovery backing / output");
		return 0;
	}

	d = (RendDiscovery *)storage;
	memset(d, 0, sizeof(*d));
	memset(&app, 0, sizeof(app));
	app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app.pApplicationName = "Rend2";
	app.apiVersion = VK_API_VERSION_1_4;
	memset(&ici, 0, sizeof(ici));
	ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	ici.pApplicationInfo = &app;
	ici.enabledExtensionCount = extension_count;
	ici.ppEnabledExtensionNames = extensions;
	result = vkCreateInstance(&ici, NULL, &d->instance);
	if (result != VK_SUCCESS)
	{
		d->instance = VK_NULL_HANDLE;
		memset(d, 0, sizeof(*d));
		rend_vk_set_diagnostic(REND_DIAG_NATIVE, result,
				       "vkCreateInstance Vulkan 1.4 failed");
		return 0;
	}

	result = vkEnumeratePhysicalDevices(d->instance, &count, NULL);
	if (result != VK_SUCCESS || count > REND_DISCOVERY_DEVICE_CAPACITY)
	{
		vkDestroyInstance(d->instance, NULL);
		memset(d, 0, sizeof(*d));
		rend_vk_set_diagnostic(
			result == VK_SUCCESS ? REND_DIAG_CAPACITY : REND_DIAG_NATIVE, result,
			"physical device count unavailable or exceeds fixed discovery "
			"capacity");
		return 0;
	}
	if (count && (result = vkEnumeratePhysicalDevices(d->instance, &count,
							  physical)) != VK_SUCCESS)
	{
		vkDestroyInstance(d->instance, NULL);
		memset(d, 0, sizeof(*d));
		rend_vk_set_diagnostic(REND_DIAG_NATIVE, result,
				       "physical device enumeration failed");
		return 0;
	}

	for (i = 0; i < count; i++)
	{
		VkPhysicalDeviceProperties properties;
		VkQueueFamilyProperties queue_properties[REND_QUEUE_FAMILY_CAPACITY];
		uint32_t queue_count = 0, j, graphics_compute_queue = 0;
		struct RendDeviceInfo *device = &d->devices[i];

		vkGetPhysicalDeviceProperties(physical[i], &properties);
		device->properties = properties;
		vkGetPhysicalDeviceMemoryProperties(physical[i], &device->memory);
		device->heap_properties.sType =
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_PROPERTIES_EXT;
		device->physical = physical[i];
		device->api_version = properties.apiVersion;
		snprintf(device->name, sizeof(device->name), "%s", properties.deviceName);
		{
			VkExtensionProperties available[256];
			uint32_t n = 0, k;
			int descriptor_heap = 0, address_commands = 0, untyped = 0;
			VkPhysicalDeviceDescriptorHeapFeaturesEXT heap = {
				.sType =
					VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_FEATURES_EXT};
			VkPhysicalDeviceDeviceAddressCommandsFeaturesKHR address = {
				.sType =
					VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_ADDRESS_COMMANDS_FEATURES_KHR,
				.pNext = &heap};
			VkPhysicalDeviceShaderUntypedPointersFeaturesKHR pointers = {
				.sType =
					VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_UNTYPED_POINTERS_FEATURES_KHR,
				.pNext = &address};
			VkPhysicalDeviceVulkan11Features f11 = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
				.pNext = &pointers};
			VkPhysicalDeviceVulkan12Features f12 = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
				.pNext = &f11};
			VkPhysicalDeviceVulkan13Features f13 = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
				.pNext = &f12};
			VkPhysicalDeviceFeatures2 f2 = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &f13};
			if (vkEnumerateDeviceExtensionProperties(physical[i], NULL, &n, NULL) ==
				    VK_SUCCESS &&
			    n <= 256 &&
			    vkEnumerateDeviceExtensionProperties(physical[i], NULL, &n,
								 available) == VK_SUCCESS)
			{
				for (k = 0; k < n; k++)
				{
					descriptor_heap |= !strcmp(available[k].extensionName,
								   VK_EXT_DESCRIPTOR_HEAP_EXTENSION_NAME);
					address_commands |=
						!strcmp(available[k].extensionName,
							VK_KHR_DEVICE_ADDRESS_COMMANDS_EXTENSION_NAME);
					untyped |= !strcmp(available[k].extensionName,
							   VK_KHR_SHADER_UNTYPED_POINTERS_EXTENSION_NAME);
					device->swapchain_extension_supported |=
						!strcmp(available[k].extensionName,
							VK_KHR_SWAPCHAIN_EXTENSION_NAME);
				}
				if (!descriptor_heap)
					device->missing_extensions |= REND_MISSING_DESCRIPTOR_HEAP;
				if (!address_commands)
					device->missing_extensions |= REND_MISSING_ADDRESS_COMMANDS;
				if (!untyped)
					device->missing_extensions |= REND_MISSING_UNTYPED_POINTERS;
				if (properties.apiVersion < VK_API_VERSION_1_4)
					device->missing_features |= REND_MISSING_API_14;
				if (descriptor_heap && address_commands && untyped)
				{
					VkPhysicalDeviceProperties2 props2 = {
						.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
						.pNext = &device->heap_properties};
					vkGetPhysicalDeviceProperties2(physical[i], &props2);
					vkGetPhysicalDeviceFeatures2(physical[i], &f2);
					if (!heap.descriptorHeap)
						device->missing_features |= REND_MISSING_DESCRIPTOR_HEAP_FEATURE;
					if (!address.deviceAddressCommands)
						device->missing_features |= REND_MISSING_ADDRESS_COMMANDS_FEATURE;
					if (!pointers.shaderUntypedPointers)
						device->missing_features |= REND_MISSING_UNTYPED_POINTERS_FEATURE;
					if (properties.limits.minUniformBufferOffsetAlignment > UINT32_MAX)
						device->missing_features |= REND_MISSING_ROOT_ALIGNMENT;
					if (!f11.shaderDrawParameters)
						device->missing_features |= REND_MISSING_SHADER_DRAW_PARAMETERS;
					if (!f12.bufferDeviceAddress)
						device->missing_features |= REND_MISSING_BUFFER_DEVICE_ADDRESS;
					if (!f12.scalarBlockLayout)
						device->missing_features |= REND_MISSING_SCALAR_BLOCK_LAYOUT;
					if (!f12.timelineSemaphore)
						device->missing_features |= REND_MISSING_TIMELINE_SEMAPHORE;
					if (!f13.synchronization2)
						device->missing_features |= REND_MISSING_SYNCHRONIZATION2;
					if (!f13.dynamicRendering)
						device->missing_features |= REND_MISSING_DYNAMIC_RENDERING;
					if (!f2.features.shaderInt64)
						device->missing_features |= REND_MISSING_SHADER_INT64;
					if (!f2.features.shaderStorageImageWriteWithoutFormat)
						device->missing_features |= REND_MISSING_STORAGE_IMAGE_WRITE;
					if (device->heap_properties.maxPushDataSize < sizeof(uint64_t))
						device->missing_features |= REND_MISSING_PUSH_DATA_SIZE;
					device->graphics_compute = !device->missing_extensions &&
								   !device->missing_features;
				}
				device->full_profile = 0;
			}
		}
		vkGetPhysicalDeviceQueueFamilyProperties(physical[i], &queue_count, NULL);
		if (queue_count > REND_QUEUE_FAMILY_CAPACITY)
		{
			vkDestroyInstance(d->instance, NULL);
			memset(d, 0, sizeof(*d));
			rend_vk_set_diagnostic(
				REND_DIAG_CAPACITY, 0,
				"device exceeds fixed 128 queue-family discovery limit");
			return 0;
		}
		vkGetPhysicalDeviceQueueFamilyProperties(physical[i], &queue_count,
							 queue_properties);
		if (queue_count > UINT32_MAX - total_families)
		{
			vkDestroyInstance(d->instance, NULL);
			memset(d, 0, sizeof(*d));
			rend_vk_set_diagnostic(REND_DIAG_OVERFLOW, 0,
					       "queue family count overflow");
			return 0;
		}
		device->family_offset = total_families;
		device->family_count = queue_count;
		total_families += queue_count;
		if (family_capacity < total_families || (!families && total_families))
		{
			vkDestroyInstance(d->instance, NULL);
			memset(d, 0, sizeof(*d));
			rend_vk_set_diagnostic(
				REND_DIAG_CAPACITY, 0,
				"family output capacity too small; no discovery object published");
			return 0;
		}
		device->family_records =
			queue_count && families ? families + device->family_offset : NULL;
		for (j = 0; j < queue_count; j++)
		{
			uint32_t capabilities = 0;
			if (queue_properties[j].queueFlags & VK_QUEUE_GRAPHICS_BIT)
				capabilities |= REND_QUEUE_GRAPHICS;
			if (queue_properties[j].queueFlags & VK_QUEUE_COMPUTE_BIT)
				capabilities |= REND_QUEUE_COMPUTE;
			if (queue_properties[j].queueFlags & VK_QUEUE_TRANSFER_BIT)
				capabilities |= REND_QUEUE_TRANSFER;
			if (queue_properties[j].queueCount &&
			    (capabilities & (REND_QUEUE_GRAPHICS | REND_QUEUE_COMPUTE)) ==
				    (REND_QUEUE_GRAPHICS | REND_QUEUE_COMPUTE))
				graphics_compute_queue = 1;
			families[device->family_offset + j].family_index = j;
			families[device->family_offset + j].queue_count =
				queue_properties[j].queueCount;
			families[device->family_offset + j].capabilities = capabilities;
		}
		device->graphics_compute &= graphics_compute_queue;
		if (!graphics_compute_queue)
			device->missing_queue_caps |= REND_MISSING_GRAPHICS_COMPUTE_QUEUE;
	}

	d->families = families;
	d->family_capacity = family_capacity;
	d->family_count = total_families;
	d->device_count = count;
	for (i = 0; i < count; i++)
		d->devices[i].owner = d;
	d->magic = REND_DISCOVERY_MAGIC;
	*out = d;
	rend_vk_set_diagnostic(REND_DIAG_NONE, 0, "");
	return 1;
}

int
rend_vk_surface_support(const RendDeviceInfo *device, uint64_t surface,
			uint32_t family, int *supported)
{
	VkSurfaceKHR native;
	VkBool32 result = VK_FALSE;
	if (supported)
		*supported = 0;
	if (!device || !surface || !supported || family >= device->family_count)
	{
		rend_vk_set_diagnostic(REND_DIAG_INVALID_ARGUMENT, 0,
				       "invalid surface-support query");
		return 0;
	}
	memcpy(&native, &surface, sizeof(native));
	if (vkGetPhysicalDeviceSurfaceSupportKHR(device->physical, family, native,
						 &result) != VK_SUCCESS)
	{
		rend_vk_set_diagnostic(REND_DIAG_NATIVE, 0,
				       "vkGetPhysicalDeviceSurfaceSupportKHR failed");
		return 0;
	}
	*supported = result != VK_FALSE;
	return 1;
}

int
rend_vk_discovery_instance(const RendDiscovery *d,
			   VkInstance *out_instance)
{
	if (out_instance)
		*out_instance = VK_NULL_HANDLE;
	if (!d || d->magic != REND_DISCOVERY_MAGIC || !out_instance)
	{
		rend_vk_set_diagnostic(REND_DIAG_INVALID_ARGUMENT, 0,
				       "invalid discovery instance query");
		return 0;
	}
	*out_instance = d->instance;
	rend_vk_set_diagnostic(REND_DIAG_NONE, 0, "");
	return 1;
}

int
rend_discovery_destroy(RendDiscovery *d)
{
	if (!d || d->magic != REND_DISCOVERY_MAGIC)
	{
		rend_vk_set_diagnostic(REND_DIAG_INVALID_ARGUMENT, 0,
				       "invalid discovery object");
		return 0;
	}
	if (d->context_count)
	{
		rend_vk_set_diagnostic(REND_DIAG_INVALID_ARGUMENT, 0,
				       "discovery still owns live device contexts");
		return 0;
	}
	d->magic = 0;
	vkDestroyInstance(d->instance, NULL);
	d->instance = VK_NULL_HANDLE;
	d->families = NULL;
	d->device_count = 0;
	d->family_count = 0;
	rend_vk_set_diagnostic(REND_DIAG_NONE, 0, "");
	return 1;
}

/* ---- Amalgamated from Rend2/rend_memory.c ---- */
/* Explicit native GPU heaps; no per-range host records. */

static uint32_t
rend_mem_properties(VkMemoryPropertyFlags flags)
{
	uint32_t p = 0;
	if (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
		p |= REND_MEMORY_DEVICE_LOCAL;
	if (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
		p |= REND_MEMORY_HOST_VISIBLE;
	if (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
		p |= REND_MEMORY_HOST_COHERENT;
	if (flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
		p |= REND_MEMORY_HOST_CACHED;
	return p;
}
static int
rend_mem_handle(RendBackend b, RendHeap h)
{
	return b && b->magic == REND_BACKEND_MAGIC && h && h->backend == b && h->live;
}

int
rend_backend_get_limits(RendBackend b, RendDeviceLimits *out)
{
	if (!b || b->magic != REND_BACKEND_MAGIC || !out)
	{
		rend_vk_set_diagnostic(REND_DIAG_INVALID_ARGUMENT, 0,
				       "invalid backend limits query");
		return 0;
	}
	out->root_alignment = b->root_alignment;
	out->max_push_bytes = b->heap_properties.maxPushDataSize;
	out->cache_atom_size = b->atom;
	return 1;
}

int
rend_heap_classes(RendBackend b, RendHeapKind kind,
		  RendHeapClassInfo *classes, uint32_t capacity,
		  uint32_t *out_count)
{
	uint32_t i, n = 0;
	if (out_count)
		*out_count = 0;
	if (!b || b->magic != REND_BACKEND_MAGIC || !out_count ||
	    kind < REND_HEAP_DATA || kind > REND_HEAP_SAMPLER_DESCRIPTORS ||
	    (!classes && capacity))
	{
		rend_vk_set_diagnostic(REND_DIAG_INVALID_ARGUMENT, 0,
				       "invalid heap-class query");
		return 0;
	}
	if (kind > REND_HEAP_TEXTURE_STORAGE)
	{
		rend_vk_set_diagnostic(
			REND_DIAG_UNSUPPORTED, 0,
			"descriptor heaps are excluded from the snake subset");
		return 0;
	}
	n = b->memory.memoryTypeCount;
	*out_count = n;
	if (!classes && !capacity)
		return 1;
	if (capacity < n)
	{
		rend_vk_set_diagnostic(REND_DIAG_CAPACITY, 0,
				       "heap-class output capacity too small");
		return 0;
	}
	n = 0;
	for (i = 0; i < b->memory.memoryTypeCount; i++)
	{
		VkMemoryPropertyFlags f = b->memory.memoryTypes[i].propertyFlags;
		uint32_t p = 0;
		if (f & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
			p |= REND_MEMORY_DEVICE_LOCAL;
		if (f & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
			p |= REND_MEMORY_HOST_VISIBLE;
		if (f & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
			p |= REND_MEMORY_HOST_COHERENT;
		if (f & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
			p |= REND_MEMORY_HOST_CACHED;
		classes[n].id = i + 1;
		classes[n].memory_properties = p;
		n++;
	}
	return 1;
}

int
rend_heap_memory_requirements(RendBackend b, const RendHeapDesc *d,
			      RendStorageRequirements *out)
{
	VkBuffer buffer = VK_NULL_HANDLE;
	VkBufferCreateInfo create;
	VkMemoryRequirements native;
	VkResult result;
	if (out)
		memset(out, 0, sizeof(*out));
	if (!b || b->magic != REND_BACKEND_MAGIC || !d || !out)
	{
		rend_vk_set_diagnostic(REND_DIAG_INVALID_ARGUMENT, 0,
				       "invalid heap requirements");
		return 0;
	}
	if (d->kind == REND_HEAP_TEXTURE_DESCRIPTORS ||
	    d->kind == REND_HEAP_SAMPLER_DESCRIPTORS)
	{
		rend_vk_set_diagnostic(REND_DIAG_UNSUPPORTED, 0,
				       "descriptor heaps are excluded from snake subset");
		return 0;
	}
	if ((d->kind != REND_HEAP_DATA && d->kind != REND_HEAP_TEXTURE_STORAGE) ||
	    !d->memory_class || d->memory_class > b->memory.memoryTypeCount ||
	    !d->size || d->descriptor_count || d->size == UINT64_MAX)
	{
		rend_vk_set_diagnostic(REND_DIAG_INVALID_ARGUMENT, 0,
				       "malformed/overflowing heap description");
		return 0;
	}
	if (d->kind == REND_HEAP_TEXTURE_STORAGE)
	{
		out->size = d->size;
		out->alignment = 1;
		return 1;
	}
	memset(&create, 0, sizeof(create));
	create.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	create.size = d->size;
	create.usage =
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	create.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	result = vkCreateBuffer(b->device, &create, NULL, &buffer);
	if (result != VK_SUCCESS)
	{
		rend_vk_set_diagnostic(REND_DIAG_NATIVE, result,
				       "VkBuffer native heap-requirements query failed");
		return 0;
	}
	vkGetBufferMemoryRequirements(b->device, buffer, &native);
	vkDestroyBuffer(b->device, buffer, NULL);
	if (!(native.memoryTypeBits & (1u << (d->memory_class - 1))))
	{
		rend_vk_set_diagnostic(
			REND_DIAG_UNSUPPORTED, 0,
			"selected class incompatible with VkBuffer memoryTypeBits");
		return 0;
	}
	out->size = native.size;
	out->alignment = native.alignment;
	return 1;
}

int
rend_heap_create(RendBackend b, const RendHeapDesc *d, RendHeap *out)
{
	uint32_t i, type;
	RendHeapRecord *h = NULL;
	VkBufferCreateInfo bi;
	VkMemoryRequirements mr;
	VkMemoryAllocateInfo ai;
	VkMemoryAllocateFlagsInfo flags;
	VkResult r = VK_SUCCESS;
	RendStorageRequirements req;
	VkBufferDeviceAddressInfo bai;
	if (out)
		*out = NULL;
	if (!out || !b || b->magic != REND_BACKEND_MAGIC || !d ||
	    !rend_heap_memory_requirements(b, d, &req))
		goto invalid;
	for (i = 0; i < b->heap_count; i++)
		if (!b->heaps[i].live)
		{
			h = &b->heaps[i];
			break;
		}
	if (!h)
	{
		rend_vk_set_diagnostic(REND_DIAG_CAPACITY, 0,
				       "heap record capacity exhausted");
		return 0;
	}
	type = d->memory_class - 1;
	memset(h, 0, sizeof(*h));
	if (d->kind == REND_HEAP_TEXTURE_STORAGE)
	{
		memset(&ai, 0, sizeof(ai));
		ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		ai.allocationSize = d->size;
		ai.memoryTypeIndex = type;
		r = vkAllocateMemory(b->device, &ai, NULL, &h->memory);
		if (r != VK_SUCCESS)
			goto native;
		h->backend = b;
		h->kind = d->kind;
		h->memory_class = d->memory_class;
		h->memory_type = type;
		h->size = d->size;
		h->allocated = d->size;
		h->cache_atom = b->atom;
		h->properties = rend_mem_properties(b->memory.memoryTypes[type].propertyFlags);
		h->live = 1;
		*out = h;
		return 1;
	}
	memset(&bi, 0, sizeof(bi));
	bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bi.size = d->size;
	bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if (d->kind == REND_HEAP_DATA)
		bi.usage =
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	else
	{
		rend_vk_set_diagnostic(REND_DIAG_UNSUPPORTED, 0,
				       "texture storage is placed as native images, not a "
				       "linear buffer heap in snake subset");
		return 0;
	}
	r = vkCreateBuffer(b->device, &bi, NULL, &h->buffer);
	if (r != VK_SUCCESS)
		goto native;
	vkGetBufferMemoryRequirements(b->device, h->buffer, &mr);
	if (!(mr.memoryTypeBits & (1u << type)))
	{
		vkDestroyBuffer(b->device, h->buffer, NULL);
		h->buffer = VK_NULL_HANDLE;
		rend_vk_set_diagnostic(
			REND_DIAG_UNSUPPORTED, 0,
			"selected memory class incompatible with heap buffer");
		return 0;
	}
	memset(&flags, 0, sizeof(flags));
	flags.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
	flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
	memset(&ai, 0, sizeof(ai));
	ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	ai.pNext = d->kind == REND_HEAP_DATA ? &flags : NULL;
	ai.allocationSize = mr.size;
	ai.memoryTypeIndex = type;
	r = vkAllocateMemory(b->device, &ai, NULL, &h->memory);
	if (r != VK_SUCCESS)
		goto native;
	r = vkBindBufferMemory(b->device, h->buffer, h->memory, 0);
	if (r != VK_SUCCESS)
		goto native;
	h->backend = b;
	h->kind = d->kind;
	h->memory_class = d->memory_class;
	h->memory_type = type;
	h->properties = rend_mem_properties(b->memory.memoryTypes[type].propertyFlags);
	h->size = d->kind <= REND_HEAP_TEXTURE_STORAGE ? d->size : req.size;
	h->allocated = mr.size;
	h->cache_atom = b->atom;
	h->descriptor_count = d->descriptor_count;
	{
		VkMemoryPropertyFlags f = b->memory.memoryTypes[type].propertyFlags;
		if (f & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
		{
			r = vkMapMemory(b->device, h->memory, 0, VK_WHOLE_SIZE, 0, &h->mapped);
			if (r != VK_SUCCESS)
				goto native;
		}
	}
	if (d->kind == REND_HEAP_DATA)
	{
		memset(&bai, 0, sizeof(bai));
		bai.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		bai.buffer = h->buffer;
		h->address = vkGetBufferDeviceAddress(b->device, &bai);
		if (!h->address)
			goto native;
	}
	h->live = 1;
	*out = h;
	return 1;
native:
	if (h->mapped)
		vkUnmapMemory(b->device, h->memory);
	if (h->buffer)
		vkDestroyBuffer(b->device, h->buffer, NULL);
	if (h->memory)
		vkFreeMemory(b->device, h->memory, NULL);
	memset(h, 0, sizeof(*h));
	rend_vk_set_diagnostic(REND_DIAG_NATIVE, r,
			       "native heap buffer/memory create-bind-map failed");
	return 0;
invalid:
	rend_vk_set_diagnostic(REND_DIAG_INVALID_ARGUMENT, 0,
			       "invalid heap creation argument");
	return 0;
}

int
rend_heap_get_info(RendBackend b, RendHeap h, RendHeapInfo *out)
{
	if (!rend_mem_handle(b, h) || !out)
		return 0;
	out->kind = h->kind;
	out->memory_class = h->memory_class;
	out->size = h->size;
	out->cpu_base = h->mapped;
	out->gpu_base = h->address;
	out->cache_atom_size = h->cache_atom;
	out->memory_properties = h->properties;
	out->descriptor_count = h->descriptor_count;
	return 1;
}

static int
rend_heap_cache(RendBackend b, RendHeapRange range, int invalidate)
{
	RendHeapRecord *h = range.heap;
	uint64_t end, start, rounded;
	VkMappedMemoryRange mr;
	VkResult r;
	if (!rend_mem_handle(b, h) || !h->mapped || !range.size ||
	    range.offset > h->size || range.size > h->size - range.offset)
		return 0;
	if (h->properties & REND_MEMORY_HOST_COHERENT)
		return 1;
	start = range.offset & ~(h->cache_atom - 1);
	end = range.offset + range.size;
	if (end > UINT64_MAX - (h->cache_atom - 1))
		return 0;
	rounded = (end + h->cache_atom - 1) & ~(h->cache_atom - 1);
	if (rounded > h->allocated)
		rounded = h->allocated;
	memset(&mr, 0, sizeof(mr));
	mr.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
	mr.memory = h->memory;
	mr.offset = start;
	mr.size = rounded - start;
	r = invalidate ? vkInvalidateMappedMemoryRanges(b->device, 1, &mr)
		       : vkFlushMappedMemoryRanges(b->device, 1, &mr);
	return r == VK_SUCCESS;
}
int
rend_heap_flush(RendBackend b, RendHeapRange r)
{
	return rend_heap_cache(b, r, 0);
}
int
rend_heap_invalidate(RendBackend b, RendHeapRange r)
{
	return rend_heap_cache(b, r, 1);
}
int
rend_heap_destroy(RendBackend b, RendHeap h)
{
	if (!rend_mem_handle(b, h) || h->refs)
		return 0;
	if (h->mapped)
		vkUnmapMemory(b->device, h->memory);
	if (h->buffer)
		vkDestroyBuffer(b->device, h->buffer, NULL);
	vkFreeMemory(b->device, h->memory, NULL);
	memset(h, 0, sizeof(*h));
	return 1;
}

/* ---- Amalgamated from Rend2/rend_pipeline.c ---- */
/* Prepared Vulkan SPIR-V graphics pipeline; shader ABI tag is caller assertion,
 * not reflection. */
static VkFormat
rend_pipe_format(RendFormat f)
{
	return f == REND_FORMAT_RGBA8_UNORM   ? VK_FORMAT_R8G8B8A8_UNORM
	       : f == REND_FORMAT_BGRA8_UNORM ? VK_FORMAT_B8G8R8A8_UNORM
	       : f == REND_FORMAT_D32_FLOAT   ? VK_FORMAT_D32_SFLOAT
					      : VK_FORMAT_UNDEFINED;
}
int
rend_graphics_pipeline_create(RendBackend b,
			      const RendGraphicsPipelineDesc *d,
			      RendPipeline *out)
{
	uint32_t i;
	RendPipelineRecord *p = NULL;
	VkShaderModule modules[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
	VkShaderModuleCreateInfo sm;
	VkDescriptorSetAndBindingMappingEXT root;
	VkShaderDescriptorSetAndBindingMappingInfoEXT mapping;
	VkPipelineCreateFlags2CreateInfo flags;
	VkPipelineShaderStageCreateInfo stages[2];
	VkPipelineVertexInputStateCreateInfo vi;
	VkPipelineInputAssemblyStateCreateInfo ia;
	VkPipelineViewportStateCreateInfo vp;
	VkPipelineRasterizationStateCreateInfo rs;
	VkPipelineMultisampleStateCreateInfo ms;
	VkPipelineColorBlendAttachmentState ba;
	VkPipelineColorBlendStateCreateInfo blend;
	VkDynamicState ds[5] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,
				VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,
				VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,
				VK_DYNAMIC_STATE_DEPTH_COMPARE_OP};
	VkPipelineDepthStencilStateCreateInfo depth;
	VkPipelineDynamicStateCreateInfo dyn;
	VkPipelineRenderingCreateInfo rendering;
	VkGraphicsPipelineCreateInfo ci;
	VkFormat fmt;
	VkResult r;
	if (out)
		*out = NULL;
	if (!out || !b || b->magic != REND_BACKEND_MAGIC || !d ||
	    d->abi_tag != REND_SHADER_ABI_SLANG_TYPED_POINTER_ROOT_V1 ||
	    !d->vertex.bytes || !d->fragment.bytes || d->vertex.size < 20 ||
	    d->fragment.size < 20 || d->vertex.size % 4 || d->fragment.size % 4 ||
	    ((const uint32_t *)d->vertex.bytes)[0] != 0x07230203 ||
	    ((const uint32_t *)d->fragment.bytes)[0] != 0x07230203 ||
	    !(fmt = rend_pipe_format(d->color_format)) ||
	    d->depth_format != REND_FORMAT_D32_FLOAT || d->samples != 1 ||
	    (uintptr_t)d->vertex.bytes % sizeof(uint32_t) ||
	    (uintptr_t)d->fragment.bytes % sizeof(uint32_t))
	{
		rend_vk_set_diagnostic(
			REND_DIAG_INVALID_ARGUMENT, 0,
			"invalid pipeline request or untagged/non-SPIR-V snake artifact");
		return 0;
	}
	for (i = 0; i < b->pipeline_count; i++)
		if (!b->pipelines[i].live)
		{
			p = &b->pipelines[i];
			break;
		}
	if (!p)
	{
		rend_vk_set_diagnostic(REND_DIAG_CAPACITY, 0,
				       "pipeline record capacity exhausted");
		return 0;
	}
	memset(&sm, 0, sizeof(sm));
	sm.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	sm.codeSize = d->vertex.size;
	sm.pCode = d->vertex.bytes;
	r = vkCreateShaderModule(b->device, &sm, NULL, &modules[0]);
	if (r != VK_SUCCESS)
		goto fail;
	sm.codeSize = d->fragment.size;
	sm.pCode = d->fragment.bytes;
	r = vkCreateShaderModule(b->device, &sm, NULL, &modules[1]);
	if (r != VK_SUCCESS)
		goto fail;
	memset(&root, 0, sizeof(root));
	root.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT;
	root.descriptorSet = 0;
	root.firstBinding = 0;
	root.bindingCount = 1;
	root.resourceMask = VK_SPIRV_RESOURCE_TYPE_UNIFORM_BUFFER_BIT_EXT;
	root.source = VK_DESCRIPTOR_MAPPING_SOURCE_PUSH_ADDRESS_EXT;
	root.sourceData.pushAddressOffset = 0;
	memset(&mapping, 0, sizeof(mapping));
	mapping.sType =
		VK_STRUCTURE_TYPE_SHADER_DESCRIPTOR_SET_AND_BINDING_MAPPING_INFO_EXT;
	mapping.mappingCount = 1;
	mapping.pMappings = &root;
	memset(stages, 0, sizeof(stages));
	stages[0].sType = stages[1].sType =
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].pNext = stages[1].pNext = &mapping;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = modules[0];
	stages[0].pName = "vertMain";
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = modules[1];
	stages[1].pName = "fragMain";
	memset(&flags, 0, sizeof(flags));
	flags.sType = VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO;
	flags.flags = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;
	memset(&vi, 0, sizeof(vi));
	vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	memset(&ia, 0, sizeof(ia));
	ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	memset(&vp, 0, sizeof(vp));
	vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	vp.viewportCount = 1;
	vp.scissorCount = 1;
	memset(&rs, 0, sizeof(rs));
	rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rs.polygonMode = VK_POLYGON_MODE_FILL;
	rs.cullMode = VK_CULL_MODE_NONE;
	rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rs.lineWidth = 1;
	memset(&ms, 0, sizeof(ms));
	ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	memset(&ba, 0, sizeof(ba));
	ba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
			    VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	memset(&blend, 0, sizeof(blend));
	blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend.attachmentCount = 1;
	blend.pAttachments = &ba;
	memset(&dyn, 0, sizeof(dyn));
	dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dyn.dynamicStateCount = 5;
	dyn.pDynamicStates = ds;
	memset(&depth, 0, sizeof(depth));
	depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	memset(&rendering, 0, sizeof(rendering));
	rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	rendering.colorAttachmentCount = 1;
	rendering.pColorAttachmentFormats = &fmt;
	rendering.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;
	memset(&ci, 0, sizeof(ci));
	ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	ci.pNext = &flags;
	ci.stageCount = 2;
	ci.pStages = stages;
	ci.pVertexInputState = &vi;
	ci.pInputAssemblyState = &ia;
	ci.pViewportState = &vp;
	ci.pRasterizationState = &rs;
	ci.pMultisampleState = &ms;
	ci.pDepthStencilState = &depth;
	ci.pColorBlendState = &blend;
	ci.pDynamicState = &dyn;
	flags.pNext = &rendering;
	r = vkCreateGraphicsPipelines(b->device, VK_NULL_HANDLE, 1, &ci, NULL,
				      &p->pipeline);
	vkDestroyShaderModule(b->device, modules[0], NULL);
	modules[0] = VK_NULL_HANDLE;
	vkDestroyShaderModule(b->device, modules[1], NULL);
	modules[1] = VK_NULL_HANDLE;
	if (r != VK_SUCCESS)
		goto fail;
	p->backend = b;
	p->format = d->color_format;
	p->depth_format = d->depth_format;
	p->live = 1;
	*out = p;
	return 1;
fail:
	if (modules[0])
		vkDestroyShaderModule(b->device, modules[0], NULL);
	if (modules[1])
		vkDestroyShaderModule(b->device, modules[1], NULL);
	rend_vk_set_diagnostic(REND_DIAG_NATIVE, r,
			       "shader module or native graphics pipeline creation "
			       "failed (entry points vertMain/fragMain)");
	return 0;
}
int
rend_pipeline_destroy(RendBackend b, RendPipeline p)
{
	if (!b || b->magic != REND_BACKEND_MAGIC || !p || p->backend != b ||
	    !p->live || p->refs)
		return 0;
	vkDestroyPipeline(b->device, p->pipeline, NULL);
	memset(p, 0, sizeof(*p));
	return 1;
}

/* ---- Amalgamated from Rend2/rend_presentation.c ---- */
/* Narrow Vulkan WSI bridge for a caller-owned VkSurfaceKHR. */

int
rend_device_surface_support(const RendDeviceInfo *d, uint64_t surface,
			    uint32_t family, int *out)
{
	return rend_vk_surface_support(d, surface, family, out);
}
static RendFormat
rend_wsi_rend_format(VkFormat f)
{
	return f == VK_FORMAT_R8G8B8A8_UNORM   ? REND_FORMAT_RGBA8_UNORM
	       : f == VK_FORMAT_B8G8R8A8_UNORM ? REND_FORMAT_BGRA8_UNORM
					       : REND_FORMAT_NONE;
}
static void
rend_presentation_cleanup(RendPresentationRecord *p)
{
	RendBackend b = p->backend;
	uint32_t i;
	for (i = 0; i < p->image_count; i++)
	{
		memset(&p->view_records[i], 0, sizeof(p->view_records[i]));
		if (p->views[i])
			vkDestroyImageView(b->device, p->views[i], NULL);
		if (p->acquire_semaphores[i])
			vkDestroySemaphore(b->device, p->acquire_semaphores[i], NULL);
		if (p->render_semaphores[i])
			vkDestroySemaphore(b->device, p->render_semaphores[i], NULL);
	}
	if (p->swapchain)
		vkDestroySwapchainKHR(b->device, p->swapchain, NULL);
	p->live = 0;
}
int
rend_presentation_create(RendBackend b, uint64_t native_surface,
			 uint32_t width, uint32_t height,
			 RendPresentation *out)
{
	VkSurfaceKHR surface;
	VkSurfaceCapabilitiesKHR caps;
	VkSurfaceFormatKHR formats[512], selected;
	VkSwapchainCreateInfoKHR ci;
	VkResult r;
	uint32_t count = 0, i;
	int supported = 0;
	RendPresentationRecord *p = NULL;
	VkExtent2D extent;
	if (out)
		*out = NULL;
	if (!out || !b || b->magic != REND_BACKEND_MAGIC || !b->presentation_count ||
	    !native_surface || !width || !height || b->terminal)
	{
		rend_vk_set_diagnostic(REND_DIAG_INVALID_ARGUMENT, 0,
				       "invalid presentation configuration");
		return 0;
	}
	memcpy(&surface, &native_surface, sizeof(surface));
	for (i = 0; i < b->presentation_count; i++)
		if (!b->presentations[i].live)
		{
			p = &b->presentations[i];
			break;
		}
	if (!p)
	{
		rend_vk_set_diagnostic(REND_DIAG_CAPACITY, 0,
				       "presentation record capacity exhausted");
		return 0;
	}
	if (!b->queue_count ||
	    !rend_vk_surface_support(b->device_info, native_surface,
				     b->queues[0].family, &supported) ||
	    !supported)
	{
		rend_vk_set_diagnostic(REND_DIAG_UNSUPPORTED, 0,
				       "configured queue zero does not support this "
				       "surface; choose a compatible family");
		return 0;
	}
	r = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(b->physical, surface, &caps);
	if (r != VK_SUCCESS)
		goto native;
	r = vkGetPhysicalDeviceSurfaceFormatsKHR(b->physical, surface, &count, NULL);
	if (r != VK_SUCCESS)
		goto native;
	if (!count || count > 512) {
		rend_vk_set_diagnostic(REND_DIAG_CAPACITY, 0, "surface format count exceeds bounded 512-format workspace or is empty");
		return 0;
	}
	r = vkGetPhysicalDeviceSurfaceFormatsKHR(b->physical, surface, &count,
						 formats);
	if (r != VK_SUCCESS)
		goto native;
	selected = formats[0];
	if (count == 1 && selected.format == VK_FORMAT_UNDEFINED)
		selected.format = VK_FORMAT_R8G8B8A8_UNORM;
	for (i = 0; i < count; i++)
		if (formats[i].format == VK_FORMAT_R8G8B8A8_UNORM)
		{
			selected = formats[i];
			break;
		}
	if (selected.format != VK_FORMAT_R8G8B8A8_UNORM)
	{
		for (i = 0; i < count; i++)
			if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM)
			{
				selected = formats[i];
				break;
			}
	}
	if (rend_wsi_rend_format(selected.format) == REND_FORMAT_NONE)
	{
		rend_vk_set_diagnostic(REND_DIAG_UNSUPPORTED, 0,
				       "surface offers neither RGBA8 nor BGRA8 UNORM");
		return 0;
	}
	if (caps.currentExtent.width != UINT32_MAX)
		extent = caps.currentExtent;
	else
	{
		extent.width = width < caps.minImageExtent.width ? caps.minImageExtent.width
			       : width > caps.maxImageExtent.width
				       ? caps.maxImageExtent.width
				       : width;
		extent.height =
			height < caps.minImageExtent.height   ? caps.minImageExtent.height
			: height > caps.maxImageExtent.height ? caps.maxImageExtent.height
							      : height;
	}
	if (!extent.width || !extent.height)
	{
		rend_vk_set_diagnostic(REND_DIAG_OUT_OF_DATE, 0,
				       "surface minimized to zero extent");
		return 0;
	}
	memset(&ci, 0, sizeof(ci));
	ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	ci.surface = surface;
	ci.minImageCount = caps.minImageCount + 1;
	if (caps.maxImageCount && ci.minImageCount > caps.maxImageCount)
		ci.minImageCount = caps.maxImageCount;
	if (ci.minImageCount > 8 && caps.minImageCount <= 8)
		ci.minImageCount = 8;
	if (ci.minImageCount > 8)
	{
		rend_vk_set_diagnostic(
			REND_DIAG_CAPACITY, 0,
			"swapchain image count exceeds fixed eight-image capacity");
		return 0;
	}
	ci.imageFormat = selected.format;
	ci.imageColorSpace = selected.colorSpace;
	ci.imageExtent = extent;
	ci.imageArrayLayers = 1;
	if (!(caps.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT))
	{
		rend_vk_set_diagnostic(REND_DIAG_UNSUPPORTED, 0,
				       "surface lacks color-attachment swapchain usage");
		return 0;
	}
	ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ci.preTransform = caps.currentTransform;
	ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	if (!(caps.supportedCompositeAlpha & ci.compositeAlpha))
	{
		for (i = 0; i < 4; i++)
			if (caps.supportedCompositeAlpha & (1u << i))
			{
				ci.compositeAlpha = 1u << i;
				break;
			}
	}
	ci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
	ci.clipped = VK_TRUE;
	memset(p, 0, sizeof(*p));
	p->backend = b;
	p->surface = surface;
	p->format = rend_wsi_rend_format(selected.format);
	p->width = extent.width;
	p->height = extent.height;
	p->queue_index = 0;
	r = vkCreateSwapchainKHR(b->device, &ci, NULL, &p->swapchain);
	if (r != VK_SUCCESS)
		goto native;
	r = vkGetSwapchainImagesKHR(b->device, p->swapchain, &count, NULL);
	if (r != VK_SUCCESS || !count || count > 8)
		goto cleanup;
	r = vkGetSwapchainImagesKHR(b->device, p->swapchain, &count, p->images);
	if (r != VK_SUCCESS)
		goto cleanup;
	p->image_count = count;
	for (i = 0; i < count; i++)
	{
		RendTextureViewRecord *v = &p->view_records[i];
		VkImageViewCreateInfo vi;
		VkSemaphoreCreateInfo si;
		memset(&vi, 0, sizeof(vi));
		vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		vi.image = p->images[i];
		vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
		vi.format = selected.format;
		vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		vi.subresourceRange.levelCount = 1;
		vi.subresourceRange.layerCount = 1;
		r = vkCreateImageView(b->device, &vi, NULL, &p->views[i]);
		if (r != VK_SUCCESS)
			goto cleanup;
		v->live = 1;
		v->external = 1;
		v->backend = b;
		v->view = p->views[i];
		v->format = p->format;
		v->width = p->width;
		v->height = p->height;
		v->desc.format = p->format;
		v->desc.view_type = REND_VIEW_2D;
		v->desc.aspect = REND_ASPECT_COLOR;
		v->desc.mip_count = 1;
		v->desc.layer_count = 1;
		v->presentation = p;
		memset(&si, 0, sizeof(si));
		si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		r = vkCreateSemaphore(b->device, &si, NULL, &p->acquire_semaphores[i]);
		if (r != VK_SUCCESS)
			goto cleanup;
		r = vkCreateSemaphore(b->device, &si, NULL, &p->render_semaphores[i]);
		if (r != VK_SUCCESS)
			goto cleanup;
	}
	p->live = 1;
	*out = p;
	return 1;
cleanup:
	p->image_count = count <= 8 ? count : 0;
	rend_presentation_cleanup(p);
	rend_vk_set_diagnostic(REND_DIAG_NATIVE, r,
			       "swapchain image/view/semaphore setup failed; native "
			       "acquisitions unwound");
	return 0;
native:
	rend_vk_set_diagnostic(
		REND_DIAG_NATIVE, r,
		"surface capabilities/formats or vkCreateSwapchainKHR failed");
	return 0;
}
int
rend_presentation_get_info(RendPresentation p, RendPresentationInfo *out)
{
	if (!p || !p->live || !out)
		return 0;
	out->width = p->width;
	out->height = p->height;
	out->format = p->format;
	out->image_count = p->image_count;
	return 1;
}
int
rend_presentation_acquire(RendPresentation p, uint64_t timeout,
			  RendTextureView *out)
{
	VkResult r;
	uint32_t index, n, slot = UINT32_MAX;
	if (out)
		*out = NULL;
	if (!out || !p || !p->live || p->acquired || p->backend->terminal)
		return 0;
	for (n = 0; n < p->image_count; n++)
	{
		uint32_t candidate = (p->next_acquire_slot + n) % p->image_count;
		if (p->acquire_timeline[candidate])
		{
			uint64_t done = 0;
			if (vkGetSemaphoreCounterValue(p->backend->device,
						       p->acquire_timeline[candidate]->semaphore,
						       &done) == VK_SUCCESS &&
			    done >= p->acquire_values[candidate])
				p->acquire_timeline[candidate] = NULL;
		}
		if (!p->acquire_timeline[candidate])
		{
			slot = candidate;
			break;
		}
	}
	if (slot == UINT32_MAX)
	{
		rend_vk_set_diagnostic(REND_DIAG_TIMEOUT, VK_NOT_READY,
				       "all acquire semaphores are still in flight");
		return 0;
	}
	r = vkAcquireNextImageKHR(p->backend->device, p->swapchain, timeout,
				  p->acquire_semaphores[slot], VK_NULL_HANDLE,
				  &index);
	if (r == VK_TIMEOUT || r == VK_NOT_READY)
	{
		rend_vk_set_diagnostic(REND_DIAG_TIMEOUT, r,
				       "swapchain image acquire timed out/no image");
		return 0;
	}
	if (r == VK_ERROR_OUT_OF_DATE_KHR)
	{
		rend_vk_set_diagnostic(
			REND_DIAG_OUT_OF_DATE, r,
			"swapchain out of date; explicitly drain/destroy/recreate");
		return 0;
	}
	if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR)
	{
		p->backend->terminal = 1;
		rend_vk_set_diagnostic(REND_DIAG_TERMINAL, r,
				       "native swapchain acquire failed");
		return 0;
	}
	p->image_index = index;
	p->acquire_slot = slot;
	p->next_acquire_slot = (slot + 1) % p->image_count;
	p->acquired = 1;
	p->submitted = 0;
	p->backend->drained = 0;
	*out = &p->view_records[index];
	return 1;
}
int
rend_presentation_present(RendPresentation p, RendQueue q)
{
	VkPresentInfoKHR info;
	VkResult r;
	VkSwapchainKHR swap;
	uint32_t index;
	if (!p || !p->live || !p->acquired || !p->submitted || !q ||
	    q->backend != p->backend ||
	    q->family != p->backend->queues[p->queue_index].family)
		return 0;
	swap = p->swapchain;
	index = p->image_index;
	memset(&info, 0, sizeof(info));
	info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	info.waitSemaphoreCount = 1;
	info.pWaitSemaphores = &p->render_semaphores[index];
	info.swapchainCount = 1;
	info.pSwapchains = &swap;
	info.pImageIndices = &index;
	r = vkQueuePresentKHR(q->queue, &info);
	if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR &&
	    r != VK_ERROR_OUT_OF_DATE_KHR)
	{
		p->backend->terminal = 1;
		rend_vk_set_diagnostic(REND_DIAG_TERMINAL, r, "vkQueuePresentKHR failed");
		return 0;
	}
	p->acquired = 0;
	p->submitted = 0;
	p->needs_retire = 1;
	p->backend->drained = 0;
	if (r == VK_ERROR_OUT_OF_DATE_KHR)
	{
		rend_vk_set_diagnostic(REND_DIAG_OUT_OF_DATE, r,
				       "presentation out of date; explicitly recreate");
		return 0;
	}
	return 1;
}
int
rend_presentation_destroy(RendPresentation p)
{
	uint32_t i;
	if (!p || !p->live)
		return 0;
	if ((p->acquired || p->needs_retire) && !p->backend->drained)
	{
		rend_vk_set_diagnostic(
			REND_DIAG_INVALID_ARGUMENT, 0,
			"presentation must be explicitly wait_idle-drained before retirement");
		return 0;
	}
	for (i = 0; i < p->image_count; i++)
		if (p->view_records[i].refs)
		{
			rend_vk_set_diagnostic(REND_DIAG_INVALID_ARGUMENT, 0,
					       "presentation view still referenced by recorded "
					       "command lists; reset those pools");
			return 0;
		}
	rend_presentation_cleanup(p);
	return 1;
}

/* ---- Amalgamated from Rend2/rend_texture.c ---- */
/* Explicit placed snake-subset images and views. */

static VkFormat
rend_vk_format(RendFormat f)
{
	switch (f)
	{
	case REND_FORMAT_RGBA8_UNORM:
		return VK_FORMAT_R8G8B8A8_UNORM;
	case REND_FORMAT_BGRA8_UNORM:
		return VK_FORMAT_B8G8R8A8_UNORM;
	case REND_FORMAT_D32_FLOAT:
		return VK_FORMAT_D32_SFLOAT;
	default:
		return VK_FORMAT_UNDEFINED;
	}
}
static VkImageUsageFlags
rend_vk_image_usage(uint32_t u)
{
	VkImageUsageFlags n = 0;
	if (u & REND_TEXTURE_SAMPLED)
		n |= VK_IMAGE_USAGE_SAMPLED_BIT;
	if (u & REND_TEXTURE_STORAGE)
		n |= VK_IMAGE_USAGE_STORAGE_BIT;
	if (u & REND_TEXTURE_COLOR_ATTACHMENT)
		n |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if (u & REND_TEXTURE_TRANSFER_SRC)
		n |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	if (u & REND_TEXTURE_TRANSFER_DST)
		n |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	if (u & REND_TEXTURE_DEPTH_ATTACHMENT)
		n |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	return n;
}
int
rend_texture_memory_requirements(RendBackend b, const RendTextureDesc *d,
				 uint32_t cls,
				 RendStorageRequirements *out)
{
	VkImageCreateInfo ci;
	VkImage image = VK_NULL_HANDLE;
	VkMemoryRequirements2 mr2;
	VkImageMemoryRequirementsInfo2 mi;
	VkMemoryDedicatedRequirements dedicated;
	VkResult r;
	uint32_t type;
	if (out)
		memset(out, 0, sizeof(*out));
	if (out == NULL || !b || b->magic != REND_BACKEND_MAGIC || !d || !cls ||
	    cls > b->memory.memoryTypeCount || !d->width || !d->height ||
	    d->depth != 1 || !d->mip_levels || !d->array_layers || d->samples != 1 ||
	    rend_vk_format((RendFormat)d->format) == VK_FORMAT_UNDEFINED ||
	    !d->usage || (d->usage & (REND_TEXTURE_SAMPLED | REND_TEXTURE_STORAGE)) ||
	    (d->format == REND_FORMAT_D32_FLOAT &&
	     !(d->usage & REND_TEXTURE_DEPTH_ATTACHMENT)) ||
	    (d->format != REND_FORMAT_D32_FLOAT &&
	     (d->usage & REND_TEXTURE_DEPTH_ATTACHMENT)) ||
	    (d->usage &
	     ~(REND_TEXTURE_SAMPLED | REND_TEXTURE_STORAGE |
	       REND_TEXTURE_COLOR_ATTACHMENT | REND_TEXTURE_TRANSFER_SRC |
	       REND_TEXTURE_TRANSFER_DST | REND_TEXTURE_DEPTH_ATTACHMENT)))
	{
		rend_vk_set_diagnostic(
			REND_DIAG_INVALID_ARGUMENT, 0,
			"invalid RGBA/BGRA texture requirements description");
		return 0;
	}
	memset(&ci, 0, sizeof(ci));
	ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	ci.imageType = VK_IMAGE_TYPE_2D;
	ci.format = rend_vk_format((RendFormat)d->format);
	ci.extent.width = d->width;
	ci.extent.height = d->height;
	ci.extent.depth = 1;
	ci.mipLevels = d->mip_levels;
	ci.arrayLayers = d->array_layers;
	ci.samples = VK_SAMPLE_COUNT_1_BIT;
	ci.tiling = VK_IMAGE_TILING_OPTIMAL;
	ci.usage = rend_vk_image_usage(d->usage);
	ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	r = vkCreateImage(b->device, &ci, NULL, &image);
	if (r != VK_SUCCESS)
	{
		rend_vk_set_diagnostic(REND_DIAG_NATIVE, r,
				       "vkCreateImage requirements probe failed");
		return 0;
	}
	memset(&mi, 0, sizeof(mi));
	mi.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2;
	mi.image = image;
	memset(&dedicated, 0, sizeof(dedicated));
	dedicated.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS;
	memset(&mr2, 0, sizeof(mr2));
	mr2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
	mr2.pNext = &dedicated;
	vkGetImageMemoryRequirements2(b->device, &mi, &mr2);
	vkDestroyImage(b->device, image, NULL);
	type = cls - 1;
	if (dedicated.requiresDedicatedAllocation)
	{
		rend_vk_set_diagnostic(REND_DIAG_UNSUPPORTED, 0,
				       "texture requires dedicated allocation not expressible by snake placement");
		return 0;
	}
	if (!(mr2.memoryRequirements.memoryTypeBits & (1u << type)))
	{
		rend_vk_set_diagnostic(
			REND_DIAG_UNSUPPORTED, 0,
			"selected memory class is incompatible with texture memoryTypeBits");
		return 0;
	}
	out->size = mr2.memoryRequirements.size;
	out->alignment = mr2.memoryRequirements.alignment;
	return 1;
}
int
rend_texture_create(RendBackend b, const RendTextureDesc *d, RendHeap h,
		    uint64_t offset, uint64_t size, RendTexture *out)
{
	uint32_t i;
	RendTextureRecord *t = NULL;
	RendStorageRequirements req;
	VkImageCreateInfo ci;
	VkMemoryRequirements mr;
	VkImageMemoryRequirementsInfo2 mi;
	VkMemoryRequirements2 mr2;
	VkMemoryDedicatedRequirements dedicated;
	VkResult r;
	if (out)
		*out = NULL;
	if (!out || !b || b->magic != REND_BACKEND_MAGIC || !h || h->backend != b ||
	    !h->live || h->kind != REND_HEAP_TEXTURE_STORAGE || !d)
		goto fail;
	if (!rend_texture_memory_requirements(b, d, h->memory_class, &req))
		return 0;
	if (offset % req.alignment || offset > h->size ||
	    size > h->size - offset || size < req.size ||
	    req.size > h->size - offset)
		goto fail;
	for (i = 0; i < b->texture_count; i++)
		if (!b->textures[i].live)
		{
			t = &b->textures[i];
			break;
		}
	if (!t)
	{
		rend_vk_set_diagnostic(REND_DIAG_CAPACITY, 0,
				       "texture record capacity exhausted");
		return 0;
	}
	memset(&ci, 0, sizeof(ci));
	ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	ci.imageType = VK_IMAGE_TYPE_2D;
	ci.format = rend_vk_format((RendFormat)d->format);
	ci.extent.width = d->width;
	ci.extent.height = d->height;
	ci.extent.depth = 1;
	ci.mipLevels = d->mip_levels;
	ci.arrayLayers = d->array_layers;
	ci.samples = VK_SAMPLE_COUNT_1_BIT;
	ci.tiling = VK_IMAGE_TILING_OPTIMAL;
	ci.usage = rend_vk_image_usage(d->usage);
	ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	r = vkCreateImage(b->device, &ci, NULL, &t->image);
	if (r != VK_SUCCESS)
		goto native;
	memset(&mi, 0, sizeof(mi));
	mi.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2;
	mi.image = t->image;
	memset(&dedicated, 0, sizeof(dedicated));
	dedicated.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS;
	memset(&mr2, 0, sizeof(mr2));
	mr2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
	mr2.pNext = &dedicated;
	vkGetImageMemoryRequirements2(b->device, &mi, &mr2);
	mr = mr2.memoryRequirements;
	if (dedicated.requiresDedicatedAllocation || mr.size > size ||
	    offset % mr.alignment ||
	    !(mr.memoryTypeBits & (1u << (h->memory_class - 1))))
	{
		vkDestroyImage(b->device, t->image, NULL);
		t->image = VK_NULL_HANDLE;
		rend_vk_set_diagnostic(
			REND_DIAG_UNSUPPORTED, 0,
			"image requires unsupported dedicated/incompatible placement");
		return 0;
	}
	r = vkBindImageMemory(b->device, t->image, h->memory, offset);
	if (r != VK_SUCCESS)
	{
		vkDestroyImage(b->device, t->image, NULL);
		t->image = VK_NULL_HANDLE;
		goto native;
	}
	t->backend = b;
	t->heap = h;
	t->desc = *d;
	t->offset = offset;
	t->requirements_size = mr.size;
	t->requirements_alignment = mr.alignment;
	t->live = 1;
	h->refs++;
	*out = t;
	return 1;
native:
	rend_vk_set_diagnostic(REND_DIAG_NATIVE, r,
			       "native placed image create/bind failed");
	return 0;
fail:
	rend_vk_set_diagnostic(REND_DIAG_INVALID_ARGUMENT, 0,
			       "invalid texture placement argument");
	return 0;
}
int
rend_texture_view_create(RendBackend b, RendTexture t,
			 const RendTextureViewDesc *d,
			 RendTextureView *out)
{
	uint32_t i;
	RendTextureViewRecord *v = NULL;
	VkImageViewCreateInfo ci;
	VkResult r;
	if (out)
		*out = NULL;
	if (!out || !b || b->magic != REND_BACKEND_MAGIC || !t || !t->live ||
	    t->backend != b || !d || d->view_type != REND_VIEW_2D ||
	    (d->aspect != REND_ASPECT_COLOR && d->aspect != REND_ASPECT_DEPTH) ||
	    rend_vk_format(d->format) == VK_FORMAT_UNDEFINED ||
	    d->format != (RendFormat)t->desc.format ||
	    (d->aspect == REND_ASPECT_COLOR &&
	     !(t->desc.usage & REND_TEXTURE_COLOR_ATTACHMENT)) ||
	    (d->aspect == REND_ASPECT_DEPTH &&
	     !(t->desc.usage & REND_TEXTURE_DEPTH_ATTACHMENT)) ||
	    (d->format == REND_FORMAT_D32_FLOAT) !=
		    (d->aspect == REND_ASPECT_DEPTH) ||
	    !d->mip_count || !d->layer_count || d->base_mip >= t->desc.mip_levels ||
	    d->mip_count > t->desc.mip_levels - d->base_mip ||
	    d->base_layer >= t->desc.array_layers ||
	    d->layer_count > t->desc.array_layers - d->base_layer)
		goto fail;
	for (i = 0; i < b->view_count; i++)
		if (!b->views[i].live)
		{
			v = &b->views[i];
			break;
		}
	if (!v)
	{
		rend_vk_set_diagnostic(REND_DIAG_CAPACITY, 0,
				       "texture-view capacity exhausted");
		return 0;
	}
	memset(&ci, 0, sizeof(ci));
	ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	ci.image = t->image;
	ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
	ci.format = rend_vk_format(d->format);
	ci.subresourceRange.aspectMask = d->aspect == REND_ASPECT_DEPTH
						 ? VK_IMAGE_ASPECT_DEPTH_BIT
						 : VK_IMAGE_ASPECT_COLOR_BIT;
	ci.subresourceRange.baseMipLevel = d->base_mip;
	ci.subresourceRange.levelCount = d->mip_count;
	ci.subresourceRange.baseArrayLayer = d->base_layer;
	ci.subresourceRange.layerCount = d->layer_count;
	r = vkCreateImageView(b->device, &ci, NULL, &v->view);
	if (r != VK_SUCCESS)
	{
		rend_vk_set_diagnostic(REND_DIAG_NATIVE, r, "vkCreateImageView failed");
		return 0;
	}
	v->backend = b;
	v->texture = t;
	v->desc = *d;
	v->width = t->desc.width;
	v->height = t->desc.height;
	v->format = (RendFormat)t->desc.format;
	v->live = 1;
	*out = v;
	return 1;
fail:
	rend_vk_set_diagnostic(REND_DIAG_INVALID_ARGUMENT, 0,
			       "invalid texture view description");
	return 0;
}
int
rend_texture_view_destroy(RendBackend b, RendTextureView v)
{
	if (!b || b->magic != REND_BACKEND_MAGIC || !v || v->backend != b ||
	    !v->live || v->external || v->refs)
		return 0;
	vkDestroyImageView(b->device, v->view, NULL);
	memset(v, 0, sizeof(*v));
	return 1;
}
int
rend_texture_destroy(RendBackend b, RendTexture t)
{
	uint32_t i;
	if (!b || b->magic != REND_BACKEND_MAGIC || !t || t->backend != b ||
	    !t->live || t->refs)
		return 0;
	for (i = 0; i < b->view_count; i++)
		if (b->views[i].live && b->views[i].texture == t)
		{
			rend_vk_set_diagnostic(REND_DIAG_INVALID_ARGUMENT, 0,
					       "texture still has live views");
			return 0;
		}
	vkDestroyImage(b->device, t->image, NULL);
	if (t->heap->refs)
		t->heap->refs--;
	memset(t, 0, sizeof(*t));
	return 1;
}

/* ---- Amalgamated from Rend2/rend_work.c ---- */
/* Direct Vulkan work recording and explicit completion. */
enum
{
	REND_REF_VIEW = 1,
	REND_REF_TEXTURE = 2,
	REND_REF_PIPELINE = 3,
	REND_REF_HEAP = 4
};
static int
rend_ref(RendCommandList c, uint32_t kind, void *object)
{
	uint32_t i;
	if (!object)
		return 0;
	for (i = 0; i < c->ref_count; i++)
		if (c->refs[i].kind == kind && c->refs[i].object == object)
			return 1;
	if (c->ref_count >= 16)
	{
		c->state = 4;
		rend_vk_set_diagnostic(REND_DIAG_CAPACITY, 0,
				       "command list native-object reference capacity (16) "
				       "exhausted; reset to recover");
		return 0;
	}
	c->refs[c->ref_count].kind = kind;
	c->refs[c->ref_count++].object = object;
	switch (kind)
	{
	case REND_REF_VIEW:
		((RendTextureViewRecord *)object)->refs++;
		break;
	case REND_REF_TEXTURE:
		((RendTextureRecord *)object)->refs++;
		break;
	case REND_REF_PIPELINE:
		((RendPipelineRecord *)object)->refs++;
		break;
	case REND_REF_HEAP:
		((RendHeapRecord *)object)->refs++;
		break;
	}
	return 1;
}
static void
rend_unref_all(RendCommandRecord *c)
{
	uint32_t i;
	for (i = 0; i < c->ref_count; i++)
	{
		void *o = c->refs[i].object;
		switch (c->refs[i].kind)
		{
		case REND_REF_VIEW:
			if (((RendTextureViewRecord *)o)->refs)
				((RendTextureViewRecord *)o)->refs--;
			break;
		case REND_REF_TEXTURE:
			if (((RendTextureRecord *)o)->refs)
				((RendTextureRecord *)o)->refs--;
			break;
		case REND_REF_PIPELINE:
			if (((RendPipelineRecord *)o)->refs)
				((RendPipelineRecord *)o)->refs--;
			break;
		case REND_REF_HEAP:
			if (((RendHeapRecord *)o)->refs)
				((RendHeapRecord *)o)->refs--;
			break;
		}
	}
	c->ref_count = 0;
}
static int
rend_command_fail(RendCommandList c)
{
	if (c && c->state == 1)
		c->state = 4;
	return 0;
}
static int
rend_list_recording(RendCommandList c)
{
	return c && c->backend && c->backend->magic == REND_BACKEND_MAGIC &&
	       !c->backend->terminal && c->state == 1;
}
static void
rend_queue_reap(RendQueueRecord *q)
{
	uint32_t i;
	RendBackend b = q->backend;
	for (i = 0; i < q->submission_capacity; i++)
	{
		RendQueueSubmission *s = &q->submissions[i];
		uint64_t done = 0;
		if (s->live && !s->uncertain &&
		    vkGetSemaphoreCounterValue(b->device, s->timeline->semaphore, &done) ==
			    VK_SUCCESS &&
		    done >= s->value)
		{
			memset(s, 0, sizeof(*s));
			if (q->active_submissions)
				q->active_submissions--;
		}
	}
}
static VkPipelineStageFlags2
rend_vk_stage(uint32_t s)
{
	VkPipelineStageFlags2 r = 0;
	if (s & REND_STAGE_HOST)
		r |= VK_PIPELINE_STAGE_2_HOST_BIT;
	if (s & REND_STAGE_TRANSFER)
		r |= VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
	if (s & REND_STAGE_COMPUTE)
		r |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	if (s & REND_STAGE_INDEX_INPUT)
		r |= VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT;
	if (s & REND_STAGE_VERTEX)
		r |= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
	if (s & REND_STAGE_TASK)
		r |= VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT;
	if (s & REND_STAGE_MESH)
		r |= VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT;
	if (s & REND_STAGE_FRAGMENT)
		r |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	if (s & REND_STAGE_EARLY_DEPTH_STENCIL)
		r |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
	if (s & REND_STAGE_LATE_DEPTH_STENCIL)
		r |= VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
	if (s & REND_STAGE_COLOR_OUTPUT)
		r |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	if (s & REND_STAGE_INDIRECT)
		r |= VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
	if (s & REND_STAGE_GRAPHICS)
		r |= VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
	return r;
}
static VkAccessFlags2
rend_vk_access(uint32_t a)
{
	return (a & REND_ACCESS_READ ? VK_ACCESS_2_MEMORY_READ_BIT : 0) |
	       (a & REND_ACCESS_WRITE ? VK_ACCESS_2_MEMORY_WRITE_BIT : 0);
}
int
rend_queue_get(RendBackend b, uint32_t i, RendQueue *o)
{
	if (o)
		*o = NULL;
	if (!o || !b || b->magic != REND_BACKEND_MAGIC || i >= b->queue_count)
		return 0;
	*o = &b->queues[i];
	return 1;
}
int
rend_command_pool_get(RendBackend b, uint32_t i, RendCommandPool *o)
{
	if (o)
		*o = NULL;
	if (!o || !b || b->magic != REND_BACKEND_MAGIC || i >= b->pool_count)
		return 0;
	*o = &b->pools[i];
	return 1;
}
int
rend_command_list_begin(RendCommandPool p, uint32_t slot,
			RendCommandList *o)
{
	VkCommandBufferBeginInfo bi;
	VkResult r;
	if (o)
		*o = NULL;
	if (!o || !p || !p->live || p->backend->terminal || slot >= p->count ||
	    p->commands[slot].state)
		return 0;
	memset(&bi, 0, sizeof(bi));
	bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	r = vkBeginCommandBuffer(p->commands[slot].command, &bi);
	if (r != VK_SUCCESS)
	{
		rend_vk_set_diagnostic(REND_DIAG_NATIVE, r, "vkBeginCommandBuffer failed");
		return 0;
	}
	p->commands[slot].state = 1;
	p->commands[slot].in_render = 0;
	p->commands[slot].pipeline = NULL;
	p->commands[slot].viewport_set = 0;
	p->commands[slot].scissor_set = 0;
	p->commands[slot].depth_state_set = 0;
	p->commands[slot].presentation = NULL;
	p->commands[slot].has_depth = 0;
	p->commands[slot].render_width = p->commands[slot].render_height = 0;
	*o = &p->commands[slot];
	return 1;
}
int
rend_command_list_end(RendCommandList c)
{
	VkResult r;
	if (!rend_list_recording(c) || c->in_render)
	{
		if (c && c->state == 1)
			c->state = 4;
		return 0;
	}
	r = vkEndCommandBuffer(c->command);
	if (r != VK_SUCCESS)
	{
		c->state = 4;
		rend_vk_set_diagnostic(REND_DIAG_NATIVE, r,
				       "vkEndCommandBuffer failed; reset pool to recover");
		return 0;
	}
	c->state = 2;
	return 1;
}
int
rend_command_pool_reset(RendCommandPool p)
{
	uint32_t i;
	VkResult r;
	if (!p || !p->live)
		return 0;
	for (i = 0; i < p->count; i++)
		if (p->commands[i].state == 3 && !p->backend->drained)
		{
			uint64_t v = 0;
			if (!p->commands[i].completion ||
			    vkGetSemaphoreCounterValue(p->backend->device,
						       p->commands[i].completion->semaphore,
						       &v) != VK_SUCCESS ||
			    v < p->commands[i].completion_value)
			{
				rend_vk_set_diagnostic(REND_DIAG_INVALID_ARGUMENT, 0,
						       "command pool still has pending GPU use");
				return 0;
			}
		}
	r = vkResetCommandPool(p->backend->device, p->pool, 0);
	if (r != VK_SUCCESS)
	{
		rend_vk_set_diagnostic(REND_DIAG_NATIVE, r, "vkResetCommandPool failed");
		return 0;
	}
	for (i = 0; i < p->count; i++)
	{
		RendCommandRecord *c = &p->commands[i];
		rend_unref_all(c);
		c->state = 0;
		c->completion = NULL;
		c->completion_value = 0;
		c->in_render = 0;
		c->pipeline = NULL;
		c->color_target = c->depth_target = NULL;
		c->presentation = NULL;
		c->has_depth = 0;
		c->viewport_set = c->scissor_set = c->depth_state_set = 0;
		c->render_width = c->render_height = 0;
		c->active_color_format = REND_FORMAT_NONE;
	}
	return 1;
}
int
rend_cmd_barrier(RendCommandList c, const RendBarrier *b)
{
	const uint32_t all =
		REND_STAGE_HOST | REND_STAGE_TRANSFER | REND_STAGE_COMPUTE |
		REND_STAGE_INDEX_INPUT | REND_STAGE_VERTEX | REND_STAGE_FRAGMENT |
		REND_STAGE_EARLY_DEPTH_STENCIL | REND_STAGE_LATE_DEPTH_STENCIL |
		REND_STAGE_COLOR_OUTPUT | REND_STAGE_INDIRECT | REND_STAGE_GRAPHICS;
	VkMemoryBarrier2 m;
	VkDependencyInfo d;
	if (!rend_list_recording(c) || !b || c->in_render ||
	    (b->producer_stages & ~all) || (b->consumer_stages & ~all) ||
	    (b->producer_access & ~(REND_ACCESS_READ | REND_ACCESS_WRITE)) ||
	    (b->consumer_access & ~(REND_ACCESS_READ | REND_ACCESS_WRITE)) ||
	    (!b->producer_stages && b->producer_access) ||
	    (!b->consumer_stages && b->consumer_access) ||
	    ((b->producer_stages & (REND_STAGE_INDEX_INPUT | REND_STAGE_INDIRECT)) &&
	     (b->producer_access & REND_ACCESS_WRITE)) ||
	    ((b->consumer_stages & (REND_STAGE_INDEX_INPUT | REND_STAGE_INDIRECT)) &&
	     (b->consumer_access & REND_ACCESS_WRITE)))
		return rend_command_fail(c);
	memset(&m, 0, sizeof(m));
	m.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
	m.srcStageMask = rend_vk_stage(b->producer_stages);
	m.srcAccessMask = rend_vk_access(b->producer_access);
	m.dstStageMask = rend_vk_stage(b->consumer_stages);
	m.dstAccessMask = rend_vk_access(b->consumer_access);
	memset(&d, 0, sizeof(d));
	d.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	d.memoryBarrierCount = 1;
	d.pMemoryBarriers = &m;
	vkCmdPipelineBarrier2(c->command, &d);
	return 1;
}
static void
rend_swap_transition(RendCommandRecord *c,
		     RendPresentationRecord *p,
		     VkImageLayout next)
{
	VkImageMemoryBarrier2 m;
	VkDependencyInfo d;
	memset(&m, 0, sizeof(m));
	m.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	m.srcStageMask = next == VK_IMAGE_LAYOUT_GENERAL
				 ? VK_PIPELINE_STAGE_2_NONE
				 : VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	m.srcAccessMask = next == VK_IMAGE_LAYOUT_GENERAL
				  ? 0
				  : VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
	m.dstStageMask = next == VK_IMAGE_LAYOUT_GENERAL
				 ? VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
				 : VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
	m.dstAccessMask = next == VK_IMAGE_LAYOUT_GENERAL
				  ? VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
				  : 0;
	m.oldLayout = next == VK_IMAGE_LAYOUT_GENERAL ? VK_IMAGE_LAYOUT_UNDEFINED
						      : VK_IMAGE_LAYOUT_GENERAL;
	m.newLayout = next;
	m.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	m.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	m.image = p->images[p->image_index];
	m.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	m.subresourceRange.levelCount = 1;
	m.subresourceRange.layerCount = 1;
	memset(&d, 0, sizeof(d));
	d.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	d.imageMemoryBarrierCount = 1;
	d.pImageMemoryBarriers = &m;
	vkCmdPipelineBarrier2(c->command, &d);
}
static int
rend_activate(RendCommandList c, RendTextureRecord *t)
{
	(void)c;
	{
		VkImageMemoryBarrier2 m;
		VkDependencyInfo d;
		memset(&m, 0, sizeof(m));
		m.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		m.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
		m.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		m.dstAccessMask =
			VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
		m.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		m.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		m.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		m.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		m.image = t->image;
		m.subresourceRange.aspectMask = t->desc.format == REND_FORMAT_D32_FLOAT
							? VK_IMAGE_ASPECT_DEPTH_BIT
							: VK_IMAGE_ASPECT_COLOR_BIT;
		m.subresourceRange.levelCount = t->desc.mip_levels;
		m.subresourceRange.layerCount = t->desc.array_layers;
		memset(&d, 0, sizeof(d));
		d.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		d.imageMemoryBarrierCount = 1;
		d.pImageMemoryBarriers = &m;
		vkCmdPipelineBarrier2(c->command, &d);
	}
	return 1;
}
int
rend_cmd_begin_render(RendCommandList c, const RendRenderDesc *x)
{
	VkRenderingAttachmentInfo colors[1], depth;
	VkRenderingInfo info;
	uint32_t i;
	if (!rend_list_recording(c) || c->in_render || !x || !x->width ||
	    !x->height || !x->colors || x->color_count != 1 || !x->depth_attachment)
		return rend_command_fail(c);
	c->has_depth = 0;
	c->depth_target = NULL;
	c->color_target = NULL;
	c->viewport_set = c->scissor_set = c->depth_state_set = 0;
	for (i = 0; i < x->color_count; i++)
	{
		RendTextureViewRecord *v = x->colors[i].view;
		if (!v || !v->live || v->backend != c->backend ||
		    v->desc.aspect != REND_ASPECT_COLOR || x->width > v->width ||
		    x->height > v->height ||
		    (!v->external &&
		     !(v->texture->desc.usage & REND_TEXTURE_COLOR_ATTACHMENT)) ||
		    !rend_ref(c, REND_REF_VIEW, v))
			return rend_command_fail(c);
		if (v->external)
		{
			if (!v->presentation || !v->presentation->acquired ||
			    (c->presentation && c->presentation != v->presentation))
				return rend_command_fail(c);
			c->presentation = v->presentation;
			rend_swap_transition(c, v->presentation, VK_IMAGE_LAYOUT_GENERAL);
		} else
		{
			rend_activate(c, v->texture);
			if (!rend_ref(c, REND_REF_TEXTURE, v->texture))
				return rend_command_fail(c);
		}
		memset(&colors[i], 0, sizeof(colors[i]));
		colors[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		colors[i].imageView = v->view;
		colors[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		colors[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colors[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		for (uint32_t channel = 0; channel < 4; channel++)
			if (!isfinite(x->colors[i].clear[channel]))
				return rend_command_fail(c);
		memcpy(colors[i].clearValue.color.float32, x->colors[i].clear,
		       sizeof(x->colors[i].clear));
	}
	if (x->depth_attachment)
	{
		RendTextureViewRecord *v = x->depth_attachment->view;
		if (!v || !v->live || v->backend != c->backend ||
		    v->desc.aspect != REND_ASPECT_DEPTH || v->external ||
		    v->format != REND_FORMAT_D32_FLOAT ||
		    !(v->texture->desc.usage & REND_TEXTURE_DEPTH_ATTACHMENT) ||
		    x->width > v->width || x->height > v->height ||
		    !rend_ref(c, REND_REF_VIEW, v) ||
		    !rend_ref(c, REND_REF_TEXTURE, v->texture))
			return rend_command_fail(c);
		rend_activate(c, v->texture);
		memset(&depth, 0, sizeof(depth));
		depth.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		depth.imageView = v->view;
		depth.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		if (!isfinite(x->depth_attachment->clear_depth) ||
		    x->depth_attachment->clear_depth < 0.0f ||
		    x->depth_attachment->clear_depth > 1.0f)
			return rend_command_fail(c);
		depth.clearValue.depthStencil.depth = x->depth_attachment->clear_depth;
		c->depth_target = v->texture;
		c->has_depth = 1;
	}
	memset(&info, 0, sizeof(info));
	info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	info.renderArea.extent.width = x->width;
	info.renderArea.extent.height = x->height;
	info.layerCount = 1;
	info.colorAttachmentCount = x->color_count;
	info.pColorAttachments = colors;
	info.pDepthAttachment = x->depth_attachment ? &depth : NULL;
	vkCmdBeginRendering(c->command, &info);
	c->in_render = 1;
	c->color_target = x->colors[0].view->texture;
	c->active_color_format = x->colors[0].view->format;
	c->render_width = x->width;
	c->render_height = x->height;
	return 1;
}
int
rend_cmd_end_render(RendCommandList c)
{
	if (!rend_list_recording(c) || !c->in_render)
		return rend_command_fail(c);
	vkCmdEndRendering(c->command);
	if (c->presentation)
		rend_swap_transition(c, c->presentation, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	c->in_render = 0;
	return 1;
}
int
rend_cmd_set_pipeline(RendCommandList c, RendPipeline p)
{
	if (!rend_list_recording(c) || !c->in_render || !p || !p->live ||
	    p->backend != c->backend || p->format != c->active_color_format ||
	    !c->has_depth || p->depth_format != REND_FORMAT_D32_FLOAT ||
	    !rend_ref(c, REND_REF_PIPELINE, p))
		return rend_command_fail(c);
	vkCmdBindPipeline(c->command, VK_PIPELINE_BIND_POINT_GRAPHICS, p->pipeline);
	c->pipeline = p;
	return 1;
}
int
rend_cmd_set_viewport(RendCommandList c, const RendViewport *v)
{
	VkViewport n;
	if (!rend_list_recording(c) || !c->in_render || !v || !isfinite(v->x) ||
	    !isfinite(v->y) || !isfinite(v->width) || !isfinite(v->height) ||
	    !isfinite(v->min_depth) || !isfinite(v->max_depth) || v->width <= 0 ||
	    v->height <= 0 || v->x < 0 || v->y < 0 ||
	    v->x + v->width > c->render_width ||
	    v->y + v->height > c->render_height || v->min_depth < 0 ||
	    v->max_depth > 1 || v->min_depth > v->max_depth)
		return rend_command_fail(c);
	n = (VkViewport){v->x, v->y, v->width, v->height, v->min_depth, v->max_depth};
	vkCmdSetViewport(c->command, 0, 1, &n);
	c->viewport_set = 1;
	return 1;
}
int
rend_cmd_set_scissor(RendCommandList c, const RendScissor *s)
{
	VkRect2D n;
	if (!rend_list_recording(c) || !c->in_render || !s || s->x < 0 || s->y < 0 ||
	    !s->width || !s->height || (uint64_t)s->x + s->width > c->render_width ||
	    (uint64_t)s->y + s->height > c->render_height)
		return rend_command_fail(c);
	n.offset.x = s->x;
	n.offset.y = s->y;
	n.extent.width = s->width;
	n.extent.height = s->height;
	vkCmdSetScissor(c->command, 0, 1, &n);
	c->scissor_set = 1;
	return 1;
}
static VkCompareOp
rend_compare(RendCompareOp c)
{
	if (c == REND_COMPARE_NONE || c == REND_COMPARE_ALWAYS)
		return VK_COMPARE_OP_ALWAYS;
	if (c == REND_COMPARE_LESS)
		return VK_COMPARE_OP_LESS;
	return VK_COMPARE_OP_MAX_ENUM;
}
int
rend_cmd_set_depth_state(RendCommandList c, const RendDepthState *s)
{
	VkCompareOp op;
	if (!rend_list_recording(c) || !c->in_render || !s || s->test_enable > 1 ||
	    s->write_enable > 1 ||
	    (op = rend_compare(s->compare)) == VK_COMPARE_OP_MAX_ENUM ||
	    (s->compare == REND_COMPARE_NONE && s->test_enable))
		return rend_command_fail(c);
	vkCmdSetDepthTestEnable(c->command, s->test_enable);
	vkCmdSetDepthWriteEnable(c->command, s->write_enable);
	vkCmdSetDepthCompareOp(c->command, op);
	c->depth_state_set = 1;
	return 1;
}
int
rend_cmd_draw_indexed(RendCommandList c, uint64_t root, uint64_t index,
		      RendIndexType type, uint32_t count,
		      uint32_t instances, uint32_t first, int32_t offset,
		      uint32_t first_instance)
{
	VkBindIndexBuffer3InfoKHR bind;
	VkPushDataInfoEXT push;
	VkDeviceAddress address = root;
	VkIndexType index_type;
	uint32_t width;
	if (!rend_list_recording(c) || !c->in_render || !c->pipeline || !root ||
	    root % c->backend->root_alignment || !index || !count || !instances ||
	    !c->viewport_set || !c->scissor_set || !c->depth_state_set ||
	    (type != REND_INDEX_UINT16 && type != REND_INDEX_UINT32) ||
	    first > UINT32_MAX - count)
		return rend_command_fail(c);
	width = type == REND_INDEX_UINT16 ? 2 : 4;
	if (index % width ||
	    (uint64_t)first + count > (UINT64_MAX - index) / width)
		return rend_command_fail(c);
	index_type =
		type == REND_INDEX_UINT16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
	memset(&bind, 0, sizeof(bind));
	bind.sType = VK_STRUCTURE_TYPE_BIND_INDEX_BUFFER_3_INFO_KHR;
	bind.addressRange.address = index + (VkDeviceAddress)first * width;
	bind.addressRange.size = (VkDeviceSize)count * width;
	bind.addressFlags = VK_ADDRESS_COMMAND_FULLY_BOUND_BIT_KHR | VK_ADDRESS_COMMAND_STORAGE_BUFFER_USAGE_BIT_KHR;
	bind.indexType = index_type;
	c->backend->bind_index(c->command, &bind);
	memset(&push, 0, sizeof(push));
	push.sType = VK_STRUCTURE_TYPE_PUSH_DATA_INFO_EXT;
	push.data.address = &address;
	push.data.size = sizeof(address);
	c->backend->push_data(c->command, &push);
	vkCmdDrawIndexed(c->command, count, instances, 0, offset, first_instance);
	return 1;
}
int
rend_cmd_copy_texture_to_data(RendCommandList c, RendTexture t,
			      RendHeapRange dst, uint32_t width,
			      uint32_t height)
{
	VkDeviceMemoryImageCopyKHR region;
	VkCopyDeviceMemoryImageInfoKHR copy;
	RendHeapRecord *h = dst.heap;
	uint64_t bytes;
	if (!rend_list_recording(c) || c->in_render || !t || !t->live ||
	    t->backend != c->backend || !h || h->backend != c->backend || !h->live ||
	    h->kind != REND_HEAP_DATA || t->desc.format != REND_FORMAT_RGBA8_UNORM ||
	    !(t->desc.usage & REND_TEXTURE_TRANSFER_SRC) || !width || !height ||
	    width > t->desc.width || height > t->desc.height || dst.offset % 4 ||
	    width > UINT64_MAX / height / 4)
		return rend_command_fail(c);
	bytes = (uint64_t)width * height * 4;
	if (dst.offset > h->size || bytes > h->size - dst.offset ||
	    dst.size < bytes || !h->address ||
	    dst.offset > UINT64_MAX - h->address ||
	    bytes > UINT64_MAX - h->address - dst.offset ||
	    !rend_ref(c, REND_REF_TEXTURE, t) ||
	    !rend_ref(c, REND_REF_HEAP, h))
		return rend_command_fail(c);
	memset(&region, 0, sizeof(region));
	region.sType = VK_STRUCTURE_TYPE_DEVICE_MEMORY_IMAGE_COPY_KHR;
	region.addressRange.address = h->address + dst.offset;
	region.addressRange.size = bytes;
	region.addressFlags = VK_ADDRESS_COMMAND_FULLY_BOUND_BIT_KHR |
			      VK_ADDRESS_COMMAND_STORAGE_BUFFER_USAGE_BIT_KHR;
	region.addressRowLength = width;
	region.addressImageHeight = height;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.layerCount = 1;
	region.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	region.imageExtent.width = width;
	region.imageExtent.height = height;
	region.imageExtent.depth = 1;
	memset(&copy, 0, sizeof(copy));
	copy.sType = VK_STRUCTURE_TYPE_COPY_DEVICE_MEMORY_IMAGE_INFO_KHR;
	copy.image = t->image;
	copy.regionCount = 1;
	copy.pRegions = &region;
	c->backend->copy_from_image(c->command, &copy);
	return 1;
}
int
rend_timeline_create(RendQueue q, uint64_t initial, RendTimeline *out)
{
	uint32_t i;
	RendTimelineRecord *t = NULL;
	VkSemaphoreTypeCreateInfo ti;
	VkSemaphoreCreateInfo ci;
	VkResult r;
	if (out)
		*out = NULL;
	if (!out || !q || !q->live || q->backend->terminal)
		return 0;
	for (i = 0; i < q->backend->timeline_count; i++)
		if (!q->backend->timelines[i].live)
		{
			t = &q->backend->timelines[i];
			break;
		}
	if (!t)
	{
		rend_vk_set_diagnostic(REND_DIAG_CAPACITY, 0,
				       "timeline capacity exhausted");
		return 0;
	}
	memset(&ti, 0, sizeof(ti));
	ti.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
	ti.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
	ti.initialValue = initial;
	memset(&ci, 0, sizeof(ci));
	ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	ci.pNext = &ti;
	r = vkCreateSemaphore(q->backend->device, &ci, NULL, &t->semaphore);
	if (r != VK_SUCCESS)
	{
		rend_vk_set_diagnostic(REND_DIAG_NATIVE, r,
				       "vkCreateSemaphore timeline failed");
		return 0;
	}
	t->backend = q->backend;
	t->producer = q;
	t->last_signal = initial;
	t->live = 1;
	*out = t;
	return 1;
}
int
rend_timeline_query(RendTimeline t, uint64_t *out)
{
	uint32_t i;
	if (!out || !t || !t->live)
		return 0;
	if (vkGetSemaphoreCounterValue(t->backend->device, t->semaphore, out) !=
	    VK_SUCCESS)
		return 0;
	for (i = 0; i < t->backend->queue_count; i++)
		rend_queue_reap(&t->backend->queues[i]);
	return 1;
}
int
rend_timeline_wait(RendTimeline t, uint64_t value, uint64_t timeout)
{
	VkSemaphoreWaitInfo wi;
	VkResult r;
	if (!t || !t->live)
		return 0;
	memset(&wi, 0, sizeof(wi));
	wi.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
	wi.semaphoreCount = 1;
	wi.pSemaphores = &t->semaphore;
	wi.pValues = &value;
	r = vkWaitSemaphores(t->backend->device, &wi, timeout);
	if (r == VK_TIMEOUT)
	{
		rend_vk_set_diagnostic(REND_DIAG_TIMEOUT, r, "timeline wait timed out");
		return 0;
	}
	if (r != VK_SUCCESS)
	{
		t->backend->terminal = 1;
		rend_vk_set_diagnostic(REND_DIAG_TERMINAL, r,
				       "timeline wait native/device failure");
		return 0;
	}
	{
		uint32_t i;
		for (i = 0; i < t->backend->queue_count; i++)
			rend_queue_reap(&t->backend->queues[i]);
	}
	return 1;
}
int
rend_timeline_destroy(RendTimeline t)
{
	RendBackend b;
	uint32_t i, j;
	if (!t || !t->live || !(b = t->backend) || b->magic != REND_BACKEND_MAGIC)
		return 0;
	for (i = 0; i < b->queue_count; i++)
	{
		uint32_t k;
		rend_queue_reap(&b->queues[i]);
		for (k = 0; k < b->queues[i].submission_capacity; k++)
		{
			RendQueueSubmission *s = &b->queues[i].submissions[k];
			uint32_t w;
			if (!s->live)
				continue;
			if (s->timeline == t)
				return 0;
			for (w = 0; w < s->wait_count; w++)
				if (b->queues[i].wait_refs[k * b->queues[i].max_waits + w].timeline ==
				    t)
					return 0;
		}
	}
	for (i = 0; i < b->presentation_count; i++)
	{
		uint32_t k;
		RendPresentationRecord *p = &b->presentations[i];
		if (!p->live)
			continue;
		for (k = 0; k < 8; k++)
			if (p->acquire_timeline[k] == t)
			{
				uint64_t done = 0;
				if (vkGetSemaphoreCounterValue(b->device, t->semaphore, &done) !=
					    VK_SUCCESS ||
				    done < p->acquire_values[k])
					return 0;
				p->acquire_timeline[k] = NULL;
			}
	}
	for (i = 0; i < b->pool_count; i++)
		for (j = 0; j < b->pools[i].count; j++)
			if (b->pools[i].commands[j].completion == t &&
			    b->pools[i].commands[j].state)
				return 0;
	vkDestroySemaphore(b->device, t->semaphore, NULL);
	memset(t, 0, sizeof(*t));
	return 1;
}
int
rend_queue_submit(RendQueue q, const RendSubmitDesc *d)
{
	RendBackend b;
	uint32_t i, j, slot_index = UINT32_MAX;
	VkCommandBufferSubmitInfo *cmds;
	VkSemaphoreSubmitInfo *waits, *signals;
	VkSubmitInfo2 submit;
	VkResult r;
	RendTimeline completion;
	RendTimelinePoint point;
	RendPresentationRecord *presentation = NULL;
	uint32_t wait_count, signal_count;
	if (!q || !q->live || !d || (b = q->backend)->magic != REND_BACKEND_MAGIC ||
	    b->terminal || !d->commands || !d->command_count ||
	    d->command_count > q->max_submit_lists || d->wait_count > q->max_waits ||
	    (!d->waits && d->wait_count) || !d->completion.timeline)
		return 0;
	completion = d->completion.timeline;
	point = d->completion;
	if (!completion->live || completion->backend != b ||
	    completion->producer != q || point.value <= completion->last_signal)
		return 0;
	VkCommandBufferSubmitInfo cmd_storage[64];
	VkSemaphoreSubmitInfo wait_storage[64], signal_storage[2];
	if (d->command_count > 64 || d->wait_count > 64)
		return 0;
	cmds = cmd_storage;
	waits = wait_storage;
	signals = signal_storage;
	rend_queue_reap(q);
	for (i = 0; i < q->submission_capacity; i++)
		if (!q->submissions[i].live)
		{
			slot_index = i;
			break;
		}
	if (slot_index == UINT32_MAX)
		return 0;
	for (i = 0; i < d->command_count; i++)
	{
		RendCommandRecord *c = d->commands[i];
		if (!c || c->backend != b || c->state != 2 || c->pool->family != q->family)
			return 0;
		if (c->presentation)
		{
			if ((presentation && presentation != c->presentation) ||
			    !c->presentation->acquired || c->presentation->submitted)
				return 0;
			presentation = c->presentation;
		}
		memset(&cmds[i], 0, sizeof(cmds[i]));
		cmds[i].sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
		cmds[i].commandBuffer = c->command;
		for (j = 0; j < i; j++)
			if (d->commands[j] == c)
				return 0;
	}
	wait_count = d->wait_count + (presentation ? 1u : 0u);
	signal_count = 1 + (presentation ? 1u : 0u);
	if (wait_count > 64 || d->wait_count > q->max_waits)
		return 0;
	for (i = 0; i < d->wait_count; i++)
	{
		RendTimeline t = d->waits[i].point.timeline;
		if (!t || !t->live || t->backend != b || t == completion ||
		    !d->waits[i].consumer_stages ||
		    (d->waits[i].consumer_stages &
		     ~(REND_STAGE_TRANSFER | REND_STAGE_COMPUTE | REND_STAGE_INDEX_INPUT |
		       REND_STAGE_VERTEX | REND_STAGE_FRAGMENT | REND_STAGE_COLOR_OUTPUT |
		       REND_STAGE_INDIRECT | REND_STAGE_GRAPHICS)))
			return 0;
		for (j = 0; j < i; j++)
			if (d->waits[j].point.timeline == t)
				return 0;
		memset(&waits[i], 0, sizeof(waits[i]));
		waits[i].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		waits[i].semaphore = t->semaphore;
		waits[i].value = d->waits[i].point.value;
		waits[i].stageMask = rend_vk_stage(d->waits[i].consumer_stages);
	}
	if (presentation)
	{
		memset(&waits[d->wait_count], 0, sizeof(waits[0]));
		waits[d->wait_count].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		waits[d->wait_count].semaphore =
			presentation->acquire_semaphores[presentation->acquire_slot];
		waits[d->wait_count].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	}
	memset(signal_storage, 0, sizeof(signal_storage));
	signals[0].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	signals[0].semaphore = completion->semaphore;
	signals[0].value = point.value;
	signals[0].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	if (presentation)
	{
		signals[1].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		signals[1].semaphore =
			presentation->render_semaphores[presentation->image_index];
		signals[1].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	}
	memset(&submit, 0, sizeof(submit));
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submit.waitSemaphoreInfoCount = wait_count;
	submit.pWaitSemaphoreInfos = waits;
	submit.commandBufferInfoCount = d->command_count;
	submit.pCommandBufferInfos = cmds;
	submit.signalSemaphoreInfoCount = signal_count;
	submit.pSignalSemaphoreInfos = signals;
	b->drained = 0;
	r = vkQueueSubmit2(q->queue, 1, &submit, VK_NULL_HANDLE);
	if (r != VK_SUCCESS)
	{
		RendQueueSubmission *slot = &q->submissions[slot_index];
		memset(slot, 0, sizeof(*slot));
		slot->live = 1;
		slot->uncertain = 1;
		slot->timeline = completion;
		slot->value = point.value;
		slot->wait_count = d->wait_count;
		for (i = 0; i < d->wait_count; i++)
		{
			q->wait_refs[slot_index * q->max_waits + i].timeline =
				d->waits[i].point.timeline;
			q->wait_refs[slot_index * q->max_waits + i].value =
				d->waits[i].point.value;
		}
		q->active_submissions++;
		for (i = 0; i < d->command_count; i++)
		{
			RendCommandRecord *c = d->commands[i];
			c->state = 3;
			c->completion = completion;
			c->completion_value = point.value;
		}
		b->terminal = 1;
		rend_vk_set_diagnostic(REND_DIAG_TERMINAL, r,
				       "native queue submission failure; context terminal "
				       "and backing retained");
		return 0;
	}
	completion->last_signal = point.value;
	if (presentation)
	{
		presentation->submitted = 1;
		presentation->acquire_timeline[presentation->acquire_slot] = completion;
		presentation->acquire_values[presentation->acquire_slot] = point.value;
	}
	{
		RendQueueSubmission *slot = &q->submissions[slot_index];
		memset(slot, 0, sizeof(*slot));
		slot->live = 1;
		slot->timeline = completion;
		slot->value = point.value;
		slot->wait_count = d->wait_count;
		for (i = 0; i < d->wait_count; i++)
		{
			q->wait_refs[slot_index * q->max_waits + i].timeline =
				d->waits[i].point.timeline;
			q->wait_refs[slot_index * q->max_waits + i].value =
				d->waits[i].point.value;
		}
		q->active_submissions++;
	}
	for (i = 0; i < d->command_count; i++)
	{
		RendCommandRecord *c = d->commands[i];
		c->state = 3;
		c->completion = completion;
		c->completion_value = point.value;
	}
	return 1;
}

int
rend_backend_wait_idle(RendBackend b)
{
	uint32_t i, j;
	VkResult r;
	if (!b || b->magic != REND_BACKEND_MAGIC)
		return 0;
	r = vkDeviceWaitIdle(b->device);
	if (r != VK_SUCCESS)
	{
		rend_vk_set_diagnostic(
			REND_DIAG_NATIVE, r,
			"explicit vkDeviceWaitIdle failed; retain native backing");
		return 0;
	}
	b->drained = 1;
	for (i = 0; i < b->presentation_count; i++)
	{
		uint32_t k;
		for (k = 0; k < 8; k++)
			b->presentations[i].acquire_timeline[k] = NULL;
	}
	for (i = 0; i < b->pool_count; i++)
		for (j = 0; j < b->pools[i].count; j++)
			if (b->pools[i].commands[j].state == 3)
				b->pools[i].commands[j].state = 4;
	for (i = 0; i < b->queue_count; i++)
	{
		b->queues[i].active_submissions = 0;
		for (j = 0; j < b->queues[i].submission_capacity; j++)
			b->queues[i].submissions[j].live = 0;
	}
	return 1;
}

int
rend_backend_destroy(RendBackend b)
{
	uint32_t i, j;
	if (!b || b->magic != REND_BACKEND_MAGIC)
		return 0;
	for (i = 0; i < b->queue_count; i++)
		rend_queue_reap(&b->queues[i]);
	for (i = 0; i < b->pool_count; i++)
		for (j = 0; j < b->pools[i].count; j++)
			if (b->pools[i].commands[j].state)
			{
				rend_vk_set_diagnostic(
					REND_DIAG_INVALID_ARGUMENT, 0,
					"command pools must be reset before backend destruction");
				return 0;
			}
	for (i = 0; i < b->heap_count; i++)
		if (b->heaps[i].live)
			return 0;
	for (i = 0; i < b->texture_count; i++)
		if (b->textures[i].live)
			return 0;
	for (i = 0; i < b->view_count; i++)
		if (b->views[i].live)
			return 0;
	for (i = 0; i < b->pipeline_count; i++)
		if (b->pipelines[i].live)
			return 0;
	for (i = 0; i < b->timeline_count; i++)
		if (b->timelines[i].live)
			return 0;
	for (i = 0; i < b->presentation_count; i++)
		if (b->presentations[i].live)
			return 0;
	for (i = 0; i < b->queue_count; i++)
		if (b->queues[i].active_submissions)
		{
			rend_vk_set_diagnostic(
				REND_DIAG_INVALID_ARGUMENT, 0,
				"backend has pending work; call explicit wait_idle first");
			return 0;
		}
	for (i = 0; i < b->view_count; i++)
		if (b->views[i].live)
			vkDestroyImageView(b->device, b->views[i].view, NULL);
	for (i = 0; i < b->texture_count; i++)
		if (b->textures[i].live)
			vkDestroyImage(b->device, b->textures[i].image, NULL);
	for (i = 0; i < b->pipeline_count; i++)
		if (b->pipelines[i].live)
			vkDestroyPipeline(b->device, b->pipelines[i].pipeline, NULL);
	for (i = 0; i < b->timeline_count; i++)
		if (b->timelines[i].live)
			vkDestroySemaphore(b->device, b->timelines[i].semaphore, NULL);
	for (i = 0; i < b->heap_count; i++)
		if (b->heaps[i].live)
		{
			RendHeapRecord *h = &b->heaps[i];
			if (h->mapped)
				vkUnmapMemory(b->device, h->memory);
			if (h->buffer)
				vkDestroyBuffer(b->device, h->buffer, NULL);
			vkFreeMemory(b->device, h->memory, NULL);
		}
	for (i = 0; i < b->pool_count; i++)
		if (b->pools[i].pool)
			vkDestroyCommandPool(b->device, b->pools[i].pool, NULL);
	vkDestroyDevice(b->device, NULL);
	if (b->device_info->owner && b->device_info->owner->context_count)
		b->device_info->owner->context_count--;
	b->magic = 0;
	b->device = VK_NULL_HANDLE;
	return 1;
}

#endif /* REND2_IMPLEMENTATION && !REND2_IMPLEMENTATION_INCLUDED */

/*
------------------------------------------------------------------------------
MIT License
Copyright (c) 2026 Vasco Alves
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
*/
