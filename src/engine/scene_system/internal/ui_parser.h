#ifndef UI_PARSER_H
#define UI_PARSER_H

#include "../ui_core.h"
#include "../ui_assets.h"

// INTERNAL API
// Loads a UI Asset from a YAML file. 
// This function is implemented in ui_parser.c and wrapped by ui_core.c
SceneAsset* scene_asset_load_internal(const char* path);

#endif // UI_PARSER_H

