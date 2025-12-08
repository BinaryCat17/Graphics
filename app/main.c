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

typedef struct {
    ScrollContext* scroll;
    WidgetArray* widgets;
    Model* model;
} AppContext;

static int point_in_widget(const Widget* w, double mx, double my) {
    if (!w) return 0;
    float x = w->rect.x;
    float y = w->rect.y + w->scroll_offset;
    return mx >= x && mx <= x + w->rect.w && my >= y && my <= y + w->rect.h;
}

static void apply_click_action(Widget* w, Model* model) {
    if (!w || !model) return;
    if (w->type == W_BUTTON && w->click_binding) {
        const char* payload = w->click_value ? w->click_value : (w->id ? w->id : w->text);
        if (payload) model_set_string(model, w->click_binding, payload);
    }
    if (w->type == W_CHECKBOX) {
        float new_val = (w->value > 0.5f) ? 0.0f : 1.0f;
        w->value = new_val;
        if (w->value_binding) {
            model_set_number(model, w->value_binding, new_val);
        }
        if (w->click_binding) {
            const char* payload = (new_val > 0.5f) ? (w->click_value ? w->click_value : "1") : "0";
            model_set_string(model, w->click_binding, payload);
        }
    }
}

static void on_mouse_button(GLFWwindow* window, int button, int action, int mods) {
    (void)mods;
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) return;
    AppContext* ctx = (AppContext*)glfwGetWindowUserPointer(window);
    if (!ctx || !ctx->widgets) return;
    double mx = 0.0, my = 0.0;
    glfwGetCursorPos(window, &mx, &my);
    for (size_t i = 0; i < ctx->widgets->count; i++) {
        Widget* w = &ctx->widgets->items[i];
        if ((w->type == W_BUTTON || w->type == W_CHECKBOX) && point_in_widget(w, mx, my)) {
            apply_click_action(w, ctx->model);
            break;
        }
    }
}

static void on_scroll(GLFWwindow* window, double xoff, double yoff) {
    (void)xoff;
    AppContext* ctx = (AppContext*)glfwGetWindowUserPointer(window);
    if (!ctx || !ctx->widgets) return;
    double mx = 0.0, my = 0.0;
    glfwGetCursorPos(window, &mx, &my);
    scroll_handle_event(ctx->scroll, ctx->widgets->items, ctx->widgets->count, mx, my, yoff);
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
    AppContext app_ctx = { .scroll = scroll_ctx, .widgets = &widgets, .model = model };
    glfwSetWindowUserPointer(window, &app_ctx);
    glfwSetScrollCallback(window, on_scroll);
    glfwSetMouseButtonCallback(window, on_mouse_button);

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
