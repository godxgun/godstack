#ifndef REND_VK_INTERNAL_H
#define REND_VK_INTERNAL_H

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <assert.h>

#define CHECK_VK_RESULT(r)                                                     \
{                                                                            \
    assert(r == VK_SUCCESS && __LINE__);                                       \
}


typedef struct RendVkImage  RendVkImage;
typedef struct RendVkArenaAllocator RendVkArenaAllocator;
typedef struct RendVkSbta RendVkSbta;

typedef struct RendVkPendingCmd RendVkPendingCmd;
typedef struct RendVkDeferredCmds RendVkDeferredCmds;

typedef struct {
    const char **extention_names_darray;
    bool sampler_anisotropy;
    bool discrete_gpu;
    bool graphics;
    bool present;
    bool compute;
    bool transfer;
} RendVkDevicespecs;

typedef struct {
    uint32_t graphics_family_index;
    uint32_t present_family_index;
    uint32_t compute_family_index;
    uint32_t transfer_family_index;
} RendVkQueueFamily;

typedef struct {
    VkSurfaceCapabilitiesKHR capabilities;
    VkSurfaceFormatKHR *format;
    VkPresentModeKHR *present_modes;
    uint32_t format_count;
    uint32_t present_mode_count;
} RendVkSwapchainSupport;

typedef struct {
    /* logical device handle */
    VkDevice logical_device;

    /* physical device limitation */
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceMemoryProperties memory;
    VkPhysicalDevice physical_device;
    VkPhysicalDeviceFeatures features;
    VkPhysicalDeviceDescriptorBufferPropertiesEXT desc_props;

    /* swapchain support */
    VkSurfaceKHR surface;
    RendVkSwapchainSupport swapchain_support;
    VkFormat depth_format;

    /* queues */
    VkQueue graphics_queue;
    VkQueue present_queue;
    VkQueue compute_queue;
    VkQueue transfer_queue;
    uint32_t graphics_family_index;
    uint32_t present_family_index;
    uint32_t compute_family_index;
    uint32_t transfer_family_index;

    /* memory heap indices */
    uint32_t host_index;
    uint32_t device_index;
} RendVkDevice;

static VkInstance vk_instance = 0;
static VkDebugUtilsMessengerEXT vk_debug_messenger = 0;
static RendVkDevice vk_device = {0};

/* helper functions */
static int32_t      rend_vk_memory_find_index(VkPhysicalDevice device, uint32_t type_filter, VkMemoryPropertyFlags property_flags);

/* rend_vk_device.c */
static bool     rend_vk_device_create(VkSurfaceKHR surface, RendSpecs specs, RendVkDevice *out_device, uint32_t (*score_devices)(RendVkDevice *, RendSpecs, char **));
static uint32_t rend_vk_device_score_default(RendVkDevice *device, RendSpecs minimum_specs, char **required_extensions);
static void     rend_vk_physical_device_query_swapchain_support(RendVkDevice *device);
static bool     rend_vk_device_detect_depth_format(RendVkDevice *device);
static void     rend_vk_device_destroy(RendVkDevice *device);

/* rend_vk_arena.c */
static RendVkArenaAllocator rend_vk_arena_create(VkDevice logical_device, VkPhysicalDevice physical_device, VkPhysicalDeviceLimits device_limits, VkAllocationCallbacks *allocator);
static uint32_t             rend_vk_arena_add_page(RendVkArenaAllocator *arena, VkDeviceSize size, uint32_t heap_index, bool fit_to_alloc);
static RendMemory           rend_vk_arena_alloc(RendVkArenaAllocator *arena, VkDeviceSize size, uint32_t heap_index);
static void                 rend_vk_arena_clear(RendVkArenaAllocator *arena, uint32_t memory_type);
static void                 rend_vk_arena_clear_all(RendVkArenaAllocator *arena);
static void                 rend_vk_arena_shrink(RendVkArenaAllocator *arena, uint32_t memory_type);
static void                 rend_vk_arena_destroy(RendVkArenaAllocator *arena);

/* rend_vk_command_queue.c */
static inline RendVkDeferredCmds rend_vk_cmdbuf_deferred_create(void);
static inline void rend_vk_cmdbuf_deferred_destroy(RendVkDeferredCmds *tracker);
static inline uint64_t rend_vk_cmdbuf_deferred_submit(RendVkDeferredCmds *tracker, VkCommandPool pool, VkCommandBuffer cmd, VkQueue queue, uint64_t wait_value, VkPipelineStageFlags2 wait_stage, VkPipelineStageFlags2 signal_stage);
static inline void rend_vk_cmdbuf_deferred_push(RendVkDeferredCmds *tracker, VkCommandPool pool, VkCommandBuffer cmd, uint64_t wait_value);
static inline void rend_vk_cmdbuf_deferred_flush(RendVkDeferredCmds *tracker);

