#include "ui_parser.h"
#include "foundation/config/config_document.h"
#include "foundation/logger/logger.h"
#include "foundation/memory/arena.h"
#include "foundation/meta/reflection.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// --- Helper Functions ---

static UiKind parse_kind(const char* type_str, uint32_t* out_flags) {
    *out_flags = UI_FLAG_NONE;
    if (!type_str) return UI_KIND_CONTAINER;

    if (strcmp(type_str, "panel") == 0) return UI_KIND_CONTAINER;
    if (strcmp(type_str, "container") == 0) return UI_KIND_CONTAINER;
    
    if (strcmp(type_str, "label") == 0) return UI_KIND_TEXT;
    if (strcmp(type_str, "text") == 0) return UI_KIND_TEXT;
    
    if (strcmp(type_str, "button") == 0) {
        *out_flags |= UI_FLAG_CLICKABLE | UI_FLAG_FOCUSABLE;
        return UI_KIND_CONTAINER;
    }
    
    if (strcmp(type_str, "text_input") == 0 || strcmp(type_str, "textfield") == 0 || strcmp(type_str, "input") == 0) {
        *out_flags |= UI_FLAG_CLICKABLE | UI_FLAG_FOCUSABLE | UI_FLAG_EDITABLE;
        return UI_KIND_TEXT_INPUT;
    }
    
    if (strcmp(type_str, "checkbox") == 0) {
        *out_flags |= UI_FLAG_CLICKABLE;
        return UI_KIND_ICON; 
    }

    if (strcmp(type_str, "slider") == 0) {
        *out_flags |= UI_FLAG_CLICKABLE | UI_FLAG_DRAGGABLE;
        return UI_KIND_CONTAINER; 
    }

    if (strcmp(type_str, "curve") == 0) return UI_KIND_CUSTOM;

    return UI_KIND_CONTAINER;
}

// --- Recursive Loader ---

