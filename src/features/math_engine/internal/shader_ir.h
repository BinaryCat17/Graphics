#ifndef SHADER_IR_H
#define SHADER_IR_H

#include <stdint.h>
#include <stdbool.h>
#include "features/math_engine/math_graph.h"

// Intermediate Representation OpCodes
typedef enum IrOpCode {
    IR_OP_NOP = 0,
    
    // Values & Parameters
    IR_OP_CONST_FLOAT,      // res = float_val
    IR_OP_LOAD_PARAM_TIME,  // res = time
    IR_OP_LOAD_PARAM_MOUSE, // res = mouse (vec2)
    IR_OP_LOAD_PARAM_MOUSE_DELTA, // res = mouse_delta (vec2)
    IR_OP_LOAD_PARAM_MOUSE_SCROLL, // res = mouse_scroll (vec2)
    IR_OP_LOAD_PARAM_MOUSE_BUTTONS, // res = mouse_buttons (uint)
    IR_OP_LOAD_PARAM_TEXTURE, // res = sampler2D (handle)
    IR_OP_LOAD_PARAM_UV,    // res = uv.x (or uv vector in future)
    
    // Arithmetic
    IR_OP_ADD,              // res = op1 + op2
    IR_OP_SUB,              // res = op1 - op2
    IR_OP_MUL,              // res = op1 * op2
    IR_OP_DIV,              // res = op1 / op2
    
    // Math Functions
    IR_OP_SIN,              // res = sin(op1)
    IR_OP_COS,              // res = cos(op1)
    
    // Texture
    IR_OP_SAMPLE_TEXTURE,   // res = texture(op1_sampler, op2_uv)

    // Output
    IR_OP_RETURN            // result = op1 (Final output of the shader)
} IrOpCode;

typedef struct IrInstruction {
    IrOpCode op;
    MathDataType type;      // Data type of the result
    uint32_t id;            // Result ID (Virtual Register)
    uint32_t op1_id;        // Operand 1 ID (0 if unused)
    uint32_t op2_id;        // Operand 2 ID (0 if unused)
    float float_val;        // For IR_OP_CONST_FLOAT
} IrInstruction;

typedef struct ShaderIR {
    IrInstruction* instructions;
    uint32_t instruction_count;
    uint32_t instruction_capacity;
} ShaderIR;

#endif // SHADER_IR_H
