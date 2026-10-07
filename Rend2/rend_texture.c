/* Explicit placed snake-subset images and views. */
#include "rend_internal.h"
#include <string.h>

static VkFormat
rend_vk_format(RendFormat f)
{
	switch (f)
	{
	case REND_FORMAT_RGBA8_UNORM:
		return VK_FORMAT_R8G8B8A8_UNORM;
	case REND_FORMAT_BGRA8_UNORM:
		return VK_FORMAT_B8G8R8A8_UNORM;
	case REND_FORMAT_D32_FLOAT:
		return VK_FORMAT_D32_SFLOAT;
	default:
		return VK_FORMAT_UNDEFINED;
	}
}
static VkImageUsageFlags
rend_vk_image_usage(uint32_t u)
{
	VkImageUsageFlags n = 0;
	if (u & REND_TEXTURE_SAMPLED)
		n |= VK_IMAGE_USAGE_SAMPLED_BIT;
	if (u & REND_TEXTURE_STORAGE)
		n |= VK_IMAGE_USAGE_STORAGE_BIT;
	if (u & REND_TEXTURE_COLOR_ATTACHMENT)
		n |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if (u & REND_TEXTURE_TRANSFER_SRC)
		n |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	if (u & REND_TEXTURE_TRANSFER_DST)
		n |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	if (u & REND_TEXTURE_DEPTH_ATTACHMENT)
		n |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	return n;
}
int
rend_texture_memory_requirements(RendBackend b, const RendTextureDesc *d,
				 uint32_t cls,
				 RendStorageRequirements *out)
{
	VkImageCreateInfo ci;
	VkImage image = VK_NULL_HANDLE;
	VkMemoryRequirements2 mr2;
	VkImageMemoryRequirementsInfo2 mi;
	VkMemoryDedicatedRequirements dedicated;
	VkResult r;
	uint32_t type;
	if (out)
		memset(out, 0, sizeof(*out));
	if (out == NULL || !b || b->magic != REND_BACKEND_MAGIC || !d || !cls ||
	    cls > b->memory.memoryTypeCount || !d->width || !d->height ||
	    d->depth != 1 || !d->mip_levels || !d->array_layers || d->samples != 1 ||
	    rend_vk_format((RendFormat)d->format) == VK_FORMAT_UNDEFINED ||
	    !d->usage || (d->usage & (REND_TEXTURE_SAMPLED | REND_TEXTURE_STORAGE)) ||
	    (d->format == REND_FORMAT_D32_FLOAT &&
	     !(d->usage & REND_TEXTURE_DEPTH_ATTACHMENT)) ||
	    (d->format != REND_FORMAT_D32_FLOAT &&
	     (d->usage & REND_TEXTURE_DEPTH_ATTACHMENT)) ||
	    (d->usage &
	     ~(REND_TEXTURE_SAMPLED | REND_TEXTURE_STORAGE |
	       REND_TEXTURE_COLOR_ATTACHMENT | REND_TEXTURE_TRANSFER_SRC |
	       REND_TEXTURE_TRANSFER_DST | REND_TEXTURE_DEPTH_ATTACHMENT)))
	{
		rend_vk_set_diagnostic(
			REND_DIAG_INVALID_ARGUMENT, 0,
			"invalid RGBA/BGRA texture requirements description");
		return 0;
	}
	memset(&ci, 0, sizeof(ci));
	ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	ci.imageType = VK_IMAGE_TYPE_2D;
	ci.format = rend_vk_format((RendFormat)d->format);
	ci.extent.width = d->width;
	ci.extent.height = d->height;
	ci.extent.depth = 1;
	ci.mipLevels = d->mip_levels;
	ci.arrayLayers = d->array_layers;
	ci.samples = VK_SAMPLE_COUNT_1_BIT;
	ci.tiling = VK_IMAGE_TILING_OPTIMAL;
	ci.usage = rend_vk_image_usage(d->usage);
	ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	r = vkCreateImage(b->device, &ci, NULL, &image);
	if (r != VK_SUCCESS)
	{
		rend_vk_set_diagnostic(REND_DIAG_NATIVE, r,
				       "vkCreateImage requirements probe failed");
		return 0;
	}
	memset(&mi, 0, sizeof(mi));
	mi.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2;
	mi.image = image;
	memset(&dedicated, 0, sizeof(dedicated));
	dedicated.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS;
	memset(&mr2, 0, sizeof(mr2));
	mr2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
	mr2.pNext = &dedicated;
	vkGetImageMemoryRequirements2(b->device, &mi, &mr2);
	vkDestroyImage(b->device, image, NULL);
	type = cls - 1;
	if (dedicated.requiresDedicatedAllocation)
	{
		rend_vk_set_diagnostic(REND_DIAG_UNSUPPORTED, 0,
				       "texture requires dedicated allocation not expressible by snake placement");
		return 0;
	}
	if (!(mr2.memoryRequirements.memoryTypeBits & (1u << type)))
	{
		rend_vk_set_diagnostic(
			REND_DIAG_UNSUPPORTED, 0,
			"selected memory class is incompatible with texture memoryTypeBits");
		return 0;
	}
	out->size = mr2.memoryRequirements.size;
	out->alignment = mr2.memoryRequirements.alignment;
	return 1;
}
int
rend_texture_create(RendBackend b, const RendTextureDesc *d, RendHeap h,
		    uint64_t offset, uint64_t size, RendTexture *out)
{
	uint32_t i;
	RendTextureRecord *t = NULL;
	RendStorageRequirements req;
	VkImageCreateInfo ci;
	VkMemoryRequirements mr;
	VkImageMemoryRequirementsInfo2 mi;
	VkMemoryRequirements2 mr2;
	VkMemoryDedicatedRequirements dedicated;
	VkResult r;
	if (out)
		*out = NULL;
	if (!out || !b || b->magic != REND_BACKEND_MAGIC || !h || h->backend != b ||
	    !h->live || h->kind != REND_HEAP_TEXTURE_STORAGE || !d)
		goto fail;
	if (!rend_texture_memory_requirements(b, d, h->memory_class, &req))
		return 0;
	if (offset % req.alignment || offset > h->size ||
	    size > h->size - offset || size < req.size ||
	    req.size > h->size - offset)
		goto fail;
	for (i = 0; i < b->texture_count; i++)
		if (!b->textures[i].live)
		{
			t = &b->textures[i];
			break;
		}
	if (!t)
	{
		rend_vk_set_diagnostic(REND_DIAG_CAPACITY, 0,
				       "texture record capacity exhausted");
		return 0;
	}
	memset(&ci, 0, sizeof(ci));
	ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	ci.imageType = VK_IMAGE_TYPE_2D;
	ci.format = rend_vk_format((RendFormat)d->format);
	ci.extent.width = d->width;
	ci.extent.height = d->height;
	ci.extent.depth = 1;
	ci.mipLevels = d->mip_levels;
	ci.arrayLayers = d->array_layers;
	ci.samples = VK_SAMPLE_COUNT_1_BIT;
	ci.tiling = VK_IMAGE_TILING_OPTIMAL;
	ci.usage = rend_vk_image_usage(d->usage);
	ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	r = vkCreateImage(b->device, &ci, NULL, &t->image);
	if (r != VK_SUCCESS)
		goto native;
	memset(&mi, 0, sizeof(mi));
	mi.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2;
	mi.image = t->image;
	memset(&dedicated, 0, sizeof(dedicated));
	dedicated.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS;
	memset(&mr2, 0, sizeof(mr2));
	mr2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
	mr2.pNext = &dedicated;
	vkGetImageMemoryRequirements2(b->device, &mi, &mr2);
	mr = mr2.memoryRequirements;
	if (dedicated.requiresDedicatedAllocation || mr.size > size ||
	    offset % mr.alignment ||
	    !(mr.memoryTypeBits & (1u << (h->memory_class - 1))))
	{
		vkDestroyImage(b->device, t->image, NULL);
		t->image = VK_NULL_HANDLE;
		rend_vk_set_diagnostic(
			REND_DIAG_UNSUPPORTED, 0,
			"image requires unsupported dedicated/incompatible placement");
		return 0;
	}
	r = vkBindImageMemory(b->device, t->image, h->memory, offset);
	if (r != VK_SUCCESS)
	{
		vkDestroyImage(b->device, t->image, NULL);
		t->image = VK_NULL_HANDLE;
		goto native;
	}
	t->backend = b;
	t->heap = h;
	t->desc = *d;
	t->offset = offset;
	t->requirements_size = mr.size;
	t->requirements_alignment = mr.alignment;
	t->live = 1;
	h->refs++;
	*out = t;
	return 1;
native:
	rend_vk_set_diagnostic(REND_DIAG_NATIVE, r,
			       "native placed image create/bind failed");
	return 0;
fail:
	rend_vk_set_diagnostic(REND_DIAG_INVALID_ARGUMENT, 0,
			       "invalid texture placement argument");
	return 0;
}
int
rend_texture_view_create(RendBackend b, RendTexture t,
			 const RendTextureViewDesc *d,
			 RendTextureView *out)
{
	uint32_t i;
	RendTextureViewRecord *v = NULL;
	VkImageViewCreateInfo ci;
	VkResult r;
	if (out)
		*out = NULL;
	if (!out || !b || b->magic != REND_BACKEND_MAGIC || !t || !t->live ||
	    t->backend != b || !d || d->view_type != REND_VIEW_2D ||
	    (d->aspect != REND_ASPECT_COLOR && d->aspect != REND_ASPECT_DEPTH) ||
	    rend_vk_format(d->format) == VK_FORMAT_UNDEFINED ||
	    d->format != (RendFormat)t->desc.format ||
	    (d->aspect == REND_ASPECT_COLOR &&
	     !(t->desc.usage & REND_TEXTURE_COLOR_ATTACHMENT)) ||
	    (d->aspect == REND_ASPECT_DEPTH &&
	     !(t->desc.usage & REND_TEXTURE_DEPTH_ATTACHMENT)) ||
	    (d->format == REND_FORMAT_D32_FLOAT) !=
		    (d->aspect == REND_ASPECT_DEPTH) ||
	    !d->mip_count || !d->layer_count || d->base_mip >= t->desc.mip_levels ||
	    d->mip_count > t->desc.mip_levels - d->base_mip ||
	    d->base_layer >= t->desc.array_layers ||
	    d->layer_count > t->desc.array_layers - d->base_layer)
		goto fail;
	for (i = 0; i < b->view_count; i++)
		if (!b->views[i].live)
		{
			v = &b->views[i];
			break;
		}
	if (!v)
	{
		rend_vk_set_diagnostic(REND_DIAG_CAPACITY, 0,
				       "texture-view capacity exhausted");
		return 0;
	}
	memset(&ci, 0, sizeof(ci));
	ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	ci.image = t->image;
	ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
	ci.format = rend_vk_format(d->format);
	ci.subresourceRange.aspectMask = d->aspect == REND_ASPECT_DEPTH
						 ? VK_IMAGE_ASPECT_DEPTH_BIT
						 : VK_IMAGE_ASPECT_COLOR_BIT;
	ci.subresourceRange.baseMipLevel = d->base_mip;
	ci.subresourceRange.levelCount = d->mip_count;
	ci.subresourceRange.baseArrayLayer = d->base_layer;
	ci.subresourceRange.layerCount = d->layer_count;
	r = vkCreateImageView(b->device, &ci, NULL, &v->view);
	if (r != VK_SUCCESS)
	{
		rend_vk_set_diagnostic(REND_DIAG_NATIVE, r, "vkCreateImageView failed");
		return 0;
	}
	v->backend = b;
	v->texture = t;
	v->desc = *d;
	v->width = t->desc.width;
	v->height = t->desc.height;
	v->format = (RendFormat)t->desc.format;
	v->live = 1;
	*out = v;
	return 1;
fail:
	rend_vk_set_diagnostic(REND_DIAG_INVALID_ARGUMENT, 0,
			       "invalid texture view description");
	return 0;
}
int
rend_texture_view_destroy(RendBackend b, RendTextureView v)
{
	if (!b || b->magic != REND_BACKEND_MAGIC || !v || v->backend != b ||
	    !v->live || v->external || v->refs)
		return 0;
	vkDestroyImageView(b->device, v->view, NULL);
	memset(v, 0, sizeof(*v));
	return 1;
}
int
rend_texture_destroy(RendBackend b, RendTexture t)
{
	uint32_t i;
	if (!b || b->magic != REND_BACKEND_MAGIC || !t || t->backend != b ||
	    !t->live || t->refs)
		return 0;
	for (i = 0; i < b->view_count; i++)
		if (b->views[i].live && b->views[i].texture == t)
		{
			rend_vk_set_diagnostic(REND_DIAG_INVALID_ARGUMENT, 0,
					       "texture still has live views");
			return 0;
		}
	vkDestroyImage(b->device, t->image, NULL);
	if (t->heap->refs)
		t->heap->refs--;
	memset(t, 0, sizeof(*t));
	return 1;
}
