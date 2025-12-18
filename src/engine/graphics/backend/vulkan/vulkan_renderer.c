#include "engine/graphics/backend/vulkan/vulkan_renderer.h"

#include "engine/graphics/backend/vulkan/vk_types.h"
#include "engine/graphics/backend/vulkan/vk_context.h"
#include "engine/graphics/backend/vulkan/vk_swapchain.h"
#include "engine/graphics/backend/vulkan/vk_pipeline.h"
#include "engine/graphics/backend/vulkan/vk_resources.h"
#include "engine/graphics/backend/vulkan/vk_utils.h"
#include "engine/graphics/backend/vulkan/vk_compute.h"
#include "engine/graphics/text/font.h"

#include "foundation/logger/logger.h"
#include "foundation/platform/platform.h"
#include "foundation/math/layout_geometry.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

// Must match shader struct layout (std140)
typedef struct GpuInstanceData {
    Mat4 model;
    Vec4 color;
    Vec4 uv_rect;
    Vec4 params;
    Vec4 extra;
    Vec4 clip_rect; // Added
} GpuInstanceData;

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
    
    LOG_INFO("Resized Instance Buffer to %zu elements", new_cap);
}

static bool vulkan_renderer_init(RendererBackend* backend, const RenderBackendInit* init) {
    VulkanRendererState* state = (VulkanRendererState*)backend->state;
    
    // Config
    state->window = init->window;
    state->platform_surface = init->surface;
    state->vert_spv = init->vert_spv;
    state->frag_spv = init->frag_spv;
    state->font_path = init->font_path;
    
    // Callbacks
    state->get_required_instance_extensions = (bool (*)(const char***, uint32_t*))init->get_required_extensions;
    state->create_surface = (bool (*)(PlatformWindow*, VkInstance, const VkAllocationCallbacks*, PlatformSurface*))init->create_surface;
    state->destroy_surface = (void (*)(VkInstance, const VkAllocationCallbacks*, PlatformSurface*))init->destroy_surface;
    state->get_framebuffer_size = init->get_framebuffer_size;
    state->wait_events = init->wait_events;

    // Logger
    render_logger_init(&backend->logger, init->logger_config, "Vulkan");
    state->logger = &backend->logger;

    // 1. Instance
    vk_create_instance(state);
    
    // 2. Surface
    if (!state->create_surface(state->window, state->instance, NULL, state->platform_surface)) {
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
    vk_create_pipeline(state, state->vert_spv, state->frag_spv);

    // 8. Fonts & Textures
    vk_create_font_texture(state);
    vk_create_descriptor_pool_and_set(state);

    // 9. Static Buffers (Quad)
    float quad_verts[] = {
        // Positions      // UVs
        -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,
         0.5f, -0.5f, 0.0f,  1.0f, 0.0f,
         0.5f,  0.5f, 0.0f,  1.0f, 1.0f,
        -0.5f,  0.5f, 0.0f,  0.0f, 1.0f
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

    LOG_INFO("Vulkan Initialized.");
    return true;
}

static void vulkan_renderer_update_viewport(RendererBackend* backend, int width, int height) {
    VulkanRendererState* state = (VulkanRendererState*)backend->state;
    // Handle resize
    if (width == 0 || height == 0) return;
    
    vkDeviceWaitIdle(state->device);
    vk_cleanup_swapchain(state, true); // Keep handle? No, usually destroy old.
    
    vk_create_swapchain_and_views(state, VK_NULL_HANDLE);
    vk_create_depth_resources(state);
    vk_create_render_pass(state); // Usually compatible, but safer to recreate if format changed
    // Pipeline might need recreation if viewport is dynamic state or baked.
}

static void vulkan_renderer_render_scene(RendererBackend* backend, const Scene* scene) {
    VulkanRendererState* state = (VulkanRendererState*)backend->state;
    if (!state || !scene) return;
    
    // Frame Sync
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
    size_t count = scene->object_count;
    if (count == 0) count = 1; // Avoid 0 size
    ensure_instance_capacity(state, frame, count);
    
    // 2. Upload Data
    GpuInstanceData* instances = (GpuInstanceData*)frame->instance_mapped;
    for (size_t i = 0; i < scene->object_count; ++i) {
        SceneObject* obj = &scene->objects[i];
        
        Mat4 m = mat4_identity();
        Mat4 s = mat4_scale(obj->scale);
        Mat4 t = mat4_translation(obj->position);
        m = mat4_multiply(&t, &s);
        
        instances[i].model = m;
        instances[i].color = obj->color;
        instances[i].uv_rect = obj->uv_rect;
        instances[i].params = obj->params;
        instances[i].extra = (Vec4){0,0,0,0}; // Curve data
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
    vkCmdPushConstants(cmd, state->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Mat4), &scene->camera.view_matrix); 

    // Draw
    if (scene->object_count > 0) {
        vkCmdDraw(cmd, 6, scene->object_count, 0, 0);
    }
    
    vkCmdEndRenderPass(cmd);
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
    VulkanRendererState* state = (VulkanRendererState*)backend->state;
    vkDeviceWaitIdle(state->device);
    
    // Clean up per-frame resources
    for (int i = 0; i < 2; ++i) {
        if (state->frame_resources[i].instance_buffer) {
            vkDestroyBuffer(state->device, state->frame_resources[i].instance_buffer, NULL);
            vkFreeMemory(state->device, state->frame_resources[i].instance_memory, NULL);
        }
    }

    vk_destroy_device_resources(state);
    
    if (state->surface) {
        state->destroy_surface(state->instance, NULL, state->platform_surface);
    }
    vkDestroyInstance(state->instance, NULL);
    free(state);
}

// Factory
RendererBackend* vulkan_renderer_backend(void) {
    static RendererBackend backend;
    static VulkanRendererState state;
    
    backend.id = "vulkan";
    backend.state = &state;
    backend.init = vulkan_renderer_init;
    backend.render_scene = vulkan_renderer_render_scene;
    backend.update_viewport = vulkan_renderer_update_viewport;
    backend.cleanup = vulkan_renderer_cleanup;
    
    // Compute hooks
    // backend.run_compute_image = vk_run_compute_image_wrapper; 
    
    return &backend;
}
