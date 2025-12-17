#define _POSIX_C_SOURCE 200809L
#include "engine/render/backend/vulkan/vulkan_renderer.h"

#include "engine/render/backend/vulkan/vk_types.h"
#include "engine/render/backend/vulkan/vk_context.h"
#include "engine/render/backend/vulkan/vk_swapchain.h"
#include "engine/render/backend/vulkan/vk_pipeline.h"
#include "engine/render/backend/vulkan/vk_resources.h"
#include "engine/render/backend/vulkan/vk_utils.h"
#include "engine/render/backend/vulkan/vk_compute.h"
#include "engine/text/font.h"

#include "foundation/logger/logger.h"

#include "foundation/math/coordinate_systems.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static RendererBackend g_vulkan_backend;

// --- Compute ---

static float vk_backend_run_compute(RendererBackend* backend, const char* glsl_source) {
    if (!backend || !backend->state || !glsl_source) return 0.0f;
    VulkanRendererState* state = (VulkanRendererState*)backend->state;
    return vk_run_compute_graph_oneshot(state, glsl_source);
}

static void vk_backend_run_compute_image(RendererBackend* backend, const char* glsl_source, int width, int height) {
    if (!backend || !backend->state || !glsl_source) return;
    VulkanRendererState* state = (VulkanRendererState*)backend->state;
    vk_run_compute_graph_image(state, glsl_source, width, height);
}

// Unified Push Constants layout (Global per pass)
typedef struct {
    float view_proj[16];
} UnifiedPushConstants;

// Per-Instance Data (Must match shader struct GpuInstanceData std140)
// Aligned to 128 bytes for safety and cache line friendliness
typedef struct {
    float model[16]; // 64 bytes
    float color[4];  // 16 bytes
    float uv_rect[4]; // 16 bytes
    float params[4]; // 16 bytes
    float extra[4];  // 16 bytes -> Total 128 bytes
} GpuInstanceData;

// --- Helpers ---

static bool vk_create_and_upload_buffer(VulkanRendererState* state, VkBufferUsageFlags usage, const void* data, size_t size, VkBuffer* out_buf, VkDeviceMemory* out_mem) {
    // 1. Staging Buffer
    VkBuffer staging_buf;
    VkDeviceMemory staging_mem;
    
    vk_create_buffer(state, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
        &staging_buf, &staging_mem);
        
    void* mapped;
    vkMapMemory(state->device, staging_mem, 0, size, 0, &mapped);
    memcpy(mapped, data, size);
    vkUnmapMemory(state->device, staging_mem);
    
    // 2. Device Local Buffer
    vk_create_buffer(state, size, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
        out_buf, out_mem);
        
    // 3. Copy
    VkCommandBufferAllocateInfo ai = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = state->cmdpool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1 };
    VkCommandBuffer cb;
    vkAllocateCommandBuffers(state->device, &ai, &cb);
    VkCommandBufferBeginInfo bi = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
    vkBeginCommandBuffer(cb, &bi);
    
    VkBufferCopy copy = { .srcOffset = 0, .dstOffset = 0, .size = size };
    vkCmdCopyBuffer(cb, staging_buf, *out_buf, 1, &copy);
    
    vkEndCommandBuffer(cb);
    VkSubmitInfo si = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cb };
    vkQueueSubmit(state->queue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(state->queue);
    vkFreeCommandBuffers(state->device, state->cmdpool, 1, &cb);
    
    // Cleanup staging
    vkDestroyBuffer(state->device, staging_buf, NULL);
    vkFreeMemory(state->device, staging_mem, NULL);
    
    return true;
}

static void update_instance_descriptor(VulkanRendererState* state, VkBuffer buffer) {
    if (buffer == VK_NULL_HANDLE) return;
    VkDescriptorBufferInfo dbi = { .buffer = buffer, .offset = 0, .range = VK_WHOLE_SIZE };
    VkWriteDescriptorSet w = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = state->instance_set, .dstBinding = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &dbi };
    vkUpdateDescriptorSets(state->device, 1, &w, 0, NULL);
}

