#include "ui_input.h"
#include "ui_command_system.h"
#include "foundation/logger/logger.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define UI_MAX_EVENTS 64
#define UI_KEY_BACKSPACE 259
#define UI_SCROLL_SPEED 20.0f
#define UI_DRAG_THRESHOLD_SQ 9.0f

// --- Helper Functions ---

static void push_event(UiInputContext* ctx, UiEventType type, UiElement* target) {
    if (ctx->event_count < UI_MAX_EVENTS) {
        ctx->events[ctx->event_count].type = type;
        ctx->events[ctx->event_count].target = target;
        ctx->event_count++;
    }
}

static void set_field_from_string(void* data, const MetaField* field, const char* val_str) {
    if (field->type == META_TYPE_STRING) {
        meta_set_string(data, field, val_str);
    } else if (field->type == META_TYPE_FLOAT) {
        meta_set_float(data, field, strtof(val_str, NULL));
    } else if (field->type == META_TYPE_INT) {
        meta_set_int(data, field, atoi(val_str));
    }
}

// --- Public API ---

void ui_input_init(UiInputContext* ctx) {
    memset(ctx, 0, sizeof(UiInputContext));
}

bool ui_input_pop_event(UiInputContext* ctx, UiEvent* out_event) {
    if (!ctx || ctx->event_count == 0) return false;
    *out_event = ctx->events[0];
    // Shift remaining events
    for (int i = 0; i < ctx->event_count - 1; ++i) {
        ctx->events[i] = ctx->events[i+1];
    }
    ctx->event_count--;
    return true;
}

// --- Internal Logic Breakdown ---

