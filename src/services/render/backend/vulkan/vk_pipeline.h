#ifndef VK_PIPELINE_H
#define VK_PIPELINE_H

#include "vk_types.h"

void vk_create_pipeline(VulkanRendererState* state, const char* vert_spv, const char* frag_spv);
void vk_create_descriptor_layout(VulkanRendererState* state);

#endif // VK_PIPELINE_H
