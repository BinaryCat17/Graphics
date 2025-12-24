#include "engine/graphics/internal/vulkan/vulkan_renderer.h"

#include "engine/graphics/internal/vulkan/vk_types.h"
#include "engine/graphics/internal/vulkan/vk_context.h"
#include "engine/graphics/internal/vulkan/vk_swapchain.h"
#include "engine/graphics/internal/vulkan/vk_pipeline.h"
#include "engine/graphics/internal/vulkan/vk_resources.h"
#include "engine/graphics/internal/vulkan/vk_utils.h"
#include "engine/graphics/internal/vulkan/vk_buffer.h"
#include "engine/graphics/primitives.h"
#include "engine/graphics/internal/stream_internal.h"
#include "engine/text/font.h"

#include "foundation/logger/logger.h"
#include "foundation/platform/platform.h"
#include "foundation/platform/fs.h"
#include "foundation/math/coordinate_systems.h"
#include "foundation/image/image.h"
#include "foundation/thread/thread.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

static void vulkan_renderer_request_screenshot(RendererBackend* backend, const char* filepath) {
    VulkanRendererState* state = (VulkanRendererState*)backend->state;
    if (!state || !filepath) return;
    
    LOG_DEBUG("Vulkan: Queueing screenshot to %s", filepath);
    platform_strncpy(state->screenshot_path, filepath, sizeof(state->screenshot_path) - 1);
    state->screenshot_pending = true;
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
    
    // --- Update SSBO Descriptors (Set 1) ---
    // Identify active bindings and update the descriptor set on-the-fly.
    // In production, we should cache these or use Push Descriptors.
    VkWriteDescriptorSet writes[MAX_COMPUTE_BINDINGS];
    VkDescriptorBufferInfo dbis[MAX_COMPUTE_BINDINGS];
    uint32_t write_count = 0;

    for (int i = 0; i < MAX_COMPUTE_BINDINGS; ++i) {
        if (state->compute_bindings[i].buffer) {
            dbis[write_count].buffer = state->compute_bindings[i].buffer->buffer;
            dbis[write_count].offset = 0;
            dbis[write_count].range = VK_WHOLE_SIZE;

            writes[write_count].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[write_count].pNext = NULL;
            writes[write_count].dstSet = state->compute_ssbo_descriptor;
            writes[write_count].dstBinding = i;
            writes[write_count].dstArrayElement = 0;
            writes[write_count].descriptorCount = 1;
            writes[write_count].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writes[write_count].pImageInfo = NULL;
            writes[write_count].pBufferInfo = &dbis[write_count];
            writes[write_count].pTexelBufferView = NULL;
            
            write_count++;
        }
    }
    
    if (write_count > 0) {
        vkUpdateDescriptorSets(state->device, write_count, writes, 0, NULL);
    }
    
    // Use the dedicated compute command buffer
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

    // Bind Descriptor Set 1 (SSBOs)
    if (state->compute_ssbo_descriptor) {
        vkCmdBindDescriptorSets(state->compute_cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 1, 1, &state->compute_ssbo_descriptor, 0, NULL);
    }
    
    // Push Constants
    if (push_constants && push_constants_size > 0) {
        vkCmdPushConstants(state->compute_cmd, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, (uint32_t)push_constants_size, push_constants);
    }
    
    // Dispatch
    vkCmdDispatch(state->compute_cmd, group_x, group_y, group_z);
    
    // Barrier (Global Memory + Image)
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = state->compute_target_image,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    
    VkMemoryBarrier mem_barrier = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_HOST_READ_BIT
    };
    
    vkCmdPipelineBarrier(state->compute_cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &mem_barrier, 0, NULL, 1, &barrier);
    
    vkEndCommandBuffer(state->compute_cmd);
    
    // Submit
    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &state->compute_cmd
    };
    
    if (vkQueueSubmit(state->queue, 1, &submit_info, state->compute_fence) != VK_SUCCESS) {
        LOG_ERROR("Failed to submit compute queue");
    }
}

