#include "vk_resources.h"
#include "vk_swapchain.h"
#include "vk_utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

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

void vk_build_font_atlas(VulkanRendererState* state) {
    if (!state->font_path) fatal("Font path is null");
    FILE* f = platform_fopen(state->font_path, "rb");
    if (!f) { fprintf(stderr, "Fatal: font not found at %s\n", state->font_path); fatal("font load"); }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    state->ttf_buffer = malloc(sz); fread(state->ttf_buffer, 1, sz, f); fclose(f);
    stbtt_InitFont(&state->fontinfo, state->ttf_buffer, 0);

    state->atlas_w = 1024; state->atlas_h = 1024;
    state->atlas = malloc(state->atlas_w * state->atlas_h);
    memset(state->atlas, 0, state->atlas_w * state->atlas_h);
    memset(state->glyph_valid, 0, sizeof(state->glyph_valid));
    state->font_scale = stbtt_ScaleForPixelHeight(&state->fontinfo, 32.0f);
    int raw_ascent = 0, raw_descent = 0;
    stbtt_GetFontVMetrics(&state->fontinfo, &raw_ascent, &raw_descent, NULL);
    state->ascent = (int)roundf(raw_ascent * state->font_scale);
    state->descent = (int)roundf(raw_descent * state->font_scale);

    int ranges[][2] = { {32, 126}, {0x0400, 0x04FF} };
    int range_count = (int)(sizeof(ranges) / sizeof(ranges[0]));

    int x = 0, y = 0, rowh = 0;
    for (int r = 0; r < range_count; r++) {
        for (int c = ranges[r][0]; c <= ranges[r][1] && c < GLYPH_CAPACITY; c++) {
            int aw, ah, bx, by;
            unsigned char* bitmap = stbtt_GetCodepointBitmap(&state->fontinfo, 0, state->font_scale, c, &aw, &ah, &bx, &by);
            if (x + aw >= state->atlas_w) { x = 0; y += rowh; rowh = 0; }
            if (y + ah >= state->atlas_h) { fprintf(stderr, "atlas too small\n"); stbtt_FreeBitmap(bitmap, NULL); break; }
            for (int yy = 0; yy < ah; yy++) {
                for (int xx = 0; xx < aw; xx++) {
                    state->atlas[(y + yy) * state->atlas_w + (x + xx)] = bitmap[yy * aw + xx];
                }
            }
            stbtt_FreeBitmap(bitmap, NULL);
            int advance, lsb;
            stbtt_GetCodepointHMetrics(&state->fontinfo, c, &advance, &lsb);
            int box_x0, box_y0, box_x1, box_y1;
            stbtt_GetCodepointBitmapBox(&state->fontinfo, c, state->font_scale, state->font_scale, &box_x0, &box_y0, &box_x1, &box_y1);
            state->glyphs[c].advance = advance * state->font_scale;
            state->glyphs[c].xoff = (float)box_x0;
            state->glyphs[c].yoff = (float)box_y0;
            state->glyphs[c].w = (float)(box_x1 - box_x0);
            state->glyphs[c].h = (float)(box_y1 - box_y0);
            state->glyphs[c].u0 = (float)x / (float)state->atlas_w;
            state->glyphs[c].v0 = (float)y / (float)state->atlas_h;
            state->glyphs[c].u1 = (float)(x + aw) / (float)state->atlas_w;
            state->glyphs[c].v1 = (float)(y + ah) / (float)state->atlas_h;
            state->glyph_valid[c] = 1;
            x += aw + 1;
            if (ah > rowh) rowh = ah;
        }
    }
}

