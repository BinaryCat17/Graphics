#include "scene.h"
#include "internal/scene_internal.h"
#include <stdlib.h>
#include <string.h>

Scene* scene_create(void) {
    Scene* scene = (Scene*)malloc(sizeof(Scene));
    if (scene) {
        memset(scene, 0, sizeof(Scene));
    }
    return scene;
}

void scene_destroy(Scene* scene) {
    if (!scene) return;
    if (scene->objects) {
        free(scene->objects);
    }
    free(scene);
}

void scene_add_object(Scene* scene, SceneObject obj) {
    if (!scene) return;
    
    if (scene->object_count == scene->object_capacity) {
        size_t new_cap = scene->object_capacity == 0 ? 64 : scene->object_capacity * 2;
        SceneObject* new_list = realloc(scene->objects, new_cap * sizeof(SceneObject));
        if (!new_list) return; // Allocation failed
        
        scene->objects = new_list;
        scene->object_capacity = new_cap;
    }
    
    scene->objects[scene->object_count++] = obj;
}

void scene_clear(Scene* scene) {
    if (!scene) return;
    scene->object_count = 0;
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
