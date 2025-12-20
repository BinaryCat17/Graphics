#include "ui_parser.h"
#include "ui_internal.h"
#include "foundation/config/simple_yaml.h"
#include "foundation/logger/logger.h"
#include "foundation/memory/arena.h"
#include "foundation/meta/reflection.h"
#include "foundation/platform/fs.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// --- Helper Functions ---

static UiNodeSpec* ui_node_spec_copy(UiAsset* asset, const UiNodeSpec* src) {
    if (!src) return NULL;
    UiNodeSpec* dst = ui_asset_push_node(asset);
    memcpy(dst, src, sizeof(UiNodeSpec));
    
    // We share string pointers because they are all in the same Asset Arena
    
    // Deep copy children
    if (src->child_count > 0) {
        dst->children = (UiNodeSpec**)arena_alloc_zero(&asset->arena, src->child_count * sizeof(UiNodeSpec*));
        for (size_t i = 0; i < src->child_count; ++i) {
            dst->children[i] = ui_node_spec_copy(asset, src->children[i]);
        }
    }
    
    if (src->item_template) {
        dst->item_template = ui_node_spec_copy(asset, src->item_template);
    }
    
    return dst;
}

static bool parse_hex_color(const char* str, Vec4* out_color) {
    if (!str || str[0] != '#') return false;
    str++; // Skip '#'
    
    unsigned int r = 0, g = 0, b = 0, a = 255;
    int len = (int)strlen(str);
    
    if (len == 6) {
        if (sscanf(str, "%02x%02x%02x", &r, &g, &b) != 3) return false;
    } else if (len == 8) {
        if (sscanf(str, "%02x%02x%02x%02x", &r, &g, &b, &a) != 4) return false;
    } else {
        return false;
    }
    
    out_color->x = (float)r / 255.0f;
    out_color->y = (float)g / 255.0f;
    out_color->z = (float)b / 255.0f;
    out_color->w = (float)a / 255.0f;
    return true;
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
        return UI_KIND_CONTAINER; 
    }

    if (strcmp(type_str, "slider") == 0) {
        *out_flags |= UI_FLAG_CLICKABLE | UI_FLAG_DRAGGABLE;
        return UI_KIND_CONTAINER; 
    }

    if (strcmp(type_str, "curve") == 0) return UI_KIND_CONTAINER;

    return UI_KIND_CONTAINER;
}

// --- Recursive Loader ---

