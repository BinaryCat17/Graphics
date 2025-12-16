#include "services/ui/scroll.h"
#include "core/platform/platform.h"

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
    float offset;
    float target_offset;
    struct ScrollArea* next;
} ScrollArea;

struct ScrollContext {
    ScrollArea* areas;
    Widget* widgets;
    size_t widget_count;
    ScrollArea* dragging_area;
    Widget* dragging_widget;
    float drag_grab_offset;
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
    a->name = platform_strdup(name);
    a->offset = 0.0f;
    a->target_offset = 0.0f;
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

    if (w->type == W_SCROLLBAR) {
        float new_area = w->rect.w * w->rect.h;
        float old_area = a->has_viewport ? a->viewport.w * a->viewport.h : -1.0f;
        if (!a->has_viewport || new_area > old_area) {
            a->viewport = w->rect;
            a->has_viewport = 1;
        }
    }
}

static float clamp_scroll_offset(float offset, float max_offset) {
    if (offset < 0.0f) return 0.0f;
    if (offset > max_offset) return max_offset;
    return offset;
}

static int compute_scrollbar_geometry(const Widget* w, Rect* out_track, Rect* out_thumb, float* out_max_offset) {
    if (!w || w->type != W_SCROLLBAR || !w->data.scroll.enabled || !w->data.scroll.show || w->data.scroll.viewport_size <= 0.0f) return 0;
    
    float max_offset = w->data.scroll.content_size - w->data.scroll.viewport_size;
    if (max_offset <= 1.0f) return 0;
    
    Rect inner_rect = w->rect;
    if (w->border_thickness > 0.0f) {
        inner_rect.x += w->border_thickness;
        inner_rect.y += w->border_thickness;
        inner_rect.w -= w->border_thickness * 2.0f;
        inner_rect.h -= w->border_thickness * 2.0f;
        if (inner_rect.w < 0.0f) inner_rect.w = 0.0f;
        if (inner_rect.h < 0.0f) inner_rect.h = 0.0f;
    }
    
    float track_w = w->data.scroll.width > 0.0f ? w->data.scroll.width : fmaxf(4.0f, inner_rect.w * 0.02f);
    float track_h = inner_rect.h - w->padding * 2.0f;
    if (track_h <= 0.0f) return 0;
    
    float track_x = inner_rect.x + inner_rect.w - track_w - w->padding * 0.5f;
    float track_y = inner_rect.y + w->padding;
    float thumb_ratio = w->data.scroll.viewport_size / w->data.scroll.content_size;
    float thumb_h = fmaxf(track_h * thumb_ratio, 12.0f);
    
    float clamped_offset = clamp_scroll_offset(w->scroll_offset, max_offset);
    float offset_t = (max_offset != 0.0f) ? (clamped_offset / max_offset) : 0.0f;
    float thumb_y = track_y + offset_t * (track_h - thumb_h);
    
    if (out_track) *out_track = (Rect){ track_x, track_y, track_w, track_h };
    if (out_thumb) *out_thumb = (Rect){ track_x, thumb_y, track_w, thumb_h };
    if (out_max_offset) *out_max_offset = max_offset;
    return 1;
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
        if (w->type == W_SCROLLBAR) area->has_static_anchor = 1;
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
        // w->show_scrollbar = 0; // Removed common field
        if(w->type == W_SCROLLBAR) w->data.scroll.show = 0;

        w->has_clip = 0;
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
        float view_x = viewport.x;
        float view_y = viewport.y;
        float view_w = viewport.w;
        float view_h = viewport.h;
        if (w->border_thickness > 0.0f) {
            view_x += w->border_thickness;
            view_y += w->border_thickness;
            view_w -= w->border_thickness * 2.0f;
            view_h -= w->border_thickness * 2.0f;
            if (view_w < 0.0f) view_w = 0.0f;
            if (view_h < 0.0f) view_h = 0.0f;
        }
        float viewport_h = viewport.h;
        float content_h = a->has_bounds ? a->bounds.h : viewport_h;
        float overflow = content_h - viewport_h;
        if (overflow < 0.0f) overflow = 0.0f;
        a->offset = clamp_scroll_offset(a->offset, overflow);
        
        if (w->type == W_SCROLLBAR) {
            w->data.scroll.viewport_size = viewport_h;
            w->data.scroll.content_size = content_h;
            w->scroll_offset = a->offset;
            w->data.scroll.show = w->data.scroll.enabled && overflow > 1.0f;
            w->has_clip = 1;
            w->clip = viewport;
        } else {
            w->scroll_offset = w->type == W_SCROLLBAR ? 0.0f : a->offset;
            // Removed common fields from normal widgets
            // w->scroll_viewport = viewport_h;
            // w->scroll_content = content_h;
            
            int should_clip = (w->type != W_SCROLLBAR) && w->clip_to_viewport;
            if (should_clip && (a->has_viewport || a->has_bounds)) {
                w->has_clip = 1;
                w->clip = viewport;
            }
        }
    }
}

