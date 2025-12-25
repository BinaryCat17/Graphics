#include "engine/core/engine.h"
#include "foundation/logger/logger.h"
#include "foundation/platform/platform.h"
#include "foundation/platform/fs.h"
#include "foundation/meta/reflection.h"
#include "engine/text/font.h"
#include "foundation/memory/arena.h"
#include "engine/graphics/render_system.h"
#include "engine/assets/assets.h"
#include "engine/input/input.h"
#include "engine/graphics/graphics_types.h"
#include "engine/graphics/gpu_input.h"
#include "engine/ui/ui_core.h"
#include "engine/ui/ui_renderer.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct Engine {
    // Platform
    PlatformWindow* window;
    InputSystem* input_system;

    // Systems
    RenderSystem* render_system;
    Assets* assets;
    
    // Application Data
    void* user_data;
    MemoryArena frame_arena;
    
    // Features
    EngineFeature features[32];
    int feature_count;
    
    // State
    bool running;
    bool show_compute_visualizer;
    EngineConfig config;
    double screenshot_interval;
    double last_screenshot_time;
    double last_time;
    float dt;
    
    // Callbacks
    void (*on_update)(Engine* engine);
} Engine;

// --- Input Callbacks ---

static void on_framebuffer_size(PlatformWindow* window, int width, int height, void* user_data) {
    (void)window;
    Engine* engine = (Engine*)user_data;
    if (!engine) return;
    
    if (engine->render_system) {
        render_system_resize(engine->render_system, width, height);
    }
}

void engine_register_feature(Engine* engine, EngineFeature feature) {
    if (!engine || engine->feature_count >= 32) return;
    
    engine->features[engine->feature_count] = feature;
    EngineFeature* f = &engine->features[engine->feature_count];
    engine->feature_count++;
    
    LOG_INFO("Engine: Registered feature '%s'", f->name ? f->name : "Unknown");
    
    if (f->on_init) {
        f->on_init(f, engine);
    }
}

Engine* engine_create(const EngineConfig* config) {
    if (!config) return NULL;

    Engine* engine = (Engine*)calloc(1, sizeof(Engine));
    if (!engine) return NULL;

    // Init Frame Arena (16MB)
    if (!arena_init(&engine->frame_arena, 16 * 1024 * 1024)) {
        LOG_FATAL("Failed to initialize Frame Arena.");
        goto cleanup_engine;
    }

    engine->config = *config;

    // Store Callbacks
    engine->on_update = config->on_update;

    // 1. Logger
    logger_set_console_level(config->log_level);
    LOG_INFO("Engine Initializing...");
    
    // 2. Platform & Window
    if (!platform_layer_init()) {
        LOG_FATAL("Failed to initialize platform layer.");
        goto cleanup_frame_arena;
    }
    
    engine->window = platform_create_window(config->width, config->height, config->title);
    if (!engine->window) {
        LOG_FATAL("Failed to create window.");
        goto cleanup_platform;
    }
    
    // Callbacks
    platform_set_window_user_pointer(engine->window, engine);
    // Note: Input callbacks are registered by input_system_init
    platform_set_framebuffer_size_callback(engine->window, on_framebuffer_size, engine);

    // 3. Input System
    engine->input_system = input_system_create(engine->window);
    if (!engine->input_system) {
        LOG_FATAL("Failed to initialize InputSystem.");
        goto cleanup_window;
    }

    // 4. Assets
    engine->assets = assets_create(config->assets_path);
    if (!engine->assets) {
        LOG_FATAL("Failed to initialize assets from '%s'", config->assets_path);
        goto cleanup_input;
    }

    // 5. Render System
    RenderSystemConfig rs_config = {
        .window = engine->window,
        .backend_type = "vulkan"
    };
    engine->render_system = render_system_create(&rs_config);
    if (!engine->render_system) {
        LOG_FATAL("Failed to initialize RenderSystem.");
        goto cleanup_assets;
    }

    // 6. UI System
    ui_system_init();

    // Bindings
    render_system_bind_assets(engine->render_system, engine->assets);
    
    // 7. Application Init Hook (App sets up Graph, UI, binds them to Renderer)
    if (config->on_init) {
        config->on_init(engine);
    }
    
    // Screenshot Init
    engine->screenshot_interval = config->screenshot_interval;
    engine->last_screenshot_time = platform_get_time_ms() / 1000.0;
    
    // Clean old screenshots
    if (engine->screenshot_interval > 0.0) {
        if (!platform_mkdir("logs")) platform_mkdir("logs");
        if (platform_mkdir("logs/screenshots")) {
             // Directory exists or created, clean it
             PlatformDir* dir = platform_dir_open("logs/screenshots");
             if (dir) {
                 PlatformDirEntry entry;
                 while (platform_dir_read(dir, &entry)) {
                     if (!entry.is_dir) {
                         char path[512];
                         snprintf(path, sizeof(path), "logs/screenshots/%s", entry.name);
                         platform_remove_file(path);
                     }
                     free(entry.name);
                 }
                 platform_dir_close(dir);
             }
        }
    }

    engine->running = true;
    return engine;

cleanup_assets:
    assets_destroy(engine->assets);
cleanup_input:
    input_system_destroy(engine->input_system);
cleanup_window:
    platform_destroy_window(engine->window);
cleanup_platform:
    platform_layer_shutdown();
cleanup_frame_arena:
    arena_destroy(&engine->frame_arena);
cleanup_engine:
    free(engine);
    return NULL;
}

