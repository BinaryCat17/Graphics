#ifndef UI_CORE_H
#define UI_CORE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "foundation/string/string_id.h"
#include "foundation/math/coordinate_systems.h"

// Forward Declarations
typedef struct MetaStruct MetaStruct;
typedef struct MetaField MetaField;
typedef struct SceneNodeSpec SceneNodeSpec; // From ui_assets.h

// --- CONSTANTS & FLAGS ---

typedef enum UiLayoutStrategy {
    UI_LAYOUT_FLEX_COLUMN, // Vertical stack
    UI_LAYOUT_FLEX_ROW,    // Horizontal stack
    UI_LAYOUT_CANVAS,      // Absolute positioning (Floating nodes)
    UI_LAYOUT_SPLIT_H,     // 2-child horizontal split
    UI_LAYOUT_SPLIT_V      // 2-child vertical split
} UiLayoutStrategy; // REFLECT

typedef enum UiFlags {
    UI_FLAG_NONE        = 0,
    UI_FLAG_CLICKABLE   = 1 << 0,
    UI_FLAG_DRAGGABLE   = 1 << 1, // Updates X/Y bindings on drag
    UI_FLAG_SCROLLABLE  = 1 << 2,
    UI_FLAG_FOCUSABLE   = 1 << 3,
    UI_FLAG_HIDDEN      = 1 << 4,
    UI_FLAG_CLIPPED     = 1 << 5, // Masks children outside bounds
    UI_FLAG_EDITABLE    = 1 << 6  // Supports text input
} UiFlags; // REFLECT

// "Kind" helps the renderer choose a default visual style, 
// but functional behavior is driven by flags.
typedef enum UiKind {
    UI_KIND_CONTAINER, // Generic Rect
    UI_KIND_TEXT,      // Renders text
    UI_KIND_VIEWPORT   // Delegates rendering to a provider
} UiKind; // REFLECT

typedef enum UiLayer {
    UI_LAYER_NORMAL = 0,
    UI_LAYER_OVERLAY  // Renders last, ignores parent clipping (popups)
} UiLayer; // REFLECT

typedef enum UiRenderMode {
    UI_RENDER_MODE_DEFAULT = 0, // Inferred (current behavior)
    UI_RENDER_MODE_BOX,         // SDF Rounded Box
    UI_RENDER_MODE_TEXT,        // Text only (no background)
    UI_RENDER_MODE_IMAGE,       // Textured Quad / 9-Slice
    UI_RENDER_MODE_BEZIER       // Explicit Bezier
} UiRenderMode; // REFLECT

typedef struct SceneNode SceneNode;
typedef struct Scene Scene;
typedef struct MemoryArena MemoryArena;

// Callback for Viewport Rendering
// instance_data: The data pointer bound to the UI Element (or its parent context)
typedef void (*SceneObjectProvider)(void* instance_data, Rect screen_rect, float z_depth, Scene* scene, MemoryArena* frame_arena);

// Register a provider globally (e.g., "GraphNetwork" -> math_graph_view_provider)
void scene_register_provider(const char* name, SceneObjectProvider callback);

// --- SCENE TREE (The Living Tree) ---
// Created from a SceneAsset + Data Context.
// Managed by a SceneTree container.
#include "foundation/math/coordinate_systems.h"

typedef struct SceneAsset SceneAsset;
typedef struct SceneTree SceneTree;

// API for Instance
// Create an instance to hold runtime state for a UI tree
// Requires an Asset (templates) and a memory size for the frame arena
void ui_system_init(void);
void ui_system_shutdown(void);
SceneTree* scene_tree_create(SceneAsset* assets, size_t size);
void scene_tree_destroy(SceneTree* tree);
SceneNode* scene_tree_get_root(const SceneTree* tree);
void scene_tree_set_root(SceneTree* tree, SceneNode* root);

// Allocates element from instance arena.
SceneNode* scene_node_create(SceneTree* tree, const SceneNodeSpec* spec, void* data, const MetaStruct* meta);

// Rebuilds children (Static + Dynamic) for an existing element.
void scene_node_rebuild_children(SceneNode* element, SceneTree* tree);

// Manually add a child to an element (for dynamic editors)
void scene_node_add_child(SceneNode* parent, SceneNode* child);

// Clear all children (destroys them)
void scene_node_clear_children(SceneNode* parent, SceneTree* tree);

// Core Loop
void scene_node_update(SceneNode* element, float dt); // Syncs data

// --- Accessors ---
StringId scene_node_get_id(const SceneNode* element);
SceneNode* scene_node_find_by_id(SceneNode* root, const char* id);
void* scene_node_get_data(const SceneNode* element);
const MetaStruct* scene_node_get_meta(const SceneNode* element);
SceneNode* scene_node_get_parent(const SceneNode* element);
Rect scene_node_get_screen_rect(const SceneNode* element);

#endif // UI_CORE_H