void scroll_handle_event(ScrollContext* ctx, Widget* widgets, size_t widget_count, double mouse_x, double mouse_y, double yoff) {
    if (!ctx || !widgets) return;
    ctx->widgets = widgets;
    ctx->widget_count = widget_count;
    ScrollArea* target = NULL;
    int best_z = 0;
    for (size_t i = 0; i < widget_count; ++i) {
        Widget* w = &widgets[i];
        if (!w->scroll_area) continue;
        ScrollArea* area = find_area(ctx->areas, w->scroll_area);
        if (!area || !area->has_bounds) continue;
        Rect bounds = w->has_clip ? w->clip : (Rect){ w->rect.x, w->rect.y + (w->type == W_SCROLLBAR ? 0.0f : w->scroll_offset), w->rect.w, w->rect.h };
        if (mouse_x < bounds.x || mouse_x > bounds.x + bounds.w || mouse_y < bounds.y || mouse_y > bounds.y + bounds.h) continue;
        if (!target || w->z_index > best_z) {
            target = area;
            best_z = w->z_index;
        }
    }
    if (target) {
        float viewport_h = target->has_viewport ? target->viewport.h : target->bounds.h;
        float max_offset = target->has_bounds ? (target->bounds.h - viewport_h) : 0.0f;
        if (max_offset < 0.0f) max_offset = 0.0f;
        target->target_offset = clamp_scroll_offset(target->target_offset - (float)yoff * 120.0f, max_offset);
    }
}

int scroll_handle_mouse_button(ScrollContext* ctx, Widget* widgets, size_t widget_count, double mouse_x, double mouse_y, int pressed) {
    if (!ctx || !widgets) return 0;
    ctx->widgets = widgets;
    ctx->widget_count = widget_count;
    int consumed = 0;

    if (!pressed) {
        if (ctx->dragging_area) consumed = 1;
        ctx->dragging_area = NULL;
        ctx->dragging_widget = NULL;
        return consumed;
    }

    Widget* target_widget = NULL;
    int best_z = 0;
    for (size_t i = 0; i < widget_count; ++i) {
        Widget* w = &widgets[i];
        if (!w->scroll_area || w->type != W_SCROLLBAR) continue;
        Rect track = {0};
        Rect thumb = {0};
        float max_offset = 0.0f;
        if (!compute_scrollbar_geometry(w, &track, &thumb, &max_offset)) continue;
        if (mouse_x >= track.x && mouse_x <= track.x + track.w && mouse_y >= track.y && mouse_y <= track.y + track.h) {
            if (!target_widget || w->z_index > best_z) {
                target_widget = w;
                best_z = w->z_index;
            }
        }
    }

    if (target_widget) {
        ScrollArea* area = find_area(ctx->areas, target_widget->scroll_area);
        Rect track = {0};
        Rect thumb = {0};
        float max_offset = 0.0f;
        if (area && compute_scrollbar_geometry(target_widget, &track, &thumb, &max_offset)) {
            ctx->dragging_area = area;
            ctx->dragging_widget = target_widget;
            ctx->drag_grab_offset = (float)(mouse_y - thumb.y);
            consumed = 1;
            if (!(mouse_y >= thumb.y && mouse_y <= thumb.y + thumb.h)) {
                ctx->drag_grab_offset = thumb.h * 0.5f;
                area->offset = offset_from_cursor(&track, thumb.h, max_offset, (float)mouse_y, ctx->drag_grab_offset);
                area->target_offset = area->offset;
                scroll_apply_offsets(ctx, widgets, widget_count);
            }
        }
    }
    return consumed;
}

void scroll_handle_cursor(ScrollContext* ctx, Widget* widgets, size_t widget_count, double mouse_x, double mouse_y) {
    (void)mouse_x;
    if (!ctx || !ctx->dragging_area || !widgets || !ctx->dragging_widget) return;
    ctx->widgets = widgets;
    ctx->widget_count = widget_count;
    Rect track = {0};
    Rect thumb = {0};
    float max_offset = 0.0f;
    if (!compute_scrollbar_geometry(ctx->dragging_widget, &track, &thumb, &max_offset)) { ctx->dragging_area = NULL; ctx->dragging_widget = NULL; return; }
    float grab = ctx->drag_grab_offset > 0.0f ? ctx->drag_grab_offset : thumb.h * 0.5f;
    ctx->dragging_area->offset = offset_from_cursor(&track, thumb.h, max_offset, (float)mouse_y, grab);
    ctx->dragging_area->target_offset = ctx->dragging_area->offset;
    scroll_apply_offsets(ctx, widgets, widget_count);
}

void scroll_rebuild(ScrollContext* ctx, Widget* widgets, size_t widget_count, float offset_scale) {
    if (!ctx) return;
    ScrollArea* old = ctx->areas;
    ctx->dragging_area = NULL;
    ctx->dragging_widget = NULL;
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
        if (w->type == W_SCROLLBAR) area->has_static_anchor = 1;
    }

    free_scroll_areas(old);
    scroll_apply_offsets(ctx, widgets, widget_count);
}

void scroll_update(ScrollContext* ctx, float dt) {
    if (!ctx || !ctx->areas) return;
    int changed = 0;
    for (ScrollArea* a = ctx->areas; a; a = a->next) {
        float diff = a->target_offset - a->offset;
        if (fabsf(diff) > 0.1f) {
            a->offset += diff * 10.0f * dt; // Simple lerp-like smoothing
            if (fabsf(a->target_offset - a->offset) < 0.1f) {
                a->offset = a->target_offset;
            }
            changed = 1;
        }
    }
    if (changed) {
        scroll_apply_offsets(ctx, ctx->widgets, ctx->widget_count);
    }
}

void scroll_free(ScrollContext* ctx) {
    if (!ctx) return;
    free_scroll_areas(ctx->areas);
    free(ctx);
}