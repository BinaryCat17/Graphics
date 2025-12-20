#include "engine/graphics/internal/vulkan/vulkan_renderer.h"

#include "engine/graphics/internal/vulkan/vk_types.h"
#include "engine/graphics/internal/vulkan/vk_context.h"
#include "engine/graphics/internal/vulkan/vk_swapchain.h"
#include "engine/graphics/internal/vulkan/vk_pipeline.h"
#include "engine/graphics/internal/vulkan/vk_resources.h"
#include "engine/graphics/internal/vulkan/vk_utils.h"
#include "engine/text/font.h"

#include "foundation/logger/logger.h"
#include "foundation/platform/platform.h"
#include "foundation/platform/fs.h"
#include "foundation/math/coordinate_systems.h"
#include "foundation/image/image.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <threads.h>

typedef struct ScreenshotContext {
    char path[256];
    int width;
    int height;
    bool needs_swizzle;
    uint8_t* data;
} ScreenshotContext;

static int save_screenshot_task(void* arg) {
    ScreenshotContext* ctx = (ScreenshotContext*)arg;
    if (ctx) {
        if (ctx->needs_swizzle) {
            image_swizzle_bgra_to_rgba(ctx->data, ctx->width * ctx->height);
        }
        
        LOG_TRACE("Screenshot Thread: Writing to disk (%s)...", ctx->path);
        // Using default compression (which is slow but standard). Threading hides the latency.
        if (image_write_png(ctx->path, ctx->width, ctx->height, 4, ctx->data, ctx->width * 4)) {
            LOG_TRACE("Screenshot saved to %s", ctx->path);
        }
        free(ctx->data);
        free(ctx);
    }
    return 0;
}

// Must match shader struct layout (std140)
typedef struct GpuInstanceData {
    Mat4 model;
    Vec4 color;
    Vec4 uv_rect;
    Vec4 params_1;
    Vec4 params_2;
    Vec4 clip_rect; // Added
} GpuInstanceData;

static void vulkan_renderer_request_screenshot(RendererBackend* backend, const char* filepath) {
    VulkanRendererState* state = (VulkanRendererState*)backend->state;
    if (!state || !filepath) return;
    
    LOG_DEBUG("Vulkan: Queueing screenshot to %s", filepath);
    platform_strncpy(state->screenshot_path, filepath, sizeof(state->screenshot_path) - 1);
    state->screenshot_pending = true;
}

// Helper to resize instance buffer dynamically
static void ensure_instance_capacity(VulkanRendererState* state, FrameResources* frame, size_t required_count) {
    if (frame->instance_capacity >= required_count && frame->instance_buffer != VK_NULL_HANDLE) return;

    // Calculate new capacity (Start with 1024, double until fits)
    size_t new_cap = frame->instance_capacity > 0 ? frame->instance_capacity : 1024;
    while (new_cap < required_count) new_cap *= 2;
    
    // Cleanup old buffer
    if (frame->instance_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(state->device, frame->instance_buffer, NULL);
        vkFreeMemory(state->device, frame->instance_memory, NULL);
    }

    frame->instance_capacity = new_cap;
    VkDeviceSize size = new_cap * sizeof(GpuInstanceData);
    
    // Create new buffer (Host Coherent for frequent updates)
    vk_create_buffer(state, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                     &frame->instance_buffer, &frame->instance_memory);
    
    vkMapMemory(state->device, frame->instance_memory, 0, VK_WHOLE_SIZE, 0, &frame->instance_mapped);
    
    // Update Descriptor Set
    // If set doesn't exist, allocate it
    if (frame->instance_set == VK_NULL_HANDLE) {
        VkDescriptorSetAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = state->descriptor_pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &state->instance_layout
        };
        if (vkAllocateDescriptorSets(state->device, &alloc_info, &frame->instance_set) != VK_SUCCESS) {
            LOG_FATAL("Failed to allocate instance descriptor set");
        }
    }
    
    // Point set to new buffer
    VkDescriptorBufferInfo dbi = { .buffer = frame->instance_buffer, .offset = 0, .range = VK_WHOLE_SIZE };
    VkWriteDescriptorSet w = { 
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 
        .dstSet = frame->instance_set, 
        .dstBinding = 0, 
        .descriptorCount = 1, 
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 
        .pBufferInfo = &dbi 
    };
    vkUpdateDescriptorSets(state->device, 1, &w, 0, NULL);
    
    LOG_TRACE("Resized Instance Buffer to %zu elements", new_cap);
}

