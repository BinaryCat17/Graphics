#ifndef VULKAN_RENDERER_H
#define VULKAN_RENDERER_H

#include <stdbool.h>
#include <GLFW/glfw3.h>

#include "ui/ui_json.h"
#include "Graphics.h"

bool vk_renderer_init(GLFWwindow* window, const char* vert_spv, const char* frag_spv, const char* font_path, WidgetArray widgets, const CoordinateTransformer* transformer);
void vk_renderer_update_transformer(const CoordinateTransformer* transformer);
void vk_renderer_draw_frame(void);
void vk_renderer_cleanup(void);

#endif // VULKAN_RENDERER_H
