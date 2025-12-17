#ifndef UI_SCENE_BRIDGE_H
#define UI_SCENE_BRIDGE_H

#include "engine/ui/ui_def.h"
#include "engine/graphics/scene.h"
#include "engine/assets/assets.h"

// Traverses the UiView tree and populates the Scene with renderable objects.
void ui_build_scene(const UiView* root, Scene* scene, const Assets* assets);

#endif // UI_SCENE_BRIDGE_H
