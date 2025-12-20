#include "engine/core/engine.h"
#include "foundation/logger/logger.h"
#include "features/math_engine/math_editor.h"

#include <string.h>
#include <stdlib.h>

// --- Application Logic ---



static void app_on_init(Engine* engine) {

    // Allocate MathEditorState

    MathEditorState* editor_state = (MathEditorState*)calloc(1, sizeof(MathEditorState));

    engine_set_user_data(engine, editor_state);



    // Initialize the Editor Feature

    math_editor_init(editor_state, engine);

}



static void app_on_update(Engine* engine) {

    MathEditorState* editor_state = (MathEditorState*)engine_get_user_data(engine);

    if (!editor_state) return;



    // Update the Editor Feature

    math_editor_update(editor_state, engine);



    // Render the Editor Feature

    Scene* scene = render_system_get_scene(engine_get_render_system(engine));

    if (scene) {

        math_editor_render(editor_state, scene, engine_get_assets(engine));

    }

}



static void app_on_shutdown(Engine* engine) {

    MathEditorState* editor_state = (MathEditorState*)engine_get_user_data(engine);

    if (editor_state) {

         math_editor_shutdown(editor_state, engine);

         free(editor_state);

    }

}



int main(int argc, char** argv) {

    EngineConfig config = {

        .width = 1280, .height = 720, .title = "Graphics Engine",

        .assets_path = "assets", .ui_path = "assets/ui/editor.yaml",

        .log_level = LOG_LEVEL_INFO,

        .on_init = app_on_init, .on_update = app_on_update

    };

    logger_init("logs/graphics.log");

    

    for (int i = 1; i < argc; ++i) {

        if (strcmp(argv[i], "--assets") == 0) config.assets_path = argv[++i];

        else if (strcmp(argv[i], "--ui") == 0) config.ui_path = argv[++i];

        else if (strcmp(argv[i], "--log-level") == 0) {

            const char* l = argv[++i];

            if (strcmp(l, "debug") == 0) config.log_level = LOG_LEVEL_DEBUG;

        } else if (strcmp(argv[i], "--log-interval") == 0) {

             float interval = atof(argv[++i]);

             logger_set_trace_interval(interval);

             config.screenshot_interval = (double)interval;

        }

    }



    Engine* engine = engine_create(&config);

    if (engine) {

        engine_run(engine);

        app_on_shutdown(engine); // Clean up user data

        engine_destroy(engine);

    }

    logger_shutdown();

    return 0;

}
