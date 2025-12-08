#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

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
    float ui_scale;
} AppContext;

static void scale_layout(LayoutNode* node, float scale) {
    if (!node) return;
    node->rect.x *= scale;
    node->rect.y *= scale;
    node->rect.w *= scale;
    node->rect.h *= scale;
    for (size_t i = 0; i < node->child_count; i++) {
        scale_layout(&node->children[i], scale);
    }
}

static void scale_widget_padding(WidgetArray* widgets, float scale) {
    if (!widgets) return;
    for (size_t i = 0; i < widgets->count; i++) {
        widgets->items[i].padding *= scale;
    }
}

static float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static void apply_slider_action(Widget* w, Model* model, double mx) {
    if (!w) return;
    float local_t = (float)((mx - w->rect.x) / w->rect.w);
    local_t = clamp01(local_t);
    float denom = (w->maxv - w->minv);
    float new_val = denom != 0.0f ? (w->minv + local_t * denom) : w->minv;
    w->value = new_val;
    if (w->value_binding) {
        model_set_number(model, w->value_binding, new_val);
    }
    if (w->id) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%s: %.0f%%", w->id, clamp01((new_val - w->minv) / (denom != 0.0f ? denom : 1.0f)) * 100.0f);
        model_set_string(model, "sliderState", buf);
    }
}

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
            const char* on_payload = w->click_value ? w->click_value : "On";
            const char* payload = (new_val > 0.5f) ? on_payload : "Off";
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
        if ((w->type == W_BUTTON || w->type == W_CHECKBOX || w->type == W_HSLIDER) && point_in_widget(w, mx, my)) {
            if (w->type == W_HSLIDER) {
                apply_slider_action(w, ctx->model, mx);
            } else {
                apply_click_action(w, ctx->model);
            }
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
    float base_w = layout_root->rect.w > 1.0f ? layout_root->rect.w : 1024.0f;
    float base_h = layout_root->rect.h > 1.0f ? layout_root->rect.h : 640.0f;

    if (!glfwInit()) fatal("glfwInit");
    if (!glfwVulkanSupported()) fatal("glfw Vulkan not supported");
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = monitor ? glfwGetVideoMode(monitor) : NULL;
    float target_w = mode ? mode->width * 0.9f : base_w;
    float target_h = mode ? mode->height * 0.9f : base_h;
    float ui_scale = fminf(target_w / base_w, target_h / base_h);
    if (ui_scale < 0.8f) ui_scale = 0.8f;
    if (ui_scale > 1.35f) ui_scale = 1.35f;

    scale_layout(layout_root, ui_scale);

    WidgetArray widgets = materialize_widgets(layout_root);
    scale_widget_padding(&widgets, ui_scale);
    update_widget_bindings(ui_root, model);
    populate_widgets_from_layout(layout_root, widgets.items, widgets.count);

    int window_w = (int)(base_w * ui_scale);
    int window_h = (int)(base_h * ui_scale);
    if (window_w < 800) window_w = 800;
    if (window_h < 520) window_h = 520;
    GLFWwindow* window = glfwCreateWindow(window_w, window_h, "vk_gui (Vulkan)", NULL, NULL);
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
    AppContext app_ctx = { .scroll = scroll_ctx, .widgets = &widgets, .model = model, .ui_scale = ui_scale };
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
        scale_widget_padding(&widgets, app_ctx.ui_scale);
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
