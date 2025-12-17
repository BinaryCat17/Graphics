#ifndef VK_UTILS_H
#define VK_UTILS_H

#include "engine/graphics/vulkan/vk_types.h"

// Helper to log fatal error and exit (or abort)
void fatal_vk(const char* msg, VkResult res);

double vk_now_ms(void);

// Logging
void vk_log_command(VulkanRendererState* state, RenderLogLevel level, const char* cmd, const char* param, double start_time_ms);

// Memory
uint32_t find_mem_type(VkPhysicalDevice physical_device, uint32_t type_filter, VkMemoryPropertyFlags properties);

// File I/O
uint32_t* read_file_bin_u32(const char* filename, size_t* out_size);

#endif // VK_UTILS_H
