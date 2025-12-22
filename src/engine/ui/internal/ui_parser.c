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

static UiKind parse_kind(const char* type_str, uint32_t* out_flags);

static UiKind parse_kind(const char* type_str, uint32_t* out_flags) {
    *out_flags = UI_FLAG_NONE;
    if (!type_str) return UI_KIND_CONTAINER;

    if (strcmp(type_str, "text") == 0) return UI_KIND_TEXT;
    if (strcmp(type_str, "viewport") == 0) return UI_KIND_VIEWPORT;
    
    // Default to container for everything else (including 'container')
    return UI_KIND_CONTAINER;
}

static SceneNodeSpec* ui_node_spec_copy(UiAsset* asset, const SceneNodeSpec* src) {
    if (!src) return NULL;
    SceneNodeSpec* dst = ui_asset_push_node(asset);
    memcpy(dst, src, sizeof(SceneNodeSpec));
    
    // We share string pointers because they are all in the same Asset Arena
    
    // Deep copy children
    if (src->child_count > 0) {
        dst->children = (SceneNodeSpec**)arena_alloc_zero(&asset->arena, src->child_count * sizeof(SceneNodeSpec*));
        for (size_t i = 0; i < src->child_count; ++i) {
            dst->children[i] = ui_node_spec_copy(asset, src->children[i]);
        }
    }
    
    if (src->item_template) {
        dst->item_template = ui_node_spec_copy(asset, src->item_template);
    }
    
    return dst;
}

// --- Generic Recursive Parser for Structs ---

static void parse_struct_fields(void* instance, const MetaStruct* meta, const ConfigNode* map, UiAsset* asset) {
    if (!instance || !meta || !map || map->type != CONFIG_NODE_MAP) return;

    for (size_t i = 0; i < map->pair_count; ++i) {
        const char* key = map->pairs[i].key;
        const ConfigNode* val = map->pairs[i].value;
        if (!key || !val) continue;

        const MetaField* field = meta_find_field(meta, key);
        if (!field) {
            LOG_WARN("UiParser: Unknown field '%s' in struct '%s'", key, meta->name);
            continue;
        }
        
        void* field_ptr = (char*)instance + field->offset;

        if (field->type == META_TYPE_STRUCT) {
             const MetaStruct* sub_meta = meta_get_struct(field->type_name);
             if (sub_meta) {
                 parse_struct_fields(field_ptr, sub_meta, val, asset);
             }
        } else if (val->type == CONFIG_NODE_SEQUENCE && (field->type >= META_TYPE_VEC2 && field->type <= META_TYPE_VEC4)) {
             int vec_size = field->type - META_TYPE_VEC2 + 2;
             float* f_ptr = (float*)field_ptr;
             for (int k = 0; k < vec_size; ++k) {
                 float v = 0.0f;
                 if (k < (int)val->item_count && val->items[k]->scalar) {
                     v = (float)atof(val->items[k]->scalar);
                 } else if (k == 3) {
                     v = 1.0f; 
                 }
                 f_ptr[k] = v;
             }
        } else if (field->type == META_TYPE_STRING) {
             const char* s = val->scalar ? val->scalar : "";
             char* str_copy = arena_push_string(&asset->arena, s);
             *(char**)field_ptr = str_copy;
        } else {
             const char* s = val->scalar ? val->scalar : "";
             if (!meta_set_from_string(instance, field, s)) {
                 if (s[0] != '\0' && field->type == META_TYPE_ENUM) {
                     LOG_WARN("UiParser: Unknown enum value '%s' for type '%s'", s, field->type_name);
                 }
             }
        }
    }
}

// --- Recursive Loader ---

