#pragma once
#include "rend_internal.h"
#include "rend_vk_internal.h"
#include <vulkan/vulkan_core.h>

/*
 * Sparse Bindless Texture Array (SBTA)
 *
 */

struct RendVkSbta {
    VkDevice logical_device;
    RendVkImage image;              /* single 2D array image, arrayLayers = max_layers */
    VkImageView *views;             /* per-layer VkImageView array */
    VkFormat format;
    VkExtent2D extent;
    uint32_t max_layers;
    uint32_t mip_levels;
    uint64_t *bitmap;               /* 1 bit per layer slot */
    uint32_t bitmap_word_count;
    uint32_t allocated_count;
    RendMemory memory;
};

static inline uint32_t
rend_vk_sbta_mip_count(uint32_t w, uint32_t h)
{
    uint32_t v = (w > h) ? w : h;
    uint32_t levels = 1;
    while (v >>= 1) { levels++; }
    return levels;
}

static void
rend_vk_sbta_create(RendVkSbta *sbta, VkDevice logical_device, VkExtent2D extent, uint32_t layers)
{
    memset(sbta, 0, sizeof *sbta);
    sbta->logical_device = logical_device;
    sbta->format = VK_FORMAT_R8G8B8A8_SRGB;
    sbta->extent = extent;
    sbta->max_layers = layers;
    sbta->mip_levels = rend_vk_sbta_mip_count(extent.width, extent.height);

    /* create image */
    sbta->image = rend_vk_image_create(
        logical_device,
        VK_IMAGE_TYPE_2D,
        extent.width, extent.height,
        sbta->format,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        1,                  /* depth */
        sbta->mip_levels,
        layers,
        VK_SAMPLE_COUNT_1_BIT,
        VK_SHARING_MODE_EXCLUSIVE
    );

    /* allocate per-layer views array */
    sbta->views = rmalloc(layers * sizeof *sbta->views);
    memset(sbta->views, 0, layers * sizeof *sbta->views);

    /* bitmap: ceil(layers / 64) words */
    sbta->bitmap_word_count = (layers + 63) / 64;
    sbta->bitmap = rmalloc(sbta->bitmap_word_count * sizeof *sbta->bitmap);
    memset(sbta->bitmap, 0, sbta->bitmap_word_count * sizeof *sbta->bitmap);
    sbta->allocated_count = 0;
}

static void
rend_vk_sbta_create_views(RendVkSbta *sbta)
{
    for (uint32_t i = 0; i < sbta->max_layers; ++i) {
        VkImageViewCreateInfo view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = sbta->image.handle,
            .format = sbta->format,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .subresourceRange.baseMipLevel = 0,
            .subresourceRange.levelCount = sbta->mip_levels,
            .subresourceRange.baseArrayLayer = i,
            .subresourceRange.layerCount = 1,
        };
        vkCreateImageView(sbta->logical_device, &view_info, vk_allocator, &sbta->views[i]);
    }
}

static void
rend_vk_sbta_destroy(RendVkSbta *sbta)
{
    for (uint32_t i = 0; i < sbta->max_layers; ++i) {
        if (sbta->views[i]) {
            vkDestroyImageView(sbta->logical_device, sbta->views[i], vk_allocator);
        }
    }

    rend_vk_image_destroy(&sbta->image);

    rfree(sbta->views);
    rfree(sbta->bitmap);
    memset(sbta, 0, sizeof *sbta);
}

static uint64_t
rend_vk_sbta_alloc(RendVkSbta *sbta)
{
    for (uint32_t w = 0; w < sbta->bitmap_word_count; ++w) {
        if (sbta->bitmap[w] == ~(uint64_t)0) continue; /* word full */

        /* find first zero bit */
        uint64_t word = sbta->bitmap[w];
        uint64_t bit = ~word & (word + 1); /* isolate lowest zero bit */
        uint32_t bit_index = 0;
        uint64_t tmp = bit;
        while (tmp >>= 1) { bit_index++; }

        uint64_t slot = (uint64_t)w * 64 + bit_index;
        if (slot >= sbta->max_layers) return UINT64_MAX; /* past capacity */

        sbta->bitmap[w] |= bit;
        sbta->allocated_count++;
        return slot;
    }
    return UINT64_MAX; /* full */
}

static void
rend_vk_sbta_free(RendVkSbta *sbta, uint64_t idx)
{
    assert(idx < sbta->max_layers && "SBTA free: index out of range");

    uint32_t word = (uint32_t)(idx / 64);
    uint32_t bit  = (uint32_t)(idx % 64);
    assert((sbta->bitmap[word] & (1ULL << bit)) && "SBTA free: slot not allocated (double free?)");

    sbta->bitmap[word] &= ~(1ULL << bit);
    sbta->allocated_count--;
}

