#define _POSIX_C_SOURCE 200809L
#include "render/vulkan/vulkan_renderer.h"

#include "render/vulkan/vk_types.h"
#include "render/vulkan/vk_context.h"
#include "render/vulkan/vk_swapchain.h"
#include "render/vulkan/vk_pipeline.h"
#include "render/vulkan/vk_resources.h"
#include "render/vulkan/vk_ui_render.h"
#include "render/vulkan/vk_utils.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static RendererBackend g_vulkan_backend;

static void record_cmdbuffer(VulkanRendererState* state, uint32_t idx, const FrameResources *frame) {
    VkCommandBuffer cb = state->cmdbuffers[idx];
    vkResetCommandBuffer(cb, 0);
    VkCommandBufferBeginInfo binfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    vkBeginCommandBuffer(cb, &binfo);

    VkClearValue clr[2];
    clr[0].color = (VkClearColorValue){{0.9f,0.9f,0.9f,1.0f}};
    clr[1].depthStencil = (VkClearDepthStencilValue){1.0f, 0};
    VkRenderPassBeginInfo rpbi = { .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, .renderPass = state->render_pass, .framebuffer = state->framebuffers[idx], .renderArea = {.offset = {0,0}, .extent = state->swapchain_extent }, .clearValueCount = 2, .pClearValues = clr };
    vkCmdBeginRenderPass(cb, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, state->pipeline);
    ViewConstants pc = { .viewport = { (float)state->swapchain_extent.width, (float)state->swapchain_extent.height } };
    vkCmdPushConstants(cb, state->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ViewConstants), &pc);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, state->pipeline_layout, 0, 1, &state->descriptor_set, 0, NULL);
    if (frame && frame->vertex_buffer != VK_NULL_HANDLE && frame->vertex_count > 0) {
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cb, 0, 1, &frame->vertex_buffer, &offset);
        /* simple draw: vertices are triangles (every 3 vertices) */
        vkCmdDraw(cb, (uint32_t)frame->vertex_count, 1, 0, 0);
    }

    vkCmdEndRenderPass(cb);
    vkEndCommandBuffer(cb);
}

static bool recover_device_loss(VulkanRendererState* state) {
    fprintf(stderr, "Device lost detected; tearing down and recreating logical device and swapchain resources...\n");
    if (state->device) vkDeviceWaitIdle(state->device);
    vk_destroy_device_resources(state);
    if (state->device) {
        vkDestroyDevice(state->device, NULL);
        state->device = VK_NULL_HANDLE;
    }

    vk_recreate_instance_and_surface(state);

    vk_pick_physical_and_create_device(state);
    vk_create_swapchain_and_views(state, VK_NULL_HANDLE);
    if (!state->swapchain) return false;
    vk_create_depth_resources(state);
    vk_create_render_pass(state);
    vk_create_descriptor_layout(state);
    vk_create_pipeline(state, state->vert_spv, state->frag_spv);
    vk_create_cmds_and_sync(state);
    vk_create_font_texture(state);
    vk_create_descriptor_pool_and_set(state);
    for (size_t i = 0; i < 2; ++i) {
        state->frame_resources[i].stage = FRAME_AVAILABLE;
        if (vk_build_vertices_from_widgets(state, &state->frame_resources[i])) {
            vk_upload_vertices(state, &state->frame_resources[i]);
        }
    }
    return true;
}