static void ensure_instance_buffer(VulkanRendererState* state, size_t count) {
    if (count == 0) return;
    if (state->instance_capacity >= count) return;
    
    // Reallocate
    if (state->instance_buffer) {
        vkDestroyBuffer(state->device, state->instance_buffer, NULL);
        vkFreeMemory(state->device, state->instance_memory, NULL);
    }
    
    state->instance_capacity = (count < 1024) ? 1024 : count * 2;
    size_t size = state->instance_capacity * sizeof(GpuInstanceData);
    
    LOG_INFO("Allocating Instance Buffer: %zu elements (%zu bytes)", state->instance_capacity, size);
    
    VkBufferCreateInfo bci = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = size, .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, .sharingMode = VK_SHARING_MODE_EXCLUSIVE };
    vkCreateBuffer(state->device, &bci, NULL, &state->instance_buffer);
    
    VkMemoryRequirements mr; vkGetBufferMemoryRequirements(state->device, state->instance_buffer, &mr);
    VkMemoryAllocateInfo mai = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = mr.size, .memoryTypeIndex = find_mem_type(state->physical_device, mr.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) };
    vkAllocateMemory(state->device, &mai, NULL, &state->instance_memory);
    
    vkBindBufferMemory(state->device, state->instance_buffer, state->instance_memory, 0);
    vkMapMemory(state->device, state->instance_memory, 0, VK_WHOLE_SIZE, 0, &state->instance_mapped);
    
    // Note: Descriptor update happens in draw loop now
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
    
    // Reset instance buffer state
    state->instance_buffer = VK_NULL_HANDLE;
    state->instance_memory = VK_NULL_HANDLE;
    state->instance_capacity = 0;
    
    return true;
}

