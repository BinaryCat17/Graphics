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

// Standard Rendering Modes for UI/2D Shader
typedef enum SceneShaderMode {
    SCENE_MODE_SOLID        = 0, // Solid Color
    SCENE_MODE_TEXTURED     = 1, // Font/Bitmap
    SCENE_MODE_USER_TEXTURE = 2, // Compute Result/Image
    SCENE_MODE_9_SLICE      = 3, // UI Panel
    SCENE_MODE_SDF_BOX      = 4  // Rounded Box
} SceneShaderMode;

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
    // Generic storage for per-instance shader data.
    // Usage is defined by the specific renderer/shader (e.g., UI, Graph, etc.)
    Vec4 shader_params_0; 
    Vec4 shader_params_1; 

    
    // Instancing (Data-Driven Visualization)
    void* instance_buffer; // Pointer to GpuBuffer (if massive instancing)
    size_t instance_count;
} SceneObject;

// --- The Scene Container ---

typedef struct Scene Scene;

// --- API ---

// Lifecycle
Scene* scene_create(void);
void scene_destroy(Scene* scene);

void scene_clear(Scene* scene);
void scene_add_object(Scene* scene, SceneObject obj); // Pass by value (copy)

// Accessors
void scene_set_camera(Scene* scene, SceneCamera camera);
SceneCamera scene_get_camera(const Scene* scene);

void scene_set_frame_number(Scene* scene, uint64_t frame_number);
uint64_t scene_get_frame_number(const Scene* scene);

// Returns pointer to internal array and sets out_count. Do not free.
const SceneObject* scene_get_all_objects(const Scene* scene, size_t* out_count);

#endif // SCENE_H
