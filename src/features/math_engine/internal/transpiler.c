#include "transpiler.h"
#include "math_graph_internal.h"
#include "shader_ir.h"
#include "emitters/glsl_emitter.h"
#include "foundation/memory/arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// --- IR Generation Logic ---

static bool is_visited(int* visited, int count, int id) {
    for (int i = 0; i < count; ++i) {
        if (visited[i] == id) return true;
    }
    return false;
}

static void generate_ir_node(const MathGraph* graph, MathNodeId id, ShaderIR* ir, int* visited, int* visited_count, MathDataType* inferred_types) {
    if (id == MATH_NODE_INVALID_ID) return;
    if (is_visited(visited, *visited_count, (int)id)) return; 
    
    // Access node
    const MathNode* node = math_graph_get_node((MathGraph*)graph, id);
    if (!node) return;
    if (node->type == MATH_NODE_NONE) return;

    // Visit inputs first (Post-order traversal)
    for (int i = 0; i < MATH_NODE_MAX_INPUTS; ++i) {
        if (node->inputs[i] != MATH_NODE_INVALID_ID) {
            generate_ir_node(graph, node->inputs[i], ir, visited, visited_count, inferred_types);
        }
    }
    
    // Generate IR instruction
    IrInstruction inst = { .op = IR_OP_NOP, .id = node->id, .op1_id = 0, .op2_id = 0, .float_val = 0.0f, .type = MATH_DATA_TYPE_FLOAT };

    switch (node->type) {
        case MATH_NODE_VALUE:
            inst.op = IR_OP_CONST_FLOAT;
            inst.float_val = node->value;
            inst.type = MATH_DATA_TYPE_FLOAT;
            break;
        
        case MATH_NODE_TIME:
            inst.op = IR_OP_LOAD_PARAM_TIME;
            inst.type = MATH_DATA_TYPE_FLOAT;
            break;

        case MATH_NODE_MOUSE:
            inst.op = IR_OP_LOAD_PARAM_MOUSE;
            inst.type = MATH_DATA_TYPE_VEC4;
            break;

        case MATH_NODE_TEXTURE_PARAM:
            inst.op = IR_OP_LOAD_PARAM_TEXTURE;
            inst.type = MATH_DATA_TYPE_SAMPLER2D;
            break;

        case MATH_NODE_TEXTURE_SAMPLE:
            inst.op = IR_OP_SAMPLE_TEXTURE;
            inst.type = MATH_DATA_TYPE_VEC4;
            inst.op1_id = node->inputs[0]; // Sampler
            inst.op2_id = node->inputs[1]; // UV
            break;

        case MATH_NODE_UV:
             inst.op = IR_OP_LOAD_PARAM_UV;
             inst.type = MATH_DATA_TYPE_VEC2;
             break;
            
        case MATH_NODE_ADD:
        case MATH_NODE_SUB:
        case MATH_NODE_MUL:
        case MATH_NODE_DIV: {
            if (node->type == MATH_NODE_ADD) inst.op = IR_OP_ADD;
            else if (node->type == MATH_NODE_SUB) inst.op = IR_OP_SUB;
            else if (node->type == MATH_NODE_MUL) inst.op = IR_OP_MUL;
            else if (node->type == MATH_NODE_DIV) inst.op = IR_OP_DIV;

            inst.op1_id = node->inputs[0];
            inst.op2_id = node->inputs[1];
            
            MathDataType t1 = inferred_types[inst.op1_id];
            MathDataType t2 = inferred_types[inst.op2_id];
            
            // Simple promotion: max(t1, t2)
            inst.type = (t1 > t2) ? t1 : t2;
            if (inst.type == MATH_DATA_TYPE_UNKNOWN) inst.type = MATH_DATA_TYPE_FLOAT;
            break;
        }
            
        case MATH_NODE_SIN:
        case MATH_NODE_COS: {
            if (node->type == MATH_NODE_SIN) inst.op = IR_OP_SIN;
            else inst.op = IR_OP_COS;
            
            inst.op1_id = node->inputs[0];
            inst.type = inferred_types[inst.op1_id]; // Output type same as input
            break;
        }

        default:
            // Unknown node type
            inst.type = MATH_DATA_TYPE_FLOAT;
            break;
    }

    inferred_types[id] = inst.type;

    if (inst.op != IR_OP_NOP) {
        if (ir->instruction_count < ir->instruction_capacity) {
            ir->instructions[ir->instruction_count++] = inst;
        }
    }
    
    visited[(*visited_count)++] = node->id;
}

static ShaderIR math_graph_to_ir(const MathGraph* graph) {
    ShaderIR ir;
    ir.instruction_capacity = 256; // Fixed max instructions for now
    ir.instructions = (IrInstruction*)calloc(ir.instruction_capacity, sizeof(IrInstruction));
    ir.instruction_count = 0;

    int* visited = (int*)calloc(graph->node_count, sizeof(int));
    int visited_count = 0;
    
    MathDataType* inferred_types = (MathDataType*)calloc(graph->node_count, sizeof(MathDataType));

    // 1. Try to find the explicit OUTPUT node
    MathNodeId root_node_id = MATH_NODE_INVALID_ID;

    for (uint32_t i = 0; i < graph->node_count; ++i) {
        const MathNode* n = math_graph_get_node((MathGraph*)graph, i);
        if (n && n->type == MATH_NODE_OUTPUT) {
            // The result is what's connected to Input 0 of the Output Node
            if (n->inputs[0] != MATH_NODE_INVALID_ID) {
                root_node_id = n->inputs[0];
            }
            break;
        }
    }

    // 2. Generate IR starting from the root (recursively visits inputs)
    if (root_node_id != MATH_NODE_INVALID_ID) {
        generate_ir_node(graph, root_node_id, &ir, visited, &visited_count, inferred_types);
        
        // Add Return instruction
        if (ir.instruction_count < ir.instruction_capacity) {
            IrInstruction ret_inst = { 
                .op = IR_OP_RETURN, 
                .id = 0, 
                .op1_id = root_node_id,
                .type = inferred_types[root_node_id] 
            };
            ir.instructions[ir.instruction_count++] = ret_inst;
        }
    }

    free(inferred_types);
    free(visited);
    return ir;
}

static void free_ir(ShaderIR* ir) {
    if (ir->instructions) {
        free(ir->instructions);
    }
}

char* math_graph_transpile(const MathGraph* graph, TranspilerMode mode, ShaderTarget target) {
    if (!graph) return NULL; 

    // Phase 1: Generate IR
    ShaderIR ir = math_graph_to_ir(graph);

    // Phase 2: Emit Code based on Target
    char* result = NULL;
    switch (target) {
        case SHADER_TARGET_GLSL_VULKAN:
            result = ir_to_glsl(&ir, mode);
            break;
        default:
            // Fallback or error
            break;
    }

    // Cleanup
    free_ir(&ir);

    return result;
}
