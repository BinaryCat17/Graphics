#include "engine/render/backend/vulkan/vk_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "foundation/platform/platform.h" // For time

// --- Logging ---

void fatal_vk(const char* msg, VkResult res) {
    fprintf(stderr, "[vulkan] Fatal error in %s: %d\n", msg, res);
    exit(1);
}

void fatal(const char* msg) {
    fprintf(stderr, "[vulkan] Fatal error: %s\n", msg);
    exit(1);
}

double vk_now_ms(void) {
    return platform_get_time_ms();
}

void vk_log_command(VulkanRendererState* state, RenderLogLevel level, const char* cmd, const char* param, double start_time_ms) {
    if (!state || !state->logger) return;
    double duration = vk_now_ms() - start_time_ms;
    render_logger_log(state->logger, level, cmd, param, duration);
}

// --- Utils ---

uint32_t find_mem_type(VkPhysicalDevice physical_device, uint32_t type_filter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    fatal("failed to find suitable memory type!");
    return 0;
}

uint32_t* read_file_bin_u32(const char* filename, size_t* out_size) {
    FILE* file = fopen(filename, "rb");
    if (!file) return NULL;

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (size <= 0) {
        fclose(file);
        return NULL;
    }

    uint32_t* buffer = (uint32_t*)malloc(size);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    fread(buffer, 1, size, file);
    fclose(file);

    if (out_size) *out_size = size;
    return buffer;
}
