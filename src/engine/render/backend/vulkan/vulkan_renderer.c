#define _POSIX_C_SOURCE 200809L
#include "engine/render/backend/vulkan/vulkan_renderer.h"

#include "engine/render/backend/vulkan/vk_types.h"
#include "engine/render/backend/vulkan/vk_context.h"
#include "engine/render/backend/vulkan/vk_swapchain.h"
#include "engine/render/backend/vulkan/vk_pipeline.h"
#include "engine/render/backend/vulkan/vk_resources.h"
#include "engine/render/backend/vulkan/vk_utils.h"

#include "foundation/logger/logger.h"

#include "foundation/math/coordinate_systems.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static RendererBackend g_vulkan_backend;

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
    float pad[4];    // 16 bytes -> Total 128 bytes
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
    
    // Update Descriptor Set
    VkDescriptorBufferInfo dbi = { .buffer = state->instance_buffer, .offset = 0, .range = VK_WHOLE_SIZE };
    VkWriteDescriptorSet w = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = state->instance_set, .dstBinding = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &dbi };
    vkUpdateDescriptorSets(state->device, 1, &w, 0, NULL);
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
    static double last_log_time = -1.0; // Use -1.0 to ensure initial log
    bool debug_frame = false;

    double current_time = platform_get_time_ms();
    if (last_log_time < 0 || (current_time - last_log_time) / 1000.0 >= 5.0) {
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

    // PREPARE INSTANCE DATA (CPU -> GPU)
    if (scene && scene->object_count > 0) {
        ensure_instance_buffer(state, scene->object_count);
        GpuInstanceData* data = (GpuInstanceData*)state->instance_mapped;
        
        for (size_t i = 0; i < scene->object_count; ++i) {
            SceneObject* obj = &scene->objects[i];
            Mat4 T = mat4_translation(obj->position);
            Mat4 S = mat4_scale(obj->scale);
            Mat4 model = mat4_multiply(&T, &S);
            
            memcpy(data[i].model, model.m, sizeof(float)*16);
            
            data[i].color[0] = obj->color.x;
            data[i].color[1] = obj->color.y;
            data[i].color[2] = obj->color.z;
            data[i].color[3] = obj->color.w;
            
            if (obj->uv_rect.z == 0.0f && obj->uv_rect.w == 0.0f) {
                 data[i].uv_rect[0] = 0.0f; data[i].uv_rect[1] = 0.0f;
                 data[i].uv_rect[2] = 1.0f; data[i].uv_rect[3] = 1.0f;
            } else {
                 data[i].uv_rect[0] = obj->uv_rect.x; data[i].uv_rect[1] = obj->uv_rect.y;
                 data[i].uv_rect[2] = obj->uv_rect.z; data[i].uv_rect[3] = obj->uv_rect.w;
            }
            
            data[i].params[0] = obj->params.x;
            data[i].params[1] = 0; data[i].params[2] = 0; data[i].params[3] = 0;
            
            // Log 1st object (Background) and 2nd object (First Letter)
            if (debug_frame) {
                if (i == 0) {
                    LOG_INFO("Obj[0] (Bg): Pos(%.2f, %.2f) Scale(%.2f, %.2f) Tex(%.1f)",
                        obj->position.x, obj->position.y, obj->scale.x, obj->scale.y, obj->params.x);
                } else if (i == 1) {
                    LOG_INFO("Obj[1] (Txt): Pos(%.2f, %.2f) Scale(%.2f, %.2f) Tex(%.1f) UV(%.2f,%.2f,%.2f,%.2f)",
                        obj->position.x, obj->position.y, obj->scale.x, obj->scale.y, obj->params.x,
                        obj->uv_rect.x, obj->uv_rect.y, obj->uv_rect.z, obj->uv_rect.w);
                }
            }
        }
    } else if (debug_frame) {
        LOG_INFO("Scene is empty or NULL");
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
    
    if (state->descriptor_set) {
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, state->pipeline_layout, 0, 1, &state->descriptor_set, 0, NULL);
    }
    if (state->instance_set && scene && scene->object_count > 0) {
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, state->pipeline_layout, 1, 1, &state->instance_set, 0, NULL);
    }
    
    // Push Constants (Global ViewProj)
    float w = (float)state->swapchain_extent.width;
    float h = (float)state->swapchain_extent.height;
    // Fix Z-range: Map [-1, 1] world Z to [0, 1] Vulkan Z.
    // near=-1.0, far=1.0. 
    // Z=0.0 maps to 0.5 (Mid). Z=0.1 maps to 0.55.
    // Objects must be in [-1, 1].
    // UPDATE: We use near=1.0, far=-1.0 so that Z_ndc = Z.
    // This maps input Z=[0, 1] to NDC=[0, 1], which matches Vulkan clip volume.
    Mat4 proj = mat4_orthographic(0, w, 0, h, 1.0f, -1.0f);
    Mat4 view = mat4_identity(); 
    Mat4 view_proj = mat4_multiply(&proj, &view);
    
    if (debug_frame) {
        LOG_INFO("ViewProj: W=%.1f H=%.1f Proj[0]=%.4f Proj[5]=%.4f Proj[10]=%.4f Proj[12]=%.4f Proj[13]=%.4f Proj[14]=%.4f", 
            w, h, proj.m[0], proj.m[5], proj.m[10], proj.m[12], proj.m[13], proj.m[14]);
    }
    
    UnifiedPushConstants pc;
    memcpy(pc.view_proj, view_proj.m, sizeof(float)*16);
    vkCmdPushConstants(cb, state->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(UnifiedPushConstants), &pc);

    // Bind Vertex Buffer (Unit Quad)
    // Removed debug log here to reduce spam
    if (state->unit_quad_buffer && scene && scene->object_count > 0) {
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cb, 0, 1, &state->unit_quad_buffer, &offset);
        
        // INSTANCED DRAW CALL
        if (debug_frame) {
            LOG_INFO("Issuing DrawInstanced: VertexCount=6, InstanceCount=%zu", scene->object_count);
        }
        vkCmdDraw(cb, 6, scene->object_count, 0, 0); 
    }

    vkCmdEndRenderPass(cb);
    vkEndCommandBuffer(cb);

    // 5. Submit
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .waitSemaphoreCount = 1, .pWaitSemaphores = &state->sem_img_avail, .pWaitDstStageMask = &waitStage, .commandBufferCount = 1, .pCommandBuffers = &cb, .signalSemaphoreCount = 1, .pSignalSemaphores = &state->sem_render_done };
    vkQueueSubmit(state->queue, 1, &si, state->fences[img_idx]);

    VkPresentInfoKHR pi = { .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, .waitSemaphoreCount = 1, .pWaitSemaphores = &state->sem_render_done, .swapchainCount = 1, .pSwapchains = &state->swapchain, .pImageIndices = &img_idx };
    vkQueuePresentKHR(state->queue, &pi);
}