// --- COMPUTE SUBSYSTEM ---

static uint32_t vulkan_compute_pipeline_create(RendererBackend* backend, const void* spirv_code, size_t size, int layout_index) {
    VulkanRendererState* state = (VulkanRendererState*)backend->state;
    
    // Find free slot
    int slot = -1;
    for (int i = 0; i < MAX_COMPUTE_PIPELINES; ++i) {
        if (!state->compute_pipelines[i].active) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        LOG_ERROR("Max compute pipelines reached (%d)", MAX_COMPUTE_PIPELINES);
        return 0;
    }
    
    VkPipeline pipeline;
    VkPipelineLayout layout;
    
    // Convert void* to uint32_t* (assume 4-byte aligned and size is bytes)
    VkResult res = vk_create_compute_pipeline_shader(state, (const uint32_t*)spirv_code, size, layout_index, &pipeline, &layout);
    
    if (res != VK_SUCCESS) {
        LOG_ERROR("Failed to create compute pipeline: %d", res);
        return 0;
    }
    
    state->compute_pipelines[slot].active = true;
    state->compute_pipelines[slot].pipeline = pipeline;
    state->compute_pipelines[slot].layout = layout;
    
    return (uint32_t)(slot + 1);
}

static void vulkan_compute_pipeline_destroy(RendererBackend* backend, uint32_t pipeline_id) {
    VulkanRendererState* state = (VulkanRendererState*)backend->state;
    if (pipeline_id == 0 || pipeline_id > MAX_COMPUTE_PIPELINES) return;
    
    int idx = (int)pipeline_id - 1;
    if (state->compute_pipelines[idx].active) {
        vkDestroyPipeline(state->device, state->compute_pipelines[idx].pipeline, NULL);
        vkDestroyPipelineLayout(state->device, state->compute_pipelines[idx].layout, NULL);
        state->compute_pipelines[idx].active = false;
    }
}

