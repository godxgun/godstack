#pragma once
#include "rend_internal.h"
#include "rend_vk_internal.h"
#include <vulkan/vulkan_core.h>

/* arenas need to be subdivided into blocks
 * just like pools because we must respect page size */
typedef struct RendVkPage {
    RendMemory memory;
    size_t head;
    int reserved;
} RendVkPage;

typedef struct RendVkPagedArena {
    RendVkPage *page_darr;
    uint32_t capacity;
    uint32_t elements;
} RendVkPagedArena;

struct RendVkArenaAllocator {
    VkAllocationCallbacks *allocator;
    size_t *heap_idx_alloc_sizes; // allocation size per type of memory available on the gpu
    RendVkPagedArena *mem_arenas; // memory arena per type of memory available on the gpu
    VkDevice logical_device; // virtual device
    VkPhysicalDevice physical_device; // physical device we are allocating memory from
    VkDeviceSize gpu_alignment; // allocations must respect the physical limitations of the gpu
    VkDeviceSize block_min_size; // minimum size per block of memory
    uint64_t total_allocations;
    uint32_t heap_index_count;
    VkPhysicalDeviceMemoryProperties properties; // useful for getting properties per heap index
};

static RendVkArenaAllocator
rend_vk_arena_create(VkDevice logical_device, VkPhysicalDevice physical_device, VkPhysicalDeviceLimits device_limits, VkAllocationCallbacks *allocator)
{
        RendVkArenaAllocator arena = {
            .allocator = allocator,
            .heap_idx_alloc_sizes = NULL,
            .logical_device = logical_device,
            .physical_device = physical_device,
            .gpu_alignment = 0,
            .block_min_size = 0,
            .total_allocations = 0,
            .heap_index_count = 0,
            .mem_arenas = 0,
        };

        VkPhysicalDeviceMemoryProperties mem_properties;
        vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);
        arena.properties = mem_properties;

        uint32_t count = mem_properties.memoryTypeCount;
        arena.heap_index_count = count;
		
        arena.heap_idx_alloc_sizes = rmalloc(count * sizeof *arena.heap_idx_alloc_sizes);
        memset(arena.heap_idx_alloc_sizes, 0, count * sizeof *arena.heap_idx_alloc_sizes);

        arena.mem_arenas = rmalloc(count * sizeof *arena.mem_arenas);
        for (uint32_t u = 0; u < count; ++u) {
            arena.mem_arenas[u].capacity = 2;
            arena.mem_arenas[u].elements = 0;
            arena.mem_arenas[u].page_darr = rmalloc(2 * sizeof *arena.mem_arenas[u].page_darr);
            memset(arena.mem_arenas[u].page_darr, 0, 2 * sizeof *arena.mem_arenas[u].page_darr);
        }

        VkDeviceSize alignment = device_limits.bufferImageGranularity;
        if (device_limits.nonCoherentAtomSize > alignment) {
            alignment = device_limits.nonCoherentAtomSize;
        }

        arena.gpu_alignment = alignment;
        arena.block_min_size = arena.gpu_alignment * 10;

        return arena;
}

static uint32_t
rend_vk_arena_add_page(RendVkArenaAllocator *arena, VkDeviceSize size, uint32_t heap_index, bool fit_to_alloc)
{
        VkDeviceSize new_arena_size = fit_to_alloc ? size : (size * 2);
		new_arena_size = (new_arena_size < arena->block_min_size) ? arena->block_min_size : new_arena_size;
		
        VkMemoryPropertyFlags properties = arena->properties.memoryTypes[heap_index].propertyFlags;

        RendVkPage page = (RendVkPage) {0};
        page.head = 0;

        VkMemoryAllocateFlagsInfo flags_info = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
            .pNext = NULL,
            .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
            .deviceMask = 0
        };

        VkMemoryAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = new_arena_size,
            .pNext = &flags_info,
            .memoryTypeIndex = heap_index
        };

        page.memory = (RendMemory) {0};
        page.memory.offset = 0;
        page.memory.size = new_arena_size;

        VkResult res = vkAllocateMemory(arena->logical_device, &alloc_info, arena->allocator, (VkDeviceMemory*) &page.memory.device_memory);
        if (res != VK_SUCCESS) {
            return UINT32_MAX;
        }

        if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
            vkMapMemory(
                    arena->logical_device, 
                    (VkDeviceMemory) page.memory.device_memory, 
                    0,                 // Offset inside VkDeviceMemory!!!1
                    new_arena_size,    // MUST match alloc_info.allocationSize and NOT raw size!!!
                    0, 
                    &page.memory.host_mapped_memory
                    );
        }

        RendVkPagedArena *mem_arena = &arena->mem_arenas[heap_index];

        if (mem_arena->elements + 1 >= mem_arena->capacity) {
            uint32_t new_capacity = mem_arena->capacity * 2;
            void *new_darr = rrealloc(mem_arena->page_darr, new_capacity * sizeof *mem_arena->page_darr);
            if (!new_darr) {
                vkFreeMemory(arena->logical_device, (VkDeviceMemory) page.memory.device_memory, arena->allocator);
                return UINT32_MAX;
            }
            mem_arena->page_darr = new_darr;
            mem_arena->capacity = new_capacity;
        }

        uint32_t page_index = mem_arena->elements;
        mem_arena->page_darr[mem_arena->elements++] = page;

        arena->total_allocations++;

        return page_index;
}

