#include "domains/cad_model/scene_service.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "domains/cad_model/cad_scene_yaml.h"
#include "engine/ui/ui_config.h" 

// Removing AppServices and ServiceManager dependencies

Model* scene_load(Scene* scene, const char* path, Assets* assets) {
    if (!scene || !path || !assets) return NULL;

    SceneError scene_err = {0};
    if (!parse_scene_yaml(path, scene, &scene_err)) {
        fprintf(stderr, "Failed to load scene %s:%d:%d %s\n", path, scene_err.line, scene_err.column,
                scene_err.message);
        return NULL;
    }

    // Load Model from UI Config (which defines bindings)
    return ui_config_load_model(&assets->ui_doc);
}

void scene_unload(Scene* scene) {
    if (scene) {
        scene_dispose(scene);
    }
}
