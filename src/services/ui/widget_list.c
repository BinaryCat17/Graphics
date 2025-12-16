#include "services/ui/widget_list.h"

#include <stdlib.h>
#include <string.h> // For memset

#include "services/ui/model_style.h"

size_t count_layout_widgets(const LayoutNode* root) {
    if (!root) return 0;
    size_t total = 0;
    if (root->source && (root->source->layout == UI_LAYOUT_NONE || root->source->widget_type == W_SCROLLBAR)) total += 1;
    for (size_t i = 0; i < root->child_count; i++) total += count_layout_widgets(&root->children[i]);
    return total;
}

static int compute_z_index(const UiNode* source, size_t appearance_order) {
    int explicit_z = (source && source->has_z_index) ? source->z_index : 0;
    int group = (source && source->has_z_group) ? source->z_group : 0;
    int composite = explicit_z + group * UI_Z_ORDER_SCALE;
    return composite * UI_Z_ORDER_SCALE + (int)appearance_order;
}

static void populate_widgets_recursive(const LayoutNode* node, Widget* widgets, size_t widget_count, size_t* idx, size_t* order,
                                       char* inherited_scroll_area) {
    if (!node || !widgets || !idx || *idx >= widget_count || !order) return;
    char* active_scroll_area = node->source && node->source->scroll_area ? node->source->scroll_area : inherited_scroll_area;
    const Style* default_style = ui_default_style();
    if (node->source && (node->source->layout == UI_LAYOUT_NONE || node->source->widget_type == W_SCROLLBAR)) {
        Widget* w = &widgets[*idx];
        (*idx)++;
        size_t appearance_order = *order;
        (*order)++;
        
        // --- Common Fields ---
        w->type = node->source->widget_type;
        // Fix for Panels being marked correctly in flat list if they are layout nodes
        if (node->source->layout != UI_LAYOUT_NONE) {
             w->type = (node->source->widget_type == W_SCROLLBAR) ? W_SCROLLBAR : W_PANEL;
        }

        w->rect = node->rect;
        w->id = node->source->id;
        
        // Z-Index
        w->z_index = compute_z_index(node->source, appearance_order);
        w->base_z_index = w->z_index;
        w->z_group = node->source->has_z_group ? node->source->z_group : 0;
        
        // Appearance
        w->color = node->source->color;
        w->base_border_thickness = node->source->border_thickness;
        w->border_thickness = w->base_border_thickness;
        w->border_color = node->source->border_color;
        w->base_padding = (node->source->has_padding_override ? node->source->padding_override : (node->source->style ? node->source->style->padding : default_style->padding)) + w->base_border_thickness;
        w->padding = w->base_padding;
        
        // Interaction / Flags
        w->docking = node->source->docking;
        w->resizable = node->source->resizable;
        w->has_resizable = node->source->has_resizable;
        w->draggable = node->source->draggable;
        w->has_draggable = node->source->has_draggable;
        w->modal = node->source->modal;
        w->has_modal = node->source->has_modal;
        w->on_focus = node->source->on_focus;
        
        // Floating
        w->has_floating_rect = node->source->has_floating_rect;
        w->floating_rect = node->source->floating_rect;
        w->floating_min_w = node->source->floating_min_w;
        w->floating_min_h = node->source->floating_min_h;
        w->floating_max_w = node->source->floating_max_w;
        w->floating_max_h = node->source->floating_max_h;
        w->has_floating_min = node->source->has_floating_min;
        w->has_floating_max = node->source->has_floating_max;

        // Scroll Context
        w->scroll_area = node->source->scroll_area ? node->source->scroll_area : active_scroll_area;
        w->scroll_offset = 0.0f; // Reset runtime state
        
        // Clipping
        w->clip_to_viewport = node->source->clip_to_viewport;
        w->has_clip_to_viewport = node->source->has_clip_to_viewport;
        if (!w->has_clip_to_viewport && w->type == W_SCROLLBAR) {
            w->clip_to_viewport = 0;
        }
        w->has_clip = 0; // Runtime calculated
        
        // --- Polymorphic Data Population ---
        // Clear union to avoid garbage
        memset(&w->data, 0, sizeof(w->data));

        switch (w->type) {
            case W_LABEL:
            case W_BUTTON:
                w->data.label.text = node->source->text;
                w->data.label.text_binding = node->source->text_binding;
                w->data.label.color = node->source->text_color;
                w->data.label.click_binding = node->source->click_binding;
                w->data.label.click_value = node->source->click_value;
                break;
                
            case W_HSLIDER:
            case W_PROGRESS:
                w->data.slider.min = node->source->minv;
                w->data.slider.max = node->source->maxv;
                w->data.slider.value = node->source->value;
                w->data.slider.knob_color = node->source->text_color; // Map text_color to knob_color
                w->data.slider.value_binding = node->source->value_binding;
                break;
                
            case W_CHECKBOX:
                w->data.checkbox.text = node->source->text;
                w->data.checkbox.color = node->source->text_color;
                w->data.checkbox.value = node->source->value;
                w->data.checkbox.value_binding = node->source->value_binding;
                w->data.checkbox.click_binding = node->source->click_binding;
                w->data.checkbox.click_value = node->source->click_value;
                break;
                
            case W_SCROLLBAR:
                w->data.scroll.enabled = node->source->scrollbar_enabled;
                w->data.scroll.width = node->source->scrollbar_width;
                w->data.scroll.track_color = node->source->scrollbar_track_color;
                w->data.scroll.thumb_color = node->source->scrollbar_thumb_color;
                w->data.scroll.show = 0; // Runtime
                w->data.scroll.viewport_size = 0.0f; // Runtime
                w->data.scroll.content_size = 0.0f; // Runtime
                break;
                
            default:
                break;
        }
    }
    for (size_t i = 0; i < node->child_count; i++) populate_widgets_recursive(&node->children[i], widgets, widget_count, idx, order, active_scroll_area);
}

void populate_widgets_from_layout(const LayoutNode* root, Widget* widgets, size_t widget_count) {
    size_t idx = 0;
    size_t order = 0;
    populate_widgets_recursive(root, widgets, widget_count, &idx, &order, NULL);
}

WidgetArray materialize_widgets(const LayoutNode* root) {
    WidgetArray arr = {0};
    size_t count = count_layout_widgets(root);
    if (count == 0) return arr;
    Widget* widgets = (Widget*)calloc(count, sizeof(Widget));
    if (!widgets) return arr;
    populate_widgets_from_layout(root, widgets, count);
    arr.items = widgets;
    arr.count = count;
    return arr;
}

void apply_widget_padding_scale(WidgetArray* widgets, float scale) {
    if (!widgets) return;
    for (size_t i = 0; i < widgets->count; i++) {
        widgets->items[i].padding = widgets->items[i].base_padding * scale;
        widgets->items[i].border_thickness = widgets->items[i].base_border_thickness * scale;
    }
}

void free_widgets(WidgetArray widgets) {
    if (!widgets.items) return;
    free(widgets.items);
}