static UiElement* hit_test_recursive(UiElement* el, float x, float y) {
    if (!el || !el->spec) return NULL;
    
    // Skip hidden or non-interactive (unless specifically handled)
    if (el->flags & UI_FLAG_HIDDEN) return NULL;
    
    // Check clip rect if clipped
    if ((el->flags & UI_FLAG_CLIPPED)) {
        if (x < el->screen_rect.x || x > el->screen_rect.x + el->screen_rect.w ||
            y < el->screen_rect.y || y > el->screen_rect.y + el->screen_rect.h) {
            return NULL;
        }
    }

    // Check children first (reverse order for Z-sorting: last drawn is top)
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

static void update_hover_state(UiInputContext* ctx, UiElement* root, const InputState* input) {
    UiElement* prev_hovered = ctx->hovered;
    ctx->hovered = hit_test_recursive(root, input->mouse_x, input->mouse_y);

    if (prev_hovered && prev_hovered != ctx->hovered) {
        prev_hovered->is_hovered = false;
    }
    if (ctx->hovered) {
        ctx->hovered->is_hovered = true;
    }
}

static void handle_scroll(UiInputContext* ctx, const InputState* input) {
    if (input->scroll_dy == 0.0f && input->scroll_dx == 0.0f) return;

    UiElement* target = ctx->hovered;
    while (target) {
        if (target->flags & UI_FLAG_SCROLLABLE) {
            target->scroll_y -= input->scroll_dy * UI_SCROLL_SPEED; 
            target->scroll_x += input->scroll_dx * UI_SCROLL_SPEED;

            // Clamp Y
            float max_scroll_y = target->content_h - (target->rect.h - target->spec->padding * 2);
            if (max_scroll_y < 0) max_scroll_y = 0;
            if (target->scroll_y < 0) target->scroll_y = 0;
            if (target->scroll_y > max_scroll_y) target->scroll_y = max_scroll_y;

            // Clamp X
            float max_scroll_x = target->content_w - (target->rect.w - target->spec->padding * 2);
            if (max_scroll_x < 0) max_scroll_x = 0;
            if (target->scroll_x < 0) target->scroll_x = 0;
            if (target->scroll_x > max_scroll_x) target->scroll_x = max_scroll_x;
            
            break; // Handled
        }
        target = target->parent;
    }
}

static void handle_mouse_press(UiInputContext* ctx, const InputState* input) {
    if (!input->mouse_clicked) return;

    if (ctx->hovered) {
        ctx->active = ctx->hovered;
        ctx->possible_drag = true;
        ctx->drag_start_mouse_x = input->mouse_x;
        ctx->drag_start_mouse_y = input->mouse_y;
        
        // Cache start values for potential drag
        if (ctx->active->flags & UI_FLAG_SCROLLABLE) {
             ctx->drag_start_elem_x = ctx->active->scroll_x;
             ctx->drag_start_elem_y = ctx->active->scroll_y;
        } else if (ctx->active->flags & UI_FLAG_DRAGGABLE) {
             // Cache bound values
             if (ctx->active->data_ptr) {
                 if (ctx->active->bind_x) {
                     ctx->drag_start_elem_x = meta_get_float(ctx->active->data_ptr, ctx->active->bind_x);
                 }
                 if (ctx->active->bind_y) {
                     ctx->drag_start_elem_y = meta_get_float(ctx->active->data_ptr, ctx->active->bind_y);
                 }
             }
        }

        // Handle Focus
        if (ctx->hovered->flags & UI_FLAG_FOCUSABLE) {
            if (ctx->focused && ctx->focused != ctx->hovered) {
                ctx->focused->is_focused = false;
            }
            ctx->focused = ctx->hovered;
            ctx->focused->is_focused = true;
        } else {
            // Clicked something non-focusable (like a button), lose focus on text field?
            // Usually buttons don't steal focus from text inputs in desktop apps, 
            // but for now let's clear focus to be safe or strict.
            if (ctx->focused) {
                ctx->focused->is_focused = false;
            }
            ctx->focused = NULL;
        }
    } else {
        // Clicked void
        if (ctx->focused) {
            ctx->focused->is_focused = false;
        }
        ctx->focused = NULL;
    }
    
    if (ctx->active) {
        ctx->active->is_active = true;
    }
}

static void handle_keyboard_input(UiInputContext* ctx, const InputState* input) {
    if (!ctx->focused) return;
    
    UiElement* el = ctx->focused;
    if (!((el->flags & UI_FLAG_EDITABLE) && el->spec->kind == UI_KIND_TEXT_INPUT)) return;

    bool changed = false;
    
    // Ensure we have binding
    if (!el->data_ptr || !el->bind_text) return;
    const MetaField* field = el->bind_text;

    // 1. Typing
    if (input->last_char >= 32 && input->last_char <= 126) {
         char buf[256] = {0};
         ui_bind_read_string(el->data_ptr, field, buf, sizeof(buf));
         
         size_t len = strlen(buf);
         if (len < 255) {
             // Simple insertion at end (TODO: Use cursor_idx for insertion)
             buf[len] = (char)input->last_char;
             buf[len+1] = '\0';
             set_field_from_string(el->data_ptr, field, buf);
             el->cursor_idx++;
             changed = true;
         }
    }
    // 2. Backspace
    if (input->last_key == UI_KEY_BACKSPACE && input->last_action != 0) { // UI_KEY_BACKSPACE
         char buf[256] = {0};
         ui_bind_read_string(el->data_ptr, field, buf, sizeof(buf));
         size_t len = strlen(buf);
         if (len > 0) {
             buf[len-1] = '\0';
             set_field_from_string(el->data_ptr, field, buf);
             if (el->cursor_idx > 0) el->cursor_idx--;
             changed = true;
         }
    }
    
    if (changed) {
        push_event(ctx, UI_EVENT_VALUE_CHANGE, el);
        if (el->on_change_cmd_id) {
            ui_command_execute_id(el->on_change_cmd_id, el);
        }
    }
}

static void handle_drag_logic(UiInputContext* ctx, const InputState* input) {
    if (!ctx->active || !input->mouse_down) return;

    // Check start threshold
    if (ctx->possible_drag && !ctx->is_dragging) {
        float dx = input->mouse_x - ctx->drag_start_mouse_x;
        float dy = input->mouse_y - ctx->drag_start_mouse_y;
        if (dx*dx + dy*dy > UI_DRAG_THRESHOLD_SQ) { 
            ctx->is_dragging = true;
            push_event(ctx, UI_EVENT_DRAG_START, ctx->active);
        }
    }

    if (ctx->is_dragging) {
        float dx = input->mouse_x - ctx->drag_start_mouse_x;
        float dy = input->mouse_y - ctx->drag_start_mouse_y;
        bool changed = false;

        // Case A: Draggable Object (updates data model)
        if (ctx->active->flags & UI_FLAG_DRAGGABLE) {
            if (ctx->active->data_ptr) {
                if (ctx->active->bind_x) {
                    meta_set_float(ctx->active->data_ptr, ctx->active->bind_x, ctx->drag_start_elem_x + dx);
                    changed = true;
                }
                if (ctx->active->bind_y) {
                    meta_set_float(ctx->active->data_ptr, ctx->active->bind_y, ctx->drag_start_elem_y + dy);
                    changed = true;
                }
            }
        }
        // Case B: Scrollable (internal state)
        else if (ctx->active->flags & UI_FLAG_SCROLLABLE) {
             ctx->active->scroll_x = ctx->drag_start_elem_x - dx;
             ctx->active->scroll_y = ctx->drag_start_elem_y - dy;
             
             // Clamp
             float max_scroll_y = ctx->active->content_h - (ctx->active->rect.h - ctx->active->spec->padding * 2);
             if (max_scroll_y < 0) max_scroll_y = 0;
             if (ctx->active->scroll_y < 0) ctx->active->scroll_y = 0;
             if (ctx->active->scroll_y > max_scroll_y) ctx->active->scroll_y = max_scroll_y;

             float max_scroll_x = ctx->active->content_w - (ctx->active->rect.w - ctx->active->spec->padding * 2);
             if (max_scroll_x < 0) max_scroll_x = 0;
             if (ctx->active->scroll_x < 0) ctx->active->scroll_x = 0;
             if (ctx->active->scroll_x > max_scroll_x) ctx->active->scroll_x = max_scroll_x;
        }
        
        if (changed) {
            push_event(ctx, UI_EVENT_VALUE_CHANGE, ctx->active);
            if (ctx->active->on_change_cmd_id) {
                ui_command_execute_id(ctx->active->on_change_cmd_id, ctx->active);
            }
        }
    }
}

static void handle_mouse_release(UiInputContext* ctx, const InputState* input) {
    if (input->mouse_down) return; // Only on release

    if (ctx->active) {
        // Click?
        if (ctx->active == ctx->hovered && !ctx->is_dragging) {
            push_event(ctx, UI_EVENT_CLICK, ctx->active);
            if (ctx->active->on_click_cmd_id) {
                ui_command_execute_id(ctx->active->on_click_cmd_id, ctx->active);
            }
        }
        // Drag End?
        if (ctx->is_dragging) {
            push_event(ctx, UI_EVENT_DRAG_END, ctx->active);
        }
        
        ctx->active->is_active = false;
        ctx->active = NULL;
    }
    ctx->is_dragging = false;
    ctx->possible_drag = false;
}

// --- Main Update Loop ---

void ui_input_update(UiInputContext* ctx, UiElement* root, const InputState* input) {
    if (!ctx || !root || !input) return;

    update_hover_state(ctx, root, input);
    handle_scroll(ctx, input);
    handle_mouse_press(ctx, input);
    handle_keyboard_input(ctx, input);
    handle_drag_logic(ctx, input);
    handle_mouse_release(ctx, input);
}
