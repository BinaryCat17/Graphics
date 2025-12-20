#ifndef ASSETS_INTERNAL_H
#define ASSETS_INTERNAL_H

#include "../assets.h"
#include "foundation/memory/arena.h"

struct Assets {
    MemoryArena arena; // For storing paths and metadata

    // Resource Paths
    const char* root_dir;
    const char* ui_default_vert_spv;
    const char* ui_default_frag_spv;
    const char* font_path;
    
    // Built-in Resources
    Mesh unit_quad;
};

#endif // ASSETS_INTERNAL_H
