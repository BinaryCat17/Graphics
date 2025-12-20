#ifndef SCENE_INTERNAL_H
#define SCENE_INTERNAL_H

#include "../scene.h"

// The Scene Container implementation
struct Scene {
    SceneObject* objects; // Linear array
    size_t object_count;
    size_t object_capacity;
    
    SceneCamera camera;
    
    uint64_t frame_number;
};

#endif // SCENE_INTERNAL_H
