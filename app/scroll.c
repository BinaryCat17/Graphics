#include "scroll.h"

#include <stdlib.h>
#include <string.h>

typedef struct ScrollArea {
    char* name;
    Rect bounds;
    int has_bounds;
    int has_static_anchor;
    float offset;
    struct ScrollArea* next;
} ScrollArea;

struct ScrollContext {
    ScrollArea* areas;
    Widget* widgets;
    size_t widget_count;
};

static void free_scroll_areas(ScrollArea* areas) {
    while (areas) {
        ScrollArea* next = areas->next;
        free(areas->name);
        free(areas);
        areas = next;
    }
}

static ScrollArea* find_area(ScrollArea* areas, const char* name) {
    for (ScrollArea* a = areas; a; a = a->next) {
        if (strcmp(a->name, name) == 0) return a;
    }
    return NULL;
}

static ScrollArea* ensure_area(ScrollArea** areas, const char* name) {
    ScrollArea* a = find_area(*areas, name);
    if (a) return a;
    a = (ScrollArea*)calloc(1, sizeof(ScrollArea));
    if (!a) return NULL;
    a->name = strdup(name);
    a->offset = 0.0f;
    a->has_bounds = 0;
    a->has_static_anchor = 0;
    a->next = *areas;
    *areas = a;
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

static ScrollArea* find_area_at_point(ScrollArea* areas, float x, float y) {
    for (ScrollArea* a = areas; a; a = a->next) {
        if (!a->has_bounds) continue;
        if (x >= a->bounds.x && x <= a->bounds.x + a->bounds.w &&
            y >= a->bounds.y && y <= a->bounds.y + a->bounds.h) {
            return a;
        }
    }
    return NULL;
}

static void build_scroll_areas(ScrollContext* ctx, Widget* widgets, size_t widget_count) {
    free_scroll_areas(ctx->areas);
    ctx->areas = NULL;
    ctx->widgets = widgets;
    ctx->widget_count = widget_count;

    for (size_t i = 0; i < widget_count; i++) {
        Widget* w = &widgets[i];
        w->scroll_offset = 0.0f;
        if (!w->scroll_area) continue;
        ScrollArea* area = ensure_area(&ctx->areas, w->scroll_area);
        if (!area) continue;
        if (w->scroll_static) area->has_static_anchor = 1;
        add_area_bounds(area, w);
    }
}

ScrollContext* scroll_init(Widget* widgets, size_t widget_count) {
    ScrollContext* ctx = (ScrollContext*)calloc(1, sizeof(ScrollContext));
    if (!ctx) return NULL;

    build_scroll_areas(ctx, widgets, widget_count);
    scroll_apply_offsets(ctx, widgets, widget_count);
    return ctx;
}

void scroll_apply_offsets(ScrollContext* ctx, Widget* widgets, size_t widget_count) {
    if (!ctx) return;
    ctx->widgets = widgets;
    ctx->widget_count = widget_count;
    for (size_t i = 0; i < widget_count; i++) {
        Widget* w = &widgets[i];
        w->scroll_offset = 0.0f;
        if (!w->scroll_area) continue;
        ScrollArea* a = find_area(ctx->areas, w->scroll_area);
        if (!a) continue;
        if (w->scroll_static) w->scroll_offset = 0.0f;
        else w->scroll_offset = a->offset;
    }
}

static void on_scroll(GLFWwindow* window, double xoff, double yoff) {
    (void)xoff;
    ScrollContext* ctx = (ScrollContext*)glfwGetWindowUserPointer(window);
    if (!ctx || !ctx->widgets) return;

    double mx = 0.0, my = 0.0;
    glfwGetCursorPos(window, &mx, &my);
    ScrollArea* target = find_area_at_point(ctx->areas, (float)mx, (float)my);
    if (!target) return;

    target->offset += (float)yoff * 24.0f;
    scroll_apply_offsets(ctx, ctx->widgets, ctx->widget_count);
}

void scroll_set_callback(GLFWwindow* window, ScrollContext* ctx) {
    glfwSetWindowUserPointer(window, ctx);
    glfwSetScrollCallback(window, on_scroll);
}

void scroll_free(ScrollContext* ctx) {
    if (!ctx) return;
    free_scroll_areas(ctx->areas);
    free(ctx);
}
