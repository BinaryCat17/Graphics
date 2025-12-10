#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "runtime/runtime.h"
#include "service.h"
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
    if (!app_services_init(&services)) {
        fprintf(stderr, "Failed to initialize application services.\n");
        return 1;
    }

    if (!service_registry_register(scene_service_descriptor()) || !service_registry_register(ui_service_descriptor()) ||
        !service_registry_register(render_service_descriptor())) {
        fprintf(stderr, "Failed to register required services.\n");
        app_services_shutdown(&services);
        return 1;
    }

    const char* requested_services[] = {"scene", "ui", "render"};
    const size_t requested_count = sizeof(requested_services) / sizeof(requested_services[0]);
    const ServiceDescriptor* descriptors[3] = {0};

    for (size_t i = 0; i < requested_count; ++i) {
        descriptors[i] = service_registry_get(requested_services[i]);
        if (!descriptors[i]) {
            fprintf(stderr, "Unknown service: %s\n", requested_services[i]);
            app_services_shutdown(&services);
            return 1;
        }
    }

    ServiceConfig config = {.assets_dir = assets_dir, .scene_path = scene_path};

    for (size_t i = 0; i < requested_count; ++i) {
        if (descriptors[i]->init && !descriptors[i]->init(&services, &config)) {
            fprintf(stderr, "Service '%s' failed to initialize.\n", descriptors[i]->name);
            app_services_shutdown(&services);
            return 1;
        }
    }

    size_t started_count = 0;
    for (; started_count < requested_count; ++started_count) {
        if (descriptors[started_count]->start && !descriptors[started_count]->start(&services, &config)) {
            fprintf(stderr, "Service '%s' failed to start.\n", descriptors[started_count]->name);
            break;
        }
        state_manager_dispatch(&services.state_manager, 0);
    }

    for (size_t i = started_count; i-- > 0;) {
        if (descriptors[i]->stop) descriptors[i]->stop(&services);
    }

    app_services_shutdown(&services);

    if (started_count != requested_count) {
        fprintf(stderr, "Application exiting because not all services started successfully.\n");
        return 1;
    }

    return 0;
}
