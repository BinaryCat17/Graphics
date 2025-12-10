#ifndef CORE_CONTEXT_H
#define CORE_CONTEXT_H

#include "core/Graphics.h"
#include "assets/assets.h"
#include "cad/cad_scene.h"
#include "config/module_yaml_loader.h"
#include "state/state_manager.h"
#include "ui/ui_json.h"

// Owns application-level state shared between systems.
typedef struct CoreContext {
    StateManager state_manager;
    ModuleSchema ui_schema;
    ModuleSchema global_schema;

    Scene scene;
    Assets assets;
    Model* model;
} CoreContext;

#endif // CORE_CONTEXT_H
