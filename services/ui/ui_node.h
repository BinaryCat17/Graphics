#ifndef UI_UI_NODE_H
#define UI_UI_NODE_H

#include <stddef.h>
#include "config/config_document.h"
#include "scene/cad_scene.h"
#include "ui/model_style.h"

#define UI_Z_ORDER_SCALE 1000

typedef struct { float x, y, w, h; } Rect;

typedef enum { W_PANEL, W_LABEL, W_BUTTON, W_HSLIDER, W_RECT, W_SPACER, W_CHECKBOX, W_PROGRESS, W_SCROLLBAR } WidgetType;

typedef enum {
    UI_LAYOUT_NONE,
    UI_LAYOUT_ROW,
    UI_LAYOUT_COLUMN,
    UI_LAYOUT_TABLE,
    UI_LAYOUT_ABSOLUTE
} LayoutType;

typedef struct UiNode {
    char* type;
    LayoutType layout;
    WidgetType widget_type;
    Rect rect;
    Rect floating_rect;
    int has_x, has_y, has_w, has_h;
    int has_floating_rect;
    int z_index;
    int has_z_index;
    int z_group;
    int has_z_group;
    float spacing;
    int has_spacing;
    int columns;
    int has_columns;
    const Style* style;
    float padding_override;
    int has_padding_override;
    float border_thickness;
    int has_border_thickness;
    int has_border_color;
    Color border_color;
    Color color;
    Color text_color;
    int has_color;
    int has_text_color;
    char* style_name;
    char* use;
    char* id;
    char* text;
    char* text_binding;
    char* value_binding;
    char* click_binding;
    char* click_value;
    float minv, maxv, value;
    int has_min, has_max, has_value;
    float min_w, min_h;
    int has_min_w, has_min_h;
    float max_w, max_h;
    int has_max_w, has_max_h;
    float floating_min_w, floating_min_h;
    float floating_max_w, floating_max_h;
    int has_floating_min, has_floating_max;
    char* scroll_area;
    int scroll_static;
    int scrollbar_enabled;
    float scrollbar_width;
    int has_scrollbar_width;
    Color scrollbar_track_color;
    Color scrollbar_thumb_color;
    int has_scrollbar_track_color;
    int has_scrollbar_thumb_color;
    char* docking;
    int resizable;
    int has_resizable;
    int draggable;
    int has_draggable;
    int modal;
    int has_modal;
    int clip_to_viewport;
    int has_clip_to_viewport;
    char* on_focus;
    struct UiNode* children;
    size_t child_count;
} UiNode;

UiNode* ui_config_load_layout(const ConfigDocument* doc, const Model* model, const Style* styles, const Scene* scene);
void update_widget_bindings(UiNode* root, const Model* model);
void free_ui_tree(UiNode* node);

#endif // UI_UI_NODE_H
