#include "ui_layout.h"
#include "ui_core.h"
#include "foundation/logger/logger.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

static float calculate_width(UiElement* el, float available_w, UiTextMeasureFunc measure_func, void* measure_data) {
    const UiNodeSpec* spec = el->spec;
    float w = spec->width;
    if (spec->w_source) w = el->rect.w; // updated by ui_core

    if (w < 0) {
        bool parent_is_row = (el->parent && el->parent->spec->layout == UI_LAYOUT_FLEX_ROW);
        
        if (parent_is_row || spec->kind == UI_KIND_TEXT || (spec->flags & UI_FLAG_CLICKABLE)) {
             const char* text = el->cached_text;
             if (!text || text[0] == '\0') text = spec->static_text;

             if (text && text[0] != '\0') {
                 if (measure_func) {
                     w = measure_func(text, measure_data) + spec->padding * 2;
                 } else {
                     w = strlen(text) * 10.0f + spec->padding * 2 + 10.0f;
                 }
             } else {
                 w = 100.0f;
             }
        } else {
             w = available_w; // Fill
        }
    }
    return w;
}

static float calculate_height(UiElement* el, float available_h) {
    const UiNodeSpec* spec = el->spec;
    float h = spec->height;
    if (spec->h_source) h = el->rect.h;

    if (h < 0) {
        if (el->child_count > 0 && spec->layout == UI_LAYOUT_FLEX_COLUMN) {
            h = spec->padding * 2;
             for (size_t i = 0; i < el->child_count; ++i) {
                float child_h = el->children[i]->spec->height;
                if (child_h < 0) child_h = 30.0f; 
                h += child_h + spec->spacing;
            }
            if (el->child_count > 0) h -= spec->spacing;
            
            if (available_h > 0 && available_h < 10000.0f && h < available_h) {
                h = available_h;
            }
        } else {
             h = (available_h > 0 && available_h < 10000.0f) ? available_h : 30.0f;
        }
    }
    return h;
}

static void layout_column(UiElement* el, float start_x, float start_y, float* out_max_x, float* out_max_y) {
    float cursor_y = start_y;
    for (size_t i = 0; i < el->child_count; ++i) {
        UiElement* child = el->children[i];
        child->rect.x = start_x;
        child->rect.y = cursor_y;
        cursor_y += child->rect.h + el->spec->spacing;
        
        float child_right = child->rect.x + child->rect.w;
        if (child_right > *out_max_x) *out_max_x = child_right;
    }
    if (el->child_count > 0) cursor_y -= el->spec->spacing;
    *out_max_y = cursor_y;
}

static void layout_row(UiElement* el, float start_x, float start_y, float* out_max_x, float* out_max_y) {
    float cursor_x = start_x;
    for (size_t i = 0; i < el->child_count; ++i) {
        UiElement* child = el->children[i];
        child->rect.x = cursor_x;
        child->rect.y = start_y;
        cursor_x += child->rect.w + el->spec->spacing;
        
        float child_bottom = child->rect.y + child->rect.h;
        if (child_bottom > *out_max_y) *out_max_y = child_bottom;
    }
    if (el->child_count > 0) cursor_x -= el->spec->spacing;
    *out_max_x = cursor_x;
}

static void layout_canvas(UiElement* el) {
    if (el->spec->flags & UI_FLAG_SCROLLABLE) {
        for (size_t i = 0; i < el->child_count; ++i) {
            el->children[i]->rect.x -= el->scroll_x;
            el->children[i]->rect.y -= el->scroll_y;
        }
    }
}

static void layout_recursive(UiElement* el, Rect available, uint64_t frame_number, bool log_debug, UiTextMeasureFunc measure_func, void* measure_data) {
    if (!el || !el->spec) return;
    const UiNodeSpec* spec = el->spec;

    // 1. Self Size
    el->rect.w = calculate_width(el, available.w, measure_func, measure_data);
    el->rect.h = calculate_height(el, available.h);

    if (log_debug) {
        LOG_DEBUG("[Frame %llu] Layout Node id='%s': Rect(%.1f, %.1f, %.1f, %.1f)", 
            (unsigned long long)frame_number, spec->id ? spec->id : "(anon)",
            el->rect.x, el->rect.y, el->rect.w, el->rect.h);
    }

    // 2. Prepare Children Layout
    Rect content = {
        spec->padding, spec->padding,
        el->rect.w - spec->padding * 2, el->rect.h - spec->padding * 2
    };
    
    // Recurse First (Depth-first sizing)
    for (size_t i = 0; i < el->child_count; ++i) {
        Rect child_avail = { 0, 0, content.w, content.h };
        layout_recursive(el->children[i], child_avail, frame_number, log_debug, measure_func, measure_data);
    }

    // 3. Position Children
    float start_x = content.x - el->scroll_x;
    float start_y = content.y - el->scroll_y;
    
    float max_x = start_x; // Use absolute max coordinate tracking
    float max_y = start_y;
    
    switch (spec->layout) {
        case UI_LAYOUT_FLEX_COLUMN:
            layout_column(el, start_x, start_y, &max_x, &max_y);
            // Convert absolute max back to content relative size
            el->content_w = max_x - start_x;
            el->content_h = max_y - start_y;
            break;
        case UI_LAYOUT_FLEX_ROW:
            layout_row(el, start_x, start_y, &max_x, &max_y);
            el->content_w = max_x - start_x;
            el->content_h = max_y - start_y;
            break;
        case UI_LAYOUT_CANVAS:
            layout_canvas(el);
            // Canvas content size not fully implemented yet
            el->content_w = 0; 
            el->content_h = 0;
            break;
        default: break;
    }
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

void ui_layout_root(UiElement* root, float window_w, float window_h, uint64_t frame_number, bool log_debug, UiTextMeasureFunc measure_func, void* measure_data) {
    if (!root) return;
    
    if (root->spec->width < 0) root->rect.w = window_w;
    if (root->spec->height < 0) root->rect.h = window_h;
    
    Rect initial_avail = {0, 0, window_w, window_h};
    layout_recursive(root, initial_avail, frame_number, log_debug, measure_func, measure_data);
    update_screen_rects(root, 0, 0);
}
