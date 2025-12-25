#include "asset_storage.h"
#include "asset_loader.h"
#include "foundation/logger/logger.h"
#include "engine/text/font.h"
#include <string.h>
#include <stdio.h>

bool asset_storage_init(Assets* assets, const char* root_dir) {
    if (!assets) return false;

    memset(assets, 0, sizeof(*assets));

    // Initialize Arena (4KB is likely enough for just paths)
    if (!arena_init(&assets->arena, 4096)) {
        LOG_FATAL("Assets: Failed to initialize memory arena.");
        return false;
    }

    assets->root_dir = arena_push_string(&assets->arena, root_dir);
    
    // Load built-in primitives
    asset_loader_create_primitives(assets);

    // Load Default Font
    char font_path[512];
    snprintf(font_path, sizeof(font_path), "%s/fonts/font.ttf", assets->root_dir);
    assets->font = asset_loader_load_font(font_path);

    LOG_INFO("Assets: Initialized storage with root '%s'", root_dir);
    return true;
}

void asset_storage_shutdown(Assets* assets) {
    if (!assets) return;

    if (assets->font) {
        font_destroy(assets->font);
    }
    
    // Scenes are likely managed/freed elsewhere or need a specific free function 
    // if scene_asset_destroy exists. 
    // Assuming for now scene assets are part of the larger asset lifecycle or 
    // freed when the arena/pool they belong to is destroyed, 
    // BUT looking at scene_asset.h might be needed later.
    // For now, we just clear the struct.

    arena_destroy(&assets->arena);
}

SceneAsset* asset_storage_get_scene(Assets* assets, StringId path_id) {
    for (size_t i = 0; i < assets->cached_scene_count; ++i) {
        if (assets->cached_scenes[i].path_id == path_id) {
            return assets->cached_scenes[i].asset;
        }
    }
    return NULL;
}

void asset_storage_put_scene(Assets* assets, StringId path_id, SceneAsset* scene) {
    if (assets->cached_scene_count < MAX_CACHED_SCENES) {
        assets->cached_scenes[assets->cached_scene_count].path_id = path_id;
        assets->cached_scenes[assets->cached_scene_count].asset = scene;
        assets->cached_scene_count++;
        LOG_TRACE("Assets: Cached scene (Total: %zu)", assets->cached_scene_count);
    } else {
        LOG_WARN("Assets: Cache full, scene not cached.");
    }
}
