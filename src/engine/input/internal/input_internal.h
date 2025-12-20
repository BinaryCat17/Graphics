#ifndef ENGINE_INPUT_INTERNAL_H
#define ENGINE_INPUT_INTERNAL_H

#include "engine/input/input.h"

// Internal Event Queue
typedef struct InputEventQueue {
    InputEvent events[MAX_INPUT_EVENTS];
    int count;
} InputEventQueue;

// Internal State (Polling)
typedef struct InputState {
    float mouse_x, mouse_y;
    double last_scroll_y;
    bool mouse_down;
} InputState;

// Full System Definition
typedef struct InputSystem {
    InputState state;
    InputEventQueue queue;
    
    // Internal logic
    bool _prev_mouse_down;
} InputSystem;

#endif // ENGINE_INPUT_INTERNAL_H
