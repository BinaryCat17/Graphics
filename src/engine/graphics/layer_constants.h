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
#define RENDER_LAYER_UI_BASE        (-10.0f)

// Standard UI Panels (Windows, Sidebars) - significantly above the canvas
#define RENDER_LAYER_UI_PANEL       (-5.0f)

// Overlay Elements (Tooltips, Dropdowns, Modals) - on top of everything
#define RENDER_LAYER_UI_OVERLAY     (-1.0f)

// --- Increments ---
// Amount to increment Z for each nested child in the UI tree
#define RENDER_DEPTH_STEP_UI        (0.01f)
// Finer increment for content within the same container (text on button)
#define RENDER_DEPTH_STEP_CONTENT   (0.001f)

// --- Editor Specific Layers (Interleaved) ---
// These are used by the Math Graph Editor to inject elements between UI layers.
// They are relative to RENDER_LAYER_UI_BASE (since the editor lives in a canvas).

// Wires connect nodes. They should be slightly above the canvas background 
// but strictly BEHIND the nodes (which usually start at UI_BASE + nesting).
// Base (-10.0) < Wires (-9.985) < Nodes (-9.90 usually)
#define RENDER_LAYER_EDITOR_WIRE    (RENDER_LAYER_UI_BASE + 0.015f) // -9.985

// Ports (Connection dots) should be above wires but behind the node body?
// Actually, ports are usually children of the node, so they inherit node depth.
// However, in the current implementation, they might be drawn separately.
// Existing code used -9.97.
#define RENDER_LAYER_EDITOR_PORT    (RENDER_LAYER_UI_BASE + 0.030f) // -9.97

#endif // LAYER_CONSTANTS_H
