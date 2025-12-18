#ifndef UI_LOADER_H
#define UI_LOADER_H

#include "ui_def.h"

// Loads a UI definition from a YAML file.
// The returned UiDef owns a memory arena that contains all its nodes and strings.
// Use ui_def_free() to destroy the entire tree.
UiDef* ui_loader_load_from_file(const char* path);

#endif // UI_LOADER_H