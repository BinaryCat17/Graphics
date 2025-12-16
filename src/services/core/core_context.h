#ifndef APP_CONTEXT_CORE_CONTEXT_H
#define APP_CONTEXT_CORE_CONTEXT_H

#include "services/assets/assets_service.h"
#include "services/scene/cad_scene.h"
#include "core/config/module_yaml_loader.h"
#include "services/ui/model_style.h"

// Owns application-level state shared between systems.
typedef struct CoreContext {
    ModuleSchema ui_schema;
    ModuleSchema global_schema;

    Scene scene;
    Assets assets;
    Model* model;
} CoreContext;

#endif // APP_CONTEXT_CORE_CONTEXT_H
