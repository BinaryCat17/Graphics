#include "config_system.h"
#include "foundation/string/string_id.h"
#include "simple_yaml.h"
#include "foundation/memory/arena.h"
#include "foundation/platform/platform.h"
#include "foundation/platform/fs.h"
#include "foundation/logger/logger.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static struct {
    MemoryArena arena;
    ConfigNode* yaml_root;
    ConfigNode* cli_root;
    bool initialized;
} g_config;

void config_system_init(void) {
    if (g_config.initialized) return;
    arena_init(&g_config.arena, 1024 * 1024); // 1MB for config
    g_config.initialized = true;
    g_config.yaml_root = NULL;
    g_config.cli_root = NULL;
}

void config_system_shutdown(void) {
    if (!g_config.initialized) return;
    arena_destroy(&g_config.arena);
    g_config.initialized = false;
    g_config.yaml_root = NULL;
    g_config.cli_root = NULL;
}

static ConfigNode* config_node_create_map(void) {
    ConfigNode* node = arena_alloc_zero(&g_config.arena, sizeof(ConfigNode));
    node->type = CONFIG_NODE_MAP;
    return node;
}

static void config_node_add_scalar(ConfigNode* map, const char* key, const char* value) {
    if (map->type != CONFIG_NODE_MAP) return;
    
    // Resize pairs if needed
    if (map->pair_count + 1 > map->pair_capacity) {
        size_t new_cap = map->pair_capacity == 0 ? 4 : map->pair_capacity * 2;
        ConfigPair* new_pairs = arena_alloc(&g_config.arena, new_cap * sizeof(ConfigPair));
        if (map->pairs) {
            memcpy(new_pairs, map->pairs, map->pair_count * sizeof(ConfigPair));
        }
        map->pairs = new_pairs;
        map->pair_capacity = new_cap;
    }

    // Copy key and value to arena
    size_t key_len = strlen(key);
    char* key_copy = arena_alloc(&g_config.arena, key_len + 1);
    memcpy(key_copy, key, key_len + 1);

    size_t val_len = strlen(value);
    char* val_copy = arena_alloc(&g_config.arena, val_len + 1);
    memcpy(val_copy, value, val_len + 1);


    ConfigNode* val_node = arena_alloc_zero(&g_config.arena, sizeof(ConfigNode));
    val_node->type = CONFIG_NODE_SCALAR;
    val_node->scalar = val_copy;

    map->pairs[map->pair_count].key = key_copy;
    map->pairs[map->pair_count].value = val_node;
    map->pair_count++;
}

static const ConfigNode* find_node_recursive(const ConfigNode* root, const char* key) {
    if (!root || root->type != CONFIG_NODE_MAP) return NULL;
    
    // Check for dot notation
    const char* dot = strchr(key, '.');
    if (dot) {
        // Split key: "window.width" -> "window" + "width"
        size_t head_len = (size_t)(dot - key);
        char head[128];
        if (head_len >= sizeof(head)) head_len = sizeof(head) - 1;
        strncpy(head, key, head_len);
        head[head_len] = 0;

        const ConfigNode* child = config_node_map_get(root, head);
        return find_node_recursive(child, dot + 1);
    } else {
        return config_node_map_get(root, key);
    }
}

void config_system_load(int argc, char** argv) {
    if (!g_config.initialized) config_system_init();

    // 1. Create CLI root
    g_config.cli_root = config_node_create_map();

    // 2. Parse CLI args
    const char* config_path = "config.yaml"; // Default

    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (arg[0] == '-' && arg[1] == '-') {
            const char* key_start = arg + 2;
            const char* eq = strchr(key_start, '=');
            
            if (strcmp(key_start, "config") == 0 && i + 1 < argc) {
                config_path = argv[++i];
                continue;
            }
            if (strncmp(key_start, "config=", 7) == 0) {
                config_path = eq + 1;
                continue;
            }

            char key[128];
            const char* val = NULL;

            if (eq) {
                // --key=value
                size_t klen = (size_t)(eq - key_start);
                if (klen >= sizeof(key)) klen = sizeof(key) - 1;
                strncpy(key, key_start, klen);
                key[klen] = 0;
                val = eq + 1;
            } else {
                // --key value OR --flag
                strncpy(key, key_start, sizeof(key) - 1);
                
                // Check if next arg is a value or another flag
                if (i + 1 < argc && argv[i+1][0] != '-') {
                    val = argv[++i];
                } else {
                    val = "true";
                }
            }
            
            // Normalize key: replace '-' with '_' if needed? 
            // main.c uses --log-level, but struct has log_level.
            // Let's replace '-' with '_' to match struct fields common practice?
            // User provided "log-level", key becomes "log_level".
            for(int k=0; key[k]; ++k) {
                if(key[k] == '-') key[k] = '_';
            }

            config_node_add_scalar(g_config.cli_root, key, val);
        }
    }

    // 3. Load YAML file
    char* file_content = fs_read_text(NULL, config_path);
    if (file_content) {
        ConfigError err;
        if (!simple_yaml_parse(&g_config.arena, file_content, &g_config.yaml_root, &err)) {
            LOG_WARN("Failed to parse config file '%s': Line %d: %s", config_path, err.line, err.message);
        } else {
            LOG_INFO("Loaded config from '%s'", config_path);
        }
        free(file_content); // platform_read_file uses malloc
    } else {
        // It's okay if config file doesn't exist, unless explicitly requested?
        // For now, silent fail on default, maybe warn if custom path?
        if (strcmp(config_path, "config.yaml") != 0) {
            LOG_WARN("Config file '%s' not found.", config_path);
        }
    }
}

