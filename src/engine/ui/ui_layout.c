#include "ui_layout.h"
#include <stddef.h>

static void layout_recursive(UiView* view, Rect available) {
    if (!view || !view->def) return;

    // Determine size
    float w = view->def->width < 0 ? available.w : view->def->width;
    float h = view->def->height < 0 ? 30.0f : view->def->height; // Default height for auto
    
    // Auto height for container based on children (Simple Stack)
    if (view->def->height < 0 && view->child_count > 0 && view->def->layout == UI_LAYOUT_COLUMN) {
        h = view->def->padding * 2;
        for (size_t i = 0; i < view->child_count; ++i) {
            // Recursive pre-calc needed? 
            // For simple stacking, we assume children have fixed or relative height.
            // If child is also auto, this is tricky (multi-pass).
            // Simplified: Child auto-height defaults to 30.0f unless layout happened.
            // We do a single pass top-down. Child gets constraint, calculates its size.
            // Wait, to know container height, we need to layout children first?
            // Or we layout children with "infinite" height and see where they land.
            // Let's keep it simple: Fixed height or 30.0f default for now.
            // Proper way: Measure pass -> Layout pass.
            // Simplified way: Top-down with growing bounds? No.
            // Let's stick to the previous logic:
            
            float child_h = view->children[i]->def->height;
            if (child_h < 0) child_h = 30.0f; 
            h += child_h;
            h += view->def->spacing;
        }
        // Remove last spacing
        if (view->child_count > 0) h -= view->def->spacing;
    }

    view->rect.x = available.x;
    view->rect.y = available.y;
    view->rect.w = w;
    view->rect.h = h;

    // Layout Children
    Rect content = {
        available.x + view->def->padding,
        available.y + view->def->padding,
        w - view->def->padding * 2,
        h - view->def->padding * 2
    };

    float cursor_x = content.x;
    float cursor_y = content.y;

    for (size_t i = 0; i < view->child_count; ++i) {
        UiView* child = view->children[i];
        
        // Available space for child
        Rect child_avail = { 
            cursor_x, 
            cursor_y, 
            content.w, 
            // If column, height is flexible? No, use child def. 
            // If child is auto (-1), it takes available? Or default?
            content.h - (cursor_y - content.y) 
        };
        
        layout_recursive(child, child_avail);

        if (view->def->layout == UI_LAYOUT_COLUMN) {
            cursor_y += child->rect.h + view->def->spacing;
        } else if (view->def->layout == UI_LAYOUT_ROW) {
            cursor_x += child->rect.w + view->def->spacing;
        }
        // Overlay: cursor doesn't move
    }
}

void ui_layout_root(UiView* root, float window_w, float window_h) {
    if (!root) return;
    Rect screen_rect = {0, 0, window_w, window_h};
    layout_recursive(root, screen_rect);
}
