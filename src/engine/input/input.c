#include "engine/input/input.h"
#include "foundation/platform/platform.h"
#include <string.h>

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
    
    // Legacy State
    if (button == PLATFORM_MOUSE_BUTTON_LEFT) {
        if (action == PLATFORM_PRESS) {
            sys->state.mouse_down = true;
            sys->state.mouse_clicked = true;
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

    // Legacy State
    sys->state.scroll_dx += (float)xoff;
    sys->state.scroll_dy += (float)yoff;

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
    
    // Legacy State
    sys->state.last_key = key;
    sys->state.last_action = (int)action;

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
    
    // Legacy State
    sys->state.last_char = codepoint;

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

    // Legacy State
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

void input_system_init(InputSystem* sys, PlatformWindow* window) {
    if (!sys || !window) return;
    
    memset(sys, 0, sizeof(InputSystem));
    sys->state.last_action = -1;

    // Register Callbacks with 'sys' as user_data
    platform_set_mouse_button_callback(window, on_mouse_button, sys);
    platform_set_scroll_callback(window, on_scroll, sys);
    platform_set_key_callback(window, on_key, sys);
    platform_set_char_callback(window, on_char, sys);
    platform_set_cursor_pos_callback(window, on_cursor_pos, sys);
}

void input_system_update(InputSystem* sys) {
    if (!sys) return;

    // Reset per-frame input deltas
    sys->state.scroll_dx = 0;
    sys->state.scroll_dy = 0;
    sys->state.last_char = 0;
    sys->state.last_key = 0;
    sys->state.last_action = -1;
    sys->queue.count = 0;
    
    // Toggle Logic for Click
    bool current_down = sys->state.mouse_down;
    sys->state.mouse_clicked = current_down && !sys->_prev_mouse_down;
    sys->_prev_mouse_down = current_down;
}
