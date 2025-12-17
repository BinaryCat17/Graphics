#include "transpiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>

// --- String Buffer Helper ---

typedef struct {
    char* data;
    size_t length;
    size_t capacity;
} StringBuilder;

static void sb_init(StringBuilder* sb) {
    sb->capacity = 1024;
    sb->data = malloc(sb->capacity);
    sb->data[0] = '\0';
    sb->length = 0;
}

static void sb_append_fmt(StringBuilder* sb, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    // Determine needed size
    va_list args_copy;
    va_copy(args_copy, args);
    int len = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);
    
    if (len < 0) return; 
    
    if (sb->length + len + 1 > sb->capacity) {
        size_t new_cap = sb->capacity * 2 + len;
        char* new_data = realloc(sb->data, new_cap);
        if (!new_data) return; // OOM
        sb->data = new_data;
        sb->capacity = new_cap;
    }
    
    vsnprintf(sb->data + sb->length, len + 1, fmt, args);
    sb->length += len;
    
    va_end(args);
}

// --- Transpiler Logic ---

static bool is_visited(int* visited, int count, int id) {
    for (int i = 0; i < count; ++i) {
        if (visited[i] == id) return true;
    }
    return false;
}

static void generate_node_code(MathNode* node, StringBuilder* sb, int* visited, int* visited_count) {
    if (is_visited(visited, *visited_count, node->id)) return; 
    
    // Visit inputs first (Post-order traversal ensures dependencies are defined)
    for (size_t i = 0; i < node->input_count; ++i) {
        if (node->inputs[i]) {
            generate_node_code(node->inputs[i], sb, visited, visited_count);
        }
    }
    
    // Generate code for this node
    sb_append_fmt(sb, "    // Node %d (%s)\n", node->id, node->name ? node->name : "Unnamed");
    
    switch (node->type) {
        case MATH_NODE_VALUE:
            sb_append_fmt(sb, "    float v_%d = %f;\n", node->id, node->value);
            break;
        
        case MATH_NODE_TIME:
            sb_append_fmt(sb, "    float v_%d = params.time;\n", node->id);
            break;

        case MATH_NODE_UV:
             // uv is pre-calculated in main
             // We can split uv.x and uv.y if needed, but here we assume nodes output floats.
             // Let's assume UV node outputs x+y or length? 
             // Ideally UV node should probably have 2 outputs, but our graph is 1-output per node (mostly).
             // Let's make UV node return .x, and maybe add UV_Y later? 
             // Or better: Let's treat it as a float for now (e.g. x coordinate).
             // Actually, for a visualizer, usually we work with vec3/vec4. 
             // But our current nodes are float-only.
             // Hack: UV node returns uv.x.
             sb_append_fmt(sb, "    float v_%d = uv.x;\n", node->id); 
             break;
            
        case MATH_NODE_ADD:
            if (node->input_count >= 2 && node->inputs[0] && node->inputs[1])
                sb_append_fmt(sb, "    float v_%d = v_%d + v_%d;\n", node->id, node->inputs[0]->id, node->inputs[1]->id);
            else
                sb_append_fmt(sb, "    float v_%d = 0.0;\n", node->id);
            break;
            
        case MATH_NODE_SUB:
            if (node->input_count >= 2 && node->inputs[0] && node->inputs[1])
                sb_append_fmt(sb, "    float v_%d = v_%d - v_%d;\n", node->id, node->inputs[0]->id, node->inputs[1]->id);
            else
                sb_append_fmt(sb, "    float v_%d = 0.0;\n", node->id);
            break;

        case MATH_NODE_MUL:
            if (node->input_count >= 2 && node->inputs[0] && node->inputs[1])
                sb_append_fmt(sb, "    float v_%d = v_%d * v_%d;\n", node->id, node->inputs[0]->id, node->inputs[1]->id);
            else
                sb_append_fmt(sb, "    float v_%d = 0.0;\n", node->id);
            break;

        case MATH_NODE_DIV:
            if (node->input_count >= 2 && node->inputs[0] && node->inputs[1])
                sb_append_fmt(sb, "    float v_%d = v_%d / (v_%d + 0.0001);\n", node->id, node->inputs[0]->id, node->inputs[1]->id); // Prevent div by zero
            else
                sb_append_fmt(sb, "    float v_%d = 0.0;\n", node->id);
            break;
            
        case MATH_NODE_SIN:
            if (node->input_count >= 1 && node->inputs[0])
                sb_append_fmt(sb, "    float v_%d = sin(v_%d);\n", node->id, node->inputs[0]->id);
            else
                sb_append_fmt(sb, "    float v_%d = 0.0;\n", node->id);
            break;

        case MATH_NODE_COS:
            if (node->input_count >= 1 && node->inputs[0])
                sb_append_fmt(sb, "    float v_%d = cos(v_%d);\n", node->id, node->inputs[0]->id);
            else
                sb_append_fmt(sb, "    float v_%d = 0.0;\n", node->id);
            break;

        default:
            sb_append_fmt(sb, "    float v_%d = 0.0; // Unknown Type\n", node->id);
            break;
    }
    
    visited[(*visited_count)++] = node->id;
}

