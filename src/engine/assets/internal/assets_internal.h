#ifndef ASSETS_INTERNAL_H
#define ASSETS_INTERNAL_H

#include "../assets.h"
#include "foundation/memory/arena.h"
#include "foundation/string/string_id.h"
#include "engine/scene/render_packet.h"
#include "engine/scene/scene.h"

#define MAX_CACHED_SCENES 64

typedef struct CachedScene {
    StringId path_id;
    SceneAsset* asset;
} CachedScene;

typedef struct Assets {
    MemoryArena arena; // For storing paths and metadata

    // Resource Paths
    const char* root_dir;
    
    // Built-in Resources
    Mesh unit_quad;
    Font* font;

    // Cache
    CachedScene cached_scenes[MAX_CACHED_SCENES];
    size_t cached_scene_count;
} Assets;

#endif // ASSETS_INTERNAL_H
