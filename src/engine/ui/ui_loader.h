#ifndef UI_LOADER_H
#define UI_LOADER_H

#include "engine/ui/ui_def.h"

// Loads a UiDef hierarchy from a YAML file.
// Returns NULL on failure and prints errors to stderr.
UiDef* ui_loader_load_from_file(const char* path);

// Loads a UiDef hierarchy from a generic ConfigNode (useful for partial loading or embedded configs).
UiDef* ui_loader_load_from_node(const void* config_node);

#endif // UI_LOADER_H
