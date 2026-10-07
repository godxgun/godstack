/* Vulkan device and queue-family discovery only. */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <vulkan/vulkan.h>

#include "rend_internal.h"

int
rend_discovery_create(void *storage, size_t storage_size,
		      RendQueueFamilyInfo *families,
		      uint32_t family_capacity, RendDiscovery **out)
{
	return rend_discovery_create_with_extensions(storage, storage_size, families,
						     family_capacity, NULL, 0, out);
}

int
rend_discovery_create_with_extensions(void *storage, size_t storage_size,
				      RendQueueFamilyInfo *families,
				      uint32_t family_capacity,
				      const char *const *extensions,
				      uint32_t extension_count,
				      RendDiscovery **out)
{
	RendDiscovery *d;
	VkApplicationInfo app;
	VkInstanceCreateInfo ici;
	VkPhysicalDevice physical[REND_DISCOVERY_DEVICE_CAPACITY];
	uint32_t count = 0, total_families = 0, i;
	VkResult result;

	if (out)
		*out = NULL;
	if (!storage || !out || storage_size < sizeof(RendDiscovery) ||
	    (uintptr_t)storage % REND_DISCOVERY_ALIGNMENT ||
	    (!families && family_capacity) || extension_count > 16 ||
	    (!extensions && extension_count))
	{
		rend_vk_set_diagnostic(
			REND_DIAG_INVALID_ARGUMENT, 0,
			"invalid or insufficient discovery backing / output");
		return 0;
	}

	d = (RendDiscovery *)storage;
	memset(d, 0, sizeof(*d));
	memset(&app, 0, sizeof(app));
	app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app.pApplicationName = "Rend2";
	app.apiVersion = VK_API_VERSION_1_4;
	memset(&ici, 0, sizeof(ici));
	ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	ici.pApplicationInfo = &app;
	ici.enabledExtensionCount = extension_count;
	ici.ppEnabledExtensionNames = extensions;
	result = vkCreateInstance(&ici, NULL, &d->instance);
	if (result != VK_SUCCESS)
	{
		d->instance = VK_NULL_HANDLE;
		memset(d, 0, sizeof(*d));
		rend_vk_set_diagnostic(REND_DIAG_NATIVE, result,
				       "vkCreateInstance Vulkan 1.4 failed");
		return 0;
	}

	result = vkEnumeratePhysicalDevices(d->instance, &count, NULL);
	if (result != VK_SUCCESS || count > REND_DISCOVERY_DEVICE_CAPACITY)
	{
		vkDestroyInstance(d->instance, NULL);
		memset(d, 0, sizeof(*d));
		rend_vk_set_diagnostic(
			result == VK_SUCCESS ? REND_DIAG_CAPACITY : REND_DIAG_NATIVE, result,
			"physical device count unavailable or exceeds fixed discovery "
			"capacity");
		return 0;
	}
	if (count && (result = vkEnumeratePhysicalDevices(d->instance, &count,
							  physical)) != VK_SUCCESS)
	{
		vkDestroyInstance(d->instance, NULL);
		memset(d, 0, sizeof(*d));
		rend_vk_set_diagnostic(REND_DIAG_NATIVE, result,
				       "physical device enumeration failed");
		return 0;
	}

	for (i = 0; i < count; i++)
	{
		VkPhysicalDeviceProperties properties;
		VkQueueFamilyProperties queue_properties[REND_QUEUE_FAMILY_CAPACITY];
		uint32_t queue_count = 0, j, graphics_compute_queue = 0;
		struct RendDeviceInfo *device = &d->devices[i];

		vkGetPhysicalDeviceProperties(physical[i], &properties);
		device->properties = properties;
		vkGetPhysicalDeviceMemoryProperties(physical[i], &device->memory);
		device->heap_properties.sType =
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_PROPERTIES_EXT;
		device->physical = physical[i];
		device->api_version = properties.apiVersion;
		snprintf(device->name, sizeof(device->name), "%s", properties.deviceName);
		{
			VkExtensionProperties available[256];
			uint32_t n = 0, k;
			int descriptor_heap = 0, address_commands = 0, untyped = 0;
			VkPhysicalDeviceDescriptorHeapFeaturesEXT heap = {
				.sType =
					VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_FEATURES_EXT};
			VkPhysicalDeviceDeviceAddressCommandsFeaturesKHR address = {
				.sType =
					VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_ADDRESS_COMMANDS_FEATURES_KHR,
				.pNext = &heap};
			VkPhysicalDeviceShaderUntypedPointersFeaturesKHR pointers = {
				.sType =
					VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_UNTYPED_POINTERS_FEATURES_KHR,
				.pNext = &address};
			VkPhysicalDeviceVulkan11Features f11 = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
				.pNext = &pointers};
			VkPhysicalDeviceVulkan12Features f12 = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
				.pNext = &f11};
			VkPhysicalDeviceVulkan13Features f13 = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
				.pNext = &f12};
			VkPhysicalDeviceFeatures2 f2 = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &f13};
			if (vkEnumerateDeviceExtensionProperties(physical[i], NULL, &n, NULL) ==
				    VK_SUCCESS &&
			    n <= 256 &&
			    vkEnumerateDeviceExtensionProperties(physical[i], NULL, &n,
								 available) == VK_SUCCESS)
			{
				for (k = 0; k < n; k++)
				{
					descriptor_heap |= !strcmp(available[k].extensionName,
								   VK_EXT_DESCRIPTOR_HEAP_EXTENSION_NAME);
					address_commands |=
						!strcmp(available[k].extensionName,
							VK_KHR_DEVICE_ADDRESS_COMMANDS_EXTENSION_NAME);
					untyped |= !strcmp(available[k].extensionName,
							   VK_KHR_SHADER_UNTYPED_POINTERS_EXTENSION_NAME);
					device->swapchain_extension_supported |=
						!strcmp(available[k].extensionName,
							VK_KHR_SWAPCHAIN_EXTENSION_NAME);
				}
				if (!descriptor_heap)
					device->missing_extensions |= REND_MISSING_DESCRIPTOR_HEAP;
				if (!address_commands)
					device->missing_extensions |= REND_MISSING_ADDRESS_COMMANDS;
				if (!untyped)
					device->missing_extensions |= REND_MISSING_UNTYPED_POINTERS;
				if (properties.apiVersion < VK_API_VERSION_1_4)
					device->missing_features |= REND_MISSING_API_14;
				if (descriptor_heap && address_commands && untyped)
				{
					VkPhysicalDeviceProperties2 props2 = {
						.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
						.pNext = &device->heap_properties};
					vkGetPhysicalDeviceProperties2(physical[i], &props2);
					vkGetPhysicalDeviceFeatures2(physical[i], &f2);
					if (!heap.descriptorHeap)
						device->missing_features |= REND_MISSING_DESCRIPTOR_HEAP_FEATURE;
					if (!address.deviceAddressCommands)
						device->missing_features |= REND_MISSING_ADDRESS_COMMANDS_FEATURE;
					if (!pointers.shaderUntypedPointers)
						device->missing_features |= REND_MISSING_UNTYPED_POINTERS_FEATURE;
					if (properties.limits.minUniformBufferOffsetAlignment > UINT32_MAX)
						device->missing_features |= REND_MISSING_ROOT_ALIGNMENT;
					if (!f11.shaderDrawParameters)
						device->missing_features |= REND_MISSING_SHADER_DRAW_PARAMETERS;
					if (!f12.bufferDeviceAddress)
						device->missing_features |= REND_MISSING_BUFFER_DEVICE_ADDRESS;
					if (!f12.scalarBlockLayout)
						device->missing_features |= REND_MISSING_SCALAR_BLOCK_LAYOUT;
					if (!f12.timelineSemaphore)
						device->missing_features |= REND_MISSING_TIMELINE_SEMAPHORE;
					if (!f13.synchronization2)
						device->missing_features |= REND_MISSING_SYNCHRONIZATION2;
					if (!f13.dynamicRendering)
						device->missing_features |= REND_MISSING_DYNAMIC_RENDERING;
					if (!f2.features.shaderInt64)
						device->missing_features |= REND_MISSING_SHADER_INT64;
					if (!f2.features.shaderStorageImageWriteWithoutFormat)
						device->missing_features |= REND_MISSING_STORAGE_IMAGE_WRITE;
					if (device->heap_properties.maxPushDataSize < sizeof(uint64_t))
						device->missing_features |= REND_MISSING_PUSH_DATA_SIZE;
					device->graphics_compute = !device->missing_extensions &&
								   !device->missing_features;
				}
				device->full_profile = 0;
			}
		}
		vkGetPhysicalDeviceQueueFamilyProperties(physical[i], &queue_count, NULL);
		if (queue_count > REND_QUEUE_FAMILY_CAPACITY)
		{
			vkDestroyInstance(d->instance, NULL);
			memset(d, 0, sizeof(*d));
			rend_vk_set_diagnostic(
				REND_DIAG_CAPACITY, 0,
				"device exceeds fixed 128 queue-family discovery limit");
			return 0;
		}
		vkGetPhysicalDeviceQueueFamilyProperties(physical[i], &queue_count,
							 queue_properties);
		if (queue_count > UINT32_MAX - total_families)
		{
			vkDestroyInstance(d->instance, NULL);
			memset(d, 0, sizeof(*d));
			rend_vk_set_diagnostic(REND_DIAG_OVERFLOW, 0,
					       "queue family count overflow");
			return 0;
		}
		device->family_offset = total_families;
		device->family_count = queue_count;
		total_families += queue_count;
		if (family_capacity < total_families || (!families && total_families))
		{
			vkDestroyInstance(d->instance, NULL);
			memset(d, 0, sizeof(*d));
			rend_vk_set_diagnostic(
				REND_DIAG_CAPACITY, 0,
				"family output capacity too small; no discovery object published");
			return 0;
		}
		device->family_records =
			queue_count && families ? families + device->family_offset : NULL;
		for (j = 0; j < queue_count; j++)
		{
			uint32_t capabilities = 0;
			if (queue_properties[j].queueFlags & VK_QUEUE_GRAPHICS_BIT)
				capabilities |= REND_QUEUE_GRAPHICS;
			if (queue_properties[j].queueFlags & VK_QUEUE_COMPUTE_BIT)
				capabilities |= REND_QUEUE_COMPUTE;
			if (queue_properties[j].queueFlags & VK_QUEUE_TRANSFER_BIT)
				capabilities |= REND_QUEUE_TRANSFER;
			if (queue_properties[j].queueCount &&
			    (capabilities & (REND_QUEUE_GRAPHICS | REND_QUEUE_COMPUTE)) ==
				    (REND_QUEUE_GRAPHICS | REND_QUEUE_COMPUTE))
				graphics_compute_queue = 1;
			families[device->family_offset + j].family_index = j;
			families[device->family_offset + j].queue_count =
				queue_properties[j].queueCount;
			families[device->family_offset + j].capabilities = capabilities;
		}
		device->graphics_compute &= graphics_compute_queue;
		if (!graphics_compute_queue)
			device->missing_queue_caps |= REND_MISSING_GRAPHICS_COMPUTE_QUEUE;
	}

	d->families = families;
	d->family_capacity = family_capacity;
	d->family_count = total_families;
	d->device_count = count;
	for (i = 0; i < count; i++)
		d->devices[i].owner = d;
	d->magic = REND_DISCOVERY_MAGIC;
	*out = d;
	rend_vk_set_diagnostic(REND_DIAG_NONE, 0, "");
	return 1;
}

