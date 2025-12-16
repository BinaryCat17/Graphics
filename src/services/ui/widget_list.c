#include "services/ui/widget_list.h"

#include <stdlib.h>

#include "services/ui/model_style.h"

size_t count_layout_widgets(const LayoutNode* root) {
    if (!root) return 0;
    size_t total = 0;
    if (root->source && (root->source->layout == UI_LAYOUT_NONE || root->source->scroll_static)) total += 1;
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
    if (node->source && (node->source->layout == UI_LAYOUT_NONE || node->source->scroll_static)) {
        Widget* w = &widgets[*idx];
        (*idx)++;
        size_t appearance_order = *order;
        (*order)++;
        w->type = node->source->widget_type;
        w->rect = node->rect;
        w->scroll_offset = 0.0f;
        w->z_index = compute_z_index(node->source, appearance_order);
        w->base_z_index = w->z_index;
        w->z_group = node->source->has_z_group ? node->source->z_group : 0;
        w->color = node->source->color;
        w->text_color = node->source->text_color;
        w->base_border_thickness = node->source->border_thickness;
        w->border_thickness = w->base_border_thickness;
        w->border_color = node->source->border_color;
        w->scrollbar_enabled = node->source->scrollbar_enabled;
        w->scrollbar_width = node->source->scrollbar_width;
        w->scrollbar_track_color = node->source->scrollbar_track_color;
        w->scrollbar_thumb_color = node->source->scrollbar_thumb_color;
        w->base_padding = (node->source->has_padding_override ? node->source->padding_override : (node->source->style ? node->source->style->padding : default_style->padding)) + w->base_border_thickness;
        w->padding = w->base_padding;
        w->text = node->source->text;
        w->text_binding = node->source->text_binding;
        w->value_binding = node->source->value_binding;
        w->click_binding = node->source->click_binding;
        w->click_value = node->source->click_value;
        w->minv = node->source->minv;
        w->maxv = node->source->maxv;
        w->value = node->source->value;
        w->id = node->source->id;
        w->docking = node->source->docking;
        w->resizable = node->source->resizable;
        w->has_resizable = node->source->has_resizable;
        w->draggable = node->source->draggable;
        w->has_draggable = node->source->has_draggable;
        w->modal = node->source->modal;
        w->has_floating_rect = node->source->has_floating_rect;
        w->floating_rect = node->source->floating_rect;
        w->floating_min_w = node->source->floating_min_w;
        w->floating_min_h = node->source->floating_min_h;
        w->floating_max_w = node->source->floating_max_w;
        w->floating_max_h = node->source->floating_max_h;
        w->has_floating_min = node->source->has_floating_min;
        w->has_floating_max = node->source->has_floating_max;
        w->on_focus = node->source->on_focus;
        w->scroll_area = node->source->scroll_area ? node->source->scroll_area : active_scroll_area;
        w->scroll_static = node->source->scroll_static;
        w->clip_to_viewport = node->source->clip_to_viewport;
        w->has_clip_to_viewport = node->source->has_clip_to_viewport;
        if (!w->has_clip_to_viewport && w->scroll_static) {
            w->clip_to_viewport = 0;
        }
        w->scroll_viewport = 0.0f;
        w->scroll_content = 0.0f;
        w->show_scrollbar = 0;
        if (node->source->layout != UI_LAYOUT_NONE) {
            w->type = node->source->widget_type == W_PANEL ? node->source->widget_type : W_PANEL;
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
