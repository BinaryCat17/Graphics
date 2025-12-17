#ifndef VK_RESOURCES_H
#define VK_RESOURCES_H

#include "vk_types.h"

void vk_create_buffer(VulkanRendererState* state, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer* out_buf, VkDeviceMemory* out_mem);
bool vk_create_vertex_buffer(VulkanRendererState* state, FrameResources *frame, size_t bytes);
void vk_create_font_texture(VulkanRendererState* state);
void vk_create_descriptor_pool_and_set(VulkanRendererState* state);

void vk_destroy_device_resources(VulkanRendererState* state);

#endif // VK_RESOURCES_H
