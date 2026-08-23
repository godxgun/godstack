#pragma once
#include "rend_internal.h"
#include "rend_vk_internal.h"
#include <vulkan/vulkan_core.h>

struct RendVkImage {
    VkDevice logical_device;
    VkImage handle;
    VkImageView view;

    VkMemoryRequirements requirements;

    VkImageType img_type;
    uint32_t width, height;
    VkFormat format;
    VkImageTiling tiling;
    VkImageUsageFlags usage;
    uint32_t depth;
    uint32_t mip_levels;
    uint32_t layers;
    VkSampleCountFlags sample_count_flags;
    VkSharingMode sharing_mode;

    RendMemory *memory;
};

static RendVkImage
rend_vk_image_create(VkDevice logical_device, VkImageType img_type, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
        VkImageUsageFlags usage, VkMemoryPropertyFlags memory_flags, uint32_t depth, uint32_t mip_levels, uint32_t layers,
        VkSampleCountFlags sample_count_flags, VkSharingMode sharing_mode)
{
    RendVkImage image = {
        .handle = VK_NULL_HANDLE,
        .memory = VK_NULL_HANDLE,
        .logical_device = logical_device,
        .img_type = img_type,
        .width = width,
        .height = height,
        .format = format,
        .tiling = tiling,
        .usage = usage,
        .depth = depth,
        .mip_levels = mip_levels,
        .layers = layers,
        .sample_count_flags = sample_count_flags,
        .sharing_mode = sharing_mode
    };

    VkImageCreateInfo img_create_info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    img_create_info.imageType = img_type;
    img_create_info.extent.width = width;
    img_create_info.extent.height = height;
    img_create_info.extent.depth = depth;
    img_create_info.mipLevels = mip_levels;
    img_create_info.arrayLayers = layers;
    img_create_info.format = format;
    img_create_info.tiling = tiling;
    img_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    img_create_info.usage = usage;
    img_create_info.samples = sample_count_flags;
    img_create_info.sharingMode = sharing_mode;


    if (vkCreateImage(logical_device, &img_create_info, vk_allocator, &image.handle) != VK_SUCCESS) {
        return image;
    }

    return image;
}

static uint32_t
rend_vk_image_required_memory_type(RendVkImage *img) 
{
    assert(img->memory == NULL && "Image already bound to memory");
    VkMemoryRequirements memory_requirements = {0};
    vkGetImageMemoryRequirements(img->logical_device, img->handle, &memory_requirements);
    img->requirements = memory_requirements;
    return memory_requirements.memoryTypeBits;
}

static void
rend_vk_image_bind_memory(RendVkImage *img, RendMemory *memory) 
{
    assert(img->memory == NULL && "Image already bound to memory");
    vkBindImageMemory(img->logical_device, img->handle, (VkDeviceMemory) memory->device_memory, memory->offset);
}

static void
rend_vk_image_destroy(RendVkImage *img)
{
    if (img->view) {
        vkDestroyImageView(img->logical_device, img->view, vk_allocator);
        img->view = 0;
    }

    if (img->handle) {
        vkDestroyImage(img->logical_device, img->handle, vk_allocator);
        img->handle = 0;
    }
}

static void
rend_vk_image_view_create(RendVkImage *image, VkImageViewType view_type, VkImageAspectFlags view_aspect_flags)
{
    VkImageViewCreateInfo view_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image->handle,
        .format = image->format,
        .viewType = view_type,
        .subresourceRange.aspectMask = view_aspect_flags,
        .subresourceRange.baseMipLevel = 0,               // offset starts at 0
        .subresourceRange.levelCount = image->mip_levels, // number of mips
        .subresourceRange.baseArrayLayer = 0,             // offset starts at 0
        .subresourceRange.layerCount = image->layers,     // number of layers
    };

    vkCreateImageView(image->logical_device, &view_create_info, vk_allocator, &image->view);
}

static uint64_t
rend_vk_image_address(RendVkImage *image)
{
    assert(image->memory == NULL && "Image already bound to memory");
    return (uint64_t) image->memory->device_memory + image->memory->offset;
}
