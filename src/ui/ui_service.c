#include "ui_service.h"

#include <math.h>
#include <stdio.h>

#include "ui/scroll.h"

static float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

float ui_compute_scale(const AppServices* services, float target_w, float target_h) {
    if (!services || services->base_w <= 0.0f || services->base_h <= 0.0f) return 1.0f;
    float ui_scale = fminf(target_w / services->base_w, target_h / services->base_h);
    if (ui_scale < 0.8f) ui_scale = 0.8f;
    if (ui_scale > 1.35f) ui_scale = 1.35f;
    return ui_scale;
}

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

bool ui_build(AppServices* services) {
    if (!services || !services->model) return false;

    services->styles = parse_styles_config(services->assets.styles_doc.root);
    services->ui_root = parse_layout_config(services->assets.layout_doc.root, services->model, services->styles, services->assets.font_path, &services->scene);
    if (!services->ui_root) return false;

    services->layout_root = build_layout_tree(services->ui_root);
    if (!services->layout_root) return false;

    measure_layout(services->layout_root);
    assign_layout(services->layout_root, 0.0f, 0.0f);
    capture_layout_base(services->layout_root);
    services->base_w = services->layout_root->base_rect.w > 1.0f ? services->layout_root->base_rect.w : 1024.0f;
    services->base_h = services->layout_root->base_rect.h > 1.0f ? services->layout_root->base_rect.h : 640.0f;

    return true;
}

bool ui_prepare_runtime(AppServices* services, float ui_scale) {
    if (!services || !services->layout_root) return false;
    scale_layout(services->layout_root, ui_scale);

    services->widgets = materialize_widgets(services->layout_root);
    apply_widget_padding_scale(&services->widgets, ui_scale);
    update_widget_bindings(services->ui_root, services->model);
    populate_widgets_from_layout(services->layout_root, services->widgets.items, services->widgets.count);
    services->scroll = scroll_init(services->widgets.items, services->widgets.count);
    if (!services->scroll) return false;

    services->ui_scale = ui_scale;
    return true;
}

void ui_refresh_layout(AppServices* services, float new_scale) {
    if (!services || !services->layout_root || !services->widgets.items) return;
    if (new_scale <= 0.0f) return;
    float ratio = services->ui_scale > 0.0f ? new_scale / services->ui_scale : 1.0f;
    services->ui_scale = new_scale;
    scale_layout(services->layout_root, services->ui_scale);
    populate_widgets_from_layout(services->layout_root, services->widgets.items, services->widgets.count);
    apply_widget_padding_scale(&services->widgets, services->ui_scale);
    scroll_rebuild(services->scroll, services->widgets.items, services->widgets.count, ratio);
}

void ui_frame_update(AppServices* services) {
    if (!services || !services->widgets.items) return;
    update_widget_bindings(services->ui_root, services->model);
    populate_widgets_from_layout(services->layout_root, services->widgets.items, services->widgets.count);
    apply_widget_padding_scale(&services->widgets, services->ui_scale);
    scroll_apply_offsets(services->scroll, services->widgets.items, services->widgets.count);
}

void ui_handle_mouse_button(AppServices* services, double mx, double my, int button, int action) {
    if (!services || !services->widgets.items) return;
    int pressed = (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS);
    if (scroll_handle_mouse_button(services->scroll, services->widgets.items, services->widgets.count, (float)mx, (float)my, pressed)) return;
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) return;
    for (size_t i = 0; i < services->widgets.count; i++) {
        Widget* w = &services->widgets.items[i];
        if ((w->type == W_BUTTON || w->type == W_CHECKBOX || w->type == W_HSLIDER) && point_in_widget(w, mx, my)) {
            if (w->type == W_HSLIDER) {
                apply_slider_action(w, services->model, (float)mx);
            } else {
                apply_click_action(w, services->model);
            }
            break;
        }
    }
}

void ui_handle_scroll(AppServices* services, double mx, double my, double yoff) {
    if (!services || !services->widgets.items) return;
    scroll_handle_event(services->scroll, services->widgets.items, services->widgets.count, (float)mx, (float)my, yoff);
}

void ui_handle_cursor(AppServices* services, double x, double y) {
    if (!services || !services->widgets.items) return;
    scroll_handle_cursor(services->scroll, services->widgets.items, services->widgets.count, (float)x, (float)y);
}

void ui_service_dispose(AppServices* services) {
    if (!services) return;

    if (services->styles) {
        free_styles(services->styles);
        services->styles = NULL;
    }
    if (services->widgets.items) {
        free_widgets(services->widgets);
        services->widgets = (WidgetArray){0};
    }
    if (services->ui_root) {
        free_ui_tree(services->ui_root);
        services->ui_root = NULL;
    }
    if (services->layout_root) {
        free_layout_tree(services->layout_root);
        services->layout_root = NULL;
    }
    if (services->scroll) {
        scroll_free(services->scroll);
        services->scroll = NULL;
    }
}
