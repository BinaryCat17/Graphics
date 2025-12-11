#include "ui/scroll.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct ScrollArea {
    char* name;
    Rect bounds;
    Rect viewport;
    int has_bounds;
    int has_viewport;
    int has_static_anchor;
    int has_render_info;
    float offset;
    int has_clip;
    Rect clip;
    int z_index;
    size_t render_index;
    struct ScrollArea* next;
} ScrollArea;

struct ScrollContext {
    ScrollArea* areas;
    Widget* widgets;
    size_t widget_count;
    ScrollArea* dragging_area;
    Rect drag_track;
    float drag_thumb_h;
    float drag_grab_offset;
    float drag_max_offset;
    RenderNode* render_root;
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
    a->has_render_info = 0;
    a->has_clip = 0;
    a->z_index = 0;
    a->render_index = 0;
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

    if (w->scroll_static) {
        float new_area = w->rect.w * w->rect.h;
        float old_area = a->has_viewport ? a->viewport.w * a->viewport.h : -1.0f;
        if (!a->has_viewport || new_area > old_area) {
            a->viewport = w->rect;
            a->has_viewport = 1;
        }
    }
}

static Widget* find_scrollbar_widget(Widget* widgets, size_t widget_count, const ScrollArea* area) {
    if (!area) return NULL;
    for (size_t i = 0; i < widget_count; i++) {
        Widget* w = &widgets[i];
        if (!w->scroll_area || !w->scroll_static) continue;
        if (strcmp(w->scroll_area, area->name) == 0) return w;
    }
    return NULL;
}

static float clamp_scroll_offset(float offset, float max_offset) {
    if (offset < 0.0f) return 0.0f;
    if (offset > max_offset) return max_offset;
    return offset;
}

static int rect_intersect(const Rect* a, const Rect* b, Rect* out) {
    if (!a || !b || !out) return 0;
    float x0 = fmaxf(a->x, b->x);
    float y0 = fmaxf(a->y, b->y);
    float x1 = fminf(a->x + a->w, b->x + b->w);
    float y1 = fminf(a->y + a->h, b->y + b->h);
    if (x1 <= x0 || y1 <= y0) return 0;
    out->x = x0;
    out->y = y0;
    out->w = x1 - x0;
    out->h = y1 - y0;
    return 1;
}

static int compute_scrollbar_geometry(const Widget* w, Rect* out_track, Rect* out_thumb, float* out_max_offset) {
    if (!w || !w->scrollbar_enabled || !w->show_scrollbar || w->scroll_viewport <= 0.0f) return 0;
    float max_offset = w->scroll_content - w->scroll_viewport;
    if (max_offset <= 1.0f) return 0;
    Rect widget_rect = { w->rect.x, w->rect.y + (w->scroll_static ? 0.0f : w->scroll_offset), w->rect.w, w->rect.h };
    Rect inner_rect = widget_rect;
    if (w->border_thickness > 0.0f) {
        inner_rect.x += w->border_thickness;
        inner_rect.y += w->border_thickness;
        inner_rect.w -= w->border_thickness * 2.0f;
        inner_rect.h -= w->border_thickness * 2.0f;
        if (inner_rect.w < 0.0f) inner_rect.w = 0.0f;
        if (inner_rect.h < 0.0f) inner_rect.h = 0.0f;
    }
    float track_w = w->scrollbar_width > 0.0f ? w->scrollbar_width : fmaxf(4.0f, inner_rect.w * 0.02f);
    float track_h = inner_rect.h - w->padding * 2.0f;
    if (track_h <= 0.0f) return 0;
    float track_x = inner_rect.x + inner_rect.w - track_w - w->padding * 0.5f;
    float track_y = inner_rect.y + w->padding;
    float thumb_ratio = w->scroll_viewport / w->scroll_content;
    float thumb_h = fmaxf(track_h * thumb_ratio, 12.0f);
    float clamped_offset = clamp_scroll_offset(w->scroll_offset, max_offset);
    float offset_t = (max_offset != 0.0f) ? (clamped_offset / max_offset) : 0.0f;
    float thumb_y = track_y + offset_t * (track_h - thumb_h);
    if (out_track) *out_track = (Rect){ track_x, track_y, track_w, track_h };
    if (out_thumb) *out_thumb = (Rect){ track_x, thumb_y, track_w, thumb_h };
    if (out_max_offset) *out_max_offset = max_offset;
    return 1;
}

static void clear_area_render_info(ScrollArea* areas) {
    for (ScrollArea* a = areas; a; a = a->next) {
        a->has_render_info = 0;
        a->has_clip = 0;
        a->z_index = 0;
        a->render_index = 0;
    }
}

static int point_in_rect(const Rect* r, float x, float y) {
    if (!r) return 0;
    return x >= r->x && x <= r->x + r->w && y >= r->y && y <= r->y + r->h;
}

