#ifndef VK_PIPELINE_H
#define VK_PIPELINE_H

#include "vk_types.h"

void vk_create_descriptor_layout(VulkanRendererState* state);
VkResult vk_create_compute_pipeline_shader(VulkanRendererState* state, const uint32_t* code, size_t size, const DescriptorLayoutDef* layouts, uint32_t layout_count, VkPipeline* out_pipeline, VkPipelineLayout* out_layout, VkDescriptorSetLayout* out_set_layouts);

VkResult vk_create_graphics_pipeline_shader(VulkanRendererState* state, const uint32_t* vert_code, size_t vert_size, const uint32_t* frag_code, size_t frag_size, const DescriptorLayoutDef* layouts, uint32_t layout_count, uint32_t flags, VkPipeline* out_pipeline, VkPipelineLayout* out_layout, VkDescriptorSetLayout* out_set_layouts);

void vk_create_pipeline(VulkanRendererState* state);

#endif // VK_PIPELINE_H
