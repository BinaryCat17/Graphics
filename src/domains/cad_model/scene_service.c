#include "domains/cad_model/scene_service.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "domains/cad_model/cad_scene_yaml.h"

// Removing AppServices and ServiceManager dependencies

bool scene_load(Scene* scene, const char* path, Assets* assets) {
    if (!scene || !path || !assets) return false;

    SceneError scene_err = {0};
    if (!parse_scene_yaml(path, scene, &scene_err)) {
        fprintf(stderr, "Failed to load scene %s:%d:%d %s\n", path, scene_err.line, scene_err.column,
                scene_err.message);
        return false;
    }

    return true;
}

void scene_unload(Scene* scene) {
    if (scene) {
        scene_dispose(scene);
    }
}
