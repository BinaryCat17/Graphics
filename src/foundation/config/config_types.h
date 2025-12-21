#ifndef CONFIG_TYPES_H
#define CONFIG_TYPES_H

#include <stddef.h>

// Forward Declarations
typedef struct MemoryArena MemoryArena;

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

typedef struct ConfigError {
    int line;
    int column;
    char message[128];
} ConfigError;

// Generic tree traversal
const ConfigNode *config_node_map_get(const ConfigNode *map, const char *key);

void config_node_free(ConfigNode *node);

#endif // CONFIG_TYPES_H
