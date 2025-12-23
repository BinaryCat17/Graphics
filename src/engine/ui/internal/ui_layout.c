#include "ui_layout.h"
#include "../ui_core.h"
#include "ui_internal.h"
#include "foundation/logger/logger.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define UI_DEFAULT_WIDTH 100.0f
#define UI_DEFAULT_HEIGHT 30.0f
#define UI_CHAR_WIDTH_EST 10.0f
#define UI_INFINITY 10000.0f

static float calculate_width(SceneNode* el, float available_w, UiTextMeasureFunc measure_func, void* measure_data) {
    const SceneNodeSpec* spec = el->spec;
    float w = spec->layout.width;
    if (ui_node_get_binding(el, BINDING_TARGET_LAYOUT_WIDTH)) w = el->rect.w; // updated by ui_core

    if (w < 0) {
        bool parent_is_row = (el->parent && el->parent->spec->layout.type == SCENE_LAYOUT_FLEX_ROW);
        
    // If size is Auto, we need to calculate content size
    // For text, it's the glyph bounds. For container, it's the sum of children.
    // Hack: If container is CLICKABLE, treat it as having content even if empty? No.
    if (parent_is_row || spec->kind == SCENE_NODE_KIND_TEXT || (el->interaction_flags & SCENE_INTERACTION_CLICKABLE)) {
             const char* text = el->cached_text;
             if (!text || text[0] == '\0') text = spec->text;

             if (text && text[0] != '\0') {
                 if (measure_func) {
                     w = measure_func(text, 0.5f, measure_data).x + spec->layout.padding * 2;
                 } else {
                     w = strlen(text) * UI_CHAR_WIDTH_EST + spec->layout.padding * 2 + UI_CHAR_WIDTH_EST;
                 }
             } else {
                 w = UI_DEFAULT_WIDTH;
             }
        } else {
             w = available_w; // Fill
        }
    }
    return w;
}

static float calculate_height(SceneNode* el, float available_h) {
    const SceneNodeSpec* spec = el->spec;
    float h = spec->layout.height;
    if (ui_node_get_binding(el, BINDING_TARGET_LAYOUT_HEIGHT)) h = el->rect.h;

    if (h < 0) {
        if (el->child_count > 0 && spec->layout.type == SCENE_LAYOUT_FLEX_COLUMN) {
            h = spec->layout.padding * 2;
             for (SceneNode* child = el->first_child; child; child = child->next_sibling) {
                if (child->flags & SCENE_FLAG_HIDDEN) continue;
                float child_h = child->spec->layout.height;
                if (child_h < 0) child_h = UI_DEFAULT_HEIGHT; 
                h += child_h + spec->layout.spacing;
            }
            if (el->child_count > 0) h -= spec->layout.spacing;
            
            if (available_h > 0 && available_h < UI_INFINITY && h < available_h) {
                h = available_h;
            }
        } else {
             h = (available_h > 0 && available_h < UI_INFINITY) ? available_h : UI_DEFAULT_HEIGHT;
        }
    }
    return h;
}

static void layout_column(SceneNode* el, float start_x, float start_y, float* out_max_x, float* out_max_y) {
    float cursor_y = start_y;
    for (SceneNode* child = el->first_child; child; child = child->next_sibling) {
        if (child->flags & SCENE_FLAG_HIDDEN) continue;
        child->rect.x = start_x;
        child->rect.y = cursor_y;
        cursor_y += child->rect.h + el->spec->layout.spacing;
        
        float child_right = child->rect.x + child->rect.w;
        if (child_right > *out_max_x) *out_max_x = child_right;
    }
    if (el->child_count > 0) cursor_y -= el->spec->layout.spacing;
    *out_max_y = cursor_y;
}

static void layout_row(SceneNode* el, float start_x, float start_y, float* out_max_x, float* out_max_y) {
    float cursor_x = start_x;
    for (SceneNode* child = el->first_child; child; child = child->next_sibling) {
        if (child->flags & SCENE_FLAG_HIDDEN) continue;
        child->rect.x = cursor_x;
        child->rect.y = start_y;
        cursor_x += child->rect.w + el->spec->layout.spacing;
        
        float child_bottom = child->rect.y + child->rect.h;
        if (child_bottom > *out_max_y) *out_max_y = child_bottom;
    }
    if (el->child_count > 0) cursor_x -= el->spec->layout.spacing;
    *out_max_x = cursor_x;
}

static void layout_canvas(SceneNode* el, float* out_max_x, float* out_max_y) {
    *out_max_x = 0;
    *out_max_y = 0;
    for (SceneNode* child = el->first_child; child; child = child->next_sibling) {
    // 4. Handle Overflow / Scroll
    if (el->ui_flags & UI_FLAG_SCROLLABLE) {
            child->rect.x -= el->scroll_x;
            child->rect.y -= el->scroll_y;
        }
        
        // Calculate content bounds (using logical coordinates, reverting scroll)
        float logical_x = child->rect.x + (el->ui_flags & UI_FLAG_SCROLLABLE ? el->scroll_x : 0);
        float logical_y = child->rect.y + (el->ui_flags & UI_FLAG_SCROLLABLE ? el->scroll_y : 0);
        float right = logical_x + child->rect.w;
        float bottom = logical_y + child->rect.h;
        
        if (right > *out_max_x) *out_max_x = right;
        if (bottom > *out_max_y) *out_max_y = bottom;
    }
}

