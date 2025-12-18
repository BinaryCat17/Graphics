#ifndef SCENE_H
#define SCENE_H

#include "foundation/math/coordinate_systems.h"
#include <stddef.h> // size_t
#include <stdint.h> // uint64_t

// --- Basic Types ---

// Simple mesh descriptor for the Unified Scene
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

typedef enum ScenePrimitiveType {
    SCENE_PRIM_QUAD = 0, // Standard Mesh/Quad
    SCENE_PRIM_CURVE = 1 // SDF Bezier Curve
} ScenePrimitiveType;

// --- Scene Components ---

typedef struct SceneCamera {
    Mat4 view_matrix;
    Mat4 proj_matrix;
} SceneCamera;

typedef struct SceneObject {
    int id;
    RenderLayer layer;
    ScenePrimitiveType prim_type;
    
    // Transform
    Vec3 position;
    Vec3 rotation; 
    Vec3 scale;
    
    // Visuals
    const Mesh* mesh; 
    Vec4 color; 
    Vec4 uv_rect; // Texture Subset (xy=off, zw=scale)
    Vec4 clip_rect; // Clipping bounds (x,y,w,h). 0,0,0,0 means no clipping.

    // Unified Parameters (Maps to shader 'params' and 'extra')
    union {
        // Raw Access (Fast copy to GPU)
        struct {
            Vec4 params; // x=tex_id/type
            Vec4 extra;  // 9-slice borders, etc.
        };

        // UI Semantics
        struct {
            // Params
            float texture_id;       // params.x
            float _ui_unused;       // params.y
            float tex_width;        // params.z
            float tex_height;       // params.w
            
            // Extra
            float border_top;       // extra.x
            float border_right;     // extra.y
            float border_bottom;    // extra.z
            float border_left;      // extra.w
        } ui_style;
    };
    
    // Instancing (Data-Driven Visualization)
    void* instance_buffer; // Pointer to GpuBuffer (if massive instancing)
    size_t instance_count;
} SceneObject;

// --- The Scene Container ---

typedef struct Scene {
    SceneObject* objects; // Linear array
    size_t object_count;
    size_t object_capacity;
    
    SceneCamera camera;
    
    uint64_t frame_number;
} Scene;

// --- API ---

void scene_init(Scene* scene);
void scene_add_object(Scene* scene, SceneObject obj); // Pass by value (copy)
void scene_clear(Scene* scene);

#endif // SCENE_H
