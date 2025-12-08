#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "ui_json.h"
#include "vulkan_renderer.h"

typedef struct ScrollArea {
    char* name;
    Rect bounds;
    int has_bounds;
    int has_static_anchor;
    float offset;
    struct ScrollArea* next;
} ScrollArea;

static ScrollArea* g_scroll_areas = NULL;
static Widget* g_widgets_for_scroll = NULL;

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

static void free_scroll_areas(void) {
    while (g_scroll_areas) {
        ScrollArea* n = g_scroll_areas->next;
        free(g_scroll_areas->name);
        free(g_scroll_areas);
        g_scroll_areas = n;
    }
}

static ScrollArea* find_area(const char* name) {
    for (ScrollArea* a = g_scroll_areas; a; a = a->next) {
        if (strcmp(a->name, name) == 0) return a;
    }
    return NULL;
}

static ScrollArea* ensure_area(const char* name) {
    ScrollArea* a = find_area(name);
    if (a) return a;
    a = (ScrollArea*)calloc(1, sizeof(ScrollArea));
    if (!a) return NULL;
    a->name = strdup(name);
    a->offset = 0.0f;
    a->has_bounds = 0;
    a->has_static_anchor = 0;
    a->next = g_scroll_areas;
    g_scroll_areas = a;
    return a;
}

static void add_area_bounds(ScrollArea* a, const Widget* w) {
    if (!a || !w) return;
    Rect r = w->rect;
    float minx = r.x;
    float miny = r.y;
    float maxx = r.x + r.w;
    float maxy = r.y + r.h;
    if (!a->has_bounds) {
        a->bounds.x = minx;
        a->bounds.y = miny;
        a->bounds.w = r.w;
        a->bounds.h = r.h;
        a->has_bounds = 1;
    } else {
        float old_maxx = a->bounds.x + a->bounds.w;
        float old_maxy = a->bounds.y + a->bounds.h;
        float new_minx = (minx < a->bounds.x) ? minx : a->bounds.x;
        float new_miny = (miny < a->bounds.y) ? miny : a->bounds.y;
        float new_maxx = (maxx > old_maxx) ? maxx : old_maxx;
        float new_maxy = (maxy > old_maxy) ? maxy : old_maxy;
        a->bounds.x = new_minx;
        a->bounds.y = new_miny;
        a->bounds.w = new_maxx - new_minx;
        a->bounds.h = new_maxy - new_miny;
    }
}

static void build_scroll_areas(Widget* widgets) {
    free_scroll_areas();
    for (Widget* w = widgets; w; w = w->next) {
        w->scroll_offset = 0.0f;
        if (!w->scroll_area) continue;
        ScrollArea* area = ensure_area(w->scroll_area);
        if (!area) continue;
        if (w->scroll_static) area->has_static_anchor = 1;
        add_area_bounds(area, w);
    }
}

static void apply_scroll_offsets(Widget* widgets) {
    for (Widget* w = widgets; w; w = w->next) {
        w->scroll_offset = 0.0f;
        if (!w->scroll_area) continue;
        ScrollArea* a = find_area(w->scroll_area);
        if (!a) continue;
        if (w->scroll_static) w->scroll_offset = 0.0f;
        else w->scroll_offset = a->offset;
    }
}

static ScrollArea* find_area_at_point(float x, float y) {
    for (ScrollArea* a = g_scroll_areas; a; a = a->next) {
        if (!a->has_bounds) continue;
        if (x >= a->bounds.x && x <= a->bounds.x + a->bounds.w &&
            y >= a->bounds.y && y <= a->bounds.y + a->bounds.h) {
            return a;
        }
    }
    return NULL;
}

static void on_scroll(GLFWwindow* window, double xoff, double yoff) {
    (void)xoff;
    if (!g_widgets_for_scroll) return;
    double mx = 0.0, my = 0.0;
    glfwGetCursorPos(window, &mx, &my);
    ScrollArea* target = find_area_at_point((float)mx, (float)my);
    if (!target) return;
    target->offset += (float)yoff * 24.0f;
    apply_scroll_offsets(g_widgets_for_scroll);
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
    g_widgets_for_scroll = widgets;
    build_scroll_areas(widgets);
    apply_scroll_offsets(widgets);
    glfwSetScrollCallback(window, on_scroll);

    if (!vk_renderer_init(window, vert_spv, frag_spv, font_path, widgets)) {
        glfwDestroyWindow(window);
        glfwTerminate();
        free_model(model);
        free_styles(styles);
        free_widgets(widgets);
        free_scroll_areas();
        free(model_path); free(layout_path); free(styles_path); free(vert_spv); free(frag_spv); free(font_path); return 1;
    }

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        update_widget_bindings(widgets, model);
        apply_scroll_offsets(widgets);
        vk_renderer_draw_frame();
    }

    vk_renderer_cleanup();
    save_model(model);
    free_model(model);
    free_styles(styles);
    free_widgets(widgets);
    free_scroll_areas();
    free(model_path); free(layout_path); free(styles_path); free(vert_spv); free(frag_spv); free(font_path);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
