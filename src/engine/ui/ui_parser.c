#include "ui_parser.h"
#include "foundation/config/config_document.h"
#include "foundation/logger/logger.h"
#include "foundation/memory/arena.h"
#include "foundation/meta/reflection.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// --- Helper Functions ---

static UiLayoutStrategy parse_layout_strategy(const char* str) {
    if (!str) return UI_LAYOUT_FLEX_COLUMN;
    if (strcmp(str, "row") == 0) return UI_LAYOUT_FLEX_ROW;
    if (strcmp(str, "column") == 0) return UI_LAYOUT_FLEX_COLUMN;
    if (strcmp(str, "canvas") == 0) return UI_LAYOUT_CANVAS;
    if (strcmp(str, "overlay") == 0) return UI_LAYOUT_OVERLAY;
    return UI_LAYOUT_FLEX_COLUMN;
}

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

        // --- Special Handling for Enums / Complex Types / Recursion ---
        
        if (strcmp(key, "type") == 0) {
             spec->kind = parse_kind(val->scalar, &spec->flags);
             continue;
        }
        if (strcmp(key, "layout") == 0) {
            spec->layout = parse_layout_strategy(val->scalar);
            continue;
        }
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
                 // Warning: Enums are INTs but we handle them manually above. 
                 // This generic block handles simple ints if any.
                 int iv = val->scalar ? atoi(val->scalar) : 0;
                 meta_set_int(spec, field, iv);
            } else if (field->type == META_TYPE_STRING) {
                 const char* s = val->scalar ? val->scalar : "";
                 // Handle Bindings {binding} vs Static Text
                 // Note: We need to distinguish fields that ARE sources vs literal fields
                 // Logic: If the YAML value starts with '{', it's a binding.
                 // But wait, if field is "value_source", we expect just "my_var". 
                 // If field is "static_text", and value is "{my_var}", do we behave differently?
                 
                 // Current logic: 
                 // "text": "Hello" -> static_text = "Hello"
                 // "text": "{Name}" -> text_source = "Name"
                 
                 // If we found 'static_text' field via alias "text":
                 if (strcmp(field->name, "static_text") == 0 && s[0] == '{') {
                     // Reroute to text_source
                     size_t len = strlen(s);
                     if (len > 2) {
                         char* buf = arena_push_string_n(&asset->arena, s + 1, len - 2);
                         spec->text_source = buf;
                         // Clear static_text just in case
                         spec->static_text = NULL; 
                     }
                 } else {
                     // Standard string set
                     // We must allocate in arena, but meta_set_string does malloc!
                     // FIXME: meta_set_string uses malloc/free. UiAsset uses Arena.
                     // We cannot use meta_set_string here safely if we want to use the arena.
                     
                     // Solution: Manually set pointer via offset
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
