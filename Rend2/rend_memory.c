/* Explicit native GPU heaps; no per-range host records. */
#include "rend_internal.h"
#include <string.h>

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
