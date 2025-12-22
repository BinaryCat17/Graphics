#ifndef UI_ASSETS_H
#define UI_ASSETS_H

#include "engine/scene/scene.h"

// UI-specific Asset API (if any)
// The core SceneAsset functions are now in engine/scene/scene.h

// --- Parser API ---
SceneAsset* scene_asset_load_from_file(const char* path);

#endif // UI_ASSETS_H
