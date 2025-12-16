#pragma once

#include <stddef.h>

typedef enum SimpleYamlNodeType {
    SIMPLE_YAML_UNKNOWN,
    SIMPLE_YAML_SCALAR,
    SIMPLE_YAML_MAP,
    SIMPLE_YAML_SEQUENCE,
} SimpleYamlNodeType;

typedef struct SimpleYamlNode SimpleYamlNode;

typedef struct SimpleYamlPair {
    char *key;
    SimpleYamlNode *value;
} SimpleYamlPair;

typedef struct SimpleYamlNode {
    SimpleYamlNodeType type;
    int line;
    char *scalar;
    SimpleYamlPair *pairs;
    size_t pair_count;
    size_t pair_capacity;
    SimpleYamlNode **items;
    size_t item_count;
    size_t item_capacity;
} SimpleYamlNode;

typedef struct SimpleYamlError {
    int line;
    int column;
    char message[128];
} SimpleYamlError;

int simple_yaml_parse(const char *text, SimpleYamlNode **out_root, SimpleYamlError *err);
const SimpleYamlNode *simple_yaml_map_get(const SimpleYamlNode *map, const char *key);
void simple_yaml_free(SimpleYamlNode *node);
int simple_yaml_emit_json(const SimpleYamlNode *node, char **out_json);

