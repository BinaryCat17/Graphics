#ifndef ASSETS_INTERNAL_H
#define ASSETS_INTERNAL_H

#include "../assets.h"
#include "foundation/memory/arena.h"
#include "engine/scene/render_packet.h"

struct Assets {
    MemoryArena arena; // For storing paths and metadata

    // Resource Paths
    const char* root_dir;
    
    // Built-in Resources
    Mesh unit_quad;
    Font* font;
};

#endif // ASSETS_INTERNAL_H