static void vulkan_compute_dispatch(RendererBackend* backend, uint32_t pipeline_id, uint32_t group_x, uint32_t group_y, uint32_t group_z, void* push_constants, size_t push_constants_size) {
    VulkanRendererState* state = (VulkanRendererState*)backend->state;
    if (pipeline_id == 0 || pipeline_id > MAX_COMPUTE_PIPELINES) return;
    
    int idx = (int)pipeline_id - 1;
    if (!state->compute_pipelines[idx].active) return;
    
    VkPipeline pipeline = state->compute_pipelines[idx].pipeline;
    VkPipelineLayout layout = state->compute_pipelines[idx].layout;
    
    // Use the dedicated compute command buffer
    // Wait for fence to ensure previous submission is done
    vkWaitForFences(state->device, 1, &state->compute_fence, VK_TRUE, UINT64_MAX);
    vkResetFences(state->device, 1, &state->compute_fence);
    
    vkResetCommandBuffer(state->compute_cmd, 0);
    
    VkCommandBufferBeginInfo begin_info = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    if (vkBeginCommandBuffer(state->compute_cmd, &begin_info) != VK_SUCCESS) {
        LOG_ERROR("Failed to begin compute cmd");
        return;
    }
    
    // Bind Pipeline
    vkCmdBindPipeline(state->compute_cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    
    // Bind Descriptor Set 0 (Write Target)
    if (state->compute_write_descriptor) {
        vkCmdBindDescriptorSets(state->compute_cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &state->compute_write_descriptor, 0, NULL);
    }
    
    // Push Constants
    if (push_constants && push_constants_size > 0) {
        vkCmdPushConstants(state->compute_cmd, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, (uint32_t)push_constants_size, push_constants);
    }
    
    // Dispatch
    vkCmdDispatch(state->compute_cmd, group_x, group_y, group_z);
    
    // Barrier (Make sure writes are visible to fragment shader)
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL, // Keep it general for now
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = state->compute_target_image,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    
    vkCmdPipelineBarrier(state->compute_cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
    
    vkEndCommandBuffer(state->compute_cmd);
    
    // Submit
    VkSubmitInfo submit_info = { 
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, 
        .commandBufferCount = 1, 
        .pCommandBuffers = &state->compute_cmd 
    };
    
    vkQueueSubmit(state->queue, 1, &submit_info, state->compute_fence);
}

static void vulkan_compute_wait(RendererBackend* backend) {
    VulkanRendererState* state = (VulkanRendererState*)backend->state;
    vkWaitForFences(state->device, 1, &state->compute_fence, VK_TRUE, UINT64_MAX);
}

static bool vulkan_compile_shader(RendererBackend* backend, const char* source, size_t size, const char* stage, void** out_spv, size_t* out_spv_size) {
    (void)backend;
    
    // 1. Ensure logs dir exists
    platform_mkdir("logs");
    
    // 2. Write source to temp file
    const char* tmp_src = "logs/tmp_compile.glsl";
    const char* tmp_spv = "logs/tmp_compile.spv";
    
    FILE* f = fopen(tmp_src, "w");
    if (!f) return false;
    fwrite(source, 1, size, f);
    fclose(f);
    
    // 3. Construct Command
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "glslc -fshader-stage=%s %s -o %s", stage, tmp_src, tmp_spv);
    
    // 4. Run
    int res = system(cmd);
    if (res != 0) {
        LOG_ERROR("Vulkan: Shader compilation failed. Command: %s", cmd);
        return false;
    }
    
    // 5. Read Result
    size_t sz = 0;
    void* code = fs_read_bin(NULL, tmp_spv, &sz);
    if (!code) return false;
    
    *out_spv = code;
    *out_spv_size = sz;
    return true;
}

static bool vulkan_renderer_init(RendererBackend* backend, const RenderBackendInit* init) {
    VulkanRendererState* state = (VulkanRendererState*)backend->state;
    
    // Config
    state->window = init->window;
    state->platform_surface = init->surface;
    
    // Copy Shader Data
    if (init->vert_shader.data && init->vert_shader.size > 0) {
        state->vert_shader_src.size = init->vert_shader.size;
        state->vert_shader_src.code = malloc(init->vert_shader.size);
        memcpy(state->vert_shader_src.code, init->vert_shader.data, init->vert_shader.size);
    }
    if (init->frag_shader.data && init->frag_shader.size > 0) {
        state->frag_shader_src.size = init->frag_shader.size;
        state->frag_shader_src.code = malloc(init->frag_shader.size);
        memcpy(state->frag_shader_src.code, init->frag_shader.data, init->frag_shader.size);
    }
    
    // 1. Instance
    vk_create_instance(state);
    
    // 2. Surface
    if (!platform_create_surface(state->window, state->instance, NULL, state->platform_surface)) {
        LOG_FATAL("Failed to create surface");
        return false;
    }
    state->surface = (VkSurfaceKHR)state->platform_surface->handle;

    // 3. Device
    vk_pick_physical_and_create_device(state);

    // 4. Swapchain
    vk_create_swapchain_and_views(state, VK_NULL_HANDLE);
    
    // 5. Render Pass
    vk_create_render_pass(state);

    // 6. Resources (Depth, Command Pool, Sync)
    vk_create_depth_resources(state);
    vk_create_cmds_and_sync(state);
    
    // 7. Descriptor & Pipeline
    vk_create_descriptor_layout(state);
    vk_create_pipeline(state);

    // 8. Fonts & Textures
    vk_create_font_texture(state);
    vk_create_descriptor_pool_and_set(state);

    // 9. Static Buffers (Quad - 2 Triangles, 0..1 range)
    float quad_verts[] = {
        // Pos (x,y,z)      // UV (u,v)
        0.0f, 0.0f, 0.0f,   0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,   1.0f, 0.0f,
        1.0f, 1.0f, 0.0f,   1.0f, 1.0f,
        
        0.0f, 0.0f, 0.0f,   0.0f, 0.0f,
        1.0f, 1.0f, 0.0f,   1.0f, 1.0f,
        0.0f, 1.0f, 0.0f,   0.0f, 1.0f
    };
    VkDeviceSize v_size = sizeof(quad_verts);
    vk_create_buffer(state, v_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                     &state->unit_quad_buffer, &state->unit_quad_memory);
    
    void* v_map;
    vkMapMemory(state->device, state->unit_quad_memory, 0, VK_WHOLE_SIZE, 0, &v_map);
    memcpy(v_map, quad_verts, v_size);
    vkUnmapMemory(state->device, state->unit_quad_memory);
    
    // 10. Per-Frame Instance Resources
    for (int i = 0; i < 2; ++i) {
        state->frame_resources[i].instance_capacity = 0;
        state->frame_resources[i].instance_buffer = VK_NULL_HANDLE;
        state->frame_resources[i].instance_set = VK_NULL_HANDLE;
        ensure_instance_capacity(state, &state->frame_resources[i], 1024); // Initial allocation
    }

    // 11. Compute Infrastructure
    VkFenceCreateInfo fci = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT };
    vkCreateFence(state->device, &fci, NULL, &state->compute_fence);
    
    VkCommandBufferAllocateInfo cbai = { 
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, 
        .commandPool = state->cmdpool, 
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, 
        .commandBufferCount = 1 
    };
    vkAllocateCommandBuffers(state->device, &cbai, &state->compute_cmd);
    
    vk_ensure_compute_target(state, 512, 512);

    LOG_INFO("Vulkan Initialized.");
    return true;
}

