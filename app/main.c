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
    if (argc < 6) { fprintf(stderr, "Usage: %s model.json layout.json styles.json shader.vert.spv shader.frag.spv\n", argv[0]); return 1; }
    const char* model_path = argv[1];
    const char* layout_path = argv[2];
    const char* styles_path = argv[3];
    const char* vert_spv = argv[4];
    const char* frag_spv = argv[5];

    size_t model_len = 0, layout_len = 0, styles_len = 0;
    char* model_text = read_file_text(model_path, &model_len);
    char* layout_text = read_file_text(layout_path, &layout_len);
    char* styles_text = read_file_text(styles_path, &styles_len);
    if (!model_text || !layout_text || !styles_text) { free(model_text); free(layout_text); free(styles_text); return 1; }

    Model* model = parse_model_json(model_text, model_path);
    Style* styles = parse_styles_json(styles_text);
    Widget* widgets = parse_layout_json(layout_text, model, styles);
    free(model_text); free(layout_text); free(styles_text);
    if (!widgets) { free_model(model); free_styles(styles); return 1; }
    update_widget_bindings(widgets, model);

    if (!glfwInit()) fatal("glfwInit");
    if (!glfwVulkanSupported()) fatal("glfw Vulkan not supported");
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(1024, 640, "vk_gui (Vulkan)", NULL, NULL);
    if (!window) fatal("glfwCreateWindow");

    if (!vk_renderer_init(window, vert_spv, frag_spv, widgets)) {
        glfwDestroyWindow(window);
        glfwTerminate();
        free_model(model);
        free_styles(styles);
        free_widgets(widgets);
        return 1;
    }

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        update_widget_bindings(widgets, model);
        vk_renderer_draw_frame();
    }

    vk_renderer_cleanup();
    save_model(model);
    free_model(model);
    free_styles(styles);
    free_widgets(widgets);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
