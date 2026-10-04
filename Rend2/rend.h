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
