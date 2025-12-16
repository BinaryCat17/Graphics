#include "foundation/config/module_yaml_loader.h"
#include "foundation/platform/platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "foundation/config/config_io.h"
#include "foundation/platform/fs.h"

static void assign_error(ConfigError *err, int line, int column, const char *msg)
{
    if (!err) return;
    err->line = line;
    err->column = column;
    platform_strncpy(err->message, msg, sizeof(err->message) - 1);
    err->message[sizeof(err->message) - 1] = 0;
}

static char *dup_string(const char *s)
{
    if (!s) return NULL;
    size_t len = strlen(s);
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len + 1);
    return out;
}

static char *join_path(const char *dir, const char *leaf)
{
    if (!dir || !leaf) return NULL;
    size_t dir_len = strlen(dir);
    while (dir_len > 0 && dir[dir_len - 1] == '/') dir_len--;
    size_t leaf_len = strlen(leaf);
    size_t total = dir_len + 1 + leaf_len + 1;
    char *out = (char *)malloc(total);
    if (!out) return NULL;
    memcpy(out, dir, dir_len);
    out[dir_len] = '/';
    memcpy(out + dir_len + 1, leaf, leaf_len);
    out[total - 1] = 0;
    return out;
}

static ConfigNode *config_node_clone(const ConfigNode *node)
{
    if (!node) return NULL;

    ConfigNode *copy = (ConfigNode *)calloc(1, sizeof(ConfigNode));
    if (!copy) return NULL;

    copy->type = node->type;
    copy->line = node->line;
    copy->scalar_type = node->scalar_type;
    if (node->scalar) {
        copy->scalar = dup_string(node->scalar);
        if (node->scalar && !copy->scalar) {
            config_node_free(copy);
            return NULL;
        }
    }

    copy->pair_count = node->pair_count;
    copy->pair_capacity = node->pair_count;
    if (node->pair_count > 0) {
        copy->pairs = (ConfigPair *)calloc(node->pair_count, sizeof(ConfigPair));
        if (!copy->pairs) {
            config_node_free(copy);
            return NULL;
        }
        for (size_t i = 0; i < node->pair_count; ++i) {
            copy->pairs[i].key = dup_string(node->pairs[i].key);
            copy->pairs[i].value = config_node_clone(node->pairs[i].value);
            if ((node->pairs[i].key && !copy->pairs[i].key) || (node->pairs[i].value && !copy->pairs[i].value)) {
                config_node_free(copy);
                return NULL;
            }
        }
    }

    copy->item_count = node->item_count;
    copy->item_capacity = node->item_count;
    if (node->item_count > 0) {
        copy->items = (ConfigNode **)calloc(node->item_count, sizeof(ConfigNode *));
        if (!copy->items) {
            config_node_free(copy);
            return NULL;
        }
        for (size_t i = 0; i < node->item_count; ++i) {
            copy->items[i] = config_node_clone(node->items[i]);
            if (node->items[i] && !copy->items[i]) {
                config_node_free(copy);
                return NULL;
            }
        }
    }

    return copy;
}

static int config_document_from_node(const ConfigNode *node, const char *source_path, ConfigDocument *out_doc)
{
    if (!node || !out_doc) return 0;

    ConfigNode *root_copy = config_node_clone(node);
    if (!root_copy) return 0;

    char *path_copy = source_path ? dup_string(source_path) : NULL;
    if (source_path && !path_copy) {
        config_node_free(root_copy);
        return 0;
    }

    *out_doc = (ConfigDocument){
        .format = CONFIG_FORMAT_YAML,
        .source_path = path_copy,
        .root = root_copy,
    };
    return 1;
}

static size_t detect_default_chunk_capacity(const ConfigNode *node)
{
    const ConfigNode *cap = config_map_get(node, "chunk_capacity");
    if (cap && cap->scalar) return (size_t)strtoul(cap->scalar, NULL, 10);
    return 16;
}

