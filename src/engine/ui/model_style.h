#ifndef UI_MODEL_STYLE_H
#define UI_MODEL_STYLE_H

#include <stddef.h>
#include "foundation/config/config_document.h"
#include "engine/render/backend/common/render_composition.h"

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

float model_get_number(const Model* model, const char* key, float fallback);
const char* model_get_string(const Model* model, const char* key, const char* fallback);
void model_set_number(Model* model, const char* key, float value);
void model_set_string(Model* model, const char* key, const char* value);
void free_model(Model* model);
void free_styles(Style* styles);

const Style* ui_default_style(void);
const Style* ui_root_style(void);

Model* ui_config_load_model(const ConfigDocument* doc);
Style* ui_config_load_styles(const ConfigNode* root);

#endif // UI_MODEL_STYLE_H