static SceneNodeSpec* load_recursive(UiAsset* asset, const ConfigNode* node) {
    if (!node || node->type != CONFIG_NODE_MAP) return NULL;

    SceneNodeSpec* spec = NULL;

    // 1. Determine Base (Template or Kind)
    const ConfigNode* type_node = config_node_map_get(node, "type");
    if (type_node && type_node->scalar) {
        if (strcmp(type_node->scalar, "instance") == 0) {
            const ConfigNode* inst_node = config_node_map_get(node, "instance");
            if (inst_node && inst_node->scalar) {
                SceneNodeSpec* template_spec = ui_asset_get_template(asset, inst_node->scalar);
                if (template_spec) spec = ui_node_spec_copy(asset, template_spec);
            }
        } else {
            SceneNodeSpec* template_spec = ui_asset_get_template(asset, type_node->scalar);
            if (template_spec) spec = ui_node_spec_copy(asset, template_spec);
        }
    }

    if (!spec) {
        spec = ui_asset_push_node(asset);
        // Default values for new nodes
        spec->layout.width = -1.0f;
        spec->layout.height = -1.0f;
        spec->style.color = (Vec4){1,1,1,1};
        spec->style.text_color = (Vec4){1,1,1,1};
        spec->style.caret_color = (Vec4){1,1,1,1};
    }

    const MetaStruct* meta = meta_get_struct("SceneNodeSpec");

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
             if (ui_asset_get_template(asset, val->scalar) == NULL) {
                 spec->kind = parse_kind(val->scalar, &spec->flags);
             }
             continue;
        }

        if (strcmp(key, "instance") == 0) continue;
        
        if (strcmp(key, "children") == 0) {
            if (val->type == CONFIG_NODE_SEQUENCE) {
                spec->child_count = val->item_count;
                spec->children = (SceneNodeSpec**)arena_alloc_zero(&asset->arena, spec->child_count * sizeof(SceneNodeSpec*));
                for (size_t k = 0; k < spec->child_count; ++k) {
                    spec->children[k] = load_recursive(asset, val->items[k]);
                }
            }
            continue;
        }
        
        if (strcmp(key, "item_template") == 0) {
            if (val->type == CONFIG_NODE_SCALAR) {
                SceneNodeSpec* t = ui_asset_get_template(asset, val->scalar);
                if (t) spec->item_template = ui_node_spec_copy(asset, t);
                else LOG_ERROR("UiParser: Template '%s' not found for item_template", val->scalar);
            } else {
                spec->item_template = load_recursive(asset, val);
            }
            continue;
        }
        
        // --- Generic Reflection ---
        const MetaField* field = meta_find_field(meta, key);

        if (field) {
            void* field_ptr = (char*)spec + field->offset;

            // Handle Sub-Structs (layout, style, transform)
            if (field->type == META_TYPE_STRUCT) {
                 const MetaStruct* sub_meta = meta_get_struct(field->type_name);
                 if (sub_meta) {
                     parse_struct_fields(field_ptr, sub_meta, val, asset);
                 }
            }
            // Handle Sequence for Vectors
            else if (val->type == CONFIG_NODE_SEQUENCE && (field->type >= META_TYPE_VEC2 && field->type <= META_TYPE_VEC4)) {
                int vec_size = field->type - META_TYPE_VEC2 + 2; 
                float* data_ptr = (float*)field_ptr;
                if (data_ptr) {
                    for (int k = 0; k < vec_size; ++k) {
                        float v = 0.0f;
                        if (k < (int)val->item_count && val->items[k]->scalar) {
                            v = (float)atof(val->items[k]->scalar);
                        } else if (k == 3) {
                            v = 1.0f; // Alpha defaults to 1.0
                        }
                        data_ptr[k] = v;
                    }
                }
            }
            // Handle Strings
            else if (field->type == META_TYPE_STRING) {
                 const char* s = val->scalar ? val->scalar : "";
                 
                 // Special handling for 'text' field to support Binding Syntax "{...}"
                 if (strcmp(field->name, "text") == 0 && s[0] == '{') {
                     // Reroute to bind_text
                     size_t len = strlen(s);
                     if (len > 2) {
                         char* buf = arena_push_string_n(&asset->arena, s + 1, len - 2);
                         spec->bind_text = buf;
                         spec->text = NULL; 
                     }
                 } else {
                     char* str_copy = arena_push_string(&asset->arena, s);
                     *(char**)field_ptr = str_copy;
                 }
            } 
            // Handle Scalars
            else {
                 const char* s = val->scalar ? val->scalar : "";
                 if (!meta_set_from_string(spec, field, s)) {
                     if (s[0] != '\0' && field->type == META_TYPE_ENUM) {
                         LOG_WARN("UiParser: Unknown enum value '%s' for type '%s'", s, field->type_name);
                     }
                 }
            }
        } else if (strcmp(key, "provider") == 0) {
             if (val->scalar) {
                 spec->provider_id = str_id(val->scalar);
             }
        } else {
            LOG_WARN("UiParser: Unknown field '%s' in SceneNodeSpec (Node ID:%u). Check indentation or spelling.", key, spec->id);
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

static void validate_node(SceneNodeSpec* spec, const char* path) {
    if (!spec) return;
    
    if (spec->layout.type == UI_LAYOUT_FLEX_COLUMN || spec->layout.type == UI_LAYOUT_FLEX_ROW) {
        if (spec->bind_x || spec->bind_y) {
            LOG_WARN("UiParser: Node ID:%u uses x/y bindings inside a Flex container. These will be ignored.", spec->id);
        }
    }
    
    if (spec->layout.type == UI_LAYOUT_SPLIT_H || spec->layout.type == UI_LAYOUT_SPLIT_V) {
        if (spec->child_count != 2) {
            LOG_ERROR("UiParser: Split container ID:%u MUST have exactly 2 children (has %zu).", spec->id, spec->child_count);
        }
    }
    
    for (size_t i = 0; i < spec->child_count; ++i) {
        validate_node(spec->children[i], path);
    }
}

UiAsset* ui_parser_load_internal(const char* path) {
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
            
            SceneNodeSpec* spec = load_recursive(asset, t_actual ? t_actual : t_val);
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
