#include "transpiler.h"
#include "foundation/memory/arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>

// --- Arena String Stream ---

typedef struct {
    MemoryArena* arena;
    char* start;
    char* cursor;
    size_t capacity_left;
} ArenaStream;

static void stream_init(ArenaStream* stream, MemoryArena* arena) {
    stream->arena = arena;
    stream->start = (char*)(arena->base + arena->offset);
    stream->cursor = stream->start;
    stream->capacity_left = arena->size - arena->offset;
    // Ensure initial null terminator
    if (stream->capacity_left > 0) *stream->cursor = '\0';
}

static void stream_printf(ArenaStream* stream, const char* fmt, ...) {
    if (stream->capacity_left == 0) return;

    va_list args;
    va_start(args, fmt);
    
    // Determine needed size
    va_list args_copy;
    va_copy(args_copy, args);
    int len = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);
    
    if (len < 0) {
        va_end(args);
        return;
    }
    
    if ((size_t)len + 1 > stream->capacity_left) {
        // Overflow
        va_end(args);
        return;
    }
    
    vsnprintf(stream->cursor, stream->capacity_left, fmt, args);
    
    stream->cursor += len;
    stream->capacity_left -= len;
    
    va_end(args);
}

// --- Transpiler Logic ---

static bool is_visited(int* visited, int count, int id) {
    for (int i = 0; i < count; ++i) {
        if (visited[i] == id) return true;
    }
    return false;
}

static void generate_node_code(const MathGraph* graph, MathNodeId id, ArenaStream* stream, int* visited, int* visited_count) {
    if (id == MATH_NODE_INVALID_ID) return;
    if (is_visited(visited, *visited_count, (int)id)) return; 
    
    // Access node directly since we are inside the module
    if (id >= graph->node_count) return;
    const MathNode* node = &graph->nodes[id];
    if (node->type == MATH_NODE_NONE) return;

    // Visit inputs first (Post-order traversal)
    for (int i = 0; i < MATH_NODE_MAX_INPUTS; ++i) {
        if (node->inputs[i] != MATH_NODE_INVALID_ID) {
            generate_node_code(graph, node->inputs[i], stream, visited, visited_count);
        }
    }
    
    // Generate code for this node
    stream_printf(stream, "    // Node %d (%s)\n", node->id, node->name);
    
    switch (node->type) {
        case MATH_NODE_VALUE:
            stream_printf(stream, "    float v_%d = %f;\n", node->id, node->value);
            break;
        
        case MATH_NODE_TIME:
            stream_printf(stream, "    float v_%d = params.time;\n", node->id);
            break;

        case MATH_NODE_UV:
             stream_printf(stream, "    float v_%d = uv.x;\n", node->id); 
             break;
            
        case MATH_NODE_ADD:
            if (node->inputs[0] != MATH_NODE_INVALID_ID && node->inputs[1] != MATH_NODE_INVALID_ID)
                stream_printf(stream, "    float v_%d = v_%d + v_%d;\n", node->id, node->inputs[0], node->inputs[1]);
            else
                stream_printf(stream, "    float v_%d = 0.0;\n", node->id);
            break;
            
        case MATH_NODE_SUB:
            if (node->inputs[0] != MATH_NODE_INVALID_ID && node->inputs[1] != MATH_NODE_INVALID_ID)
                stream_printf(stream, "    float v_%d = v_%d - v_%d;\n", node->id, node->inputs[0], node->inputs[1]);
            else
                stream_printf(stream, "    float v_%d = 0.0;\n", node->id);
            break;

        case MATH_NODE_MUL:
            if (node->inputs[0] != MATH_NODE_INVALID_ID && node->inputs[1] != MATH_NODE_INVALID_ID)
                stream_printf(stream, "    float v_%d = v_%d * v_%d;\n", node->id, node->inputs[0], node->inputs[1]);
            else
                stream_printf(stream, "    float v_%d = 0.0;\n", node->id);
            break;

        case MATH_NODE_DIV:
            if (node->inputs[0] != MATH_NODE_INVALID_ID && node->inputs[1] != MATH_NODE_INVALID_ID)
                stream_printf(stream, "    float v_%d = v_%d / (v_%d + 0.0001);\n", node->id, node->inputs[0], node->inputs[1]); 
            else
                stream_printf(stream, "    float v_%d = 0.0;\n", node->id);
            break;
            
        case MATH_NODE_SIN:
            if (node->inputs[0] != MATH_NODE_INVALID_ID)
                stream_printf(stream, "    float v_%d = sin(v_%d);\n", node->id, node->inputs[0]);
            else
                stream_printf(stream, "    float v_%d = 0.0;\n", node->id);
            break;

        case MATH_NODE_COS:
            if (node->inputs[0] != MATH_NODE_INVALID_ID)
                stream_printf(stream, "    float v_%d = cos(v_%d);\n", node->id, node->inputs[0]);
            else
                stream_printf(stream, "    float v_%d = 0.0;\n", node->id);
            break;

        default:
            stream_printf(stream, "    float v_%d = 0.0; // Unknown Type\n", node->id);
            break;
    }
    
    visited[(*visited_count)++] = node->id;
}

