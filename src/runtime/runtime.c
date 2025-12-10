#include "runtime.h"

#include <math.h>
#include <stdio.h>

#include "render/render_service.h"
#include "ui/ui_service.h"

static void on_mouse_button(GLFWwindow* window, int button, int action, int mods) {
    (void)mods;
    AppServices* services = (AppServices*)glfwGetWindowUserPointer(window);
    if (!services) return;
    double mx = 0.0, my = 0.0;
    glfwGetCursorPos(window, &mx, &my);
    Vec2 screen = {(float)(mx * services->transformer.dpi_scale), (float)(my * services->transformer.dpi_scale)};
    Vec2 logical = coordinate_screen_to_logical(&services->transformer, screen);
    ui_handle_mouse_button(services, logical.x, logical.y, button, action);
}

static void on_scroll(GLFWwindow* window, double xoff, double yoff) {
    (void)xoff;
    AppServices* services = (AppServices*)glfwGetWindowUserPointer(window);
    if (!services) return;
    double mx = 0.0, my = 0.0;
    glfwGetCursorPos(window, &mx, &my);
    Vec2 screen = {(float)(mx * services->transformer.dpi_scale), (float)(my * services->transformer.dpi_scale)};
    Vec2 logical = coordinate_screen_to_logical(&services->transformer, screen);
    ui_handle_scroll(services, logical.x, logical.y, yoff);
}

static void on_cursor_pos(GLFWwindow* window, double x, double y) {
    AppServices* services = (AppServices*)glfwGetWindowUserPointer(window);
    if (!services) return;
    Vec2 screen = {(float)(x * services->transformer.dpi_scale), (float)(y * services->transformer.dpi_scale)};
    Vec2 logical = coordinate_screen_to_logical(&services->transformer, screen);
    ui_handle_cursor(services, logical.x, logical.y);
}

void runtime_update_transformer(AppServices* services) {
    if (!services || !services->window) return;
    int win_w = 0, win_h = 0, fb_w = 0, fb_h = 0;
    glfwGetWindowSize(services->window, &win_w, &win_h);
    glfwGetFramebufferSize(services->window, &fb_w, &fb_h);
    float dpi_scale_x = (win_w > 0) ? (float)fb_w / (float)win_w : 1.0f;
    float dpi_scale_y = (win_h > 0) ? (float)fb_h / (float)win_h : 1.0f;
    float dpi_scale = (dpi_scale_x + dpi_scale_y) * 0.5f;
    if (dpi_scale <= 0.0f) dpi_scale = 1.0f;

    coordinate_transformer_init(&services->transformer, dpi_scale, services->ui_scale,
                                (Vec2){(float)fb_w, (float)fb_h});
    render_service_update_transformer(services);
}

static void on_framebuffer_size(GLFWwindow* window, int width, int height) {
    if (width <= 0 || height <= 0) return;
    AppServices* services = (AppServices*)glfwGetWindowUserPointer(window);
    if (!services) return;
    float new_scale = ui_compute_scale(services, (float)width, (float)height);
    ui_refresh_layout(services, new_scale);
    runtime_update_transformer(services);
}

bool runtime_init(AppServices* services) {
    if (!services || !services->layout_root) return false;

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
    float target_w = mode ? mode->width * 0.95f : services->base_w;
    float target_h = mode ? mode->height * 0.95f : services->base_h;
    float ui_scale = ui_compute_scale(services, target_w, target_h);

    if (!ui_prepare_runtime(services, ui_scale)) return false;

    float desired_w = services->layout_root->rect.w + 32.0f;
    float desired_h = services->layout_root->rect.h + 32.0f;
    int window_w = (int)lroundf(fminf(desired_w, target_w));
    int window_h = (int)lroundf(fminf(desired_h, target_h));
    if (window_w < 720) window_w = 720;
    if (window_h < 560) window_h = 560;

    services->window = glfwCreateWindow(window_w, window_h, "vk_gui (Vulkan)", NULL, NULL);
    if (!services->window) {
        fprintf(stderr, "Fatal: glfwCreateWindow\n");
        return false;
    }

    glfwSetWindowUserPointer(services->window, services);
    glfwSetFramebufferSizeCallback(services->window, on_framebuffer_size);
    glfwSetScrollCallback(services->window, on_scroll);
    glfwSetMouseButtonCallback(services->window, on_mouse_button);
    glfwSetCursorPosCallback(services->window, on_cursor_pos);

    runtime_update_transformer(services);
    return true;
}

void runtime_shutdown(AppServices* services) {
    if (!services) return;
    if (services->window) {
        glfwDestroyWindow(services->window);
        services->window = NULL;
    }
    glfwTerminate();
}
