#include "render_service.h"

#include <GLFW/glfw3.h>
#include "render/vulkan_renderer.h"

bool render_service_init(RenderRuntimeContext* render, const Assets* assets, WidgetArray widgets) {
    if (!render || !render->window || !assets) return false;
    return vk_renderer_init(render->window, assets->vert_spv_path, assets->frag_spv_path, assets->font_path, widgets,
                            &render->transformer);
}

void render_service_update_transformer(RenderRuntimeContext* render) {
    if (!render) return;
    vk_renderer_update_transformer(&render->transformer);
}

void render_loop(RenderRuntimeContext* render, UiContext* ui, Model* model) {
    if (!render) return;
    while (!glfwWindowShouldClose(render->window)) {
        glfwPollEvents();
        ui_frame_update(ui, model);
        vk_renderer_draw_frame();
    }
}

void render_service_shutdown(RenderRuntimeContext* render) {
    (void)render;
    vk_renderer_cleanup();
}
