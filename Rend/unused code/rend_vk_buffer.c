#pragma once
#include "rend_internal.h"
#include "rend_vk_internal.h"
#include <vulkan/vulkan_core.h>

#if 0
static RendBuffer
rend_vk_buffer_create(VkDevice logical_device, VkAllocationCallbacks *allocator, VkDeviceSize size, VkBufferUsageFlags usage, uint32_t *family_indices, uint32_t family_indices_count)
{
    RendBuffer buffer = {
        .handle = 0,
        .logical_device = logical_device,
        .memory = NULL,
        .allocator = allocator,
        .usage = usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    };

    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = (family_indices_count > 1) ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = family_indices_count,
        .pQueueFamilyIndices = family_indices
    };

    vkCreateBuffer(logical_device, &buffer_info, allocator, &buffer.handle);
    return buffer;
}

static uint32_t
rend_vk_buffer_required_memory_type(RendBuffer *buffer) 
{
    assert(buffer->memory == NULL && "Buffer already bound to memory");
    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(buffer->logical_device, buffer->handle, &mem_requirements);
    return mem_requirements.memoryTypeBits;
}

static void
rend_vk_buffer_bind_memory(RendBuffer *buffer, RendVkMemory *memory) 
{
    assert(buffer->memory == NULL && "Buffer already bound to memory");
    vkBindBufferMemory(buffer->logical_device, buffer->handle, memory->device_memory, 0);
    buffer->memory = memory;
}

static void
rend_vk_buffer_destroy(RendBuffer *buffer)
{
    assert(buffer && buffer->handle != 0);
    vkDestroyBuffer(buffer->logical_device, buffer->handle, buffer->allocator);
    buffer->handle = 0;
    buffer->memory = 0;
}

static void 
rend_vk_buffer_copy_device(VkQueue queue, VkCommandPool pool, RendBuffer *dest, size_t dest_offset, size_t bytes, RendBuffer *src, size_t src_offset, VkFence fence) 
{
    assert(src && dest); // check that im not sending null pointers
    assert(src->usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT); // source buffer must be marked as transfer src
    assert(dest->usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT); // dest buffer must be marked as transfer dest

    VkCommandBuffer transfer_cmd = VK_NULL_HANDLE;

    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandBufferCount = 1,
        .commandPool = pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .pNext = NULL,
    };

    vkAllocateCommandBuffers(dest->logical_device, &alloc_info, &transfer_cmd);

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(transfer_cmd, &begin_info);

    VkBufferCopy buffer_copy = {
        .srcOffset = (VkDeviceSize)src_offset,
        .dstOffset = (VkDeviceSize)dest_offset,
        .size      = (VkDeviceSize)bytes,
    };

    vkCmdCopyBuffer(transfer_cmd, src->handle, dest->handle, 1, &buffer_copy);
    vkEndCommandBuffer(transfer_cmd);

    VkCommandBufferSubmitInfo cmd_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = transfer_cmd,
        .deviceMask = 0,
    };

    VkSubmitInfo2 submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &cmd_info,
    };

    vkQueueSubmit2(queue, 1, &submit_info, fence);
    vkQueueWaitIdle(queue);
    vkFreeCommandBuffers(dest->logical_device, pool, 1, &transfer_cmd);
}

static uint64_t
rend_vk_buffer_address(RendBuffer *buffer, VkDevice device)
{
    VkBufferDeviceAddressInfoKHR address_info = {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR};
    address_info.buffer = buffer->handle;
    VkDeviceAddress address = vkGetBufferDeviceAddress(device, &address_info);
    return (uint64_t) address;
}
#endif
