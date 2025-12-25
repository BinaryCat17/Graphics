#include "engine/core/engine.h"
#include "foundation/logger/logger.h"
#include "features/math_engine/math_editor.h"
#include "foundation/config/config_system.h"
#include "engine/graphics/render_system.h"
#include "engine/graphics/stream.h"
#include "engine/graphics/internal/backend/renderer_backend.h" // For manual dispatch test
#include "engine/assets/assets.h"
#include "foundation/memory/arena.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// --- Application Logic ---

static void app_on_init(Engine* engine) {
    // Register Features
    engine_register_feature(engine, math_engine_feature());
}

static void app_on_update(Engine* engine) {
    (void)engine;
    // Global app logic if any (e.g. global hotkeys)
    // Features are updated automatically by the engine.
}

int main(int argc, char** argv) {
    config_system_load(argc, argv);

    // Parse Log Level
    int log_level = LOG_LEVEL_INFO;
    const char* log_lvl_str = config_get_string("log_level", "info");

    if (strcmp(log_lvl_str, "debug") == 0 || strcmp(log_lvl_str, "DEBUG") == 0) log_level = LOG_LEVEL_DEBUG;
    else if (strcmp(log_lvl_str, "trace") == 0 || strcmp(log_lvl_str, "TRACE") == 0) log_level = LOG_LEVEL_TRACE;
    else if (strcmp(log_lvl_str, "warn") == 0 || strcmp(log_lvl_str, "WARN") == 0) log_level = LOG_LEVEL_WARN;
    else if (strcmp(log_lvl_str, "error") == 0 || strcmp(log_lvl_str, "ERROR") == 0) log_level = LOG_LEVEL_ERROR;
    else if (strcmp(log_lvl_str, "fatal") == 0 || strcmp(log_lvl_str, "FATAL") == 0) log_level = LOG_LEVEL_FATAL;

    EngineConfig config = {
        .width = config_get_int("width", 1280), 
        .height = config_get_int("height", 720), 
        .title = config_get_string("title", "Graphics Engine"),
        .assets_path = config_get_string("assets", "assets"), 
        .ui_path = config_get_string("ui", "assets/ui/editor.yaml"),
        .log_level = log_level,
        .on_init = app_on_init, 
        .on_update = app_on_update
    };
    
    // Log Interval / Screenshot
    float interval = config_get_float("log_interval", 0.0f);
    if (interval > 0.0f) {
         logger_set_trace_interval(interval);
         config.screenshot_interval = (double)interval;
    }

    logger_init("logs/graphics.log");

    Engine* engine = engine_create(&config);
    if (engine) {
        engine_run(engine);
        // Features are shut down automatically by engine_destroy
        engine_destroy(engine);
    }
    
    config_system_shutdown();
    logger_shutdown();
    return 0;
}
