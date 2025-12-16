#ifndef SCENE_SYSTEM_H
#define SCENE_SYSTEM_H

#include <stdbool.h>
#include "domains/cad_model/cad_scene.h"
#include "engine/assets/assets_service.h"

// Returns success/failure
bool scene_load(Scene* scene, const char* path, Assets* assets);
void scene_unload(Scene* scene);

#endif // SCENE_SYSTEM_H
