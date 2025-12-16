#include "app_services.h"

#include <stdio.h>
#include <string.h>

#include "core/service_manager/service_events.h"

AppServicesResult app_services_init(AppServices* services) {
    if (!services) return APP_SERVICES_ERROR_INVALID_ARGUMENT;

    memset(services, 0, sizeof(AppServices));
    StateManagerResult sm_result = state_manager_init(&services->state_manager, 8, 64);
    if (sm_result != STATE_MANAGER_OK) {
        fprintf(stderr, "app_services_init: state manager init failed: %s\n",
                state_manager_result_message(sm_result));
        return APP_SERVICES_ERROR_STATE_MANAGER_INIT;
    }

    Generated_RegisterComponents(services);

    return APP_SERVICES_OK;
}

void app_services_shutdown(AppServices* services) {
    if (!services) return;
    state_manager_dispose(&services->state_manager);
}

const char* app_services_result_message(AppServicesResult result) {
    switch (result) {
        case APP_SERVICES_OK:
            return "ok";
        case APP_SERVICES_ERROR_INVALID_ARGUMENT:
            return "invalid argument";
        case APP_SERVICES_ERROR_STATE_MANAGER_INIT:
            return "state manager init failed";
        case APP_SERVICES_ERROR_STATE_MANAGER_REGISTER:
            return "component type registration failed";
    }
    return "unknown error";
}