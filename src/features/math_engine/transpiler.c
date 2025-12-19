#include "transpiler.h"
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

static void generate_ir_node(const MathGraph* graph, MathNodeId id, ShaderIR* ir, int* visited, int* visited_count) {
    if (id == MATH_NODE_INVALID_ID) return;
    if (is_visited(visited, *visited_count, (int)id)) return; 
    
    // Access node
    const MathNode* node = math_graph_get_node((MathGraph*)graph, id);
    if (!node) return;
    if (node->type == MATH_NODE_NONE) return;

    // Visit inputs first (Post-order traversal)
    for (int i = 0; i < MATH_NODE_MAX_INPUTS; ++i) {
        if (node->inputs[i] != MATH_NODE_INVALID_ID) {
            generate_ir_node(graph, node->inputs[i], ir, visited, visited_count);
        }
    }
    
    // Generate IR instruction
    IrInstruction inst = { .op = IR_OP_NOP, .id = node->id, .op1_id = 0, .op2_id = 0, .float_val = 0.0f };

    switch (node->type) {
        case MATH_NODE_VALUE:
            inst.op = IR_OP_CONST_FLOAT;
            inst.float_val = node->value;
            break;
        
        case MATH_NODE_TIME:
            inst.op = IR_OP_LOAD_PARAM_TIME;
            break;

        case MATH_NODE_UV:
             inst.op = IR_OP_LOAD_PARAM_UV;
             break;
            
        case MATH_NODE_ADD:
            inst.op = IR_OP_ADD;
            inst.op1_id = node->inputs[0];
            inst.op2_id = node->inputs[1];
            break;
            
        case MATH_NODE_SUB:
            inst.op = IR_OP_SUB;
            inst.op1_id = node->inputs[0];
            inst.op2_id = node->inputs[1];
            break;

        case MATH_NODE_MUL:
            inst.op = IR_OP_MUL;
            inst.op1_id = node->inputs[0];
            inst.op2_id = node->inputs[1];
            break;

        case MATH_NODE_DIV:
            inst.op = IR_OP_DIV;
            inst.op1_id = node->inputs[0];
            inst.op2_id = node->inputs[1];
            break;
            
        case MATH_NODE_SIN:
            inst.op = IR_OP_SIN;
            inst.op1_id = node->inputs[0];
            break;

        case MATH_NODE_COS:
            inst.op = IR_OP_COS;
            inst.op1_id = node->inputs[0];
            break;

        default:
            // Unknown node type
            break;
    }

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

    // Traverse all nodes
    // Note: In a real compiler, we would only traverse from the "Output" node.
    // Here we traverse everything, similar to the previous implementation.
    for (uint32_t i = 0; i < graph->node_count; ++i) {
        const MathNode* n = math_graph_get_node((MathGraph*)graph, i);
        if (n && n->type != MATH_NODE_NONE) {
            generate_ir_node(graph, i, &ir, visited, &visited_count);
        }
    }
    
    // Determine Output Node (Last non-free node)
    int last_id = -1;
    for (int i = (int)graph->node_count - 1; i >= 0; --i) {
        const MathNode* n = math_graph_get_node((MathGraph*)graph, i);
        if (n && n->type != MATH_NODE_NONE) {
            last_id = i;
            break;
        }
    }

    if (last_id != -1) {
        if (ir.instruction_count < ir.instruction_capacity) {
            IrInstruction ret_inst = { .op = IR_OP_RETURN, .id = 0, .op1_id = last_id };
            ir.instructions[ir.instruction_count++] = ret_inst;
        }
    }

    free(visited);
    return ir;
}

static void free_ir(ShaderIR* ir) {
    if (ir->instructions) {
        free(ir->instructions);
    }
}

char* math_graph_transpile_glsl(const MathGraph* graph, TranspilerMode mode) {
    if (!graph) return NULL; 

    // Phase 1: Generate IR
    ShaderIR ir = math_graph_to_ir(graph);

    // Phase 2: Emit GLSL
    char* glsl = ir_to_glsl(&ir, mode);

    // Cleanup
    free_ir(&ir);

    return glsl;
}