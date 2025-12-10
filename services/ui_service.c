#include "ui_service.h"

#include <GLFW/glfw3.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "services/service_events.h"
#include "ui/scroll.h"

static float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

void ui_context_init(UiContext* ui) {
    if (!ui) return;
    memset(ui, 0, sizeof(UiContext));
}

float ui_compute_scale(const UiContext* ui, float target_w, float target_h) {
    if (!ui || ui->base_w <= 0.0f || ui->base_h <= 0.0f) return 1.0f;
    float ui_scale = fminf(target_w / ui->base_w, target_h / ui->base_h);
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
        snprintf(buf, sizeof(buf), "%s: %.0f%%", w->id,
                 clamp01((new_val - w->minv) / (denom != 0.0f ? denom : 1.0f)) * 100.0f);
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

bool ui_build(UiContext* ui, const CoreContext* core) {
    if (!ui || !core || !core->model) return false;

    ui->styles = parse_styles_config(core->assets.styles_doc.root);
    ui->ui_root = parse_layout_config(core->assets.layout_doc.root, core->model, ui->styles, core->assets.font_path,
                                      &core->scene);
    if (!ui->ui_root) return false;

    ui->layout_root = build_layout_tree(ui->ui_root);
    if (!ui->layout_root) return false;

    ui->model = core->model;

    measure_layout(ui->layout_root);
    assign_layout(ui->layout_root, 0.0f, 0.0f);
    capture_layout_base(ui->layout_root);
    ui->base_w = ui->layout_root->base_rect.w > 1.0f ? ui->layout_root->base_rect.w : 1024.0f;
    ui->base_h = ui->layout_root->base_rect.h > 1.0f ? ui->layout_root->base_rect.h : 640.0f;

    return true;
}

bool ui_prepare_runtime(UiContext* ui, const CoreContext* core, float ui_scale, StateManager* state_manager,
                       int ui_type_id) {
    if (!ui || !ui->layout_root) return false;
    scale_layout(ui->layout_root, ui_scale);

    ui->widgets = materialize_widgets(ui->layout_root);
    apply_widget_padding_scale(&ui->widgets, ui_scale);
    update_widget_bindings(ui->ui_root, ui->model);
    populate_widgets_from_layout(ui->layout_root, ui->widgets.items, ui->widgets.count);
    ui->scroll = scroll_init(ui->widgets.items, ui->widgets.count);
    if (!ui->scroll) return false;

    ui->ui_scale = ui_scale;
    ui->state_manager = state_manager;
    ui->ui_type_id = ui_type_id;

    if (state_manager && ui_type_id >= 0) {
        UiRuntimeComponent component = {.ui = ui, .widgets = ui->widgets};
        state_manager_publish(state_manager, STATE_EVENT_COMPONENT_ADDED, ui_type_id, "active", &component,
                              sizeof(component));
    }
    return true;
}

static void on_model_event(const StateEvent* event, void* user_data) {
    UiContext* ui = (UiContext*)user_data;
    if (!ui || !event || !event->payload) return;
    const ModelComponent* component = (const ModelComponent*)event->payload;
    ui->model = component->model;
}

bool ui_service_subscribe(UiContext* ui, StateManager* state_manager, int model_type_id) {
    if (!ui || !state_manager || model_type_id < 0) return false;
    return state_manager_subscribe(state_manager, model_type_id, "active", on_model_event, ui) == 0;
}

void ui_refresh_layout(UiContext* ui, float new_scale) {
    if (!ui || !ui->layout_root || !ui->widgets.items) return;
    if (new_scale <= 0.0f) return;
    float ratio = ui->ui_scale > 0.0f ? new_scale / ui->ui_scale : 1.0f;
    ui->ui_scale = new_scale;
    scale_layout(ui->layout_root, ui->ui_scale);
    populate_widgets_from_layout(ui->layout_root, ui->widgets.items, ui->widgets.count);
    apply_widget_padding_scale(&ui->widgets, ui->ui_scale);
    scroll_rebuild(ui->scroll, ui->widgets.items, ui->widgets.count, ratio);
}

void ui_frame_update(UiContext* ui) {
    if (!ui || !ui->widgets.items || !ui->model) return;
    update_widget_bindings(ui->ui_root, ui->model);
    populate_widgets_from_layout(ui->layout_root, ui->widgets.items, ui->widgets.count);
    apply_widget_padding_scale(&ui->widgets, ui->ui_scale);
    scroll_apply_offsets(ui->scroll, ui->widgets.items, ui->widgets.count);

    if (ui->state_manager && ui->ui_type_id >= 0) {
        UiRuntimeComponent component = {.ui = ui, .widgets = ui->widgets};
        state_manager_publish(ui->state_manager, STATE_EVENT_COMPONENT_UPDATED, ui->ui_type_id, "active", &component,
                              sizeof(component));
    }
}

void ui_handle_mouse_button(UiContext* ui, double mx, double my, int button, int action) {
    if (!ui || !ui->widgets.items || !ui->model) return;
    int pressed = (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS);
    if (scroll_handle_mouse_button(ui->scroll, ui->widgets.items, ui->widgets.count, (float)mx, (float)my, pressed))
        return;
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) return;
    for (size_t i = 0; i < ui->widgets.count; i++) {
        Widget* w = &ui->widgets.items[i];
        if ((w->type == W_BUTTON || w->type == W_CHECKBOX || w->type == W_HSLIDER) && point_in_widget(w, mx, my)) {
            if (w->type == W_HSLIDER) {
                apply_slider_action(w, ui->model, (float)mx);
            } else {
                apply_click_action(w, ui->model);
            }
            break;
        }
    }
}

void ui_handle_scroll(UiContext* ui, double mx, double my, double yoff) {
    if (!ui || !ui->widgets.items) return;
    scroll_handle_event(ui->scroll, ui->widgets.items, ui->widgets.count, (float)mx, (float)my, yoff);
}

void ui_handle_cursor(UiContext* ui, double x, double y) {
    if (!ui || !ui->widgets.items) return;
    scroll_handle_cursor(ui->scroll, ui->widgets.items, ui->widgets.count, (float)x, (float)y);
}

void ui_context_dispose(UiContext* ui) {
    if (!ui) return;

    if (ui->styles) {
        free_styles(ui->styles);
        ui->styles = NULL;
    }
    if (ui->widgets.items) {
        free_widgets(ui->widgets);
        ui->widgets = (WidgetArray){0};
    }
    if (ui->ui_root) {
        free_ui_tree(ui->ui_root);
        ui->ui_root = NULL;
    }
    if (ui->layout_root) {
        free_layout_tree(ui->layout_root);
        ui->layout_root = NULL;
    }
    if (ui->scroll) {
        scroll_free(ui->scroll);
        ui->scroll = NULL;
    }
}
