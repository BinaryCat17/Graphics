#include "services/ui/compositor.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct ClipContext {
    Rect stack[UI_CLIP_STACK_MAX];
    size_t depth;
} ClipContext;

static int rect_equal(const Rect* a, const Rect* b) {
    const float eps = 1e-4f;
    return fabsf(a->x - b->x) < eps && fabsf(a->y - b->y) < eps && fabsf(a->w - b->w) < eps && fabsf(a->h - b->h) < eps;
}

static void clip_push(ClipContext* ctx, Rect r) {
    if (!ctx || ctx->depth >= UI_CLIP_STACK_MAX) return;
    ctx->stack[ctx->depth++] = r;
}

static void clip_pop(ClipContext* ctx) {
    if (!ctx || ctx->depth == 0) return;
    ctx->depth--;
}

static int copy_stack(const ClipContext* ctx, Rect* out, size_t* out_depth) {
    if (!ctx || !out || !out_depth) return 0;
    if (ctx->depth > UI_CLIP_STACK_MAX) return 0;
    memcpy(out, ctx->stack, ctx->depth * sizeof(Rect));
    *out_depth = ctx->depth;
    return 1;
}

static int ensure_capacity(DisplayList* list, size_t required) {
    if (!list) return 0;
    if (required <= list->count) return 1;
    size_t new_cap = list->count == 0 ? required : list->count;
    while (new_cap < required) new_cap *= 2;
    DisplayItem* expanded = (DisplayItem*)realloc(list->items, new_cap * sizeof(DisplayItem));
    if (!expanded) return 0;
    list->items = expanded;
    return 1;
}

static void append_item(DisplayList* list, const LayoutNode* layout, Widget* widget, const ClipContext* clip_ctx, size_t appearance) {
    if (!list || !widget) return;
    size_t idx = list->count;
    if (!ensure_capacity(list, idx + 1)) return;

    DisplayItem* item = &list->items[idx];
    memset(item, 0, sizeof(DisplayItem));
    item->layout = layout;
    item->widget = widget;
    item->z_group = widget->z_group;
    item->z_index = widget->z_index;
    item->appearance_order = appearance;
    if (clip_ctx) copy_stack(clip_ctx, item->clip_stack, &item->clip_depth);
    list->count++;
}

static void traverse_layout(const LayoutNode* node, Widget* widgets, size_t widget_count, size_t* widget_cursor, size_t* appearance,
                            ClipContext* clips, DisplayList* out) {
    if (!node || !node->source || !clips || !out || !appearance || !widget_cursor) return;

    int pushed_layout_clip = 0;
    if (node->wants_clip) {
        clip_push(clips, (Rect){node->rect.x, node->rect.y, node->rect.w, node->rect.h});
        pushed_layout_clip = 1;
    }

    int has_widget = (node->source->layout == UI_LAYOUT_NONE || node->source->scroll_static);
    int pushed_widget_clip = 0;
    Widget* widget = NULL;
    if (has_widget && widget_cursor && widgets && *widget_cursor < widget_count) {
        widget = &widgets[*widget_cursor];
        (*widget_cursor)++;
        if (widget->has_clip) {
            clip_push(clips, widget->clip);
            pushed_widget_clip = 1;
        }
        append_item(out, node, widget, clips, *appearance);
        (*appearance)++;
    }

    for (size_t i = 0; i < node->child_count; ++i) {
        traverse_layout(&node->children[i], widgets, widget_count, widget_cursor, appearance, clips, out);
    }

    if (pushed_widget_clip) clip_pop(clips);
    if (pushed_layout_clip) clip_pop(clips);
}

static int cmp_display_item(const void* a, const void* b) {
    const DisplayItem* ia = (const DisplayItem*)a;
    const DisplayItem* ib = (const DisplayItem*)b;
    if (ia->z_group != ib->z_group) return (ia->z_group < ib->z_group) ? -1 : 1;
    if (ia->z_index != ib->z_index) return (ia->z_index < ib->z_index) ? -1 : 1;
    if (ia->appearance_order == ib->appearance_order) return 0;
    return (ia->appearance_order < ib->appearance_order) ? -1 : 1;
}

static void compute_clip_transitions(DisplayList* list) {
    if (!list || !list->items || list->count == 0) return;
    Rect prev_stack[UI_CLIP_STACK_MAX];
    size_t prev_depth = 0;

    for (size_t i = 0; i < list->count; ++i) {
        DisplayItem* item = &list->items[i];
        size_t common = 0;
        while (common < prev_depth && common < item->clip_depth && rect_equal(&prev_stack[common], &item->clip_stack[common])) {
            common++;
        }

        item->clip_pop = prev_depth - common;
        item->clip_push = item->clip_depth - common;
        if (item->clip_push > 0) {
            size_t offset = 0;
            for (size_t j = common; j < item->clip_depth && offset < UI_CLIP_STACK_MAX; ++j, ++offset) {
                item->push_rects[offset] = item->clip_stack[j];
            }
        }

        memcpy(prev_stack, item->clip_stack, item->clip_depth * sizeof(Rect));
        prev_depth = item->clip_depth;
    }
}

DisplayList ui_compositor_build(const LayoutNode* layout_root, Widget* widgets, size_t widget_count) {
    DisplayList list = {0};
    ClipContext clips = {0};
    size_t widget_cursor = 0;
    size_t appearance = 0;
    traverse_layout(layout_root, widgets, widget_count, &widget_cursor, &appearance, &clips, &list);
    if (list.items && list.count > 0) {
        qsort(list.items, list.count, sizeof(DisplayItem), cmp_display_item);
        compute_clip_transitions(&list);
    }
    return list;
}

void ui_compositor_free(DisplayList list) {
    free(list.items);
}

