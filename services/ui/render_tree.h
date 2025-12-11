#ifndef UI_RENDER_TREE_H
#define UI_RENDER_TREE_H

#include "ui/ui_config.h"

typedef struct RenderNode {
    const LayoutNode* layout;
    Widget* widget;
    Rect rect;
    Rect clip;
    int has_clip;
    int inertial_scroll;
    float alpha;
    struct RenderNode* children;
    size_t child_count;
} RenderNode;

RenderNode* render_tree_build(const LayoutNode* layout_root, Widget* widgets, size_t widget_count);
void render_tree_free(RenderNode* root);
void render_tree_propagate(RenderNode* root);
void render_tree_sync_widgets(RenderNode* root);

#endif // UI_RENDER_TREE_H
