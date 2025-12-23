#include "scene_asset.h"
#include "internal/scene_tree_internal.h"
#include "internal/scene_loader.h"
#include "foundation/memory/arena.h"
#include <stdlib.h>
#include <string.h>

SceneAsset* scene_asset_create(size_t arena_size) {
    SceneAsset* asset = (SceneAsset*)calloc(1, sizeof(SceneAsset));
    if (!asset) return NULL;
    if (!arena_init(&asset->arena, arena_size)) {
        free(asset);
        return NULL;
    }
    return asset;
}

void scene_asset_destroy(SceneAsset* asset) {
    if (!asset) return;
    arena_destroy(&asset->arena);
    free(asset);
}

SceneAsset* scene_asset_load_from_file(const char* path) {
    return scene_internal_asset_load_from_file(path);
}

SceneNodeSpec* scene_asset_push_node(SceneAsset* asset) {
    if (!asset) return NULL;
    return (SceneNodeSpec*)arena_alloc_zero(&asset->arena, sizeof(SceneNodeSpec));
}

SceneNodeSpec* scene_asset_get_template(SceneAsset* asset, const char* name) {
    if (!asset || !name) return NULL;
    SceneTemplate* t = asset->templates;
    while (t) {
        if (t->name && strcmp(t->name, name) == 0) return t->spec;
        t = t->next;
    }
    return NULL;
}

SceneNodeSpec* scene_asset_get_root(const SceneAsset* asset) {
    return asset ? asset->root : NULL;
}
