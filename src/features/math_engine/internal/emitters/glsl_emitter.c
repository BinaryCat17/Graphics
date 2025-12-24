#include "glsl_emitter.h"
#include "foundation/memory/arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// --- Helper: Simple Arena String Builder (Duplicated for now to avoid massive foundation refactor) ---
// TODO: Move this to foundation/string_utils.h

typedef struct {
    MemoryArena* arena;
    char* cursor;
    size_t capacity_left;
} EmitterStream;

static void stream_init(EmitterStream* stream, MemoryArena* arena) {
    stream->arena = arena;
    stream->cursor = (char*)(arena->base + arena->offset);
    stream->capacity_left = arena->size - arena->offset;
    if (stream->capacity_left > 0) *stream->cursor = '\0';
}

static void stream_printf(EmitterStream* stream, const char* fmt, ...) {
    if (stream->capacity_left == 0) return;

    va_list args;
    va_start(args, fmt);
    
    va_list args_copy;
    va_copy(args_copy, args);
    int len = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);
    
    if (len < 0 || (size_t)len + 1 > stream->capacity_left) {
        va_end(args);
        return; 
    }
    
    vsnprintf(stream->cursor, stream->capacity_left, fmt, args);
    stream->cursor += len;
    stream->capacity_left -= len;
    
    // Update arena offset manually since we are writing directly to memory
    stream->arena->offset += len;

    va_end(args);
}

// --- Helper: Type Name ---
static const char* get_type_name(MathDataType type) {
    switch (type) {
        case MATH_DATA_TYPE_VEC2: return "vec2";
        case MATH_DATA_TYPE_VEC3: return "vec3";
        case MATH_DATA_TYPE_VEC4: return "vec4";
        case MATH_DATA_TYPE_FLOAT:
        default: return "float";
    }
}

// --- GLSL Generation Logic ---

