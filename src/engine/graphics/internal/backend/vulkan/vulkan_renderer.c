#include "engine/graphics/internal/backend/vulkan/vulkan_renderer.h"

#include "engine/graphics/internal/backend/vulkan/vk_types.h"
#include "engine/graphics/internal/backend/vulkan/vk_context.h"
#include "engine/graphics/internal/backend/vulkan/vk_swapchain.h"
#include "engine/graphics/internal/backend/vulkan/vk_pipeline.h"
#include "engine/graphics/internal/backend/vulkan/vk_resources.h"
#include "engine/graphics/internal/backend/vulkan/vk_utils.h"
#include "engine/graphics/internal/backend/vulkan/vk_buffer.h"
#include "engine/graphics/internal/primitives.h"
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

// --- Async Screenshot Worker ---

typedef struct ScreenshotTask {
    char path[256];
    int width;
    int height;
    void* data;
} ScreenshotTask;

static int screenshot_thread_func(void* arg) {
    ScreenshotTask* task = (ScreenshotTask*)arg;
    if (!task) return 1;

    // Swizzle BGRA -> RGBA (assuming standard swapchain format)
    image_swizzle_bgra_to_rgba((uint8_t*)task->data, task->width * task->height);
    
    if (image_write_png(task->path, task->width, task->height, 4, task->data, 0)) {
        LOG_INFO("Screenshot saved: %s", task->path);
    } else {
        LOG_ERROR("Failed to save screenshot: %s", task->path);
    }
    
    free(task->data);
    free(task);
    return 0;
}

static void vulkan_renderer_request_screenshot(RendererBackend* backend, const char* filepath) {
    VulkanRendererState* state = (VulkanRendererState*)backend->state;
    if (!state || !filepath) return;
    
    LOG_TRACE("Vulkan: Queueing screenshot to %s", filepath);
    platform_strncpy(state->screenshot_path, filepath, sizeof(state->screenshot_path) - 1);
    state->screenshot_pending = true;
}



// --- COMPUTE SUBSYSTEM ---