static void draw_frame(VulkanRendererState* state) {
    if (state->swapchain == VK_NULL_HANDLE) return;
    uint32_t img_idx;
    VkResult acq = vkAcquireNextImageKHR(state->device, state->swapchain, UINT64_MAX, state->sem_img_avail, VK_NULL_HANDLE, &img_idx);
    if (acq == VK_ERROR_DEVICE_LOST) { if (!recover_device_loss(state)) fatal_vk("vkAcquireNextImageKHR", acq); return; }
    if (acq == VK_ERROR_OUT_OF_DATE_KHR || acq == VK_SUBOPTIMAL_KHR) { 
        vkDeviceWaitIdle(state->device);
        VkSwapchainKHR old_swapchain = state->swapchain;
        vk_cleanup_swapchain(state, true); 
        
        vk_create_swapchain_and_views(state, old_swapchain);
        if (!state->swapchain) {
             if (old_swapchain) vkDestroySwapchainKHR(state->device, old_swapchain, NULL);
             return; 
        }
        vk_create_depth_resources(state);
        vk_create_render_pass(state);
        vk_create_pipeline(state, state->vert_spv, state->frag_spv);
        vk_create_cmds_and_sync(state);
        
        if (old_swapchain) vkDestroySwapchainKHR(state->device, old_swapchain, NULL);
        return; 
    }
    if (acq != VK_SUCCESS) fatal_vk("vkAcquireNextImageKHR", acq);
    
    vkWaitForFences(state->device, 1, &state->fences[img_idx], VK_TRUE, UINT64_MAX);
    vkResetFences(state->device, 1, &state->fences[img_idx]);

    if (state->image_frame_owner && state->image_frame_owner[img_idx] >= 0) {
        int idx = state->image_frame_owner[img_idx];
        if (idx >= 0 && idx < 2) {
            FrameResources* owner = &state->frame_resources[idx];
            VkFence tracked_fence = owner->inflight_fence;
            if (tracked_fence && tracked_fence != state->fences[img_idx]) {
                vkWaitForFences(state->device, 1, &tracked_fence, VK_TRUE, UINT64_MAX);
            }
            owner->stage = FRAME_AVAILABLE;
            owner->inflight_fence = VK_NULL_HANDLE;
            state->image_frame_owner[img_idx] = -1;
        }
    }

    FrameResources *frame = &state->frame_resources[state->current_frame_cursor % 2];
    state->current_frame_cursor = (state->current_frame_cursor + 1) % 2;

    if (frame->stage == FRAME_SUBMITTED && frame->inflight_fence) {
        if (frame->inflight_fence != state->fences[img_idx]) {
            vkWaitForFences(state->device, 1, &frame->inflight_fence, VK_TRUE, UINT64_MAX);
        }
        frame->stage = FRAME_AVAILABLE;
        frame->inflight_fence = VK_NULL_HANDLE;
        if (state->image_frame_owner) {
            int frame_idx = (int)(frame - state->frame_resources);
            for (uint32_t i = 0; i < state->swapchain_img_count; ++i) {
                if (state->image_frame_owner[i] == frame_idx) state->image_frame_owner[i] = -1;
            }
        }
    }
    frame->stage = FRAME_FILLING;

    bool built = vk_build_vertices_from_widgets(state, frame);
    if (!built) {
        frame->vertex_count = 0;
    }

    if (!vk_upload_vertices(state, frame)) {
        frame->vertex_count = 0;
    }
    record_cmdbuffer(state, img_idx, frame);

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .waitSemaphoreCount = 1, .pWaitSemaphores = &state->sem_img_avail, .pWaitDstStageMask = &waitStage, .commandBufferCount = 1, .pCommandBuffers = &state->cmdbuffers[img_idx], .signalSemaphoreCount = 1, .pSignalSemaphores = &state->sem_render_done };
    
    double submit_start = vk_now_ms();
    VkResult submit = vkQueueSubmit(state->queue, 1, &si, state->fences[img_idx]);
    vk_log_command(state, "vkQueueSubmit", "draw", submit_start);
    
    if (submit == VK_ERROR_DEVICE_LOST) {
        if (!recover_device_loss(state)) fatal_vk("vkQueueSubmit", submit);
        return;
    }
    if (submit != VK_SUCCESS) fatal_vk("vkQueueSubmit", submit);

    frame->stage = FRAME_SUBMITTED;
    frame->inflight_fence = state->fences[img_idx];
    if (state->image_frame_owner) {
        int frame_idx = (int)(frame - state->frame_resources);
        for (uint32_t i = 0; i < state->swapchain_img_count; ++i) {
            if (state->image_frame_owner[i] == frame_idx) state->image_frame_owner[i] = -1;
        }
        state->image_frame_owner[img_idx] = frame_idx;
    }
    
    VkPresentInfoKHR pi = { .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, .waitSemaphoreCount = 1, .pWaitSemaphores = &state->sem_render_done, .swapchainCount = 1, .pSwapchains = &state->swapchain, .pImageIndices = &img_idx };
    double present_start = vk_now_ms();
    VkResult present = vkQueuePresentKHR(state->queue, &pi);
    vk_log_command(state, "vkQueuePresentKHR", "present", present_start);
    
    if (present == VK_ERROR_DEVICE_LOST) { if (!recover_device_loss(state)) fatal_vk("vkQueuePresentKHR", present); return; }
    if (present == VK_ERROR_OUT_OF_DATE_KHR || present == VK_SUBOPTIMAL_KHR) { 
        vkDeviceWaitIdle(state->device);
        VkSwapchainKHR old_swapchain = state->swapchain;
        vk_cleanup_swapchain(state, true);
        vk_create_swapchain_and_views(state, old_swapchain);
        if (!state->swapchain) { if (old_swapchain) vkDestroySwapchainKHR(state->device, old_swapchain, NULL); return; }
        vk_create_depth_resources(state);
        vk_create_render_pass(state);
        vk_create_pipeline(state, state->vert_spv, state->frag_spv);
        vk_create_cmds_and_sync(state);
        if (old_swapchain) vkDestroySwapchainKHR(state->device, old_swapchain, NULL);
        return; 
    }
    if (present != VK_SUCCESS) fatal_vk("vkQueuePresentKHR", present);
}

