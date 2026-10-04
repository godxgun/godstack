/* Rend 2 bounded host-side bootstrap. Copyright (c) 2026, MIT license. */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "rend.h"
#include "rend_internal.h"

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

/* Rend is a unity module: bootstrap implementation is compiled exactly once
 * here. */
#include "rend_backend_vulkan.c"
#include "rend_memory.c"
#include "rend_pipeline.c"
#include "rend_presentation.c"
#include "rend_texture.c"
#include "rend_work.c"