static UiNodeSpec* load_recursive(UiAsset* asset, const ConfigNode* node) {
    if (!node || node->type != CONFIG_NODE_MAP) return NULL;

    UiNodeSpec* spec = NULL;

    // 1. Determine Base (Template or Kind)
    const ConfigNode* type_node = config_node_map_get(node, "type");
    if (type_node && type_node->scalar) {
        if (strcmp(type_node->scalar, "instance") == 0) {
            const ConfigNode* inst_node = config_node_map_get(node, "instance");
            if (inst_node && inst_node->scalar) {
                UiNodeSpec* template_spec = ui_asset_get_template(asset, inst_node->scalar);
                if (template_spec) spec = ui_node_spec_copy(asset, template_spec);
            }
        } else {
            UiNodeSpec* template_spec = ui_asset_get_template(asset, type_node->scalar);
            if (template_spec) spec = ui_node_spec_copy(asset, template_spec);
        }
    }

    if (!spec) {
        spec = ui_asset_push_node(asset);
        // Default values for new nodes
        spec->width = -1.0f;
        spec->height = -1.0f;
        spec->color = (Vec4){1,1,1,1};
        // Style defaults (Legacy behavior)
        spec->active_tint = 0.5f;
        spec->hover_tint = 1.2f;
        spec->text_scale = 0.5f;
        spec->caret_width = 2.0f;
        spec->caret_height = 20.0f;
        spec->text_color = (Vec4){1,1,1,1};
        spec->caret_color = (Vec4){1,1,1,1};
    }

    const MetaStruct* meta = meta_get_struct("UiNodeSpec");

    // Iterate all pairs in the YAML map to apply overrides
    for (size_t i = 0; i < node->pair_count; ++i) {
        const char* key = node->pairs[i].key;
        const ConfigNode* val = node->pairs[i].value;
        if (!key || !val) continue;

        if (strcmp(key, "import") == 0) {
            LOG_ERROR("UiParser: 'import' is not supported inside children (Node ID:%u). Use a Template and 'type: instance' instead.", 
                      spec->id);
            continue;
        }

        if (strcmp(key, "type") == 0) {
             // If it's a template name, we already handled it. 
             // If not, parse as kind.
             if (ui_asset_get_template(asset, val->scalar) == NULL) {
                 spec->kind = parse_kind(val->scalar, &spec->flags);
             }
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
        // ... rest of overrides ...
        if (strcmp(key, "item_template") == 0) {
            if (val->type == CONFIG_NODE_SCALAR) {
                UiNodeSpec* t = ui_asset_get_template(asset, val->scalar);
                if (t) spec->item_template = ui_node_spec_copy(asset, t);
                else LOG_ERROR("UiParser: Template '%s' not found for item_template", val->scalar);
            } else {
                spec->item_template = load_recursive(asset, val);
            }
            continue;
        }

        // --- Colors ---
        Vec4* target_color = NULL;
        if (strcmp(key, "color") == 0) target_color = &spec->color;
        else if (strcmp(key, "hover_color") == 0) target_color = &spec->hover_color;
        else if (strcmp(key, "active_color") == 0) target_color = &spec->active_color;
        else if (strcmp(key, "text_color") == 0) target_color = &spec->text_color;
        else if (strcmp(key, "caret_color") == 0) target_color = &spec->caret_color;

        if (target_color) {
            if (val->type == CONFIG_NODE_SEQUENCE && val->item_count >= 3) {
                 float r = val->items[0]->scalar ? (float)atof(val->items[0]->scalar) : 1.0f;
                 float g = val->items[1]->scalar ? (float)atof(val->items[1]->scalar) : 1.0f;
                 float b = val->items[2]->scalar ? (float)atof(val->items[2]->scalar) : 1.0f;
                 float a = 1.0f;
                 if (val->item_count > 3 && val->items[3]->scalar) a = (float)atof(val->items[3]->scalar);
                 *target_color = (Vec4){r, g, b, a};
            } else if (val->type == CONFIG_NODE_SCALAR) {
                parse_hex_color(val->scalar, target_color);
            }
            continue;
        }

        // --- Floats ---
        float* target_float = NULL;
        if (strcmp(key, "animation_speed") == 0) target_float = &spec->animation_speed;
        else if (strcmp(key, "active_tint") == 0) target_float = &spec->active_tint;
        else if (strcmp(key, "hover_tint") == 0) target_float = &spec->hover_tint;
        else if (strcmp(key, "text_scale") == 0) target_float = &spec->text_scale;
        else if (strcmp(key, "caret_width") == 0) target_float = &spec->caret_width;
        else if (strcmp(key, "caret_height") == 0) target_float = &spec->caret_height;

        if (target_float) {
            *target_float = val->scalar ? (float)atof(val->scalar) : 0.0f;
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
            else if (strcmp(key, "texture") == 0) field = meta_find_field(meta, "texture_id");
            else if (strcmp(key, "bind") == 0) field = meta_find_field(meta, "value_source");
            else if (strcmp(key, "bind_visible") == 0 || strcmp(key, "bind_if") == 0) field = meta_find_field(meta, "visible_source");
            else if (strcmp(key, "bind_x") == 0) field = meta_find_field(meta, "x_source");
            else if (strcmp(key, "bind_y") == 0) field = meta_find_field(meta, "y_source");
            else if (strcmp(key, "collection") == 0) field = meta_find_field(meta, "bind_collection");
            else if (strcmp(key, "on_click") == 0) field = meta_find_field(meta, "on_click_cmd");
            else if (strcmp(key, "on_change") == 0) field = meta_find_field(meta, "on_change_cmd");
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
            } else if (field->type == META_TYPE_STRING_ID) {
                 const char* s = val->scalar ? val->scalar : "";
                 StringId id = str_id(s);
                 // We reuse meta_set_int because StringId is compatible with int (32-bit)
                 // and we don't have meta_set_uint or meta_set_string_id
                 meta_set_int(spec, field, (int)id);
            }
        }
    }

    return spec;
}