static void draw_frame_scene(VulkanRendererState* state, const Scene* scene) {
    static double last_log_time = -1.0; 
    bool debug_frame = false;

    double current_time = platform_get_time_ms();
    if (last_log_time < 0 || (current_time - last_log_time) / 1000.0 >= logger_get_trace_interval()) {
        debug_frame = true;
        last_log_time = current_time;
    }

    if (state->swapchain == VK_NULL_HANDLE) return;
    
    // 1. Acquire Image
    uint32_t img_idx;
    VkResult acq = vkAcquireNextImageKHR(state->device, state->swapchain, UINT64_MAX, state->sem_img_avail, VK_NULL_HANDLE, &img_idx);
    if (acq == VK_ERROR_DEVICE_LOST) { if (!recover_device_loss(state)) fatal_vk("vkAcquireNextImageKHR", acq); return; }
    if (acq != VK_SUCCESS && acq != VK_SUBOPTIMAL_KHR) { fatal_vk("vkAcquireNextImageKHR", acq); return; }
    
    vkWaitForFences(state->device, 1, &state->fences[img_idx], VK_TRUE, UINT64_MAX);
    vkResetFences(state->device, 1, &state->fences[img_idx]);

    // --- PREPARE DATA ---
    size_t single_count = 0;
    if (scene) {
        for (size_t i = 0; i < scene->object_count; ++i) {
            if (scene->objects[i].instance_count == 0) single_count++;
        }
    }

    if (single_count > 0) {
        ensure_instance_buffer(state, single_count);
        GpuInstanceData* data = (GpuInstanceData*)state->instance_mapped;
        size_t idx = 0;
        
        for (size_t i = 0; i < scene->object_count; ++i) {
            SceneObject* obj = &scene->objects[i];
            if (obj->instance_count > 0) continue; // Skip massive objects

            Mat4 T = mat4_translation(obj->position);
            Mat4 S = mat4_scale(obj->scale);
            Mat4 model = mat4_multiply(&T, &S);
            
            memcpy(data[idx].model, model.m, sizeof(float)*16);
            
            data[idx].color[0] = obj->color.x;
            data[idx].color[1] = obj->color.y;
            data[idx].color[2] = obj->color.z;
            data[idx].color[3] = obj->color.w;
            
            // Handle Primitives
            float prim_type = (float)obj->prim_type; // 0=Quad, 1=Curve
            
            if (obj->prim_type == SCENE_PRIM_CURVE) {
                // For curves, uv_rect in SceneObject holds P0 and P3
                data[idx].extra[0] = obj->uv_rect.x; // P0.x
                data[idx].extra[1] = obj->uv_rect.y; // P0.y
                data[idx].extra[2] = obj->uv_rect.z; // P3.x
                data[idx].extra[3] = obj->uv_rect.w; // P3.y
                
                // Reset UV for the quad itself (cover full area)
                data[idx].uv_rect[0] = 0.0f; data[idx].uv_rect[1] = 0.0f;
                data[idx].uv_rect[2] = 1.0f; data[idx].uv_rect[3] = 1.0f;
            } else {
                // Standard Quad
                data[idx].extra[0] = 0; data[idx].extra[1] = 0;
                data[idx].extra[2] = 0; data[idx].extra[3] = 0;

                if (obj->uv_rect.z == 0.0f && obj->uv_rect.w == 0.0f) {
                     data[idx].uv_rect[0] = 0.0f; data[idx].uv_rect[1] = 0.0f;
                     data[idx].uv_rect[2] = 1.0f; data[idx].uv_rect[3] = 1.0f;
                } else {
                     data[idx].uv_rect[0] = obj->uv_rect.x; data[idx].uv_rect[1] = obj->uv_rect.y;
                     data[idx].uv_rect[2] = obj->uv_rect.z; data[idx].uv_rect[3] = obj->uv_rect.w;
                }
            }
            
            data[idx].params[0] = obj->params.x; // use_texture
            data[idx].params[1] = prim_type;     // prim_type
            data[idx].params[2] = obj->params.z; // thickness (if curve)
            data[idx].params[3] = 0;
            
            if (debug_frame && idx < 2) {
                LOG_TRACE("[Frame %llu] Single[%zu]: Type=%.0f Pos(%.2f, %.2f) UV(%.2f,%.2f,%.2f,%.2f)",
                        (unsigned long long)scene->frame_number, idx, prim_type,
                        obj->position.x, obj->position.y,
                        obj->uv_rect.x, obj->uv_rect.y, obj->uv_rect.z, obj->uv_rect.w);
            }
            idx++;
        }
    }

    VkCommandBuffer cb = state->cmdbuffers[img_idx];
    vkResetCommandBuffer(cb, 0);
    VkCommandBufferBeginInfo binfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    vkBeginCommandBuffer(cb, &binfo);

    // 2. Begin Render Pass
    VkClearValue clr[2];
    clr[0].color = (VkClearColorValue){{0.1f, 0.1f, 0.1f, 1.0f}};
    clr[1].depthStencil = (VkClearDepthStencilValue){1.0f, 0};
    
    VkRenderPassBeginInfo rpbi = { 
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, 
        .renderPass = state->render_pass, 
        .framebuffer = state->framebuffers[img_idx], 
        .renderArea = {.offset = {0,0}, .extent = state->swapchain_extent }, 
        .clearValueCount = 2, 
        .pClearValues = clr 
    };
    vkCmdBeginRenderPass(cb, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

    // 3. Bind Pipeline & Resources
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, state->pipeline);
    
    // ViewProj Push Constants
    float w = (float)state->swapchain_extent.width;
    float h = (float)state->swapchain_extent.height;

    // Custom Vulkan Ortho Projection
    // Maps:
    // X: [0, w] -> [-1, 1]
    // Y: [0, h] -> [-1, 1] (Top to Bottom, Y-Down in World)
    // Z: [-1, 1] -> [0, 1] (Safe Z range)
    Mat4 view_proj = mat4_identity();
    if (w > 0 && h > 0) {
        view_proj.m[0] = 2.0f / w;
        view_proj.m[5] = 2.0f / h;
        view_proj.m[10] = 0.5f; // Compress Z range 2 (-1..1) to 1 (0..1)
        view_proj.m[12] = -1.0f;
        view_proj.m[13] = -1.0f;
        view_proj.m[14] = 0.5f; // Center Z=0 at 0.5
    }
    
    UnifiedPushConstants pc;
    memcpy(pc.view_proj, view_proj.m, sizeof(float)*16);
    vkCmdPushConstants(cb, state->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(UnifiedPushConstants), &pc);

    if (state->descriptor_set) {
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, state->pipeline_layout, 0, 1, &state->descriptor_set, 0, NULL);
    }
    
    if (state->compute_target_descriptor) {
         vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, state->pipeline_layout, 2, 1, &state->compute_target_descriptor, 0, NULL);
    }

    // DRAW SINGLES
    if (single_count > 0 && state->unit_quad_buffer) {
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cb, 0, 1, &state->unit_quad_buffer, &offset);

        // Bind Global Instance Buffer
        update_instance_descriptor(state, state->instance_buffer);
        if (state->instance_set) {
            vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, state->pipeline_layout, 1, 1, &state->instance_set, 0, NULL);
        }

        if (debug_frame) {
            LOG_TRACE("[Frame %llu] Draw Singles: Count=%zu", (unsigned long long)scene->frame_number, single_count);
        }
        vkCmdDraw(cb, 6, single_count, 0, 0); 
    }

    // DRAW MASSIVE
    if (scene) {
        for (size_t i = 0; i < scene->object_count; ++i) {
            SceneObject* obj = &scene->objects[i];
            if (obj->instance_count > 0 && obj->instance_buffer) {
                // Bind specific buffer
                update_instance_descriptor(state, (VkBuffer)(uintptr_t)obj->instance_buffer);
                vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, state->pipeline_layout, 1, 1, &state->instance_set, 0, NULL);
                
                if (debug_frame) {
                    LOG_TRACE("[Frame %llu] Draw Massive: ObjID=%d Count=%zu", (unsigned long long)scene->frame_number, obj->id, obj->instance_count);
                }
                vkCmdDraw(cb, 6, obj->instance_count, 0, 0);
            }
        }
    }

    vkCmdEndRenderPass(cb);
    vkEndCommandBuffer(cb);

    // 4. Submit & Present
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSubmitInfo si = { 
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, 
        .waitSemaphoreCount = 1, 
        .pWaitSemaphores = &state->sem_img_avail, 
        .pWaitDstStageMask = waitStages, 
        .commandBufferCount = 1, 
        .pCommandBuffers = &cb, 
        .signalSemaphoreCount = 1, 
        .pSignalSemaphores = &state->sem_render_done 
    };

    VkResult res = vkQueueSubmit(state->queue, 1, &si, state->fences[img_idx]);
    if (res != VK_SUCCESS) {
        LOG_ERROR("vkQueueSubmit failed: %d", res);
    }

    VkPresentInfoKHR pi = { 
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, 
        .waitSemaphoreCount = 1, 
        .pWaitSemaphores = &state->sem_render_done, 
        .swapchainCount = 1, 
        .pSwapchains = &state->swapchain, 
        .pImageIndices = &img_idx 
    };

    vkQueuePresentKHR(state->queue, &pi);
}