int
rend_vk_surface_support(const RendDeviceInfo *device, uint64_t surface,
			uint32_t family, int *supported)
{
	VkSurfaceKHR native;
	VkBool32 result = VK_FALSE;
	if (supported)
		*supported = 0;
	if (!device || !surface || !supported || family >= device->family_count)
	{
		rend_vk_set_diagnostic(REND_DIAG_INVALID_ARGUMENT, 0,
				       "invalid surface-support query");
		return 0;
	}
	memcpy(&native, &surface, sizeof(native));
	if (vkGetPhysicalDeviceSurfaceSupportKHR(device->physical, family, native,
						 &result) != VK_SUCCESS)
	{
		rend_vk_set_diagnostic(REND_DIAG_NATIVE, 0,
				       "vkGetPhysicalDeviceSurfaceSupportKHR failed");
		return 0;
	}
	*supported = result != VK_FALSE;
	return 1;
}

int
rend_vk_discovery_instance(const RendDiscovery *d,
			   VkInstance *out_instance)
{
	if (out_instance)
		*out_instance = VK_NULL_HANDLE;
	if (!d || d->magic != REND_DISCOVERY_MAGIC || !out_instance)
	{
		rend_vk_set_diagnostic(REND_DIAG_INVALID_ARGUMENT, 0,
				       "invalid discovery instance query");
		return 0;
	}
	*out_instance = d->instance;
	rend_vk_set_diagnostic(REND_DIAG_NONE, 0, "");
	return 1;
}

int
rend_discovery_destroy(RendDiscovery *d)
{
	if (!d || d->magic != REND_DISCOVERY_MAGIC)
	{
		rend_vk_set_diagnostic(REND_DIAG_INVALID_ARGUMENT, 0,
				       "invalid discovery object");
		return 0;
	}
	if (d->context_count)
	{
		rend_vk_set_diagnostic(REND_DIAG_INVALID_ARGUMENT, 0,
				       "discovery still owns live device contexts");
		return 0;
	}
	d->magic = 0;
	vkDestroyInstance(d->instance, NULL);
	d->instance = VK_NULL_HANDLE;
	d->families = NULL;
	d->device_count = 0;
	d->family_count = 0;
	rend_vk_set_diagnostic(REND_DIAG_NONE, 0, "");
	return 1;
}
