/* Vulkan-only native bridge; include after Vulkan headers when passing
 * VkSurfaceKHR. */
#ifndef REND2_REND_VULKAN_H
#define REND2_REND_VULKAN_H
#include "rend.h"
#include <vulkan/vulkan.h>
/* Surface lifetime remains caller-owned and outlives its RendPresentation. */
int rend_vk_discovery_instance(const RendDiscovery *discovery,
			       VkInstance *out_instance);
#endif
