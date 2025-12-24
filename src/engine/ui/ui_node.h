#ifndef UI_NODE_H
#define UI_NODE_H

#include "foundation/math/coordinate_systems.h"
#include "foundation/math/math_types.h"
#include <stdint.h>
#include <stddef.h>

// UI Render Flags
typedef enum UiRenderFlags {
    UI_RENDER_FLAG_NONE       = 0,
    UI_RENDER_FLAG_TEXT       = 1 << 0,
    UI_RENDER_FLAG_HAS_BG     = 1 << 1,
    UI_RENDER_FLAG_HAS_BORDER = 1 << 2,
    UI_RENDER_FLAG_ROUNDED    = 1 << 3,
    UI_RENDER_FLAG_CLIPPED    = 1 << 4,
    UI_RENDER_FLAG_TEXTURED   = 1 << 5,
    UI_RENDER_FLAG_9_SLICE    = 1 << 6
} UiRenderFlags;

// Represents a single drawable UI element for the current frame.
// Produced by the Layout/Update step, consumed by the Renderer.
typedef struct UiNode {
    // Spatial (Screen Space)
    Rect rect;       // x, y, w, h
    Rect clip_rect;  // Scissor bounds
    float z_index;   // Depth

    // Appearance
    Vec4 color;          // Background or Tint
    Vec4 border_color;   
    Vec4 text_color;

    // Styling Params
    float corner_radius;
    float border_width;
    
    // Texture / Image (if UI_RENDER_FLAG_TEXTURED or 9_SLICE)
    Vec4 uv_rect;        // UV coordinates
    Vec2 texture_size;   // For 9-slice calculations
    Vec4 slice_borders;  // For 9-slice (top, right, bottom, left)

    // Text Content (if UI_RENDER_FLAG_TEXT)
    const char* text;
    float text_scale;

    // Hierarchy / Metadata
    uint32_t id;         // Hash ID for identification
    uint32_t flags;      // UiRenderFlags
    
    // Custom shader data (optional)
    uint32_t primitive_type; // 0=Quad, 1=SDF, etc.
    Vec4 params;             // Generic params (e.g. curve control points, thickness)
} UiNode;

#endif // UI_NODE_H
