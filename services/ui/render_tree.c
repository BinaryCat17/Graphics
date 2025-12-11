#include "ui/render_tree.h"

#include <stdlib.h>

static void free_node(RenderNode* node) {
    if (!node) return;
    for (size_t i = 0; i < node->child_count; ++i) {
        free_node(&node->children[i]);
    }
    free(node->children);
}

static RenderNode* build_node(const LayoutNode* layout, Widget* widgets, size_t widget_count, size_t* widget_cursor) {
    if (!layout) return NULL;
    RenderNode* node = (RenderNode*)calloc(1, sizeof(RenderNode));
    if (!node) return NULL;

    node->layout = layout;
    node->rect = layout->rect;
    node->clip = layout->clip;
    node->has_clip = layout->has_clip;
    node->z_index = layout->source ? layout->source->z_index : 0;
    node->render_index = 0;
    node->alpha = 1.0f;
    node->inertial_scroll = 0;

    if (layout->source && (layout->source->layout == UI_LAYOUT_NONE || layout->source->scroll_static)) {
        if (widget_cursor && widgets && *widget_cursor < widget_count) {
            node->widget = &widgets[*widget_cursor];
            (*widget_cursor)++;
            node->rect = node->widget->rect;
            node->has_clip = node->widget->has_clip;
            if (node->has_clip) node->clip = node->widget->clip;
        }
    }

    if (layout->child_count > 0) {
        node->children = (RenderNode*)calloc(layout->child_count, sizeof(RenderNode));
        node->child_count = layout->child_count;
        if (!node->children) {
            free(node);
            return NULL;
        }
        for (size_t i = 0; i < layout->child_count; ++i) {
            size_t before = *widget_cursor;
            RenderNode* child = build_node(&layout->children[i], widgets, widget_count, widget_cursor);
            if (!child) {
                free_node(node);
                free(node);
                return NULL;
            }
            node->children[i] = *child;
            free(child);
            // Ensure widget cursor advances even if the child did not attach a widget.
            if (*widget_cursor < before) *widget_cursor = before;
        }
    }

    return node;
}

RenderNode* render_tree_build(const LayoutNode* layout_root, Widget* widgets, size_t widget_count) {
    if (!layout_root) return NULL;
    size_t cursor = 0;
    return build_node(layout_root, widgets, widget_count, &cursor);
}

void render_tree_free(RenderNode* root) {
    if (!root) return;
    free_node(root);
    free(root);
}

static Rect intersect_rect(const Rect* a, const Rect* b, int* has) {
    Rect out = {0};
    if (!a && !b) {
        *has = 0;
        return out;
    }
    if (!a) {
        *has = 1;
        return *b;
    }
    if (!b) {
        *has = 1;
        return *a;
    }
    float x0 = a->x > b->x ? a->x : b->x;
    float y0 = a->y > b->y ? a->y : b->y;
    float x1 = (a->x + a->w) < (b->x + b->w) ? (a->x + a->w) : (b->x + b->w);
    float y1 = (a->y + a->h) < (b->y + b->h) ? (a->y + a->h) : (b->y + b->h);
    if (x1 <= x0 || y1 <= y0) {
        *has = 0;
        return (Rect){0};
    }
    *has = 1;
    out.x = x0;
    out.y = y0;
    out.w = x1 - x0;
    out.h = y1 - y0;
    return out;
}

static void propagate_down(RenderNode* node, const Rect* parent_clip) {
    if (!node) return;
    const Rect* self_clip = node->has_clip ? &node->clip : NULL;
    int merged_has = 0;
    node->clip = intersect_rect(parent_clip, self_clip, &merged_has);
    node->has_clip = merged_has;

    for (size_t i = 0; i < node->child_count; ++i) {
        propagate_down(&node->children[i], node->has_clip ? &node->clip : parent_clip);
    }
}

static void assign_render_indices(RenderNode* node, size_t* cursor) {
    if (!node || !cursor) return;
    node->render_index = *cursor;
    (*cursor)++;
    for (size_t i = 0; i < node->child_count; ++i) {
        assign_render_indices(&node->children[i], cursor);
    }
}

static void propagate_up(RenderNode* node, Rect* content_bounds, int* has_content) {
    if (!node) return;
    Rect bounds = node->rect;
    int bounds_valid = (bounds.w > 0.0f && bounds.h > 0.0f);
    if (bounds_valid) {
        if (!*has_content) {
            *content_bounds = bounds;
            *has_content = 1;
        } else {
            float x0 = bounds.x < content_bounds->x ? bounds.x : content_bounds->x;
            float y0 = bounds.y < content_bounds->y ? bounds.y : content_bounds->y;
            float x1 = (bounds.x + bounds.w) > (content_bounds->x + content_bounds->w)
                           ? (bounds.x + bounds.w)
                           : (content_bounds->x + content_bounds->w);
            float y1 = (bounds.y + bounds.h) > (content_bounds->y + content_bounds->h)
                           ? (bounds.y + bounds.h)
                           : (content_bounds->y + content_bounds->h);
            content_bounds->x = x0;
            content_bounds->y = y0;
            content_bounds->w = x1 - x0;
            content_bounds->h = y1 - y0;
        }
    }

    for (size_t i = 0; i < node->child_count; ++i) {
        propagate_up(&node->children[i], content_bounds, has_content);
    }
}

void render_tree_propagate(RenderNode* root) {
    if (!root) return;
    propagate_down(root, NULL);
    size_t order = 0;
    assign_render_indices(root, &order);
    Rect bounds = {0};
    int has_bounds = 0;
    propagate_up(root, &bounds, &has_bounds);
}

static void sync_widget(RenderNode* node) {
    if (!node) return;
    if (node->widget) {
        node->rect = node->widget->rect;
        node->has_clip = node->widget->has_clip;
        if (node->has_clip) node->clip = node->widget->clip;
        node->z_index = node->widget->z_index;
        node->inertial_scroll = (int)(node->widget->scroll_offset != 0.0f);
    } else if (node->layout) {
        node->rect = node->layout->rect;
        node->has_clip = node->layout->has_clip;
        if (node->has_clip) node->clip = node->layout->clip;
        node->z_index = node->layout->source ? node->layout->source->z_index : node->z_index;
    }
    for (size_t i = 0; i < node->child_count; ++i) sync_widget(&node->children[i]);
}

void render_tree_sync_widgets(RenderNode* root) {
    sync_widget(root);
    render_tree_propagate(root);
}
