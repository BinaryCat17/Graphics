#ifndef ENGINE_INPUT_TYPES_H
#define ENGINE_INPUT_TYPES_H

#include <stdint.h>
#include <stdbool.h>

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
