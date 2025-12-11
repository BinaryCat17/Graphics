#ifndef APP_SERVICES_H
#define APP_SERVICES_H

#include "app/context/core_context.h"
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

typedef enum AppServicesResult {
    APP_SERVICES_OK = 0,
    APP_SERVICES_ERROR_INVALID_ARGUMENT,
    APP_SERVICES_ERROR_STATE_MANAGER_INIT,
    APP_SERVICES_ERROR_STATE_MANAGER_REGISTER
} AppServicesResult;

AppServicesResult app_services_init(AppServices* services);
const char* app_services_result_message(AppServicesResult result);
void app_services_shutdown(AppServices* services);

#endif // APP_SERVICES_H
