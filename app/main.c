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
#include "Graphics.h"
#include "cad_scene_yaml.h"

static void fatal(const char* msg) {
    fprintf(stderr, "Fatal: %s\n", msg);
    exit(1);
}

typedef struct {
    ScrollContext* scroll;
    WidgetArray* widgets;
    Model* model;
    LayoutNode* layout_root;
    float base_w;
    float base_h;
    float ui_scale;
    CoordinateTransformer transformer;
} AppContext;

static void scale_layout(LayoutNode* node, float scale) {
    if (!node) return;
    node->rect.x = node->base_rect.x * scale;
    node->rect.y = node->base_rect.y * scale;
    node->rect.w = node->base_rect.w * scale;
    node->rect.h = node->base_rect.h * scale;
    for (size_t i = 0; i < node->child_count; i++) {
        scale_layout(&node->children[i], scale);
    }
}

static float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static float compute_ui_scale(float base_w, float base_h, float target_w, float target_h) {
    if (base_w <= 0.0f || base_h <= 0.0f) return 1.0f;
    float ui_scale = fminf(target_w / base_w, target_h / base_h);
    if (ui_scale < 0.8f) ui_scale = 0.8f;
    if (ui_scale > 1.35f) ui_scale = 1.35f;
    return ui_scale;
}

static void update_transformer(AppContext* ctx, GLFWwindow* window) {
    if (!ctx || !window) return;
    int win_w = 0, win_h = 0, fb_w = 0, fb_h = 0;
    glfwGetWindowSize(window, &win_w, &win_h);
    glfwGetFramebufferSize(window, &fb_w, &fb_h);
    float dpi_scale_x = (win_w > 0) ? (float)fb_w / (float)win_w : 1.0f;
    float dpi_scale_y = (win_h > 0) ? (float)fb_h / (float)win_h : 1.0f;
    float dpi_scale = (dpi_scale_x + dpi_scale_y) * 0.5f;
    if (dpi_scale <= 0.0f) dpi_scale = 1.0f;

    coordinate_transformer_init(&ctx->transformer, dpi_scale, ctx->ui_scale,
                                (Vec2){(float)fb_w, (float)fb_h});
    vk_renderer_update_transformer(&ctx->transformer);
}

static void refresh_layout_for_scale(AppContext* ctx, float new_scale) {
    if (!ctx || !ctx->layout_root || !ctx->widgets) return;
    if (new_scale <= 0.0f) return;
    float ratio = ctx->ui_scale > 0.0f ? new_scale / ctx->ui_scale : 1.0f;
    ctx->ui_scale = new_scale;
    scale_layout(ctx->layout_root, ctx->ui_scale);
    populate_widgets_from_layout(ctx->layout_root, ctx->widgets->items, ctx->widgets->count);
    apply_widget_padding_scale(ctx->widgets, ctx->ui_scale);
    scroll_rebuild(ctx->scroll, ctx->widgets->items, ctx->widgets->count, ratio);
}