static int area_contains_point(const ScrollArea* area, const Rect* clip, float x, float y) {
    if (!area || !area->has_bounds) return 0;
    Rect viewport = area->has_viewport ? area->viewport : area->bounds;
    if (area->has_viewport && area->has_bounds && !area->has_static_anchor) {
        float viewport_area = viewport.w * viewport.h;
        float bounds_area = area->bounds.w * area->bounds.h;
        if (viewport_area < bounds_area * 0.5f) {
            viewport = area->bounds;
        }
    }
    if (clip) {
        Rect clipped = {0};
        if (!rect_intersect(&viewport, clip, &clipped)) return 0;
        viewport = clipped;
    }
    return point_in_rect(&viewport, x, y);
}

static void update_area_render_info(ScrollArea* area, const RenderNode* node) {
    if (!area || !node) return;
    area->has_render_info = 1;
    area->z_index = node->z_index;
    area->render_index = node->render_index;
    area->has_clip = node->has_clip;
    if (node->has_clip) area->clip = node->clip;
}

static void collect_area_render_info(ScrollContext* ctx, const RenderNode* node) {
    if (!ctx || !node) return;
    if (node->widget && node->widget->scroll_area) {
        ScrollArea* area = find_area(ctx->areas, node->widget->scroll_area);
        if (area && (node->widget->scroll_static || !area->has_render_info)) {
            update_area_render_info(area, node);
        }
    }
    for (size_t i = 0; i < node->child_count; ++i) collect_area_render_info(ctx, &node->children[i]);
}

static const RenderNode* top_render_node_at_point(const RenderNode* node, float x, float y, const RenderNode* best) {
    if (!node) return best;
    if (node->widget) {
        const Rect* region = node->has_clip ? &node->clip : &node->rect;
        if (region && region->w > 0.0f && region->h > 0.0f && point_in_rect(region, x, y)) {
            if (!best || node->z_index > best->z_index ||
                (node->z_index == best->z_index && node->render_index > best->render_index)) {
                best = node;
            }
        }
    }
    for (size_t i = 0; i < node->child_count; ++i) {
        best = top_render_node_at_point(&node->children[i], x, y, best);
    }
    return best;
}

static ScrollArea* find_area_by_order(ScrollArea* areas, float x, float y) {
    ScrollArea* best = NULL;
    for (ScrollArea* a = areas; a; a = a->next) {
        const Rect* clip = a->has_clip ? &a->clip : NULL;
        if (!area_contains_point(a, clip, x, y)) continue;
        if (!best || a->z_index > best->z_index ||
            (a->z_index == best->z_index && a->render_index > best->render_index)) {
            best = a;
        }
    }
    return best;
}

static ScrollArea* find_scroll_target(ScrollContext* ctx, float x, float y) {
    if (ctx && ctx->render_root) {
        const RenderNode* hit = top_render_node_at_point(ctx->render_root, x, y, NULL);
        if (!hit || !hit->widget) return NULL;
        if (!hit->widget->scroll_area) return NULL;
        ScrollArea* area = find_area(ctx->areas, hit->widget->scroll_area);
        if (area && area_contains_point(area, hit->has_clip ? &hit->clip : NULL, x, y)) return area;
        return NULL;
    }
    return find_area_by_order(ctx ? ctx->areas : NULL, x, y);
}

static float offset_from_cursor(const Rect* track, float thumb_h, float max_offset, float mouse_y, float grab_offset) {
    if (!track || max_offset <= 0.0f) return 0.0f;
    float range = track->h - thumb_h;
    if (range <= 0.0f) return 0.0f;
    float thumb_y = mouse_y - grab_offset;
    float min_y = track->y;
    float max_y = track->y + range;
    if (thumb_y < min_y) thumb_y = min_y;
    if (thumb_y > max_y) thumb_y = max_y;
    float offset_t = (thumb_y - track->y) / range;
    float offset = offset_t * max_offset;
    return clamp_scroll_offset(offset, max_offset);
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

    clear_area_render_info(ctx->areas);
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
        Rect viewport = a->has_viewport ? a->viewport : a->bounds;
        if (a->has_viewport && a->has_bounds && !a->has_static_anchor) {
            float viewport_area = viewport.w * viewport.h;
            float bounds_area = a->bounds.w * a->bounds.h;
            if (viewport_area < bounds_area * 0.5f) {
                viewport = a->bounds;
            }
        }
        if (w->border_thickness > 0.0f) {
            viewport.x += w->border_thickness;
            viewport.y += w->border_thickness;
            viewport.w -= w->border_thickness * 2.0f;
            viewport.h -= w->border_thickness * 2.0f;
            if (viewport.w < 0.0f) viewport.w = 0.0f;
            if (viewport.h < 0.0f) viewport.h = 0.0f;
        }
        float viewport_h = viewport.h;
        float content_h = a->has_bounds ? a->bounds.h : viewport_h;
        float overflow = content_h - viewport_h;
        if (overflow < 0.0f) overflow = 0.0f;
        a->offset = clamp_scroll_offset(a->offset, overflow);
        w->scroll_offset = a->offset;
        if (w->scroll_static) {
            w->scroll_viewport = viewport_h;
            w->scroll_content = content_h;
            w->show_scrollbar = w->scrollbar_enabled && overflow > 1.0f;
        }
        if (a->has_viewport || a->has_bounds) {
            w->has_clip = 1;
            w->clip = viewport;
        }
    }
}

