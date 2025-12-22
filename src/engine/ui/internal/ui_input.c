#include "ui_input.h"
#include "ui_command_system.h"
#include "ui_internal.h"
#include "foundation/logger/logger.h"
#include "foundation/meta/reflection.h"
#include "engine/input/input.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define UI_MAX_EVENTS 64
#define UI_SCROLL_SPEED 20.0f
#define UI_DRAG_THRESHOLD_SQ 9.0f

// ... Helper Functions ...

static void push_event(UiInputContext* ctx, UiEventType type, UiElement* target) {
    if (ctx->event_count < UI_MAX_EVENTS) {
        ctx->events[ctx->event_count].type = type;
        ctx->events[ctx->event_count].target = target;
        ctx->event_count++;
    }
}

// --- Public API ---

void ui_input_init(UiInputContext* ctx) {
    memset(ctx, 0, sizeof(UiInputContext));
}

UiInputContext* ui_input_create(void) {
    UiInputContext* ctx = (UiInputContext*)calloc(1, sizeof(UiInputContext));
    if (ctx) ui_input_init(ctx);
    return ctx;
}

void ui_input_destroy(UiInputContext* ctx) {
    free(ctx);
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
    for (UiElement* child = el->last_child; child; child = child->prev_sibling) {
        UiElement* hit = hit_test_recursive(child, x, y);
        if (hit) return hit;
    }

    // Check Self
    if (x >= el->screen_rect.x && x <= el->screen_rect.x + el->screen_rect.w &&
        y >= el->screen_rect.y && y <= el->screen_rect.y + el->screen_rect.h) {
        return el;
    }

    return NULL;
}

static void update_hover_state(UiInputContext* ctx, UiElement* root, const InputSystem* input) {
    UiElement* prev_hovered = ctx->hovered;
    float mx = input_get_mouse_x(input);
    float my = input_get_mouse_y(input);
    
    ctx->hovered = hit_test_recursive(root, mx, my);

    if (prev_hovered && prev_hovered != ctx->hovered) {
        prev_hovered->is_hovered = false;
    }
    if (ctx->hovered) {
        ctx->hovered->is_hovered = true;
    }
}

static void handle_scroll_event(UiInputContext* ctx, const InputEvent* event) {
    if (event->type != INPUT_EVENT_SCROLL) return;
    float dx = event->data.scroll.dx;
    float dy = event->data.scroll.dy;

    UiElement* target = ctx->hovered;
    while (target) {
        if (target->flags & UI_FLAG_SCROLLABLE) {
            target->scroll_y -= dy * UI_SCROLL_SPEED; 
            target->scroll_x += dx * UI_SCROLL_SPEED;

            // Clamp Y
            float max_scroll_y = target->content_h - (target->rect.h - target->spec->layout.padding * 2);
            if (max_scroll_y < 0) max_scroll_y = 0;
            if (target->scroll_y < 0) target->scroll_y = 0;
            if (target->scroll_y > max_scroll_y) target->scroll_y = max_scroll_y;

            // Clamp X
            float max_scroll_x = target->content_w - (target->rect.w - target->spec->layout.padding * 2);
            if (max_scroll_x < 0) max_scroll_x = 0;
            if (target->scroll_x < 0) target->scroll_x = 0;
            if (target->scroll_x > max_scroll_x) target->scroll_x = max_scroll_x;
            
            break; // Handled
        }
        target = target->parent;
    }
}

