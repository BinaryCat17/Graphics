#include "engine/core/engine.h"
#include "foundation/logger/logger.h"
#include "features/math_engine/math_editor.h"
#include "foundation/config/config_system.h"
#include "engine/graphics/render_system.h"
#include "engine/assets/assets.h"
#include "foundation/memory/arena.h"

#include <string.h>
#include <stdlib.h>

// --- Application Logic ---

static void app_on_init(Engine* engine) {
    // Allocate MathEditor
    MathEditor* editor = math_editor_create(engine);
    if (!editor) {
        LOG_FATAL("Failed to create MathEditor");
        exit(1);
    }
    engine_set_user_data(engine, editor);
}

static void app_on_update(Engine* engine) {
    MathEditor* editor = (MathEditor*)engine_get_user_data(engine);
    if (!editor) return;

    // Update the Editor Feature
    math_editor_update(editor, engine);

    // Render the Editor Feature
    Scene* scene = render_system_get_scene(engine_get_render_system(engine));
    if (scene) {
        math_editor_render(editor, scene, engine_get_assets(engine), engine_get_frame_arena(engine));
    }
}

static void app_on_shutdown(Engine* engine) {
    MathEditor* editor = (MathEditor*)engine_get_user_data(engine);
    if (editor) {
         math_editor_destroy(editor);
    }
}

int main(int argc, char** argv) {
    config_system_load(argc, argv);

    // Parse Log Level
    int log_level = LOG_LEVEL_INFO;
    const char* log_lvl_str = config_get_string("log_level", "info");

    if (strcmp(log_lvl_str, "debug") == 0) log_level = LOG_LEVEL_DEBUG;
    else if (strcmp(log_lvl_str, "trace") == 0) log_level = LOG_LEVEL_TRACE;
    else if (strcmp(log_lvl_str, "warn") == 0) log_level = LOG_LEVEL_WARN;
    else if (strcmp(log_lvl_str, "error") == 0) log_level = LOG_LEVEL_ERROR;
    else if (strcmp(log_lvl_str, "fatal") == 0) log_level = LOG_LEVEL_FATAL;

    EngineConfig config = {
        .width = config_get_int("width", 1280), 
        .height = config_get_int("height", 720), 
        .title = config_get_string("title", "Graphics Engine"),
        .assets_path = config_get_string("assets", "assets"), 
        .ui_path = config_get_string("ui", "assets/ui/editor.yaml"),
        .log_level = log_level,
        .on_init = app_on_init, .on_update = app_on_update
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
        app_on_shutdown(engine); // Clean up user data
        engine_destroy(engine);
    }
    
    config_system_shutdown();
    logger_shutdown();
    return 0;
}
