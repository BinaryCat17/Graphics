#include "engine/graphics/gpu_input.h"
#include "engine/input/input.h"
#include <string.h>

void gpu_input_update(GpuInputState* state, const InputSystem* input, float time, float dt, float width, float height) {
    if (!state || !input) return;

    state->time = time;
    state->delta_time = dt;
    state->screen_width = width;
    state->screen_height = height;

    state->mouse_pos.x = input_get_mouse_x(input);
    state->mouse_pos.y = input_get_mouse_y(input);

    input_get_mouse_delta(input, &state->mouse_delta.x, &state->mouse_delta.y);
    input_get_scroll(input, &state->mouse_scroll.x, &state->mouse_scroll.y);
    
    state->mouse_buttons = input_get_mouse_buttons(input);
    state->padding = 0;
}
