#pragma once
#include "rend_internal.h"
#include "rend_vk_internal.h"
#include <vulkan/vulkan_core.h>
#include <stdatomic.h>
#include <stdlib.h>

/* DEFERRED COMMANDS:
 * Tracks single-use command buffers submitted for async GPU work,
 * so callers don't have to block (vkQueueWaitIdle) to know when it's
 * safe to free/reset them. One shared timeline semaphore; every
 * submission claims the next monotonic value.
 */

struct RendVkPendingCmd {
    VkCommandPool   pool;
    VkCommandBuffer cmd;
    uint64_t        wait_value;
};

struct RendVkDeferredCmds {
    VkSemaphore        timeline;
    RendVkPendingCmd  *darray;
    size_t             count;
    size_t             capacity;
};

#define RENDER_VK_DEFERRED_CMDS_INITIAL_CAPACITY 16

static inline RendVkDeferredCmds
rend_vk_cmdbuf_deferred_create(void)
{
    RendVkDeferredCmds tracker = {0};

    VkSemaphoreTypeCreateInfo type_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
        .initialValue = 0,
    };
    VkSemaphoreCreateInfo sem_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &type_info,
    };
    vkCreateSemaphore(vk_device.logical_device, &sem_info, vk_allocator, &tracker.timeline);

    tracker.capacity = RENDER_VK_DEFERRED_CMDS_INITIAL_CAPACITY;
    tracker.darray = rmalloc(sizeof(RendVkPendingCmd) * tracker.capacity);
    tracker.count = 0;

    return tracker;
}

static inline void
rend_vk_cmdbuf_deferred_destroy(RendVkDeferredCmds *tracker)
{
    vkDestroySemaphore(vk_device.logical_device, tracker->timeline, vk_allocator);
    rfree(tracker->darray);
    *tracker = (RendVkDeferredCmds) {0};
}

static inline void rend_vk_cmdbuf_deferred_lock(RendVkDeferredCmds *t)   { (void)t; }
static inline void rend_vk_cmdbuf_deferred_unlock(RendVkDeferredCmds *t) { (void)t; }

static inline VkCommandBuffer
rend_vk_cmdbuf_deferred_begin(VkCommandPool pool)
{
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandPool = pool,
        .commandBufferCount = 1,
    };

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(vk_device.logical_device, &alloc_info, &cmd);

    VkCommandBufferBeginInfo begin = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    vkBeginCommandBuffer(cmd, &begin);
    return cmd;
}

static inline void
rend_vk_cmdbuf_deferred_push(RendVkDeferredCmds *tracker, VkCommandPool pool, VkCommandBuffer cmd, uint64_t wait_value)
{
    rend_vk_cmdbuf_deferred_lock(tracker);
    if (tracker->count == tracker->capacity) {
        tracker->capacity *= 2;
        tracker->darray = realloc(tracker->darray, sizeof(RendVkPendingCmd) * tracker->capacity);
    }
    tracker->darray[tracker->count++] = (RendVkPendingCmd) {
        .pool = pool,
        .cmd = cmd,
        .wait_value = wait_value,
    };
    rend_vk_cmdbuf_deferred_unlock(tracker);
}

static inline uint64_t
rend_vk_cmdbuf_deferred_submit(RendVkDeferredCmds *tracker, VkCommandPool pool, VkCommandBuffer cmd, VkQueue queue, uint64_t wait_value, VkPipelineStageFlags2 wait_stage, VkPipelineStageFlags2 signal_stage)
{
    uint64_t signal_value = wait_value + 1;

    VkCommandBufferSubmitInfo cmd_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = cmd,
    };
    VkSemaphoreSubmitInfo wait_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = tracker->timeline,
        .value = wait_value,
        .stageMask = wait_stage,
    };
    VkSemaphoreSubmitInfo signal_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = tracker->timeline,
        .value = signal_value,
        .stageMask = signal_stage,
    };
    VkSubmitInfo2 submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount = wait_value ? 1 : 0,
        .pWaitSemaphoreInfos = &wait_info,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &cmd_info,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos = &signal_info,
    };

    vkQueueSubmit2(queue, 1, &submit, VK_NULL_HANDLE);

    rend_vk_cmdbuf_deferred_push(tracker, pool, cmd, signal_value);
    return signal_value;
}

static inline void
rend_vk_cmdbuf_deferred_flush(RendVkDeferredCmds *tracker)
{
    uint64_t completed;
    vkGetSemaphoreCounterValue(vk_device.logical_device, tracker->timeline, &completed);

    for (size_t i = 0; i < tracker->count; ) {
        if (completed >= tracker->darray[i].wait_value) {
            vkFreeCommandBuffers(vk_device.logical_device, tracker->darray[i].pool, 1, &tracker->darray[i].cmd);
            tracker->darray[i] = tracker->darray[--tracker->count];
        } else {
            i++;
        }
    }
}
