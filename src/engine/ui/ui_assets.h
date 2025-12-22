#ifndef UI_ASSETS_H
#define UI_ASSETS_H

#include <stddef.h>
#include <stdint.h>
#include "foundation/string/string_id.h"

// Forward Declarations
typedef struct UiAsset UiAsset;
typedef struct SceneNodeSpec SceneNodeSpec;

UiAsset* ui_asset_create(size_t arena_size);
void ui_asset_destroy(UiAsset* asset);

// Access
SceneNodeSpec* ui_asset_get_template(UiAsset* asset, const char* name);
SceneNodeSpec* ui_asset_get_root(const UiAsset* asset);

// --- Parser API ---
UiAsset* ui_parser_load_from_file(const char* path);

#endif // UI_ASSETS_H
