#ifndef SCENE_GRAPH_H
#define SCENE_GRAPH_H

#include "scene_tree_internal.h"

// Internal Scene Graph Functions (called by scene_system.c)

SceneTree* scene_internal_tree_create(SceneAsset* assets, size_t arena_size);
void scene_internal_tree_destroy(SceneTree* tree);

SceneNode* scene_internal_node_create(SceneTree* tree, const SceneNodeSpec* spec, void* data, const struct MetaStruct* meta);
void scene_internal_node_add_child(SceneNode* parent, SceneNode* child);
void scene_internal_node_clear_children(SceneNode* parent, SceneTree* tree);
void scene_internal_node_update_transforms(SceneNode* node, const Mat4* parent_world);
SceneNode* scene_internal_node_find_by_id(SceneNode* root, const char* id);

#endif // SCENE_GRAPH_H
