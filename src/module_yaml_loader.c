#include "module_yaml_loader.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static size_t detect_default_chunk_capacity(const SimpleYamlNode *node)
{
    const SimpleYamlNode *cap = simple_yaml_map_get(node, "chunk_capacity");
    if (cap && cap->scalar) return (size_t)strtoul(cap->scalar, NULL, 10);
    return 16;
}

int module_schema_load(const char *schema_path, ModuleSchema *out_schema, SimpleYamlError *err)
{
    if (!schema_path || !out_schema) return 0;
    memset(out_schema, 0, sizeof(*out_schema));

    FILE *f = fopen(schema_path, "rb");
    if (!f) {
        assign_error(err, 0, 0, "Failed to open schema file");
        return 0;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *text = (char *)malloc((size_t)len + 1);
    if (!text) {
        fclose(f);
        return 0;
    }
    fread(text, 1, (size_t)len, f);
    text[len] = 0;
    fclose(f);

    SimpleYamlNode *root = NULL;
    if (!simple_yaml_parse(text, &root, err)) {
        free(text);
        return 0;
    }
    free(text);

    const SimpleYamlNode *ns = simple_yaml_map_get(root, "namespace");
    if (!ns || !ns->scalar) {
        assign_error(err, root ? root->line : 0, 1, "Schema missing namespace");
        simple_yaml_free(root);
        return 0;
    }
    out_schema->namespace_name = dup_string(ns->scalar);

    const SimpleYamlNode *stores = simple_yaml_map_get(root, "stores");
    if (stores && stores->type == SIMPLE_YAML_SEQUENCE) {
        out_schema->store_count = stores->item_count;
        out_schema->stores = (ModuleStoreSchema *)calloc(out_schema->store_count, sizeof(ModuleStoreSchema));
        out_schema->type_ids = (int *)calloc(out_schema->store_count, sizeof(int));
        if ((!out_schema->stores || !out_schema->type_ids) && out_schema->store_count > 0) {
            simple_yaml_free(root);
            module_schema_free(out_schema);
            return 0;
        }
        for (size_t i = 0; i < stores->item_count; ++i) {
            const SimpleYamlNode *store = stores->items[i];
            if (!store || store->type != SIMPLE_YAML_MAP) continue;
            const SimpleYamlNode *name = simple_yaml_map_get(store, "name");
            out_schema->stores[i].name = name && name->scalar ? dup_string(name->scalar) : NULL;
            out_schema->stores[i].chunk_capacity = detect_default_chunk_capacity(store);
        }
    }

    simple_yaml_free(root);
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
        int type_id = state_manager_register_type(manager, type_name, sizeof(YamlConfigEntry), schema->stores[i].chunk_capacity);
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

static char *read_file_text(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *text = (char *)malloc((size_t)len + 1);
    if (!text) {
        fclose(f);
        return NULL;
    }
    fread(text, 1, (size_t)len, f);
    text[len] = 0;
    fclose(f);
    return text;
}

int load_yaml_file_as_json(const char *path, char **out_json, SimpleYamlError *err)
{
    if (!path || !out_json) return 0;
    char *text = read_file_text(path);
    if (!text) return 0;
    SimpleYamlNode *root = NULL;
    if (!simple_yaml_parse(text, &root, err)) {
        free(text);
        return 0;
    }
    free(text);
    int ok = simple_yaml_emit_json(root, out_json);
    simple_yaml_free(root);
    return ok;
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

static int store_entry(StateManager *manager, int type_id, const ModuleSchema *schema, const char *store, const char *key, const char *path, SimpleYamlNode *root)
{
    YamlConfigEntry entry = {0};
    entry.ns = dup_string(schema->namespace_name);
    entry.store = dup_string(store);
    entry.key = dup_string(key);
    entry.source_path = dup_string(path);
    entry.root = root;
    simple_yaml_emit_json(root, &entry.json_text);
    state_manager_write(manager, type_id, entry.key, &entry);
    return 1;
}

static int load_single_config(StateManager *manager, const ModuleSchema *schema, const char *path)
{
    SimpleYamlError err = {0};
    char *text = read_file_text(path);
    if (!text) return 0;
    SimpleYamlNode *root = NULL;
    if (!simple_yaml_parse(text, &root, &err)) {
        fprintf(stderr, "Config YAML error %s:%d:%d %s\n", path, err.line, err.column, err.message);
        free(text);
        return 0;
    }
    free(text);

    const SimpleYamlNode *store_node = simple_yaml_map_get(root, "store");
    const SimpleYamlNode *key_node = simple_yaml_map_get(root, "key");
    const SimpleYamlNode *data_node = simple_yaml_map_get(root, "data");
    const char *store = (store_node && store_node->scalar) ? store_node->scalar : NULL;
    char *fallback_store = NULL;
    if (!store && schema->store_count == 1) {
        fallback_store = dup_string(schema->stores[0].name);
        store = fallback_store;
    }
    if (!store) {
        fprintf(stderr, "Config %s missing store name\n", path);
        simple_yaml_free(root);
        free(fallback_store);
        return 0;
    }
    int store_idx = detect_store_type(schema, store);
    if (store_idx < 0) {
        fprintf(stderr, "Unknown store '%s' in %s\n", store, path);
        simple_yaml_free(root);
        free(fallback_store);
        return 0;
    }
    char *key = key_node && key_node->scalar ? dup_string(key_node->scalar) : basename_no_ext(path);
    const SimpleYamlNode *payload = data_node ? data_node : root;
    int type_id = schema->type_ids ? schema->type_ids[store_idx] : store_idx;
    store_entry(manager, type_id, schema, store, key, path, payload ? (SimpleYamlNode *)payload : root);
    free(key);
    free(fallback_store);
    return 1;
}

int module_load_configs(const ModuleSchema *schema, const char *config_dir, StateManager *manager)
{
    if (!schema || !config_dir || !manager) return 0;
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
    return 1;
}

static void assign_error(SimpleYamlError *err, int line, int column, const char *msg)
{
    if (!err) return;
    err->line = line;
    err->column = column;
    strncpy(err->message, msg, sizeof(err->message) - 1);
    err->message[sizeof(err->message) - 1] = 0;
}

