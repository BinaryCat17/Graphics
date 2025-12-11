#pragma once

#include "scene/cad_scene.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Parse a YAML scene file into a structured representation. */
int parse_scene_yaml(const char *path, Scene *out, SceneError *err);

#ifdef __cplusplus
}
#endif