void scroll_handle_event(ScrollContext* ctx, Widget* widgets, size_t widget_count, double mouse_x, double mouse_y, double yoff) {
    if (!ctx || !widgets) return;
    ctx->widgets = widgets;
    ctx->widget_count = widget_count;
    ScrollArea* target = find_scroll_target(ctx, (float)mouse_x, (float)mouse_y);
    if (!target) return;

    target->offset -= (float)yoff * 24.0f;
    scroll_apply_offsets(ctx, widgets, widget_count);
}

int scroll_handle_mouse_button(ScrollContext* ctx, Widget* widgets, size_t widget_count, double mouse_x, double mouse_y, int pressed) {
    if (!ctx || !widgets) return 0;
    ctx->widgets = widgets;
    ctx->widget_count = widget_count;
    int consumed = 0;

    if (!pressed) {
        if (ctx->dragging_area) consumed = 1;
        ctx->dragging_area = NULL;
        return consumed;
    }

    ScrollArea* target = find_scroll_target(ctx, (float)mouse_x, (float)mouse_y);
    if (!target) return 0;

    for (size_t i = 0; i < widget_count; i++) {
        Widget* w = &widgets[i];
        if (!w->scroll_area) continue;
        ScrollArea* area = find_area(ctx->areas, w->scroll_area);
        if (!area || area != target) continue;
        Rect track = {0};
        Rect thumb = {0};
        float max_offset = 0.0f;
        if (!compute_scrollbar_geometry(w, &track, &thumb, &max_offset)) continue;
        if (mouse_x >= track.x && mouse_x <= track.x + track.w && mouse_y >= track.y && mouse_y <= track.y + track.h) {
            ctx->dragging_area = area;
            ctx->drag_track = track;
            ctx->drag_thumb_h = thumb.h;
            ctx->drag_max_offset = max_offset;
            ctx->drag_grab_offset = (float)(mouse_y - thumb.y);
            consumed = 1;
            if (!(mouse_y >= thumb.y && mouse_y <= thumb.y + thumb.h)) {
                ctx->drag_grab_offset = thumb.h * 0.5f;
                area->offset = offset_from_cursor(&track, thumb.h, max_offset, (float)mouse_y, ctx->drag_grab_offset);
                scroll_apply_offsets(ctx, widgets, widget_count);
            }
            break;
        }
    }
    return consumed;
}

void scroll_handle_cursor(ScrollContext* ctx, Widget* widgets, size_t widget_count, double mouse_x, double mouse_y) {
    if (!ctx || !ctx->dragging_area || !widgets) return;
    ctx->widgets = widgets;
    ctx->widget_count = widget_count;
    Widget* anchor = find_scrollbar_widget(widgets, widget_count, ctx->dragging_area);
    if (!anchor) { ctx->dragging_area = NULL; return; }

    Rect track = {0};
    Rect thumb = {0};
    float max_offset = 0.0f;
    if (!compute_scrollbar_geometry(anchor, &track, &thumb, &max_offset)) { ctx->dragging_area = NULL; return; }
    float grab = ctx->drag_grab_offset > 0.0f ? ctx->drag_grab_offset : thumb.h * 0.5f;
    ctx->dragging_area->offset = offset_from_cursor(&track, thumb.h, max_offset, (float)mouse_y, grab);
    scroll_apply_offsets(ctx, widgets, widget_count);
}

void scroll_rebuild(ScrollContext* ctx, Widget* widgets, size_t widget_count, float offset_scale) {
    if (!ctx) return;
    ScrollArea* old = ctx->areas;
    ctx->dragging_area = NULL;
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

void scroll_set_render_tree(ScrollContext* ctx, RenderNode* render_root) {
    if (!ctx) return;
    ctx->render_root = render_root;
    clear_area_render_info(ctx->areas);
    if (ctx->render_root) collect_area_render_info(ctx, ctx->render_root);
}

void scroll_free(ScrollContext* ctx) {
    if (!ctx) return;
    free_scroll_areas(ctx->areas);
    free(ctx);
}
