#include "engine/core/engine.h"
#include "foundation/logger/logger.h"
#include "foundation/platform/platform.h"
#include "foundation/platform/fs.h"
#include "foundation/meta/reflection.h"
#include "engine/text/font.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// --- Input Callbacks ---

static void on_framebuffer_size(PlatformWindow* window, int width, int height, void* user_data) {
    (void)window;
    Engine* engine = (Engine*)user_data;
    if (!engine) return;
    
    if (engine->render_system) {
        render_system_resize(engine->render_system, width, height);
    }
}

bool engine_init(Engine* engine, const EngineConfig* config) {
    if (!engine || !config) return false;
    memset(engine, 0, sizeof(Engine));
    engine->config = *config;

    // Store Callbacks
    engine->on_update = config->on_update;

    // 1. Logger
    logger_set_level(config->log_level);
    LOG_INFO("Engine Initializing...");
    
    // 2. Platform & Window
    if (!platform_layer_init()) {
        LOG_FATAL("Failed to initialize platform layer.");
        return false;
    }
    
    engine->window = platform_create_window(config->width, config->height, config->title);
    if (!engine->window) {
        LOG_FATAL("Failed to create window.");
        return false;
    }
    
    // Callbacks
    platform_set_window_user_pointer(engine->window, engine);
    // Note: Input callbacks are registered by input_system_init
    platform_set_framebuffer_size_callback(engine->window, on_framebuffer_size, engine);

    // 3. Input System
    input_system_init(&engine->input_system, engine->window);

    // 4. Assets
    if (!assets_init(&engine->assets, config->assets_path, NULL)) {
        LOG_FATAL("Failed to initialize assets from '%s'", config->assets_path);
        return false;
    }

    if (!font_init(engine->assets.font_path)) {
        LOG_FATAL("Failed to initialize font from '%s'", engine->assets.font_path);
        return false;
    }

    // 5. Render System
    RenderSystemConfig rs_config = {
        .window = engine->window,
        .backend_type = "vulkan"
    };
    engine->render_system = render_system_create(&rs_config);
    if (!engine->render_system) {
        LOG_FATAL("Failed to initialize RenderSystem.");
        return false;
    }

    // Bindings
    render_system_bind_assets(engine->render_system, &engine->assets);
    
    // 6. Application Init Hook (App sets up Graph, UI, binds them to Renderer)
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
    return true;
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
        input_system_update(&engine->input_system);
        
        // Input Poll (Triggers callbacks)
        platform_poll_events();

        // Application Update Hook (App updates Graph, UI Layout, etc.)
        if (engine->on_update) {
            engine->on_update(engine);
        }

        // Render Update
        render_system_update(rs);

        // Draw
        render_system_draw(rs);
    }
}

void engine_shutdown(Engine* engine) {
    if (!engine) return;
    
    render_system_destroy(engine->render_system);
    assets_shutdown(&engine->assets);
    
    if (engine->window) {
        platform_destroy_window(engine->window);
    }
    platform_layer_shutdown();
}