#pragma once

#include <stddef.h>

typedef enum ConfigFormat {
    CONFIG_FORMAT_YAML,
    CONFIG_FORMAT_JSON,
} ConfigFormat;

typedef enum ConfigNodeType {
    CONFIG_NODE_SCALAR,
    CONFIG_NODE_MAP,
    CONFIG_NODE_SEQUENCE,
} ConfigNodeType;

typedef enum ConfigScalarType {
    CONFIG_SCALAR_STRING,
    CONFIG_SCALAR_NUMBER,
    CONFIG_SCALAR_BOOL,
    CONFIG_SCALAR_NULL,
} ConfigScalarType;

typedef struct ConfigError {
    int line;
    int column;
    char message[256];
} ConfigError;

typedef struct ConfigNode ConfigNode;

typedef struct ConfigPair {
    char *key;
    ConfigNode *value;
} ConfigPair;

struct ConfigNode {
    ConfigNodeType type;
    int line;
    char *scalar;
    ConfigScalarType scalar_type;
    ConfigPair *pairs;
    size_t pair_count;
    size_t pair_capacity;
    ConfigNode **items;
    size_t item_count;
    size_t item_capacity;
};

typedef struct ConfigDocument {
    ConfigFormat format;
    char *source_path;
    ConfigNode *root;
} ConfigDocument;

const ConfigNode *config_map_get(const ConfigNode *map, const char *key);
void config_node_free(ConfigNode *node);
void config_document_free(ConfigDocument *doc);

int config_emit_json(const ConfigNode *node, char **out_json);

int load_config_document(const char *path, ConfigFormat format, ConfigDocument *out_doc, ConfigError *err);

