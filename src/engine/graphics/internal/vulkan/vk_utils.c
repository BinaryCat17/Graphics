#include "engine/graphics/internal/vulkan/vk_utils.h"
#include "foundation/logger/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <vulkan/vulkan.h>

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
    fseek(file, 0, SEEK_SET);

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

VkCommandBuffer vk_begin_single_time_commands(VulkanRendererState* state) {
    VkCommandBufferAllocateInfo ai = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = state->cmdpool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1 };
    VkCommandBuffer cb;
    vkAllocateCommandBuffers(state->device, &ai, &cb);
    VkCommandBufferBeginInfo bi = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
    vkBeginCommandBuffer(cb, &bi);
    return cb;
}

void vk_end_single_time_commands(VulkanRendererState* state, VkCommandBuffer cb) {
    vkEndCommandBuffer(cb);
    VkSubmitInfo si = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cb };
    vkQueueSubmit(state->queue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(state->queue);
    vkFreeCommandBuffers(state->device, state->cmdpool, 1, &cb);
}