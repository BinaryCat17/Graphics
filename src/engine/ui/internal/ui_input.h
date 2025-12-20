#ifndef UI_INPUT_H
#define UI_INPUT_H

#include "../ui_core.h"

// --- UI Input Context ---
// Stores the persistent state of interaction across frames.

typedef struct UiInputContext {
    // Current Frame State
    UiElement* hovered;      // Element currently under mouse
    UiElement* active;       // Element being pressed (mouse down)
    UiElement* focused;      // Element with keyboard focus

    // Dragging State
    bool is_dragging;
    float drag_start_mouse_x;
    float drag_start_mouse_y;
    float drag_start_elem_x; // Element's cached value at start of drag
    float drag_start_elem_y;

    // Helper to detect click vs drag
    bool possible_drag; 

    // Event Queue
    UiEvent events[64];
    int event_count;
} UiInputContext;

void ui_input_init(UiInputContext* ctx);
void ui_input_update(UiInputContext* ctx, UiElement* root, const InputSystem* input);
bool ui_input_pop_event(UiInputContext* ctx, UiEvent* out_event);

#endif // UI_INPUT_H