static bool vk_backend_init(RendererBackend* backend, const RenderBackendInit* init) {
    if (!backend || !init) return false;
    
    VulkanRendererState* state = calloc(1, sizeof(VulkanRendererState));
    if (!state) return false;
    backend->state = state;

    render_logger_init(&backend->logger, init->logger_config, backend->id);
    state->logger = &backend->logger;

    state->window = init->window;
    state->platform_surface = init->surface;
    state->get_required_instance_extensions = init->get_required_instance_extensions;
    state->create_surface = (bool (*)(PlatformWindow*, VkInstance, const VkAllocationCallbacks*, PlatformSurface*))init->create_surface;
    state->destroy_surface = (void (*)(VkInstance, const VkAllocationCallbacks*, PlatformSurface*))init->destroy_surface;
    state->get_framebuffer_size = init->get_framebuffer_size;
    state->wait_events = init->wait_events;
    state->widgets = init->widgets;
    state->display_list = init->display_list;
    state->vert_spv = init->vert_spv;
    state->frag_spv = init->frag_spv;
    state->font_path = init->font_path;

    if (!state->window || !state->platform_surface || !state->get_required_instance_extensions || !state->create_surface ||
        !state->destroy_surface || !state->get_framebuffer_size || !state->wait_events) {
        fprintf(stderr, "Vulkan renderer missing platform callbacks.\n");
        free(state);
        backend->state = NULL;
        return false;
    }

    if (init->transformer) {
        state->transformer = *init->transformer;
    } else {
        coordinate_system2d_init(&state->transformer, 1.0f, 1.0f, (Vec2){0.0f, 0.0f});
    }

    state->current_frame_cursor = 0;
    for (size_t i = 0; i < 2; ++i) {
        state->frame_resources[i].stage = FRAME_AVAILABLE;
        state->frame_resources[i].inflight_fence = VK_NULL_HANDLE;
        state->frame_resources[i].vertex_count = 0;
    }

    vk_create_instance(state);
    if (!state->create_surface(state->window, state->instance, NULL, state->platform_surface)) return false;
    state->surface = (VkSurfaceKHR)(state->platform_surface ? state->platform_surface->handle : NULL);

    vk_pick_physical_and_create_device(state);
    vk_create_swapchain_and_views(state, VK_NULL_HANDLE);
    vk_create_depth_resources(state);
    vk_create_render_pass(state);
    vk_create_descriptor_layout(state);
    vk_create_pipeline(state, state->vert_spv, state->frag_spv);
    vk_create_cmds_and_sync(state);

    vk_build_font_atlas(state);
    vk_create_font_texture(state);
    vk_create_descriptor_pool_and_set(state);
    for (size_t i = 0; i < 2; ++i) {
        state->frame_resources[i].stage = FRAME_AVAILABLE;
        vk_build_vertices_from_widgets(state, &state->frame_resources[i]);
        vk_upload_vertices(state, &state->frame_resources[i]);
    }
    return true;
}

