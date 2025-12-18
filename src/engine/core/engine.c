#include "engine/core/engine.h"
#include "foundation/logger/logger.h"
#include "foundation/platform/platform.h"
#include "foundation/meta/reflection.h"
#include "engine/ui/ui_layout.h"
#include "engine/graphics/text/font.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// --- Input Callbacks ---

static void on_mouse_button(PlatformWindow* window, PlatformMouseButton button, PlatformInputAction action, int mods, void* user_data) {
    (void)window; (void)mods;
    Engine* engine = (Engine*)user_data;
    if (!engine) return;
    
    if (button == PLATFORM_MOUSE_BUTTON_LEFT) {
        if (action == PLATFORM_PRESS) {
            engine->input.mouse_down = true;
            engine->input.mouse_clicked = true;
        } else if (action == PLATFORM_RELEASE) {
            engine->input.mouse_down = false;
        }
    }
}

static void on_scroll(PlatformWindow* window, double xoff, double yoff, void* user_data) {
    (void)window; 
    Engine* engine = (Engine*)user_data;
    if (!engine) return;

    engine->input.scroll_dx += (float)xoff;
    engine->input.scroll_dy += (float)yoff;
}

static void on_cursor_pos(PlatformWindow* window, double x, double y, void* user_data) {
    (void)window;
    Engine* engine = (Engine*)user_data;
    if (!engine) return;

    engine->input.mouse_x = (float)x;
    engine->input.mouse_y = (float)y;
}

static void on_framebuffer_size(PlatformWindow* window, int width, int height, void* user_data) {
    (void)window;
    Engine* engine = (Engine*)user_data;
    if (!engine) return;
    
    if (engine->render_system.backend && engine->render_system.backend->update_viewport) {
        engine->render_system.backend->update_viewport(engine->render_system.backend, width, height);
    }
}

bool engine_init(Engine* engine, const EngineConfig* config) {
    if (!engine || !config) return false;
    memset(engine, 0, sizeof(Engine));

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
    platform_set_mouse_button_callback(engine->window, on_mouse_button, engine);
    platform_set_scroll_callback(engine->window, on_scroll, engine);
    platform_set_cursor_pos_callback(engine->window, on_cursor_pos, engine);
    platform_set_framebuffer_size_callback(engine->window, on_framebuffer_size, engine);

    // 3. Assets
    if (!assets_init(&engine->assets, config->assets_path, NULL)) {
        LOG_FATAL("Failed to initialize assets from '%s'", config->assets_path);
        return false;
    }

    if (!font_init(engine->assets.font_path)) {
        LOG_FATAL("Failed to initialize font from '%s'", engine->assets.font_path);
        return false;
    }

    // 4. Math Graph (Model)
    math_graph_init(&engine->graph);
    
    // Application Init Hook (App sets up the graph here)
    if (config->on_init) {
        config->on_init(engine);
    }

    // 5. UI (View)
    engine->ui_def = ui_loader_load_from_file(config->ui_path);
    if (!engine->ui_def) {
        LOG_ERROR("Failed to load UI definition from '%s'", config->ui_path);
        return false;
    }

    const MetaStruct* graph_meta = meta_get_struct("MathGraph");
    if (!graph_meta) {
        LOG_FATAL("Reflection metadata for 'MathGraph' not found.");
        return false;
    }
    
    engine->ui_root = ui_view_create(engine->ui_def, &engine->graph, graph_meta);
    if (!engine->ui_root) {
        LOG_ERROR("Failed to create UI View.");
        return false;
    }

    // 6. Render System
    RenderSystemConfig rs_config = {
        .window = engine->window,
        .backend_type = "vulkan",
        .log_level = config->log_level
    };
    if (!render_system_init(&engine->render_system, &rs_config)) {
        LOG_FATAL("Failed to initialize RenderSystem.");
        return false;
    }

    // Bindings (Legacy coupling, to be refactored)
    render_system_bind_assets(&engine->render_system, &engine->assets);
    render_system_bind_ui(&engine->render_system, engine->ui_root);
    
    engine->running = true;
    return true;
}

void engine_run(Engine* engine) {
    if (!engine) return;

    LOG_INFO("Engine Loop Starting...");
    
    RenderSystem* rs = &engine->render_system;
    
    while (engine->running && !platform_window_should_close(engine->window)) {
        rs->frame_count++;
        
        // Reset per-frame input deltas
        engine->input.scroll_dx = 0;
        engine->input.scroll_dy = 0;
        
        // Input Poll (Triggers callbacks)
        platform_poll_events();
        
        // Toggle Logic for Click
        static bool last_down = false;
        engine->input.mouse_clicked = engine->input.mouse_down && !last_down;
        last_down = engine->input.mouse_down;

        // Application Update Hook
        if (engine->on_update) {
            engine->on_update(engine);
        }

        // Update UI
        if (engine->ui_root) {
            ui_view_process_input(engine->ui_root, &engine->input);
            ui_view_update(engine->ui_root);
            
            PlatformWindowSize size = platform_get_framebuffer_size(engine->window);
            ui_layout_root(engine->ui_root, (float)size.width, (float)size.height, rs->frame_count, false);
        }

        // Update Graph
        math_graph_update(&engine->graph);
        math_graph_update_visuals(&engine->graph, false);

        // Render Update
        render_system_update(rs);

        // Draw
        if (rs->renderer_ready && rs->backend) {
            const RenderFramePacket* packet = render_system_acquire_packet(rs);
            if (packet && rs->backend->render_scene) {
                rs->backend->render_scene(rs->backend, &packet->scene);
            }
        }
    }
}

void engine_shutdown(Engine* engine) {
    if (!engine) return;
    
    render_system_shutdown(&engine->render_system);
    
    if (engine->ui_root) ui_view_free(engine->ui_root);
    if (engine->ui_def) ui_def_free(engine->ui_def);
    
    math_graph_dispose(&engine->graph);
    assets_shutdown(&engine->assets);
    
    if (engine->window) {
        platform_destroy_window(engine->window);
    }
    platform_layer_shutdown();
}