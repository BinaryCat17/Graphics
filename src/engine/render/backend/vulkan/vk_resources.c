#include "vk_resources.h"
#include "vk_swapchain.h"
#include "vk_utils.h"
#include "foundation/logger/logger.h"
#include "engine/text/font.h" // Include Font Module
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

// ... (previous functions unchanged)

void vk_create_buffer(VulkanRendererState* state, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer* out_buf, VkDeviceMemory* out_mem) {
    VkBufferCreateInfo bci = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = size, .usage = usage, .sharingMode = VK_SHARING_MODE_EXCLUSIVE };
    state->res = vkCreateBuffer(state->device, &bci, NULL, out_buf);
    if (state->res != VK_SUCCESS) fatal_vk("vkCreateBuffer", state->res);
    VkMemoryRequirements mr; vkGetBufferMemoryRequirements(state->device, *out_buf, &mr);
    VkMemoryAllocateInfo mai = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = mr.size, .memoryTypeIndex = find_mem_type(state->physical_device, mr.memoryTypeBits, props) };
    state->res = vkAllocateMemory(state->device, &mai, NULL, out_mem);
    if (state->res != VK_SUCCESS) fatal_vk("vkAllocateMemory", state->res);
    vkBindBufferMemory(state->device, *out_buf, *out_mem, 0);
}

static VkCommandBuffer begin_single_time_commands(VulkanRendererState* state) {
    VkCommandBufferAllocateInfo ai = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = state->cmdpool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1 };
    VkCommandBuffer cb;
    vkAllocateCommandBuffers(state->device, &ai, &cb);
    VkCommandBufferBeginInfo bi = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
    vkBeginCommandBuffer(cb, &bi);
    return cb;
}

static void end_single_time_commands(VulkanRendererState* state, VkCommandBuffer cb) {
    vkEndCommandBuffer(cb);
    VkSubmitInfo si = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cb };
    vkQueueSubmit(state->queue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(state->queue);
    vkFreeCommandBuffers(state->device, state->cmdpool, 1, &cb);
}

static void transition_image_layout(VulkanRendererState* state, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkCommandBuffer cb = begin_single_time_commands(state);
    VkImageMemoryBarrier barrier = { .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, .oldLayout = oldLayout, .newLayout = newLayout, .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .image = image, .subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 } };
    VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    vkCmdPipelineBarrier(cb, src_stage, dst_stage, 0, 0, NULL, 0, NULL, 1, &barrier);
    end_single_time_commands(state, cb);
}

static void copy_buffer_to_image(VulkanRendererState* state, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkCommandBuffer cb = begin_single_time_commands(state);
    VkBufferImageCopy copy = { .bufferOffset = 0, .bufferRowLength = 0, .bufferImageHeight = 0, .imageSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1 }, .imageOffset = {0,0,0}, .imageExtent = { width, height, 1 } };
    vkCmdCopyBufferToImage(cb, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    end_single_time_commands(state, cb);
}

bool vk_create_vertex_buffer(VulkanRendererState* state, FrameResources *frame, size_t bytes) {
    if (frame->vertex_buffer != VK_NULL_HANDLE && frame->vertex_capacity >= bytes) {
        return true;
    }

    if (frame->vertex_buffer) {
        vkDestroyBuffer(state->device, frame->vertex_buffer, NULL);
        frame->vertex_buffer = VK_NULL_HANDLE;
    }
    if (frame->vertex_memory) {
        vkFreeMemory(state->device, frame->vertex_memory, NULL);
        frame->vertex_memory = VK_NULL_HANDLE;
        frame->vertex_capacity = 0;
    }

    VkBuffer new_buffer = VK_NULL_HANDLE;
    VkDeviceMemory new_memory = VK_NULL_HANDLE;
    VkBufferCreateInfo bci = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = bytes, .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, .sharingMode = VK_SHARING_MODE_EXCLUSIVE };
    
    VkResult create = vkCreateBuffer(state->device, &bci, NULL, &new_buffer);
    if (create != VK_SUCCESS) {
        fprintf(stderr, "vkCreateBuffer failed for vertex buffer\n");
        return false;
    }

    VkMemoryRequirements mr; vkGetBufferMemoryRequirements(state->device, new_buffer, &mr);
    VkMemoryAllocateInfo mai = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = mr.size, .memoryTypeIndex = find_mem_type(state->physical_device, mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) };
    
    VkResult alloc = vkAllocateMemory(state->device, &mai, NULL, &new_memory);
    if (alloc != VK_SUCCESS) {
        fprintf(stderr, "vkAllocateMemory failed for vertex buffer\n");
        vkDestroyBuffer(state->device, new_buffer, NULL);
        return false;
    }

    vkBindBufferMemory(state->device, new_buffer, new_memory, 0);
    frame->vertex_buffer = new_buffer;
    frame->vertex_memory = new_memory;
    frame->vertex_capacity = bytes;
    return true;
}

