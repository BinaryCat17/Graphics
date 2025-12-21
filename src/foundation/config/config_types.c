#include "config_types.h"
#include <string.h>

const ConfigNode *config_node_map_get(const ConfigNode *map, const char *key)
{
    if (!map || map->type != CONFIG_NODE_MAP) return NULL;
    for (size_t i = 0; i < map->pair_count; ++i) {
        if (map->pairs[i].key && strcmp(map->pairs[i].key, key) == 0) return map->pairs[i].value;
    }
    return NULL;
}

void config_node_free(ConfigNode *node)
{
    // No-op: Memory is managed by the Arena
    (void)node;
}