int module_schema_load(const char *schema_path, ModuleSchema *out_schema, ConfigError *err)
{
    if (!schema_path || !out_schema) return 0;
    memset(out_schema, 0, sizeof(*out_schema));

    ConfigNode *root = NULL;
    if (!parse_config(schema_path, CONFIG_FORMAT_YAML, &root, err)) {
        return 0;
    }

    const ConfigNode *ns = config_map_get(root, "namespace");
    if (!ns || !ns->scalar) {
        config_node_free(root);
        assign_error(err, 0, 1, "Schema missing namespace");
        return 0;
    }
    out_schema->namespace_name = dup_string(ns->scalar);

    const ConfigNode *stores = config_map_get(root, "stores");
    if (stores && stores->type == CONFIG_NODE_SEQUENCE) {
        out_schema->store_count = stores->item_count;
        out_schema->stores = (ModuleStoreSchema *)calloc(out_schema->store_count, sizeof(ModuleStoreSchema));
        out_schema->type_ids = (int *)calloc(out_schema->store_count, sizeof(int));
        if ((!out_schema->stores || !out_schema->type_ids) && out_schema->store_count > 0) {
            config_node_free(root);
            module_schema_free(out_schema);
            return 0;
        }
        for (size_t i = 0; i < stores->item_count; ++i) {
            const ConfigNode *store = stores->items[i];
            if (!store || store->type != CONFIG_NODE_MAP) continue;
            const ConfigNode *name = config_map_get(store, "name");
            out_schema->stores[i].name = name && name->scalar ? dup_string(name->scalar) : NULL;
            out_schema->stores[i].chunk_capacity = detect_default_chunk_capacity(store);
        }
    }

    config_node_free(root);
    return 1;
}

void module_schema_free(ModuleSchema *schema)
{
    if (!schema) return;
    free(schema->namespace_name);
    for (size_t i = 0; i < schema->store_count; ++i) {
        free(schema->stores[i].name);
    }
    free(schema->stores);
    free(schema->type_ids);
    schema->namespace_name = NULL;
    schema->stores = NULL;
    schema->type_ids = NULL;
    schema->store_count = 0;
}

int module_schema_register(StateManager *manager, const ModuleSchema *schema, int *type_ids_out)
{
    if (!manager || !schema) return 0;
    for (size_t i = 0; i < schema->store_count; ++i) {
        char type_name[128];
        snprintf(type_name, sizeof(type_name), "%s::%s", schema->namespace_name, schema->stores[i].name);
        int type_id = -1;
        StateManagerResult result =
            state_manager_register_type(manager, type_name, sizeof(YamlConfigEntry), schema->stores[i].chunk_capacity,
                                        &type_id);
        if (result != STATE_MANAGER_OK) {
            fprintf(stderr, "module_schema_register: failed to register %s: %s\n", type_name,
                    state_manager_result_message(result));
            return 0;
        }
        if (type_ids_out) type_ids_out[i] = type_id;
        if (schema->type_ids) schema->type_ids[i] = type_id;
    }
    return 1;
}

static int detect_store_type(const ModuleSchema *schema, const char *store)
{
    if (!schema || !store) return -1;
    for (size_t i = 0; i < schema->store_count; ++i) {
        if (schema->stores[i].name && strcmp(schema->stores[i].name, store) == 0) return (int)i;
    }
    return -1;
}

static char *derive_store_from_path(const ModuleSchema *schema, const char *path)
{
    if (!schema || !path) return NULL;
    size_t path_len = strlen(path);
    for (size_t i = 0; i < schema->store_count; ++i) {
        const char *name = schema->stores[i].name;
        if (!name) continue;
        size_t name_len = strlen(name);
        if (name_len == 0) continue;
        for (size_t pos = 0; pos + name_len <= path_len; ++pos) {
            if (path[pos] != '/' && path[pos] != '\\') continue;
            if (strncmp(path + pos + 1, name, name_len) == 0) {
                char next = path[pos + 1 + name_len];
                if (next == '/' || next == '\\') return dup_string(name);
            }
        }
    }
    return NULL;
}

static char *basename_no_ext(const char *path)
{
    const char *slash = strrchr(path, '/');
    const char *name = slash ? slash + 1 : path;
    const char *dot = strrchr(name, '.');
    size_t len = dot ? (size_t)(dot - name) : strlen(name);
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, name, len);
    out[len] = 0;
    return out;
}

static int store_entry(StateManager *manager, int type_id, const ModuleSchema *schema, const char *store, const char *key, const char *path, ConfigDocument *doc);

