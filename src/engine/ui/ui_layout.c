#include "ui_layout.h"
#include "foundation/logger/logger.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

static UiTextMeasureFunc g_measure_func = NULL;
static void* g_measure_user_data = NULL;

void ui_layout_set_measure_func(UiTextMeasureFunc func, void* user_data) {
    g_measure_func = func;
    g_measure_user_data = user_data;
}

static void layout_recursive(UiView* view, Rect available, uint64_t frame_number, bool log_debug) {
    if (!view || !view->def) return;

    // Determine size
    float w = view->def->width;
    float h = view->def->height;
    
    // Override with bound values if present
    if (view->def->w_source) w = view->rect.w;
    if (view->def->h_source) h = view->rect.h;

    // Auto-width logic
    if (w < 0) {
        bool parent_is_row = (view->parent && view->parent->def->layout == UI_LAYOUT_ROW);
        
        if (parent_is_row || view->def->type == UI_NODE_LABEL || view->def->type == UI_NODE_BUTTON) {
             const char* text = view->cached_text ? view->cached_text : view->def->text;
             if (text) {
                 if (g_measure_func) {
                     float text_w = g_measure_func(text, g_measure_user_data);
                     w = text_w + view->def->padding * 2;
                 } else {
                     // Fallback Estimate: 10px per char approx
                     w = strlen(text) * 10.0f + view->def->padding * 2 + 10.0f;
                 }
             } else {
                 w = 100.0f; // Default small width
             }
        } else {
             w = available.w; // Fill available (Column default)
        }
    }

    if (h < 0) h = 30.0f; // Default height for auto
    
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

    if (view->def->x_source) {
        view->rect.x = available.x + view->rect.x; // Add relative offset
    } else {
        view->rect.x = available.x;
    }
    
    if (view->def->y_source) {
        view->rect.y = available.y + view->rect.y;
    } else {
        view->rect.y = available.y;
    }

    view->rect.w = w;
    view->rect.h = h;

    if (log_debug) {
        LOG_DEBUG("[Frame %llu] Layout Node id='%s': Rect(%.1f, %.1f, %.1f, %.1f)", 
            (unsigned long long)frame_number,
            view->def->id ? view->def->id : "(anon)",
            view->rect.x, view->rect.y, view->rect.w, view->rect.h);
    }

    // Layout Children
    Rect content = {
        view->rect.x + view->def->padding,
        view->rect.y + view->def->padding,
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
        
        layout_recursive(child, child_avail, frame_number, log_debug);

        if (view->def->layout == UI_LAYOUT_COLUMN) {
            cursor_y += child->rect.h + view->def->spacing;
        } else if (view->def->layout == UI_LAYOUT_ROW) {
            cursor_x += child->rect.w + view->def->spacing;
        }
        // Overlay: cursor doesn't move
    }
}

void ui_layout_root(UiView* root, float window_w, float window_h, uint64_t frame_number, bool log_debug) {
    if (!root) return;
    Rect screen_rect = {0, 0, window_w, window_h};
    layout_recursive(root, screen_rect, frame_number, log_debug);
}
