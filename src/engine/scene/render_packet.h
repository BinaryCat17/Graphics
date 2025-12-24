#ifndef RENDER_PACKET_H
#define RENDER_PACKET_H

#include "foundation/math/coordinate_systems.h"
#include <stddef.h> // size_t
#include <stdint.h> // uint64_t

#include "engine/ui/ui_node.h"
#include "engine/graphics/render_batch.h"

// --- Basic Types ---

// Simple mesh descriptor for the Unified Scene
typedef struct Mesh {
    float *positions; // xyz triplets
    size_t position_count;
    float *uvs; // uv pairs
    size_t uv_count;
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
    SCENE_PRIM_CURVE = 1, // SDF Bezier Curve
    SCENE_PRIM_CUSTOM = 2 // Custom Pipeline / Zero-Copy
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

// --- The Scene Container ---

typedef struct Scene Scene;

// --- API ---

// Lifecycle
Scene* scene_create(void);
void scene_destroy(Scene* scene);

void scene_clear(Scene* scene);

// Push commands
void scene_push_ui_node(Scene* scene, UiNode node);
void scene_push_render_batch(Scene* scene, RenderBatch batch);

// Accessors
void scene_set_camera(Scene* scene, SceneCamera camera);
SceneCamera scene_get_camera(const Scene* scene);

void scene_set_frame_number(Scene* scene, uint64_t frame_number);
uint64_t scene_get_frame_number(const Scene* scene);

// Data Access for Backend
// Returns pointer to internal linear array and sets out_count.
const UiNode* scene_get_ui_nodes(const Scene* scene, size_t* out_count);
const RenderBatch* scene_get_render_batches(const Scene* scene, size_t* out_count);

// --- High-Level Drawing API (Legacy/Helpers) ---

void scene_push_rect_sdf(Scene* scene, Vec3 pos, Vec2 size, Vec4 color, float radius, float border, Vec4 clip_rect);
void scene_push_circle_sdf(Scene* scene, Vec3 center, float radius, Vec4 color, Vec4 clip_rect);
void scene_push_curve(Scene* scene, Vec3 start, Vec3 end, float thickness, Vec4 color, Vec4 clip_rect);

// New Basic Primitives
void scene_push_quad(Scene* scene, Vec3 pos, Vec2 size, Vec4 color, Vec4 clip_rect);
void scene_push_quad_textured(Scene* scene, Vec3 pos, Vec2 size, Vec4 color, Vec4 uv_rect, Vec4 clip_rect);
void scene_push_quad_9slice(Scene* scene, Vec3 pos, Vec2 size, Vec4 color, Vec4 uv_rect, Vec2 texture_size, Vec4 borders, Vec4 clip_rect);

#endif // RENDER_PACKET_H
