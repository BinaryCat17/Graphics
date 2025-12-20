#ifndef ENGINE_INPUT_H
#define ENGINE_INPUT_H

#include <stdint.h>
#include <stdbool.h>

// Forward declarations
typedef struct PlatformWindow PlatformWindow;
typedef struct InputSystem InputSystem; // Opaque handle

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

// --- Lifecycle ---

InputSystem* input_system_create(PlatformWindow* window);
void input_system_destroy(InputSystem* sys);
void input_system_update(InputSystem* sys);

// --- Accessors (State) ---

float input_get_mouse_x(const InputSystem* sys);
float input_get_mouse_y(const InputSystem* sys);
bool input_is_mouse_down(const InputSystem* sys);

// --- Accessors (Events) ---

// Returns the number of events recorded this frame
int input_get_event_count(const InputSystem* sys);

// Returns a pointer to the event at index 'index', or NULL if out of bounds.
// The pointer is valid only until the next update.
const InputEvent* input_get_event(const InputSystem* sys, int index);

#endif // ENGINE_INPUT_H