char* ir_to_glsl(const ShaderIR* ir, TranspilerMode mode) {
    if (!ir) return NULL;

    // Estimate size: 4KB should be enough for simple shaders
    MemoryArena arena;
    if (!arena_init(&arena, 16 * 1024)) return NULL;

    EmitterStream stream;
    stream_init(&stream, &arena);
    
    // Find result type
    MathDataType result_type = MATH_DATA_TYPE_FLOAT;
    for (uint32_t i = 0; i < ir->instruction_count; ++i) {
        if (ir->instructions[i].op == IR_OP_RETURN) {
            result_type = ir->instructions[i].type;
            break;
        }
    }
    const char* result_type_name = get_type_name(result_type);

    // Header
    stream_printf(&stream, "#version 450\n");

    if (mode == TRANSPILE_MODE_IMAGE_2D) {
        stream_printf(&stream, "layout(local_size_x = 16, local_size_y = 16) in;\n\n");
        stream_printf(&stream, "layout(set=0, binding=0, rgba8) writeonly uniform image2D outImg;\n\n");
        
        stream_printf(&stream, "layout(push_constant) uniform Params {\n");
        stream_printf(&stream, "    float time;\n");
        stream_printf(&stream, "    float width;\n");
        stream_printf(&stream, "    float height;\n");
        stream_printf(&stream, "    vec4 mouse;\n");
        stream_printf(&stream, "} params;\n\n");
    } else {
        stream_printf(&stream, "layout(local_size_x = 1) in;\n\n");
        stream_printf(&stream, "layout(set=0, binding=0) buffer OutBuf {\n");
        stream_printf(&stream, "    %s result;\n", result_type_name);
        stream_printf(&stream, "} b_out;\n\n");
        
        stream_printf(&stream, "struct Params { float time; float width; float height; vec4 mouse; };\n");
        stream_printf(&stream, "const Params params = Params(0.0, 1.0, 1.0, vec4(0));\n\n");
    }

    stream_printf(&stream, "void main() {\n");

    // Setup UV/Coordinates
    if (mode == TRANSPILE_MODE_IMAGE_2D) {
        stream_printf(&stream, "    ivec2 storePos = ivec2(gl_GlobalInvocationID.xy);\n");
        stream_printf(&stream, "    if (storePos.x >= int(params.width) || storePos.y >= int(params.height)) return;\n\n");
        stream_printf(&stream, "    vec2 uv = vec2(storePos) / vec2(params.width, params.height);\n\n");
    } else {
        stream_printf(&stream, "    vec2 uv = vec2(0.0, 0.0);\n\n");
    }

    // Body (Emit Instructions)
    uint32_t final_result_id = 0;
    bool has_result = false;

    for (uint32_t i = 0; i < ir->instruction_count; ++i) {
        IrInstruction* inst = &ir->instructions[i];
        const char* tname = get_type_name(inst->type);
        
        switch (inst->op) {
            case IR_OP_CONST_FLOAT:
                if (inst->type == MATH_DATA_TYPE_FLOAT) {
                    stream_printf(&stream, "    float v_%d = %f;\n", inst->id, inst->float_val);
                } else {
                    // Splatting (e.g. vec3(0.5))
                    stream_printf(&stream, "    %s v_%d = %s(%f);\n", tname, inst->id, tname, inst->float_val);
                }
                break;
            case IR_OP_LOAD_PARAM_TIME:
                stream_printf(&stream, "    float v_%d = params.time;\n", inst->id);
                break;
            case IR_OP_LOAD_PARAM_MOUSE:
                stream_printf(&stream, "    vec4 v_%d = params.mouse;\n", inst->id);
                break;
            case IR_OP_LOAD_PARAM_UV:
                 stream_printf(&stream, "    vec2 v_%d = uv;\n", inst->id);
                 break;
            case IR_OP_ADD:
                stream_printf(&stream, "    %s v_%d = v_%d + v_%d;\n", tname, inst->id, inst->op1_id, inst->op2_id);
                break;
            case IR_OP_SUB:
                stream_printf(&stream, "    %s v_%d = v_%d - v_%d;\n", tname, inst->id, inst->op1_id, inst->op2_id);
                break;
            case IR_OP_MUL:
                stream_printf(&stream, "    %s v_%d = v_%d * v_%d;\n", tname, inst->id, inst->op1_id, inst->op2_id);
                break;
            case IR_OP_DIV:
                stream_printf(&stream, "    %s v_%d = v_%d / (v_%d + 0.0001);\n", tname, inst->id, inst->op1_id, inst->op2_id);
                break;
            case IR_OP_SIN:
                stream_printf(&stream, "    %s v_%d = sin(v_%d);\n", tname, inst->id, inst->op1_id);
                break;
            case IR_OP_COS:
                stream_printf(&stream, "    %s v_%d = cos(v_%d);\n", tname, inst->id, inst->op1_id);
                break;
            case IR_OP_RETURN:
                final_result_id = inst->op1_id;
                has_result = true;
                break;
            default:
                break;
        }
    }

    // Output Writing
    if (has_result) {
        if (mode == TRANSPILE_MODE_IMAGE_2D) {
            // If result is float, replicate to RGB. If vec3/4, use it.
            if (result_type == MATH_DATA_TYPE_FLOAT) {
                 stream_printf(&stream, "    vec4 finalColor = vec4(v_%d, v_%d, v_%d, 1.0);\n", final_result_id, final_result_id, final_result_id);
            } else if (result_type == MATH_DATA_TYPE_VEC3) {
                 stream_printf(&stream, "    vec4 finalColor = vec4(v_%d, 1.0);\n", final_result_id);
            } else if (result_type == MATH_DATA_TYPE_VEC2) {
                 stream_printf(&stream, "    vec4 finalColor = vec4(v_%d, 0.0, 1.0);\n", final_result_id);
            } else {
                 stream_printf(&stream, "    vec4 finalColor = v_%d;\n", final_result_id);
            }
            stream_printf(&stream, "    imageStore(outImg, storePos, finalColor);\n");
        } else {
            stream_printf(&stream, "    b_out.result = v_%d;\n", final_result_id);
        }
    } else {
        // Default 0
        if (mode == TRANSPILE_MODE_IMAGE_2D) {
            stream_printf(&stream, "    imageStore(outImg, storePos, vec4(0,0,0,1));\n");
        } else {
            // Need to match result type
            if (result_type == MATH_DATA_TYPE_FLOAT) stream_printf(&stream, "    b_out.result = 0.0;\n");
            else stream_printf(&stream, "    b_out.result = %s(0.0);\n", result_type_name);
        }
    }

    stream_printf(&stream, "}\n");

    // Copy to heap and free arena
    char* result = strdup((char*)arena.base);
    arena_destroy(&arena);
    return result;
}
