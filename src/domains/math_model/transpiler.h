#ifndef TRANSPILER_H
#define TRANSPILER_H

#include "math_graph.h"

// Compiles the math graph into a GLSL function body.
// The generated code assumes standard GLSL math functions (sin, cos, etc.) are available.
// Returns a heap-allocated string that must be freed by the caller.
char* math_graph_transpile_glsl(const MathGraph* graph);

#endif // TRANSPILER_H