static uint32_t vulkan_compute_pipeline_create(RendererBackend* backend, const void* spirv_code, size_t size, const DescriptorLayoutDef* layouts, uint32_t layout_count) {
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
    VkDescriptorSetLayout set_layouts[4] = {0};
    
    // Convert void* to uint32_t* (assume 4-byte aligned and size is bytes)
    VkResult res = vk_create_compute_pipeline_shader(state, (const uint32_t*)spirv_code, size, layouts, layout_count, &pipeline, &layout, set_layouts);
    
    if (res != VK_SUCCESS) {
        LOG_ERROR("Failed to create compute pipeline: %d", res);
        return 0;
    }
    
    state->compute_pipelines[slot].active = true;
    state->compute_pipelines[slot].pipeline = pipeline;
    state->compute_pipelines[slot].layout = layout;
    
    // Store layouts for cleanup
    state->compute_pipelines[slot].set_layout_count = 0;
    for(uint32_t i=0; i<layout_count && i<4; ++i) {
        state->compute_pipelines[slot].set_layouts[i] = set_layouts[i];
        state->compute_pipelines[slot].set_layout_count++;
    }
    
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
    state->unit_quad_buffer = calloc(1, sizeof(struct VkBufferWrapper));
    if (!vk_buffer_create(state, v_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, state->unit_quad_buffer)) {
         LOG_FATAL("Failed to create unit quad vertex buffer");
    }
    vk_buffer_upload(state, state->unit_quad_buffer, PRIM_QUAD_VERTS, v_size, 0);

    // Index Buffer
    VkDeviceSize i_size = sizeof(PRIM_QUAD_INDICES);
    state->unit_quad_index_buffer = calloc(1, sizeof(struct VkBufferWrapper));
    if (!vk_buffer_create(state, i_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, state->unit_quad_index_buffer)) {
         LOG_FATAL("Failed to create unit quad index buffer");
    }
    vk_buffer_upload(state, state->unit_quad_index_buffer, PRIM_QUAD_INDICES, i_size, 0);
    
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

static uint32_t vulkan_graphics_pipeline_create(RendererBackend* backend, const void* vert_code, size_t vert_size, const void* frag_code, size_t frag_size, const DescriptorLayoutDef* layouts, uint32_t layout_count, uint32_t flags) {
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
    VkDescriptorSetLayout set_layouts[4] = {0};
    
    // Convert void* to uint32_t* (assume aligned)
    VkResult res = vk_create_graphics_pipeline_shader(state, (const uint32_t*)vert_code, vert_size, (const uint32_t*)frag_code, frag_size, layouts, layout_count, flags, &pipeline, &layout, set_layouts);
    
    if (res != VK_SUCCESS) {
        LOG_ERROR("Failed to create graphics pipeline: %d", res);
        return 0;
    }
    
    state->graphics_pipelines[slot].active = true;
    state->graphics_pipelines[slot].pipeline = pipeline;
    state->graphics_pipelines[slot].layout = layout;

    // Store layouts for cleanup
    state->graphics_pipelines[slot].set_layout_count = 0;
    for(uint32_t i=0; i<layout_count && i<4; ++i) {
        state->graphics_pipelines[slot].set_layouts[i] = set_layouts[i];
        state->graphics_pipelines[slot].set_layout_count++;
    }
    
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

// --- Dynamic Texture Management ---

static void vk_create_texture_internal(VulkanRendererState* state, int slot, uint32_t w, uint32_t h, uint32_t fmt) {
    // 1. Format Mapping
    VkFormat vk_fmt = VK_FORMAT_R8G8B8A8_UNORM;
    VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    
    if (fmt == 1) { // RGBA16F
        vk_fmt = VK_FORMAT_R16G16B16A16_SFLOAT;
        usage |= VK_IMAGE_USAGE_STORAGE_BIT; // Compute Write capable
    } else if (fmt == 2) { // D32
        vk_fmt = VK_FORMAT_D32_SFLOAT;
        usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT; // Depth Attachment
        aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    } else {
        // RGBA8 (Default)
        usage |= VK_IMAGE_USAGE_STORAGE_BIT; // Also allow Compute Write
    }

    // 2. Image
    VkImageCreateInfo ici = { 
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, 
        .imageType = VK_IMAGE_TYPE_2D, 
        .format = vk_fmt, 
        .extent = { w, h, 1 }, 
        .mipLevels = 1, 
        .arrayLayers = 1, 
        .samples = VK_SAMPLE_COUNT_1_BIT, 
        .tiling = VK_IMAGE_TILING_OPTIMAL, 
        .usage = usage, 
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE, 
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED 
    };
    
    if (vkCreateImage(state->device, &ici, NULL, &state->textures[slot].image) != VK_SUCCESS) {
        LOG_ERROR("Failed to create texture image");
        return;
    }
    
    // 3. Memory
    VkMemoryRequirements mr; 
    vkGetImageMemoryRequirements(state->device, state->textures[slot].image, &mr);
    VkMemoryAllocateInfo mai = { 
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, 
        .allocationSize = mr.size, 
        .memoryTypeIndex = find_mem_type(state->physical_device, mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) 
    };
    
    if (vkAllocateMemory(state->device, &mai, NULL, &state->textures[slot].memory) != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate texture memory");
        return;
    }
    vkBindImageMemory(state->device, state->textures[slot].image, state->textures[slot].memory, 0);

    // 4. View
    VkImageViewCreateInfo ivci = { 
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, 
        .image = state->textures[slot].image, 
        .viewType = VK_IMAGE_VIEW_TYPE_2D, 
        .format = vk_fmt, 
        .subresourceRange = { .aspectMask = aspect, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 } 
    };
    if (vkCreateImageView(state->device, &ivci, NULL, &state->textures[slot].view) != VK_SUCCESS) {
         LOG_ERROR("Failed to create texture view");
         return;
    }

    // 5. Sampler (Default Linear)
    // Only create sampler if not Depth (Depth usually sampled with specific samplers or point)
    // For now, use linear everywhere.
    VkSamplerCreateInfo sci = { 
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, 
        .magFilter = VK_FILTER_LINEAR, .minFilter = VK_FILTER_LINEAR, 
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK, 
        .unnormalizedCoordinates = VK_FALSE, 
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST 
    };
    vkCreateSampler(state->device, &sci, NULL, &state->textures[slot].sampler);

    // 6. Initial Transition
    if (aspect == VK_IMAGE_ASPECT_COLOR_BIT) {
        // Transition to General for flexible usage (Compute Write or Read)
        vk_transition_image_layout(state, state->textures[slot].image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    } else {
        // Depth
         // Usually undefined -> DepthAttachmentOptimal
         // But here we transition to ShaderReadOnly if we want to sample it? 
         // Let's leave it undefined, the RenderPass will handle Layout transitions via Attachments.
    }
    
    state->textures[slot].active = true;
    state->textures[slot].width = w;
    state->textures[slot].height = h;
    state->textures[slot].format = fmt;
}

static uint32_t vulkan_texture_create(RendererBackend* backend, uint32_t width, uint32_t height, uint32_t format) {
    VulkanRendererState* state = (VulkanRendererState*)backend->state;
    
    int slot = -1;
    for(int i=0; i<MAX_DYNAMIC_TEXTURES; ++i) {
        if (!state->textures[i].active) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        LOG_ERROR("Max dynamic textures reached (%d)", MAX_DYNAMIC_TEXTURES);
        return 0;
    }
    
    vk_create_texture_internal(state, slot, width, height, format);
    return (uint32_t)(slot + 1);
}

static void vulkan_texture_destroy(RendererBackend* backend, uint32_t handle) {
    VulkanRendererState* state = (VulkanRendererState*)backend->state;
    if (handle == 0 || handle > MAX_DYNAMIC_TEXTURES) return;
    
    int idx = (int)handle - 1;
    if (state->textures[idx].active) {
        vkDeviceWaitIdle(state->device); // Safety wait
        
        if (state->textures[idx].view) vkDestroyImageView(state->device, state->textures[idx].view, NULL);
        if (state->textures[idx].image) vkDestroyImage(state->device, state->textures[idx].image, NULL);
        if (state->textures[idx].memory) vkFreeMemory(state->device, state->textures[idx].memory, NULL);
        if (state->textures[idx].sampler) vkDestroySampler(state->device, state->textures[idx].sampler, NULL);
        
        // Note: We don't free the cached descriptor set because it's allocated from a pool 
        // that we might reset or it's just one set. 
        // A proper implementation would free the descriptor set or use a dynamic pool.
        // For MVP, we leak the descriptor handle (not memory, pool recycles it eventually if we reset pool).
        // Actually we don't reset the global pool. So this is a minor leak of slots in the pool.
        
        memset(&state->textures[idx], 0, sizeof(state->textures[idx]));
    }
}

static void vulkan_texture_resize(RendererBackend* backend, uint32_t handle, uint32_t width, uint32_t height) {
    VulkanRendererState* state = (VulkanRendererState*)backend->state;
    if (handle == 0 || handle > MAX_DYNAMIC_TEXTURES) return;
    
    int idx = (int)handle - 1;
    if (state->textures[idx].active) {
        if (state->textures[idx].width == width && state->textures[idx].height == height) return;
        
        // Recreate
        uint32_t fmt = state->textures[idx].format;
        vulkan_texture_destroy(backend, handle);
        vk_create_texture_internal(state, idx, width, height, fmt);
        
        // Update Descriptor if it exists
        if (state->textures[idx].descriptor) {
             VkDescriptorImageInfo dii = { 
                .sampler = state->textures[idx].sampler, 
                .imageView = state->textures[idx].view, 
                .imageLayout = VK_IMAGE_LAYOUT_GENERAL // Assuming General for now
            };
            VkWriteDescriptorSet w = { 
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 
                .dstSet = state->textures[idx].descriptor, 
                .dstBinding = 0, 
                .descriptorCount = 1, 
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 
                .pImageInfo = &dii 
            };
            vkUpdateDescriptorSets(state->device, 1, &w, 0, NULL);
        }
    }
}

static void* vulkan_texture_get_descriptor(RendererBackend* backend, uint32_t handle) {
    VulkanRendererState* state = (VulkanRendererState*)backend->state;
    if (handle == 0 || handle > MAX_DYNAMIC_TEXTURES) return NULL;
    
    int idx = (int)handle - 1;
    if (!state->textures[idx].active) return NULL;
    
    if (state->textures[idx].descriptor) return (void*)state->textures[idx].descriptor;
    
    // Allocate new descriptor (Set 2 compatible layout)
    VkDescriptorSetAllocateInfo dsai = { 
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, 
        .descriptorPool = state->descriptor_pool, 
        .descriptorSetCount = 1, 
        .pSetLayouts = &state->descriptor_layout // Default Layout (1 Sampler)
    };
    
    if (vkAllocateDescriptorSets(state->device, &dsai, &state->textures[idx].descriptor) != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate texture descriptor");
        return NULL;
    }
    
    // Update
    VkDescriptorImageInfo dii = { 
        .sampler = state->textures[idx].sampler, 
        .imageView = state->textures[idx].view, 
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL 
    };
    VkWriteDescriptorSet w = { 
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 
        .dstSet = state->textures[idx].descriptor, 
        .dstBinding = 0, 
        .descriptorCount = 1, 
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 
        .pImageInfo = &dii 
    };
    vkUpdateDescriptorSets(state->device, 1, &w, 0, NULL);
    
    return (void*)state->textures[idx].descriptor;
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
                vkCmdBindVertexBuffers(cmd, 0, 1, &state->unit_quad_buffer->buffer, offsets);
                vkCmdBindIndexBuffer(cmd, state->unit_quad_index_buffer->buffer, 0, VK_INDEX_TYPE_UINT32);
    // Bind Global Sets (0 and 2)
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state->pipeline_layout, 0, 1, &state->descriptor_set, 0, NULL);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state->pipeline_layout, 2, 1, &state->compute_target_descriptor, 0, NULL);
    
    // Bind Default Pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state->pipeline);
    
    // Track current layout to ensure Descriptor Sets are bound correctly
    VkPipelineLayout current_layout = state->pipeline_layout;

    // Pending Bindings State for Set 1
    VkBufferWrapper* pending_buffers[4] = {0};
    bool bindings_dirty = false;
    
    // --- Process Commands ---
    static double last_log_time = 0.0;
    double current_time = platform_get_time_ms() / 1000.0;
    bool should_log = (current_time - last_log_time >= logger_get_trace_interval());

    if (should_log && list->count > 0) {
        LOG_DEBUG("Vulkan: Executing %d commands", list->count);
        last_log_time = current_time;
    }

    for (uint32_t i = 0; i < list->count; ++i) {
        RenderCommand* rc = &list->commands[i];
        switch (rc->type) {
            case RENDER_CMD_BIND_PIPELINE: {
                uint32_t pid = rc->bind_pipeline.pipeline_id;
                if (pid == 0) {
                     vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state->pipeline);
                     current_layout = state->pipeline_layout;
                     
                     // Rebind global sets as they might have been disturbed by other pipelines
                     vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, current_layout, 0, 1, &state->descriptor_set, 0, NULL);
                     vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, current_layout, 2, 1, &state->compute_target_descriptor, 0, NULL);
                } else if (pid <= MAX_GRAPHICS_PIPELINES) {
                     int idx = (int)pid - 1;
                     if (state->graphics_pipelines[idx].active) {
                         vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state->graphics_pipelines[idx].pipeline);
                         current_layout = state->graphics_pipelines[idx].layout;
                     }
                }
                break;
            }
            case RENDER_CMD_BIND_BUFFER: {
                if (rc->bind_buffer.slot < 4) {
                    if (rc->bind_buffer.stream) {
                        pending_buffers[rc->bind_buffer.slot] = (VkBufferWrapper*)rc->bind_buffer.stream->buffer_handle;
                        // LOG_TRACE("Bind Buffer Slot %d: %p", rc->bind_buffer.slot, pending_buffers[rc->bind_buffer.slot]);
                    } else {
                        pending_buffers[rc->bind_buffer.slot] = NULL;
                    }
                    bindings_dirty = true;
                }
                break;
            }
            case RENDER_CMD_PUSH_CONSTANTS: {
                // Use current_layout!
                vkCmdPushConstants(cmd, current_layout, rc->push_constants.stage_flags, 0, rc->push_constants.size, rc->push_constants.data);
                break;
            }
            case RENDER_CMD_BIND_VERTEX_BUFFER: {
                if (rc->bind_buffer.stream && rc->bind_buffer.stream->buffer_handle) {
                    VkBufferWrapper* buf = (VkBufferWrapper*)rc->bind_buffer.stream->buffer_handle;
                    VkBuffer vbuf = buf->buffer;
                    VkDeviceSize off = 0;
                    vkCmdBindVertexBuffers(cmd, 0, 1, &vbuf, &off);
                }
                break;
            }
            case RENDER_CMD_BIND_INDEX_BUFFER: {
                if (rc->bind_buffer.stream && rc->bind_buffer.stream->buffer_handle) {
                    VkBufferWrapper* buf = (VkBufferWrapper*)rc->bind_buffer.stream->buffer_handle;
                    vkCmdBindIndexBuffer(cmd, buf->buffer, 0, VK_INDEX_TYPE_UINT32);
                }
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
                        .pSetLayouts = &state->compute_ssbo_layout
                    };
                    if (vkAllocateDescriptorSets(state->device, &alloc_info, &set) == VK_SUCCESS) {
                        VkWriteDescriptorSet writes[4];
                        VkDescriptorBufferInfo dbis[4];
                        
                        // Fallback: If slot 0 is valid, use it for others to prevent invalid descriptors.
                        VkBufferWrapper* fallback = pending_buffers[0]; 

                        for(int b=0; b<4; ++b) {
                            VkBufferWrapper* target = pending_buffers[b] ? pending_buffers[b] : fallback;
                            if (target) {
                                dbis[b].buffer = target->buffer;
                                dbis[b].offset = 0;
                                dbis[b].range = VK_WHOLE_SIZE;
                                
                                writes[b].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                                writes[b].pNext = NULL;
                                writes[b].dstSet = set;
                                writes[b].dstBinding = b;
                                writes[b].dstArrayElement = 0;
                                writes[b].descriptorCount = 1;
                                writes[b].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                                writes[b].pImageInfo = NULL;
                                writes[b].pBufferInfo = &dbis[b];
                                writes[b].pTexelBufferView = NULL;
                            }
                        }
                        // Always update 4 bindings because the layout requires it
                        vkUpdateDescriptorSets(state->device, 4, writes, 0, NULL);
                        
                        // Use current_layout!
                        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, current_layout, 1, 1, &set, 0, NULL);
                    } else {
                        LOG_ERROR("Vulkan: Failed to allocate frame descriptor set!");
                    }
                    bindings_dirty = false;
                }
                
                if (should_log) {
                    LOG_DEBUG("Vulkan: DrawIndexed IndexCount=%d InstanceCount=%d", rc->draw_indexed.index_count, rc->draw_indexed.instance_count);
                }
                if (rc->type == RENDER_CMD_DRAW) {
                    // LOG_TRACE("Draw: %d verts, %d insts", rc->draw.vertex_count, rc->draw.instance_count);
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

    // Handle Screenshot Logic
    VkBuffer screenshot_buffer = VK_NULL_HANDLE;
    VkDeviceMemory screenshot_memory = VK_NULL_HANDLE;
    bool capturing_screenshot = state->screenshot_pending;

    if (capturing_screenshot) {
        state->screenshot_pending = false; // Clear flag
        
        VkDeviceSize size = state->swapchain_extent.width * state->swapchain_extent.height * 4;
        
        // 1. Create Buffer
        VkBufferCreateInfo bci = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = size,
            .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
        };
        vkCreateBuffer(state->device, &bci, NULL, &screenshot_buffer);
        
        VkMemoryRequirements mem_reqs;
        vkGetBufferMemoryRequirements(state->device, screenshot_buffer, &mem_reqs);
        
        VkMemoryAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = mem_reqs.size,
            .memoryTypeIndex = find_mem_type(state->physical_device, mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
        };
        vkAllocateMemory(state->device, &alloc_info, NULL, &screenshot_memory);
        vkBindBufferMemory(state->device, screenshot_buffer, screenshot_memory, 0);
        
        // 2. Transition Swapchain to TRANSFER_SRC
        VkImageMemoryBarrier barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
            .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
            .image = state->swapchain_imgs[image_index],
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        };
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
        
        // 3. Copy
        VkBufferImageCopy region = {
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
            .imageOffset = {0, 0, 0},
            .imageExtent = {state->swapchain_extent.width, state->swapchain_extent.height, 1}
        };
        vkCmdCopyImageToBuffer(cmd, state->swapchain_imgs[image_index], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, screenshot_buffer, 1, &region);
        
        // 4. Transition back to PRESENT_SRC
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
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
    
    // Save Screenshot (Async)
    if (capturing_screenshot) {
        vkWaitForFences(state->device, 1, &state->fences[state->current_frame_cursor], VK_TRUE, UINT64_MAX);
        
        void* mapped_data;
        vkMapMemory(state->device, screenshot_memory, 0, VK_WHOLE_SIZE, 0, &mapped_data);
        
        // Copy to CPU buffer
        VkDeviceSize data_size = state->swapchain_extent.width * state->swapchain_extent.height * 4;
        void* cpu_data = malloc(data_size);
        if (cpu_data) {
            memcpy(cpu_data, mapped_data, data_size);
            
            // Spawn Thread
            ScreenshotTask* task = malloc(sizeof(ScreenshotTask));
            if (task) {
                platform_strncpy(task->path, state->screenshot_path, sizeof(task->path) - 1);
                task->width = state->swapchain_extent.width;
                task->height = state->swapchain_extent.height;
                task->data = cpu_data;
                
                Thread* t = thread_create(screenshot_thread_func, task);
                if (t) {
                    thread_detach(t);
                } else {
                    LOG_ERROR("Failed to create screenshot thread");
                    free(task);
                    free(cpu_data);
                }
            } else {
                free(cpu_data);
            }
        }
        
        vkUnmapMemory(state->device, screenshot_memory);
        vkDestroyBuffer(state->device, screenshot_buffer, NULL);
        vkFreeMemory(state->device, screenshot_memory, NULL);
    }

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

    backend->texture_create = vulkan_texture_create;
    backend->texture_destroy = vulkan_texture_destroy;
    backend->texture_resize = vulkan_texture_resize;
    backend->texture_get_descriptor = vulkan_texture_get_descriptor;

    return backend;
}
