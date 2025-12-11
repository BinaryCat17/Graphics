#include "config/module_yaml_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

#include "config/config_io.h"

static void assign_error(ConfigError *err, int line, int column, const char *msg)
{
    if (!err) return;
    err->line = line;
    err->column = column;
    strncpy(err->message, msg, sizeof(err->message) - 1);
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

    const ConfigNode *store_node = config_map_get(doc.root, "store");
    const ConfigNode *key_node = config_map_get(doc.root, "key");
    const ConfigNode *data_node = config_map_get(doc.root, "data");
    const char *store = (store_node && store_node->scalar) ? store_node->scalar : NULL;
    char *fallback_store = NULL;
    if (!store && data_node && data_node->type == CONFIG_NODE_MAP) {
        // If the document omits the explicit store name, try to infer it from the
        // keys present under the data section to support unified UI config files.
        for (size_t i = 0; i < schema->store_count; ++i) {
            const char *candidate = schema->stores[i].name;
            if (config_map_get(data_node, candidate)) {
                store = candidate;
                break;
            }
        }
        if (!store) {
            // Heuristics for common UI config layouts where the section name does
            // not exactly match the schema store name.
            if (config_map_get(data_node, "layout") || config_map_get(data_node, "widgets") ||
                config_map_get(data_node, "floating")) {
                store = "layout";
            } else if (config_map_get(data_node, "styles")) {
                store = "styles";
            } else if (config_map_get(data_node, "model")) {
                store = "model";
            }
        }
    }
    if (!store && schema->store_count == 1) {
        fallback_store = dup_string(schema->stores[0].name);
        store = fallback_store;
    }
    if (!store && schema->store_count > 0) {
        // As a last resort, pick the first store from the schema to keep the
        // configuration usable instead of hard failing.
        fallback_store = dup_string(schema->stores[0].name);
        store = fallback_store;
    }
    if (!store) {
        fprintf(stderr, "Config %s missing store name\n", path);
        config_document_free(&doc);
        free(fallback_store);
        return 0;
    }
    int store_idx = detect_store_type(schema, store);
    if (store_idx < 0) {
        fprintf(stderr, "Unknown store '%s' in %s\n", store, path);
        config_document_free(&doc);
        free(fallback_store);
        return 0;
    }
    char *key = key_node && key_node->scalar ? dup_string(key_node->scalar) : basename_no_ext(path);
    int type_id = schema->type_ids ? schema->type_ids[store_idx] : store_idx;
    store_entry(manager, type_id, schema, store, key, path, &doc);
    free(key);
    free(fallback_store);
    return 1;
}

int module_load_configs(const ModuleSchema *schema, const char *config_dir, StateManager *manager)
{
    if (!schema || !config_dir || !manager) return 0;
#ifdef _WIN32
    size_t dir_len = strlen(config_dir);
    while (dir_len > 0 && (config_dir[dir_len - 1] == '/' || config_dir[dir_len - 1] == '\\')) dir_len--;
    size_t pattern_len = dir_len + 3; // "\\*" + null
    char *pattern = (char *)malloc(pattern_len);
    if (!pattern) return 0;
    memcpy(pattern, config_dir, dir_len);
    pattern[dir_len] = '\\';
    pattern[dir_len + 1] = '*';
    pattern[dir_len + 2] = 0;

    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA(pattern, &ffd);
    free(pattern);
    if (hFind == INVALID_HANDLE_VALUE) return 0;

    do {
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        const char *name = ffd.cFileName;
        size_t len = strlen(name);
        if (len < 6 || strcmp(name + len - 5, ".yaml") != 0) continue;
        char *path = join_path(config_dir, name);
        load_single_config(manager, schema, path);
        free(path);
    } while (FindNextFileA(hFind, &ffd));
    FindClose(hFind);
#else
    DIR *dir = opendir(config_dir);
    if (!dir) return 0;
    struct dirent *ent = NULL;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_type == DT_DIR) continue;
        const char *name = ent->d_name;
        size_t len = strlen(name);
        if (len < 6 || strcmp(name + len - 5, ".yaml") != 0) continue;
        char *path = join_path(config_dir, name);
        load_single_config(manager, schema, path);
        free(path);
    }
    closedir(dir);
#endif
    return 1;
}
