#ifndef SCENE_H
#define SCENE_H

#include "foundation/math/coordinate_systems.h"
#include "foundation/string/string_id.h"
#include "scene_asset.h" 
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// Forward Declarations
typedef struct SceneNode SceneNode;
typedef struct SceneTree SceneTree;
typedef struct MetaStruct MetaStruct;

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