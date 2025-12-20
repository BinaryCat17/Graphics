#ifndef UI_RENDERER_H
#define UI_RENDERER_H

// --- INTERNAL HEADER: Do not include in public API ---
// Use ui_core.h (ui_instance_render) instead.

#include "engine/ui/ui_core.h"
#include "engine/graphics/scene/scene.h"
#include "engine/assets/assets.h"

// Traverses the UiElement tree and populates the Scene with renderable objects.
void ui_renderer_build_scene(const UiElement* root, Scene* scene, const Assets* assets);

#endif // UI_RENDERER_H
