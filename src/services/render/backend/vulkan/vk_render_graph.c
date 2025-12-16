#include "vk_render_graph.h"
#include "services/render/render_graph/render_graph_private.h"

#include <stdio.h>
#include <stdlib.h>
#include "services/render/backend/vulkan/vk_utils.h"

static VkImageLayout rg_usage_to_layout(RgPassResourceRef* ref) {
    if (ref->is_depth) return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    if (ref->is_write) return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    // Reads could be shader read or transfer source
    // Simplification: Assume shader read for now
    return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

static VkAccessFlags rg_usage_to_access(VkImageLayout layout) {
    switch (layout) {
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return VK_ACCESS_SHADER_READ_BIT;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: return VK_ACCESS_TRANSFER_WRITE_BIT;
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: return 0; // Memory visible to presentation engine
        default: return 0;
    }
}

static VkPipelineStageFlags rg_layout_to_stage(VkImageLayout layout) {
    switch (layout) {
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; // Assume fragment for now
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: return VK_PIPELINE_STAGE_TRANSFER_BIT;
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        default: return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }
}

void vk_rg_execute(RgGraph* graph, VkRenderGraphContext* ctx) {
    if (!graph || !ctx || !ctx->cmd) return;

    // Reset resource tracking state at start of frame? 
    // Or assume it persists? 
    // Usually we track current layout. Imported resources might have a known state.
    // For simplicity, we assume imported resources (swapchain) start at UNDEFINED or PRESENT_SRC.
    
    // Iterate passes
    for (size_t i = 0; i < graph->pass_count; ++i) {
        RgPass* pass = &graph->passes[i];
        
        // 1. Insert Barriers
        for (size_t r = 0; r < pass->resource_count; ++r) {
            RgPassResourceRef* ref = &pass->resources[r];
            RgResource* res = &graph->resources[ref->handle - 1]; // Handle is 1-based
            
            // We only handle textures for barriers right now
            if (res->type != RG_RESOURCE_TEXTURE) continue;
            
            // Assume imported resource pointer is actually the VkImage (or wrapper)
            // But we need the image handle.
            // Let's assume external_ptr IS the VkImage for now (unsafe cast).
            // In a real engine, we'd have a specific wrapper struct.
            VkImage image = (VkImage)res->external_ptr; 
            if (image == VK_NULL_HANDLE) continue;

            VkImageLayout new_layout = rg_usage_to_layout(ref);
            
            // Determine old layout. 
            // Ideally stored in RgResource. For now, assume UNDEFINED if first use?
            // Or we need to track it.
            // Let's rely on Vulkan blindly transitioning from Undefined if we don't care, 
            // but that clears content.
            // For now: Always transition.
            
            VkImageMemoryBarrier barrier = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED, // TODO: Track actual previous layout
                .newLayout = new_layout,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = image,
                .subresourceRange = {
                    .aspectMask = (ref->is_depth) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0, .levelCount = 1,
                    .baseArrayLayer = 0, .layerCount = 1
                },
                .srcAccessMask = 0, // TODO: Track previous access
                .dstAccessMask = rg_usage_to_access(new_layout)
            };
            
            // Optimization: If layout matches, skip barrier?
            // Yes, but we need to track state.
            // Let's just emit the barrier for correctness first (logic above is incomplete on tracking).
            
            vkCmdPipelineBarrier(
                ctx->cmd,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, // TODO: Track previous stage
                rg_layout_to_stage(new_layout),
                0, 
                0, NULL,
                0, NULL,
                1, &barrier
            );
        }

        // 2. Execute Pass
        if (pass->execute_fn) {
            RgCmdBuffer rg_cmd = {
                .backend_cmd = ctx->cmd,
                .backend_state = ctx->state
            };
            pass->execute_fn(&rg_cmd, pass->user_data);
        }
    }
}