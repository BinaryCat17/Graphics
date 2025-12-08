#include "scroll.h"

#include <stdlib.h>
#include <string.h>

typedef struct ScrollArea {
    char* name;
    Rect bounds;
    Rect viewport;
    int has_bounds;
    int has_viewport;
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
    a->has_viewport = 0;
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

    if (w->scroll_static && !a->has_viewport) {
        a->viewport = w->rect;
        a->has_viewport = 1;
    }
}

static ScrollArea* find_area_at_point(ScrollArea* areas, float x, float y) {
    for (ScrollArea* a = areas; a; a = a->next) {
        if (!a->has_bounds) continue;
        Rect viewport = a->has_viewport ? a->viewport : a->bounds;
        if (x >= viewport.x && x <= viewport.x + viewport.w &&
            y >= viewport.y && y <= viewport.y + viewport.h) {
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
        w->show_scrollbar = 0;
        if (!w->scroll_area) continue;
        ScrollArea* a = find_area(ctx->areas, w->scroll_area);
        if (!a) continue;
        float viewport_h = a->has_viewport ? a->viewport.h : a->bounds.h;
        float content_h = a->has_bounds ? a->bounds.h : viewport_h;
        float overflow = content_h - viewport_h;
        if (overflow < 0.0f) overflow = 0.0f;
        if (!w->scroll_static) {
            if (a->offset > overflow) a->offset = overflow;
            if (a->offset < -overflow) a->offset = -overflow;
            w->scroll_offset = a->offset;
        }
        if (w->scroll_static) {
            w->scroll_viewport = viewport_h;
            w->scroll_content = content_h;
            w->show_scrollbar = overflow > 1.0f;
        }
    }
}

void scroll_handle_event(ScrollContext* ctx, Widget* widgets, size_t widget_count, double mouse_x, double mouse_y, double yoff) {
    if (!ctx || !widgets) return;
    ctx->widgets = widgets;
    ctx->widget_count = widget_count;
    ScrollArea* target = find_area_at_point(ctx->areas, (float)mouse_x, (float)mouse_y);
    if (!target) return;

    target->offset += (float)yoff * 24.0f;
    scroll_apply_offsets(ctx, widgets, widget_count);
}

void scroll_rebuild(ScrollContext* ctx, Widget* widgets, size_t widget_count, float offset_scale) {
    if (!ctx) return;
    ScrollArea* old = ctx->areas;
    ctx->areas = NULL;
    ctx->widgets = widgets;
    ctx->widget_count = widget_count;

    for (size_t i = 0; i < widget_count; i++) {
        Widget* w = &widgets[i];
        if (!w->scroll_area) continue;
        ScrollArea* area = ensure_area(&ctx->areas, w->scroll_area);
        if (!area) continue;
        ScrollArea* prev = find_area(old, w->scroll_area);
        if (prev) area->offset = prev->offset * offset_scale;
        add_area_bounds(area, w);
        if (w->scroll_static) area->has_static_anchor = 1;
    }

    free_scroll_areas(old);
    scroll_apply_offsets(ctx, widgets, widget_count);
}

void scroll_free(ScrollContext* ctx) {
    if (!ctx) return;
    free_scroll_areas(ctx->areas);
    free(ctx);
}
