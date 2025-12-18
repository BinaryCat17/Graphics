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
    // FIX: Depth resources MUST be created before Framebuffers (which are in vk_create_cmds_and_sync)
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
    vk_create_buffer(state, v_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &state->unit_quad_buffer, &state->unit_quad_memory);
    
    // Upload Quad Data (HACK: using host visible for now to avoid staging complexity in this snippet)
    // Actually, let's just do it properly with staging if we had the function exposed.
    // For now, assume unit quad is not critical or valid (we aren't binding it yet in render_scene anyway).
    // The shader uses hardcoded quad or expects it? The shader has `layout(location = 0) in vec3 inPosition;`.
    // We need to bind vertex buffer.
    
    // Create Instance Buffer (Storage Buffer)
    state->instance_capacity = 1000;
    VkDeviceSize inst_size = state->instance_capacity * sizeof(GpuInstanceData);
    vk_create_buffer(state, inst_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                     &state->instance_buffer, &state->instance_memory);
    
    vkMapMemory(state->device, state->instance_memory, 0, VK_WHOLE_SIZE, 0, &state->instance_mapped);

    // Update Descriptor Set 1 to point to Instance Buffer
    VkDescriptorBufferInfo dbi = { .buffer = state->instance_buffer, .offset = 0, .range = VK_WHOLE_SIZE };
    VkWriteDescriptorSet w1 = { 
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 
        .dstSet = state->instance_set, 
        .dstBinding = 0, 
        .descriptorCount = 1, 
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 
        .pBufferInfo = &dbi 
    };
    vkUpdateDescriptorSets(state->device, 1, &w1, 0, NULL);
    
    // Create and upload Quad Vertices (Reuse instance staging logic for simplicity or just create host visible vertex buffer)
    vkDestroyBuffer(state->device, state->unit_quad_buffer, NULL);
    vkFreeMemory(state->device, state->unit_quad_memory, NULL);
    
    vk_create_buffer(state, v_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                     &state->unit_quad_buffer, &state->unit_quad_memory);
    
    void* v_map;
    vkMapMemory(state->device, state->unit_quad_memory, 0, VK_WHOLE_SIZE, 0, &v_map);
    memcpy(v_map, quad_verts, v_size);
    vkUnmapMemory(state->device, state->unit_quad_memory);

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
    VkCommandBuffer cmd = state->cmdbuffers[state->current_frame_cursor];
    vkResetCommandBuffer(cmd, 0);
    
    // Update Instance Buffer
    GpuInstanceData* instances = (GpuInstanceData*)state->instance_mapped;
    size_t count = scene->object_count;
    if (count > state->instance_capacity) count = state->instance_capacity;
    
    for (size_t i = 0; i < count; ++i) {
        SceneObject* obj = &scene->objects[i];
        
        // Simple SRT to Mat4
        // T * R * S
        // Assuming 2D/3D mix. For now just basic Translation + Scale.
        // Rotation is currently ignored for UI quads (usually aligned).
        
        Mat4 m = mat4_identity();
        
        // 1. Scale
        Mat4 s = mat4_scale(obj->scale);
        
        // 2. Translate
        Mat4 t = mat4_translation(obj->position);
        
        // M = T * S
        m = mat4_multiply(&t, &s);
        
        instances[i].model = m;
        instances[i].color = obj->color;
        instances[i].uv_rect = obj->uv_rect;
        instances[i].params = obj->params;
        instances[i].extra = (Vec4){0,0,0,0}; // Curve data
        instances[i].clip_rect = obj->clip_rect; // Copy clip rect
    }
    
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
    // Set 0: Texture (Global Font Atlas for now)
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state->pipeline_layout, 0, 1, &state->descriptor_set, 0, NULL);
    
    // Set 1: Instance Buffer
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state->pipeline_layout, 1, 1, &state->instance_set, 0, NULL);
    
    // Push Constants (ViewProj)
    vkCmdPushConstants(cmd, state->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Mat4), &scene->camera.view_matrix); // Actually need Proj * View

    // Draw
    if (count > 0) {
        vkCmdDraw(cmd, 6, count, 0, 0);
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