#include "vk_context.h"
#include "vk_utils.h"
#include <stdio.h>
#include <stdlib.h>

static void log_gpu_info(VkPhysicalDevice dev) {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(dev, &props);

    const char* type = "Unknown";
    switch (props.deviceType) {
    case VK_PHYSICAL_DEVICE_TYPE_OTHER: type = "Other"; break;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: type = "Integrated"; break;
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: type = "Discrete"; break;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: type = "Virtual"; break;
    case VK_PHYSICAL_DEVICE_TYPE_CPU: type = "CPU"; break;
    default: break;
    }

    printf("Using GPU: %s (%s) vendor=0x%04x device=0x%04x driver=0x%x api=%u.%u.%u\n",
           props.deviceName,
           type,
           props.vendorID,
           props.deviceID,
           props.driverVersion,
           VK_VERSION_MAJOR(props.apiVersion),
           VK_VERSION_MINOR(props.apiVersion),
           VK_VERSION_PATCH(props.apiVersion));
}

void vk_create_instance(VulkanRendererState* state) {
    VkApplicationInfo ai = { .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .pApplicationName = "vk_gui", .apiVersion = VK_API_VERSION_1_0 };
    VkInstanceCreateInfo ici = { .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &ai };
    
    /* request platform extensions */
    uint32_t extc = 0; 
    const char** exts = NULL;
    if (!state->get_required_instance_extensions || !state->get_required_instance_extensions(&exts, &extc)) {
        fatal("Failed to query platform Vulkan extensions");
    }
    ici.enabledExtensionCount = extc; 
    ici.ppEnabledExtensionNames = exts;
    
    double start = vk_now_ms();
    state->res = vkCreateInstance(&ici, NULL, &state->instance);
    vk_log_command(state, RENDER_LOG_INFO, "vkCreateInstance", "application", start);
    
    if (state->res != VK_SUCCESS) fatal_vk("vkCreateInstance", state->res);
}

void vk_pick_physical_and_create_device(VulkanRendererState* state) {
    uint32_t pc = 0; 
    vkEnumeratePhysicalDevices(state->instance, &pc, NULL); 
    if (pc == 0) fatal("No physical dev");
    
    VkPhysicalDevice* list = malloc(sizeof(VkPhysicalDevice) * pc); 
    vkEnumeratePhysicalDevices(state->instance, &pc, list);
    
    VkPhysicalDevice best_device = VK_NULL_HANDLE;
    int best_score = -1;

    for (uint32_t i = 0; i < pc; i++) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(list[i], &props);
        
        int score = 0;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score = 1000;
        else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score = 100;
        else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU) score = 50;
        else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) score = 1;

        printf("Candidate GPU [%u]: %s (Score: %d)\n", i, props.deviceName, score);
        
        if (score > best_score) {
            best_score = score;
            best_device = list[i];
        }
    }
    state->physical_device = best_device;
    free(list);

    log_gpu_info(state->physical_device);

    /* find queue family with graphics + present */
    uint32_t qcount = 0; 
    vkGetPhysicalDeviceQueueFamilyProperties(state->physical_device, &qcount, NULL);
    VkQueueFamilyProperties* qprops = malloc(sizeof(VkQueueFamilyProperties) * qcount); 
    vkGetPhysicalDeviceQueueFamilyProperties(state->physical_device, &qcount, qprops);
    
    int found = -1;
    for (uint32_t i = 0; i < qcount; i++) {
        VkBool32 pres = false; 
        vkGetPhysicalDeviceSurfaceSupportKHR(state->physical_device, i, state->surface, &pres);
        if ((qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && pres) { found = (int)i; break; }
    }
    free(qprops);

    if (found < 0) fatal("No suitable queue family");
    state->graphics_family = (uint32_t)found;

    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci = { .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueFamilyIndex = state->graphics_family, .queueCount = 1, .pQueuePriorities = &prio };
    const char* dev_ext[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkDeviceCreateInfo dci = { .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .queueCreateInfoCount = 1, .pQueueCreateInfos = &qci, .enabledExtensionCount = 1, .ppEnabledExtensionNames = dev_ext };
    
    state->res = vkCreateDevice(state->physical_device, &dci, NULL, &state->device);
    if (state->res != VK_SUCCESS) fatal_vk("vkCreateDevice", state->res);
    
    vkGetDeviceQueue(state->device, state->graphics_family, 0, &state->queue);
}

void vk_recreate_instance_and_surface(VulkanRendererState* state) {
    if (state->platform_surface && state->destroy_surface && state->instance) {
        state->destroy_surface(state->instance, NULL, state->platform_surface);
    } else if (state->surface && state->instance) {
        vkDestroySurfaceKHR(state->instance, state->surface, NULL);
    }
    state->surface = VK_NULL_HANDLE;
    if (state->instance) { 
        vkDestroyInstance(state->instance, NULL); 
        state->instance = VK_NULL_HANDLE; 
    }

    vk_create_instance(state);
    
    if (!state->create_surface || !state->platform_surface ||
        !state->create_surface(state->window, state->instance, NULL, state->platform_surface)) {
        fatal("Failed to recreate platform surface");
    }
    state->surface = (VkSurfaceKHR)(state->platform_surface ? state->platform_surface->handle : NULL);
}
