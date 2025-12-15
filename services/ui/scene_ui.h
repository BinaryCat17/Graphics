#ifndef SCENE_UI_H
#define SCENE_UI_H

#include "scene/cad_scene.h"
#include "ui/ui_node.h"

/**
 * Attach scene-specific UI nodes to the parsed layout tree. The layout must
 * expose containers with identifiers used by the implementation (for example
 * `sceneHierarchy`, `materialsList`, `jointsList`, `analysisList`). The
 * function will append read-only rows describing the scene structure so that
 * widgets can be materialized later.
 */
void scene_ui_inject(UiNode* root, const Scene* scene);

/** Update model bindings with scene metadata for header labels. */
void scene_ui_bind_model(Model* model, const Scene* scene, const char* scene_path);

#endif // SCENE_UI_H
