#include "scene.h"
#include "render_packet.h"
#include "internal/render_packet_internal.h"
#include "internal/scene_tree_internal.h"
#include "internal/scene_graph.h"
#include "foundation/memory/arena.h"
#include "foundation/string/string_id.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#define SCENE_ARENA_SIZE (4 * 1024 * 1024)
#define MAX_UI_NODES 16384
#define MAX_BATCHES 4096

// --- Scene Struct Definition ---

struct Scene {
    MemoryArena arena;
    SceneCamera camera;
    uint64_t frame_number;
    
    UiNode* ui_nodes;
    size_t ui_count;
    size_t ui_capacity;
    
    RenderBatch* batches;
    size_t batch_count;
    size_t batch_capacity;
};

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
        // Pre-allocate arrays
        scene_clear(scene);
    }
    return scene;
}

void scene_destroy(Scene* scene) {
    if (!scene) return;
    arena_destroy(&scene->arena);
    free(scene);
}

void scene_clear(Scene* scene) {
    if (!scene) return;
    arena_reset(&scene->arena);
    
    scene->ui_nodes = (UiNode*)arena_alloc(&scene->arena, sizeof(UiNode) * MAX_UI_NODES);
    scene->ui_capacity = MAX_UI_NODES;
    scene->ui_count = 0;
    
    scene->batches = (RenderBatch*)arena_alloc(&scene->arena, sizeof(RenderBatch) * MAX_BATCHES);
    scene->batch_capacity = MAX_BATCHES;
    scene->batch_count = 0;
}

void scene_push_ui_node(Scene* scene, UiNode node) {
    if (!scene || scene->ui_count >= scene->ui_capacity) return;
    scene->ui_nodes[scene->ui_count++] = node;
}

void scene_push_render_batch(Scene* scene, RenderBatch batch) {
    if (!scene || scene->batch_count >= scene->batch_capacity) return;
    scene->batches[scene->batch_count++] = batch;
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

const UiNode* scene_get_ui_nodes(const Scene* scene, size_t* out_count) {
    if (!scene) { if (out_count) *out_count = 0; return NULL; }
    if (out_count) *out_count = scene->ui_count;
    return scene->ui_nodes;
}

const RenderBatch* scene_get_render_batches(const Scene* scene, size_t* out_count) {
    if (!scene) { if (out_count) *out_count = 0; return NULL; }
    if (out_count) *out_count = scene->batch_count;
    return scene->batches;
}

// --- High-Level Drawing API (Adapted to UiNode) ---

void scene_push_rect_sdf(Scene* scene, Vec3 pos, Vec2 size, Vec4 color, float radius, float border, Vec4 clip_rect) {
    UiNode node = {0};
    node.rect = (Rect){pos.x, pos.y, size.x, size.y};
    node.z_index = pos.z;
    node.color = color;
    node.clip_rect = (Rect){clip_rect.x, clip_rect.y, clip_rect.z, clip_rect.w};
    node.primitive_type = SCENE_MODE_SDF_BOX; // Using enum from render_packet.h or similar
    node.corner_radius = radius;
    node.border_width = border;
    node.flags = UI_RENDER_FLAG_HAS_BG | UI_RENDER_FLAG_ROUNDED;
    scene_push_ui_node(scene, node);
}

void scene_push_circle_sdf(Scene* scene, Vec3 center, float radius, Vec4 color, Vec4 clip_rect) {
    // Circle is just a very rounded square
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

    UiNode node = {0};
    node.rect = (Rect){min_x + width * 0.5f - width*0.5f, min_y + height * 0.5f - height*0.5f, width, height}; // Rect is x,y,w,h (bottom-left)
    // Actually, previous implementation used center pos + scale.
    // Rect usually implies x,y (bottom-left or top-left). 
    // Let's assume RenderPacket expects x,y,w,h.
    // Previous: pos = center. scale = size.
    node.rect = (Rect){min_x, min_y, width, height};
    node.z_index = start.z;
    node.color = color;
    node.primitive_type = SCENE_PRIM_CURVE; // 1
    node.flags = UI_RENDER_FLAG_NONE;
    
    // Packing params into custom Vec4
    // params.x = u1, params.y = v1, params.z = u2, params.w = v2
    node.params = (Vec4){u1, v1, u2, v2};
    // We also need thickness.
    // In previous code: style_params.z = thickness/height.
    // We might need another param slot if we want to be clean.
    // But for now let's squeeze it or assume shader handles it.
    // Wait, UiNode has uv_rect. We can use that for u1,v1,u2,v2?
    node.uv_rect = (Vec4){u1, v1, u2, v2};
    node.border_width = thickness; // Use border_width for thickness
    
    node.clip_rect = (Rect){clip_rect.x, clip_rect.y, clip_rect.z, clip_rect.w};
    
    scene_push_ui_node(scene, node);
}

void scene_push_quad(Scene* scene, Vec3 pos, Vec2 size, Vec4 color, Vec4 clip_rect) {
    UiNode node = {0};
    node.rect = (Rect){pos.x, pos.y, size.x, size.y};
    node.z_index = pos.z;
    node.color = color;
    node.clip_rect = (Rect){clip_rect.x, clip_rect.y, clip_rect.z, clip_rect.w};
    node.primitive_type = SCENE_MODE_SOLID;
    node.flags = UI_RENDER_FLAG_HAS_BG;
    scene_push_ui_node(scene, node);
}

void scene_push_quad_textured(Scene* scene, Vec3 pos, Vec2 size, Vec4 color, Vec4 uv_rect, Vec4 clip_rect) {
    UiNode node = {0};
    node.rect = (Rect){pos.x, pos.y, size.x, size.y};
    node.z_index = pos.z;
    node.color = color;
    node.clip_rect = (Rect){clip_rect.x, clip_rect.y, clip_rect.z, clip_rect.w};
    node.uv_rect = uv_rect;
    node.primitive_type = SCENE_MODE_TEXTURED;
    node.flags = UI_RENDER_FLAG_HAS_BG | UI_RENDER_FLAG_TEXTURED;
    scene_push_ui_node(scene, node);
}

void scene_push_quad_9slice(Scene* scene, Vec3 pos, Vec2 size, Vec4 color, Vec4 uv_rect, Vec2 texture_size, Vec4 borders, Vec4 clip_rect) {
    UiNode node = {0};
    node.rect = (Rect){pos.x, pos.y, size.x, size.y};
    node.z_index = pos.z;
    node.color = color;
    node.clip_rect = (Rect){clip_rect.x, clip_rect.y, clip_rect.z, clip_rect.w};
    node.uv_rect = uv_rect;
    node.texture_size = texture_size;
    node.slice_borders = borders;
    node.primitive_type = SCENE_MODE_9_SLICE;
    node.flags = UI_RENDER_FLAG_HAS_BG | UI_RENDER_FLAG_TEXTURED | UI_RENDER_FLAG_9_SLICE;
    scene_push_ui_node(scene, node);
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
