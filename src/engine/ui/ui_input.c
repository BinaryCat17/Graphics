#include "ui_input.h"
#include "foundation/logger/logger.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// --- Helpers ---

static void set_field_from_string(void* data, const MetaField* field, const char* val_str) {
    if (field->type == META_TYPE_STRING) {
        meta_set_string(data, field, val_str);
    } else if (field->type == META_TYPE_FLOAT) {
        meta_set_float(data, field, strtof(val_str, NULL));
    } else if (field->type == META_TYPE_INT) {
        meta_set_int(data, field, atoi(val_str));
    }
}

void ui_input_init(UiInputContext* ctx) {
    memset(ctx, 0, sizeof(UiInputContext));
}

void ui_input_reset(UiInputContext* ctx) {
    if (!ctx) return;
    ctx->hovered = NULL;
    ctx->active = NULL;
    ctx->focused = NULL;
    ctx->possible_drag = false;
    ctx->is_dragging = false;
    // Don't reset text input buffers if we want to keep them? 
    // Actually if UI is rebuilt, the element is gone, so focus is invalid.
}

// --- Hit Testing ---

static UiElement* hit_test_recursive(UiElement* el, float x, float y) {
    if (!el || !el->spec) return NULL;
    
    // Skip hidden or non-interactive (optional: maybe we want to hover non-interactive?)
    // For now, only skip if explicitly hidden.
    if (el->spec->flags & UI_FLAG_HIDDEN) return NULL;
    
    // Check clip rect if clipped
    if ((el->spec->flags & UI_FLAG_CLIPPED)) {
        // If point is outside screen_rect, we can't hit children inside?
        // Actually, if clipped, children are invisible outside.
        // So we strictly check bounds.
        if (x < el->screen_rect.x || x > el->screen_rect.x + el->screen_rect.w ||
            y < el->screen_rect.y || y > el->screen_rect.y + el->screen_rect.h) {
            return NULL;
        }
    }

    // Check children first (Render order is usually back-to-front, so last child is on top)
    // We iterate backwards.
    for (int i = (int)el->child_count - 1; i >= 0; --i) {
        UiElement* hit = hit_test_recursive(el->children[i], x, y);
        if (hit) return hit;
    }

    // Check Self
    if (x >= el->screen_rect.x && x <= el->screen_rect.x + el->screen_rect.w &&
        y >= el->screen_rect.y && y <= el->screen_rect.y + el->screen_rect.h) {
        return el;
    }

    return NULL;
}

// --- Logic ---