void vk_create_font_texture(VulkanRendererState* state) {
    const FontAtlas* atlas = font_get_atlas();
    if (!atlas || !atlas->pixels) {
        LOG_FATAL("Font atlas not available from Font Module");
    }

    VkImageCreateInfo ici = { .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .imageType = VK_IMAGE_TYPE_2D, .format = VK_FORMAT_R8_UNORM, .extent = { (uint32_t)atlas->width, (uint32_t)atlas->height, 1 }, .mipLevels = 1, .arrayLayers = 1, .samples = VK_SAMPLE_COUNT_1_BIT, .tiling = VK_IMAGE_TILING_OPTIMAL, .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, .sharingMode = VK_SHARING_MODE_EXCLUSIVE, .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED }; 
    state->res = vkCreateImage(state->device, &ici, NULL, &state->font_image);
    if (state->res != VK_SUCCESS) fatal_vk("vkCreateImage", state->res);
    VkMemoryRequirements mr; vkGetImageMemoryRequirements(state->device, state->font_image, &mr);
    VkMemoryAllocateInfo mai = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = mr.size, .memoryTypeIndex = find_mem_type(state->physical_device, mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) };
    state->res = vkAllocateMemory(state->device, &mai, NULL, &state->font_image_mem);
    if (state->res != VK_SUCCESS) fatal_vk("vkAllocateMemory", state->res);
    vkBindImageMemory(state->device, state->font_image, state->font_image_mem, 0);

    VkBuffer staging = VK_NULL_HANDLE; VkDeviceMemory staging_mem = VK_NULL_HANDLE;
    vk_create_buffer(state, atlas->width * atlas->height, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &staging, &staging_mem);
    void* mapped = NULL; vkMapMemory(state->device, staging_mem, 0, VK_WHOLE_SIZE, 0, &mapped); memcpy(mapped, atlas->pixels, atlas->width * atlas->height); vkUnmapMemory(state->device, staging_mem);

    transition_image_layout(state, state->font_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copy_buffer_to_image(state, staging, state->font_image, (uint32_t)atlas->width, (uint32_t)atlas->height);
    transition_image_layout(state, state->font_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(state->device, staging, NULL);
    vkFreeMemory(state->device, staging_mem, NULL);

    VkImageViewCreateInfo ivci = { .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = state->font_image, .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = VK_FORMAT_R8_UNORM, .subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 } };
    state->res = vkCreateImageView(state->device, &ivci, NULL, &state->font_image_view);
    if (state->res != VK_SUCCESS) fatal_vk("vkCreateImageView", state->res);

    VkSamplerCreateInfo sci = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .magFilter = VK_FILTER_LINEAR, .minFilter = VK_FILTER_LINEAR, .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK, .unnormalizedCoordinates = VK_FALSE, .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST };
    state->res = vkCreateSampler(state->device, &sci, NULL, &state->font_sampler);
    if (state->res != VK_SUCCESS) fatal_vk("vkCreateSampler", state->res);
}

