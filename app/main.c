#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "ui_json.h"
#include "vulkan_renderer.h"

static char* read_file_text(const char* path, size_t * out_len) {
    FILE* f = fopen(path, "rb"); if (!f) { fprintf(stderr, "Failed open %s\n", path); return NULL; }
    fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
    char* b = malloc((size_t)len + 1); fread(b, 1, (size_t)len, f); b[len] = 0; if (out_len) *out_len = (size_t)len; fclose(f); return b;
}

static void fatal(const char* msg) {
    fprintf(stderr, "Fatal: %s\n", msg);
    exit(1);
}

int main(int argc, char** argv) {
    if (argc < 4) { fprintf(stderr, "Usage: %s ui.json shader.vert.spv shader.frag.spv\n", argv[0]); return 1; }
    const char* json_path = argv[1];
    const char* vert_spv = argv[2];
    const char* frag_spv = argv[3];

    size_t json_len = 0; char* json_text = read_file_text(json_path, &json_len);
    if (!json_text) return 1;
    Widget* widgets = parse_ui_json(json_text);
    free(json_text);
    if (!widgets) return 1;

    if (!glfwInit()) fatal("glfwInit");
    if (!glfwVulkanSupported()) fatal("glfw Vulkan not supported");
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(1024, 640, "vk_gui (Vulkan)", NULL, NULL);
    if (!window) fatal("glfwCreateWindow");

    if (!vk_renderer_init(window, vert_spv, frag_spv, widgets)) {
        glfwDestroyWindow(window);
        glfwTerminate();
        free_widgets(widgets);
        return 1;
    }

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        vk_renderer_draw_frame();
    }

    vk_renderer_cleanup();
    free_widgets(widgets);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
