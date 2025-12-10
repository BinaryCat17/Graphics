#include "app_services.h"

#include <string.h>

#include "services/service_events.h"

bool app_services_init(AppServices* services) {
    if (!services) return false;

    memset(services, 0, sizeof(AppServices));
    state_manager_init(&services->state_manager, 8, 64);

    services->scene_type_id =
        state_manager_register_type(&services->state_manager, STATE_COMPONENT_SCENE, sizeof(SceneComponent), 1);
    services->assets_type_id =
        state_manager_register_type(&services->state_manager, STATE_COMPONENT_ASSETS, sizeof(AssetsComponent), 1);
    services->model_type_id =
        state_manager_register_type(&services->state_manager, STATE_COMPONENT_MODEL, sizeof(ModelComponent), 1);
    services->ui_type_id =
        state_manager_register_type(&services->state_manager, STATE_COMPONENT_UI, sizeof(UiRuntimeComponent), 1);

    return services->scene_type_id >= 0 && services->assets_type_id >= 0 && services->model_type_id >= 0 &&
           services->ui_type_id >= 0;
}

void app_services_shutdown(AppServices* services) {
    if (!services) return;
    state_manager_dispose(&services->state_manager);
}