static RendMemory 
rend_vk_arena_alloc(RendVkArenaAllocator *arena, VkDeviceSize size, uint32_t heap_index)
{
    assert(heap_index < 32 && "Unusual heap index. Did you pass the memory type instead?");
    RendVkPagedArena *mem_arena = &arena->mem_arenas[heap_index];

    VkMemoryPropertyFlags properties = arena->properties.memoryTypes[heap_index].propertyFlags;

    /* align to allocation to fit page size */
    VkDeviceSize align = arena->gpu_alignment;
    VkDeviceSize aligned_size = (size + align - 1) & ~(align - 1);

    /* check if fits in previous blocks */
    int whole_page = !(properties & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    uint32_t page_idx = UINT32_MAX;
    for (uint32_t u = 0; u < mem_arena->elements; ++u) {
        RendVkPage page = mem_arena->page_darr[u];
        int valid = (whole_page) ? (page.head == 0) : 1;
        if ((page.head + aligned_size <= page.memory.size) && valid) {
            page_idx = u;
            break;
        }
    }

    /* page not found, add new page */
    if (page_idx == UINT32_MAX) {
        page_idx = rend_vk_arena_add_page(arena, aligned_size, heap_index, whole_page);
        if (page_idx == UINT32_MAX) {
            REND__CRASH("[REND_VK] Arena page allocation failed!");
        }
    }

    mem_arena->page_darr[page_idx].reserved = whole_page;

    RendMemory memory = {
        .device_memory = mem_arena->page_darr[page_idx].memory.device_memory,
        .size = aligned_size,
        .offset = mem_arena->page_darr[page_idx].head,
        .host_mapped_memory = 0,
        .heap_index = heap_index,
        .id = page_idx,
    };
    
    if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        memory.host_mapped_memory = mem_arena->page_darr[page_idx].memory.host_mapped_memory + memory.offset;
    }
        
    /* move head */
    arena->heap_idx_alloc_sizes[heap_index] += aligned_size;
    mem_arena->page_darr[page_idx].head += aligned_size;
    return memory;
}


static void
rend_vk_arena_clear(RendVkArenaAllocator *arena, uint32_t heap_index)
{
    assert(heap_index < 32 && "Unusual heap index. Did you pass the memory type instead?");
    RendVkPagedArena mem_arena = arena->mem_arenas[heap_index];
    if (mem_arena.page_darr) {
        for (uint32_t u = 0; u < mem_arena.elements; ++u) {
            mem_arena.page_darr[u].head = 0;
            mem_arena.page_darr[u].reserved = 0;
        }
    }
}

static void
rend_vk_arena_clear_all(RendVkArenaAllocator *arena)
{
    for (uint32_t u = 0; u < arena->properties.memoryTypeCount; ++u) {
        rend_vk_arena_clear(arena, u);
    }
}

static void
rend_vk_arena_shrink(RendVkArenaAllocator *arena, uint32_t heap_index)
{
    assert(heap_index < 32 && "Unusual heap index. Did you pass the memory type instead?");
    RendVkPagedArena *mem_arena = &arena->mem_arenas[heap_index];
    if (mem_arena->capacity > 4) {
        uint32_t new_capacity = mem_arena->capacity / 2;
        void *new_darr = rrealloc(mem_arena->page_darr, new_capacity * sizeof *mem_arena->page_darr);
        if (!new_darr) return;
        mem_arena->page_darr = new_darr;
        mem_arena->capacity = new_capacity;
    }
}

static void
rend_vk_arena_destroy(RendVkArenaAllocator *arena)
{
    if (arena->mem_arenas) { 
        for (size_t u = 0; u < arena->heap_index_count; ++u) {
            RendVkPagedArena *mem_arena = &arena->mem_arenas[u];
            for (uint32_t p = 0; p < mem_arena->elements; ++p) {

                RendMemory memory = mem_arena->page_darr[p].memory ;
                if (memory.offset == 0) {
                    if (memory.host_mapped_memory) {
                        vkUnmapMemory(arena->logical_device, (VkDeviceMemory) memory.device_memory);
                    }
                    vkFreeMemory(arena->logical_device, (VkDeviceMemory) memory.device_memory, arena->allocator);
                } else {
                    PWARN("[REND_VK] Attempted to free memory with an offset!");
                }

            }
            if (mem_arena->page_darr) {
                rfree(mem_arena->page_darr);
                mem_arena->page_darr = 0;
            }
        }
        rfree(arena->mem_arenas);
        arena->mem_arenas = 0;
    }
    if (arena->heap_idx_alloc_sizes) {
        rfree(arena->heap_idx_alloc_sizes);
        arena->heap_idx_alloc_sizes = 0;
    }
    memset(arena, 0, sizeof *arena);
}
