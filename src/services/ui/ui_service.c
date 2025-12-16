#include "services/ui/ui_service.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "app/app_services.h"
#include "core/platform/platform.h"
#include "core/service_manager/service_events.h"
#include "services/ui/compositor.h"
#include "services/ui/scroll.h"

static float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
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
    if (!w || w->type != W_HSLIDER) return;
    
    float local_t = (float)((mx - w->rect.x) / w->rect.w);
    local_t = clamp01(local_t);
    float denom = (w->data.slider.max - w->data.slider.min);
    float new_val = denom != 0.0f ? (w->data.slider.min + local_t * denom) : w->data.slider.min;
    
    w->data.slider.value = new_val;
    
    if (w->data.slider.value_binding) {
        model_set_number(model, w->data.slider.value_binding, new_val);
    }
    if (w->id) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%s: %.0f%%", w->id,
                 clamp01((new_val - w->data.slider.min) / (denom != 0.0f ? denom : 1.0f)) * 100.0f);
        model_set_string(model, "sliderState", buf);
    }
}

static int point_in_widget(const Widget* w, double mx, double my) {
    if (!w) return 0;
    float x = w->rect.x;
    float y_offset = w->type == W_SCROLLBAR ? 0.0f : w->scroll_offset;
    float y = w->rect.y + y_offset;
    return mx >= x && mx <= x + w->rect.w && my >= y && my <= y + w->rect.h;
}

static int point_in_rect(const Rect* r, double mx, double my) {
    if (!r) return 0;
    return mx >= r->x && mx <= r->x + r->w && my >= r->y && my <= r->y + r->h;
}

static Widget* pick_widget_at(const UiContext* ui, double mx, double my) {
    if (!ui || !ui->display_list.items) return NULL;
    for (size_t i = ui->display_list.count; i > 0; --i) {
        const DisplayItem* item = &ui->display_list.items[i - 1];
        Widget* w = item->widget;
        if (!w) continue;
        int inside = 1;
        for (size_t c = 0; c < item->clip_depth && inside; ++c) {
            inside = point_in_rect(&item->clip_stack[c], mx, my);
        }
        if (!inside) continue;
        if (point_in_widget(w, mx, my)) return w;
    }
    return NULL;
}

static void apply_click_action(Widget* w, Model* model) {
    if (!w || !model) return;
    
    if (w->type == W_BUTTON) {
        if (w->data.label.click_binding) {
            const char* payload = w->data.label.click_value ? w->data.label.click_value : (w->id ? w->id : w->data.label.text);
            if (payload) model_set_string(model, w->data.label.click_binding, payload);
        }
    } else if (w->type == W_CHECKBOX) {
        float new_val = (w->data.checkbox.value > 0.5f) ? 0.0f : 1.0f;
        w->data.checkbox.value = new_val;
        
        if (w->data.checkbox.value_binding) {
            model_set_number(model, w->data.checkbox.value_binding, new_val);
        }
        if (w->data.checkbox.click_binding) {
            const char* on_payload = w->data.checkbox.click_value ? w->data.checkbox.click_value : "On";
            const char* payload = (new_val > 0.5f) ? on_payload : "Off";
            model_set_string(model, w->data.checkbox.click_binding, payload);
        }
    }
}

bool ui_build(UiContext* ui, const Assets* assets, const Scene* scene, Model* model) {
    if (!ui || !assets || !model) {
        fprintf(stderr, "UI build received invalid context or missing model.\n");
        return false;
    }

    ui->styles = ui_config_load_styles(assets->ui_doc.root);
    if (!ui->styles) {
        fprintf(stderr, "Failed to parse UI styles from %s.\n",
                assets->ui_doc.source_path ? assets->ui_doc.source_path : "(unknown)");
        return false;
    }

    ui->ui_root = ui_config_load_layout(&assets->ui_doc, model, ui->styles, scene);
    if (!ui->ui_root) {
        fprintf(stderr, "Failed to parse UI layout configuration.\n");
        return false;
    }

    ui->layout_root = build_layout_tree(ui->ui_root);
    if (!ui->layout_root) {
        fprintf(stderr, "Failed to build UI layout tree.\n");
        return false;
    }

    ui->model = model;

    measure_layout(ui->layout_root, assets->font_path);
    assign_layout(ui->layout_root, 0.0f, 0.0f);
    capture_layout_base(ui->layout_root);
    ui->base_w = ui->layout_root->base_rect.w > 1.0f ? ui->layout_root->base_rect.w : 1024.0f;
    ui->base_h = ui->layout_root->base_rect.h > 1.0f ? ui->layout_root->base_rect.h : 640.0f;

    return true;
}

