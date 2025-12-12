#ifndef UI_COMPOSITOR_H
#define UI_COMPOSITOR_H

#include "ui/layout_tree.h"
#include "ui/widget_list.h"

#define UI_CLIP_STACK_MAX 16

typedef struct DisplayItem {
    const LayoutNode* layout;
    Widget* widget;
    int z_group;
    int z_index;
    size_t appearance_order;

    Rect clip_stack[UI_CLIP_STACK_MAX];
    size_t clip_depth;

    size_t clip_push;
    size_t clip_pop;
    Rect push_rects[UI_CLIP_STACK_MAX];
} DisplayItem;

typedef struct DisplayList {
    DisplayItem* items;
    size_t count;
} DisplayList;

DisplayList ui_compositor_build(const LayoutNode* layout_root, Widget* widgets, size_t widget_count);
void ui_compositor_free(DisplayList list);

#endif // UI_COMPOSITOR_H