void vk_create_descriptor_pool_and_set(VulkanRendererState* state) {
    VkDescriptorPoolSize pools[2] = {
        { .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 2 }, // Reserve a bit more
        { .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 2 }
    };
    VkDescriptorPoolCreateInfo dpci = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .maxSets = 4, .poolSizeCount = 2, .pPoolSizes = pools };
    state->res = vkCreateDescriptorPool(state->device, &dpci, NULL, &state->descriptor_pool);
    if (state->res != VK_SUCCESS) fatal_vk("vkCreateDescriptorPool", state->res);

    // Set 0: Texture
    VkDescriptorSetAllocateInfo dsai0 = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, .descriptorPool = state->descriptor_pool, .descriptorSetCount = 1, .pSetLayouts = &state->descriptor_layout };
    state->res = vkAllocateDescriptorSets(state->device, &dsai0, &state->descriptor_set);
    if (state->res != VK_SUCCESS) fatal_vk("vkAllocateDescriptorSets (Set 0)", state->res);

    VkDescriptorImageInfo dii = { .sampler = state->font_sampler, .imageView = state->font_image_view, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    VkWriteDescriptorSet w0 = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = state->descriptor_set, .dstBinding = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &dii };
    vkUpdateDescriptorSets(state->device, 1, &w0, 0, NULL);
    
    // Set 1: Instance Buffer
    VkDescriptorSetAllocateInfo dsai1 = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, .descriptorPool = state->descriptor_pool, .descriptorSetCount = 1, .pSetLayouts = &state->instance_layout };
    state->res = vkAllocateDescriptorSets(state->device, &dsai1, &state->instance_set);
    if (state->res != VK_SUCCESS) fatal_vk("vkAllocateDescriptorSets (Set 1)", state->res);
    
    // Note: Instance Buffer is not bound here because it's dynamic (resizable).
    // It is bound in ensure_instance_buffer().

    // Set 2: User Texture (Compute Target)
    VkDescriptorSetAllocateInfo dsai2 = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, .descriptorPool = state->descriptor_pool, .descriptorSetCount = 1, .pSetLayouts = &state->descriptor_layout }; // Same layout as Set 0
    state->res = vkAllocateDescriptorSets(state->device, &dsai2, &state->compute_target_descriptor);
    if (state->res != VK_SUCCESS) fatal_vk("vkAllocateDescriptorSets (Set 2)", state->res);
    
    // Bind placeholder (font) initially so it's valid
    VkDescriptorImageInfo dii2 = { .sampler = state->font_sampler, .imageView = state->font_image_view, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    VkWriteDescriptorSet w2 = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = state->compute_target_descriptor, .dstBinding = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &dii2 };
    vkUpdateDescriptorSets(state->device, 1, &w2, 0, NULL);
}

void vk_ensure_compute_target(VulkanRendererState* state, int width, int height) {
    if (state->compute_width == width && state->compute_height == height && state->compute_target_image) return;

    // Cleanup old
    if (state->compute_target_view) { vkDestroyImageView(state->device, state->compute_target_view, NULL); state->compute_target_view = VK_NULL_HANDLE; }
    if (state->compute_target_image) { vkDestroyImage(state->device, state->compute_target_image, NULL); state->compute_target_image = VK_NULL_HANDLE; }
    if (state->compute_target_memory) { vkFreeMemory(state->device, state->compute_target_memory, NULL); state->compute_target_memory = VK_NULL_HANDLE; }

    if (width <= 0 || height <= 0) return;

    state->compute_width = width;
    state->compute_height = height;

    VkImageCreateInfo ici = { 
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, 
        .imageType = VK_IMAGE_TYPE_2D, 
        .format = VK_FORMAT_R8G8B8A8_UNORM, // Standard RGBA
        .extent = { (uint32_t)width, (uint32_t)height, 1 }, 
        .mipLevels = 1, 
        .arrayLayers = 1, 
        .samples = VK_SAMPLE_COUNT_1_BIT, 
        .tiling = VK_IMAGE_TILING_OPTIMAL, 
        .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // Storage + Sampled
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE, 
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED 
    }; 
    
    state->res = vkCreateImage(state->device, &ici, NULL, &state->compute_target_image);
    if (state->res != VK_SUCCESS) fatal_vk("vkCreateImage (compute)", state->res);

    VkMemoryRequirements mr; vkGetImageMemoryRequirements(state->device, state->compute_target_image, &mr);
    VkMemoryAllocateInfo mai = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = mr.size, .memoryTypeIndex = find_mem_type(state->physical_device, mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) };
    state->res = vkAllocateMemory(state->device, &mai, NULL, &state->compute_target_memory);
    if (state->res != VK_SUCCESS) fatal_vk("vkAllocateMemory (compute)", state->res);
    vkBindImageMemory(state->device, state->compute_target_image, state->compute_target_memory, 0);

    // Create View
    VkImageViewCreateInfo ivci = { 
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, 
        .image = state->compute_target_image, 
        .viewType = VK_IMAGE_VIEW_TYPE_2D, 
        .format = VK_FORMAT_R8G8B8A8_UNORM, 
        .subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 } 
    };
    state->res = vkCreateImageView(state->device, &ivci, NULL, &state->compute_target_view);
    if (state->res != VK_SUCCESS) fatal_vk("vkCreateImageView (compute)", state->res);

    // Transition to General (ready for compute write)
    transition_image_layout(state, state->compute_target_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    
    // Update Descriptor Set 2 to point to this new view
    if (state->compute_target_descriptor) {
        VkDescriptorImageInfo dii = { 
            .sampler = state->font_sampler, // Reuse sampler
            .imageView = state->compute_target_view, 
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL // We will read from General layout for simplicity in MVP
        };
        VkWriteDescriptorSet w = { 
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 
            .dstSet = state->compute_target_descriptor, 
            .dstBinding = 0, 
            .descriptorCount = 1, 
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 
            .pImageInfo = &dii 
        };
        vkUpdateDescriptorSets(state->device, 1, &w, 0, NULL);
    }
}

