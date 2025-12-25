#ifndef ASSET_STORAGE_H
#define ASSET_STORAGE_H

#include "assets_internal.h"

// Lifecycle
bool asset_storage_init(Assets* assets, const char* root_dir);
void asset_storage_shutdown(Assets* assets);

// Cache Management
SceneAsset* asset_storage_get_scene(Assets* assets, StringId path_id);
void asset_storage_put_scene(Assets* assets, StringId path_id, SceneAsset* scene);

#endif // ASSET_STORAGE_H
