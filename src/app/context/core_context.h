#ifndef APP_CONTEXT_CORE_CONTEXT_H
#define APP_CONTEXT_CORE_CONTEXT_H

#include "assets/assets.h"
#include "cad/cad_scene.h"
#include "config/module_yaml_loader.h"
#include "ui/ui_config.h"

// Owns application-level state shared between systems.
typedef struct CoreContext {
    ModuleSchema ui_schema;
    ModuleSchema global_schema;

    Scene scene;
    Assets assets;
    Model* model;
} CoreContext;

#endif // APP_CONTEXT_CORE_CONTEXT_H
