#ifndef UI_INPUT_INTERNAL_H
#define UI_INPUT_INTERNAL_H

#include "../ui_input.h" // Public definition of UiEvent
#include "../ui_core.h"

// --- UI Input Context ---
// Stores the persistent state of interaction across frames.

struct UiInputContext {
    // Current Frame State
    SceneNode* hovered;      // Element currently under mouse
    SceneNode* active;       // Element being pressed (mouse down)
    SceneNode* focused;      // Element with keyboard focus

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
};

#endif // UI_INPUT_INTERNAL_H
