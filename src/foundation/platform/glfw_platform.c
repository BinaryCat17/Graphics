#include "foundation/platform/glfw_platform.h"
#include "foundation/logger/logger.h"
#include <stdio.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdlib.h>

typedef struct PlatformWindowCallbacks {
    PlatformFramebufferSizeCallback framebuffer_size;
    PlatformScrollCallback scroll;
    PlatformMouseButtonCallback mouse_button;
    PlatformCursorPosCallback cursor_pos;
    void* user_data;
} PlatformWindowCallbacks;

struct PlatformWindow {
    GLFWwindow* handle;
    PlatformWindowCallbacks callbacks;
    void* user_pointer;
};

static void glfw_error_callback(int error, const char* description) {
    LOG_ERROR("GLFW Error %d: %s", error, description);
}

static void on_glfw_framebuffer_size(GLFWwindow* window, int width, int height) {
    PlatformWindow* platform_window = (PlatformWindow*)glfwGetWindowUserPointer(window);
    if (!platform_window || !platform_window->callbacks.framebuffer_size) return;
    platform_window->callbacks.framebuffer_size(platform_window, width, height, platform_window->callbacks.user_data);
}

static void on_glfw_scroll(GLFWwindow* window, double xoff, double yoff) {
    PlatformWindow* platform_window = (PlatformWindow*)glfwGetWindowUserPointer(window);
    if (!platform_window || !platform_window->callbacks.scroll) return;
    platform_window->callbacks.scroll(platform_window, xoff, yoff, platform_window->callbacks.user_data);
}

static void on_glfw_mouse_button(GLFWwindow* window, int button, int action, int mods) {
    PlatformWindow* platform_window = (PlatformWindow*)glfwGetWindowUserPointer(window);
    if (!platform_window || !platform_window->callbacks.mouse_button) return;
    platform_window->callbacks.mouse_button(platform_window, (PlatformMouseButton)button, (PlatformInputAction)action, mods,
                                           platform_window->callbacks.user_data);
}

static void on_glfw_cursor_pos(GLFWwindow* window, double x, double y) {
    PlatformWindow* platform_window = (PlatformWindow*)glfwGetWindowUserPointer(window);
    if (!platform_window || !platform_window->callbacks.cursor_pos) return;
    platform_window->callbacks.cursor_pos(platform_window, x, y, platform_window->callbacks.user_data);
}