static void layout_split_h(SceneNode* el, float start_x, float start_y) {
    if (el->child_count < 2) return;
    SceneNode* c1 = el->first_child;
    if (!c1) return;
    SceneNode* c2 = c1->next_sibling;
    if (!c2) return;
    
    c1->rect.x = start_x;
    c1->rect.y = start_y;
    c2->rect.x = start_x + c1->rect.w;
    c2->rect.y = start_y;
}

static void layout_split_v(SceneNode* el, float start_x, float start_y) {
    if (el->child_count < 2) return;
    SceneNode* c1 = el->first_child;
    if (!c1) return;
    SceneNode* c2 = c1->next_sibling;
    if (!c2) return;
    
    c1->rect.x = start_x;
    c1->rect.y = start_y;
    c2->rect.x = start_x;
    c2->rect.y = start_y + c1->rect.h;
}

static void layout_recursive(SceneNode* el, Rect available, uint64_t frame_number, bool log_debug, UiTextMeasureFunc measure_func, void* measure_data) {
    if (!el || !el->spec) return;
    
    if (el->flags & SCENE_FLAG_HIDDEN) {
        el->rect.w = 0;
        el->rect.h = 0;
    }

    const SceneNodeSpec* spec = el->spec;

    // 1. Self Size
    el->rect.w = calculate_width(el, available.w, measure_func, measure_data);
    el->rect.h = calculate_height(el, available.h);

    if (log_debug) {
        LOG_DEBUG("[Frame %llu] Layout Node id='%u': Rect(%.1f, %.1f, %.1f, %.1f)", 
            (unsigned long long)frame_number, spec->id,
            el->rect.x, el->rect.y, el->rect.w, el->rect.h);
    }

    // 2. Prepare Children Layout
    Rect content = {
        spec->layout.padding, spec->layout.padding,
        el->rect.w - spec->layout.padding * 2, el->rect.h - spec->layout.padding * 2
    };
    
    // Recurse First (Depth-first sizing)
    int i = 0;
    for (SceneNode* child = el->first_child; child; child = child->next_sibling) {
        Rect child_avail = { 0, 0, content.w, content.h };
        
        if (spec->layout.type == SCENE_LAYOUT_SPLIT_H && el->child_count >= 2) {
             float ratio = spec->layout.split_ratio > 0 ? spec->layout.split_ratio : 0.5f;
             if (i == 0) child_avail.w = content.w * ratio;
             else child_avail.w = content.w * (1.0f - ratio);
        } else if (spec->layout.type == SCENE_LAYOUT_SPLIT_V && el->child_count >= 2) {
             float ratio = spec->layout.split_ratio > 0 ? spec->layout.split_ratio : 0.5f;
             if (i == 0) child_avail.h = content.h * ratio;
             else child_avail.h = content.h * (1.0f - ratio);
        }

        layout_recursive(child, child_avail, frame_number, log_debug, measure_func, measure_data);
        i++;
    }

    // 3. Position Children
    float start_x = content.x - el->scroll_x;
    float start_y = content.y - el->scroll_y;
    
    float max_x = start_x; // Use absolute max coordinate tracking
    float max_y = start_y;
    
    switch (spec->layout.type) {
        case SCENE_LAYOUT_FLEX_COLUMN:
            layout_column(el, start_x, start_y, &max_x, &max_y);
            // Convert absolute max back to content relative size
            el->content_w = max_x - start_x;
            el->content_h = max_y - start_y;
            break;
        case SCENE_LAYOUT_FLEX_ROW:
            layout_row(el, start_x, start_y, &max_x, &max_y);
            el->content_w = max_x - start_x;
            el->content_h = max_y - start_y;
            break;
        case SCENE_LAYOUT_CANVAS:
            layout_canvas(el, &max_x, &max_y);
            el->content_w = max_x;
            el->content_h = max_y;
            break;
        case SCENE_LAYOUT_SPLIT_H:
            layout_split_h(el, start_x, start_y);
            el->content_w = el->rect.w;
            el->content_h = el->rect.h;
            break;
        case SCENE_LAYOUT_SPLIT_V:
            layout_split_v(el, start_x, start_y);
            el->content_w = el->rect.w;
            el->content_h = el->rect.h;
            break;
        default: break;
    }
}

static void update_screen_rects(SceneNode* el, float parent_x, float parent_y) {
    if (!el) return;
    
    el->screen_rect.x = parent_x + el->rect.x;
    el->screen_rect.y = parent_y + el->rect.y;
    el->screen_rect.w = el->rect.w;
    el->screen_rect.h = el->rect.h;

    for (SceneNode* child = el->first_child; child; child = child->next_sibling) {
        update_screen_rects(child, el->screen_rect.x, el->screen_rect.y);
    }
}

void ui_layout_root(SceneNode* root, float window_w, float window_h, uint64_t frame_number, bool log_debug, UiTextMeasureFunc measure_func, void* measure_data) {
    if (!root) return;
    
    if (root->spec->layout.width < 0) root->rect.w = window_w;
    if (root->spec->layout.height < 0) root->rect.h = window_h;
    
    Rect initial_avail = {0, 0, window_w, window_h};
    layout_recursive(root, initial_avail, frame_number, log_debug, measure_func, measure_data);
    update_screen_rects(root, 0, 0);
}
