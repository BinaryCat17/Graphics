#ifndef VK_SWAPCHAIN_H
#define VK_SWAPCHAIN_H

#include "vk_types.h"

void vk_create_swapchain_and_views(VulkanRendererState* state, VkSwapchainKHR old_swapchain);
void vk_create_depth_resources(VulkanRendererState* state);
void vk_destroy_depth_resources(VulkanRendererState* state);
void vk_create_render_pass(VulkanRendererState* state);
void vk_create_cmds_and_sync(VulkanRendererState* state);
void vk_cleanup_swapchain(VulkanRendererState* state, bool keep_swapchain_handle);

#endif // VK_SWAPCHAIN_H