static void handle_mouse_press_event(UiInputContext* ctx, const InputEvent* event) {
    if (event->type != INPUT_EVENT_MOUSE_PRESSED) return;
    if (event->data.mouse_button.button != 0) return; // Left click only for now

    float mx = event->data.mouse_button.x;
    float my = event->data.mouse_button.y;

    if (ctx->hovered) {
        ctx->active = ctx->hovered;
        ctx->possible_drag = true;
        ctx->drag_start_mouse_x = mx;
        ctx->drag_start_mouse_y = my;
        
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

static void handle_char_event(UiInputContext* ctx, const InputEvent* event) {
    if (!ctx->focused) return;
    UiElement* el = ctx->focused;
    if (!(el->flags & UI_FLAG_EDITABLE)) return;

    if (event->type == INPUT_EVENT_CHAR) {
         char buf[256] = {0};
         if (el->data_ptr && el->bind_text) {
             ui_bind_read_string(el->data_ptr, el->bind_text, buf, sizeof(buf));
             
             size_t len = strlen(buf);
             if (len < 255) {
                 buf[len] = (char)event->data.character.codepoint;
                 buf[len+1] = '\0';
                 meta_set_from_string(el->data_ptr, el->bind_text, buf);
                 el->cursor_idx++;
                 
                 push_event(ctx, UI_EVENT_VALUE_CHANGE, el);
                 if (el->on_change_cmd_id) {
                     ui_command_execute_id(el->on_change_cmd_id, el);
                 }
             }
         }
    }
}

static void handle_key_event(UiInputContext* ctx, const InputEvent* event) {
    if (!ctx->focused) return;
    UiElement* el = ctx->focused;
    if (!(el->flags & UI_FLAG_EDITABLE)) return;

    if (event->type == INPUT_EVENT_KEY_PRESSED || event->type == INPUT_EVENT_KEY_REPEAT) {
        if (event->data.key.key == INPUT_KEY_BACKSPACE) {
             char buf[256] = {0};
             if (el->data_ptr && el->bind_text) {
                 ui_bind_read_string(el->data_ptr, el->bind_text, buf, sizeof(buf));
                 size_t len = strlen(buf);
                 if (len > 0) {
                     buf[len-1] = '\0';
                     meta_set_from_string(el->data_ptr, el->bind_text, buf);
                     if (el->cursor_idx > 0) el->cursor_idx--;
                     
                     push_event(ctx, UI_EVENT_VALUE_CHANGE, el);
                     if (el->on_change_cmd_id) {
                         ui_command_execute_id(el->on_change_cmd_id, el);
                     }
                 }
             }
        }
    }
}

static void handle_drag_logic(UiInputContext* ctx, const InputSystem* input) {
    if (!ctx->active || !input_is_mouse_down(input)) return;

    float mx = input_get_mouse_x(input);
    float my = input_get_mouse_y(input);

    // Check start threshold
    if (ctx->possible_drag && !ctx->is_dragging) {
        float dx = mx - ctx->drag_start_mouse_x;
        float dy = my - ctx->drag_start_mouse_y;
        if (dx*dx + dy*dy > UI_DRAG_THRESHOLD_SQ) { 
            ctx->is_dragging = true;
            push_event(ctx, UI_EVENT_DRAG_START, ctx->active);
        }
    }

    if (ctx->is_dragging) {
        float dx = mx - ctx->drag_start_mouse_x;
        float dy = my - ctx->drag_start_mouse_y;
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
             float max_scroll_y = ctx->active->content_h - (ctx->active->rect.h - ctx->active->spec->layout.padding * 2);
             if (max_scroll_y < 0) max_scroll_y = 0;
             if (ctx->active->scroll_y < 0) ctx->active->scroll_y = 0;
             if (ctx->active->scroll_y > max_scroll_y) ctx->active->scroll_y = max_scroll_y;

             float max_scroll_x = ctx->active->content_w - (ctx->active->rect.w - ctx->active->spec->layout.padding * 2);
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

static void handle_mouse_release_event(UiInputContext* ctx, const InputEvent* event) {
    if (event->type != INPUT_EVENT_MOUSE_RELEASED) return;

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

void ui_input_update(UiInputContext* ctx, UiElement* root, const InputSystem* input) {
    if (!ctx || !root || !input) return;

    update_hover_state(ctx, root, input);
    
    // Process Events
    int count = input_get_event_count(input);
    for (int i = 0; i < count; ++i) {
        const InputEvent* e = input_get_event(input, i);
        if (!e) continue;

        switch (e->type) {
            case INPUT_EVENT_SCROLL: handle_scroll_event(ctx, e); break;
            case INPUT_EVENT_MOUSE_PRESSED: handle_mouse_press_event(ctx, e); break;
            case INPUT_EVENT_MOUSE_RELEASED: handle_mouse_release_event(ctx, e); break;
            case INPUT_EVENT_CHAR: handle_char_event(ctx, e); break;
            case INPUT_EVENT_KEY_PRESSED: 
            case INPUT_EVENT_KEY_REPEAT: 
                handle_key_event(ctx, e); 
                break;
            default: break;
        }
    }
    
    // Continuous Drag Logic
    handle_drag_logic(ctx, input);
}
