#ifndef VULKAN_RENDERER_H
#define VULKAN_RENDERER_H

#include <stdbool.h>
#include <GLFW/glfw3.h>

#include "ui_json.h"

bool vk_renderer_init(GLFWwindow* window, const char* vert_spv, const char* frag_spv, const Widget* widgets);
void vk_renderer_draw_frame(void);
void vk_renderer_cleanup(void);

#endif // VULKAN_RENDERER_H