static UiNodeSpec* load_recursive(UiAsset* asset, const void* node_ptr) {
    const ConfigNode* node = (const ConfigNode*)node_ptr;
    if (!node || node->type != CONFIG_NODE_MAP) return NULL;

    // Allocate Spec from Asset's Arena
    UiNodeSpec* spec = ui_asset_push_node(asset);
    if (!spec) return NULL;

    const MetaStruct* meta = meta_get_struct("UiNodeSpec");
    if (!meta) {
        LOG_ERROR("UiParser: MetaStruct for UiNodeSpec not found!");
        return spec; 
    }

    // Default values
    spec->width = -1.0f;
    spec->height = -1.0f;
    spec->color = (Vec4){1,1,1,1};

    // Iterate all pairs in the YAML map
    for (size_t i = 0; i < node->pair_count; ++i) {
        const char* key = node->pairs[i].key;
        const ConfigNode* val = node->pairs[i].value;
        if (!key || !val) continue;

        // --- Special Handling for Recursion / Templates ---
        
        if (strcmp(key, "type") == 0) {
             spec->kind = parse_kind(val->scalar, &spec->flags);
             continue;
        }
        
        // Removed explicit "layout" handler -> handled by reflection now

        if (strcmp(key, "children") == 0) {
            if (val->type == CONFIG_NODE_SEQUENCE) {
                spec->child_count = val->item_count;
                spec->children = (UiNodeSpec**)arena_alloc_zero(&asset->arena, spec->child_count * sizeof(UiNodeSpec*));
                for (size_t k = 0; k < spec->child_count; ++k) {
                    spec->children[k] = load_recursive(asset, val->items[k]);
                }
            }
            continue;
        }
        if (strcmp(key, "item_template") == 0) {
            spec->item_template = load_recursive(asset, val);
            continue;
        }
        if (strcmp(key, "color") == 0) {
            if (val->type == CONFIG_NODE_SEQUENCE && val->item_count >= 3) {
                 float r = val->items[0]->scalar ? (float)atof(val->items[0]->scalar) : 1.0f;
                 float g = val->items[1]->scalar ? (float)atof(val->items[1]->scalar) : 1.0f;
                 float b = val->items[2]->scalar ? (float)atof(val->items[2]->scalar) : 1.0f;
                 float a = 1.0f;
                 if (val->item_count > 3 && val->items[3]->scalar) a = (float)atof(val->items[3]->scalar);
                 spec->color = (Vec4){r, g, b, a};
            }
            continue;
        }
        // Flags manual overrides (mixes with kind)
        if (strcmp(key, "draggable") == 0 && val->scalar && strcmp(val->scalar, "true") == 0) {
            spec->flags |= UI_FLAG_DRAGGABLE;
            continue;
        }
        if (strcmp(key, "clickable") == 0 && val->scalar && strcmp(val->scalar, "true") == 0) {
            spec->flags |= UI_FLAG_CLICKABLE;
            continue;
        }
        
        // --- Generic Reflection for Scalars ---
        const MetaField* field = meta_find_field(meta, key);
        if (!field) {
            // Check alias mappings (YAML key -> Struct field)
            if (strcmp(key, "text") == 0) field = meta_find_field(meta, "static_text");
            else if (strcmp(key, "texture") == 0) field = meta_find_field(meta, "texture_path");
            else if (strcmp(key, "bind") == 0) field = meta_find_field(meta, "value_source");
            else if (strcmp(key, "bind_x") == 0) field = meta_find_field(meta, "x_source");
            else if (strcmp(key, "bind_y") == 0) field = meta_find_field(meta, "y_source");
            else if (strcmp(key, "items") == 0) field = meta_find_field(meta, "data_source");
        }

        if (field) {
            // Handle scalar types
            if (field->type == META_TYPE_FLOAT) {
                 float fv = val->scalar ? (float)atof(val->scalar) : 0.0f;
                 meta_set_float(spec, field, fv);
            } else if (field->type == META_TYPE_INT) {
                 int iv = val->scalar ? atoi(val->scalar) : 0;
                 meta_set_int(spec, field, iv);
            } else if (field->type == META_TYPE_ENUM) {
                 int enum_val = 0;
                 const MetaEnum* e = meta_get_enum(field->type_name);
                 if (e && val->scalar) {
                     if (meta_enum_get_value(e, val->scalar, &enum_val)) {
                         meta_set_int(spec, field, enum_val);
                     } else {
                         LOG_WARN("UiParser: Unknown enum value '%s' for type '%s'", val->scalar, field->type_name);
                     }
                 }
            } else if (field->type == META_TYPE_STRING) {
                 const char* s = val->scalar ? val->scalar : "";
                 
                 if (strcmp(field->name, "static_text") == 0 && s[0] == '{') {
                     // Reroute to text_source
                     size_t len = strlen(s);
                     if (len > 2) {
                         char* buf = arena_push_string_n(&asset->arena, s + 1, len - 2);
                         spec->text_source = buf;
                         spec->static_text = NULL; 
                     }
                 } else {
                     char* str_copy = arena_push_string(&asset->arena, s);
                     char** field_ptr = (char**)meta_get_field_ptr(spec, field);
                     if (field_ptr) *field_ptr = str_copy;
                 }
            }
        }
    }

    return spec;
}

UiAsset* ui_parser_load_from_file(const char* path) {
    if (!path) return NULL;

    LOG_INFO("UiParser: Loading UI definition from file: %s", path);

    ConfigError err;
    ConfigDocument doc;
    if (!load_config_document(path, CONFIG_FORMAT_YAML, &doc, &err)) {
        LOG_ERROR("UiParser: Failed to load %s: %s (line %d)", path, err.message, err.line);
        return NULL;
    }

    // Create Asset (Owner)
    UiAsset* asset = ui_asset_create(64 * 1024);
    if (!asset) {
        config_document_free(&doc);
        return NULL;
    }

    asset->root = load_recursive(asset, doc.root);
    
    config_document_free(&doc);
    return asset;
}