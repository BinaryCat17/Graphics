#include "engine/render/render_thread.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#include "foundation/platform/platform.h"
#include "engine/render/render_system.h"
#include "engine/ui/ui_service.h"

static bool get_logical_cursor(PlatformWindow* window, double* x, double* y, Vec2* logical_out) {
    RenderSystem* sys = NULL;
    double mx = 0.0, my = 0.0;
    Vec2 screen = {0};
    if (!window || !logical_out) return false;

    sys = (RenderSystem*)platform_get_window_user_pointer(window);
    if (!sys) return false;

    if (x && y) {
        mx = *x;
        my = *y;
    } else {
        platform_get_cursor_pos(window, &mx, &my);
    }

    screen = (Vec2){(float)(mx * sys->render_context.transformer.dpi_scale),
                    (float)(my * sys->render_context.transformer.dpi_scale)};
    *logical_out = coordinate_screen_to_logical(&sys->render_context.transformer, screen);

    return true;
}

static void on_mouse_button(PlatformWindow* window, PlatformMouseButton button, PlatformInputAction action, int mods,
                            void* user_data) {
    RenderSystem* sys = (RenderSystem*)user_data;
    Vec2 logical = {0};
    (void)mods;
    if (!get_logical_cursor(window, NULL, NULL, &logical)) return;

    if (sys->ui) {
        ui_system_handle_mouse(sys->ui, logical.x, logical.y, (int)button, (int)action);
    }
}

static void on_scroll(PlatformWindow* window, double xoff, double yoff, void* user_data) {
    RenderSystem* sys = (RenderSystem*)user_data;
    Vec2 logical = {0};
    (void)xoff;
    if (!get_logical_cursor(window, NULL, NULL, &logical)) return;

    if (sys->ui) {
        ui_system_handle_scroll(sys->ui, logical.x, logical.y, yoff);
    }
}

static void on_cursor_pos(PlatformWindow* window, double x, double y, void* user_data) {
    RenderSystem* sys = (RenderSystem*)user_data;
    Vec2 logical = {0};
    if (!get_logical_cursor(window, &x, &y, &logical)) return;

    if (sys->ui) {
        ui_system_handle_cursor(sys->ui, logical.x, logical.y);
    }
}

static void on_framebuffer_size(PlatformWindow* window, int width, int height, void* user_data) {
    RenderSystem* sys = (RenderSystem*)user_data;
    PlatformWindowSize logical_size = {0};
    (void)width;
    (void)height;

    if (!sys || !sys->ui) return;

    logical_size = platform_get_window_size(window);
    if (logical_size.width <= 0 || logical_size.height <= 0) return;

    float new_scale = ui_compute_scale(sys->ui, (float)logical_size.width, (float)logical_size.height);
    ui_system_refresh_layout(sys->ui, new_scale);
    
    // Update transformer
    RenderRuntimeContext* render = &sys->render_context;
    PlatformWindowSize framebuffer_size = platform_get_framebuffer_size(render->window);
    
    float dpi_scale = 1.0f; // Simplified DPI
    
    float ui_scale = sys->ui->ui_scale;
    coordinate_system2d_init(&render->transformer, dpi_scale, ui_scale,
                             (Vec2){(float)framebuffer_size.width, (float)framebuffer_size.height});
                             
    render_system_update_transformer(sys);
}

bool runtime_init(RenderSystem* sys) {
    if (!sys) return false;
    RenderRuntimeContext* context = &sys->render_context;

    if (!platform_layer_init()) {
        fprintf(stderr, "Fatal: platform init\n");
        return false;
    }
    if (!platform_vulkan_supported()) {
        fprintf(stderr, "Fatal: Vulkan not supported\n");
        return false;
    }

    int window_w = 1024;
    int window_h = 768;
    
    // Hint from UI?
    if (sys->ui) {
        float target_w = sys->ui->base_w;
        float target_h = sys->ui->base_h;
        // ... logic ...
        window_w = (int)target_w;
        window_h = (int)target_h;
    }

    context->window = platform_create_window(window_w, window_h, "vk_gui (Vulkan)");
    if (!context->window) {
        fprintf(stderr, "Fatal: platform create window\n");
        return false;
    }

    platform_set_window_user_pointer(context->window, sys);
    platform_set_framebuffer_size_callback(context->window, on_framebuffer_size, sys);
    platform_set_scroll_callback(context->window, on_scroll, sys);
    platform_set_mouse_button_callback(context->window, on_mouse_button, sys);
    platform_set_cursor_pos_callback(context->window, on_cursor_pos, sys);

    // Initial Transformer
    if (sys->ui) {
        PlatformWindowSize logical_size = platform_get_window_size(context->window);
        float ui_scale = ui_compute_scale(sys->ui, (float)logical_size.width, (float)logical_size.height);
        ui_system_prepare_runtime(sys->ui, ui_scale); 
        // Main.c calls prepare_runtime separately.
    }

    return true;
}

void runtime_shutdown(RenderRuntimeContext* context) {
    if (!context) return;
    if (context->window) {
        platform_destroy_window(context->window);
        context->window = NULL;
    }
    platform_layer_shutdown();
}
