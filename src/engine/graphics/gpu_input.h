#ifndef GPU_INPUT_H
#define GPU_INPUT_H

#include <stdint.h>
#include "foundation/math/math_types.h"

// Forward Declaration
typedef struct InputSystem InputSystem;

// Standard layout for GPU Input Uniform Buffer (std140)
// Must be 16-byte aligned.
typedef struct GpuInputState {
    float time;             // 0
    float delta_time;       // 4
    float screen_width;     // 8
    float screen_height;    // 12
    
    Vec2 mouse_pos;         // 16
    Vec2 mouse_delta;       // 24
    
    Vec2 mouse_scroll;      // 32
    uint32_t mouse_buttons; // 40 (Bitmask: 0=Left, 1=Right, 2=Middle)
    uint32_t padding;       // 44
    
    // Total: 48 bytes -> aligned to 64 bytes if needed, or just 48
} GpuInputState;

// Updates the GPU Input State struct from the Engine's InputSystem.
// This does NOT upload to GPU, it just prepares the struct.
void gpu_input_update(GpuInputState* state, const InputSystem* input, float time, float dt, float width, float height);

#endif // GPU_INPUT_H
