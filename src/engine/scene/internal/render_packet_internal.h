#ifndef RENDER_PACKET_INTERNAL_H
#define RENDER_PACKET_INTERNAL_H

#include "../render_packet.h"
#include "foundation/memory/arena.h"

// The Scene Container implementation
struct Scene {
    // Memory Arena for frame-local scene objects
    // This replaces the malloc/realloc dynamic array pattern
    MemoryArena arena;

    // Pointer to the start of the object array in the arena
    SceneObject* objects;
    
    // Number of objects currently in the arena
    size_t object_count;
    
    SceneCamera camera;
    
    uint64_t frame_number;
};

#endif // RENDER_PACKET_INTERNAL_H
