#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
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
    const char* ui_config_path = NULL;
    const char* renderer_backend = "vulkan";
    const char* render_log_sink = "stdout";
    const char* render_log_target = NULL;
    bool render_log_enabled = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--assets") == 0 && i + 1 < argc) {
            assets_dir = argv[++i];
        } else if (strcmp(argv[i], "--scene") == 0 && i + 1 < argc) {
            scene_path = argv[++i];
        } else if (strcmp(argv[i], "--ui") == 0 && i + 1 < argc) {
            ui_config_path = argv[++i];
        } else if (strcmp(argv[i], "--renderer") == 0 && i + 1 < argc) {
            renderer_backend = argv[++i];
        } else if (strcmp(argv[i], "--render-log") == 0) {
            render_log_enabled = true;
        } else if (strcmp(argv[i], "--render-log-sink") == 0 && i + 1 < argc) {
            render_log_sink = argv[++i];
            render_log_enabled = true;
        } else if (strcmp(argv[i], "--render-log-target") == 0 && i + 1 < argc) {
            render_log_target = argv[++i];
            render_log_enabled = true;
        }
    }
    if (!scene_path) {
        fprintf(stderr, "Usage: %s --scene <file> [--assets <dir>] [--ui <ui.yaml>]\n", argv[0]);
        return 1;
    }

    AppServices services = {0};
    if (!app_services_init(&services)) {
        fprintf(stderr, "Failed to initialize application services.\n");
        return 1;
    }

    ServiceConfig config = {.assets_dir = assets_dir,
                           .scene_path = scene_path,
                           .ui_config_path = ui_config_path,
                           .renderer_backend = renderer_backend,
                           .render_log_sink = render_log_sink,
                           .render_log_target = render_log_target,
                           .render_log_enabled = render_log_enabled};

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
