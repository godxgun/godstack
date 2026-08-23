#pragma once
#include "rend_internal.h"
#include "rend_vk_internal.h"
#include <vulkan/vulkan_core.h>

/* NOTE(vasco):
 * Check out https://kylehalladay.com/blog/tutorial/2017/12/13/Custom-Allocators-Vulkan.html
 * for a basic grasp on what it entails to actually write a memory allocator for vulkan.
 *
 * This allocator isnt anything particularly amazing but it works so I'm keeping this code.
 * Based on the framework presented here we could implement other more efficient allocators.
 * Which is what I did for the arena allocator.
 */

#if 0
typedef struct RendVkAddress {
    VkDeviceMemory handle;
    uint32_t type;
    uint32_t id;
    VkDeviceSize size;
    VkDeviceSize offset;
} RendVkAddress;

typedef struct RendVkPoolLayout {
    uint64_t offset, size;
} RendVkPoolLayout;

typedef struct RendVKPoolBlock {
    RendVkAddress address;
    RendVkPoolLayout *layout_darr;
    uint8_t reserved;
} RendVKPoolBlock;

typedef struct RendVkPoolMemory {
    RendVKPoolBlock *block_darr;
} RendVkPoolMemory;

struct RendVkPoolAllocator {
    VkAllocationCallbacks *allocator;
    size_t *mem_type_alloc_sizes; // allocation size per type of memory available on the gpu
    RendVkPoolMemory *mem_pools; // memory pools per type of memory available on the gpu
    VkDevice logical_device; // virtual device
    VkPhysicalDevice physical_device; // physical device we are allocating memory from
    VkDeviceSize page_size; // allocations must respect the physical limitations of the gpu
    VkDeviceSize block_min_size; // minimum size per block of memory
    uint64_t total_allocations;
    uint32_t memory_type_count;
};

static RendVkPoolAllocator
rend_vk_pool_create(VkDevice logical_device, VkPhysicalDevice physical_device, VkPhysicalDeviceLimits device_limits, VkAllocationCallbacks *allocator)
{
        RendVkPoolAllocator pool = {
            .allocator = allocator,
            .mem_type_alloc_sizes = NULL,
            .mem_pools = NULL,
            .logical_device = logical_device,
            .physical_device = physical_device,
            .page_size = 0,
            .block_min_size = 0,
            .total_allocations = 0,
            .memory_type_count = 0,
        };

		VkPhysicalDeviceMemoryProperties mem_properties;
		vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);
		
        pool.mem_type_alloc_sizes = malloc(mem_properties.memoryTypeCount * sizeof *pool.mem_type_alloc_sizes);
        memset(pool.mem_type_alloc_sizes, 0, mem_properties.memoryTypeCount * sizeof *pool.mem_type_alloc_sizes);

        pool.mem_pools = malloc(mem_properties.memoryTypeCount * sizeof *pool.mem_pools);
        memset(pool.mem_pools, 0, mem_properties.memoryTypeCount * sizeof *pool.mem_pools);

        pool.memory_type_count = mem_properties.memoryTypeCount;
		pool.page_size = device_limits.bufferImageGranularity;
		pool.block_min_size = pool.page_size * 10;

        return pool;
}

static uint32_t
rend_vk_pool_add_block(RendVkPoolAllocator *pool, VkDeviceSize size, uint32_t memory_type, VkMemoryPropertyFlags properties, bool fit_to_alloc)
{
		VkDeviceSize new_pool_size = size * 2;
		new_pool_size = (new_pool_size < pool->block_min_size) ? pool->block_min_size : new_pool_size;
		
        VkMemoryAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = new_pool_size,
            .memoryTypeIndex = rend_vk_memory_find_index(pool->physical_device, memory_type, properties)
        };

		RendVKPoolBlock block = {0};
		VkResult res = vkAllocateMemory(pool->logical_device, &alloc_info, pool->allocator, &block.address.handle);
		block.address.type = memory_type;
		block.address.size = new_pool_size;

        RendVkPoolMemory *mem_pool = &pool->mem_pools[memory_type];
        p_darray_push(mem_pool->block_darr, block);
		
        RendVkPoolLayout layout = {
            .offset = 0,
            .size = new_pool_size
        };
        p_darray_push(mem_pool->block_darr[pool_size - 1].layout_darr, layout);
        
        pool->total_allocations++;

        size_t pool_size = p_darray_len(mem_pool->block_darr);
		return pool_size - 1;
}

