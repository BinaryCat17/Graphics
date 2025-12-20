#include "engine/input/input.h"
#include "engine/input/internal/input_internal.h"
#include "foundation/platform/platform.h"
#include "foundation/logger/logger.h"
#include <string.h>
#include <stdlib.h>

// --- Helpers ---

static bool check_modifiers(const InputSystem* sys, int mods) {
    bool shift = sys->state.keys[INPUT_KEY_LEFT_SHIFT] || sys->state.keys[INPUT_KEY_RIGHT_SHIFT];
    bool ctrl = sys->state.keys[INPUT_KEY_LEFT_CONTROL] || sys->state.keys[INPUT_KEY_RIGHT_CONTROL];
    bool alt = sys->state.keys[INPUT_KEY_LEFT_ALT] || sys->state.keys[INPUT_KEY_RIGHT_ALT];
    bool super = sys->state.keys[INPUT_KEY_LEFT_SUPER] || sys->state.keys[INPUT_KEY_RIGHT_SUPER];
    // TODO: Caps/Num lock if needed

    if ((mods & INPUT_MOD_SHIFT) && !shift) return false;
    if ((mods & INPUT_MOD_CONTROL) && !ctrl) return false;
    if ((mods & INPUT_MOD_ALT) && !alt) return false;
    if ((mods & INPUT_MOD_SUPER) && !super) return false;
    
    // Strict modifier check? 
    // Usually if I ask for Ctrl+Z, Ctrl+Shift+Z should NOT trigger it?
    // For now, let's implement strict checking: if mod is NOT requested but IS pressed, fail?
    // Common behavior: simple inclusion. Ctrl+Z usually triggers on Ctrl+Shift+Z unless Shift+Z is also bound.
    // Let's stick to "required modifiers are present".
    
    return true;
}

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
    
    // State Update
    if (key >= 0 && key <= INPUT_KEY_LAST) {
        if (action == PLATFORM_PRESS) {
            sys->state.keys[key] = true;
        } else if (action == PLATFORM_RELEASE) {
            sys->state.keys[key] = false;
        }
        // Repeat does not change boolean state
    }

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
    memcpy(sys->_prev_keys, sys->state.keys, sizeof(sys->state.keys));
}

// --- Action Mapping ---

void input_map_action(InputSystem* sys, const char* action_name, InputKey default_key, int modifiers) {
    if (!sys || !action_name) return;

    StringId id = str_id(action_name);

    // Update existing or add new
    for (int i = 0; i < sys->action_count; ++i) {
        if (sys->actions[i].name_hash == id) {
            sys->actions[i].key = default_key;
            sys->actions[i].mods = modifiers;
            return;
        }
    }

    if (sys->action_count < MAX_ACTIONS) {
        sys->actions[sys->action_count].name_hash = id;
        sys->actions[sys->action_count].key = default_key;
        sys->actions[sys->action_count].mods = modifiers;
        sys->action_count++;
        // LOG_DEBUG("Mapped Action '%s' to Key %d (Mods: %d)", action_name, default_key, modifiers);
    } else {
        LOG_ERROR("Input Action limit reached! Cannot map '%s'", action_name);
    }
}

bool input_is_action_pressed(const InputSystem* sys, const char* action_name) {
    if (!sys || !action_name) return false;
    StringId id = str_id(action_name);

    for (int i = 0; i < sys->action_count; ++i) {
        if (sys->actions[i].name_hash == id) {
            InputKey k = sys->actions[i].key;
            if (k == INPUT_KEY_UNKNOWN) return false; // Unbound
            
            bool key_down = sys->state.keys[k];
            bool mods_ok = check_modifiers(sys, sys->actions[i].mods);
            return key_down && mods_ok;
        }
    }
    return false;
}

bool input_is_action_just_pressed(const InputSystem* sys, const char* action_name) {
    if (!sys || !action_name) return false;
    StringId id = str_id(action_name);

    for (int i = 0; i < sys->action_count; ++i) {
        if (sys->actions[i].name_hash == id) {
            InputKey k = sys->actions[i].key;
            if (k == INPUT_KEY_UNKNOWN) return false;

            bool key_down = sys->state.keys[k];
            bool prev_down = sys->_prev_keys[k];
            bool mods_ok = check_modifiers(sys, sys->actions[i].mods);

            // Note: We don't check if modifiers were "just pressed", usually just the trigger key.
            return key_down && !prev_down && mods_ok;
        }
    }
    return false;
}

bool input_is_action_released(const InputSystem* sys, const char* action_name) {
    if (!sys || !action_name) return false;
    StringId id = str_id(action_name);

    for (int i = 0; i < sys->action_count; ++i) {
        if (sys->actions[i].name_hash == id) {
            InputKey k = sys->actions[i].key;
            if (k == INPUT_KEY_UNKNOWN) return false;

            bool key_down = sys->state.keys[k];
            bool prev_down = sys->_prev_keys[k];
            // Modifiers state at release moment? 
            // Usually we want to know if it WAS pressed and NOW isn't.
            return !key_down && prev_down;
        }
    }
    return false;
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

bool input_is_key_down(const InputSystem* sys, InputKey key) {
    if (!sys || key < 0 || key > INPUT_KEY_LAST) return false;
    return sys->state.keys[key];
}

int input_get_event_count(const InputSystem* sys) {
    return sys ? sys->queue.count : 0;
}

const InputEvent* input_get_event(const InputSystem* sys, int index) {
    if (!sys || index < 0 || index >= sys->queue.count) return NULL;
    return &sys->queue.events[index];
}