static const char* get_value_raw(const char* key) {
    if (!g_config.initialized) return NULL;
    
    // 1. CLI Override
    const ConfigNode* node = find_node_recursive(g_config.cli_root, key);
    if (node && node->type == CONFIG_NODE_SCALAR) return node->scalar;

    // 2. YAML Config
    node = find_node_recursive(g_config.yaml_root, key);
    if (node && node->type == CONFIG_NODE_SCALAR) return node->scalar;

    return NULL;
}

const char* config_get_string(const char* key, const char* default_value) {
    const char* val = get_value_raw(key);
    return val ? val : default_value;
}

int config_get_int(const char* key, int default_value) {
    const char* val = get_value_raw(key);
    if (!val) return default_value;
    return atoi(val);
}

float config_get_float(const char* key, float default_value) {
    const char* val = get_value_raw(key);
    if (!val) return default_value;
    return (float)atof(val);
}

bool config_get_bool(const char* key, bool default_value) {
    const char* val = get_value_raw(key);
    if (!val) return default_value;
    if (strcmp(val, "true") == 0 || strcmp(val, "1") == 0 || strcmp(val, "yes") == 0) return true;
    if (strcmp(val, "false") == 0 || strcmp(val, "0") == 0 || strcmp(val, "no") == 0) return false;
    return default_value;
}

// --- Generic Deserializer Implementation ---

bool config_load_struct_array(const ConfigNode* node, const MetaStruct* meta, void*** out_array, size_t* out_count, MemoryArena* arena) {
    if (!node || node->type != CONFIG_NODE_SEQUENCE || !meta || !out_array || !out_count || !arena) return false;

    size_t count = node->item_count;
    *out_count = count;
    
    if (count == 0) {
        *out_array = NULL;
        return true;
    }

    // Allocate array of pointers
    void** array = (void**)arena_alloc(arena, count * sizeof(void*));
    *out_array = array;

    for (size_t i = 0; i < count; ++i) {
        ConfigNode* item_node = node->items[i];
        if (item_node->type != CONFIG_NODE_MAP) {
             array[i] = NULL;
             continue;
        }

        // Allocate struct instance
        void* instance = arena_alloc_zero(arena, meta->size);
        array[i] = instance;

        // Recursive load
        config_load_struct(item_node, meta, instance, arena);
    }
    return true;
}

