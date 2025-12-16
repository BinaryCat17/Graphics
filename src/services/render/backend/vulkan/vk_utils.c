#define _POSIX_C_SOURCE 200809L
#include "vk_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

void fatal(const char* msg) {
    fprintf(stderr, "Fatal: %s\n", msg);
    exit(1);
}

static const char* vk_result_name(VkResult r) {
    switch (r) {
    case VK_SUCCESS: return "VK_SUCCESS";
    case VK_NOT_READY: return "VK_NOT_READY";
    case VK_TIMEOUT: return "VK_TIMEOUT";
    case VK_EVENT_SET: return "VK_EVENT_SET";
    case VK_EVENT_RESET: return "VK_EVENT_RESET";
    case VK_INCOMPLETE: return "VK_INCOMPLETE";
    case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
    case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
    case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
    case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
    case VK_ERROR_UNKNOWN: return "VK_ERROR_UNKNOWN";
    case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
    case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
    case VK_ERROR_FRAGMENTATION: return "VK_ERROR_FRAGMENTATION";
    case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
    case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
    case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
    case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
    case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
    case VK_ERROR_INVALID_SHADER_NV: return "VK_ERROR_INVALID_SHADER_NV";
    case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR: return "VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR";
    default: return "UNKNOWN_VK_RESULT";
    }
}

static const char* vk_result_description(VkResult r) {
    switch (r) {
    case VK_ERROR_OUT_OF_HOST_MEMORY: return "Host system ran out of memory while fulfilling the request.";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "GPU memory was insufficient for the requested allocation or object.";
    case VK_ERROR_INITIALIZATION_FAILED: return "Driver rejected initialization, often due to invalid parameters or missing prerequisites.";
    case VK_ERROR_DEVICE_LOST: return "The GPU stopped responding or was reset; usually caused by device removal or timeout.";
    case VK_ERROR_MEMORY_MAP_FAILED: return "Mapping the requested memory range failed (invalid offset/size or unsupported).";
    case VK_ERROR_LAYER_NOT_PRESENT: return "Requested validation layer is not available on this system.";
    case VK_ERROR_EXTENSION_NOT_PRESENT: return "Requested Vulkan extension is not supported by the implementation.";
    case VK_ERROR_FEATURE_NOT_PRESENT: return "A required device feature is unavailable on the selected GPU.";
    case VK_ERROR_INCOMPATIBLE_DRIVER: return "The installed driver does not support the requested Vulkan version.";
    case VK_ERROR_TOO_MANY_OBJECTS: return "Implementation-specific object limit exceeded (try freeing unused resources).";
    case VK_ERROR_FORMAT_NOT_SUPPORTED: return "Chosen image/format combination is unsupported for the requested usage.";
    case VK_ERROR_FRAGMENTED_POOL: return "Pool allocation failed because the pool became internally fragmented.";
    case VK_ERROR_OUT_OF_POOL_MEMORY: return "Descriptor or command pool cannot satisfy the allocation request.";
    case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "External handle provided is not valid for this driver or platform.";
    case VK_ERROR_FRAGMENTATION: return "Allocation failed due to excessive fragmentation of the available memory.";
    case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: return "Opaque capture address is invalid or already in use.";
    case VK_ERROR_SURFACE_LOST_KHR: return "The presentation surface became invalid (resized, moved, or destroyed).";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "Surface creation failed because the window is already bound to another surface.";
    case VK_ERROR_OUT_OF_DATE_KHR: return "Swapchain no longer matches the surface; recreate swapchain to continue.";
    case VK_SUBOPTIMAL_KHR: return "Swapchain is still usable but no longer matches the surface optimally (consider recreating).";
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "Requested display configuration is incompatible with the selected display.";
    case VK_ERROR_VALIDATION_FAILED_EXT: return "Validation layers found an error; check validation output for details.";
    case VK_ERROR_INVALID_SHADER_NV: return "Shader failed to compile or link for the driver; inspect SPIR-V or compile options.";
    case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR: return "Requested image usage flags are unsupported for this surface format.";
    case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR: return "Video profile does not support the requested picture layout.";
    case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR: return "Video profile does not support the requested operation.";
    case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR: return "Video profile does not support the requested pixel format.";
    case VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR: return "Video profile does not support the requested codec.";
    case VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR: return "Requested video standard version is not supported.";
    default: return "Consult validation output or driver logs for more details.";
    }
}

void fatal_vk(const char* msg, VkResult r) {
    const char* name = vk_result_name(r);
    const char* desc = vk_result_description(r);
    fprintf(stderr, "Fatal: %s failed with %s. %s\n", msg, name, desc);
    exit(1);
}

double vk_now_ms(void) {
#ifdef CLOCK_MONOTONIC
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
#else
    return platform_get_time_ms();
#endif
}

void vk_log_command(VulkanRendererState* state, const char* command, const char* params, double start_ms) {
    if (!state || !state->logger) return;
    render_logger_log(state->logger, command, params, vk_now_ms() - start_ms);
}

uint32_t find_mem_type(VkPhysicalDevice physical, uint32_t typeFilter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(physical, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; i++) {
        if ((typeFilter & (1u << i)) && (mp.memoryTypes[i].propertyFlags & props) == props) return i;
    }
    fatal("no mem type");
    return 0;
}

uint32_t* read_file_bin_u32(const char* path, size_t * out_words) {
    FILE* f = platform_fopen(path, "rb"); if (!f) { fprintf(stderr, "Failed open %s\n", path); return NULL; }
    fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
    uint32_t* b = malloc(len); fread(b, 1, len, f); if (out_words) *out_words = (size_t)(len / 4); fclose(f); return b;
}
