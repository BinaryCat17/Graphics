#ifndef UI_RENDERER_INTERNAL_H
#define UI_RENDERER_INTERNAL_H

// --- INTERNAL HEADER: Do not include in public API ---
// Use ui_core.h (ui_instance_render) instead.

#include <stddef.h> // for NULL, size_t, etc. if needed (though strictly not needed for opaque pointers, good practice)

// Forward Declarations
typedef struct SceneNode SceneNode;
typedef struct Scene Scene;
typedef struct Assets Assets;
typedef struct MemoryArena MemoryArena;

// Traverses the SceneNode tree and populates the Scene with renderable objects.
void scene_builder_build(const SceneNode* root, Scene* scene, const Assets* assets, MemoryArena* arena);

#endif // UI_RENDERER_INTERNAL_H
