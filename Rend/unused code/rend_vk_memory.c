#pragma once

#include "rend_internal.h"
#include "rend_vk_internal.h"
#include <vulkan/vulkan_core.h>


#if 0
static RendVkMemory
rend_vk_memory_malloc(size_t size, VkDevice logical_device, VkPhysicalDevice physical_device, uint32_t heap_index, VkAllocationCallbacks *allocator)
{
    RendVkMemory memory = {
        .host_mapped_memory = NULL,
        .device_memory = 0,
        .logical_device = logical_device,
        .heap_index = heap_index
    };

    VkMemoryAllocateFlagsInfo flags_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .pNext = NULL,
        .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT, /* REQUIRED for BDA buffers */
        .deviceMask = 0
    };

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = size,
        .pNext = &flags_info,
        .memoryTypeIndex = heap_index
    };

    vkAllocateMemory(logical_device, &alloc_info, allocator, &memory.device_memory);
    return memory;
}

static void
rend_vk_memory_free(RendVkMemory *memory)
{
    if (memory->host_mapped_memory) {
        rend_vk_memory_unmap(memory);
    }
    if (memory->offset == 0) {
        vkFreeMemory(memory->logical_device, memory->device_memory, memory->allocator);
        memset(memory, 0, sizeof *memory);
    } else {
        PWARN("[REND_VK] Attempted to free memory with an offset!");
    }
}

static void*
rend_vk_memory_map(RendVkMemory *memory)
{
    // SPEC: memory must have been created with a memory type that reports VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
    // vkMapMemory will fail if the implementation is unable to allocate an appropriately sized contiguous virtual address range,
    // e.g. due to virtual address space fragmentation or platform limits.
    // In such cases, vkMapMemory must return VK_ERROR_MEMORY_MAP_FAILED.
    // The application can improve the likelihood of success by reducing the size of the mapped range and/or removing unneeded mappings using vkUnmapMemory.

    vkMapMemory(memory->logical_device, memory->device_memory, memory->offset, memory->size, 0, &memory->host_mapped_memory);
    return memory->host_mapped_memory;
}

static void
rend_vk_memory_unmap(RendVkMemory *memory) 
{
    if (memory->offset == 0) {
        vkUnmapMemory(memory->logical_device, memory->device_memory);
        memory->host_mapped_memory = 0;
    } else {
        PWARN("[REND_VK] Attempted to unmap memory with an offset!");
    }
}

static void
rend_vk_memory_copy(RendVkMemory *memory, size_t offset, const void *data, size_t size)
{
    /* bounds check */
    if (offset + size > memory->size) {
        REND__WARN("Memory write out of bounds!");
        return;
    }

    /* important to cast this */
    uint8_t *dest = memory->host_mapped_memory;
    memcpy(dest + offset, data, size);

    /* flush host writes if memory is non-coherent */
    // VkMappedMemoryRange range = {
    //     .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
    //     .memory = memory->device_memory,
    //     .offset = offset,
    //     .size = size
    // };
    //
    // vkFlushMappedMemoryRanges(memory->logical_device, 1, &range);

}

#endif
