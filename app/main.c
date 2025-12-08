#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "ui_json.h"
#include "vulkan_renderer.h"
#include "assets.h"
#include "scroll.h"

static void fatal(const char* msg) {
    fprintf(stderr, "Fatal: %s\n", msg);
    exit(1);
}

int main(int argc, char** argv) {
    const char* assets_dir = argc >= 2 ? argv[1] : "assets";
    Assets assets;
    if (!load_assets(assets_dir, &assets)) return 1;

    Model* model = parse_model_json(assets.model_text, assets.model_path);
    Style* styles = parse_styles_json(assets.styles_text);
    UiNode* ui_root = parse_layout_json(assets.layout_text, model, styles);
    if (!ui_root) { free_model(model); free_styles(styles); free_assets(&assets); return 1; }
    LayoutNode* layout_root = build_layout_tree(ui_root);
    if (!layout_root) { free_model(model); free_styles(styles); free_ui_tree(ui_root); free_assets(&assets); return 1; }
    measure_layout(layout_root);
    assign_layout(layout_root, 0.0f, 0.0f);
    WidgetArray widgets = materialize_widgets(layout_root);
    update_widget_bindings(ui_root, model);
    populate_widgets_from_layout(layout_root, widgets.items, widgets.count);

    if (!glfwInit()) fatal("glfwInit");
    if (!glfwVulkanSupported()) fatal("glfw Vulkan not supported");
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(1024, 640, "vk_gui (Vulkan)", NULL, NULL);
    if (!window) fatal("glfwCreateWindow");

    ScrollContext* scroll_ctx = scroll_init(widgets.items, widgets.count);
    if (!scroll_ctx) {
        glfwDestroyWindow(window);
        glfwTerminate();
        free_model(model);
        free_styles(styles);
        free_widgets(widgets);
        free_ui_tree(ui_root);
        free_layout_tree(layout_root);
        free_assets(&assets);
        return 1;
    }
    scroll_set_callback(window, scroll_ctx);

    if (!vk_renderer_init(window, assets.vert_spv_path, assets.frag_spv_path, assets.font_path, widgets)) {
        glfwDestroyWindow(window);
        glfwTerminate();
        free_model(model);
        free_styles(styles);
        free_widgets(widgets);
        scroll_free(scroll_ctx);
        free_ui_tree(ui_root);
        free_layout_tree(layout_root);
        free_assets(&assets);
        return 1;
    }

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        update_widget_bindings(ui_root, model);
        populate_widgets_from_layout(layout_root, widgets.items, widgets.count);
        scroll_apply_offsets(scroll_ctx, widgets.items, widgets.count);
        vk_renderer_draw_frame();
    }

    vk_renderer_cleanup();
    save_model(model);
    free_model(model);
    free_styles(styles);
    free_widgets(widgets);
    free_ui_tree(ui_root);
    free_layout_tree(layout_root);
    scroll_free(scroll_ctx);
    free_assets(&assets);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
