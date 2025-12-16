#include "foundation/logger/logger.h"
#include <stdio.h>
#include <string.h>

#include "foundation/platform/platform.h"
#include "engine/assets/assets_service.h"
#include "engine/ui/ui_loader.h"
#include "engine/ui/ui_def.h"
#include "engine/render/render_system.h"
#include "domains/math_model/math_graph.h"
#include "foundation/meta/reflection.h"

int main(int argc, char** argv) {
    // 1. Config
    const char* assets_dir = "assets";
    const char* ui_path = "assets/ui/test_binding.yaml"; // New default
    
    // Default log level
    logger_set_level(LOG_LEVEL_INFO);

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--assets") == 0 && i + 1 < argc) {
            assets_dir = argv[++i];
        } else if (strcmp(argv[i], "--ui") == 0 && i + 1 < argc) {
            ui_path = argv[++i];
        } else if (strcmp(argv[i], "--log-level") == 0 && i + 1 < argc) {
            const char* level_str = argv[++i];
            if (strcmp(level_str, "trace") == 0) {
                logger_set_level(LOG_LEVEL_TRACE);
            } else if (strcmp(level_str, "debug") == 0) {
                logger_set_level(LOG_LEVEL_DEBUG);
            } else if (strcmp(level_str, "info") == 0) {
                logger_set_level(LOG_LEVEL_INFO);
            } else if (strcmp(level_str, "warn") == 0) {
                logger_set_level(LOG_LEVEL_WARN);
            } else if (strcmp(level_str, "error") == 0) {
                logger_set_level(LOG_LEVEL_ERROR);
            } else if (strcmp(level_str, "fatal") == 0) {
                logger_set_level(LOG_LEVEL_FATAL);
            }
        }
    }

    LOG_INFO("Initializing Graphics Engine (MVVM-C Architecture)...");
    LOG_INFO("Assets: %s", assets_dir);
    LOG_INFO("UI: %s", ui_path);

    // 2. Systems
    Assets assets = {0};
    // Note: assets_init old UI loading part is now ignored/redundant but harmless
    if (!assets_init(&assets, assets_dir, NULL)) return 1;

    // 3. Domain Data (The Model)
    MathGraph graph;
    math_graph_init(&graph);
    
    // Create a test node to bind to
    MathNode* node = math_graph_add_node(&graph, MATH_NODE_SIN);
    node->name = strdup("Debug Node");
    node->value = 0.0f;
    node->id = 101;
    node->dirty = 0;

    // 4. UI System (The View)
    UiDef* ui_def = ui_loader_load_from_file(ui_path);
    if (!ui_def) {
        LOG_ERROR("Failed to load UI definition from %s", ui_path);
        return 1;
    }

    // Create View bound to the Node
    const MetaStruct* node_meta = meta_get_struct("MathNode");
    if (!node_meta) {
        LOG_FATAL("Reflection metadata for 'MathNode' not found.");
        return 1;
    }
    
    UiView* root_view = ui_view_create(ui_def, node, node_meta);
    if (!root_view) {
        LOG_ERROR("Failed to create UI View.");
        return 1;
    }

    // 5. Render System
    RenderSystem render = {0};
    RenderSystemConfig render_config = { .backend_type = "vulkan" }; // Removed legacy log_level
    if (!render_system_init(&render, &render_config)) return 1;

    // 6. Bindings
    render_system_bind_assets(&render, &assets);
    render_system_bind_ui(&render, root_view);
    render_system_bind_math_graph(&render, &graph);

    // 7. Run
    LOG_INFO("Starting Main Loop...");
    render_system_run(&render);

    // 8. Shutdown
    render_system_shutdown(&render);
    ui_view_free(root_view);
    ui_def_free(ui_def);
    math_graph_dispose(&graph);
    assets_shutdown(&assets);

    return 0;
}