/* rend_vk_image.c */
static RendVkImage  rend_vk_image_create(VkDevice logical_device, VkImageType img_type, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags memory_flags, uint32_t depth, uint32_t mip_levels, uint32_t layers, VkSampleCountFlags sample_count_flags, VkSharingMode sharing_mode);
static uint32_t     rend_vk_image_required_memory_type(RendVkImage *img);
static void         rend_vk_image_bind_memory(RendVkImage *image, RendMemory *memory);
static void         rend_vk_image_destroy(RendVkImage *img);
static void         rend_vk_image_view_create(RendVkImage *image, VkImageViewType view_type, VkImageAspectFlags view_aspect_flags);
static uint64_t     rend_vk_image_address(RendVkImage *image);


static VkAllocationCallbacks *vk_allocator = 0; // we are only ever going to use one, either the default one or the custom one

static VkFormat vk_format_from_rend_format[] = {
    [REND_FORMAT_R8_UNORM]           = VK_FORMAT_R8_UNORM,
    [REND_FORMAT_R8G8_UNORM]         = VK_FORMAT_R8G8_UNORM,
    [REND_FORMAT_R8G8B8A8_UNORM]     = VK_FORMAT_R8G8B8A8_UNORM,
    [REND_FORMAT_B8G8R8A8_UNORM]     = VK_FORMAT_B8G8R8A8_UNORM,
    [REND_FORMAT_R8G8B8A8_SRGB]      = VK_FORMAT_R8G8B8A8_SRGB,
    [REND_FORMAT_B8G8R8A8_SRGB]      = VK_FORMAT_B8G8R8A8_SRGB,
    [REND_FORMAT_R32_SFLOAT]         = VK_FORMAT_R32_SFLOAT,         
    [REND_FORMAT_R32G32_SFLOAT]      = VK_FORMAT_R32G32_SFLOAT,      
    [REND_FORMAT_R32G32B32_SFLOAT]   = VK_FORMAT_R32G32B32_SFLOAT,   
    [REND_FORMAT_R32G32B32A32_SFLOAT]= VK_FORMAT_R32G32B32A32_SFLOAT,
    [REND_FORMAT_R16_SFLOAT]         = VK_FORMAT_R16_SFLOAT,
    [REND_FORMAT_R16G16_SFLOAT]      = VK_FORMAT_R16G16_SFLOAT,
    [REND_FORMAT_R16G16B16A16_SFLOAT]= VK_FORMAT_R16G16B16A16_SFLOAT,
    [REND_FORMAT_R32_UINT]           = VK_FORMAT_R32_UINT,
    [REND_FORMAT_R32_SINT]           = VK_FORMAT_R32_SINT,
    [REND_FORMAT_R32G32B32A32_UINT]  = VK_FORMAT_R32G32B32A32_UINT,
    [REND_FORMAT_R16G16B16A16_UINT]  = VK_FORMAT_R16G16B16A16_UINT,
    [REND_FORMAT_R8G8B8A8_UINT]      = VK_FORMAT_R8G8B8A8_UINT,
    [REND_FORMAT_D32_SFLOAT]         = VK_FORMAT_D32_SFLOAT,
    [REND_FORMAT_D24_UNORM_S8_UINT]  = VK_FORMAT_D24_UNORM_S8_UINT,
    [REND_FORMAT_D32_SFLOAT_S8_UINT] = VK_FORMAT_D32_SFLOAT_S8_UINT,
};

static VkPrimitiveTopology vk_topology[] = {
            [REND_TOPOLOGY_TRIANGLE_LIST]   = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            [REND_TOPOLOGY_TRIANGLE_STRIP]  = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
            [REND_TOPOLOGY_LINE_LIST]       = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
            [REND_TOPOLOGY_LINE_STRIP]      = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
            [REND_TOPOLOGY_POINT_LIST]      = VK_PRIMITIVE_TOPOLOGY_POINT_LIST
};

static VkPolygonMode vk_polymode[] = {
    [REND_POLYGON_MODE_FILL]  = VK_POLYGON_MODE_FILL,
    [REND_POLYGON_MODE_LINE]  = VK_POLYGON_MODE_LINE,
    [REND_POLYGON_MODE_POINT] = VK_POLYGON_MODE_POINT,
};
        
static VkCullModeFlags vk_cullflags[] = {
    [REND_CULL_MODE_NONE]           = VK_CULL_MODE_NONE,
    [REND_CULL_MODE_FRONT]          = VK_CULL_MODE_FRONT_BIT,
    [REND_CULL_MODE_BACK]           = VK_CULL_MODE_BACK_BIT,
    [REND_CULL_MODE_FRONT_AND_BACK] = VK_CULL_MODE_FRONT_AND_BACK,
};

static int32_t
rend_vk_memory_find_index(VkPhysicalDevice device, uint32_t type_filter, VkMemoryPropertyFlags property_flags)
{
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(device, &memory_properties);
    for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++) {
        if (type_filter & (1 << i) && (memory_properties.memoryTypes[i].propertyFlags & property_flags) == property_flags) {
            return i;
        }
    }

    return -1;
}

#endif
