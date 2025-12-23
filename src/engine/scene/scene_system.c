#include "scene.h"
#include "render_packet.h"
#include "internal/render_packet_internal.h"
#include "internal/scene_tree_internal.h"
#include "internal/scene_graph.h"
#include "internal/scene_loader.h"
#include "foundation/memory/arena.h"
#include "foundation/string/string_id.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#define SCENE_ARENA_SIZE (4 * 1024 * 1024)

// --- System Lifecycle ---

void scene_system_init(void) {
    // Core system init
}

void scene_system_shutdown(void) {
    // Core system shutdown
}

// --- Scene Lifecycle ---

Scene* scene_create(void) {
    Scene* scene = (Scene*)malloc(sizeof(Scene));
    if (scene) {
        memset(scene, 0, sizeof(Scene));
        if (!arena_init(&scene->arena, SCENE_ARENA_SIZE)) {
            free(scene);
            return NULL;
        }
        scene->objects = (SceneObject*)scene->arena.base;
    }
    return scene;
}

void scene_destroy(Scene* scene) {
    if (!scene) return;
    arena_destroy(&scene->arena);
    free(scene);
}

void scene_add_object(Scene* scene, SceneObject obj) {
    if (!scene) return;
    SceneObject* new_slot = (SceneObject*)arena_alloc(&scene->arena, sizeof(SceneObject));
    if (new_slot) {
        *new_slot = obj;
        scene->object_count++;
    }
}

void scene_clear(Scene* scene) {
    if (!scene) return;
    arena_reset(&scene->arena);
    scene->object_count = 0;
    scene->objects = (SceneObject*)scene->arena.base;
}

void scene_set_camera(Scene* scene, SceneCamera camera) {
    if (scene) scene->camera = camera;
}

SceneCamera scene_get_camera(const Scene* scene) {
    if (scene) return scene->camera;
    SceneCamera empty = {0};
    return empty;
}

void scene_set_frame_number(Scene* scene, uint64_t frame_number) {
    if (scene) scene->frame_number = frame_number;
}

uint64_t scene_get_frame_number(const Scene* scene) {
    return scene ? scene->frame_number : 0;
}

const SceneObject* scene_get_all_objects(const Scene* scene, size_t* out_count) {
    if (!scene) {
        if (out_count) *out_count = 0;
        return NULL;
    }
    if (out_count) *out_count = scene->object_count;
    return scene->objects;
}

// --- High-Level Drawing API ---

void scene_push_rect_sdf(Scene* scene, Vec3 pos, Vec2 size, Vec4 color, float radius, float border, Vec4 clip_rect) {
    SceneObject obj = {0};
    obj.prim_type = SCENE_PRIM_QUAD;
    obj.position = pos;
    obj.scale = (Vec3){size.x, size.y, 1.0f};
    obj.color = color;
    obj.ui.clip_rect = clip_rect;
    obj.uv_rect = (Vec4){0.0f, 0.0f, 1.0f, 1.0f}; 
    obj.ui.style_params.x = (float)SCENE_MODE_SDF_BOX;
    obj.ui.style_params.y = radius;
    obj.ui.style_params.z = border;
    scene_add_object(scene, obj);
}

void scene_push_circle_sdf(Scene* scene, Vec3 center, float radius, Vec4 color, Vec4 clip_rect) {
    scene_push_rect_sdf(scene, 
        (Vec3){center.x - radius, center.y - radius, center.z}, 
        (Vec2){radius * 2.0f, radius * 2.0f}, 
        color, radius, 1.0f, clip_rect
    );
}

void scene_push_curve(Scene* scene, Vec3 start, Vec3 end, float thickness, Vec4 color, Vec4 clip_rect) {
    float min_x = start.x < end.x ? start.x : end.x;
    float max_x = start.x > end.x ? start.x : end.x;
    float min_y = start.y < end.y ? start.y : end.y;
    float max_y = start.y > end.y ? start.y : end.y;
    float padding = 50.0f;
    min_x -= padding; min_y -= padding;
    max_x += padding; max_y += padding;
    float width = max_x - min_x;
    float height = max_y - min_y;
    if (width < 1.0f) width = 1.0f;
    if (height < 1.0f) height = 1.0f;

    float u1 = (start.x - min_x) / width;
    float v1 = (start.y - min_y) / height;
    float u2 = (end.x - min_x) / width;
    float v2 = (end.y - min_y) / height;

    SceneObject wire = {0};
    wire.prim_type = SCENE_PRIM_CURVE;
    wire.position = (Vec3){min_x + width * 0.5f, min_y + height * 0.5f, start.z};
    wire.scale = (Vec3){width, height, 1.0f};
    wire.color = color;
    wire.ui.clip_rect = clip_rect;
    wire.uv_rect = (Vec4){0.0f, 0.0f, 1.0f, 1.0f};
    wire.ui.style_params.y = 1.0f; 
    wire.ui.extra_params = (Vec4){u1, v1, u2, v2};
    wire.ui.style_params.z = thickness / height; 
    wire.ui.style_params.w = width / height; 
    scene_add_object(scene, wire);
}

