#ifndef VK_UI_RENDER_H
#define VK_UI_RENDER_H

#include "vk_types.h"
#include "engine/ui/ui_renderer.h"

bool vk_build_vertices_from_draw_list(VulkanRendererState* state, FrameResources *frame, const UiDrawList* draw_list);

#endif // VK_UI_RENDER_H
