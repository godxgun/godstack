#pragma once
#include "rend_internal.h"
#include <stdint.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

typedef struct RendVkAllocatorState {
    const char *name;
} RendVkAllocatorState;

typedef struct RendVkAllocatorHeader {
    uint32_t offset; 
} RendVkAllocatorHeader;

static bool rend_vk_allocator_is_power_of_two(uintptr_t x);
static void *rend_vk_allocator_alloc(void *pUserData, size_t size, size_t alignment, VkSystemAllocationScope allocationScope);
static void *rend_vk_allocator_realloc(void *pUserData, void *pOriginal, size_t size, size_t alignment, VkSystemAllocationScope allocationScope);
static void rend_vk_allocator_free(void *pUserData, void *pMemory);
static void rend_vk_allocator_internal_notification(void *pUserData, size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope);
static void rend_vk_allocator_free_notification(void *pUserData, size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope);

static VkAllocationCallbacks rend_vk_allocator = {
    .pfnAllocation = rend_vk_allocator_alloc,
    .pfnReallocation = rend_vk_allocator_realloc,
    .pfnFree = rend_vk_allocator_free,
    .pfnInternalAllocation = rend_vk_allocator_internal_notification,
    .pfnInternalFree = rend_vk_allocator_free_notification,
};

static const char* rend_vk_allocator_scope_name[] = {
    [VK_SYSTEM_ALLOCATION_SCOPE_CACHE] = "Cache",
    [VK_SYSTEM_ALLOCATION_SCOPE_COMMAND] = "Command",
    [VK_SYSTEM_ALLOCATION_SCOPE_DEVICE] = "Device",
    [VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE] = "Instance",
    [VK_SYSTEM_ALLOCATION_SCOPE_OBJECT] = "Object",
};


static bool
rend_vk_allocator_is_power_of_two(uintptr_t x) 
{
	return (x & (x-1)) == 0;
}

static uintptr_t
rend_vk_allocator_align_forward(uintptr_t ptr, size_t align) 
{
	uintptr_t p, a, modulo;

	assert(rend_vk_allocator_is_power_of_two(align));

	p = ptr;
	a = (uintptr_t)align;
	modulo = p & (a-1);

	if (modulo != 0) {
		p += a - modulo;
	}
	return p;
}

static void* 
rend_vk_allocator_alloc(void *pUserData, size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
{
    size_t total_size = size + alignment + sizeof (RendVkAllocatorHeader);
    uint8_t *raw_ptr = malloc(total_size);
    uintptr_t unaligned_addr = (uintptr_t)(raw_ptr + sizeof (RendVkAllocatorHeader));

    uint8_t *aligned_ptr = (uint8_t*)rend_vk_allocator_align_forward(unaligned_addr, alignment);
    RendVkAllocatorHeader *header = (RendVkAllocatorHeader*) aligned_ptr - 1;
    header->offset = aligned_ptr - raw_ptr;

    PDEBUG("[VK_ALLOC] %p - bytes %lu with alignment %lu - scope %s",
            aligned_ptr, size, alignment, rend_vk_allocator_scope_name[allocationScope]);

    return (void*) aligned_ptr;
}

static void*
rend_vk_allocator_realloc(void *pUserData, void *pOriginal, size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
{
    if (!pOriginal) return rend_vk_allocator_alloc(pUserData, size, alignment, allocationScope);
    if (size == 0) {
        rend_vk_allocator_free(pUserData, pOriginal);
        return NULL;
    }
    void *new_ptr = rend_vk_allocator_alloc(pUserData, size, alignment, allocationScope);
    if (!new_ptr) return NULL;
    memcpy(new_ptr, pOriginal, size);
    rend_vk_allocator_free(pUserData, pOriginal);
    return new_ptr;
}

static void
rend_vk_allocator_free(void *pUserData, void *pMemory)
{
    RendVkAllocatorHeader *ptr = pMemory;
    uint8_t *raw_ptr = pMemory;
    raw_ptr -= (ptr-1)->offset;

    PDEBUG("[VK_FREE] %p", ptr);
    free(raw_ptr);
}

static void
rend_vk_allocator_internal_notification(void *pUserData, size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope)
{
    PDEBUG("[VK_ALLOC_INTERNAL] bytes %lu - scope %s",
            size, rend_vk_allocator_scope_name[allocationScope]);

}

static void
rend_vk_allocator_free_notification(void *pUserData, size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope)
{
    PDEBUG("[VK_FREE_INTERNAL] bytes %lu - scope %s",
            size, rend_vk_allocator_scope_name[allocationScope]);
}
