#include <stdio.h>
#include <string.h>

#include "foundation/platform/platform.h"
#include "engine/assets/assets_service.h"
#include "engine/ui/ui_service.h"
#include "engine/render/render_system.h"
#include "domains/cad_model/scene_service.h"

int main(int argc, char** argv) {
    // 1. Config
    const char* assets_dir = "assets";
    const char* scene_path = "assets/scenes/gear_reducer.yaml"; // Default
    const char* ui_path = NULL;
    RenderLogLevel log_level = RENDER_LOG_INFO;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--assets") == 0 && i + 1 < argc) {
            assets_dir = argv[++i];
        } else if (strcmp(argv[i], "--scene") == 0 && i + 1 < argc) {
            scene_path = argv[++i];
        } else if (strcmp(argv[i], "--ui") == 0 && i + 1 < argc) {
            ui_path = argv[++i];
        } else if (strcmp(argv[i], "--log-level") == 0 && i + 1 < argc) {
            const char* level_str = argv[++i];
            if (strcmp(level_str, "none") == 0) log_level = RENDER_LOG_NONE;
            else if (strcmp(level_str, "info") == 0) log_level = RENDER_LOG_INFO;
            else if (strcmp(level_str, "verbose") == 0) log_level = RENDER_LOG_VERBOSE;
        }
    }

    printf("Initializing Graphics Engine...\n");
    printf("Assets: %s\n", assets_dir);
    printf("Scene: %s\n", scene_path);
    printf("Log Level: %d\n", log_level);

    // 2. Systems
    Assets assets = {0};
    if (!assets_init(&assets, assets_dir, ui_path)) return 1;

    Scene scene = {0};
    Model* model = scene_load(&scene, scene_path, &assets);
    if (!model) {
        fprintf(stderr, "Failed to load scene/model.\n");
        return 1;
    }

    UiContext ui = {0};
    if (!ui_system_init(&ui)) return 1;
    if (!ui_system_build(&ui, &assets, &scene, model)) return 1;

    RenderSystem render = {0};
    RenderSystemConfig render_config = { .backend_type = "vulkan", .log_level = log_level };
    if (!render_system_init(&render, &render_config)) return 1;

    // 3. Bindings
    render_system_bind_assets(&render, &assets);
    render_system_bind_ui(&render, &ui);
    render_system_bind_model(&render, model);

    // 4. Run
    ui_system_prepare_runtime(&ui, 1.0f);
    render_thread_update_window_state(&render);

    printf("Starting Main Loop...\n");
    render_system_run(&render);

    // 5. Shutdown
    render_system_shutdown(&render);
    ui_system_shutdown(&ui);
    scene_unload(&scene);
    assets_shutdown(&assets);
    if (model) free_model(model);

    return 0;
}