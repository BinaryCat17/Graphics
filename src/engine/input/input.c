#include "engine/input/input.h"
#include "engine/input/internal/input_internal.h"
#include "foundation/platform/platform.h"
#include <string.h>
#include <stdlib.h>

// --- Input Callbacks ---

static void push_event(InputSystem* sys, InputEvent event) {
    if (sys->queue.count < MAX_INPUT_EVENTS) {
        sys->queue.events[sys->queue.count++] = event;
    }
}

static void on_mouse_button(PlatformWindow* window, PlatformMouseButton button, PlatformInputAction action, int mods, void* user_data) {
    (void)window;
    InputSystem* sys = (InputSystem*)user_data;
    if (!sys) return;
    
    // State
    if (button == PLATFORM_MOUSE_BUTTON_LEFT) {
        if (action == PLATFORM_PRESS) {
            sys->state.mouse_down = true;
        } else if (action == PLATFORM_RELEASE) {
            sys->state.mouse_down = false;
        }
    }

    // Event
    InputEvent event = {0};
    if (action == PLATFORM_PRESS) event.type = INPUT_EVENT_MOUSE_PRESSED;
    else if (action == PLATFORM_RELEASE) event.type = INPUT_EVENT_MOUSE_RELEASED;
    
    if (event.type != INPUT_EVENT_NONE) {
        event.data.mouse_button.button = (int)button;
        event.data.mouse_button.mods = mods;
        event.data.mouse_button.x = sys->state.mouse_x;
        event.data.mouse_button.y = sys->state.mouse_y;
        push_event(sys, event);
    }
}

static void on_scroll(PlatformWindow* window, double xoff, double yoff, void* user_data) {
    (void)window;
    InputSystem* sys = (InputSystem*)user_data;
    if (!sys) return;

    // Event
    InputEvent event = {0};
    event.type = INPUT_EVENT_SCROLL;
    event.data.scroll.dx = (float)xoff;
    event.data.scroll.dy = (float)yoff;
    push_event(sys, event);
}

static void on_key(PlatformWindow* window, int key, int scancode, PlatformInputAction action, int mods, void* user_data) {
    (void)window;
    InputSystem* sys = (InputSystem*)user_data;
    if (!sys) return;
    
    // Event
    InputEvent event = {0};
    if (action == PLATFORM_PRESS) event.type = INPUT_EVENT_KEY_PRESSED;
    else if (action == PLATFORM_RELEASE) event.type = INPUT_EVENT_KEY_RELEASED;
    else if (action == PLATFORM_REPEAT) event.type = INPUT_EVENT_KEY_REPEAT;

    if (event.type != INPUT_EVENT_NONE) {
        event.data.key.key = key;
        event.data.key.scancode = scancode;
        event.data.key.mods = mods;
        push_event(sys, event);
    }
}

static void on_char(PlatformWindow* window, unsigned int codepoint, void* user_data) {
    (void)window;
    InputSystem* sys = (InputSystem*)user_data;
    if (!sys) return;
    
    // Event
    InputEvent event = {0};
    event.type = INPUT_EVENT_CHAR;
    event.data.character.codepoint = codepoint;
    push_event(sys, event);
}

static void on_cursor_pos(PlatformWindow* window, double x, double y, void* user_data) {
    (void)window;
    InputSystem* sys = (InputSystem*)user_data;
    if (!sys) return;

    // State
    sys->state.mouse_x = (float)x;
    sys->state.mouse_y = (float)y;

    // Event
    InputEvent event = {0};
    event.type = INPUT_EVENT_MOUSE_MOVED;
    event.data.mouse.x = (float)x;
    event.data.mouse.y = (float)y;
    push_event(sys, event);
}

// --- Public API ---

InputSystem* input_system_create(PlatformWindow* window) {
    if (!window) return NULL;
    
    InputSystem* sys = (InputSystem*)calloc(1, sizeof(InputSystem));
    if (!sys) return NULL;

    // Register Callbacks with 'sys' as user_data
    platform_set_mouse_button_callback(window, on_mouse_button, sys);
    platform_set_scroll_callback(window, on_scroll, sys);
    platform_set_key_callback(window, on_key, sys);
    platform_set_char_callback(window, on_char, sys);
    platform_set_cursor_pos_callback(window, on_cursor_pos, sys);

    return sys;
}

void input_system_destroy(InputSystem* sys) {
    if (sys) {
        free(sys);
    }
}

void input_system_update(InputSystem* sys) {
    if (!sys) return;

    // Reset per-frame input
    sys->queue.count = 0;
    
    sys->_prev_mouse_down = sys->state.mouse_down;
}

// --- Accessors ---

float input_get_mouse_x(const InputSystem* sys) {
    return sys ? sys->state.mouse_x : 0.0f;
}

float input_get_mouse_y(const InputSystem* sys) {
    return sys ? sys->state.mouse_y : 0.0f;
}

bool input_is_mouse_down(const InputSystem* sys) {
    return sys ? sys->state.mouse_down : false;
}

int input_get_event_count(const InputSystem* sys) {
    return sys ? sys->queue.count : 0;
}

const InputEvent* input_get_event(const InputSystem* sys, int index) {
    if (!sys || index < 0 || index >= sys->queue.count) return NULL;
    return &sys->queue.events[index];
}