static int load_document_entry(StateManager *manager, const ModuleSchema *schema, const char *path, ConfigDocument *doc)
{
    const ConfigNode *store_node = config_map_get(doc->root, "store");
    const ConfigNode *key_node = config_map_get(doc->root, "key");
    const char *store = (store_node && store_node->scalar) ? store_node->scalar : NULL;
    char *fallback_store = NULL;
    if (!store) fallback_store = derive_store_from_path(schema, path);
    if (!store) store = fallback_store;
    if (!store && schema->store_count == 1) {
        fallback_store = dup_string(schema->stores[0].name);
        store = fallback_store;
    }
    if (!store && schema->store_count > 0) {
        fallback_store = dup_string(schema->stores[0].name);
        store = fallback_store;
    }
    if (!store) {
        fprintf(stderr, "Config %s missing store name\n", path);
        free(fallback_store);
        return 0;
    }
    int store_idx = detect_store_type(schema, store);
    if (store_idx < 0) {
        fprintf(stderr, "Unknown store '%s' in %s\n", store, path);
        free(fallback_store);
        return 0;
    }
    char *key = key_node && key_node->scalar ? dup_string(key_node->scalar) : basename_no_ext(path);
    int type_id = schema->type_ids ? schema->type_ids[store_idx] : store_idx;
    store_entry(manager, type_id, schema, store, key, path, doc);
    free(key);
    free(fallback_store);
    return 1;
}

static int store_entry(StateManager *manager, int type_id, const ModuleSchema *schema, const char *store, const char *key, const char *path, ConfigDocument *doc)
{
    YamlConfigEntry entry = {0};
    entry.ns = dup_string(schema->namespace_name);
    entry.store = dup_string(store);
    entry.key = dup_string(key);
    entry.source_path = dup_string(path);
    entry.document = *doc;
    state_manager_write(manager, type_id, entry.key, &entry);
    return 1;
}

static int load_single_config(StateManager *manager, const ModuleSchema *schema, const char *path)
{
    ConfigError err = {0};
    ConfigDocument doc = {0};
    if (!load_config_document(path, CONFIG_FORMAT_YAML, &doc, &err)) {
        fprintf(stderr, "Config error %s:%d:%d %s\n", path, err.line, err.column, err.message);
        return 0;
    }

    int result = load_document_entry(manager, schema, path, &doc);
    if (!result) {
        config_document_free(&doc);
    }
    return result;
}

static int module_load_configs_recursive(const ModuleSchema *schema, const char *config_dir, StateManager *manager)
{
    if (!schema || !config_dir || !manager) return 0;
    PlatformDir *dir = platform_dir_open(config_dir);
    if (!dir) return 0;

    PlatformDirEntry ent;
    while (platform_dir_read(dir, &ent)) {
        char *path = join_path(config_dir, ent.name);
        if (!path) {
            free(ent.name);
            continue;
        }
        if (ent.is_dir) {
            module_load_configs_recursive(schema, path, manager);
            free(path);
            free(ent.name);
            continue;
        }
        size_t len = strlen(ent.name);
        if (len >= 6 && strcmp(ent.name + len - 5, ".yaml") == 0) {
            load_single_config(manager, schema, path);
        }
        free(path);
        free(ent.name);
    }
    platform_dir_close(dir);
    return 1;
}

static int module_load_config_bundle(const ModuleSchema *schema, const char *config_file, StateManager *manager)
{
    ConfigError err = {0};
    ConfigNode *root = NULL;
    if (!parse_config(config_file, CONFIG_FORMAT_YAML, &root, &err)) {
        fprintf(stderr, "Config error %s:%d:%d %s\n", config_file, err.line, err.column, err.message);
        return 0;
    }

    const ConfigNode *configs = config_map_get(root, "configs");
    int loaded = 1;
    if (configs && configs->type == CONFIG_NODE_SEQUENCE) {
        for (size_t i = 0; i < configs->item_count; ++i) {
            ConfigDocument doc = {0};
            if (!config_document_from_node(configs->items[i], config_file, &doc) ||
                !load_document_entry(manager, schema, config_file, &doc)) {
                config_document_free(&doc);
                loaded = 0;
                break;
            }
        }
    } else {
        ConfigDocument doc = {0};
        if (!config_document_from_node(root, config_file, &doc) ||
            !load_document_entry(manager, schema, config_file, &doc)) {
            config_document_free(&doc);
            loaded = 0;
        }
    }

    config_node_free(root);
    return loaded;
}

int module_load_configs(const ModuleSchema *schema, const char *config_dir, StateManager *manager)
{
    if (!schema || !config_dir || !manager) return 0;

    PlatformDir *dir = platform_dir_open(config_dir);
    if (dir) {
        platform_dir_close(dir);
        return module_load_configs_recursive(schema, config_dir, manager);
    }

    return module_load_config_bundle(schema, config_dir, manager);
}