static bool vk_backend_get_glyph(RendererBackend* backend, uint32_t codepoint, RenderGlyph* out_glyph) {
    if (!backend || !backend->state || !out_glyph) return false;
    VulkanRendererState* state = (VulkanRendererState*)backend->state;
    
    if (codepoint >= GLYPH_CAPACITY || !state->glyph_valid[codepoint]) return false;
    
    Glyph* g = &state->glyphs[codepoint];
    out_glyph->u0 = g->u0;
    out_glyph->v0 = g->v0;
    out_glyph->u1 = g->u1;
    out_glyph->v1 = g->v1;
    out_glyph->w = g->w;
    out_glyph->h = g->h;
    out_glyph->xoff = g->xoff;
    out_glyph->yoff = g->yoff;
    out_glyph->advance = g->advance;
    
    return true;
}

static void vk_backend_render_scene(RendererBackend* backend, const Scene* scene) {
    if (!backend || !backend->state) return;
    VulkanRendererState* state = (VulkanRendererState*)backend->state;
    draw_frame_scene(state, scene);
}

static void vk_backend_draw(RendererBackend* backend) {
    // Legacy draw, empty scene
    vk_backend_render_scene(backend, NULL);
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
    vk_build_font_atlas(state);
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
    g_vulkan_backend.get_glyph = vk_backend_get_glyph;
    g_vulkan_backend.draw = vk_backend_draw;
    g_vulkan_backend.cleanup = vk_backend_cleanup;
    return &g_vulkan_backend;
}