bool ui_prepare_runtime(UiContext* ui, float ui_scale, StateManager* state_manager,
                       int ui_type_id) {
    if (!ui || !ui->layout_root) {
        fprintf(stderr, "UI runtime preparation received invalid layout.\n");
        return false;
    }
    scale_layout(ui->layout_root, ui_scale);

    ui->widgets = materialize_widgets(ui->layout_root);
    apply_widget_padding_scale(&ui->widgets, ui_scale);
    update_widget_bindings(ui->ui_root, ui->model);
    populate_widgets_from_layout(ui->layout_root, ui->widgets.items, ui->widgets.count);
    ui->scroll = scroll_init(ui->widgets.items, ui->widgets.count);
    if (!ui->scroll) {
        fprintf(stderr, "Failed to initialize scroll subsystem for UI.\n");
        return false;
    }
    ui->display_list = ui_compositor_build(ui->layout_root, ui->widgets.items, ui->widgets.count);

    ui->ui_scale = ui_scale;
    ui->state_manager = state_manager;
    ui->ui_type_id = ui_type_id;

    if (state_manager && ui_type_id >= 0) {
        UiRuntimeComponent component = {.ui = ui, .widgets = ui->widgets, .display_list = ui->display_list};
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
    if (!ui || !state_manager || model_type_id < 0) {
        fprintf(stderr, "UI service subscribe called with invalid arguments.\n");
        return false;
    }
    return state_manager_subscribe(state_manager, model_type_id, "active", on_model_event, ui) != 0;
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
    DisplayList old_list = ui->display_list;
    ui->display_list = ui_compositor_build(ui->layout_root, ui->widgets.items, ui->widgets.count);

    if (ui->state_manager && ui->ui_type_id >= 0) {
        UiRuntimeComponent component = {.ui = ui, .widgets = ui->widgets, .display_list = ui->display_list};
        state_manager_publish(ui->state_manager, STATE_EVENT_COMPONENT_UPDATED, ui->ui_type_id, "active", &component,
                              sizeof(component));
    }

    ui_compositor_free(old_list);
}

void ui_frame_update(UiContext* ui) {
    if (!ui || !ui->widgets.items || !ui->model) return;
    update_widget_bindings(ui->ui_root, ui->model);
    populate_widgets_from_layout(ui->layout_root, ui->widgets.items, ui->widgets.count);
    apply_widget_padding_scale(&ui->widgets, ui->ui_scale);
    scroll_update(ui->scroll, 0.016f);
    scroll_apply_offsets(ui->scroll, ui->widgets.items, ui->widgets.count);
    DisplayList old_list = ui->display_list;
    ui->display_list = ui_compositor_build(ui->layout_root, ui->widgets.items, ui->widgets.count);

    if (ui->state_manager && ui->ui_type_id >= 0) {
        UiRuntimeComponent component = {.ui = ui, .widgets = ui->widgets, .display_list = ui->display_list};
        state_manager_publish(ui->state_manager, STATE_EVENT_COMPONENT_UPDATED, ui->ui_type_id, "active", &component,
                              sizeof(component));
    }

    ui_compositor_free(old_list);
}

void ui_handle_mouse_button(UiContext* ui, double mx, double my, int button, int action) {
    if (!ui || !ui->widgets.items || !ui->model) return;
    int pressed = (button == PLATFORM_MOUSE_BUTTON_LEFT && action == PLATFORM_PRESS);
    if (scroll_handle_mouse_button(ui->scroll, ui->widgets.items, ui->widgets.count, (float)mx, (float)my, pressed))
        return;
    if (button != PLATFORM_MOUSE_BUTTON_LEFT || action != PLATFORM_PRESS) return;
    Widget* w = pick_widget_at(ui, mx, my);
    if (!w) return;
    if (w->type == W_HSLIDER) {
        apply_slider_action(w, ui->model, (float)mx);
    } else if (w->type == W_BUTTON || w->type == W_CHECKBOX) {
        apply_click_action(w, ui->model);
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

typedef struct UiServiceContext {
    UiContext* ui;
    const Assets* assets;
    const Scene* scene;
    Model* model;
} UiServiceContext;

static UiServiceContext g_ui_service_context_storage;

static UiServiceContext* ui_service_context(void) {
    return &g_ui_service_context_storage;
}

static bool ui_service_init(void* ptr, const ServiceConfig* config) {
    AppServices* services = (AppServices*)ptr;
    (void)config;
    if (!services) {
        fprintf(stderr, "UI service init received null services context.\n");
        return false;
    }
    
    UiServiceContext* context = ui_service_context();
    context->ui = &services->ui;
    context->assets = &services->core.assets;
    context->scene = &services->core.scene;
    context->model = services->core.model;
    
    ui_context_init(&services->ui);
    return ui_service_subscribe(&services->ui, &services->state_manager, services->type_id_model);
}

static bool ui_service_start(void* ptr, const ServiceConfig* config) {
    (void)ptr; // Ignore AppServices*
    (void)config;
    
    UiServiceContext* context = ui_service_context();
    if (!context || !context->ui || !context->assets || !context->scene || !context->model) {
        fprintf(stderr, "UI service start missing context or dependencies.\n");
        return false;
    }
    
    if (!ui_build(context->ui, context->assets, context->scene, context->model)) {
        fprintf(stderr, "UI service failed to build UI.\n");
        return false;
    }
    return true;
}

static void ui_service_stop(void* ptr) {
    (void)ptr;
    UiServiceContext* context = ui_service_context();
    if (context && context->ui) {
        ui_context_dispose(context->ui);
    }
}

static const char* g_ui_dependencies[] = {"scene"};

static ServiceDescriptor g_ui_service_descriptor = {
    .name = "ui",
    .dependencies = g_ui_dependencies,
    .dependency_count = sizeof(g_ui_dependencies) / sizeof(g_ui_dependencies[0]),
    .init = ui_service_init,
    .start = ui_service_start,
    .stop = ui_service_stop,
    .context = &g_ui_service_context_storage,
    .thread_handle = NULL,
};

const ServiceDescriptor* ui_service_descriptor(void) { 
    return &g_ui_service_descriptor; 
}