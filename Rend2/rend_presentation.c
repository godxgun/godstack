/* Narrow Vulkan WSI bridge for a caller-owned VkSurfaceKHR. */
#include "rend_internal.h"
#include <string.h>

int
rend_device_surface_support(const RendDeviceInfo *d, uint64_t surface,
			    uint32_t family, int *out)
{
	return rend_vk_surface_support(d, surface, family, out);
}
static RendFormat
rend_wsi_rend_format(VkFormat f)
{
	return f == VK_FORMAT_R8G8B8A8_UNORM   ? REND_FORMAT_RGBA8_UNORM
	       : f == VK_FORMAT_B8G8R8A8_UNORM ? REND_FORMAT_BGRA8_UNORM
					       : REND_FORMAT_NONE;
}
static void
rend_presentation_cleanup(RendPresentationRecord *p)
{
	RendBackend b = p->backend;
	uint32_t i;
	for (i = 0; i < p->image_count; i++)
	{
		memset(&p->view_records[i], 0, sizeof(p->view_records[i]));
		if (p->views[i])
			vkDestroyImageView(b->device, p->views[i], NULL);
		if (p->acquire_semaphores[i])
			vkDestroySemaphore(b->device, p->acquire_semaphores[i], NULL);
		if (p->render_semaphores[i])
			vkDestroySemaphore(b->device, p->render_semaphores[i], NULL);
	}
	if (p->swapchain)
		vkDestroySwapchainKHR(b->device, p->swapchain, NULL);
	p->live = 0;
}
int
rend_presentation_create(RendBackend b, uint64_t native_surface,
			 uint32_t width, uint32_t height,
			 RendPresentation *out)
{
	VkSurfaceKHR surface;
	VkSurfaceCapabilitiesKHR caps;
	VkSurfaceFormatKHR formats[512], selected;
	VkSwapchainCreateInfoKHR ci;
	VkResult r;
	uint32_t count = 0, i;
	int supported = 0;
	RendPresentationRecord *p = NULL;
	VkExtent2D extent;
	if (out)
		*out = NULL;
	if (!out || !b || b->magic != REND_BACKEND_MAGIC || !b->presentation_count ||
	    !native_surface || !width || !height || b->terminal)
	{
		rend_vk_set_diagnostic(REND_DIAG_INVALID_ARGUMENT, 0,
				       "invalid presentation configuration");
		return 0;
	}
	memcpy(&surface, &native_surface, sizeof(surface));
	for (i = 0; i < b->presentation_count; i++)
		if (!b->presentations[i].live)
		{
			p = &b->presentations[i];
			break;
		}
	if (!p)
	{
		rend_vk_set_diagnostic(REND_DIAG_CAPACITY, 0,
				       "presentation record capacity exhausted");
		return 0;
	}
	if (!b->queue_count ||
	    !rend_vk_surface_support(b->device_info, native_surface,
				     b->queues[0].family, &supported) ||
	    !supported)
	{
		rend_vk_set_diagnostic(REND_DIAG_UNSUPPORTED, 0,
				       "configured queue zero does not support this "
				       "surface; choose a compatible family");
		return 0;
	}
	r = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(b->physical, surface, &caps);
	if (r != VK_SUCCESS)
		goto native;
	r = vkGetPhysicalDeviceSurfaceFormatsKHR(b->physical, surface, &count, NULL);
	if (r != VK_SUCCESS)
		goto native;
	if (!count || count > 512) {
		rend_vk_set_diagnostic(REND_DIAG_CAPACITY, 0, "surface format count exceeds bounded 512-format workspace or is empty");
		return 0;
	}
	r = vkGetPhysicalDeviceSurfaceFormatsKHR(b->physical, surface, &count,
						 formats);
	if (r != VK_SUCCESS)
		goto native;
	selected = formats[0];
	if (count == 1 && selected.format == VK_FORMAT_UNDEFINED)
		selected.format = VK_FORMAT_R8G8B8A8_UNORM;
	for (i = 0; i < count; i++)
		if (formats[i].format == VK_FORMAT_R8G8B8A8_UNORM)
		{
			selected = formats[i];
			break;
		}
	if (selected.format != VK_FORMAT_R8G8B8A8_UNORM)
	{
		for (i = 0; i < count; i++)
			if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM)
			{
				selected = formats[i];
				break;
			}
	}
	if (rend_wsi_rend_format(selected.format) == REND_FORMAT_NONE)
	{
		rend_vk_set_diagnostic(REND_DIAG_UNSUPPORTED, 0,
				       "surface offers neither RGBA8 nor BGRA8 UNORM");
		return 0;
	}
	if (caps.currentExtent.width != UINT32_MAX)
		extent = caps.currentExtent;
	else
	{
		extent.width = width < caps.minImageExtent.width ? caps.minImageExtent.width
			       : width > caps.maxImageExtent.width
				       ? caps.maxImageExtent.width
				       : width;
		extent.height =
			height < caps.minImageExtent.height   ? caps.minImageExtent.height
			: height > caps.maxImageExtent.height ? caps.maxImageExtent.height
							      : height;
	}
	if (!extent.width || !extent.height)
	{
		rend_vk_set_diagnostic(REND_DIAG_OUT_OF_DATE, 0,
				       "surface minimized to zero extent");
		return 0;
	}
	memset(&ci, 0, sizeof(ci));
	ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	ci.surface = surface;
	ci.minImageCount = caps.minImageCount + 1;
	if (caps.maxImageCount && ci.minImageCount > caps.maxImageCount)
		ci.minImageCount = caps.maxImageCount;
	if (ci.minImageCount > 8 && caps.minImageCount <= 8)
		ci.minImageCount = 8;
	if (ci.minImageCount > 8)
	{
		rend_vk_set_diagnostic(
			REND_DIAG_CAPACITY, 0,
			"swapchain image count exceeds fixed eight-image capacity");
		return 0;
	}
	ci.imageFormat = selected.format;
	ci.imageColorSpace = selected.colorSpace;
	ci.imageExtent = extent;
	ci.imageArrayLayers = 1;
	if (!(caps.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT))
	{
		rend_vk_set_diagnostic(REND_DIAG_UNSUPPORTED, 0,
				       "surface lacks color-attachment swapchain usage");
		return 0;
	}
	ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ci.preTransform = caps.currentTransform;
	ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	if (!(caps.supportedCompositeAlpha & ci.compositeAlpha))
	{
		for (i = 0; i < 4; i++)
			if (caps.supportedCompositeAlpha & (1u << i))
			{
				ci.compositeAlpha = 1u << i;
				break;
			}
	}
	ci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
	ci.clipped = VK_TRUE;
	memset(p, 0, sizeof(*p));
	p->backend = b;
	p->surface = surface;
	p->format = rend_wsi_rend_format(selected.format);
	p->width = extent.width;
	p->height = extent.height;
	p->queue_index = 0;
	r = vkCreateSwapchainKHR(b->device, &ci, NULL, &p->swapchain);
	if (r != VK_SUCCESS)
		goto native;
	r = vkGetSwapchainImagesKHR(b->device, p->swapchain, &count, NULL);
	if (r != VK_SUCCESS || !count || count > 8)
		goto cleanup;
	r = vkGetSwapchainImagesKHR(b->device, p->swapchain, &count, p->images);
	if (r != VK_SUCCESS)
		goto cleanup;
	p->image_count = count;
	for (i = 0; i < count; i++)
	{
		RendTextureViewRecord *v = &p->view_records[i];
		VkImageViewCreateInfo vi;
		VkSemaphoreCreateInfo si;
		memset(&vi, 0, sizeof(vi));
		vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		vi.image = p->images[i];
		vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
		vi.format = selected.format;
		vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		vi.subresourceRange.levelCount = 1;
		vi.subresourceRange.layerCount = 1;
		r = vkCreateImageView(b->device, &vi, NULL, &p->views[i]);
		if (r != VK_SUCCESS)
			goto cleanup;
		v->live = 1;
		v->external = 1;
		v->backend = b;
		v->view = p->views[i];
		v->format = p->format;
		v->width = p->width;
		v->height = p->height;
		v->desc.format = p->format;
		v->desc.view_type = REND_VIEW_2D;
		v->desc.aspect = REND_ASPECT_COLOR;
		v->desc.mip_count = 1;
		v->desc.layer_count = 1;
		v->presentation = p;
		memset(&si, 0, sizeof(si));
		si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		r = vkCreateSemaphore(b->device, &si, NULL, &p->acquire_semaphores[i]);
		if (r != VK_SUCCESS)
			goto cleanup;
		r = vkCreateSemaphore(b->device, &si, NULL, &p->render_semaphores[i]);
		if (r != VK_SUCCESS)
			goto cleanup;
	}
	p->live = 1;
	*out = p;
	return 1;
