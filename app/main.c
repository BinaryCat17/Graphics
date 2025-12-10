#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "runtime/runtime.h"
#include "service.h"
#include "render_runtime_service.h"
#include "render_service.h"
#include "scene_service.h"
#include "service_manager.h"
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
    if (!app_services_init(&services)) {
        fprintf(stderr, "Failed to initialize application services.\n");
        return 1;
    }

    ServiceConfig config = {.assets_dir = assets_dir, .scene_path = scene_path};

    ServiceManager manager;
    service_manager_init(&manager);

    if (!service_manager_register(&manager, scene_service_descriptor()) ||
        !service_manager_register(&manager, ui_service_descriptor()) ||
        !service_manager_register(&manager, render_runtime_service_descriptor()) ||
        !service_manager_register(&manager, render_service_descriptor())) {
        fprintf(stderr, "Failed to register required services.\n");
        app_services_shutdown(&services);
        return 1;
    }

    if (!service_manager_start(&manager, &services, &config)) {
        fprintf(stderr, "Application exiting because not all services started successfully.\n");
        app_services_shutdown(&services);
        return 1;
    }

    service_manager_wait(&manager);

    service_manager_stop(&manager, &services);
    app_services_shutdown(&services);

    return 0;
}
