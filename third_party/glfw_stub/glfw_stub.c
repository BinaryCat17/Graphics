#include "GLFW/glfw3.h"
#include <stdlib.h>

static const char* empty_extensions[] = { NULL };

int glfwInit(void) {
    return 1;
}

void glfwTerminate(void) {
}

int glfwVulkanSupported(void) {
    return 1;
}

void glfwWindowHint(int hint, int value) {
    (void)hint;
    (void)value;
}

GLFWwindow* glfwCreateWindow(int width, int height, const char* title, GLFWwindow* monitor, GLFWwindow* share) {
    (void)width;
    (void)height;
    (void)title;
    (void)monitor;
    (void)share;
    return (GLFWwindow*)calloc(1, sizeof(GLFWwindow));
}

void glfwDestroyWindow(GLFWwindow* window) {
    free(window);
}

int glfwCreateWindowSurface(VkInstance instance, GLFWwindow* window, const VkAllocationCallbacks* allocator, VkSurfaceKHR* surface) {
    (void)instance;
    (void)window;
    (void)allocator;
    if (surface) {
        *surface = VK_NULL_HANDLE;
    }
    return VK_ERROR_INITIALIZATION_FAILED;
}

const char** glfwGetRequiredInstanceExtensions(uint32_t* count) {
    if (count) {
        *count = 0;
    }
    return empty_extensions;
}

void glfwGetFramebufferSize(GLFWwindow* window, int* width, int* height) {
    (void)window;
    if (width) {
        *width = 0;
    }
    if (height) {
        *height = 0;
    }
}

void glfwWaitEvents(void) {
}

int glfwWindowShouldClose(GLFWwindow* window) {
    (void)window;
    return 1;
}

void glfwPollEvents(void) {
}

