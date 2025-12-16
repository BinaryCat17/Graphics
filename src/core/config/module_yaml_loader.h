#pragma once

#include "core/config/config_document.h"
#include "core/state/state_manager.h"

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
    ConfigDocument document;
} YamlConfigEntry;

int module_schema_load(const char *schema_path, ModuleSchema *out_schema, ConfigError *err);
void module_schema_free(ModuleSchema *schema);
int module_schema_register(StateManager *manager, const ModuleSchema *schema, int *type_ids_out);

int module_load_configs(const ModuleSchema *schema, const char *config_dir, StateManager *manager);

