#ifndef APP_SERVICES_H
#define APP_SERVICES_H

#include <stdbool.h>

#include "core/context.h"
#include "render/common/render_context.h"
#include "state/state_manager.h"
#include "ui/ui_context.h"

struct RenderRuntimeServiceContext;

// Central structure passed between application layers.
typedef struct AppServices {
    StateManager state_manager;
    int scene_type_id;
    int assets_type_id;
    int model_type_id;
    int ui_type_id;
    int render_ready_type_id;

    struct RenderRuntimeServiceContext* render_runtime_context;

    CoreContext core;
    UiContext ui;
    RenderRuntimeContext render;
} AppServices;

bool app_services_init(AppServices* services);
void app_services_shutdown(AppServices* services);

#endif // APP_SERVICES_H