void vk_create_font_texture(VulkanRendererState* state) {
    if (!state->atlas) fatal("font atlas not built");
    VkImageCreateInfo ici = { .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .imageType = VK_IMAGE_TYPE_2D, .format = VK_FORMAT_R8_UNORM, .extent = { (uint32_t)state->atlas_w, (uint32_t)state->atlas_h, 1 }, .mipLevels = 1, .arrayLayers = 1, .samples = VK_SAMPLE_COUNT_1_BIT, .tiling = VK_IMAGE_TILING_OPTIMAL, .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, .sharingMode = VK_SHARING_MODE_EXCLUSIVE, .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED }; 
    state->res = vkCreateImage(state->device, &ici, NULL, &state->font_image);
    if (state->res != VK_SUCCESS) fatal_vk("vkCreateImage", state->res);
    VkMemoryRequirements mr; vkGetImageMemoryRequirements(state->device, state->font_image, &mr);
    VkMemoryAllocateInfo mai = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = mr.size, .memoryTypeIndex = find_mem_type(state->physical_device, mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) };
    state->res = vkAllocateMemory(state->device, &mai, NULL, &state->font_image_mem);
    if (state->res != VK_SUCCESS) fatal_vk("vkAllocateMemory", state->res);
    vkBindImageMemory(state->device, state->font_image, state->font_image_mem, 0);

    VkBuffer staging = VK_NULL_HANDLE; VkDeviceMemory staging_mem = VK_NULL_HANDLE;
    vk_create_buffer(state, state->atlas_w * state->atlas_h, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &staging, &staging_mem);
    void* mapped = NULL; vkMapMemory(state->device, staging_mem, 0, VK_WHOLE_SIZE, 0, &mapped); memcpy(mapped, state->atlas, state->atlas_w * state->atlas_h); vkUnmapMemory(state->device, staging_mem);

    transition_image_layout(state, state->font_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copy_buffer_to_image(state, staging, state->font_image, (uint32_t)state->atlas_w, (uint32_t)state->atlas_h);
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
    VkDescriptorPoolSize pool = { .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1 };
    VkDescriptorPoolCreateInfo dpci = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .maxSets = 1, .poolSizeCount = 1, .pPoolSizes = &pool };
    state->res = vkCreateDescriptorPool(state->device, &dpci, NULL, &state->descriptor_pool);
    if (state->res != VK_SUCCESS) fatal_vk("vkCreateDescriptorPool", state->res);

    VkDescriptorSetAllocateInfo dsai = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, .descriptorPool = state->descriptor_pool, .descriptorSetCount = 1, .pSetLayouts = &state->descriptor_layout };
    state->res = vkAllocateDescriptorSets(state->device, &dsai, &state->descriptor_set);
    if (state->res != VK_SUCCESS) fatal_vk("vkAllocateDescriptorSets", state->res);

    VkDescriptorImageInfo dii = { .sampler = state->font_sampler, .imageView = state->font_image_view, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    VkWriteDescriptorSet w = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = state->descriptor_set, .dstBinding = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &dii };
    vkUpdateDescriptorSets(state->device, 1, &w, 0, NULL);
}

bool vk_upload_vertices(VulkanRendererState* state, FrameResources *frame) {
    if (frame->vertex_count == 0) {
        if (frame->vertex_buffer) {
            vkDestroyBuffer(state->device, frame->vertex_buffer, NULL);
            frame->vertex_buffer = VK_NULL_HANDLE;
        }
        if (frame->vertex_memory) {
            vkFreeMemory(state->device, frame->vertex_memory, NULL);
            frame->vertex_memory = VK_NULL_HANDLE;
        }
        frame->vertex_capacity = 0;
        return true;
    }

    size_t bytes = frame->vertex_count * sizeof(Vtx);
    if (!vk_create_vertex_buffer(state, frame, bytes)) {
        return false;
    }

    // Staging Buffer
    VkBuffer staging_buffer;
    VkDeviceMemory staging_memory;
    VkBufferCreateInfo bci = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = bytes, .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT, .sharingMode = VK_SHARING_MODE_EXCLUSIVE };
    if (vkCreateBuffer(state->device, &bci, NULL, &staging_buffer) != VK_SUCCESS) return false;

    VkMemoryRequirements mr; vkGetBufferMemoryRequirements(state->device, staging_buffer, &mr);
    VkMemoryAllocateInfo mai = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = mr.size, .memoryTypeIndex = find_mem_type(state->physical_device, mr.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) };
    if (vkAllocateMemory(state->device, &mai, NULL, &staging_memory) != VK_SUCCESS) {
        vkDestroyBuffer(state->device, staging_buffer, NULL);
        return false;
    }
    vkBindBufferMemory(state->device, staging_buffer, staging_memory, 0);

    void* dst = NULL;
    vkMapMemory(state->device, staging_memory, 0, bytes, 0, &dst);
    memcpy(dst, frame->cpu.vertices, bytes);
    vkUnmapMemory(state->device, staging_memory);

    // Copy from Staging to Device Local
    VkCommandBuffer cb = begin_single_time_commands(state);
    VkBufferCopy copyRegion = { .srcOffset = 0, .dstOffset = 0, .size = bytes };
    vkCmdCopyBuffer(cb, staging_buffer, frame->vertex_buffer, 1, &copyRegion);
    end_single_time_commands(state, cb);

    vkDestroyBuffer(state->device, staging_buffer, NULL);
    vkFreeMemory(state->device, staging_memory, NULL);

    return true;
}

void vk_destroy_device_resources(VulkanRendererState* state) {
    vk_cleanup_swapchain(state, false);

    if (state->descriptor_pool) { vkDestroyDescriptorPool(state->device, state->descriptor_pool, NULL); state->descriptor_pool = VK_NULL_HANDLE; }
    if (state->descriptor_layout) { vkDestroyDescriptorSetLayout(state->device, state->descriptor_layout, NULL); state->descriptor_layout = VK_NULL_HANDLE; }
    if (state->font_sampler) { vkDestroySampler(state->device, state->font_sampler, NULL); state->font_sampler = VK_NULL_HANDLE; }
    if (state->font_image_view) { vkDestroyImageView(state->device, state->font_image_view, NULL); state->font_image_view = VK_NULL_HANDLE; }
    if (state->font_image) { vkDestroyImage(state->device, state->font_image, NULL); state->font_image = VK_NULL_HANDLE; }
    if (state->font_image_mem) { vkFreeMemory(state->device, state->font_image_mem, NULL); state->font_image_mem = VK_NULL_HANDLE; }
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
