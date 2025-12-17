#include "scene.h"
#include <stdlib.h>
#include <string.h>

void scene_init(Scene* scene) {
    if (!scene) return;
    memset(scene, 0, sizeof(Scene));
    // Default camera?
}

void scene_add_object(Scene* scene, SceneObject obj) {
    if (!scene) return;
    
    if (scene->object_count == scene->object_capacity) {
        size_t new_cap = scene->object_capacity == 0 ? 64 : scene->object_capacity * 2;
        SceneObject* new_list = realloc(scene->objects, new_cap * sizeof(SceneObject));
        if (!new_list) return; // OOM
        scene->objects = new_list;
        scene->object_capacity = new_cap;
    }
    
    scene->objects[scene->object_count++] = obj;
}

void scene_clear(Scene* scene) {
    if (!scene) return;
    scene->object_count = 0;
}