char* math_graph_transpile_glsl(const MathGraph* graph, TranspilerMode mode) {
    if (!graph) return NULL; 
    
    StringBuilder sb;
    sb_init(&sb);
    
    sb_append_fmt(&sb, "#version 450\n");
    
    if (mode == TRANSPILE_MODE_IMAGE_2D) {
        sb_append_fmt(&sb, "layout(local_size_x = 16, local_size_y = 16) in;\n\n");
        sb_append_fmt(&sb, "layout(set=0, binding=0, rgba8) writeonly uniform image2D outImg;\n\n");
        
        sb_append_fmt(&sb, "layout(push_constant) uniform Params {\n");
        sb_append_fmt(&sb, "    float time;\n");
        sb_append_fmt(&sb, "    float width;\n");
        sb_append_fmt(&sb, "    float height;\n");
        sb_append_fmt(&sb, "} params;\n\n");
    } else {
        sb_append_fmt(&sb, "layout(local_size_x = 1) in;\n\n");
        sb_append_fmt(&sb, "layout(set=0, binding=0) buffer OutBuf {\n");
        sb_append_fmt(&sb, "    float result;\n");
        sb_append_fmt(&sb, "} b_out;\n\n");
        
        // Dummy params for buffer mode to allow compilation if nodes use them
        sb_append_fmt(&sb, "struct Params { float time; float width; float height; };\n");
        sb_append_fmt(&sb, "const Params params = Params(0.0, 1.0, 1.0);\n\n");
    }
    
    sb_append_fmt(&sb, "void main() {\n");
    
    if (mode == TRANSPILE_MODE_IMAGE_2D) {
        sb_append_fmt(&sb, "    ivec2 storePos = ivec2(gl_GlobalInvocationID.xy);\n");
        sb_append_fmt(&sb, "    if (storePos.x >= int(params.width) || storePos.y >= int(params.height)) return;\n\n");
        sb_append_fmt(&sb, "    vec2 uv = vec2(storePos) / vec2(params.width, params.height);\n\n");
    } else {
        sb_append_fmt(&sb, "    vec2 uv = vec2(0.0, 0.0);\n\n");
    }
    
    // Tracking visited nodes to avoid duplicate declarations
    int* visited = malloc(graph->node_count * sizeof(int));
    int visited_count = 0;
    
    for (int i = 0; i < graph->node_count; ++i) {
        generate_node_code(graph->nodes[i], &sb, visited, &visited_count);
    }
    
    // Assign Output
    if (graph->node_count > 0) {
        int last_id = graph->nodes[graph->node_count - 1]->id;
        if (mode == TRANSPILE_MODE_IMAGE_2D) {
            sb_append_fmt(&sb, "    float res = v_%d;\n", last_id);
            sb_append_fmt(&sb, "    imageStore(outImg, storePos, vec4(res, res, res, 1.0));\n");
        } else {
            sb_append_fmt(&sb, "    b_out.result = v_%d;\n", last_id);
        }
    } else {
        if (mode == TRANSPILE_MODE_IMAGE_2D) {
            sb_append_fmt(&sb, "    imageStore(outImg, storePos, vec4(0,0,0,1));\n");
        } else {
            sb_append_fmt(&sb, "    b_out.result = 0.0;\n");
        }
    }
    
    sb_append_fmt(&sb, "}\n");
    
    free(visited);
    return sb.data;
}
