#pragma once

#include <stddef.h>
#include "foundation/memory/arena.h"

typedef enum ConfigNodeType {
    CONFIG_NODE_UNKNOWN,
    CONFIG_NODE_SCALAR,
    CONFIG_NODE_MAP,
    CONFIG_NODE_SEQUENCE,
} ConfigNodeType;

typedef struct ConfigNode ConfigNode;

typedef struct ConfigPair {
    char *key;
    ConfigNode *value;
} ConfigPair;

struct ConfigNode {
    ConfigNodeType type;
    int line;
    char *scalar;
    ConfigPair *pairs;
    size_t pair_count;
    size_t pair_capacity;
    ConfigNode **items;
    size_t item_count;
    size_t item_capacity;
};
// Wait, earlier on line 13 it says 'typedef struct ConfigNode ConfigNode;'.
// So I should change this block to 'struct ConfigNode { ... };'


typedef struct ConfigError {
    int line;
    int column;
    char message[128];
} ConfigError;

// Parser specifically for YAML, but produces a generic ConfigNode tree
int simple_yaml_parse(MemoryArena* arena, const char *text, ConfigNode **out_root, ConfigError *err);

// Generic tree traversal
const ConfigNode *config_node_map_get(const ConfigNode *map, const char *key);

void config_node_free(ConfigNode *node);

// Utility
int config_node_emit_json(const ConfigNode *node, char **out_json);

