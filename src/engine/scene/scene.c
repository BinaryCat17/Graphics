#include "scene.h"
#include "internal/scene_internal.h"
#include "foundation/memory/arena.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#define SCENE_ARENA_SIZE (4 * 1024 * 1024) // 4 MB Capacity (~25k objects)

Scene* scene_create(void) {
    Scene* scene = (Scene*)malloc(sizeof(Scene));
    if (scene) {
        memset(scene, 0, sizeof(Scene));
        if (!arena_init(&scene->arena, SCENE_ARENA_SIZE)) {
            free(scene);
            return NULL;
        }
        // Objects array starts at the base of the arena
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
    
    // Allocate directly from the arena
    SceneObject* new_slot = (SceneObject*)arena_alloc(&scene->arena, sizeof(SceneObject));
    
    if (new_slot) {
        *new_slot = obj;
        scene->object_count++;
    } else {
        // Out of memory in the arena - silently fail or log error
        // For a production engine, we might want a logging macro here
    }
}

void scene_clear(Scene* scene) {
    if (!scene) return;
    arena_reset(&scene->arena);
    scene->object_count = 0;
    // Reset objects pointer to base (though it shouldn't have changed)
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
    
    // White pixel UV (assuming Font/WhitePixel is at 0,0 approx or handled by shader defaults for SDF)
    // Actually, for SDF box mode, UVs define the gradient space 0..1
    obj.uv_rect = (Vec4){0.0f, 0.0f, 1.0f, 1.0f}; 

    obj.ui.style_params.x = (float)SCENE_MODE_SDF_BOX;
    obj.ui.style_params.y = radius;
    obj.ui.style_params.z = border;
    
    scene_add_object(scene, obj);
}

void scene_push_circle_sdf(Scene* scene, Vec3 center, float radius, Vec4 color, Vec4 clip_rect) {
    // A circle is just a rounded box with radius = half size
    scene_push_rect_sdf(scene, 
        (Vec3){center.x - radius, center.y - radius, center.z}, 
        (Vec2){radius * 2.0f, radius * 2.0f}, 
        color, 
        radius, 
        1.0f, // Default border 1px? Or pass as arg?
        clip_rect
    );
}

void scene_push_curve(Scene* scene, Vec3 start, Vec3 end, float thickness, Vec4 color, Vec4 clip_rect) {
    // Bounding Box Logic
    float min_x = start.x < end.x ? start.x : end.x;
    float max_x = start.x > end.x ? start.x : end.x;
    float min_y = start.y < end.y ? start.y : end.y;
    float max_y = start.y > end.y ? start.y : end.y;
    
    float padding = 50.0f; // Padding for curve control points
    min_x -= padding; min_y -= padding;
    max_x += padding; max_y += padding;
    
    float width = max_x - min_x;
    float height = max_y - min_y;
    
    if (width < 1.0f) width = 1.0f;
    if (height < 1.0f) height = 1.0f;

    // Normalize Points to 0..1 relative to Quad
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

    wire.ui.style_params.y = 1.0f; // Curve Type
    wire.ui.extra_params = (Vec4){u1, v1, u2, v2};
    wire.ui.style_params.z = thickness / height; 
    wire.ui.style_params.w = width / height; // AR
    
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
    // UVs should point to a white pixel in the atlas for solid color to work with tint
    // We assume default 0,0,1,1 maps to full white or handled by shader fallback
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
    
    obj.ui.extra_params = borders; // top, right, bottom, left
    
    scene_add_object(scene, obj);
}
