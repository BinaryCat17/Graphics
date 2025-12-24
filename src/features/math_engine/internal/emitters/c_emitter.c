#include "c_emitter.h"
#include "foundation/memory/arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// --- Helper: Simple Arena String Builder ---
// (Copy-pasted pattern from glsl_emitter.c to avoid dependency complications for now)

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
    stream->arena->offset += len;

    va_end(args);
}

// --- Type Helper ---
static const char* get_c_type_name(MathDataType type) {
    switch (type) {
        case MATH_DATA_TYPE_VEC2: return "vec2";
        case MATH_DATA_TYPE_VEC3: return "vec3";
        case MATH_DATA_TYPE_VEC4: return "vec4";
        case MATH_DATA_TYPE_SAMPLER2D: return "void*"; // Placeholder for sampler
        case MATH_DATA_TYPE_FLOAT:
        default: return "float";
    }
}

// --- Preamble Generation ---
static void emit_preamble(EmitterStream* stream) {
    stream_printf(stream, "#include <math.h>\n\n");
    stream_printf(stream, "typedef struct { float x, y; } vec2;\n");
    stream_printf(stream, "typedef struct { float x, y, z; } vec3;\n");
    stream_printf(stream, "typedef struct { float x, y, z, w; } vec4;\n\n");
    
    // Constructors/Splatting
    stream_printf(stream, "static inline vec2 vec2_splat(float v) { vec2 r = {v, v}; return r; }\n");
    stream_printf(stream, "static inline vec3 vec3_splat(float v) { vec3 r = {v, v, v}; return r; }\n");
    stream_printf(stream, "static inline vec4 vec4_splat(float v) { vec4 r = {v, v, v, v}; return r; }\n");
    stream_printf(stream, "static inline vec2 vec2_ctor(float x, float y) { vec2 r = {x, y}; return r; }\n");
    stream_printf(stream, "static inline vec4 vec4_ctor(float x, float y, float z, float w) { vec4 r = {x, y, z, w}; return r; }\n\n");

    // Math Helpers (Add)
    stream_printf(stream, "static inline float f_add(float a, float b) { return a + b; }\n");
    stream_printf(stream, "static inline vec2 vec2_add(vec2 a, vec2 b) { vec2 r = {a.x+b.x, a.y+b.y}; return r; }\n");
    stream_printf(stream, "static inline vec3 vec3_add(vec3 a, vec3 b) { vec3 r = {a.x+b.x, a.y+b.y, a.z+b.z}; return r; }\n");
    stream_printf(stream, "static inline vec4 vec4_add(vec4 a, vec4 b) { vec4 r = {a.x+b.x, a.y+b.y, a.z+b.z, a.w+b.w}; return r; }\n\n");

    // Math Helpers (Sub)
    stream_printf(stream, "static inline float f_sub(float a, float b) { return a - b; }\n");
    stream_printf(stream, "static inline vec2 vec2_sub(vec2 a, vec2 b) { vec2 r = {a.x-b.x, a.y-b.y}; return r; }\n");
    stream_printf(stream, "static inline vec3 vec3_sub(vec3 a, vec3 b) { vec3 r = {a.x-b.x, a.y-b.y, a.z-b.z}; return r; }\n");
    stream_printf(stream, "static inline vec4 vec4_sub(vec4 a, vec4 b) { vec4 r = {a.x-b.x, a.y-b.y, a.z-b.z, a.w-b.w}; return r; }\n\n");

    // Math Helpers (Mul)
    stream_printf(stream, "static inline float f_mul(float a, float b) { return a * b; }\n");
    stream_printf(stream, "static inline vec2 vec2_mul(vec2 a, vec2 b) { vec2 r = {a.x*b.x, a.y*b.y}; return r; }\n");
    stream_printf(stream, "static inline vec3 vec3_mul(vec3 a, vec3 b) { vec3 r = {a.x*b.x, a.y*b.y, a.z*b.z}; return r; }\n");
    stream_printf(stream, "static inline vec4 vec4_mul(vec4 a, vec4 b) { vec4 r = {a.x*b.x, a.y*b.y, a.z*b.z, a.w*b.w}; return r; }\n\n");

    // Math Helpers (Div)
    stream_printf(stream, "static inline float f_div(float a, float b) { return a / (b + 0.0001f); }\n");
    stream_printf(stream, "static inline vec2 vec2_div(vec2 a, vec2 b) { vec2 r = {a.x/(b.x+0.0001f), a.y/(b.y+0.0001f)}; return r; }\n");
    stream_printf(stream, "static inline vec3 vec3_div(vec3 a, vec3 b) { vec3 r = {a.x/(b.x+0.0001f), a.y/(b.y+0.0001f), a.z/(b.z+0.0001f)}; return r; }\n");
    stream_printf(stream, "static inline vec4 vec4_div(vec4 a, vec4 b) { vec4 r = {a.x/(b.x+0.0001f), a.y/(b.y+0.0001f), a.z/(b.z+0.0001f), a.w/(b.w+0.0001f)}; return r; }\n\n");

    // Math Helpers (Sin/Cos)
    stream_printf(stream, "static inline float f_sin(float a) { return sinf(a); }\n");
    stream_printf(stream, "static inline vec2 vec2_sin(vec2 a) { vec2 r = {sinf(a.x), sinf(a.y)}; return r; }\n");
    stream_printf(stream, "static inline vec3 vec3_sin(vec3 a) { vec3 r = {sinf(a.x), sinf(a.y), sinf(a.z)}; return r; }\n");
    stream_printf(stream, "static inline vec4 vec4_sin(vec4 a) { vec4 r = {sinf(a.x), sinf(a.y), sinf(a.z), sinf(a.w)}; return r; }\n\n");

    stream_printf(stream, "static inline float f_cos(float a) { return cosf(a); }\n");
    stream_printf(stream, "static inline vec2 vec2_cos(vec2 a) { vec2 r = {cosf(a.x), cosf(a.y)}; return r; }\n");
    stream_printf(stream, "static inline vec3 vec3_cos(vec3 a) { vec3 r = {cosf(a.x), cosf(a.y), cosf(a.z)}; return r; }\n");
    stream_printf(stream, "static inline vec4 vec4_cos(vec4 a) { vec4 r = {cosf(a.x), cosf(a.y), cosf(a.z), cosf(a.w)}; return r; }\n\n");

    // Texture Placeholder
    stream_printf(stream, "static inline vec4 sample_texture(void* tex, vec2 uv) { (void)tex; (void)uv; return vec4_splat(0.0f); }\n\n");
}

