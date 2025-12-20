#ifndef UI_ASSETS_H
#define UI_ASSETS_H

#include <stddef.h>
#include <stdint.h>
#include "foundation/string/string_id.h"

// Forward Declarations
typedef struct UiAsset UiAsset;
typedef struct UiNodeSpec UiNodeSpec;
typedef struct UiTemplate UiTemplate;

// --- UI SPECIFICATION (The DNA) ---
// Pure data. Allocated inside a UiAsset arena. Read-only at runtime.
// Note: Actual struct definitions are internal.

// --- UI ASSET (The Resource) ---
// Owns the memory. Created by the Parser.

// API for Asset
UiAsset* ui_asset_create(size_t arena_size);
void ui_asset_free(UiAsset* asset);
UiNodeSpec* ui_asset_push_node(UiAsset* asset);
UiNodeSpec* ui_asset_get_template(UiAsset* asset, const char* name);
UiNodeSpec* ui_asset_get_root(const UiAsset* asset);

// --- Parser API ---
UiAsset* ui_parser_load_from_file(const char* path);

#endif // UI_ASSETS_H
