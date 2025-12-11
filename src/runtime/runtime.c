#include "runtime.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#include "render_runtime_service.h"
#include "ui_service.h"

static bool get_logical_cursor(GLFWwindow* window, double* x, double* y, Vec2* logical_out) {
    AppServices* services = NULL;
    double mx = 0.0, my = 0.0;
    Vec2 screen = {0};
    if (!window || !logical_out) return false;

    services = (AppServices*)glfwGetWindowUserPointer(window);
    if (!services || !services->render.window) return false;

    if (x && y) {
        mx = *x;
        my = *y;
    } else {
        glfwGetCursorPos(window, &mx, &my);
    }

    screen = (Vec2){(float)(mx * services->render.transformer.dpi_scale),
                    (float)(my * services->render.transformer.dpi_scale)};
    *logical_out = coordinate_screen_to_logical(&services->render.transformer, screen);

    return true;
}

static void on_mouse_button(GLFWwindow* window, int button, int action, int mods) {
    AppServices* services = NULL;
    Vec2 logical = {0};
    (void)mods;
    if (!get_logical_cursor(window, NULL, NULL, &logical)) return;

    services = (AppServices*)glfwGetWindowUserPointer(window);
    ui_handle_mouse_button(&services->ui, logical.x, logical.y, button, action);
}

static void on_scroll(GLFWwindow* window, double xoff, double yoff) {
    AppServices* services = NULL;
    Vec2 logical = {0};
    (void)xoff;
    if (!get_logical_cursor(window, NULL, NULL, &logical)) return;

    services = (AppServices*)glfwGetWindowUserPointer(window);
    ui_handle_scroll(&services->ui, logical.x, logical.y, yoff);
}

static void on_cursor_pos(GLFWwindow* window, double x, double y) {
    AppServices* services = NULL;
    Vec2 logical = {0};
    if (!get_logical_cursor(window, &x, &y, &logical)) return;

    services = (AppServices*)glfwGetWindowUserPointer(window);
    ui_handle_cursor(&services->ui, logical.x, logical.y);
}

void runtime_update_transformer(AppServices* services) {
    if (!services) return;
    RenderRuntimeContext* render = &services->render;
    if (!render->window) return;
    int win_w = 0, win_h = 0, fb_w = 0, fb_h = 0;
    glfwGetWindowSize(render->window, &win_w, &win_h);
    glfwGetFramebufferSize(render->window, &fb_w, &fb_h);
    float dpi_scale_x = (win_w > 0) ? (float)fb_w / (float)win_w : 1.0f;
    float dpi_scale_y = (win_h > 0) ? (float)fb_h / (float)win_h : 1.0f;
    float dpi_scale = (dpi_scale_x + dpi_scale_y) * 0.5f;
    if (dpi_scale <= 0.0f) dpi_scale = 1.0f;

    float ui_scale = services->ui.ui_scale;
    coordinate_system2d_init(&render->transformer, dpi_scale, ui_scale, (Vec2){(float)fb_w, (float)fb_h});
    render_runtime_service_update_transformer(services->render_runtime_context, render);
}

static void on_framebuffer_size(GLFWwindow* window, int width, int height) {
    AppServices* services = NULL;
    if (width <= 0 || height <= 0) return;
    services = (AppServices*)glfwGetWindowUserPointer(window);
    if (!services) return;
    float new_scale = ui_compute_scale(&services->ui, (float)width, (float)height);
    ui_refresh_layout(&services->ui, new_scale);
    runtime_update_transformer(services);
}

bool runtime_init(AppServices* services) {
    if (!services || !services->ui.layout_root) return false;

    if (!glfwInit()) {
        fprintf(stderr, "Fatal: glfwInit\n");
        return false;
    }
    if (!glfwVulkanSupported()) {
        fprintf(stderr, "Fatal: glfw Vulkan not supported\n");
        return false;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = monitor ? glfwGetVideoMode(monitor) : NULL;
    float target_w = mode ? mode->width * 0.95f : services->ui.base_w;
    float target_h = mode ? mode->height * 0.95f : services->ui.base_h;
    float ui_scale = ui_compute_scale(&services->ui, target_w, target_h);

    if (!ui_prepare_runtime(&services->ui, &services->core, ui_scale, &services->state_manager, services->ui_type_id))
        return false;

    float desired_w = services->ui.layout_root->rect.w + 32.0f;
    float desired_h = services->ui.layout_root->rect.h + 32.0f;
    int window_w = (int)lroundf(fminf(desired_w, target_w));
    int window_h = (int)lroundf(fminf(desired_h, target_h));
    if (window_w < 720) window_w = 720;
    if (window_h < 560) window_h = 560;

    services->render.window = glfwCreateWindow(window_w, window_h, "vk_gui (Vulkan)", NULL, NULL);
    if (!services->render.window) {
        fprintf(stderr, "Fatal: glfwCreateWindow\n");
        return false;
    }

    glfwSetWindowUserPointer(services->render.window, services);
    glfwSetFramebufferSizeCallback(services->render.window, on_framebuffer_size);
    glfwSetScrollCallback(services->render.window, on_scroll);
    glfwSetMouseButtonCallback(services->render.window, on_mouse_button);
    glfwSetCursorPosCallback(services->render.window, on_cursor_pos);

    runtime_update_transformer(services);
    return true;
}

void runtime_shutdown(AppServices* services) {
    if (!services) return;
    if (services->render.window) {
        glfwDestroyWindow(services->render.window);
        services->render.window = NULL;
    }
    glfwTerminate();
}
