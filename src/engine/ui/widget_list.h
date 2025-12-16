#ifndef UI_WIDGET_LIST_H
#define UI_WIDGET_LIST_H

#include "engine/ui/layout_tree.h"

// --- Specific Widget Data Structures ---

typedef struct WidgetTextData {
    char* text;
    char* text_binding;
    Color color; // text_color
    char* click_binding;
    char* click_value;
} WidgetTextData;

typedef struct WidgetValueData {
    float min, max, value;
    Color knob_color; // Was text_color
    char* value_binding;
} WidgetValueData;

typedef struct WidgetScrollData {
    int enabled;           // scrollbar_enabled
    float width;           // scrollbar_width
    Color track_color;     // scrollbar_track_color
    Color thumb_color;     // scrollbar_thumb_color
    int show;              // show_scrollbar
    float viewport_size;   // scroll_viewport
    float content_size;    // scroll_content
} WidgetScrollData;

typedef struct WidgetCheckboxData {
    char* text; // label
    Color color; // text_color
    float value; // 0.0 or 1.0
    char* value_binding;
    char* click_binding;
    char* click_value;
} WidgetCheckboxData;

// --- Main Widget Structure ---

typedef struct Widget {
    // -- Common Identity & Layout --
    WidgetType type;
    char* id;
    Rect rect;
    Rect floating_rect;
    int has_floating_rect;
    
    // -- Common Rendering --
    int z_index;
    int base_z_index;
    int z_group;
    Color color;            // Background color
    
    // -- Common Styling --
    float base_padding;
    float padding;
    float base_border_thickness;
    float border_thickness;
    Color border_color;
    
    // -- Common Scroll Context --
    char* scroll_area;
    float scroll_offset;    // Current offset applied to this widget
    
    // -- Common Clipping --
    int has_clip;
    Rect clip;
    int clip_to_viewport;
    int has_clip_to_viewport;

    // -- Common Interaction Flags --
    char* docking;
    int resizable;
    int draggable;
    int modal;
    int has_resizable;
    int has_draggable;
    int has_modal;
    int has_floating_min, has_floating_max;
    float floating_min_w, floating_min_h;
    float floating_max_w, floating_max_h;
    char* on_focus;

    // -- Polymorphic Data --
    union {
        WidgetTextData label;      // Label, Button
        WidgetValueData slider;    // Slider, Progress
        WidgetCheckboxData checkbox; // Checkbox
        WidgetScrollData scroll;   // Scrollbar
    } data;
    
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