void engine_run(Engine* engine) {
    if (!engine) return;

    LOG_INFO("Engine Loop Starting...");
    
    RenderSystem* rs = engine->render_system;
    engine->last_time = platform_get_time_ms() / 1000.0;
    
    while (engine->running && !platform_window_should_close(engine->window)) {
        double now = platform_get_time_ms() / 1000.0;
        engine->dt = (float)(now - engine->last_time);
        engine->last_time = now;

        // Reset Frame Arena
        arena_reset(&engine->frame_arena);
        
        // Auto Screenshot
        if (engine->screenshot_interval > 0.0 && (now - engine->last_screenshot_time) > engine->screenshot_interval) {
            char path[256];
            snprintf(path, sizeof(path), "logs/screenshots/screen_%.3f.png", now);
            LOG_INFO("Requesting screenshot: %s", path);
            render_system_request_screenshot(rs, path);
            engine->last_screenshot_time = now;
        }

        render_system_begin_frame(rs, now);
        
        // Input Update
        input_system_update(engine->input_system);
        
        // Input Poll (Triggers callbacks)
        platform_poll_events();

        // GPU Input Sync (Branch B)
        {
            PlatformWindowSize size = platform_get_framebuffer_size(engine->window);
            GpuInputState gpu_input = {0};
            gpu_input_update(&gpu_input, engine->input_system, (float)now, engine->dt, (float)size.width, (float)size.height);
            render_system_update_gpu_input(rs, &gpu_input);
        }

        // Application Update Hook (App updates Graph, UI Layout, etc.)
        if (engine->on_update) {
            engine->on_update(engine);
        }

        // Features Update
        for (int i = 0; i < engine->feature_count; ++i) {
            EngineFeature* f = &engine->features[i];
            if (f->on_update) f->on_update(f, engine);
        }

        // Features Extract
        for (int i = 0; i < engine->feature_count; ++i) {
            EngineFeature* f = &engine->features[i];
            if (f->on_extract) f->on_extract(f, engine);
        }

        // Extract UI to Render Batches
        ui_renderer_extract(render_system_get_scene(rs), rs);

        // Render Update
        render_system_update(rs);

        // Draw
        render_system_draw(rs);
    }
}

void engine_destroy(Engine* engine) {
    if (!engine) return;
    
    // Shutdown features in reverse order
    for (int i = engine->feature_count - 1; i >= 0; --i) {
        EngineFeature* f = &engine->features[i];
        if (f->on_shutdown) f->on_shutdown(f);
    }
    
    ui_system_shutdown();
    render_system_destroy(engine->render_system);
    input_system_destroy(engine->input_system);
    assets_destroy(engine->assets);
    
    if (engine->window) {
        platform_destroy_window(engine->window);
    }
    platform_layer_shutdown();
    arena_destroy(&engine->frame_arena);
    free(engine);
}

// --- Accessors ---

RenderSystem* engine_get_render_system(Engine* engine) {
    return engine ? engine->render_system : NULL;
}

InputSystem* engine_get_input_system(Engine* engine) {
    return engine ? engine->input_system : NULL;
}

Assets* engine_get_assets(Engine* engine) {
    return engine ? engine->assets : NULL;
}

PlatformWindow* engine_get_window(Engine* engine) {
    return engine ? engine->window : NULL;
}

MemoryArena* engine_get_frame_arena(Engine* engine) {
    return engine ? &engine->frame_arena : NULL;
}

const EngineConfig* engine_get_config(const Engine* engine) {
    return engine ? &engine->config : NULL;
}

void* engine_get_user_data(const Engine* engine) {
    return engine ? engine->user_data : NULL;
}

void engine_set_user_data(Engine* engine, void* user_data) {
    if (engine) engine->user_data = user_data;
}

float engine_get_dt(const Engine* engine) {
    return engine ? engine->dt : 0.0f;
}

bool engine_is_running(const Engine* engine) {
    return engine ? engine->running : false;
}

void engine_set_show_compute(Engine* engine, bool show) {
    if (engine) engine->show_compute_visualizer = show;
}

bool engine_get_show_compute(const Engine* engine) {
    return engine ? engine->show_compute_visualizer : false;
}
