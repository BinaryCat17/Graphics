#ifndef VK_PIPELINE_H
#define VK_PIPELINE_H

#include "vk_types.h"

void vk_create_descriptor_layout(VulkanRendererState* state);
void vk_create_pipeline(VulkanRendererState* state);

VkResult vk_create_compute_pipeline_shader(VulkanRendererState* state, const uint32_t* code, size_t size, int layout_idx, VkPipeline* out_pipeline, VkPipelineLayout* out_layout);

#endif // VK_PIPELINE_H
