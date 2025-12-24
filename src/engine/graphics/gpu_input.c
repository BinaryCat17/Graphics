#include "gpu_input.h"
#include "engine/input/input.h"

void gpu_input_update(GpuInputState* state, const InputSystem* input, float time, float dt, float width, float height) {
    if (!state || !input) return;

    state->time = time;
    state->delta_time = dt;
    state->screen_width = width;
    state->screen_height = height;

    state->mouse_pos.x = input_get_mouse_x(input);
    state->mouse_pos.y = input_get_mouse_y(input);

    // Delta and Scroll would require tracking previous frame or accumulating events
    // For now we set them to 0 or implement a more complex accumulation logic if InputSystem provides it.
    // Assuming InputSystem accumulates frame events:
    state->mouse_delta = (Vec2){0, 0}; // TODO: Retrieve from InputSystem if available
    state->mouse_scroll = (Vec2){0, 0}; // TODO: Retrieve from InputSystem

    state->mouse_buttons = 0;
    if (input_is_mouse_down(input)) { // Checks any button, we need specific
        // TODO: Update InputSystem to expose specific buttons or iterate events
        // Temporary: assume left button if any is down
        state->mouse_buttons |= 1; 
    }
}
