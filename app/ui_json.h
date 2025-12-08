#ifndef UI_JSON_H
#define UI_JSON_H

#include <stddef.h>

typedef struct { float x, y, w, h; } Rect;
typedef struct { float r, g, b, a; } Color;
typedef enum { W_PANEL, W_LABEL, W_BUTTON, W_HSLIDER } WidgetType;

typedef struct ModelEntry {
    char* key;
    char* string_value;
    float number_value;
    int is_string;
    struct ModelEntry* next;
} ModelEntry;

typedef struct Model {
    ModelEntry* entries;
    char* source_path;
} Model;

typedef struct Style {
    char* name;
    Color background;
    Color text;
    struct Style* next;
} Style;

typedef struct Widget {
    WidgetType type;
    Rect rect;
    Color color;
    Color text_color;
    char* text; /* for labels/buttons */
    char* text_binding;
    char* value_binding;
    float minv, maxv, value;
    char* id;
    struct Widget* next;
} Widget;

Model* parse_model_json(const char* json_text, const char* source_path);
Style* parse_styles_json(const char* json_text);
Widget* parse_layout_json(const char* json_text, const Model* model, const Style* styles);

void update_widget_bindings(Widget* widgets, const Model* model);

float model_get_number(const Model* model, const char* key, float fallback);
const char* model_get_string(const Model* model, const char* key, const char* fallback);
void model_set_number(Model* model, const char* key, float value);
void model_set_string(Model* model, const char* key, const char* value);
int save_model(const Model* model);

void free_model(Model* model);
void free_styles(Style* styles);
void free_widgets(Widget* widgets);

#endif // UI_JSON_H
