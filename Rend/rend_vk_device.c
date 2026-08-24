#pragma once
#include "rend_internal.h"
#include "rend_vk_internal.h"
#include <vulkan/vulkan_core.h>

// struct RendDevice {
//     bool anisotropy_support;
//     bool bindless_support;
// };


static bool
rend_vk_device_create(VkSurfaceKHR surface, RendSpecs specs, RendVkDevice *out_device, uint32_t (*score_devices)(RendVkDevice *, RendSpecs, const char **, uint32_t))
{
    assert(out_device);

    uint32_t device_count = 0;
    CHECK_VK_RESULT(vkEnumeratePhysicalDevices(vk_instance, &device_count, VK_NULL_HANDLE));
    if (device_count == 0) {
        PFATAL("No GPU with Vulkan support found!");
        return false;
    }

    VkPhysicalDevice physical_devices[device_count];
    CHECK_VK_RESULT(vkEnumeratePhysicalDevices(vk_instance, &device_count, physical_devices));

    const char *extension_names[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    uint32_t best_score = 1; // 1 so that devices that do not meet specs get ignored
    RendVkDevice best_device = {0};

    PDEBUG("DEVICE                SCORE");
    for (uint32_t i = 0; i < device_count; i++) {

        /* populate device struct */
        RendVkDevice scoring = {0};
        scoring.surface = surface;
        scoring.physical_device = physical_devices[i];
        vkGetPhysicalDeviceProperties(physical_devices[i], &scoring.properties);
        vkGetPhysicalDeviceFeatures(physical_devices[i], &scoring.features);
        vkGetPhysicalDeviceMemoryProperties(physical_devices[i], &scoring.memory);

        /* score device */
        uint32_t dev_score = score_devices(&scoring, specs, extension_names, 1);
        PDEBUG("%-20.20s  %5d", scoring.properties.deviceName, dev_score);
        if (dev_score >= best_score) {
            if (best_device.swapchain_support.format) {
                rfree(best_device.swapchain_support.format);
            }
            if (best_device.swapchain_support.present_modes) {
                rfree(best_device.swapchain_support.present_modes);
            }
            best_score = dev_score;
            best_device = scoring;
        } else {
            if (scoring.swapchain_support.format) {
                rfree(scoring.swapchain_support.format);
            }
            if (scoring.swapchain_support.present_modes) {
                rfree(scoring.swapchain_support.present_modes);
            }
        }

    }

    if (best_score > 1) {
        PDEBUG("Driver version %d.%d.%d", VK_VERSION_MAJOR(best_device.properties.driverVersion), VK_VERSION_MINOR(best_device.properties.driverVersion), VK_VERSION_PATCH(best_device.properties.driverVersion));
        PDEBUG("Vulkan API version %d.%d.%d", VK_VERSION_MAJOR(best_device.properties.apiVersion), VK_VERSION_MINOR(best_device.properties.apiVersion), VK_VERSION_PATCH(best_device.properties.apiVersion));
    }

    if (!best_device.physical_device) {
        PERROR("No physical devices were found that meet specs!");
        return false;
    }

    /* update out device to use best selected device */
    *out_device = best_device;

    PDEBUG("Graphics Family Index: %u", out_device->graphics_family_index);
    PDEBUG("Present Family Index: %u",  out_device->present_family_index);
    PDEBUG("Compute Family Index: %u",  out_device->compute_family_index);
    PDEBUG("Transfer Family Index: %u", out_device->transfer_family_index);

    bool present_shares_graphics_q = out_device->present_family_index == out_device->graphics_family_index;
    bool transfer_shares_graphics_q = out_device->transfer_family_index == out_device->graphics_family_index;

    uint32_t index_count = 1;
    if (!present_shares_graphics_q) index_count++;
    if (!transfer_shares_graphics_q) index_count++;

    uint32_t indices[index_count];
    uint8_t index = 0;

    indices[index++] = out_device->graphics_family_index;
    if (!present_shares_graphics_q) {
        indices[index++] = out_device->present_family_index;
    }
    if (!transfer_shares_graphics_q) {
        indices[index++] = out_device->transfer_family_index;
    }

    VkDeviceQueueCreateInfo q_create_info[index_count];

    float queue_priority[2] = {1.0f, 1.0f};
    for (uint32_t i = 0; i < index_count; i++) {
        q_create_info[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        q_create_info[i].queueFamilyIndex = indices[i];
        // TODO: request 2 queues when possible
        // q_create_info[i].queueCount = (indices[i] == out_device->graphics_family_index) ? 2 : 1;
        q_create_info[i].queueCount = 1;
        q_create_info[i].flags = 0;
        q_create_info[i].pNext = 0;
        q_create_info[i].pQueuePriorities = queue_priority;
    }

    // TODO: driven by the same config as the check_specs function
    // used earlier

    VkPhysicalDeviceFeatures device_features = {0};
    device_features.samplerAnisotropy = specs.sampler_anisotropy;
    device_features.fillModeNonSolid = VK_TRUE;
    device_features.shaderInt64 = VK_TRUE;

    // vulkan 1.3 features (dynamic rendering + sync2)
    VkPhysicalDeviceVulkan13Features vk13_features = {0};
    vk13_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    vk13_features.dynamicRendering = VK_TRUE;
    vk13_features.synchronization2 = VK_TRUE;

    // vulkan 1.2 features (timeline semaphores)
    VkPhysicalDeviceVulkan12Features vk12_features = {0};
    vk12_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vk12_features.timelineSemaphore = VK_TRUE;

    // vk12_features.descriptorIndexing = VK_TRUE;
    // vk12_features.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
    vk12_features.descriptorBindingPartiallyBound = VK_TRUE;
    // vk12_features.descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE;
    // vk12_features.descriptorBindingVariableDescriptorCount = VK_TRUE;
    // vk12_features.runtimeDescriptorArray = VK_TRUE;

    vk12_features.bufferDeviceAddress = VK_TRUE;
    vk12_features.scalarBlockLayout = VK_TRUE;
    vk12_features.pNext = &vk13_features;

    // vulkan 1.1 features (draw parameters)
    VkPhysicalDeviceVulkan11Features vk11_features = {0};
    vk11_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    vk11_features.shaderDrawParameters = VK_TRUE;
    vk11_features.pNext = &vk12_features;

    VkDeviceCreateInfo device_create_info = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    device_create_info.pNext = &vk11_features;
    device_create_info.queueCreateInfoCount = index_count;
    device_create_info.pQueueCreateInfos = q_create_info;
    device_create_info.pEnabledFeatures = &device_features;
    device_create_info.enabledExtensionCount = 1;

    const char *extention_names = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    device_create_info.ppEnabledExtensionNames = &extention_names;

    /* deprecated and ignored */
    device_create_info.enabledLayerCount = 0;
    device_create_info.ppEnabledLayerNames = 0;

    CHECK_VK_RESULT(vkCreateDevice(out_device->physical_device, &device_create_info, vk_allocator, &out_device->logical_device));

    vkGetDeviceQueue(out_device->logical_device, out_device->graphics_family_index, 0, &out_device->graphics_queue);
    vkGetDeviceQueue(out_device->logical_device, out_device->present_family_index, 0, &out_device->present_queue);
    vkGetDeviceQueue(out_device->logical_device, out_device->transfer_family_index, 0, &out_device->transfer_queue);

    PDEBUG("GRAPHICS | PRESENT | COMPUTE | TRANSFER | DEVICE");
    PDEBUG("      %02d |      %02d |      %02d |       %02d | %s",
            out_device->graphics_family_index != UINT32_MAX,
            out_device->present_family_index  != UINT32_MAX,
            out_device->compute_family_index  != UINT32_MAX,
            out_device->transfer_family_index != UINT32_MAX,
            out_device->properties.deviceName);

    VkPhysicalDeviceMemoryProperties mem_props = out_device->memory;
    out_device->device_index = UINT32_MAX;
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        if ((mem_props.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
            out_device->device_index = i;
            break;
        }
    }

    VkMemoryPropertyFlags host_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    out_device->host_index = UINT32_MAX;
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        if ((mem_props.memoryTypes[i].propertyFlags & host_flags) == host_flags) {
            out_device->host_index = i;
            break;
        }
    }

    PDEBUG("Device local heap: %2u", out_device->device_index);
    PDEBUG("Host mapped heap:  %2u", out_device->host_index);
    return true;
}

static void
rend_vk_device_destroy(RendVkDevice *device)
{
    RASSERT(device && device->logical_device, "Invalid or uninitialized device.");

    vkDeviceWaitIdle(device->logical_device); // safe when destroying device
    vkDestroyDevice(device->logical_device, vk_allocator);
    if (device->swapchain_support.format) rfree(vk_device.swapchain_support.format);
    if (device->swapchain_support.present_modes) rfree(vk_device.swapchain_support.present_modes);

    *device = (RendVkDevice){0};
}

static void
rend_vk_device_query_swapchain_support(RendVkDevice *device) 
{
    RASSERT(device->physical_device, "Invalid device pointer.");

    /* surface capabilities */
    CHECK_VK_RESULT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->physical_device, device->surface, &device->swapchain_support.capabilities));

    /* surface formats */
    CHECK_VK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR(device->physical_device, device->surface, &device->swapchain_support.format_count, VK_NULL_HANDLE));
    if (device->swapchain_support.format_count != 0) {
        if (!device->swapchain_support.format) {
            device->swapchain_support.format = rmalloc(device->swapchain_support.format_count * sizeof(*device->swapchain_support.format));
        }
        CHECK_VK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR(device->physical_device, device->surface, &device->swapchain_support.format_count, device->swapchain_support.format));
    }

    /* present modes */
    CHECK_VK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(device->physical_device, device->surface, &device->swapchain_support.present_mode_count, VK_NULL_HANDLE));
    if (device->swapchain_support.present_mode_count != 0) {
        if (!device->swapchain_support.present_modes) {
            device->swapchain_support.present_modes = rmalloc(device->swapchain_support.present_mode_count * sizeof(*device->swapchain_support.present_modes));
        }
        CHECK_VK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(device->physical_device, device->surface, &device->swapchain_support.present_mode_count, device->swapchain_support.present_modes));
    }
}