static void apply_slider_action(Widget* w, Model* model, float mx) {
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
    float y_offset = w->scroll_static ? 0.0f : w->scroll_offset;
    float y = w->rect.y + y_offset;
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
    AppContext* ctx = (AppContext*)glfwGetWindowUserPointer(window);
    if (!ctx || !ctx->widgets) return;
    double mx = 0.0, my = 0.0;
    glfwGetCursorPos(window, &mx, &my);
    Vec2 screen = {(float)(mx * ctx->transformer.dpi_scale), (float)(my * ctx->transformer.dpi_scale)};
    Vec2 logical = coordinate_screen_to_logical(&ctx->transformer, screen);
    int pressed = (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS);
    if (scroll_handle_mouse_button(ctx->scroll, ctx->widgets->items, ctx->widgets->count, logical.x, logical.y, pressed)) return;
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) return;
    for (size_t i = 0; i < ctx->widgets->count; i++) {
        Widget* w = &ctx->widgets->items[i];
        if ((w->type == W_BUTTON || w->type == W_CHECKBOX || w->type == W_HSLIDER) && point_in_widget(w, logical.x, logical.y)) {
            if (w->type == W_HSLIDER) {
                apply_slider_action(w, ctx->model, logical.x);
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
    Vec2 screen = {(float)(mx * ctx->transformer.dpi_scale), (float)(my * ctx->transformer.dpi_scale)};
    Vec2 logical = coordinate_screen_to_logical(&ctx->transformer, screen);
    scroll_handle_event(ctx->scroll, ctx->widgets->items, ctx->widgets->count, logical.x, logical.y, yoff);
}

static void on_cursor_pos(GLFWwindow* window, double x, double y) {
    AppContext* ctx = (AppContext*)glfwGetWindowUserPointer(window);
    if (!ctx || !ctx->widgets) return;
    Vec2 screen = {(float)(x * ctx->transformer.dpi_scale), (float)(y * ctx->transformer.dpi_scale)};
    Vec2 logical = coordinate_screen_to_logical(&ctx->transformer, screen);
    scroll_handle_cursor(ctx->scroll, ctx->widgets->items, ctx->widgets->count, logical.x, logical.y);
}

static void on_framebuffer_size(GLFWwindow* window, int width, int height) {
    if (width <= 0 || height <= 0) return;
    AppContext* ctx = (AppContext*)glfwGetWindowUserPointer(window);
    if (!ctx) return;
    float new_scale = compute_ui_scale(ctx->base_w, ctx->base_h, (float)width, (float)height);
    refresh_layout_for_scale(ctx, new_scale);
    update_transformer(ctx, window);
}

int main(int argc, char** argv) {
    const char* assets_dir = "assets";
    const char* scene_path = NULL;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--assets") == 0 && i + 1 < argc) {
            assets_dir = argv[++i];
        } else if (strcmp(argv[i], "--scene") == 0 && i + 1 < argc) {
            scene_path = argv[++i];
        }
    }
    if (!scene_path) {
        fprintf(stderr, "Usage: %s --scene <file> [--assets <dir>]\n", argv[0]);
        return 1;
    }

    Scene scene;
    SceneError scene_err = {0};
    if (!parse_scene_yaml(scene_path, &scene, &scene_err)) {
        fprintf(stderr, "Failed to load scene %s:%d:%d %s\n", scene_path, scene_err.line, scene_err.column, scene_err.message);
        return 1;
    }

    Assets assets;
    if (!load_assets(assets_dir, &assets)) {
        scene_dispose(&scene);
        return 1;
    }

    Model* model = parse_model_json(assets.model_text, assets.model_path);
    Style* styles = parse_styles_json(assets.styles_text);
    UiNode* ui_root = parse_layout_json(assets.layout_text, model, styles, assets.font_path);
    if (!ui_root) { free_model(model); free_styles(styles); free_assets(&assets); return 1; }
    LayoutNode* layout_root = build_layout_tree(ui_root);
    if (!layout_root) { free_model(model); free_styles(styles); free_ui_tree(ui_root); free_assets(&assets); return 1; }
    measure_layout(layout_root);
    assign_layout(layout_root, 0.0f, 0.0f);
    capture_layout_base(layout_root);
    float base_w = layout_root->base_rect.w > 1.0f ? layout_root->base_rect.w : 1024.0f;
    float base_h = layout_root->base_rect.h > 1.0f ? layout_root->base_rect.h : 640.0f;

    if (!glfwInit()) fatal("glfwInit");
    if (!glfwVulkanSupported()) fatal("glfw Vulkan not supported");
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = monitor ? glfwGetVideoMode(monitor) : NULL;
    float target_w = mode ? mode->width * 0.9f : base_w;
    float target_h = mode ? mode->height * 0.9f : base_h;
    float ui_scale = compute_ui_scale(base_w, base_h, target_w, target_h);

    float layout_scale = ui_scale;
    scale_layout(layout_root, layout_scale);

    WidgetArray widgets = materialize_widgets(layout_root);
    apply_widget_padding_scale(&widgets, layout_scale);
    update_widget_bindings(ui_root, model);
    populate_widgets_from_layout(layout_root, widgets.items, widgets.count);

    int window_w = (int)fminf(layout_root->rect.w, target_w);
    int window_h = (int)fminf(layout_root->rect.h, target_h);
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
    CoordinateTransformer transformer = {0};
    AppContext app_ctx = { .scroll = scroll_ctx, .widgets = &widgets, .model = model, .layout_root = layout_root, .base_w = base_w, .base_h = base_h, .ui_scale = layout_scale, .transformer = transformer };
    glfwSetWindowUserPointer(window, &app_ctx);
    glfwSetFramebufferSizeCallback(window, on_framebuffer_size);
    glfwSetScrollCallback(window, on_scroll);
    glfwSetMouseButtonCallback(window, on_mouse_button);
    glfwSetCursorPosCallback(window, on_cursor_pos);

    update_transformer(&app_ctx, window);

    if (!vk_renderer_init(window, assets.vert_spv_path, assets.frag_spv_path, assets.font_path, widgets, &app_ctx.transformer)) {
        glfwDestroyWindow(window);
        glfwTerminate();
        free_model(model);
        free_styles(styles);
        free_widgets(widgets);
        scroll_free(scroll_ctx);
        free_ui_tree(ui_root);
        free_layout_tree(layout_root);
        free_assets(&assets);
        scene_dispose(&scene);
        return 1;
    }

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        update_widget_bindings(ui_root, model);
        populate_widgets_from_layout(layout_root, widgets.items, widgets.count);
        apply_widget_padding_scale(&widgets, app_ctx.ui_scale);
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
    scene_dispose(&scene);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