static void vulkan_renderer_update_viewport(RendererBackend* backend, int width, int height) {
    VulkanRendererState* state = (VulkanRendererState*)backend->state;
    // Handle resize
    if (width == 0 || height == 0) return;
    
    vkDeviceWaitIdle(state->device);
    
    // Save old swapchain handle
    VkSwapchainKHR old_swapchain = state->swapchain;
    
    // Cleanup old resources (destroys framebuffers, cmd buffers, pipelines, etc.)
    // Pass 'true' to keep the swapchain handle alive in 'state->swapchain' for now,
    // although we have a copy in 'old_swapchain'.
    vk_cleanup_swapchain(state, true); 
    
    // Create new swapchain, passing the old one for recycling
    vk_create_swapchain_and_views(state, old_swapchain);
    
    // Destroy the old swapchain handle (driver requires this if not NULL)
    // Note: vkCreateSwapchainKHR uses oldSwapchain for optimization but doesn't destroy it.
    if (old_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(state->device, old_swapchain, NULL);
    }
    
    // Recreate dependent resources
    vk_create_depth_resources(state);
    vk_create_render_pass(state);
    
    // Recreate Framebuffers, Command Pool, and Command Buffers (which were destroyed by cleanup)
    vk_create_cmds_and_sync(state);
    
    // Recreate Pipeline (which depends on Render Pass and was destroyed)
    vk_create_pipeline(state);
    
    // Reset frame cursor as sync objects were recreated
    state->current_frame_cursor = 0;
}