static bool
rend_vk_device_detect_depth_format(RendVkDevice *device)
{
    const uint64_t candidate_count = 3;
    VkFormat candidates[] = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};

    uint32_t flags = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
    for (uint32_t i = 0; i < candidate_count; i++) {
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(device->physical_device, candidates[i], &properties);
        if ((properties.linearTilingFeatures & flags) == flags) {
            device->depth_format = candidates[i];
            return true;
        } else if ((properties.optimalTilingFeatures & flags) == flags) {
            device->depth_format = candidates[i];
            return true;
        }
    }

    return false;
}

static uint32_t
rend_vk_device_score_default(RendVkDevice *device, RendSpecs minimum_specs, const char **required_extensions, uint32_t required_extension_count)
{
    /* NOTE: When we score the device we will also check for specs. Not meeting a spec
     * leads to 0 score and continue. We will assume device already contains some information 
     * about properties when this function is called. */

    uint32_t score = 0;

    device->graphics_family_index = UINT32_MAX;
    device->present_family_index  = UINT32_MAX;
    device->compute_family_index  = UINT32_MAX;
    device->transfer_family_index = UINT32_MAX;

    if (minimum_specs.discrete_gpu) {
        if (device->properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            return 0;
        }
    }

    uint32_t q_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device->physical_device, &q_family_count, VK_NULL_HANDLE);

    VkQueueFamilyProperties q_family[q_family_count];
    vkGetPhysicalDeviceQueueFamilyProperties(device->physical_device, &q_family_count, q_family);

    uint8_t min_transfer_score = 255;
    for (uint32_t i = 0; i < q_family_count; i++) {

        uint8_t transfer_score = 0;
        if (q_family[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            device->graphics_family_index = i;
            transfer_score++;
        }

        if (q_family[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            device->compute_family_index = i;
            transfer_score++;
        }

        if (q_family[i].queueFlags & VK_QUEUE_TRANSFER_BIT) {
            // take the index if the transfer score is minimal
            if (transfer_score <= min_transfer_score) {
                min_transfer_score = transfer_score;
                device->transfer_family_index = i;
            }
        }

        VkBool32 supports_present = VK_FALSE;
        CHECK_VK_RESULT(vkGetPhysicalDeviceSurfaceSupportKHR(device->physical_device, i, device->surface, &supports_present));
        if (supports_present) {
            device->present_family_index = i;
        }
    }

    if (    (minimum_specs.graphics && device->graphics_family_index  == UINT32_MAX) ||
            (minimum_specs.present  && device->present_family_index   == UINT32_MAX) ||
            (minimum_specs.compute  && device->compute_family_index   == UINT32_MAX) ||
            (minimum_specs.transfer && device->transfer_family_index  == UINT32_MAX)) {
        return 0;
    }

    // swapchain support
    rend_vk_device_query_swapchain_support(device);

    if (device->swapchain_support.format_count < 1 || device->swapchain_support.present_mode_count < 1) {
        return 0;
    }

    // check for extension specs
    if (required_extensions) {
        uint32_t available_extentions_count = 0;
        VkExtensionProperties *available_extentions = NULL;
        CHECK_VK_RESULT(vkEnumerateDeviceExtensionProperties(device->physical_device, VK_NULL_HANDLE, &available_extentions_count, VK_NULL_HANDLE));

        if (available_extentions_count != 0) {

            available_extentions = rmalloc(available_extentions_count * sizeof(*available_extentions));
            CHECK_VK_RESULT(vkEnumerateDeviceExtensionProperties(device->physical_device, VK_NULL_HANDLE, &available_extentions_count, available_extentions));

            bool overall_found = true;
            for (uint32_t i = 0; i < required_extension_count; ++i) {
                bool found = false;
                for (uint32_t j = 0; j < available_extentions_count; ++j) {
                    if (strcmp(required_extensions[i], available_extentions[j].extensionName) == 0) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    overall_found = false;
                    break;
                }
            }

            rfree(available_extentions);
            if (!overall_found) {
                return 0;
            }
        }
    }

    if (minimum_specs.sampler_anisotropy && !device->features.samplerAnisotropy) {
        return 0;
    }

    score = 10;
    if (device->properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 1000;
    }

    return score;
}
