#include "scene.h"
#include "internal/scene_internal.h"
#include "foundation/memory/arena.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

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
