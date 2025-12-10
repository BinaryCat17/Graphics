#ifndef APP_SERVICES_H
#define APP_SERVICES_H

#include <stdbool.h>

#include "core/context.h"
#include "render/render_context.h"
#include "state/state_manager.h"
#include "ui/ui_context.h"

// Central structure passed between application layers.
typedef struct AppServices {
    StateManager state_manager;
    int scene_type_id;
    int assets_type_id;
    int model_type_id;
    int ui_type_id;

    CoreContext core;
    UiContext ui;
    RenderRuntimeContext render;
} AppServices;

bool app_services_init(AppServices* services);
void app_services_shutdown(AppServices* services);

#endif // APP_SERVICES_H
