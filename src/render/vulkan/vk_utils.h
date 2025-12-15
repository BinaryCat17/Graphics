#ifndef VK_UTILS_H
#define VK_UTILS_H

#include "vk_types.h"

void fatal(const char* msg);
void fatal_vk(const char* msg, VkResult r);
double vk_now_ms(void);
void vk_log_command(VulkanRendererState* state, const char* command, const char* params, double start_ms);
uint32_t find_mem_type(VkPhysicalDevice physical, uint32_t typeFilter, VkMemoryPropertyFlags props);
uint32_t* read_file_bin_u32(const char* path, size_t * out_words);

#endif // VK_UTILS_H
