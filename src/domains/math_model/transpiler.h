#ifndef TRANSPILER_H
#define TRANSPILER_H

#include "math_graph.h"
#include <stdbool.h>

typedef enum TranspilerMode {
    TRANSPILE_MODE_BUFFER_1D = 0, // Output: float result (Compute Buffer)
    TRANSPILE_MODE_IMAGE_2D   // Output: image2D (Storage Image)
} TranspilerMode;

char* math_graph_transpile_glsl(const MathGraph* graph, TranspilerMode mode);

#endif // TRANSPILER_H