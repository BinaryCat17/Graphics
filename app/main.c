#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "runtime/runtime.h"
#include "render_service.h"
#include "scene_service.h"
#include "ui_service.h"

int main(int argc, char** argv) {
    const char* assets_dir = "assets";
    const char* scene_path = NULL;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--assets") == 0 && i + 1 < argc) {
            assets_dir = argv[++i];
        } else if (strcmp(argv[i], "--scene") == 0 && i + 1 < argc) {
            scene_path = argv[++i];
        }
    }
    if (!scene_path) {
        fprintf(stderr, "Usage: %s --scene <file> [--assets <dir>]\n", argv[0]);
        return 1;
    }

    AppServices services = {0};
    if (!app_services_init(&services)) return 1;
    ui_context_init(&services.ui);

    ui_service_subscribe(&services.ui, &services.state_manager, services.model_type_id);
    render_service_bind(&services.render, &services.state_manager, services.assets_type_id, services.ui_type_id,
                        services.model_type_id);

    if (!scene_service_load(&services, assets_dir, scene_path)) {
        app_services_shutdown(&services);
        return 1;
    }

    state_manager_dispatch(&services.state_manager, 0);

    if (!ui_build(&services.ui, &services.core)) {
        ui_context_dispose(&services.ui);
        scene_service_unload(&services);
        app_services_shutdown(&services);
        return 1;
    }
    if (!runtime_init(&services)) {
        runtime_shutdown(&services);
        ui_context_dispose(&services.ui);
        scene_service_unload(&services);
        app_services_shutdown(&services);
        return 1;
    }

    state_manager_dispatch(&services.state_manager, 0);

    render_loop(&services.render, &services.state_manager);

    render_service_shutdown(&services.render);
    runtime_shutdown(&services);
    ui_context_dispose(&services.ui);
    scene_service_unload(&services);
    app_services_shutdown(&services);
    return 0;
}
