#ifndef VK_RENDER_GRAPH_H
#define VK_RENDER_GRAPH_H

#include "engine/render/render_graph/render_graph.h"
#include "engine/render/backend/vulkan/vk_types.h"

// Backend-specific execution context
typedef struct VkRenderGraphContext {
    VulkanRendererState* state;
    VkCommandBuffer cmd;
    uint32_t current_frame_index;
} VkRenderGraphContext;

// Implementation of the execution function
void vk_rg_execute(RgGraph* graph, VkRenderGraphContext* ctx);

#endif // VK_RENDER_GRAPH_H