static void vulkan_renderer_render_scene(RendererBackend* backend, const Scene* scene) {
    VulkanRendererState* state = (VulkanRendererState*)backend->state;
    if (!state || !scene) return;
    
    // Frame Sync
    vkWaitForFences(state->device, 1, &state->fences[state->current_frame_cursor], VK_TRUE, UINT64_MAX);
    vkWaitForFences(state->device, 1, &state->fences[state->current_frame_cursor], VK_TRUE, UINT64_MAX);
    
    uint32_t image_index;
    VkResult result = vkAcquireNextImageKHR(state->device, state->swapchain, UINT64_MAX, 
                                            state->sem_img_avail, VK_NULL_HANDLE, &image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        // resize
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        return;
    }
    
    vkResetFences(state->device, 1, &state->fences[state->current_frame_cursor]);
    
    // --- UPDATE RESOURCES ---
    FrameResources* frame = &state->frame_resources[state->current_frame_cursor];
    
    // 1. Check Capacity and Resize if needed
    size_t count = 0;
    const SceneObject* scene_objects = scene_get_all_objects(scene, &count);
    
    if (count == 0) count = 1; // Avoid 0 size
    ensure_instance_capacity(state, frame, count);
    
    // 2. Upload Data
    GpuInstanceData* instances = (GpuInstanceData*)frame->instance_mapped;
    if (scene_objects) {
        for (size_t i = 0; i < (count == 1 && !scene_objects ? 0 : count); ++i) { // Handle dummy count=1 if objects are null
             // Re-check count usage. If scene_objects is NULL, count is 0 from accessor, but we forced it to 1 above for buffer alloc.
             // So we must use original count for loop, or check scene_objects.
             // Better:
        }
    }
    
    // Refined loop:
    size_t actual_count = 0;
    scene_objects = scene_get_all_objects(scene, &actual_count);
    
    for (size_t i = 0; i < actual_count; ++i) {
        const SceneObject* obj = &scene_objects[i];
        
        Mat4 m = mat4_identity();
        Mat4 s = mat4_scale(obj->scale);
        Mat4 t = mat4_translation(obj->position);
        m = mat4_multiply(&t, &s);
        
        instances[i].model = m;
        instances[i].color = obj->color;
        instances[i].uv_rect = obj->uv_rect;
        instances[i].params_1 = obj->shader_params_0;
        instances[i].params_2 = obj->shader_params_1; // Pass through extra data (9-slice or curves)
        instances[i].clip_rect = obj->clip_rect;
    }
    
    // --- COMMAND RECORDING ---
    VkCommandBuffer cmd = state->cmdbuffers[state->current_frame_cursor];
    vkResetCommandBuffer(cmd, 0);
    
    VkCommandBufferBeginInfo begin_info = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(cmd, &begin_info);
    
    // Begin Pass
    VkRenderPassBeginInfo pass_info = {.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    pass_info.renderPass = state->render_pass;
    pass_info.framebuffer = state->framebuffers[image_index];
    pass_info.renderArea.offset = (VkOffset2D){0, 0};
    pass_info.renderArea.extent = state->swapchain_extent;
    
    VkClearValue clear_values[2];
    clear_values[0].color = (VkClearColorValue){{0.1f, 0.1f, 0.12f, 1.0f}};
    clear_values[1].depthStencil = (VkClearDepthStencilValue){1.0f, 0};
    
    pass_info.clearValueCount = 2;
    pass_info.pClearValues = clear_values;
    
    vkCmdBeginRenderPass(cmd, &pass_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state->pipeline);
    
    // Viewport
    VkViewport viewport = {0.0f, 0.0f, (float)state->swapchain_extent.width, (float)state->swapchain_extent.height, 0.0f, 1.0f};
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    
    VkRect2D scissor = {{0, 0}, state->swapchain_extent};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Bind Quad Vertex Buffer
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, &state->unit_quad_buffer, offsets);

    // Bind Descriptors
    // Set 0: Global Textures (Font)
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state->pipeline_layout, 0, 1, &state->descriptor_set, 0, NULL);
    
    // Set 1: Instance Data (Per-Frame)
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state->pipeline_layout, 1, 1, &frame->instance_set, 0, NULL);
    
    // Push Constants (ViewProj)
    SceneCamera cam = scene_get_camera(scene);
    vkCmdPushConstants(cmd, state->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Mat4), &cam.view_matrix); 

    // Draw
    if (actual_count > 0) {
        vkCmdDraw(cmd, 6, actual_count, 0, 0);
    }
    
    vkCmdEndRenderPass(cmd);

    // Screenshot Buffer
    VkBuffer screenshot_buffer = VK_NULL_HANDLE;
    VkDeviceMemory screenshot_memory = VK_NULL_HANDLE;

    if (state->screenshot_pending) {
        LOG_TRACE("Screenshot: Starting capture sequence...");
        // Prepare Buffer
        uint32_t w = state->swapchain_extent.width;
        uint32_t h = state->swapchain_extent.height;
        VkDeviceSize size = w * h * 4;
        
        vk_create_buffer(state, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                         &screenshot_buffer, &screenshot_memory);
                         
        LOG_TRACE("Screenshot: Buffer created (Handle: %p)", (void*)screenshot_buffer);

        // Transition Image: PRESENT_SRC_KHR -> TRANSFER_SRC_OPTIMAL
        VkImageMemoryBarrier barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, // Assumes renderpass finalLayout
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = state->swapchain_imgs[image_index],
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        };
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 
                             0, 0, NULL, 0, NULL, 1, &barrier);

        // Copy
        VkBufferImageCopy region = {
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
            .imageOffset = {0, 0, 0},
            .imageExtent = {w, h, 1}
        };
        vkCmdCopyImageToBuffer(cmd, state->swapchain_imgs[image_index], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, screenshot_buffer, 1, &region);
        
        // Transition Back: TRANSFER_SRC -> PRESENT_SRC
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = 0;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 
                             0, 0, NULL, 0, NULL, 1, &barrier);
    }

    vkEndCommandBuffer(cmd);
    
    // Submit
    VkSubmitInfo submit_info = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO};
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &state->sem_img_avail;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &state->sem_render_done;
    
    vkQueueSubmit(state->queue, 1, &submit_info, state->fences[state->current_frame_cursor]);

    // Save Screenshot (Offloaded to thread)
    if (state->screenshot_pending && screenshot_buffer) {
        LOG_TRACE("Screenshot: Waiting for GPU...");
        vkQueueWaitIdle(state->queue);
        
        uint32_t w = state->swapchain_extent.width;
        uint32_t h = state->swapchain_extent.height;
        
        LOG_TRACE("Screenshot: Mapping memory...");
        uint8_t* data = NULL;
        vkMapMemory(state->device, screenshot_memory, 0, VK_WHOLE_SIZE, 0, (void**)&data);
        
        if (data) {
            LOG_TRACE("Screenshot: Copying to host buffer...");
            size_t size = w * h * 4;
            uint8_t* host_copy = (uint8_t*)malloc(size);
            if (host_copy) {
                memcpy(host_copy, data, size);
                
                ScreenshotContext* ctx = (ScreenshotContext*)malloc(sizeof(ScreenshotContext));
                if (ctx) {
                    platform_strncpy(ctx->path, state->screenshot_path, sizeof(ctx->path) - 1);
                    ctx->width = (int)w;
                    ctx->height = (int)h;
                    ctx->data = host_copy;
                    ctx->needs_swizzle = (state->swapchain_format == VK_FORMAT_B8G8R8A8_UNORM || state->swapchain_format == VK_FORMAT_B8G8R8A8_SRGB);

                    thrd_t t;
                    if (thrd_create(&t, save_screenshot_task, ctx) == thrd_success) {
                        thrd_detach(t);
                        LOG_TRACE("Screenshot: Offloaded to thread.");
                    } else {
                        LOG_ERROR("Screenshot: Failed to create thread!");
                        free(host_copy);
                        free(ctx);
                    }
                } else {
                    LOG_ERROR("Screenshot: Failed to allocate context!");
                    free(host_copy);
                }
            } else {
                LOG_ERROR("Screenshot: Failed to allocate host buffer!");
            }
            vkUnmapMemory(state->device, screenshot_memory);
        } else {
            LOG_ERROR("Screenshot: Failed to map memory!");
        }
        
        vkDestroyBuffer(state->device, screenshot_buffer, NULL);
        vkFreeMemory(state->device, screenshot_memory, NULL);
        
        state->screenshot_pending = false;
        LOG_TRACE("Screenshot: Done.");
    }
    
    // Present
    VkPresentInfoKHR present_info = {.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &state->sem_render_done;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &state->swapchain;
    present_info.pImageIndices = &image_index;
    
    vkQueuePresentKHR(state->queue, &present_info);
    
    state->current_frame_cursor = (state->current_frame_cursor + 1) % 2;
}

