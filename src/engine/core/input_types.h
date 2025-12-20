#ifndef ENGINE_INPUT_TYPES_H
#define ENGINE_INPUT_TYPES_H

#include <stdint.h>
#include <stdbool.h>

// Event-based Input System
typedef enum InputEventType {
    INPUT_EVENT_NONE = 0,
    INPUT_EVENT_KEY_PRESSED,
    INPUT_EVENT_KEY_RELEASED,
    INPUT_EVENT_KEY_REPEAT,
    INPUT_EVENT_MOUSE_MOVED,
    INPUT_EVENT_MOUSE_PRESSED,
    INPUT_EVENT_MOUSE_RELEASED,
    INPUT_EVENT_SCROLL,
    INPUT_EVENT_CHAR
} InputEventType;

typedef struct InputEvent {
    InputEventType type;
    union {
        struct {
            int key;
            int scancode;
            int mods;
        } key;
        struct {
            float x, y;
        } mouse;
        struct {
            int button;
            int mods;
            float x, y;
        } mouse_button;
        struct {
            float dx, dy;
        } scroll;
        struct {
            uint32_t codepoint;
        } character;
    } data;
} InputEvent;

#define MAX_INPUT_EVENTS 256

typedef struct InputEventQueue {
    InputEvent events[MAX_INPUT_EVENTS];
    int count;
} InputEventQueue;

// Legacy / Continuous State (Polling)
typedef struct InputState {
    float mouse_x, mouse_y;
    bool mouse_down;
    bool mouse_clicked;
    float scroll_dx, scroll_dy;
    
    // Keyboard
    uint32_t last_char; // Unicode codepoint
    int last_key;       // Platform key code
    int last_action;    // Press/Release/Repeat
} InputState;

#endif // ENGINE_INPUT_TYPES_H
