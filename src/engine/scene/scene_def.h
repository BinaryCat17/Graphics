#ifndef SCENE_DEF_H
#define SCENE_DEF_H

#include "foundation/math/coordinate_systems.h"
#include "foundation/meta/reflection.h"

typedef struct Mesh {
    float *positions; // xyz triplets
    size_t position_count;
    unsigned int *indices;
    size_t index_count;
    float aabb_min[3];
    float aabb_max[3];
} Mesh;

typedef enum RenderLayer {
    LAYER_WORLD_OPAQUE = 0,
    LAYER_WORLD_TRANSPARENT,
    LAYER_UI_BACKGROUND,
    LAYER_UI_CONTENT,
    LAYER_UI_OVERLAY,
    LAYER_COUNT
} RenderLayer;

typedef struct SceneCamera {
    Mat4 view_matrix;
    Mat4 proj_matrix;
    // Viewport rect?
} SceneCamera;

typedef struct SceneObject {
    int id;
    RenderLayer layer;
    
    // Transform
    Vec3 position;
    Vec3 rotation; // Euler angles or Quaternion
    Vec3 scale;
    
    // Visuals
    const Mesh* mesh; // Geometry
    // Material* material; // TODO: Define Material struct
    Vec4 color; // Simple color support
    
    // Instancing (Data-Driven Visualization)
    // If instance_count > 0, this object is a template
    void* instance_buffer; // Pointer to GpuBuffer (TBD)
    size_t instance_count;
    
    // UI Specifics (Optional, could be component)
    // bool is_interactive;
    // ...
} SceneObject;

typedef struct Scene {
    SceneObject* objects; // Array of structs, not pointers
    size_t object_count;
    size_t object_capacity;
    
    SceneCamera camera;
} Scene;

// API
void scene_init(Scene* scene);
void scene_add_object(Scene* scene, SceneObject obj); // Pass by value
void scene_clear(Scene* scene);

#endif // SCENE_DEF_H
