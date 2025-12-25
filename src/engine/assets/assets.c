#include "engine/assets/assets.h"
#include "engine/assets/internal/assets_internal.h"
#include "engine/assets/internal/asset_storage.h"
#include "engine/assets/internal/asset_loader.h"

#include <stdlib.h>
#include <stdio.h>

Assets* assets_create(const char* assets_dir) {
    Assets* assets = (Assets*)calloc(1, sizeof(Assets));
    if (!assets) return NULL;
    
    if (!asset_storage_init(assets, assets_dir)) {
        free(assets);
        return NULL;
    }
    return assets;
}

void assets_destroy(Assets* assets) {
    if (!assets) return;
    asset_storage_shutdown(assets);
    free(assets);
}

SceneAsset* assets_load_scene(Assets* assets, const char* relative_path) {
    if (!assets || !relative_path) return NULL;

    StringId id = str_id(relative_path);

    // 1. Check Cache
    SceneAsset* cached = asset_storage_get_scene(assets, id);
    if (cached) return cached;

    // 2. Load
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s/%s", assets->root_dir, relative_path);

    SceneAsset* asset = asset_loader_load_scene_from_disk(full_path);
    
    // 3. Cache
    if (asset) {
        asset_storage_put_scene(assets, id, asset);
    }

    return asset;
}

AssetData assets_load_file(const Assets* assets, const char* relative_path) {
    // Construct full path
    char full_path[512];
    if (assets && relative_path) {
        snprintf(full_path, sizeof(full_path), "%s/%s", assets->root_dir, relative_path);
        return asset_loader_read_file(full_path);
    }
    return (AssetData){0};
}

void assets_free_file(AssetData* data) {
    asset_loader_free_file(data);
}

const char* assets_get_root_dir(const Assets* assets) {
    return assets ? assets->root_dir : NULL;
}

const Mesh* assets_get_unit_quad(const Assets* assets) {
    return assets ? &assets->unit_quad : NULL;
}

const Font* assets_get_font(const Assets* assets) {
    return assets ? assets->font : NULL;
}