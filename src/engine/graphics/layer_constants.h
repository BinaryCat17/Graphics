#ifndef LAYER_CONSTANTS_H
#define LAYER_CONSTANTS_H

// --- Orthographic Projection Range ---
// Defined in render_system_begin_frame: mat4_orthographic(..., -100.0f, 100.0f)
// Note: Due to OpenGL->Vulkan clip space differences and the specific projection matrix,
// Visible Z range is effectively [-100.0, 0.0] where:
// Z = 0.0   -> Depth 0.0 (Near / Topmost)
// Z = -100.0 -> Depth 1.0 (Far / Bottommost)
// Therefore, HIGHER Z values (closer to 0) render ON TOP of LOWER Z values.

#define RENDER_ORTHO_Z_NEAR (-100.0f)
#define RENDER_ORTHO_Z_FAR  (100.0f)

// --- UI Depth Layers ---

// The deepest background layer (e.g., Canvas background)
// Start deeper so children can stack on top (towards 0.0)
#define RENDER_LAYER_UI_BASE        (-10.0f)

// Standard UI Panels (Windows, Sidebars)
#define RENDER_LAYER_UI_PANEL       (-9.0f)

// Overlay Elements
#define RENDER_LAYER_UI_OVERLAY     (-1.0f)

// --- Increments ---
// Increment Z (move closer to 0 / positive) for each child
#define RENDER_DEPTH_STEP_UI        (0.01f)
#define RENDER_DEPTH_STEP_CONTENT   (0.001f)

#endif // LAYER_CONSTANTS_H
