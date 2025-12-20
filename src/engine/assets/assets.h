#ifndef ASSETS_SYSTEM_H
#define ASSETS_SYSTEM_H

#include <stdbool.h>
#include "engine/graphics/scene/scene.h"
#include "foundation/memory/arena.h"

typedef struct Assets {
    MemoryArena arena; // For storing paths and metadata

    // Paths (Legacy/Config)
    const char* root_dir;
    const char* ui_default_vert_spv;
    const char* ui_default_frag_spv;
    const char* font_path;
    
    // Built-in Resources
    Mesh unit_quad;
} Assets;

// Public API
bool assets_init(Assets* assets, const char* assets_dir, const char* ui_config_path);
void assets_shutdown(Assets* assets);

#endif // ASSETS_SYSTEM_H