static const char* get_op_prefix(MathDataType type) {
    switch(type) {
        case MATH_DATA_TYPE_VEC2: return "vec2_";
        case MATH_DATA_TYPE_VEC3: return "vec3_";
        case MATH_DATA_TYPE_VEC4: return "vec4_";
        default: return "f_";
    }
}

char* ir_to_c(const ShaderIR* ir, TranspilerMode mode) {
    if (!ir) return NULL;

    MemoryArena arena;
    if (!arena_init(&arena, 32 * 1024)) return NULL;

    EmitterStream stream;
    stream_init(&stream, &arena);

    // 1. Emit Preamble (Structs & Math)
    emit_preamble(&stream);

    // 2. Emit Params Struct
    stream_printf(&stream, "typedef struct {\n");
    stream_printf(&stream, "    float time;\n");
    stream_printf(&stream, "    float width;\n");
    stream_printf(&stream, "    float height;\n");
    stream_printf(&stream, "    vec4 mouse;\n");
    
    for (uint32_t i = 0; i < ir->instruction_count; ++i) {
        if (ir->instructions[i].op == IR_OP_LOAD_PARAM_TEXTURE) {
            stream_printf(&stream, "    void* tex_%d;\n", ir->instructions[i].id);
        }
    }
    stream_printf(&stream, "} GraphParams;\n\n");

    // 3. Emit Function Signature
    stream_printf(&stream, "void execute_graph(void* out_buffer, GraphParams params) {\n");
    
    // UV Logic
    if (mode == TRANSPILE_MODE_IMAGE_2D) {
        stream_printf(&stream, "    // Loop injected by caller usually, but here we assume single pixel eval for simplicity or loop internal?\n");
        stream_printf(&stream, "    // NOTE: This generated C code is intended to be the 'inner loop' body or a single eval function.\n");
        stream_printf(&stream, "    // We will assume 'uv' is calculated from params or passed in. \n");
        stream_printf(&stream, "    // For strict correctness with image2d, we need x/y coordinates.\n");
        stream_printf(&stream, "    vec2 uv = vec2_ctor(0.0f, 0.0f); // Placeholder\n");
    } else {
        stream_printf(&stream, "    vec2 uv = vec2_ctor(0.0f, 0.0f);\n");
    }

    // 4. Emit Instructions
    uint32_t final_result_id = 0;
    bool has_result = false;
    MathDataType result_type = MATH_DATA_TYPE_FLOAT;

    for (uint32_t i = 0; i < ir->instruction_count; ++i) {
        IrInstruction* inst = &ir->instructions[i];
        const char* type_name = get_c_type_name(inst->type);
        const char* prefix = get_op_prefix(inst->type);
        
        switch (inst->op) {
            case IR_OP_CONST_FLOAT:
                if (inst->type == MATH_DATA_TYPE_FLOAT) {
                    stream_printf(&stream, "    float v_%d = %ff;\n", inst->id, inst->float_val);
                } else {
                    // Splat
                    stream_printf(&stream, "    %s v_%d = %ssplat(%ff);\n", type_name, inst->id, prefix, inst->float_val);
                }
                break;

            case IR_OP_LOAD_PARAM_TIME:
                stream_printf(&stream, "    float v_%d = params.time;\n", inst->id);
                break;

            case IR_OP_LOAD_PARAM_MOUSE:
                stream_printf(&stream, "    vec4 v_%d = params.mouse;\n", inst->id);
                break;
                
            case IR_OP_LOAD_PARAM_TEXTURE:
                // Just a handle/pointer
                stream_printf(&stream, "    void* v_%d = params.tex_%d;\n", inst->id, inst->id);
                break;
                
            case IR_OP_SAMPLE_TEXTURE:
                stream_printf(&stream, "    vec4 v_%d = sample_texture(v_%d, v_%d);\n", inst->id, inst->op1_id, inst->op2_id);
                break;

            case IR_OP_LOAD_PARAM_UV:
                 stream_printf(&stream, "    vec2 v_%d = uv;\n", inst->id);
                 break;

            case IR_OP_ADD:
                stream_printf(&stream, "    %s v_%d = %sadd(v_%d, v_%d);\n", type_name, inst->id, prefix, inst->op1_id, inst->op2_id);
                break;
            case IR_OP_SUB:
                stream_printf(&stream, "    %s v_%d = %ssub(v_%d, v_%d);\n", type_name, inst->id, prefix, inst->op1_id, inst->op2_id);
                break;
            case IR_OP_MUL:
                stream_printf(&stream, "    %s v_%d = %smul(v_%d, v_%d);\n", type_name, inst->id, prefix, inst->op1_id, inst->op2_id);
                break;
            case IR_OP_DIV:
                stream_printf(&stream, "    %s v_%d = %sdiv(v_%d, v_%d);\n", type_name, inst->id, prefix, inst->op1_id, inst->op2_id);
                break;
            case IR_OP_SIN:
                stream_printf(&stream, "    %s v_%d = %ssin(v_%d);\n", type_name, inst->id, prefix, inst->op1_id);
                break;
            case IR_OP_COS:
                stream_printf(&stream, "    %s v_%d = %scos(v_%d);\n", type_name, inst->id, prefix, inst->op1_id);
                break;
            
            case IR_OP_RETURN:
                final_result_id = inst->op1_id;
                result_type = inst->type;
                has_result = true;
                break;
            
            default: break;
        }
    }

    // 5. Output Assignment
    if (has_result) {
        stream_printf(&stream, "    // Write result\n");
        stream_printf(&stream, "    *(%s*)out_buffer = v_%d;\n", get_c_type_name(result_type), final_result_id);
    }

    stream_printf(&stream, "}\n");

    char* result = strdup((char*)arena.base);
    arena_destroy(&arena);
    return result;
}