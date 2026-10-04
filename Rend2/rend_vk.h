/* Vulkan-only native bridge; include after Vulkan headers when using
 * VkSurfaceKHR. */
#ifndef REND2_REND_VK_H
#define REND2_REND_VK_H
#include <vulkan/vulkan.h>

#include "rend.h"
/* Surface lifetime is caller-owned and must outlive its RendPresentation. */
int rend_vk_discovery_instance(const RendDiscovery *discovery,
			       VkInstance *out_instance);
#endif
