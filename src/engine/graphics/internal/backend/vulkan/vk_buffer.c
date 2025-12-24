#include "vk_buffer.h"
#include "vk_utils.h"
#include "foundation/logger/logger.h"
#include <string.h>

bool vk_buffer_create(VulkanRendererState* state, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBufferWrapper* out_buffer) {
    out_buffer->size = size;
    out_buffer->usage = usage;
    out_buffer->memory_props = props;
    out_buffer->mapped_data = NULL;
    out_buffer->buffer = VK_NULL_HANDLE;
    out_buffer->memory = VK_NULL_HANDLE;

    VkBufferCreateInfo bci = { 
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, 
        .size = size, 
        .usage = usage, 
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE 
    };
    VkResult res = vkCreateBuffer(state->device, &bci, NULL, &out_buffer->buffer);
    if (res != VK_SUCCESS) {
        LOG_ERROR("vkCreateBuffer failed: %d", res);
        return false;
    }

    VkMemoryRequirements mr; 
    vkGetBufferMemoryRequirements(state->device, out_buffer->buffer, &mr);
    
    VkMemoryAllocateInfo mai = { 
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, 
        .allocationSize = mr.size, 
        .memoryTypeIndex = find_mem_type(state->physical_device, mr.memoryTypeBits, props) 
    };
    
    res = vkAllocateMemory(state->device, &mai, NULL, &out_buffer->memory);
    if (res != VK_SUCCESS) {
        LOG_ERROR("vkAllocateMemory failed: %d", res);
        vkDestroyBuffer(state->device, out_buffer->buffer, NULL);
        out_buffer->buffer = VK_NULL_HANDLE;
        return false;
    }

    vkBindBufferMemory(state->device, out_buffer->buffer, out_buffer->memory, 0);
    return true;
}

void vk_buffer_destroy(VulkanRendererState* state, VkBufferWrapper* buffer) {
    if (buffer->buffer) {
        vkDestroyBuffer(state->device, buffer->buffer, NULL);
        buffer->buffer = VK_NULL_HANDLE;
    }
    if (buffer->memory) {
        vkFreeMemory(state->device, buffer->memory, NULL);
        buffer->memory = VK_NULL_HANDLE;
    }
    buffer->mapped_data = NULL;
    buffer->size = 0;
}

void* vk_buffer_map(VulkanRendererState* state, VkBufferWrapper* buffer) {
    if (buffer->mapped_data) return buffer->mapped_data;
    
    // We can only map HOST_VISIBLE memory
    if (!(buffer->memory_props & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))) {
        LOG_ERROR("Attempting to map non-host-visible buffer");
        return NULL;
    }

    VkResult res = vkMapMemory(state->device, buffer->memory, 0, VK_WHOLE_SIZE, 0, &buffer->mapped_data);
    if (res != VK_SUCCESS) {
        LOG_ERROR("vkMapMemory failed: %d", res);
        return NULL;
    }
    return buffer->mapped_data;
}

void vk_buffer_unmap(VulkanRendererState* state, VkBufferWrapper* buffer) {
    if (buffer->mapped_data) {
        vkUnmapMemory(state->device, buffer->memory);
        buffer->mapped_data = NULL;
    }
}

bool vk_buffer_upload(VulkanRendererState* state, VkBufferWrapper* buffer, const void* data, VkDeviceSize size, VkDeviceSize offset) {
    if (size == 0) return true;
    if (size + offset > buffer->size) {
        LOG_ERROR("Upload size %zu + offset %zu exceeds buffer size %zu", size, offset, buffer->size);
        return false;
    }

    // Direct copy if Host Visible
    if (buffer->memory_props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        void* ptr = vk_buffer_map(state, buffer);
        if (!ptr) return false;
        
        memcpy((uint8_t*)ptr + offset, data, size);
        
        if (!(buffer->memory_props & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
             VkMappedMemoryRange range = { 
                 .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, 
                 .memory = buffer->memory, 
                 .offset = offset, 
                 .size = size 
             };
             vkFlushMappedMemoryRanges(state->device, 1, &range);
        }
        vk_buffer_unmap(state, buffer);
        return true;
    } else {
        // Staging Buffer for Device Local memory
        VkBufferWrapper staging;
        if (!vk_buffer_create(state, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &staging)) {
            return false;
        }
        
        void* ptr = vk_buffer_map(state, &staging);
        if (ptr) {
            memcpy(ptr, data, size);
            vk_buffer_unmap(state, &staging);

            VkCommandBuffer cb = vk_begin_single_time_commands(state);
            VkBufferCopy copy = { .srcOffset = 0, .dstOffset = offset, .size = size };
            vkCmdCopyBuffer(cb, staging.buffer, buffer->buffer, 1, &copy);
            vk_end_single_time_commands(state, cb);
        }

        vk_buffer_destroy(state, &staging);
        return ptr != NULL;
    }
}

bool vk_buffer_read(VulkanRendererState* state, VkBufferWrapper* buffer, void* dst, VkDeviceSize size, VkDeviceSize offset) {
    if (size == 0) return true;
    if (size + offset > buffer->size) return false;

    // Direct read if Host Visible
    if (buffer->memory_props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        void* ptr = vk_buffer_map(state, buffer);
        if (!ptr) return false;

        if (!(buffer->memory_props & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
             VkMappedMemoryRange range = { 
                 .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, 
                 .memory = buffer->memory, 
                 .offset = offset, 
                 .size = size 
             };
             vkInvalidateMappedMemoryRanges(state->device, 1, &range);
        }
        
        memcpy(dst, (uint8_t*)ptr + offset, size);
        vk_buffer_unmap(state, buffer);
        return true;
    } else {
        // Staging Buffer
        VkBufferWrapper staging;
        if (!vk_buffer_create(state, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &staging)) {
            return false;
        }

        VkCommandBuffer cb = vk_begin_single_time_commands(state);
        VkBufferCopy copy = { .srcOffset = offset, .dstOffset = 0, .size = size };
        vkCmdCopyBuffer(cb, buffer->buffer, staging.buffer, 1, &copy);
        
        // Memory barrier to ensure transfer is visible to host
        VkBufferMemoryBarrier barrier = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = staging.buffer,
            .offset = 0,
            .size = size
        };
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, NULL, 1, &barrier, 0, NULL);
        
        vk_end_single_time_commands(state, cb);

        void* ptr = vk_buffer_map(state, &staging);
        if (ptr) {
            memcpy(dst, ptr, size);
            vk_buffer_unmap(state, &staging);
        }
        
        vk_buffer_destroy(state, &staging);
        return ptr != NULL;
    }
}