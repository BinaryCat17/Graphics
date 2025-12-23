#include "../scene.h"
#include "scene_graph.h"
#include "foundation/memory/arena.h"
#include "foundation/memory/pool.h"
#include "foundation/math/coordinate_systems.h"
#include "foundation/meta/reflection.h"

#include <stdlib.h>
#include <string.h>

// --- Scene Tree Internal ---

static void destroy_recursive(SceneTree* tree, SceneNode* node) {
    if (!node) return;
    
    SceneNode* child = node->first_child;
    while (child) {
        SceneNode* next = child->next_sibling;
        destroy_recursive(tree, child);
        child = next;
    }
    
    pool_free(tree->node_pool, node);
}

SceneTree* scene_internal_tree_create(SceneAsset* assets, size_t arena_size) {
    SceneTree* tree = (SceneTree*)calloc(1, sizeof(SceneTree));
    if (!tree) return NULL;
    
    if (!arena_init(&tree->arena, arena_size)) {
        free(tree);
        return NULL;
    }
    
    tree->assets = assets;
    tree->node_pool = pool_create(sizeof(SceneNode), 256);
    return tree;
}

void scene_internal_tree_destroy(SceneTree* tree) {
    if (!tree) return;
    if (tree->root) destroy_recursive(tree, tree->root);
    pool_destroy(tree->node_pool);
    arena_destroy(&tree->arena);
    free(tree);
}

// --- Node Management Internal ---

SceneNode* scene_internal_node_create(SceneTree* tree, const SceneNodeSpec* spec, void* data, const MetaStruct* meta) {
    if (!tree || !spec) return NULL;

    SceneNode* node = (SceneNode*)pool_alloc(tree->node_pool);
    node->spec = spec;
    node->data_ptr = data;
    node->meta = meta;
    node->flags = spec->flags;
    
    // Initial transform update
    node->local_matrix = mat4_identity();
    node->world_matrix = mat4_identity();

    // Create static children defined in spec
    for (size_t i = 0; i < spec->child_count; ++i) {
        SceneNode* child = scene_internal_node_create(tree, spec->children[i], data, meta);
        if (child) {
            scene_internal_node_add_child(node, child);
        }
    }
    
    return node;
}

void scene_internal_node_add_child(SceneNode* parent, SceneNode* child) {
    if (!parent || !child) return;
    
    child->parent = parent;
    child->next_sibling = NULL;
    child->prev_sibling = parent->last_child;
    
    if (parent->last_child) {
        parent->last_child->next_sibling = child;
    } else {
        parent->first_child = child;
    }
    parent->last_child = child;
    parent->child_count++;
    
    child->flags |= SCENE_FLAG_DIRTY;
}

void scene_internal_node_clear_children(SceneNode* parent, SceneTree* tree) {
    if (!parent || !tree) return;
    
    SceneNode* curr = parent->first_child;
    while (curr) {
        SceneNode* next = curr->next_sibling;
        destroy_recursive(tree, curr);
        curr = next;
    }
    parent->first_child = NULL;
    parent->last_child = NULL;
    parent->child_count = 0;
}

// --- Transform System Internal ---

void scene_internal_node_update_transforms(SceneNode* node, const Mat4* parent_world) {
    if (!node) return;

    // 1. Build Local Matrix (from spec transform)
    const SceneTransformSpec* trans = &node->spec->transform;
    
    Vec3 scale = { 
        trans->local_scale.x == 0 ? 1.0f : trans->local_scale.x, 
        trans->local_scale.y == 0 ? 1.0f : trans->local_scale.y, 
        trans->local_scale.z == 0 ? 1.0f : trans->local_scale.z 
    };
    Mat4 mat_s = mat4_scale(scale);
    
    EulerAngles euler = { trans->local_rotation.x, trans->local_rotation.y, trans->local_rotation.z };
    Mat4 mat_r = mat4_rotation_euler(euler);
    
    Mat4 mat_t = mat4_translation(trans->local_position);
    
    Mat4 mat_rs = mat4_multiply(&mat_r, &mat_s);
    node->local_matrix = mat4_multiply(&mat_t, &mat_rs);

    // 2. World Matrix
    if (parent_world) {
        node->world_matrix = mat4_multiply(parent_world, &node->local_matrix);
    } else {
        node->world_matrix = node->local_matrix;
    }

    // 3. Recurse
    for (SceneNode* child = node->first_child; child; child = child->next_sibling) {
        scene_internal_node_update_transforms(child, &node->world_matrix);
    }
}

SceneNode* scene_internal_node_find_by_id(SceneNode* root, const char* id) {
    if (!root || !id) return NULL;
    StringId target = str_id(id);
    if ((root->spec ? root->spec->id : 0) == target) return root;
    
    for (SceneNode* child = root->first_child; child; child = child->next_sibling) {
        SceneNode* found = scene_internal_node_find_by_id(child, id);
        if (found) return found;
    }
    return NULL;
}
