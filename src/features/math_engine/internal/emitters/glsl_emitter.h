#ifndef GLSL_EMITTER_H
#define GLSL_EMITTER_H

#include "../shader_ir.h"
#include "../transpiler.h" // For TranspilerMode

// Generates GLSL source code from the Shader IR.
// Returns a heap-allocated string (caller must free).
char* ir_to_glsl(const ShaderIR* ir, TranspilerMode mode);

#endif // GLSL_EMITTER_H