static void vk_backend_update_transformer(RendererBackend* backend, const CoordinateTransformer* transformer) {
    if (!backend || !backend->state || !transformer) return;
    VulkanRendererState* state = (VulkanRendererState*)backend->state;
    state->transformer = *transformer;
    state->transformer.viewport_size = (Vec2){(float)state->swapchain_extent.width, (float)state->swapchain_extent.height};
}

static void vk_backend_update_ui(RendererBackend* backend, WidgetArray widgets, DisplayList display_list) {
    if (!backend || !backend->state) return;
    VulkanRendererState* state = (VulkanRendererState*)backend->state;
    state->widgets = widgets;
    state->display_list = display_list;
}

static void vk_backend_draw(RendererBackend* backend) {
    if (!backend || !backend->state) return;
    VulkanRendererState* state = (VulkanRendererState*)backend->state;
    draw_frame(state);
}

static void vk_backend_cleanup(RendererBackend* backend) {
    if (!backend || !backend->state) return;
    VulkanRendererState* state = (VulkanRendererState*)backend->state;
    
    if (state->device) vkDeviceWaitIdle(state->device);
    
    free(state->atlas);
    state->atlas = NULL;
    free(state->ttf_buffer);
    state->ttf_buffer = NULL;
    
    for (size_t i = 0; i < 2; ++i) {
        free(state->frame_resources[i].cpu.background_vertices);
        free(state->frame_resources[i].cpu.text_vertices);
        free(state->frame_resources[i].cpu.vertices);
        state->frame_resources[i].cpu.background_vertices = NULL;
        state->frame_resources[i].cpu.text_vertices = NULL;
        state->frame_resources[i].cpu.vertices = NULL;
        state->frame_resources[i].cpu.background_capacity = 0;
        state->frame_resources[i].cpu.text_capacity = 0;
        state->frame_resources[i].cpu.vertex_capacity = 0;
    }
    
    vk_destroy_device_resources(state);
    
    if (state->device) { vkDestroyDevice(state->device, NULL); state->device = VK_NULL_HANDLE; }
    if (state->platform_surface && state->destroy_surface && state->instance) {
        state->destroy_surface(state->instance, NULL, state->platform_surface);
    } else if (state->surface && state->instance) {
        vkDestroySurfaceKHR(state->instance, state->surface, NULL);
    }
    state->surface = VK_NULL_HANDLE;
    if (state->instance) { vkDestroyInstance(state->instance, NULL); state->instance = VK_NULL_HANDLE; }
    
    render_logger_cleanup(state->logger);
    state->logger = NULL;
    free(state);
    backend->state = NULL;
}

RendererBackend* vulkan_renderer_backend(void) {
    g_vulkan_backend.id = "vulkan";
    g_vulkan_backend.state = NULL; // State is allocated in init
    g_vulkan_backend.init = vk_backend_init;
    g_vulkan_backend.update_transformer = vk_backend_update_transformer;
    g_vulkan_backend.update_ui = vk_backend_update_ui;
    g_vulkan_backend.draw = vk_backend_draw;
    g_vulkan_backend.cleanup = vk_backend_cleanup;
    return &g_vulkan_backend;
}