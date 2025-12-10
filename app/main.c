#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "render/render_service.h"
#include "runtime/runtime.h"
#include "scene/scene_service.h"
#include "ui/ui_service.h"

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
    ui_context_init(&services.ui);

    if (!scene_service_load(&services.core, assets_dir, scene_path)) return 1;
    if (!ui_build(&services.ui, &services.core)) {
        ui_context_dispose(&services.ui);
        scene_service_unload(&services.core);
        return 1;
    }
    if (!runtime_init(&services)) {
        runtime_shutdown(&services);
        ui_context_dispose(&services.ui);
        scene_service_unload(&services.core);
        return 1;
    }
    if (!render_service_init(&services.render, &services.core.assets, services.ui.widgets)) {
        runtime_shutdown(&services);
        ui_context_dispose(&services.ui);
        scene_service_unload(&services.core);
        return 1;
    }

    render_loop(&services.render, &services.ui, services.core.model);

    render_service_shutdown(&services.render);
    runtime_shutdown(&services);
    ui_context_dispose(&services.ui);
    scene_service_unload(&services.core);
    return 0;
}
