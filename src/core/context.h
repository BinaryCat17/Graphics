#ifndef CORE_CONTEXT_H
#define CORE_CONTEXT_H

#include "Graphics.h"
#include "assets/assets.h"
#include "cad_scene.h"
#include "module_yaml_loader.h"
#include "state_manager.h"

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
