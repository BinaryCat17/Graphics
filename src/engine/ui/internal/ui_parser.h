#ifndef UI_PARSER_H
#define UI_PARSER_H

#include "../ui_core.h"

// INTERNAL API
// Loads a UI Asset from a YAML file. 
// This function is implemented in ui_parser.c and wrapped by ui_core.c
UiAsset* ui_parser_load_internal(const char* path);

#endif // UI_PARSER_H

