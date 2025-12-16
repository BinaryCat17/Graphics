#ifndef VK_CONTEXT_H
#define VK_CONTEXT_H

#include "vk_types.h"

void vk_create_instance(VulkanRendererState* state);
void vk_pick_physical_and_create_device(VulkanRendererState* state);
void vk_recreate_instance_and_surface(VulkanRendererState* state);

#endif // VK_CONTEXT_H