static void vulkan_compute_wait(RendererBackend* backend) {
    VulkanRendererState* state = (VulkanRendererState*)backend->state;
    vkWaitForFences(state->device, 1, &state->compute_fence, VK_TRUE, UINT64_MAX);
}

static bool vulkan_compile_shader(RendererBackend* backend, const char* source, size_t size, const char* stage, void** out_spv, size_t* out_spv_size) {
    (void)backend;
    
    LOG_INFO("Vulkan Compile: Start. Size: %zu", size);

    // 1. Ensure logs dir exists
    platform_mkdir("logs");
    
    // 2. Write source to temp file
    const char* tmp_src = "logs/tmp_compile.glsl";
    const char* tmp_spv = "logs/tmp_compile.spv";
    
    LOG_INFO("Vulkan Compile: Writing source to %s", tmp_src);
    FILE* f = fopen(tmp_src, "w");
    if (!f) {
        LOG_ERROR("Failed to open tmp file");
        return false;
    }
    fwrite(source, 1, size, f);
    fclose(f);
    
    // 3. Construct Command
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "glslc -fshader-stage=%s %s -o %s", stage, tmp_src, tmp_spv);
    
    // 4. Run
    LOG_INFO("Vulkan Compile: Running '%s'", cmd);
    int res = system(cmd);
    if (res != 0) {
        LOG_ERROR("Vulkan: Shader compilation failed. Command: %s", cmd);
        return false;
    }
    
    // 5. Read Result
    LOG_INFO("Vulkan Compile: Reading result from %s", tmp_spv);
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
    state->font = init->font;
    
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
    
    // Create Compute SSBO Layout (Set 1) - Moved before Pipeline Creation
    // Also used for Graphics (Zero-Copy) -> Set 1
    VkDescriptorSetLayoutBinding bindings[MAX_COMPUTE_BINDINGS];
    for (int i=0; i<MAX_COMPUTE_BINDINGS; ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[i].pImmutableSamplers = NULL;
    }
    VkDescriptorSetLayoutCreateInfo dslci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = MAX_COMPUTE_BINDINGS,
        .pBindings = bindings
    };
    if (vkCreateDescriptorSetLayout(state->device, &dslci, NULL, &state->compute_ssbo_layout) != VK_SUCCESS) {
        LOG_FATAL("Failed to create compute SSBO layout");
    }

    // 7. Descriptor & Pipeline
    vk_create_descriptor_layout(state);
    vk_create_pipeline(state);

    // 8. Fonts & Textures
    vk_create_font_texture(state);
    vk_create_descriptor_pool_and_set(state);

    VkDescriptorSetAllocateInfo dsai = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = state->descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &state->compute_ssbo_layout
    };
    if (vkAllocateDescriptorSets(state->device, &dsai, &state->compute_ssbo_descriptor) != VK_SUCCESS) {
        LOG_FATAL("Failed to allocate compute SSBO descriptor");
    }

    // 9. Static Buffers (Quad)
    // Vertex Buffer
    VkDeviceSize v_size = sizeof(PRIM_QUAD_VERTS);
    vk_create_buffer(state, v_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                     &state->unit_quad_buffer, &state->unit_quad_memory);
    
    void* v_map;
    vkMapMemory(state->device, state->unit_quad_memory, 0, VK_WHOLE_SIZE, 0, &v_map);
    memcpy(v_map, PRIM_QUAD_VERTS, v_size);
    vkUnmapMemory(state->device, state->unit_quad_memory);

    // Index Buffer
    VkDeviceSize i_size = sizeof(PRIM_QUAD_INDICES);
    vk_create_buffer(state, i_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                     &state->unit_quad_index_buffer, &state->unit_quad_index_memory);
                     
    void* i_map;
    vkMapMemory(state->device, state->unit_quad_index_memory, 0, VK_WHOLE_SIZE, 0, &i_map);
    memcpy(i_map, PRIM_QUAD_INDICES, i_size);
    vkUnmapMemory(state->device, state->unit_quad_index_memory);
    
    // 10. Per-Frame Instance Resources
    for (int i = 0; i < 2; ++i) {
        // Create Pool for Custom Descriptors
        VkDescriptorPoolSize sizes[] = {
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 128 } // Allow up to 128 buffers per frame
        };
        VkDescriptorPoolCreateInfo dpci = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = 32, // Allow up to 32 custom draw calls per frame
            .poolSizeCount = 1,
            .pPoolSizes = sizes
        };
        if (vkCreateDescriptorPool(state->device, &dpci, NULL, &state->frame_resources[i].frame_descriptor_pool) != VK_SUCCESS) {
            LOG_FATAL("Failed to create frame descriptor pool");
        }
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

