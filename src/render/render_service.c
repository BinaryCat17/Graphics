#include "render_service.h"

#include <GLFW/glfw3.h>

#include "ui/ui_service.h"
#include "vulkan_renderer.h"

bool render_service_init(AppServices* services) {
    if (!services || !services->window) return false;
    return vk_renderer_init(services->window, services->assets.vert_spv_path, services->assets.frag_spv_path,
                            services->assets.font_path, services->widgets, &services->transformer);
}

void render_service_update_transformer(AppServices* services) {
    if (!services) return;
    vk_renderer_update_transformer(&services->transformer);
}

void render_loop(AppServices* services) {
    if (!services) return;
    while (!glfwWindowShouldClose(services->window)) {
        glfwPollEvents();
        ui_frame_update(services);
        vk_renderer_draw_frame();
    }
}

void render_service_shutdown(AppServices* services) {
    (void)services;
    vk_renderer_cleanup();
}