bool config_load_struct(const ConfigNode* node, const MetaStruct* meta, void* instance, MemoryArena* arena) {
    if (!node || node->type != CONFIG_NODE_MAP || !meta || !instance || !arena) return false;

    for (size_t i = 0; i < meta->field_count; ++i) {
        const MetaField* field = &meta->fields[i];
        const ConfigNode* child = config_node_map_get(node, field->name);

        if (!child) continue;

        switch (field->type) {
            case META_TYPE_INT:
                if (child->type == CONFIG_NODE_SCALAR) {
                    meta_set_int(instance, field, atoi(child->scalar));
                }
                break;
            case META_TYPE_FLOAT:
                if (child->type == CONFIG_NODE_SCALAR) {
                    meta_set_float(instance, field, (float)atof(child->scalar));
                }
                break;
            case META_TYPE_BOOL:
                if (child->type == CONFIG_NODE_SCALAR) {
                    bool val = (strcmp(child->scalar, "true") == 0 || strcmp(child->scalar, "yes") == 0 || strcmp(child->scalar, "1") == 0);
                    meta_set_bool(instance, field, val);
                }
                break;
            case META_TYPE_STRING:
                if (child->type == CONFIG_NODE_SCALAR) {
                    size_t len = strlen(child->scalar);
                    char* copy = arena_alloc(arena, len + 1);
                    memcpy(copy, child->scalar, len + 1);
                    
                    char** ptr = (char**)meta_get_field_ptr(instance, field);
                    *ptr = copy;
                }
                break;
            case META_TYPE_STRING_ARRAY:
                if (child->type == CONFIG_NODE_SCALAR) {
                    char* ptr = (char*)meta_get_field_ptr(instance, field);
                    strncpy(ptr, child->scalar, 255);
                    ptr[255] = 0;
                }
                break;
            case META_TYPE_STRING_ID:
                if (child->type == CONFIG_NODE_SCALAR) {
                    StringId id = str_id(child->scalar);
                    *(StringId*)meta_get_field_ptr(instance, field) = id;
                }
                break;
            case META_TYPE_ENUM:
                if (child->type == CONFIG_NODE_SCALAR) {
                    const MetaEnum* e = meta_get_enum(field->type_name);
                    int val = 0;
                    if (e && meta_enum_get_value(e, child->scalar, &val)) {
                        meta_set_int(instance, field, val);
                    }
                }
                break;
            case META_TYPE_FLAGS:
                if (child->type == CONFIG_NODE_SCALAR) {
                    meta_set_from_string(instance, field, child->scalar);
                }
                break;
            case META_TYPE_VEC2:
            case META_TYPE_VEC3:
            case META_TYPE_VEC4: {
                if (child->type == CONFIG_NODE_SCALAR) {
                    meta_set_from_string(instance, field, child->scalar);
                } else if (child->type == CONFIG_NODE_SEQUENCE) {
                    int vec_size = field->type - META_TYPE_VEC2 + 2;
                    float* ptr = (float*)meta_get_field_ptr(instance, field);
                    for(int k=0; k < vec_size; ++k) {
                        float v = 0.0f;
                        if (k < (int)child->item_count && child->items[k]->scalar) {
                             v = (float)atof(child->items[k]->scalar);
                        } else if (k == 3) {
                             v = 1.0f; // Alpha
                        }
                        ptr[k] = v;
                    }
                }
                break;
            }
            case META_TYPE_STRUCT:
                if (child->type == CONFIG_NODE_MAP) {
                    const MetaStruct* s = meta_get_struct(field->type_name);
                    void* sub_inst = meta_get_field_ptr(instance, field);
                    if (s && sub_inst) {
                        config_load_struct(child, s, sub_inst, arena);
                    }
                }
                break;
            case META_TYPE_POINTER_ARRAY:
                if (child->type == CONFIG_NODE_SEQUENCE) {
                    const MetaStruct* elem_meta = meta_get_struct(field->type_name);
                    if (!elem_meta) continue;

                    void** array_out = NULL;
                    size_t count = 0;

                    if (config_load_struct_array(child, elem_meta, &array_out, &count, arena)) {
                        // Set array
                        void*** ptr_to_array = (void***)meta_get_field_ptr(instance, field);
                        *ptr_to_array = array_out;

                        // Set count (Heuristic)
                        char count_name[512];
                        snprintf(count_name, sizeof(count_name), "%.400s_count", field->name);
                        const MetaField* count_field = meta_find_field(meta, count_name);
                        
                        // Fallback: "children" -> "child_count"
                        if (!count_field) {
                            size_t name_len = strlen(field->name);
                            if (name_len > 1 && field->name[name_len-1] == 's') {
                                char singular[256];
                                if (name_len < sizeof(singular)) {
                                    strncpy(singular, field->name, name_len - 1);
                                    singular[name_len - 1] = 0;
                                    snprintf(count_name, sizeof(count_name), "%s_count", singular);
                                    count_field = meta_find_field(meta, count_name);
                                }
                            }
                        }
                        
                        if (!count_field && strcmp(field->name, "children") == 0) {
                             count_field = meta_find_field(meta, "child_count");
                        }

                        if (count_field && count_field->type == META_TYPE_INT) {
                            meta_set_int(instance, count_field, (int)count);
                        }
                    }
                }
                break;
            case META_TYPE_POINTER:
                 if (child->type == CONFIG_NODE_MAP) {
                    const MetaStruct* s = meta_get_struct(field->type_name);
                    if (s) {
                        void* sub_inst = arena_alloc_zero(arena, s->size);
                         if (config_load_struct(child, s, sub_inst, arena)) {
                             void** ptr = (void**)meta_get_field_ptr(instance, field);
                             *ptr = sub_inst;
                         }
                    }
                }
                break;
        }
    }
    return true;
}
