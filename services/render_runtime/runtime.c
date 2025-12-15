#include "render_runtime/runtime.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#include "platform/platform.h"
#include "render_runtime/render_runtime_service.h"
#include "ui/ui_service.h"

static bool get_logical_cursor(PlatformWindow* window, double* x, double* y, Vec2* logical_out) {
    RenderRuntimeServiceContext* context = NULL;
    double mx = 0.0, my = 0.0;
    Vec2 screen = {0};
    if (!window || !logical_out) return false;

    context = (RenderRuntimeServiceContext*)platform_get_window_user_pointer(window);
    if (!context || !context->render) return false;

    if (x && y) {
        mx = *x;
        my = *y;
    } else {
        platform_get_cursor_pos(window, &mx, &my);
    }

    screen = (Vec2){(float)(mx * context->render->transformer.dpi_scale),
                    (float)(my * context->render->transformer.dpi_scale)};
    *logical_out = coordinate_screen_to_logical(&context->render->transformer, screen);

    return true;
}

static void on_mouse_button(PlatformWindow* window, PlatformMouseButton button, PlatformInputAction action, int mods,
                            void* user_data) {
    RenderRuntimeServiceContext* context = NULL;
    Vec2 logical = {0};
    (void)mods;
    if (!get_logical_cursor(window, NULL, NULL, &logical)) return;

    context = (RenderRuntimeServiceContext*)user_data;
    if (context->ui) {
        ui_handle_mouse_button(context->ui, logical.x, logical.y, (int)button, (int)action);
    }
}

static void on_scroll(PlatformWindow* window, double xoff, double yoff, void* user_data) {
    RenderRuntimeServiceContext* context = NULL;
    Vec2 logical = {0};
    (void)xoff;
    if (!get_logical_cursor(window, NULL, NULL, &logical)) return;

    context = (RenderRuntimeServiceContext*)user_data;
    if (context->ui) {
        ui_handle_scroll(context->ui, logical.x, logical.y, yoff);
    }
}

static void on_cursor_pos(PlatformWindow* window, double x, double y, void* user_data) {
    RenderRuntimeServiceContext* context = NULL;
    Vec2 logical = {0};
    if (!get_logical_cursor(window, &x, &y, &logical)) return;

    context = (RenderRuntimeServiceContext*)user_data;
    if (context->ui) {
        ui_handle_cursor(context->ui, logical.x, logical.y);
    }
}

void runtime_update_transformer(RenderRuntimeServiceContext* context) {
    if (!context || !context->render || !context->ui) return;
    RenderRuntimeContext* render = context->render;
    if (!render->window) return;
    PlatformWindowSize window_size = platform_get_window_size(render->window);
    PlatformWindowSize framebuffer_size = platform_get_framebuffer_size(render->window);
    float dpi_scale_x = (window_size.width > 0) ? (float)framebuffer_size.width / (float)window_size.width : 1.0f;
    float dpi_scale_y = (window_size.height > 0) ? (float)framebuffer_size.height / (float)window_size.height : 1.0f;
    float dpi_scale = (dpi_scale_x + dpi_scale_y) * 0.5f;
    if (dpi_scale <= 0.0f) {
        PlatformDpiScale dpi_scale_data = platform_get_window_dpi(render->window);
        dpi_scale = (dpi_scale_data.x_scale + dpi_scale_data.y_scale) * 0.5f;
    }
    if (dpi_scale <= 0.0f) dpi_scale = 1.0f;

    float ui_scale = context->ui->ui_scale;
    coordinate_system2d_init(&render->transformer, dpi_scale, ui_scale,
                             (Vec2){(float)framebuffer_size.width, (float)framebuffer_size.height});
    render_runtime_service_update_transformer(context, render);
}

static void on_framebuffer_size(PlatformWindow* window, int width, int height, void* user_data) {
    RenderRuntimeServiceContext* context = NULL;
    PlatformWindowSize logical_size = {0};
    (void)width;
    (void)height;

    context = (RenderRuntimeServiceContext*)user_data;
    if (!context || !context->ui) return;

    logical_size = platform_get_window_size(window);
    if (logical_size.width <= 0 || logical_size.height <= 0) return;

    float new_scale = ui_compute_scale(context->ui, (float)logical_size.width, (float)logical_size.height);
    ui_refresh_layout(context->ui, new_scale);
    runtime_update_transformer(context);
}

bool runtime_init(RenderRuntimeServiceContext* context) {
    if (!context || !context->ui || !context->ui->layout_root || !context->render) return false;

    if (!platform_layer_init()) {
        fprintf(stderr, "Fatal: platform init\n");
        return false;
    }
    if (!platform_vulkan_supported()) {
        fprintf(stderr, "Fatal: Vulkan not supported\n");
        return false;
    }

    float target_w = context->ui->base_w;
    float target_h = context->ui->base_h;

    float desired_w = context->ui->layout_root->rect.w + 32.0f;
    float desired_h = context->ui->layout_root->rect.h + 32.0f;
    int window_w = (int)lroundf(fminf(desired_w, target_w));
    int window_h = (int)lroundf(fminf(desired_h, target_h));
    if (window_w < 720) window_w = 720;
    if (window_h < 560) window_h = 560;

    context->render->window = platform_create_window(window_w, window_h, "vk_gui (Vulkan)");
    if (!context->render->window) {
        fprintf(stderr, "Fatal: platform create window\n");
        return false;
    }

    platform_set_window_user_pointer(context->render->window, context);
    platform_set_framebuffer_size_callback(context->render->window, on_framebuffer_size, context);
    platform_set_scroll_callback(context->render->window, on_scroll, context);
    platform_set_mouse_button_callback(context->render->window, on_mouse_button, context);
    platform_set_cursor_pos_callback(context->render->window, on_cursor_pos, context);

    PlatformWindowSize logical_size = platform_get_window_size(context->render->window);
    float ui_scale = ui_compute_scale(context->ui, (float)logical_size.width, (float)logical_size.height);

    if (!ui_prepare_runtime(context->ui, ui_scale, context->state_manager, context->ui_type_id))
        return false;

    runtime_update_transformer(context);
    return true;
}

void runtime_shutdown(RenderRuntimeServiceContext* context) {
    if (!context || !context->render) return;
    if (context->render->window) {
        platform_destroy_window(context->render->window);
        context->render->window = NULL;
    }
    platform_layer_shutdown();
}
