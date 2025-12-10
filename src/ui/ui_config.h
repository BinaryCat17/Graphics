#ifndef UI_CONFIG_H
#define UI_CONFIG_H

#include <stddef.h>
#include "core/Graphics.h"
#include "cad/cad_scene.h"
#include "config/config_document.h"

#define UI_Z_ORDER_SCALE 1000

typedef struct { float x, y, w, h; } Rect;
typedef enum { W_PANEL, W_LABEL, W_BUTTON, W_HSLIDER, W_RECT, W_SPACER, W_CHECKBOX, W_PROGRESS } WidgetType;

typedef enum {
    UI_LAYOUT_NONE,
    UI_LAYOUT_ROW,
    UI_LAYOUT_COLUMN,
    UI_LAYOUT_TABLE,
    UI_LAYOUT_ABSOLUTE
} LayoutType;

typedef struct ModelEntry {
    char* key;
    char* string_value;
    float number_value;
    int is_string;
    struct ModelEntry* next;
} ModelEntry;

typedef struct Model {
    ModelEntry* entries;
    char* store;
    char* key;
    char* source_path;
    const ConfigDocument* source_doc;
} Model;

typedef struct Style {
    char* name;
    Color background;
    Color text;
    Color border_color;
    Color scrollbar_track_color;
    Color scrollbar_thumb_color;
    float padding;
    float border_thickness;
    float scrollbar_width;
    int has_scrollbar_width;
    struct Style* next;
} Style;

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
    char* on_focus;
    struct UiNode* children;
    size_t child_count;
} UiNode;

typedef struct LayoutNode {
    const UiNode* source;
    Rect rect;
    Rect base_rect;
    Rect local_rect;
    Vec2 transform;
    Rect clip;
    int has_clip;
    struct LayoutNode* children;
    size_t child_count;
} LayoutNode;

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
    float scroll_viewport;
    float scroll_content;
    int show_scrollbar;
} Widget;

typedef struct WidgetArray {
    Widget* items;
    size_t count;
} WidgetArray;

Model* ui_config_load_model(const ConfigDocument* doc);
Style* ui_config_load_styles(const ConfigNode* root);
UiNode* ui_config_load_layout(const ConfigNode* root, const Model* model, const Style* styles, const char* font_path, const Scene* scene);

void update_widget_bindings(UiNode* root, const Model* model);

LayoutNode* build_layout_tree(const UiNode* root);
void free_layout_tree(LayoutNode* root);
void measure_layout(LayoutNode* root);
void assign_layout(LayoutNode* root, float origin_x, float origin_y);
void capture_layout_base(LayoutNode* root);
size_t count_layout_widgets(const LayoutNode* root);
void populate_widgets_from_layout(const LayoutNode* root, Widget* widgets, size_t widget_count);
WidgetArray materialize_widgets(const LayoutNode* root);
void apply_widget_padding_scale(WidgetArray* widgets, float scale);

float model_get_number(const Model* model, const char* key, float fallback);
const char* model_get_string(const Model* model, const char* key, const char* fallback);
void model_set_number(Model* model, const char* key, float value);
void model_set_string(Model* model, const char* key, const char* value);
int save_model(const Model* model);

void free_model(Model* model);
void free_styles(Style* styles);
void free_widgets(WidgetArray widgets);
void free_ui_tree(UiNode* root);

#endif // UI_CONFIG_H
