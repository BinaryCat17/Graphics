#ifndef UI_RENDERER_H
#define UI_RENDERER_H

#include <stdint.h>
#include "foundation/math/coordinate_systems.h"
#include "ui_core.h"

// Forward Declarations
typedef struct SceneTree SceneTree;
typedef struct Scene Scene;
typedef struct Assets Assets;
typedef struct MemoryArena MemoryArena;
typedef struct RenderSystem RenderSystem;

// --- High-Level Pipeline API ---

// Layout & Render Pipeline

// Generates render commands into the Scene.
void scene_tree_render(SceneTree* instance, Scene* scene, const Assets* assets, MemoryArena* arena);

// Extract UI Nodes from Scene, convert to GPU buffers, and push RenderBatch.
// Should be called before render_system_update/draw.
void ui_renderer_extract(Scene* scene, struct RenderSystem* rs);

// --- Viewport Provider ---
// Callback for Viewport Rendering (dynamic 3D/custom content inside UI)
typedef void (*SceneObjectProvider)(void* instance_data, Rect screen_rect, float z_depth, Scene* scene, MemoryArena* frame_arena);

// Register a provider globally (e.g., "GraphNetwork" -> math_graph_view_provider)
void scene_register_provider(const char* name, SceneObjectProvider callback);

#endif // UI_RENDERER_H