void ui_input_update(UiInputContext* ctx, UiElement* root, const InputState* input) {
    if (!ctx || !root || !input) return;

    // 1. Hit Test
    UiElement* prev_hovered = ctx->hovered;
    ctx->hovered = hit_test_recursive(root, input->mouse_x, input->mouse_y);

    // Update Hover Flags
    if (prev_hovered && prev_hovered != ctx->hovered) {
        prev_hovered->is_hovered = false;
    }
    if (ctx->hovered) {
        ctx->hovered->is_hovered = true;
    }
    
    // 1.5 Scrolling
    // If we have scroll input, find the nearest scrollable parent starting from hovered
    if (input->scroll_dy != 0.0f || input->scroll_dx != 0.0f) {
        UiElement* scroll_target = ctx->hovered;
        while (scroll_target) {
            if (scroll_target->spec->flags & UI_FLAG_SCROLLABLE) {
                // Apply Scroll
                scroll_target->scroll_y -= input->scroll_dy * 20.0f; // Speed factor
                scroll_target->scroll_x += input->scroll_dx * 20.0f; // Shift + Wheel usually maps to X? Or just direct support.

                // Clamp Y
                float max_scroll_y = scroll_target->content_h - (scroll_target->rect.h - scroll_target->spec->padding * 2);
                if (max_scroll_y < 0) max_scroll_y = 0;
                
                if (scroll_target->scroll_y < 0) scroll_target->scroll_y = 0;
                if (scroll_target->scroll_y > max_scroll_y) scroll_target->scroll_y = max_scroll_y;

                // Clamp X
                float max_scroll_x = scroll_target->content_w - (scroll_target->rect.w - scroll_target->spec->padding * 2);
                if (max_scroll_x < 0) max_scroll_x = 0;

                if (scroll_target->scroll_x < 0) scroll_target->scroll_x = 0;
                if (scroll_target->scroll_x > max_scroll_x) scroll_target->scroll_x = max_scroll_x;
                
                break;
            }
            scroll_target = scroll_target->parent;
        }
    }

    // 2. Mouse Press (Activation & Focus)
    if (input->mouse_clicked) {
        if (ctx->hovered) {
            ctx->active = ctx->hovered;
            ctx->possible_drag = true;
            ctx->drag_start_mouse_x = input->mouse_x;
            ctx->drag_start_mouse_y = input->mouse_y;
            
            // Capture initial element state
            if (ctx->active->spec->flags & UI_FLAG_SCROLLABLE) {
                 ctx->drag_start_elem_x = ctx->active->scroll_x;
                 ctx->drag_start_elem_y = ctx->active->scroll_y;
            }

            // Update Focus
            if (ctx->hovered->spec->flags & UI_FLAG_FOCUSABLE) {
                if (ctx->focused && ctx->focused != ctx->hovered) {
                    ctx->focused->is_focused = false;
                }
                ctx->focused = ctx->hovered;
                ctx->focused->is_focused = true;
            } else {
                if (ctx->focused) {
                    ctx->focused->is_focused = false;
                }
                ctx->focused = NULL;
            }
        } else {
            // Clicked empty space
            if (ctx->focused) {
                ctx->focused->is_focused = false;
            }
            ctx->focused = NULL;
        }
    }
    
    // Update is_active (Pressed state)
    if (ctx->active) {
        ctx->active->is_active = true;
    }

    // 2.5 Keyboard Events (Routed to Focused Element)
    if (ctx->focused) {
        UiElement* el = ctx->focused;
        // Text Input Logic
        if ((el->spec->flags & UI_FLAG_EDITABLE) && el->spec->kind == UI_KIND_TEXT_INPUT) {
            
            // 1. Typing (Printable chars)
            if (input->last_char >= 32 && input->last_char <= 126) {
                if (el->data_ptr && el->meta && el->spec->text_source) {
                    const MetaField* field = meta_find_field(el->meta, el->spec->text_source);
                    if (field) {
                         char buf[256] = {0};
                         ui_bind_read_string(el->data_ptr, field, buf, sizeof(buf));
                         
                         size_t len = strlen(buf);
                         if (len < 255) {
                             buf[len] = (char)input->last_char;
                             buf[len+1] = '\0';
                             
                             set_field_from_string(el->data_ptr, field, buf);
                             el->cursor_idx++;
                         }
                    }
                }
            }
            
            // 2. Backspace (Key 259 is GLFW_KEY_BACKSPACE)
            if (input->last_key == 259 && input->last_action != 0) { // Press or Repeat
                if (el->data_ptr && el->meta && el->spec->text_source) {
                    const MetaField* field = meta_find_field(el->meta, el->spec->text_source);
                    if (field) {
                         char buf[256] = {0};
                         ui_bind_read_string(el->data_ptr, field, buf, sizeof(buf));

                         size_t len = strlen(buf);
                         if (len > 0) {
                             buf[len-1] = '\0';
                             
                             set_field_from_string(el->data_ptr, field, buf);
                             if (el->cursor_idx > 0) el->cursor_idx--;
                         }
                    }
                }
            }
        }
    }

    // 3. Mouse Release
    if (!input->mouse_down) {
        if (ctx->active) {
            // Click Event could fire here if we stayed on same element
            ctx->active->is_active = false;
            ctx->active = NULL;
        }
        ctx->is_dragging = false;
        ctx->possible_drag = false;
    }

    // 4. Drag Logic
    if (ctx->active && input->mouse_down) {
        // Check drag threshold
        if (ctx->possible_drag && !ctx->is_dragging) {
            float dx = input->mouse_x - ctx->drag_start_mouse_x;
            float dy = input->mouse_y - ctx->drag_start_mouse_y;
            if (dx*dx + dy*dy > 9.0f) { // 3px threshold
                ctx->is_dragging = true;
                
                // Cache initial data values for precise delta application
                // We need to read reflection data here.
                if (ctx->active->data_ptr && ctx->active->meta) {
                    // Try to read X
                    if (ctx->active->spec->x_source) {
                        const MetaField* fx = meta_find_field(ctx->active->meta, ctx->active->spec->x_source);
                        ctx->drag_start_elem_x = meta_get_float(ctx->active->data_ptr, fx);
                    }
                    // Try to read Y
                    if (ctx->active->spec->y_source) {
                        const MetaField* fy = meta_find_field(ctx->active->meta, ctx->active->spec->y_source);
                        ctx->drag_start_elem_y = meta_get_float(ctx->active->data_ptr, fy);
                    }
                }
            }
        }

        // Apply Drag
        if (ctx->is_dragging) {
            float dx = input->mouse_x - ctx->drag_start_mouse_x;
            float dy = input->mouse_y - ctx->drag_start_mouse_y;

            // Case A: Draggable Object (Movement)
            if (ctx->active->spec->flags & UI_FLAG_DRAGGABLE) {
                // Write back to reflection
                if (ctx->active->data_ptr && ctx->active->meta) {
                    if (ctx->active->spec->x_source) {
                        const MetaField* fx = meta_find_field(ctx->active->meta, ctx->active->spec->x_source);
                        meta_set_float(ctx->active->data_ptr, fx, ctx->drag_start_elem_x + dx);
                    }
                    if (ctx->active->spec->y_source) {
                        const MetaField* fy = meta_find_field(ctx->active->meta, ctx->active->spec->y_source);
                        meta_set_float(ctx->active->data_ptr, fy, ctx->drag_start_elem_y + dy);
                    }
                }
            }
            // Case B: Scrollable Container (Pan)
            else if (ctx->active->spec->flags & UI_FLAG_SCROLLABLE) {
                 // Drag UP moves scroll DOWN (positive). 
                 // If I drag mouse UP (-dy), I want to see content below, so scroll Y increases? 
                 // No, standard Pan: Drag UP means "pull paper up", so content moves up.
                 // Scroll Y is "offset from top". 
                 // If I drag UP (-dy), content moves UP. 
                 // Content Y = Start - Scroll.
                 // New Content Y = Start - (Scroll + delta).
                 // We want content to follow mouse.
                 // Mouse moved -10. Content should move -10.
                 // -Scroll_new = -Scroll_old - 10 => Scroll_new = Scroll_old + 10.
                 // Wait.
                 // Mouse moves -10 (Up). 
                 // Visual: Paper moves Up.
                 // Top of paper (y=0) becomes y=-10.
                 // Layout: y = start_y - scroll. 
                 // -10 = 0 - scroll => scroll = 10.
                 // So Scroll should INCREASE by -dy.
                 // Let's test: dy = -10. scroll += 10. Correct.
                 
                 ctx->active->scroll_x = ctx->drag_start_elem_x - dx;
                 ctx->active->scroll_y = ctx->drag_start_elem_y - dy;
                 
                 // Clamp again
                 float max_scroll_y = ctx->active->content_h - (ctx->active->rect.h - ctx->active->spec->padding * 2);
                 if (max_scroll_y < 0) max_scroll_y = 0;
                 if (ctx->active->scroll_y < 0) ctx->active->scroll_y = 0;
                 if (ctx->active->scroll_y > max_scroll_y) ctx->active->scroll_y = max_scroll_y;

                 float max_scroll_x = ctx->active->content_w - (ctx->active->rect.w - ctx->active->spec->padding * 2);
                 if (max_scroll_x < 0) max_scroll_x = 0;
                 if (ctx->active->scroll_x < 0) ctx->active->scroll_x = 0;
                 if (ctx->active->scroll_x > max_scroll_x) ctx->active->scroll_x = max_scroll_x;
            }
        }
    }
}