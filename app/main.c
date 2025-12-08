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

static char* join_path(const char* dir, const char* leaf) {
    if (!dir || !leaf) return NULL;
    size_t dir_len = strlen(dir);
    while (dir_len > 0 && dir[dir_len - 1] == '/') dir_len--;
    size_t leaf_len = strlen(leaf);
    size_t total = dir_len + 1 + leaf_len + 1;
    char* out = (char*)malloc(total);
    if (!out) return NULL;
    memcpy(out, dir, dir_len);
    out[dir_len] = '/';
    memcpy(out + dir_len + 1, leaf, leaf_len);
    out[total - 1] = 0;
    return out;
}

static void fatal(const char* msg) {
    fprintf(stderr, "Fatal: %s\n", msg);
    exit(1);
}

int main(int argc, char** argv) {
    const char* assets_dir = argc >= 2 ? argv[1] : "assets";
    char* model_path = join_path(assets_dir, "model.json");
    char* layout_path = join_path(assets_dir, "layout.json");
    char* styles_path = join_path(assets_dir, "styles.json");
    char* vert_spv = join_path(assets_dir, "shaders/shader.vert.spv");
    char* frag_spv = join_path(assets_dir, "shaders/shader.frag.spv");
    char* font_path = join_path(assets_dir, "font.ttf");
    if (!model_path || !layout_path || !styles_path || !vert_spv || !frag_spv || !font_path) {
        fprintf(stderr, "Fatal: failed to compose asset paths for directory '%s'\n", assets_dir);
        free(model_path); free(layout_path); free(styles_path); free(vert_spv); free(frag_spv); free(font_path);
        return 1;
    }

    size_t model_len = 0, layout_len = 0, styles_len = 0;
    char* model_text = read_file_text(model_path, &model_len);
    char* layout_text = read_file_text(layout_path, &layout_len);
    char* styles_text = read_file_text(styles_path, &styles_len);
    if (!model_text || !layout_text || !styles_text) { free(model_text); free(layout_text); free(styles_text); free(model_path); free(layout_path); free(styles_path); free(vert_spv); free(frag_spv); free(font_path); return 1; }

    Model* model = parse_model_json(model_text, model_path);
    Style* styles = parse_styles_json(styles_text);
    Widget* widgets = parse_layout_json(layout_text, model, styles);
    free(model_text); free(layout_text); free(styles_text);
    if (!widgets) { free_model(model); free_styles(styles); free(model_path); free(layout_path); free(styles_path); free(vert_spv); free(frag_spv); free(font_path); return 1; }
    update_widget_bindings(widgets, model);

    if (!glfwInit()) fatal("glfwInit");
    if (!glfwVulkanSupported()) fatal("glfw Vulkan not supported");
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(1024, 640, "vk_gui (Vulkan)", NULL, NULL);
    if (!window) fatal("glfwCreateWindow");

    if (!vk_renderer_init(window, vert_spv, frag_spv, font_path, widgets)) {
        glfwDestroyWindow(window);
        glfwTerminate();
        free_model(model);
        free_styles(styles);
        free_widgets(widgets);
        free(model_path); free(layout_path); free(styles_path); free(vert_spv); free(frag_spv); free(font_path); return 1;
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
    free(model_path); free(layout_path); free(styles_path); free(vert_spv); free(frag_spv); free(font_path);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
