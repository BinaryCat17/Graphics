#ifndef SCENE_H
#define SCENE_H

#include "foundation/math/coordinate_systems.h"
#include "foundation/string/string_id.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// Forward Declarations
typedef struct SceneNode SceneNode;
typedef struct SceneTree SceneTree;
typedef struct SceneAsset SceneAsset;
typedef struct SceneNodeSpec SceneNodeSpec;
typedef struct MetaStruct MetaStruct;

// --- CORE TYPES ---

typedef enum SceneFlags {
    SCENE_FLAG_NONE        = 0,
    SCENE_FLAG_HIDDEN      = 1 << 0,
    SCENE_FLAG_DIRTY       = 1 << 1, // Transform needs update
    SCENE_FLAG_CLIPPED     = 1 << 2,
    
    SCENE_FLAG_SYSTEM_BIT  = 1 << 8
} SceneFlags; // REFLECT

typedef enum SceneInteractionFlags {
    SCENE_INTERACTION_NONE       = 0,
    SCENE_INTERACTION_CLICKABLE  = 1 << 0,
    SCENE_INTERACTION_DRAGGABLE  = 1 << 1,
    SCENE_INTERACTION_FOCUSABLE  = 1 << 3,
    SCENE_INTERACTION_HOVERABLE  = 1 << 4
} SceneInteractionFlags; // REFLECT

typedef enum UiFlags {
    UI_FLAG_NONE       = 0,
    UI_FLAG_SCROLLABLE = 1 << 0,
    UI_FLAG_EDITABLE   = 1 << 1
} UiFlags; // REFLECT

typedef enum SceneNodeKind {
    SCENE_NODE_KIND_CONTAINER,
    SCENE_NODE_KIND_TEXT,
    SCENE_NODE_KIND_VIEWPORT
} SceneNodeKind; // REFLECT

typedef enum SceneLayoutStrategy {
    SCENE_LAYOUT_FLEX_COLUMN,
    SCENE_LAYOUT_FLEX_ROW,
    SCENE_LAYOUT_CANVAS,
    SCENE_LAYOUT_SPLIT_H,
    SCENE_LAYOUT_SPLIT_V
} SceneLayoutStrategy; // REFLECT

typedef enum SceneLayer {
    SCENE_LAYER_NORMAL = 0,
    SCENE_LAYER_OVERLAY
} SceneLayer; // REFLECT

typedef enum SceneRenderMode {
    SCENE_RENDER_MODE_DEFAULT = 0,
    SCENE_RENDER_MODE_BOX,
    SCENE_RENDER_MODE_TEXT,
    SCENE_RENDER_MODE_IMAGE,
    SCENE_RENDER_MODE_BEZIER
} SceneRenderMode; // REFLECT

// --- SCENE ASSET (The DNA) ---

SceneAsset* scene_asset_create(size_t arena_size);
void scene_asset_destroy(SceneAsset* asset);
SceneAsset* scene_asset_load_from_file(const char* path);

SceneNodeSpec* scene_asset_push_node(SceneAsset* asset);
SceneNodeSpec* scene_asset_get_template(SceneAsset* asset, const char* name);
SceneNodeSpec* scene_asset_get_root(const SceneAsset* asset);

// --- SCENE CORE API ---

// Lifecycle
void scene_system_init(void);
void scene_system_shutdown(void);

SceneTree* scene_tree_create(SceneAsset* assets, size_t arena_size);
void scene_tree_destroy(SceneTree* tree);

SceneNode* scene_tree_get_root(const SceneTree* tree);
void scene_tree_set_root(SceneTree* tree, SceneNode* root);

// Node Management
SceneNode* scene_node_create(SceneTree* tree, const SceneNodeSpec* spec, void* data, const MetaStruct* meta);
void scene_node_add_child(SceneNode* parent, SceneNode* child);
void scene_node_remove_child(SceneNode* parent, SceneNode* child);
void scene_node_clear_children(SceneNode* parent, SceneTree* tree);

// Transform & Update
void scene_node_update_transforms(SceneNode* node, const Mat4* parent_world);

// Accessors
StringId scene_node_get_id(const SceneNode* node);
SceneNode* scene_node_find_by_id(SceneNode* root, const char* id);
void* scene_node_get_data(const SceneNode* node);
SceneNode* scene_node_get_parent(const SceneNode* node);
const struct MetaStruct* scene_node_get_meta(const SceneNode* node);

#endif // SCENE_H