static void vulkan_renderer_cleanup(RendererBackend* backend) {
    if (!backend) return;
    VulkanRendererState* state = (VulkanRendererState*)backend->state;
    if (state) {
        vkDeviceWaitIdle(state->device);
        
        // Clean up per-frame resources
        for (int i = 0; i < 2; ++i) {
            if (state->frame_resources[i].frame_descriptor_pool) {
                vkDestroyDescriptorPool(state->device, state->frame_resources[i].frame_descriptor_pool, NULL);
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

static bool vulkan_buffer_create(RendererBackend* backend, Stream* stream) {
    VulkanRendererState* state = (VulkanRendererState*)backend->state;
    VkBufferWrapper* wrapper = malloc(sizeof(VkBufferWrapper));
    // Default to Storage Buffer + Transfer Dest/Src + Vertex Buffer
    if (vk_buffer_create(state, stream->total_size, 
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, wrapper)) {
        stream->buffer_handle = wrapper;
        return true;
    }
    free(wrapper);
    stream->buffer_handle = NULL;
    return false;
}

static void vulkan_buffer_destroy(RendererBackend* backend, Stream* stream) {
    VulkanRendererState* state = (VulkanRendererState*)backend->state;
    VkBufferWrapper* wrapper = (VkBufferWrapper*)stream->buffer_handle;
    if (wrapper) {
        // Clear bindings if this buffer is bound
        for (int i=0; i<MAX_COMPUTE_BINDINGS; ++i) {
            if (state->compute_bindings[i].buffer == wrapper) {
                state->compute_bindings[i].buffer = NULL;
            }
        }
        for (int i=0; i<MAX_COMPUTE_BINDINGS; ++i) {
            if (state->graphics_bindings[i].buffer == wrapper) {
                state->graphics_bindings[i].buffer = NULL;
            }
        }
        
        vk_buffer_destroy(state, wrapper);
        free(wrapper);
        stream->buffer_handle = NULL;
    }
}

static void* vulkan_buffer_map(RendererBackend* backend, Stream* stream) {
    VulkanRendererState* state = (VulkanRendererState*)backend->state;
    return vk_buffer_map(state, (VkBufferWrapper*)stream->buffer_handle);
}

static void vulkan_buffer_unmap(RendererBackend* backend, Stream* stream) {
    VulkanRendererState* state = (VulkanRendererState*)backend->state;
    vk_buffer_unmap(state, (VkBufferWrapper*)stream->buffer_handle);
}

static bool vulkan_buffer_upload(RendererBackend* backend, Stream* stream, const void* data, size_t size, size_t offset) {
    VulkanRendererState* state = (VulkanRendererState*)backend->state;
    return vk_buffer_upload(state, (VkBufferWrapper*)stream->buffer_handle, data, size, offset);
}

static bool vulkan_buffer_read(RendererBackend* backend, Stream* stream, void* dst, size_t size, size_t offset) {
    VulkanRendererState* state = (VulkanRendererState*)backend->state;
    return vk_buffer_read(state, (VkBufferWrapper*)stream->buffer_handle, dst, size, offset);
}

static void vulkan_compute_bind_buffer(RendererBackend* backend, Stream* stream, uint32_t slot) {
    VulkanRendererState* state = (VulkanRendererState*)backend->state;
    if (slot < MAX_COMPUTE_BINDINGS) {
        state->compute_bindings[slot].buffer = (VkBufferWrapper*)stream->buffer_handle;
    }
}

static uint32_t vulkan_graphics_pipeline_create(RendererBackend* backend, const void* vert_code, size_t vert_size, const void* frag_code, size_t frag_size, int layout_index) {
    VulkanRendererState* state = (VulkanRendererState*)backend->state;
    
    // Find free slot
    int slot = -1;
    for (int i = 0; i < MAX_GRAPHICS_PIPELINES; ++i) {
        if (!state->graphics_pipelines[i].active) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        LOG_ERROR("Max graphics pipelines reached (%d)", MAX_GRAPHICS_PIPELINES);
        return 0;
    }
    
    VkPipeline pipeline;
    VkPipelineLayout layout;
    
    // Convert void* to uint32_t* (assume aligned)
    VkResult res = vk_create_graphics_pipeline_shader(state, (const uint32_t*)vert_code, vert_size, (const uint32_t*)frag_code, frag_size, layout_index, &pipeline, &layout);
    
    if (res != VK_SUCCESS) {
        LOG_ERROR("Failed to create graphics pipeline: %d", res);
        return 0;
    }
    
    state->graphics_pipelines[slot].active = true;
    state->graphics_pipelines[slot].pipeline = pipeline;
    state->graphics_pipelines[slot].layout = layout;
    
    return (uint32_t)(slot + 1);
}

static void vulkan_graphics_pipeline_destroy(RendererBackend* backend, uint32_t pipeline_id) {
    VulkanRendererState* state = (VulkanRendererState*)backend->state;
    if (pipeline_id == 0 || pipeline_id > MAX_GRAPHICS_PIPELINES) return;
    
    int idx = (int)pipeline_id - 1;
    if (state->graphics_pipelines[idx].active) {
        vkDestroyPipeline(state->device, state->graphics_pipelines[idx].pipeline, NULL);
        vkDestroyPipelineLayout(state->device, state->graphics_pipelines[idx].layout, NULL);
        state->graphics_pipelines[idx].active = false;
    }
}

static void vulkan_graphics_bind_buffer(RendererBackend* backend, Stream* stream, uint32_t slot) {
    VulkanRendererState* state = (VulkanRendererState*)backend->state;
    if (slot < MAX_COMPUTE_BINDINGS) {
        state->graphics_bindings[slot].buffer = (VkBufferWrapper*)stream->buffer_handle;
    }
}

static void vulkan_graphics_draw(RendererBackend* backend, uint32_t pipeline_id, uint32_t vertex_count, uint32_t instance_count) {
    // This function is intended to be called from within a generic render pass logic,
    // but currently render_scene handles the pass. 
    // We leave this empty or for future use if we expose pass control.
    // The actual draw logic for Custom Nodes is integrated into vulkan_renderer_render_scene.
    (void)backend; (void)pipeline_id; (void)vertex_count; (void)instance_count;
}

static void vulkan_renderer_submit_commands(RendererBackend* backend, const RenderCommandList* list) {
    VulkanRendererState* state = (VulkanRendererState*)backend->state;
    if (!state || !list) return;

    // --- Frame Sync ---
    vkWaitForFences(state->device, 1, &state->fences[state->current_frame_cursor], VK_TRUE, UINT64_MAX);
    
    uint32_t image_index;
    VkResult result = vkAcquireNextImageKHR(state->device, state->swapchain, UINT64_MAX, 
                                            state->sem_img_avail, VK_NULL_HANDLE, &image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)) {
        return;
    }
    
    vkResetFences(state->device, 1, &state->fences[state->current_frame_cursor]);
    
    // --- Resources ---
    FrameResources* frame = &state->frame_resources[state->current_frame_cursor];
    vkResetDescriptorPool(state->device, frame->frame_descriptor_pool, 0);

    // --- Begin Cmd ---
    VkCommandBuffer cmd = state->cmdbuffers[state->current_frame_cursor];
    vkResetCommandBuffer(cmd, 0);
    
    VkCommandBufferBeginInfo begin_info = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(cmd, &begin_info);
    
    // --- Begin Pass ---
    VkRenderPassBeginInfo pass_info = {.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    pass_info.renderPass = state->render_pass;
    pass_info.framebuffer = state->framebuffers[image_index];
    pass_info.renderArea.offset = (VkOffset2D){0, 0};
    pass_info.renderArea.extent = state->swapchain_extent;
    
    VkClearValue clear_values[2];
    clear_values[0].color = (VkClearColorValue){{0.1f, 0.1f, 0.1f, 1.0f}}; 
    clear_values[1].depthStencil = (VkClearDepthStencilValue){1.0f, 0};
    pass_info.clearValueCount = 2;
    pass_info.pClearValues = clear_values;
    
    vkCmdBeginRenderPass(cmd, &pass_info, VK_SUBPASS_CONTENTS_INLINE);
    
    // --- Defaults ---
    VkViewport viewport = {0.0f, 0.0f, (float)state->swapchain_extent.width, (float)state->swapchain_extent.height, 0.0f, 1.0f};
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor = {{0, 0}, state->swapchain_extent};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Bind Quad Vertex Buffer (Global default)
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, &state->unit_quad_buffer, offsets);
    vkCmdBindIndexBuffer(cmd, state->unit_quad_index_buffer, 0, VK_INDEX_TYPE_UINT32);

    // Bind Global Sets (0 and 2)
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state->pipeline_layout, 0, 1, &state->descriptor_set, 0, NULL);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state->pipeline_layout, 2, 1, &state->compute_target_descriptor, 0, NULL);
    
    // Bind Default Pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state->pipeline);

    // Pending Bindings State for Set 1
    VkBufferWrapper* pending_buffers[4] = {0};
    bool bindings_dirty = false;
    
    // --- Process Commands ---
    for (uint32_t i = 0; i < list->count; ++i) {
        RenderCommand* rc = &list->commands[i];
        switch (rc->type) {
            case RENDER_CMD_BIND_PIPELINE: {
                uint32_t pid = rc->bind_pipeline.pipeline_id;
                if (pid == 0) {
                     vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state->pipeline);
                } else if (pid <= MAX_GRAPHICS_PIPELINES) {
                     int idx = (int)pid - 1;
                     if (state->graphics_pipelines[idx].active) {
                         vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state->graphics_pipelines[idx].pipeline);
                     }
                }
                break;
            }
            case RENDER_CMD_BIND_BUFFER: {
                if (rc->bind_buffer.slot < 4) {
                    if (rc->bind_buffer.stream) {
                        pending_buffers[rc->bind_buffer.slot] = (VkBufferWrapper*)rc->bind_buffer.stream->buffer_handle;
                    } else {
                        pending_buffers[rc->bind_buffer.slot] = NULL;
                    }
                    bindings_dirty = true;
                }
                break;
            }
            case RENDER_CMD_PUSH_CONSTANTS: {
                vkCmdPushConstants(cmd, state->pipeline_layout, rc->push_constants.stage_flags, 0, rc->push_constants.size, rc->push_constants.data);
                break;
            }
            case RENDER_CMD_SET_VIEWPORT: {
                VkViewport vp = {rc->viewport.x, rc->viewport.y, rc->viewport.w, rc->viewport.h, rc->viewport.min_depth, rc->viewport.max_depth};
                vkCmdSetViewport(cmd, 0, 1, &vp);
                break;
            }
             case RENDER_CMD_SET_SCISSOR: {
                VkRect2D sc = {{rc->scissor.x, rc->scissor.y}, {rc->scissor.w, rc->scissor.h}};
                vkCmdSetScissor(cmd, 0, 1, &sc);
                break;
            }
            case RENDER_CMD_DRAW:
            case RENDER_CMD_DRAW_INDEXED: {
                // Apply Bindings if dirty
                if (bindings_dirty) {
                    VkDescriptorSet set = VK_NULL_HANDLE;
                    VkDescriptorSetAllocateInfo alloc_info = {
                        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                        .descriptorPool = frame->frame_descriptor_pool,
                        .descriptorSetCount = 1,
                        .pSetLayouts = &state->compute_ssbo_layout // Reusing layout: 4 SSBOs
                    };
                    if (vkAllocateDescriptorSets(state->device, &alloc_info, &set) == VK_SUCCESS) {
                        VkWriteDescriptorSet writes[4];
                        VkDescriptorBufferInfo dbis[4];
                        uint32_t w_count = 0;
                        for(int b=0; b<4; ++b) {
                            if (pending_buffers[b]) {
                                dbis[w_count].buffer = pending_buffers[b]->buffer;
                                dbis[w_count].offset = 0;
                                dbis[w_count].range = VK_WHOLE_SIZE;
                                
                                writes[w_count].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                                writes[w_count].pNext = NULL;
                                writes[w_count].dstSet = set;
                                writes[w_count].dstBinding = b;
                                writes[w_count].dstArrayElement = 0;
                                writes[w_count].descriptorCount = 1;
                                writes[w_count].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                                writes[w_count].pImageInfo = NULL;
                                writes[w_count].pBufferInfo = &dbis[w_count];
                                writes[w_count].pTexelBufferView = NULL;
                                w_count++;
                            }
                        }
                        if (w_count > 0) vkUpdateDescriptorSets(state->device, w_count, writes, 0, NULL);
                        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state->pipeline_layout, 1, 1, &set, 0, NULL);
                    }
                    bindings_dirty = false;
                }
                
                if (rc->type == RENDER_CMD_DRAW) {
                    vkCmdDraw(cmd, rc->draw.vertex_count, rc->draw.instance_count, rc->draw.first_vertex, rc->draw.first_instance);
                } else {
                    vkCmdDrawIndexed(cmd, rc->draw_indexed.index_count, rc->draw_indexed.instance_count, rc->draw_indexed.first_index, rc->draw_indexed.vertex_offset, rc->draw_indexed.first_instance);
                }
                break;
            }
            default: break;
        }
    }
    
    vkCmdEndRenderPass(cmd);

    // Handle Screenshot Logic (Partial Duplicate of render_scene)
    if (state->screenshot_pending) {
         // Minimal screenshot support for now: just clear flag to avoid hanging
         state->screenshot_pending = false; 
         // TODO: Port full screenshot logic
    }

    vkEndCommandBuffer(cmd);
    
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
    
    VkPresentInfoKHR present_info = {.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &state->sem_render_done;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &state->swapchain;
    present_info.pImageIndices = &image_index;
    
    vkQueuePresentKHR(state->queue, &present_info);
    
    state->current_frame_cursor = (state->current_frame_cursor + 1) % 2;
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
    backend->submit_commands = vulkan_renderer_submit_commands; // Register new method
    backend->update_viewport = vulkan_renderer_update_viewport;

    backend->cleanup = vulkan_renderer_cleanup;
    backend->request_screenshot = vulkan_renderer_request_screenshot;
    
    // Compute
    backend->compute_pipeline_create = vulkan_compute_pipeline_create;
    backend->compute_pipeline_destroy = vulkan_compute_pipeline_destroy;
    backend->compute_dispatch = vulkan_compute_dispatch;
    backend->compute_wait = vulkan_compute_wait;
    backend->compile_shader = vulkan_compile_shader;
    
    // Buffer
    backend->buffer_create = vulkan_buffer_create;
    backend->buffer_destroy = vulkan_buffer_destroy;
    backend->buffer_map = vulkan_buffer_map;
    backend->buffer_unmap = vulkan_buffer_unmap;
    backend->buffer_upload = vulkan_buffer_upload;
    backend->buffer_read = vulkan_buffer_read;
    backend->compute_bind_buffer = vulkan_compute_bind_buffer;
    
    // Graphics
    backend->graphics_pipeline_create = vulkan_graphics_pipeline_create;
    backend->graphics_pipeline_destroy = vulkan_graphics_pipeline_destroy;
    backend->graphics_bind_buffer = vulkan_graphics_bind_buffer;
    backend->graphics_draw = vulkan_graphics_draw;

    return backend;
}