static void
rend_vk_sbta_upload(RendVkSbta *sbta, VkCommandBuffer cmd, RendVkArenaAllocator *staging_arena, uint64_t slot, void *pixels, uint64_t size)
{
    assert(slot < sbta->max_layers);
    assert(pixels && size > 0);

    /* ---- staging buffer ---- */
    VkBufferCreateInfo buf_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size  = size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    };

    VkBuffer staging_buf;
    vkCreateBuffer(sbta->logical_device, &buf_info, vk_allocator, &staging_buf);

    VkMemoryRequirements staging_reqs;
    vkGetBufferMemoryRequirements(sbta->logical_device, staging_buf, &staging_reqs);

    uint32_t host_index = vk_device.host_index;
    RendMemory staging_mem = rend_vk_arena_alloc(staging_arena, staging_reqs.size, host_index);
    vkBindBufferMemory(sbta->logical_device, staging_buf, (VkDeviceMemory) staging_mem.device_memory, staging_mem.offset);

    /* copy pixels into staging */
    memcpy(staging_mem.host_mapped_memory, pixels, size);

    /* ---- transition layer mip 0: UNDEFINED -> TRANSFER_DST ---- */
    VkImageMemoryBarrier2 barrier_to_dst = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask  = VK_PIPELINE_STAGE_2_NONE,
        .srcAccessMask = VK_ACCESS_2_NONE,
        .dstStageMask  = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .image = sbta->image.handle,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = sbta->mip_levels,
            .baseArrayLayer = (uint32_t)slot,
            .layerCount = 1,
        },
    };

    VkDependencyInfo dep_to_dst = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier_to_dst,
    };
    vkCmdPipelineBarrier2(cmd, &dep_to_dst);

    /* ---- copy staging -> image mip 0 ---- */
    VkBufferImageCopy copy_region = {
        .bufferOffset = 0,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = (uint32_t)slot,
            .layerCount = 1,
        },
        .imageExtent = { sbta->extent.width, sbta->extent.height, 1 },
    };

    vkCmdCopyBufferToImage(cmd, staging_buf, sbta->image.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

    /* ---- generate mipmaps via blit chain ---- */
    int32_t mip_w = (int32_t)sbta->extent.width;
    int32_t mip_h = (int32_t)sbta->extent.height;

    for (uint32_t mip = 1; mip < sbta->mip_levels; ++mip) {
        /* transition previous mip: TRANSFER_DST -> TRANSFER_SRC */
        VkImageMemoryBarrier2 barrier_src = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask  = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .dstStageMask  = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .image = sbta->image.handle,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = mip - 1,
                .levelCount = 1,
                .baseArrayLayer = (uint32_t)slot,
                .layerCount = 1,
            },
        };

        VkDependencyInfo dep_src = {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &barrier_src,
        };
        vkCmdPipelineBarrier2(cmd, &dep_src);

        /* blit from mip-1 to mip */
        int32_t next_w = (mip_w > 1) ? mip_w / 2 : 1;
        int32_t next_h = (mip_h > 1) ? mip_h / 2 : 1;

        VkImageBlit2 blit = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
            .srcSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = mip - 1,
                .baseArrayLayer = (uint32_t)slot,
                .layerCount = 1,
            },
            .srcOffsets = { {0, 0, 0}, {mip_w, mip_h, 1} },
            .dstSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = mip,
                .baseArrayLayer = (uint32_t)slot,
                .layerCount = 1,
            },
            .dstOffsets = { {0, 0, 0}, {next_w, next_h, 1} },
        };

        VkBlitImageInfo2 blit_info = {
            .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
            .srcImage = sbta->image.handle,
            .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .dstImage = sbta->image.handle,
            .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .regionCount = 1,
            .pRegions = &blit,
            .filter = VK_FILTER_LINEAR,
        };
        vkCmdBlitImage2(cmd, &blit_info);

        mip_w = next_w;
        mip_h = next_h;
    }

    /* ---- final transition: all mips -> SHADER_READ_ONLY ---- */
    /* last mip is still TRANSFER_DST, all others are TRANSFER_SRC */

    /* transition last mip: TRANSFER_DST -> SHADER_READ_ONLY */
    VkImageMemoryBarrier2 barriers_final[2] = {
        /* mips 0..N-2: TRANSFER_SRC -> SHADER_READ_ONLY */
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask  = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
            .dstStageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            .dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .image = sbta->image.handle,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = (sbta->mip_levels > 1) ? sbta->mip_levels - 1 : 1,
                .baseArrayLayer = (uint32_t)slot,
                .layerCount = 1,
            },
        },
        /* last mip: TRANSFER_DST -> SHADER_READ_ONLY */
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask  = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .dstStageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            .dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .image = sbta->image.handle,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = sbta->mip_levels - 1,
                .levelCount = 1,
                .baseArrayLayer = (uint32_t)slot,
                .layerCount = 1,
            },
        },
    };

    uint32_t barrier_count = (sbta->mip_levels > 1) ? 2 : 1;

    /* if only 1 mip, use second barrier (TRANSFER_DST path) */
    VkImageMemoryBarrier2 *barrier_ptr = (sbta->mip_levels > 1) ? barriers_final : &barriers_final[1];

    VkDependencyInfo dep_final = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = barrier_count,
        .pImageMemoryBarriers = barrier_ptr,
    };
    vkCmdPipelineBarrier2(cmd, &dep_final);
}
