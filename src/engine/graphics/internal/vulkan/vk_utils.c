#include "engine/graphics/internal/vulkan/vk_utils.h"
#include "foundation/logger/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void fatal_vk(const char* msg, VkResult res) {
    LOG_FATAL("%s: VkResult %d", msg, res);
}

double vk_now_ms(void) {
    // Implement or wrap platform time
    // For now, simple placeholder or use platform if available
    // Better to move this to platform layer or use platform_get_time_ms
    return 0.0; // Placeholder
}

uint32_t find_mem_type(VkPhysicalDevice physical_device, uint32_t type_filter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);

    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) && 
            (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    LOG_FATAL("Failed to find suitable memory type!");
    return 0;
}

uint32_t* read_file_bin_u32(const char* filename, size_t* out_size) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        LOG_ERROR("Failed to open file: %s", filename);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    if (file_size % 4 != 0) {
        LOG_WARN("File size is not a multiple of 4 (SPIR-V requirement?): %s", filename);
    }

    uint32_t* buffer = (uint32_t*)malloc(file_size);
    if (!buffer) {
        LOG_FATAL("Failed to allocate memory for file: %s", filename);
        fclose(file);
        return NULL;
    }

    size_t read_bytes = fread(buffer, 1, file_size, file);
    if (read_bytes != (size_t)file_size) {
        LOG_ERROR("Failed to read file: %s", filename);
        free(buffer);
        fclose(file);
        return NULL;
    }

    fclose(file);
    if (out_size) *out_size = file_size;
    return buffer;
}