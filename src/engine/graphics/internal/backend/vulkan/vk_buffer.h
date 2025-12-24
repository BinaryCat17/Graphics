#ifndef VK_BUFFER_H
#define VK_BUFFER_H

#include "vk_types.h"

// Unified Buffer wrapper
typedef struct VkBufferWrapper {
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkDeviceSize size;
    VkBufferUsageFlags usage;
    VkMemoryPropertyFlags memory_props;
    void* mapped_data; // NULL if not mapped
} VkBufferWrapper;

// Creates a buffer on the GPU.
// usage: VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | ...
// props: VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT or HOST_VISIBLE...
bool vk_buffer_create(VulkanRendererState* state, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBufferWrapper* out_buffer);

// Destroys the buffer.
void vk_buffer_destroy(VulkanRendererState* state, VkBufferWrapper* buffer);

// Maps memory (if host visible). Returns pointer.
void* vk_buffer_map(VulkanRendererState* state, VkBufferWrapper* buffer);

// Unmaps memory.
void vk_buffer_unmap(VulkanRendererState* state, VkBufferWrapper* buffer);

// Uploads data to the buffer.
// If buffer is HOST_VISIBLE, maps and copies.
// If DEVICE_LOCAL, uses a staging buffer.
bool vk_buffer_upload(VulkanRendererState* state, VkBufferWrapper* buffer, const void* data, VkDeviceSize size, VkDeviceSize offset);

// Downloads data from the buffer.
// Handles staging buffer automatically if needed.
bool vk_buffer_read(VulkanRendererState* state, VkBufferWrapper* buffer, void* dst, VkDeviceSize size, VkDeviceSize offset);

#endif // VK_BUFFER_H
