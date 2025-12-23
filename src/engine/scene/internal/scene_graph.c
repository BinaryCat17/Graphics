#include "../scene.h"
#include "scene_tree_internal.h"
#include "foundation/memory/arena.h"
#include "foundation/memory/pool.h"
#include "foundation/math/coordinate_systems.h"
#include "foundation/meta/reflection.h"

#include <stdlib.h>
#include <string.h>

// --- Scene Asset ---

SceneAsset* scene_asset_create(size_t arena_size) {
    SceneAsset* asset = (SceneAsset*)calloc(1, sizeof(SceneAsset));
    if (!asset) return NULL;
    
    if (!arena_init(&asset->arena, arena_size)) {
        free(asset);
        return NULL;
    }
    
    return asset;
}

void scene_asset_destroy(SceneAsset* asset) {
    if (!asset) return;
    arena_destroy(&asset->arena);
    free(asset);
}

SceneNodeSpec* scene_asset_push_node(SceneAsset* asset) {
    if (!asset) return NULL;
    return (SceneNodeSpec*)arena_alloc_zero(&asset->arena, sizeof(SceneNodeSpec));
}

SceneNodeSpec* scene_asset_get_template(SceneAsset* asset, const char* name) {
    if (!asset || !name) return NULL;
    SceneTemplate* t = asset->templates;
    while (t) {
        if (t->name && strcmp(t->name, name) == 0) {
            return t->spec;
        }
        t = t->next;
    }
    return NULL;
}

SceneNodeSpec* scene_asset_get_root(const SceneAsset* asset) {
    return asset ? asset->root : NULL;
}

// --- Scene Tree ---

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

SceneTree* scene_tree_create(SceneAsset* assets, size_t arena_size) {
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

void scene_tree_destroy(SceneTree* tree) {
    if (!tree) return;
    if (tree->root) destroy_recursive(tree, tree->root);
    pool_destroy(tree->node_pool);
    arena_destroy(&tree->arena);
    free(tree);
}

SceneNode* scene_tree_get_root(const SceneTree* tree) {
    return tree ? tree->root : NULL;
}

void scene_tree_set_root(SceneTree* tree, SceneNode* root) {
    if (tree) tree->root = root;
}

// --- Node Management ---

SceneNode* scene_node_create(SceneTree* tree, const SceneNodeSpec* spec, void* data, const MetaStruct* meta) {
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
        SceneNode* child = scene_node_create(tree, spec->children[i], data, meta);
        if (child) {
            scene_node_add_child(node, child);
        }
    }
    
    return node;
}

void scene_node_add_child(SceneNode* parent, SceneNode* child) {
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

void scene_node_clear_children(SceneNode* parent, SceneTree* tree) {
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

// --- Transform System ---

void scene_node_update_transforms(SceneNode* node, const Mat4* parent_world) {
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
        scene_node_update_transforms(child, &node->world_matrix);
    }
}

// --- Accessors ---

StringId scene_node_get_id(const SceneNode* node) {
    return (node && node->spec) ? node->spec->id : 0;
}

SceneNode* scene_node_find_by_id(SceneNode* root, const char* id) {
    if (!root || !id) return NULL;
    StringId target = str_id(id);
    if (scene_node_get_id(root) == target) return root;
    
    for (SceneNode* child = root->first_child; child; child = child->next_sibling) {
        SceneNode* found = scene_node_find_by_id(child, id);
        if (found) return found;
    }
    return NULL;
}

void* scene_node_get_data(const SceneNode* node) {
    return node ? node->data_ptr : NULL;
}

SceneNode* scene_node_get_parent(const SceneNode* node) {
    return node ? node->parent : NULL;
}

const MetaStruct* scene_node_get_meta(const SceneNode* node) {
    return node ? node->meta : NULL;
}