static void vulkan_renderer_cleanup(RendererBackend* backend) {
    if (!backend) return;
    VulkanRendererState* state = (VulkanRendererState*)backend->state;
    if (state) {
        vkDeviceWaitIdle(state->device);
        
        // Clean up per-frame resources
        for (int i = 0; i < 2; ++i) {
            if (state->frame_resources[i].instance_buffer) {
                vkDestroyBuffer(state->device, state->frame_resources[i].instance_buffer, NULL);
                vkFreeMemory(state->device, state->frame_resources[i].instance_memory, NULL);
            }
        }
        
        if (state->vert_shader_src.code) free(state->vert_shader_src.code);
        if (state->frag_shader_src.code) free(state->frag_shader_src.code);

        vk_destroy_device_resources(state);
        
        if (state->surface) {
            platform_destroy_surface(state->instance, NULL, state->platform_surface);
        }
        vkDestroyInstance(state->instance, NULL);
        free(state);
    }
    
    free(backend);
}

// Factory
RendererBackend* vulkan_renderer_backend(void) {
    // Allocate Backend and State on Heap
    RendererBackend* backend = (RendererBackend*)calloc(1, sizeof(RendererBackend));
    if (!backend) return NULL;

    VulkanRendererState* state = (VulkanRendererState*)calloc(1, sizeof(VulkanRendererState));
    if (!state) {
        free(backend);
        return NULL;
    }
    
    backend->id = "vulkan";
    backend->state = state;
    backend->init = vulkan_renderer_init;
    backend->render_scene = vulkan_renderer_render_scene;
    backend->update_viewport = vulkan_renderer_update_viewport;
    backend->cleanup = vulkan_renderer_cleanup;
    backend->request_screenshot = vulkan_renderer_request_screenshot;
    
    // Compute
    backend->compute_pipeline_create = vulkan_compute_pipeline_create;
    backend->compute_pipeline_destroy = vulkan_compute_pipeline_destroy;
    backend->compute_dispatch = vulkan_compute_dispatch;
    backend->compute_wait = vulkan_compute_wait;
    backend->compile_shader = vulkan_compile_shader;
    
    return backend;
}
