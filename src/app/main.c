#include "engine/core/engine.h"
#include "foundation/logger/logger.h"
#include "features/math_engine/math_editor.h"
#include "foundation/config/config_system.h"
#include "engine/graphics/render_system.h"
#include "engine/graphics/stream.h"
#include "engine/graphics/internal/renderer_backend.h" // For manual dispatch test
#include "engine/assets/assets.h"
#include "foundation/memory/arena.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// --- Application Logic ---

static void app_on_init(Engine* engine) {
    // Allocate MathEditor
    MathEditor* editor = math_editor_create(engine);
    if (!editor) {
        LOG_FATAL("Failed to create MathEditor");
        exit(1);
    }
    engine_set_user_data(engine, editor);

    /*
    // --- INTEGRATION TEST: COMPUTE STREAM ---
    LOG_INFO("Running Compute Stream Integration Test...");
    
    RenderSystem* rs = engine_get_render_system(engine);
    Stream* stream = stream_create(rs, STREAM_FLOAT, 16);
    if (stream) {
        float input[16];
        for(int i=0; i<16; ++i) input[i] = (float)i;
        
        if (!stream_set_data(stream, input, 16)) {
            LOG_ERROR("Failed to upload stream data.");
        }
        
        const char* compute_src = 
            "#version 450\n"
            "layout(local_size_x = 16) in;\n"
            "layout(std430, set = 1, binding = 0) buffer Data { float values[]; };\n" // set 1 is buffers
            "void main() {\n"
            "    uint id = gl_GlobalInvocationID.x;\n"
            "    if (id < 16) values[id] = values[id] * 2.0;\n"
            "}\n";
            
        uint32_t pipeline = render_system_create_compute_pipeline_from_source(rs, compute_src);
        if (pipeline > 0) {
            stream_bind_compute(stream, 0); // Binding 0
            
            // Manual Dispatch for Test
            RendererBackend* backend = render_system_get_backend(rs);
            if (backend && backend->compute_dispatch) {
                backend->compute_dispatch(backend, pipeline, 1, 1, 1, NULL, 0);
                backend->compute_wait(backend);
                
                float output[16];
                if (stream_read_back(stream, output, 16)) {
                     LOG_INFO("Stream Readback: [0]=%.1f, [1]=%.1f ... [15]=%.1f", output[0], output[1], output[15]);
                     if (output[1] == 2.0f && output[15] == 30.0f) {
                         LOG_INFO("SUCCESS: Compute Stream Test Passed!");
                     } else {
                         LOG_ERROR("FAILURE: Compute Stream Test Values Incorrect.");
                     }
                } else {
                     LOG_ERROR("FAILURE: Stream Readback failed.");
                }
            }
            
            // Clean up test pipeline (optional, or reuse)
            render_system_destroy_compute_pipeline(rs, pipeline);
        } else {
            LOG_ERROR("Failed to compile test compute shader.");
        }
        
        stream_destroy(stream);
    }
    */
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
