#ifndef ASSET_LOADER_H
#define ASSET_LOADER_H

#include "assets_internal.h"

// Raw I/O
AssetData asset_loader_read_file(const char* full_path);
void asset_loader_free_file(AssetData* data);

// Specific Loaders
void asset_loader_create_primitives(Assets* assets); // Generates unit quad etc.
Font* asset_loader_load_font(const char* full_path);
SceneAsset* asset_loader_load_scene_from_disk(const char* full_path);

#endif // ASSET_LOADER_H
