#include "render_thread.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine/render/render_system.h"
#include "foundation/platform/platform.h"

// --- Callbacks ---

static void on_mouse_button(PlatformWindow* window, PlatformMouseButton button, PlatformInputAction action, int mods, void* user_data) {
    RenderSystem* sys = (RenderSystem*)user_data;
    if (!sys) return;
    
    double x, y;
    platform_get_cursor_pos(window, &x, &y);
    
    // In RenderContext, 'coordinates' is CoordinateSystem2D directly
    Vec2 logical = coordinate_screen_to_logical(&sys->render_context.transformer, (Vec2){(float)x, (float)y});
    
    // TODO: Forward input to New UI System
}

static void on_scroll(PlatformWindow* window, double xoff, double yoff, void* user_data) {
    RenderSystem* sys = (RenderSystem*)user_data;
    if (!sys) return;

    double x, y;
    platform_get_cursor_pos(window, &x, &y);
    Vec2 logical = coordinate_screen_to_logical(&sys->render_context.transformer, (Vec2){(float)x, (float)y});

    // TODO: Forward scroll to New UI System
}

static void on_cursor_pos(PlatformWindow* window, double x, double y, void* user_data) {
    RenderSystem* sys = (RenderSystem*)user_data;
    if (!sys) return;

    Vec2 logical = coordinate_screen_to_logical(&sys->render_context.transformer, (Vec2){(float)x, (float)y});

    // TODO: Forward cursor to New UI System
}

static void on_framebuffer_size(PlatformWindow* window, int width, int height, void* user_data) {
    RenderSystem* sys = (RenderSystem*)user_data;
    if (!sys) return;
    render_thread_update_window_state(sys);
}

// --- Internal ---

void render_thread_update_window_state(RenderSystem* sys) {
    if (!sys || !sys->render_context.window) return;

    PlatformWindowSize size = platform_get_framebuffer_size(sys->render_context.window);
    int w = size.width;
    int h = size.height;
    if (w == 0 || h == 0) return;

    PlatformWindowSize logical_size = platform_get_window_size(sys->render_context.window);
    float dpi_scale = (float)w / (float)logical_size.width;
    
    float ui_scale = 1.0f; // Default

    coordinate_system2d_init(&sys->render_context.transformer, dpi_scale, ui_scale, (Vec2){(float)w, (float)h});
}

// --- Init/Shutdown ---

bool runtime_init(RenderSystem* sys) {
    if (!sys) return false;

    if (!platform_layer_init()) {
        fprintf(stderr, "Fatal: Failed to initialize platform layer (GLFW).\n");
        return false;
    }

    // Window Creation
    float target_w = 1024.0f;
    float target_h = 768.0f;

    sys->render_context.window = platform_create_window((int)target_w, (int)target_h, "Graphics Engine");
    if (!sys->render_context.window) {
        fprintf(stderr, "Failed to create window.\n");
        return false;
    }

    platform_set_window_user_pointer(sys->render_context.window, sys);
    platform_set_framebuffer_size_callback(sys->render_context.window, on_framebuffer_size, sys);
    platform_set_mouse_button_callback(sys->render_context.window, on_mouse_button, sys);
    platform_set_scroll_callback(sys->render_context.window, on_scroll, sys);
    platform_set_cursor_pos_callback(sys->render_context.window, on_cursor_pos, sys);

    render_thread_update_window_state(sys);

    return true;
}

void runtime_shutdown(RenderRuntimeContext* ctx) {
    if (!ctx) return;
    if (ctx->window) {
        platform_destroy_window(ctx->window);
        ctx->window = NULL;
    }
    platform_layer_shutdown();
}