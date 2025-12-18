#include "ui_layout.h"
#include "ui_core.h"
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

static void layout_recursive(UiElement* el, Rect available, uint64_t frame_number, bool log_debug) {
    if (!el || !el->spec) return;
    const UiNodeSpec* spec = el->spec;

    // 1. Determine size
    float w = spec->width;
    float h = spec->height;
    
    // Override with bound values
    if (spec->w_source) w = el->rect.w; // updated by ui_core
    if (spec->h_source) h = el->rect.h;

    // Auto-width logic
    if (w < 0) {
        bool parent_is_row = (el->parent && el->parent->spec->layout == UI_LAYOUT_FLEX_ROW);
        
        if (parent_is_row || spec->kind == UI_KIND_TEXT || (spec->flags & UI_FLAG_CLICKABLE)) {
             const char* text = el->cached_text ? el->cached_text : spec->static_text;
             if (text) {
                 if (g_measure_func) {
                     float text_w = g_measure_func(text, g_measure_user_data);
                     w = text_w + spec->padding * 2;
                 } else {
                     w = strlen(text) * 10.0f + spec->padding * 2 + 10.0f;
                 }
             } else {
                 w = 100.0f;
             }
        } else {
             w = available.w; // Fill
        }
    }

    // Auto-height / Fill logic
    if (h < 0) {
        // If we are in a column, -1 usually means "fit content" (auto) because we can't fill infinite scroll.
        // But if we have specific "available" height passed from parent (like fixed height parent), we could fill.
        // Current simplified logic:
        // If Container and Column: Auto grow.
        // If Container and Row or Canvas: Fill available?
        // Let's rely on "available" if it's not a huge number (infinite).
        
        if (el->child_count > 0 && spec->layout == UI_LAYOUT_FLEX_COLUMN) {
            // Calculate from children later
            h = spec->padding * 2;
             for (size_t i = 0; i < el->child_count; ++i) {
                float child_h = el->children[i]->spec->height;
                if (child_h < 0) child_h = 30.0f; // Approx for now
                h += child_h;
                h += spec->spacing;
            }
            if (el->child_count > 0) h -= spec->spacing;
            
            // If calculated height is small but we want to fill (e.g. root), take available
            if (available.h > 0 && available.h < 10000.0f && h < available.h) {
                // Only fill if we are the only child or explicity asked?
                // For 'root' (width=-1, height=-1), we expect fill.
                // Let's say if h < available.h and we are 'root' or similar, we take available.
                // Simple heuristic: take max.
                h = available.h;
            }
        } else {
            // Default fill
             if (available.h > 0 && available.h < 10000.0f) {
                 h = available.h;
             } else {
                 h = 30.0f; 
             }
        }
    }

    // 2. Position (Relative to Parent Content Area)
    if (spec->x_source) {
        // Absolute overrides (Canvas)
        // el->rect.x is already set by ui_core
    } else {
        // Relative start (default)
        // el->rect.x = 0; 
    }
    
    // Set Final Size
    el->rect.w = w;
    el->rect.h = h;

    if (log_debug) {
        LOG_DEBUG("[Frame %llu] Layout Node id='%s': Rect(%.1f, %.1f, %.1f, %.1f)", 
            (unsigned long long)frame_number,
            spec->id ? spec->id : "(anon)",
            el->rect.x, el->rect.y, el->rect.w, el->rect.h);
    }

    // 3. Layout Children
    Rect content = {
        0 + spec->padding,
        0 + spec->padding,
        w - spec->padding * 2,
        h - spec->padding * 2
    };

    float cursor_x = content.x - el->scroll_x;
    float cursor_y = content.y - el->scroll_y;

    float max_child_x = 0.0f;
    float max_child_y = 0.0f;

    for (size_t i = 0; i < el->child_count; ++i) {
        UiElement* child = el->children[i];
        
        // Available space for child
        Rect child_avail = { 
            cursor_x, 
            cursor_y, 
            content.w, 
            content.h // Simplified
        };
        
        // Recurse first
        layout_recursive(child, child_avail, frame_number, log_debug);

        // Apply Layout Strategy
        if (spec->layout == UI_LAYOUT_FLEX_COLUMN) {
            child->rect.x = cursor_x;
            child->rect.y = cursor_y;
            cursor_y += child->rect.h + spec->spacing;
            
            // Track content size (relative to scroll origin)
            // The cursor already moved, so we use the child's bottom edge + spacing relative to content start
            // Content start is at (content.x, content.y)
            float child_bottom = (child->rect.y - (content.y - el->scroll_y)) + child->rect.h;
            float child_right = (child->rect.x - (content.x - el->scroll_x)) + child->rect.w;
            
            if (child_bottom > max_child_y) max_child_y = child_bottom;
            if (child_right > max_child_x) max_child_x = child_right;

        } else if (spec->layout == UI_LAYOUT_FLEX_ROW) {
            child->rect.x = cursor_x;
            child->rect.y = cursor_y;
            cursor_x += child->rect.w + spec->spacing;

            float child_bottom = (child->rect.y - (content.y - el->scroll_y)) + child->rect.h;
            float child_right = (child->rect.x - (content.x - el->scroll_x)) + child->rect.w;
            
            if (child_bottom > max_child_y) max_child_y = child_bottom;
            if (child_right > max_child_x) max_child_x = child_right;
        }
        else if (spec->layout == UI_LAYOUT_CANVAS) {
            // Apply Scroll to Absolute nodes too?
            // Yes, if I scroll a canvas, the nodes should move.
            // But they have absolute X/Y from binding.
            // We should apply scroll offset to their base position?
            // Ideally: final_x = bound_x - scroll_x.
            // But here layout is relative.
            // So we just ensure child->rect.x includes the scroll offset?
            // If child->rect.x comes from binding (e.g. 500), and scroll is 100.
            // child->rect.x should stay 500 relative to origin.
            // But when drawing relative to parent, it should be 400?
            // Yes.
                         if (spec->flags & UI_FLAG_SCROLLABLE) {
                             child->rect.x -= el->scroll_x;
                             child->rect.y -= el->scroll_y;
                        }
                        // Canvas content size is tricky, assume it fits or max child
                        // Simple max logic for now
                         // We need to un-apply scroll to get "true" content size? 
                         // Actually, content size is the bounds of children in "unscrolled" space.             // If child is at 0, and we scrolled down 100, child.y is -100.
             // True Y is 0. 
             // content_h should be max(True Y + h)
             // True Y = child.rect.y + el->scroll_y - content.y
             // This is getting messy for Canvas. Let's stick to Flex for correct calcs first.
        }
    }
    
    // Store calculated content size
    if (spec->layout == UI_LAYOUT_FLEX_COLUMN && el->child_count > 0) {
        // Remove trailing spacing
        max_child_y -= spec->spacing;
    }
     if (spec->layout == UI_LAYOUT_FLEX_ROW && el->child_count > 0) {
        // Remove trailing spacing
        max_child_x -= spec->spacing;
    }
    
    el->content_w = max_child_x;
    el->content_h = max_child_y;
}

static void update_screen_rects(UiElement* el, float parent_x, float parent_y) {
    if (!el) return;
    
    el->screen_rect.x = parent_x + el->rect.x;
    el->screen_rect.y = parent_y + el->rect.y;
    el->screen_rect.w = el->rect.w;
    el->screen_rect.h = el->rect.h;

    for (size_t i = 0; i < el->child_count; ++i) {
        update_screen_rects(el->children[i], el->screen_rect.x, el->screen_rect.y);
    }
}

void ui_layout_root(UiElement* root, float window_w, float window_h, uint64_t frame_number, bool log_debug) {
    if (!root) return;
    
    if (root->spec->width < 0) root->rect.w = window_w;
    if (root->spec->height < 0) root->rect.h = window_h;
    
    Rect initial_avail = {0, 0, window_w, window_h};
    layout_recursive(root, initial_avail, frame_number, log_debug);
    update_screen_rects(root, 0, 0);
}