static RendVkMemory 
rend_vk_pool_alloc(RendVkPoolAllocator *pool, VkDeviceSize size, uint32_t usage, uint32_t memory_type, VkMemoryPropertyFlags properties)
{
    RendVkPoolMemory *mem_pool = &pool->mem_pools[memory_type];

    VkDeviceSize requested_alloc_size = ((size / pool->page_size) + 1) * pool->page_size;
    pool->mem_type_alloc_sizes[memory_type] += requested_alloc_size;

    /* find free chunk for allocation 
     * TODO: free list? 
     */
    int whole_page = usage != VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    uint64_t block_idx = UINT64_MAX;
    uint64_t span_idx  = UINT64_MAX;
    for (uint32_t i = 0; i < p_darray_len(mem_pool->block_darr); ++i) {
        for (uint32_t j = 0; j < p_darray_len(mem_pool->block_darr[i].layout_darr); ++j) {
            int valid = (whole_page) ? mem_pool->block_darr[i].layout_darr[j].offset == 0 : 1;
            if (mem_pool->block_darr[i].layout_darr[j].size >= size && valid) {
                block_idx = i;
                span_idx = j;
            }
        }
    }

    if (block_idx == UINT64_MAX || span_idx == UINT64_MAX) {
        block_idx = rend_vk_pool_add_block(pool, size, memory_type, properites, whole_page);
        span_idx = 0;
    }

    mem_pool->block_darr[block_idx].reserved = whole_page;

    RendVkMemory memory = {
        .device_memory = mem_pool->block_darr[block_idx].address.handle,
        .size = size,
        .offset = mem_pool->block_darr[block_idx].layout_darr[span_idx].offset, 
        .type = memory_type,

        .logical_device = pool->logical_device,
        .allocator = pool->allocator,
        .host_mapped_memory = 0,
        .physical_device = pool->physical_device,
        .properties = properties,

        .id = block_idx,
    };
        
    /* mark chunck */
    mem_pool->block_darr[block_idx].layout_darr[span_idx].offset += size;
    mem_pool->block_darr[block_idx].layout_darr[span_idx].size -= size;
    return memory;
}


static void
rend_vk_pool_free(RendVkPoolAllocator *pool, RendVkMemory *memory)
{
    VkDeviceSize requested = ((memory->size / pool->page_size) + 1) * pool->page_size;

    RendVkPoolMemory *mem_pool = &pool->mem_pools[memory->type];

    pool->blocks_[allocation.id].pageReserved = false;

    mem_pool->block_darr[block_idx].layout_darr[span_idx].offset += size;
    mem_pool->block_darr[block_idx].layout_darr[span_idx].size -= size;

    OffsetSize span = {allocation.offset, requestedAllocSize };
    bool found = false;

    uint32_t numLayoutMems = pool.blocks[allocation.id].layout.size();
    for (uint32_t j = 0; j < numLayoutMems; ++j)
    {
        if (pool.blocks[allocation.id].layout[j].offset == requestedAllocSize +allocation.offset)
        {
            pool.blocks[allocation.id].layout[j].offset = allocation.offset;
            pool.blocks[allocation.id].layout[j].size += requestedAllocSize;
            found = true;
            break;
        }
    }

    if (!found)
    {
        state.memPools[allocation.type].blocks[allocation.id].layout.push_back(span);
        state.memTypeAllocSizes[allocation.type] -= requestedAllocSize;
    }
}

static void
rend_vk_pool_destroy(RendVkPoolAllocator *pool)
{
    if (pool->mem_pools) { 
        for (size_t u = 0; u < pool->memory_type_count; ++u) {
            RendVkPoolMemory mem_pool = pool->mem_pools[u];
            for (size_t b = 0; b < p_darray_len(mem_pool.block_darr); ++b) {
                RendVKPoolBlock block = mem_pool.block_darr[b];
                p_darray_destroy(block.layout_darr);
            }
            p_darray_destroy(mem_pool.block_darr);
        }
        free(pool->mem_pools);
        pool->mem_pools = 0;
    }
    if (pool->mem_type_alloc_sizes) {
        free(pool->mem_type_alloc_sizes);
        pool->mem_type_alloc_sizes = 0;
    }
    memset(pool, 0, sizeof *pool);
}
#endif
