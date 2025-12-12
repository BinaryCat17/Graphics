#ifndef UI_WIDGET_LIST_H
#define UI_WIDGET_LIST_H

#include "ui/layout_tree.h"

typedef struct Widget {
    WidgetType type;
    Rect rect;
    Rect floating_rect;
    float scroll_offset;
    int z_index;
    int base_z_index;
    int z_group;
    Color color;
    Color text_color;
    float base_padding;
    float padding;
    float base_border_thickness;
    float border_thickness;
    Color border_color;
    char* text; /* for labels/buttons */
    char* text_binding;
    char* value_binding;
    char* click_binding;
    char* click_value;
    float minv, maxv, value;
    char* id;
    char* docking;
    int resizable;
    int draggable;
    int modal;
    int has_resizable;
    int has_draggable;
    int has_modal;
    int has_floating_rect;
    float floating_min_w, floating_min_h;
    float floating_max_w, floating_max_h;
    int has_floating_min, has_floating_max;
    char* on_focus;
    char* scroll_area;
    int scroll_static;
    int scrollbar_enabled;
    float scrollbar_width;
    Color scrollbar_track_color;
    Color scrollbar_thumb_color;
    int has_clip;
    Rect clip;
    int clip_to_viewport;
    int has_clip_to_viewport;
    float scroll_viewport;
    float scroll_content;
    int show_scrollbar;
} Widget;

typedef struct WidgetArray {
    Widget* items;
    size_t count;
} WidgetArray;

size_t count_layout_widgets(const LayoutNode* root);
void populate_widgets_from_layout(const LayoutNode* root, Widget* widgets, size_t widget_count);
WidgetArray materialize_widgets(const LayoutNode* root);
void apply_widget_padding_scale(WidgetArray* widgets, float scale);
void free_widgets(WidgetArray widgets);

#endif // UI_WIDGET_LIST_H