bool platform_layer_init(void) {
    glfwSetErrorCallback(glfw_error_callback); // Set the error callback
    if (!glfwInit()) {
        LOG_FATAL("Failed to initialize GLFW");
        return false;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    return true;
}

void platform_layer_shutdown(void) { glfwTerminate(); }

bool platform_vulkan_supported(void) { return glfwVulkanSupported() == GLFW_TRUE; }

PlatformWindow* platform_create_window(int width, int height, const char* title) {
    PlatformWindow* window = (PlatformWindow*)calloc(1, sizeof(PlatformWindow));
    if (!window) return NULL;

    window->handle = glfwCreateWindow(width, height, title, NULL, NULL);
    if (!window->handle) {
        LOG_FATAL("Failed to create GLFW window");
        free(window);
        return NULL;
    }

    glfwSetWindowUserPointer(window->handle, window);
    glfwSetFramebufferSizeCallback(window->handle, on_glfw_framebuffer_size);
    glfwSetScrollCallback(window->handle, on_glfw_scroll);
    glfwSetMouseButtonCallback(window->handle, on_glfw_mouse_button);
    glfwSetCursorPosCallback(window->handle, on_glfw_cursor_pos);
    return window;
}

void platform_destroy_window(PlatformWindow* window) {
    if (!window) return;
    if (window->handle) {
        glfwDestroyWindow(window->handle);
    }
    free(window);
}

void platform_set_window_user_pointer(PlatformWindow* window, void* user_pointer) {
    if (!window) return;
    window->user_pointer = user_pointer;
}

void* platform_get_window_user_pointer(PlatformWindow* window) { return window ? window->user_pointer : NULL; }

PlatformWindowSize platform_get_window_size(PlatformWindow* window) {
    PlatformWindowSize size = {0};
    if (!window || !window->handle) return size;
    glfwGetWindowSize(window->handle, &size.width, &size.height);
    return size;
}

PlatformWindowSize platform_get_framebuffer_size(PlatformWindow* window) {
    PlatformWindowSize size = {0};
    if (!window || !window->handle) return size;
    glfwGetFramebufferSize(window->handle, &size.width, &size.height);
    return size;
}

PlatformDpiScale platform_get_window_dpi(PlatformWindow* window) {
    PlatformDpiScale scale = {.x_scale = 1.0f, .y_scale = 1.0f};
    if (!window || !window->handle) return scale;
    glfwGetWindowContentScale(window->handle, &scale.x_scale, &scale.y_scale);
    return scale;
}

void platform_get_cursor_pos(PlatformWindow* window, double* x, double* y) {
    if (!window || !window->handle) return;
    glfwGetCursorPos(window->handle, x, y);
}

bool platform_get_mouse_button(PlatformWindow* window, int button) {
    if (!window || !window->handle) return false;
    return glfwGetMouseButton(window->handle, button) == GLFW_PRESS;
}

void platform_set_framebuffer_size_callback(PlatformWindow* window, PlatformFramebufferSizeCallback callback, 
                                            void* user_data) {
    if (!window) return;
    window->callbacks.framebuffer_size = callback;
    window->callbacks.user_data = user_data;
}

void platform_set_scroll_callback(PlatformWindow* window, PlatformScrollCallback callback, void* user_data) {
    if (!window) return;
    window->callbacks.scroll = callback;
    window->callbacks.user_data = user_data;
}

void platform_set_mouse_button_callback(PlatformWindow* window, PlatformMouseButtonCallback callback, void* user_data) {
    if (!window) return;
    window->callbacks.mouse_button = callback;
    window->callbacks.user_data = user_data;
}

void platform_set_cursor_pos_callback(PlatformWindow* window, PlatformCursorPosCallback callback, void* user_data) {
    if (!window) return;
    window->callbacks.cursor_pos = callback;
    window->callbacks.user_data = user_data;
}

bool platform_window_should_close(PlatformWindow* window) {
    if (!window || !window->handle) return true;
    return glfwWindowShouldClose(window->handle) == GLFW_TRUE;
}

void platform_set_window_should_close(PlatformWindow* window, bool should_close) {
    if (!window || !window->handle) return;
    glfwSetWindowShouldClose(window->handle, should_close ? GLFW_TRUE : GLFW_FALSE);
}

void platform_poll_events(void) { glfwPollEvents(); }

void platform_wait_events(void) { glfwWaitEvents(); }

double platform_get_time_ms(void) { return glfwGetTime() * 1000.0; }

bool platform_get_required_vulkan_instance_extensions(const char*** names, uint32_t* count) {
    uint32_t ext_count = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&ext_count);
    if (!extensions || ext_count == 0) return false;
    if (names) *names = extensions;
    if (count) *count = ext_count;
    return true;
}

bool platform_create_vulkan_surface(PlatformWindow* window, void* instance, const void* allocation_callbacks,
                                    PlatformSurface* out_surface) {
    if (!window || !window->handle || !instance || !out_surface) return false;
    VkInstance vk_instance = (VkInstance)instance;
    const VkAllocationCallbacks* vk_alloc = (const VkAllocationCallbacks*)allocation_callbacks;

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkResult res = glfwCreateWindowSurface(vk_instance, window->handle, vk_alloc, &surface);
    if (res != VK_SUCCESS) return false;
    out_surface->handle = surface;
    return true;
}

void platform_destroy_vulkan_surface(void* instance, const void* allocation_callbacks, PlatformSurface* surface) {
    if (!instance || !surface || !surface->handle) return;
    VkInstance vk_instance = (VkInstance)instance;
    const VkAllocationCallbacks* vk_alloc = (const VkAllocationCallbacks*)allocation_callbacks;
    vkDestroySurfaceKHR(vk_instance, (VkSurfaceKHR)surface->handle, vk_alloc);
    surface->handle = NULL;
}