// --- Generic Reflection for Scalars ---

static ConfigNode* resolve_import(MemoryArena* scratch, const ConfigNode* node) {
    if (node->type == CONFIG_NODE_MAP) {
        const ConfigNode* import_val = config_node_map_get(node, "import");
        if (import_val && import_val->scalar) {
            char* text = fs_read_text(scratch, import_val->scalar);
            if (text) {
                ConfigNode* imported_root = NULL;
                ConfigError err;
                if (simple_yaml_parse(scratch, text, &imported_root, &err)) {
                    return imported_root;
                }
            }
        }
    }
    return NULL;
}

// --- Validation ---

static void validate_node(UiNodeSpec* spec, const char* path) {
    if (!spec) return;
    
    if (spec->layout == UI_LAYOUT_FLEX_COLUMN || spec->layout == UI_LAYOUT_FLEX_ROW) {
        if (spec->x_source || spec->y_source) {
            LOG_WARN("UiParser: Node ID:%u uses x/y bindings inside a Flex container. These will be ignored.", spec->id);
        }
    }
    
    if (spec->layout == UI_LAYOUT_SPLIT_H || spec->layout == UI_LAYOUT_SPLIT_V) {
        if (spec->child_count != 2) {
            LOG_ERROR("UiParser: Split container ID:%u MUST have exactly 2 children (has %zu).", spec->id, spec->child_count);
        }
    }
    
    for (size_t i = 0; i < spec->child_count; ++i) {
        validate_node(spec->children[i], path);
    }
}

UiAsset* ui_parser_load_from_file(const char* path) {
    if (!path) return NULL;

    LOG_TRACE("UiParser: Loading UI definition from file: %s", path);

    // Scratch Arena for parsing (2MB should be plenty for config files)
    MemoryArena scratch;
    if (!arena_init(&scratch, 2 * 1024 * 1024)) {
        LOG_ERROR("UiParser: Failed to init scratch arena");
        return NULL;
    }

    char* text = fs_read_text(&scratch, path);
    if (!text) {
        LOG_ERROR("UiParser: Failed to read file %s", path);
        arena_destroy(&scratch);
        return NULL;
    }

    ConfigNode* root = NULL;
    ConfigError err = {0};
    if (!simple_yaml_parse(&scratch, text, &root, &err)) {
        LOG_ERROR("UiParser: YAML Parse error in %s (line %d, col %d): %s", path, err.line, err.column, err.message);
        arena_destroy(&scratch);
        return NULL;
    }

    // Create Asset (Owner)
    UiAsset* asset = ui_asset_create(64 * 1024);
    if (!asset) {
        arena_destroy(&scratch);
        return NULL;
    }

    // Parse Templates (if any)
    const ConfigNode* templates_node = config_node_map_get(root, "templates");
    if (templates_node && templates_node->type == CONFIG_NODE_MAP) {
        for (size_t i = 0; i < templates_node->pair_count; ++i) {
            const char* t_name = templates_node->pairs[i].key;
            const ConfigNode* t_val = templates_node->pairs[i].value;
            
            ConfigNode* t_actual = resolve_import(&scratch, t_val);
            
            UiNodeSpec* spec = load_recursive(asset, t_actual ? t_actual : t_val);
            if (spec) {
                UiTemplate* t = (UiTemplate*)arena_alloc_zero(&asset->arena, sizeof(UiTemplate));
                t->name = arena_push_string(&asset->arena, t_name);
                t->spec = spec;
                t->next = asset->templates;
                asset->templates = t;
                LOG_TRACE("UiParser: Registered template '%s'", t->name);
            }
        }
    }

    ConfigNode* root_actual = resolve_import(&scratch, root);
    asset->root = load_recursive(asset, root_actual ? root_actual : root);
    
    validate_node(asset->root, path);
    
    arena_destroy(&scratch);
    return asset;
}