void vk_destroy_device_resources(VulkanRendererState* state) {
    vk_cleanup_swapchain(state, false);

    if (state->descriptor_pool) { vkDestroyDescriptorPool(state->device, state->descriptor_pool, NULL); state->descriptor_pool = VK_NULL_HANDLE; }
    if (state->descriptor_layout) { vkDestroyDescriptorSetLayout(state->device, state->descriptor_layout, NULL); state->descriptor_layout = VK_NULL_HANDLE; }
    if (state->instance_layout) { vkDestroyDescriptorSetLayout(state->device, state->instance_layout, NULL); state->instance_layout = VK_NULL_HANDLE; }
    if (state->font_sampler) { vkDestroySampler(state->device, state->font_sampler, NULL); state->font_sampler = VK_NULL_HANDLE; }
    if (state->font_image_view) { vkDestroyImageView(state->device, state->font_image_view, NULL); state->font_image_view = VK_NULL_HANDLE; }
    if (state->font_image) { vkDestroyImage(state->device, state->font_image, NULL); state->font_image = VK_NULL_HANDLE; }
    if (state->font_image_mem) { vkFreeMemory(state->device, state->font_image_mem, NULL); state->font_image_mem = VK_NULL_HANDLE; }
    
    // Cleanup Compute
    if (state->compute_target_view) { vkDestroyImageView(state->device, state->compute_target_view, NULL); state->compute_target_view = VK_NULL_HANDLE; }
    if (state->compute_target_image) { vkDestroyImage(state->device, state->compute_target_image, NULL); state->compute_target_image = VK_NULL_HANDLE; }
    if (state->compute_target_memory) { vkFreeMemory(state->device, state->compute_target_memory, NULL); state->compute_target_memory = VK_NULL_HANDLE; }
    
    for (size_t i = 0; i < 2; ++i) {
        if (state->frame_resources[i].vertex_buffer) { vkDestroyBuffer(state->device, state->frame_resources[i].vertex_buffer, NULL); state->frame_resources[i].vertex_buffer = VK_NULL_HANDLE; }
        if (state->frame_resources[i].vertex_memory) { vkFreeMemory(state->device, state->frame_resources[i].vertex_memory, NULL); state->frame_resources[i].vertex_memory = VK_NULL_HANDLE; }
        state->frame_resources[i].vertex_capacity = 0;
        state->frame_resources[i].vertex_count = 0;
        state->frame_resources[i].stage = FRAME_AVAILABLE;
        state->frame_resources[i].inflight_fence = VK_NULL_HANDLE;
    }
    if (state->sem_img_avail) { vkDestroySemaphore(state->device, state->sem_img_avail, NULL); state->sem_img_avail = VK_NULL_HANDLE; }
    if (state->sem_render_done) { vkDestroySemaphore(state->device, state->sem_render_done, NULL); state->sem_render_done = VK_NULL_HANDLE; }
}