cleanup:
	p->image_count = count <= 8 ? count : 0;
	rend_presentation_cleanup(p);
	rend_vk_set_diagnostic(REND_DIAG_NATIVE, r,
			       "swapchain image/view/semaphore setup failed; native "
			       "acquisitions unwound");
	return 0;
native:
	rend_vk_set_diagnostic(
		REND_DIAG_NATIVE, r,
		"surface capabilities/formats or vkCreateSwapchainKHR failed");
	return 0;
}
int
rend_presentation_get_info(RendPresentation p, RendPresentationInfo *out)
{
	if (!p || !p->live || !out)
		return 0;
	out->width = p->width;
	out->height = p->height;
	out->format = p->format;
	out->image_count = p->image_count;
	return 1;
}
int
rend_presentation_acquire(RendPresentation p, uint64_t timeout,
			  RendTextureView *out)
{
	VkResult r;
	uint32_t index, n, slot = UINT32_MAX;
	if (out)
		*out = NULL;
	if (!out || !p || !p->live || p->acquired || p->backend->terminal)
		return 0;
	for (n = 0; n < p->image_count; n++)
	{
		uint32_t candidate = (p->next_acquire_slot + n) % p->image_count;
		if (p->acquire_timeline[candidate])
		{
			uint64_t done = 0;
			if (vkGetSemaphoreCounterValue(p->backend->device,
						       p->acquire_timeline[candidate]->semaphore,
						       &done) == VK_SUCCESS &&
			    done >= p->acquire_values[candidate])
				p->acquire_timeline[candidate] = NULL;
		}
		if (!p->acquire_timeline[candidate])
		{
			slot = candidate;
			break;
		}
	}
	if (slot == UINT32_MAX)
	{
		rend_vk_set_diagnostic(REND_DIAG_TIMEOUT, VK_NOT_READY,
				       "all acquire semaphores are still in flight");
		return 0;
	}
	r = vkAcquireNextImageKHR(p->backend->device, p->swapchain, timeout,
				  p->acquire_semaphores[slot], VK_NULL_HANDLE,
				  &index);
	if (r == VK_TIMEOUT || r == VK_NOT_READY)
	{
		rend_vk_set_diagnostic(REND_DIAG_TIMEOUT, r,
				       "swapchain image acquire timed out/no image");
		return 0;
	}
	if (r == VK_ERROR_OUT_OF_DATE_KHR)
	{
		rend_vk_set_diagnostic(
			REND_DIAG_OUT_OF_DATE, r,
			"swapchain out of date; explicitly drain/destroy/recreate");
		return 0;
	}
	if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR)
	{
		p->backend->terminal = 1;
		rend_vk_set_diagnostic(REND_DIAG_TERMINAL, r,
				       "native swapchain acquire failed");
		return 0;
	}
	p->image_index = index;
	p->acquire_slot = slot;
	p->next_acquire_slot = (slot + 1) % p->image_count;
	p->acquired = 1;
	p->submitted = 0;
	p->backend->drained = 0;
	*out = &p->view_records[index];
	return 1;
}
int
rend_presentation_present(RendPresentation p, RendQueue q)
{
	VkPresentInfoKHR info;
	VkResult r;
	VkSwapchainKHR swap;
	uint32_t index;
	if (!p || !p->live || !p->acquired || !p->submitted || !q ||
	    q->backend != p->backend ||
	    q->family != p->backend->queues[p->queue_index].family)
		return 0;
	swap = p->swapchain;
	index = p->image_index;
	memset(&info, 0, sizeof(info));
	info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	info.waitSemaphoreCount = 1;
	info.pWaitSemaphores = &p->render_semaphores[index];
	info.swapchainCount = 1;
	info.pSwapchains = &swap;
	info.pImageIndices = &index;
	r = vkQueuePresentKHR(q->queue, &info);
	if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR &&
	    r != VK_ERROR_OUT_OF_DATE_KHR)
	{
		p->backend->terminal = 1;
		rend_vk_set_diagnostic(REND_DIAG_TERMINAL, r, "vkQueuePresentKHR failed");
		return 0;
	}
	p->acquired = 0;
	p->submitted = 0;
	p->needs_retire = 1;
	p->backend->drained = 0;
	if (r == VK_ERROR_OUT_OF_DATE_KHR)
	{
		rend_vk_set_diagnostic(REND_DIAG_OUT_OF_DATE, r,
				       "presentation out of date; explicitly recreate");
		return 0;
	}
	return 1;
}
int
rend_presentation_destroy(RendPresentation p)
{
	uint32_t i;
	if (!p || !p->live)
		return 0;
	if ((p->acquired || p->needs_retire) && !p->backend->drained)
	{
		rend_vk_set_diagnostic(
			REND_DIAG_INVALID_ARGUMENT, 0,
			"presentation must be explicitly wait_idle-drained before retirement");
		return 0;
	}
	for (i = 0; i < p->image_count; i++)
		if (p->view_records[i].refs)
		{
			rend_vk_set_diagnostic(REND_DIAG_INVALID_ARGUMENT, 0,
					       "presentation view still referenced by recorded "
					       "command lists; reset those pools");
			return 0;
		}
	rend_presentation_cleanup(p);
	return 1;
}
