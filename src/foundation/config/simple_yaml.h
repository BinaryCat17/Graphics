#ifndef SIMPLE_YAML_H
#define SIMPLE_YAML_H

#include "config_types.h"

// Parser specifically for YAML, produces a generic ConfigNode tree
int simple_yaml_parse(MemoryArena* arena, const char *text, ConfigNode **out_root, ConfigError *err);

#endif // SIMPLE_YAML_H