static void vk_backend_render_scene(RendererBackend* backend, const Scene* scene) {
    if (!backend || !backend->state) return;
    VulkanRendererState* state = (VulkanRendererState*)backend->state;
    draw_frame_scene(state, scene);
}

static void vk_backend_cleanup(RendererBackend* backend) {
    if (!backend || !backend->state) return;
    VulkanRendererState* state = (VulkanRendererState*)backend->state;
    if (state->device) vkDeviceWaitIdle(state->device);
    
    if (state->unit_quad_buffer) vkDestroyBuffer(state->device, state->unit_quad_buffer, NULL);
    if (state->unit_quad_memory) vkFreeMemory(state->device, state->unit_quad_memory, NULL);
    
    if (state->instance_buffer) vkDestroyBuffer(state->device, state->instance_buffer, NULL);
    if (state->instance_memory) vkFreeMemory(state->device, state->instance_memory, NULL);
    
    vk_destroy_device_resources(state);
    if (state->device) vkDestroyDevice(state->device, NULL);
    if (state->platform_surface) state->destroy_surface(state->instance, NULL, state->platform_surface);
    if (state->instance) vkDestroyInstance(state->instance, NULL);
    
    font_cleanup(); // Clean up font module
    
    render_logger_cleanup(state->logger);
    free(state);
    backend->state = NULL;
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
    state->vert_spv = init->vert_spv;
    state->frag_spv = init->frag_spv;
    state->font_path = init->font_path;

    // Initialize Font Module
    if (!font_init(state->font_path)) {
        LOG_ERROR("Failed to initialize font module with path: %s", state->font_path);
        // Continue but with broken text? or fail? 
        // For now, continue to see if at least rendering works.
    }

    vk_create_instance(state);
    state->create_surface(state->window, state->instance, NULL, state->platform_surface);
    state->surface = (VkSurfaceKHR)(state->platform_surface ? state->platform_surface->handle : NULL);
    vk_pick_physical_and_create_device(state);
    vk_create_swapchain_and_views(state, VK_NULL_HANDLE);
    vk_create_depth_resources(state);
    vk_create_render_pass(state);
    vk_create_descriptor_layout(state);
    vk_create_pipeline(state, state->vert_spv, state->frag_spv);
    vk_create_cmds_and_sync(state);
    
    // Upload font atlas (now via FontService)
    vk_create_font_texture(state);
    
    vk_create_descriptor_pool_and_set(state);

    // Create Unit Quad Buffer (6 vertices for list)
    // Vertices: [x, y, z, u, v] (stride 20)
    // 0,0 - 1,0 - 1,1
    // 0,0 - 1,1 - 0,1
    float quad_verts[] = {
        // Pos(3)       // UV(2)
        0.0f, 0.0f, 0.0f,  0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,  1.0f, 0.0f,
        1.0f, 1.0f, 0.0f,  1.0f, 1.0f,
        
        0.0f, 0.0f, 0.0f,  0.0f, 0.0f,
        1.0f, 1.0f, 0.0f,  1.0f, 1.0f,
        0.0f, 1.0f, 0.0f,  0.0f, 1.0f
    };
    vk_create_and_upload_buffer(state, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, quad_verts, sizeof(quad_verts), &state->unit_quad_buffer, &state->unit_quad_memory);
    
    LOG_INFO("Vulkan Initialized. Unit Quad Buffer: %p", (void*)state->unit_quad_buffer);

    return true;
}

RendererBackend* vulkan_renderer_backend(void) {
    g_vulkan_backend.id = "vulkan";
    g_vulkan_backend.state = NULL;
    g_vulkan_backend.init = vk_backend_init;
    g_vulkan_backend.render_scene = vk_backend_render_scene;
    g_vulkan_backend.cleanup = vk_backend_cleanup;
    g_vulkan_backend.run_compute = vk_backend_run_compute;
    g_vulkan_backend.run_compute_image = vk_backend_run_compute_image;
    return &g_vulkan_backend;
}