char* math_graph_transpile_glsl(const MathGraph* graph, TranspilerMode mode) {
    if (!graph) return NULL; 

    MemoryArena arena;
    if (!arena_init(&arena, 64 * 1024)) return NULL;
    
    ArenaStream stream;
    stream_init(&stream, &arena);
    
    stream_printf(&stream, "#version 450\n");
    
    if (mode == TRANSPILE_MODE_IMAGE_2D) {
        stream_printf(&stream, "layout(local_size_x = 16, local_size_y = 16) in;\n\n");
        stream_printf(&stream, "layout(set=0, binding=0, rgba8) writeonly uniform image2D outImg;\n\n");
        
        stream_printf(&stream, "layout(push_constant) uniform Params {\n");
        stream_printf(&stream, "    float time;\n");
        stream_printf(&stream, "    float width;\n");
        stream_printf(&stream, "    float height;\n");
        stream_printf(&stream, "} params;\n\n");
    } else {
        stream_printf(&stream, "layout(local_size_x = 1) in;\n\n");
        stream_printf(&stream, "layout(set=0, binding=0) buffer OutBuf {\n");
        stream_printf(&stream, "    float result;\n");
        stream_printf(&stream, "} b_out;\n\n");
        
        stream_printf(&stream, "struct Params { float time; float width; float height; };\n");
        stream_printf(&stream, "const Params params = Params(0.0, 1.0, 1.0);\n\n");
    }
    
    stream_printf(&stream, "void main() {\n");
    
    if (mode == TRANSPILE_MODE_IMAGE_2D) {
        stream_printf(&stream, "    ivec2 storePos = ivec2(gl_GlobalInvocationID.xy);\n");
        stream_printf(&stream, "    if (storePos.x >= int(params.width) || storePos.y >= int(params.height)) return;\n\n");
        stream_printf(&stream, "    vec2 uv = vec2(storePos) / vec2(params.width, params.height);\n\n");
    } else {
        stream_printf(&stream, "    vec2 uv = vec2(0.0, 0.0);\n\n");
    }
    
    int* visited = (int*)calloc(graph->node_count, sizeof(int));
    int visited_count = 0;
    
    // We only need to generate code for nodes that actually lead to the output?
    // For now, let's just generate everything valid.
    for (uint32_t i = 0; i < graph->node_count; ++i) {
        if (graph->nodes[i].type != MATH_NODE_NONE) {
            generate_node_code(graph, i, &stream, visited, &visited_count);
        }
    }
    
    // Assign Output (Take the last non-free node as output for now)
    int last_id = -1;
    for (int i = (int)graph->node_count - 1; i >= 0; --i) {
        if (graph->nodes[i].type != MATH_NODE_NONE) {
            last_id = i;
            break;
        }
    }

    if (last_id != -1) {
        if (mode == TRANSPILE_MODE_IMAGE_2D) {
            stream_printf(&stream, "    float res = v_%d;\n", last_id);
            stream_printf(&stream, "    imageStore(outImg, storePos, vec4(res, res, res, 1.0));\n");
        } else {
            stream_printf(&stream, "    b_out.result = v_%d;\n", last_id);
        }
    } else {
        if (mode == TRANSPILE_MODE_IMAGE_2D) {
            stream_printf(&stream, "    imageStore(outImg, storePos, vec4(0,0,0,1));\n");
        } else {
            stream_printf(&stream, "    b_out.result = 0.0;\n");
        }
    }
    
    stream_printf(&stream, "}\n");
    
    free(visited);
    char* result = strdup(stream.start);
    arena_destroy(&arena);
    return result;
}