#ifndef PRIMITIVES_H
#define PRIMITIVES_H

#include "foundation/math/math_types.h"

// Standard Vertex Format for Primitives: Pos(3) + UV(2) = 5 floats
#define PRIM_VERTEX_STRIDE 5
#define PRIM_QUAD_VERTEX_COUNT 4
#define PRIM_QUAD_INDEX_COUNT 6

// Interleaved: Pos (3) + UV (2)
// Quad: 0..1 range
static const float PRIM_QUAD_VERTS[] = {
    0.0f, 0.0f, 0.0f,   0.0f, 0.0f, // BL
    1.0f, 0.0f, 0.0f,   1.0f, 0.0f, // BR
    1.0f, 1.0f, 0.0f,   1.0f, 1.0f, // TR
    0.0f, 1.0f, 0.0f,   0.0f, 1.0f  // TL
};

static const unsigned int PRIM_QUAD_INDICES[] = {
    0, 1, 2, 
    0, 2, 3
};

// GPU Instance Data Layout (std140/std430 compatible)
// Used for UI and Sprite rendering
typedef struct GpuInstanceData {
    Mat4 model;
    Vec4 color;
    Vec4 uv_rect;
    Vec4 params_1;
    Vec4 params_2;
    Vec4 clip_rect;
} GpuInstanceData;

#endif // PRIMITIVES_H