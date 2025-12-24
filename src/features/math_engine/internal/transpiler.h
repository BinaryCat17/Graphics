#ifndef TRANSPILER_H
#define TRANSPILER_H

#include "features/math_engine/math_graph.h"
#include <stdbool.h>

typedef enum TranspilerMode {
    TRANSPILE_MODE_BUFFER_1D = 0, // Output: float result (Compute Buffer)
    TRANSPILE_MODE_IMAGE_2D   // Output: image2D (Storage Image)
} TranspilerMode;

typedef enum ShaderTarget {
    SHADER_TARGET_GLSL_VULKAN,
    SHADER_TARGET_C // CPU Fallback
} ShaderTarget;

// Generates shader source code for the specified target.
// Returns a heap-allocated string that must be freed by the caller.
char* math_graph_transpile(const MathGraph* graph, TranspilerMode mode, ShaderTarget target);

#endif // TRANSPILER_H
