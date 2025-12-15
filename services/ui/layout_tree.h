#ifndef UI_LAYOUT_TREE_H
#define UI_LAYOUT_TREE_H

#include "coordinate_systems/coordinate_systems.h"
#include "ui/ui_node.h"

typedef struct LayoutNode {
    const UiNode* source;
    Rect rect;
    Rect base_rect;
    Rect local_rect;
    Vec2 transform;
    int wants_clip;
    struct LayoutNode* children;
    size_t child_count;
} LayoutNode;

LayoutNode* build_layout_tree(const UiNode* root);
void free_layout_tree(LayoutNode* root);
void measure_layout(LayoutNode* root, const char* font_path);
void assign_layout(LayoutNode* root, float origin_x, float origin_y);
void capture_layout_base(LayoutNode* root);

#endif // UI_LAYOUT_TREE_H