void scene_push_quad(Scene* scene, Vec3 pos, Vec2 size, Vec4 color, Vec4 clip_rect) {
    SceneObject obj = {0};
    obj.prim_type = SCENE_PRIM_QUAD;
    obj.position = pos;
    obj.scale = (Vec3){size.x, size.y, 1.0f};
    obj.color = color;
    obj.ui.clip_rect = clip_rect;
    obj.ui.style_params.x = (float)SCENE_MODE_SOLID;
    obj.uv_rect = (Vec4){0.0f, 0.0f, 1.0f, 1.0f}; 
    scene_add_object(scene, obj);
}

void scene_push_quad_textured(Scene* scene, Vec3 pos, Vec2 size, Vec4 color, Vec4 uv_rect, Vec4 clip_rect) {
    SceneObject obj = {0};
    obj.prim_type = SCENE_PRIM_QUAD;
    obj.position = pos;
    obj.scale = (Vec3){size.x, size.y, 1.0f};
    obj.color = color;
    obj.ui.clip_rect = clip_rect;
    obj.uv_rect = uv_rect;
    obj.ui.style_params.x = (float)SCENE_MODE_TEXTURED;
    scene_add_object(scene, obj);
}

void scene_push_quad_9slice(Scene* scene, Vec3 pos, Vec2 size, Vec4 color, Vec4 uv_rect, Vec2 texture_size, Vec4 borders, Vec4 clip_rect) {
    SceneObject obj = {0};
    obj.prim_type = SCENE_PRIM_QUAD;
    obj.position = pos;
    obj.scale = (Vec3){size.x, size.y, 1.0f};
    obj.color = color;
    obj.ui.clip_rect = clip_rect;
    obj.uv_rect = uv_rect;
    obj.ui.style_params.x = (float)SCENE_MODE_9_SLICE;
    obj.ui.style_params.z = texture_size.x; 
    obj.ui.style_params.w = texture_size.y; 
    obj.ui.extra_params = borders;
    scene_add_object(scene, obj);
}

// --- Scene Asset Implementation ---

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
        if (t->name && strcmp(t->name, name) == 0) return t->spec;
        t = t->next;
    }
    return NULL;
}

SceneNodeSpec* scene_asset_get_root(const SceneAsset* asset) {
    return asset ? asset->root : NULL;
}

// --- Scene Tree Wrappers ---

SceneTree* scene_tree_create(SceneAsset* assets, size_t arena_size) {
    return scene_internal_tree_create(assets, arena_size);
}

void scene_tree_destroy(SceneTree* tree) {
    scene_internal_tree_destroy(tree);
}

SceneNode* scene_tree_get_root(const SceneTree* tree) {
    return tree ? tree->root : NULL;
}

void scene_tree_set_root(SceneTree* tree, SceneNode* root) {
    if (tree) tree->root = root;
}

// --- Scene Node Wrappers ---

SceneNode* scene_node_create(SceneTree* tree, const SceneNodeSpec* spec, void* data, const struct MetaStruct* meta) {
    return scene_internal_node_create(tree, spec, data, meta);
}

void scene_node_add_child(SceneNode* parent, SceneNode* child) {
    scene_internal_node_add_child(parent, child);
}

void scene_node_remove_child(SceneNode* parent, SceneNode* child) {
    // Not implemented internally yet
    (void)parent; (void)child;
}

void scene_node_clear_children(SceneNode* parent, SceneTree* tree) {
    scene_internal_node_clear_children(parent, tree);
}

void scene_node_update_transforms(SceneNode* node, const Mat4* parent_world) {
    scene_internal_node_update_transforms(node, parent_world);
}

SceneNode* scene_node_find_by_id(SceneNode* root, const char* id) {
    return scene_internal_node_find_by_id(root, id);
}

// --- Accessors ---

StringId scene_node_get_id(const SceneNode* node) {
    return (node && node->spec) ? node->spec->id : 0;
}

void* scene_node_get_data(const SceneNode* node) {
    return node ? node->data_ptr : NULL;
}

SceneNode* scene_node_get_parent(const SceneNode* node) {
    return node ? node->parent : NULL;
}

const struct MetaStruct* scene_node_get_meta(const SceneNode* node) {
    return node ? node->meta : NULL;
}

// --- Scene Loader Wrappers ---

SceneAsset* scene_asset_load_from_file(const char* path) {
    return scene_internal_asset_load_from_file(path);
}
