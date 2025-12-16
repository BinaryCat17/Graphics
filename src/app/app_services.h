#ifndef APP_SERVICES_H
#define APP_SERVICES_H

#include "services_registry.h"
#include "services/core/core_context.h"
#include "services/render/backend/common/render_context.h"
#include "core/state/state_manager.h"
#include "services/ui/ui_context.h"

struct RenderRuntimeServiceContext;

// Central structure passed between application layers.
typedef GeneratedServicesContext AppServices;

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
