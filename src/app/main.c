#include "engine/core/engine.h"
#include "foundation/logger/logger.h"
#include "features/graph_editor/transpiler.h"
#include "foundation/platform/platform.h"
#include "engine/graphics/backend/renderer_backend.h"
#include <string.h>
#include <stdlib.h>

#define KEY_C 67

// --- Application Logic ---

static void app_setup_graph(Engine* engine) {
    LOG_INFO("App: Setting up default Math Graph...");
    
    // Create Test Nodes (Visualizer Graph)
    MathNode* uv = math_graph_add_node(&engine->graph, MATH_NODE_UV);
    uv->name = strdup("UV.x");
    uv->x = 50; uv->y = 100;

    MathNode* freq = math_graph_add_node(&engine->graph, MATH_NODE_VALUE);
    freq->name = strdup("Frequency");
    freq->value = 20.0f;
    freq->x = 50; freq->y = 250;
    
    MathNode* mul = math_graph_add_node(&engine->graph, MATH_NODE_MUL);
    mul->name = strdup("Multiply");
    mul->x = 250; mul->y = 175;
    
    MathNode* s = math_graph_add_node(&engine->graph, MATH_NODE_SIN);
    s->name = strdup("Sin");
    s->x = 450; s->y = 175;
    
    math_graph_connect(mul, 0, uv);
    math_graph_connect(mul, 1, freq);
    math_graph_connect(s, 0, mul);
}

static void app_on_init(Engine* engine) {
    app_setup_graph(engine);
}

static void app_on_update(Engine* engine) {
    static bool key_c_prev = false;
    bool key_c_curr = platform_get_key(engine->window, KEY_C);
    
    // Toggle Compute Visualization
    if (key_c_curr && !key_c_prev) {
        engine->show_compute_visualizer = !engine->show_compute_visualizer;
        engine->render_system.show_compute_result = engine->show_compute_visualizer;
        
        if (engine->show_compute_visualizer) {
            LOG_INFO("App: Transpiling & Running Compute Graph...");
            
            // 1. Transpile Graph to GLSL
            char* glsl = math_graph_transpile_glsl(&engine->graph, TRANSPILE_MODE_IMAGE_2D);
            
            // 2. Run on GPU
            if (glsl) {
                RenderSystem* rs = &engine->render_system;
                if (rs->backend && rs->backend->run_compute_image) {
                    rs->backend->run_compute_image(rs->backend, glsl, 512, 512);
                }
                free(glsl);
            }
        }
    }
    key_c_prev = key_c_curr;
}

// --- Main Entry Point ---

int main(int argc, char** argv) {
    // 1. Config
    EngineConfig config = {
        .width = 1280,
        .height = 720,
        .title = "Graphics Engine",
        .assets_path = "assets",
        .ui_path = "assets/ui/editor.yaml",
        .log_level = LOG_LEVEL_INFO,
        // Bind Callbacks
        .on_init = app_on_init,
        .on_update = app_on_update
    };

    logger_init("logs/graphics.log");

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--assets") == 0 && i + 1 < argc) {
            config.assets_path = argv[++i];
        } else if (strcmp(argv[i], "--ui") == 0 && i + 1 < argc) {
            config.ui_path = argv[++i];
        } else if (strcmp(argv[i], "--log-level") == 0 && i + 1 < argc) {
            const char* level_str = argv[++i];
            if (strcmp(level_str, "trace") == 0) config.log_level = LOG_LEVEL_TRACE;
            else if (strcmp(level_str, "debug") == 0) config.log_level = LOG_LEVEL_DEBUG;
            else if (strcmp(level_str, "info") == 0) config.log_level = LOG_LEVEL_INFO;
            else if (strcmp(level_str, "warn") == 0) config.log_level = LOG_LEVEL_WARN;
            else if (strcmp(level_str, "error") == 0) config.log_level = LOG_LEVEL_ERROR;
            else if (strcmp(level_str, "fatal") == 0) config.log_level = LOG_LEVEL_FATAL;
        } else if (strcmp(argv[i], "--log-interval") == 0 && i + 1 < argc) {
             logger_set_trace_interval(atof(argv[++i]));
        }
    }

    // 2. Engine Lifecycle
    Engine engine;
    if (engine_init(&engine, &config)) {
        engine_run(&engine);
        engine_shutdown(&engine);
    } else {
        LOG_FATAL("Engine failed to initialize.");
        return 1;
    }

    logger_shutdown();
    return 0;
}