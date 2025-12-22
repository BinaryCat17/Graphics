#ifndef UI_ASSETS_H
#define UI_ASSETS_H

#include <stddef.h>
#include <stdint.h>
#include "foundation/string/string_id.h"

// Forward Declarations
typedef struct SceneAsset SceneAsset;
typedef struct SceneNodeSpec SceneNodeSpec;

SceneAsset* scene_asset_create(size_t arena_size);
void scene_asset_destroy(SceneAsset* asset);

// Access
SceneNodeSpec* scene_asset_get_template(SceneAsset* asset, const char* name);
SceneNodeSpec* scene_asset_get_root(const SceneAsset* asset);

// --- Parser API ---
SceneAsset* scene_asset_load_from_file(const char* path);

#endif // UI_ASSETS_H
