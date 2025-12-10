#pragma once

#include "simple_yaml.h"
#include "state_manager.h"

typedef struct ModuleStoreSchema {
    char *name;
    size_t chunk_capacity;
} ModuleStoreSchema;

typedef struct ModuleSchema {
    char *namespace_name;
    ModuleStoreSchema *stores;
    size_t store_count;
    int *type_ids;
} ModuleSchema;

typedef struct YamlConfigEntry {
    char *ns;
    char *store;
    char *key;
    char *source_path;
    SimpleYamlNode *root;
    char *json_text;
} YamlConfigEntry;

int module_schema_load(const char *schema_path, ModuleSchema *out_schema, SimpleYamlError *err);
void module_schema_free(ModuleSchema *schema);
int module_schema_register(StateManager *manager, const ModuleSchema *schema, int *type_ids_out);

int module_load_configs(const ModuleSchema *schema, const char *config_dir, StateManager *manager);
int load_yaml_file_as_json(const char *path, char **out_json, SimpleYamlError *err);

