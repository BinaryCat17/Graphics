#ifndef C_EMITTER_H
#define C_EMITTER_H

#include "../shader_ir.h"
#include "../transpiler.h"

// Generates C code from the provided Shader IR.
// The generated code includes necessary structs (vec2, vec3, vec4) and math helpers.
char* ir_to_c(const ShaderIR* ir, TranspilerMode mode);

#endif // C_EMITTER_H
