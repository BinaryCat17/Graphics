#ifndef ENGINE_INPUT_INTERNAL_H
#define ENGINE_INPUT_INTERNAL_H

#include "engine/input/input.h"
#include "foundation/string/string_id.h"

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
    
    // Key state for the current frame
    bool keys[INPUT_KEY_LAST + 1];
} InputState;

typedef struct ActionMapping {
    StringId name_hash;
    InputKey key;
    int mods;
} ActionMapping;

#define MAX_ACTIONS 128

// Full System Definition
struct InputSystem {
    InputState state;
    InputEventQueue queue;
    
    // Internal logic
    bool _prev_mouse_down;
    bool _prev_keys[INPUT_KEY_LAST + 1];

    // Action Mappings
    ActionMapping actions[MAX_ACTIONS];
    int action_count;
};

#endif // ENGINE_INPUT_INTERNAL_H
