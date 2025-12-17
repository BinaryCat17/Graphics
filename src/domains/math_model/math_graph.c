#include "math_graph.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

void math_graph_init(MathGraph* graph) {
    if (!graph) return;
    memset(graph, 0, sizeof(MathGraph));
}

static void free_node(MathNode* node) {
    if (!node) return;
    free(node->inputs);
    free(node->name);
    free(node);
}

void math_graph_dispose(MathGraph* graph) {
    if (!graph) return;
    for (size_t i = 0; i < graph->node_count; ++i) {
        free_node(graph->nodes[i]);
    }
    free(graph->nodes);
    memset(graph, 0, sizeof(MathGraph));
}

MathNode* math_graph_add_node(MathGraph* graph, MathNodeType type) {
    if (!graph) return NULL;
    
    if (graph->node_count == graph->node_capacity) {
        size_t new_cap = graph->node_capacity == 0 ? 16 : graph->node_capacity * 2;
        MathNode** new_nodes = (MathNode**)realloc(graph->nodes, new_cap * sizeof(MathNode*));
        if (!new_nodes) return NULL;
        graph->nodes = new_nodes;
        graph->node_capacity = new_cap;
    }
    
    MathNode* node = (MathNode*)calloc(1, sizeof(MathNode));
    if (!node) return NULL;
    
    node->id = (int)graph->node_count + 1;
    node->type = type;
    node->dirty = 1;
    
    // Allocate space for inputs based on type
    // This is a simplified approach; in a real system this might be dynamic or type-dependent tables
    size_t input_slots = 0;
    switch (type) {
        case MATH_NODE_VALUE: input_slots = 0; break;
        case MATH_NODE_ADD:
        case MATH_NODE_SUB:
        case MATH_NODE_MUL:
        case MATH_NODE_DIV: input_slots = 2; break;
        case MATH_NODE_SIN:
        case MATH_NODE_COS: input_slots = 1; break;
        default: input_slots = 0; break;
    }
    
    if (input_slots > 0) {
        node->inputs = (MathNode**)calloc(input_slots, sizeof(MathNode*));
        node->input_count = input_slots;
    }
    
    graph->nodes[graph->node_count++] = node;
    return node;
}

void math_graph_connect(MathNode* target, size_t input_index, MathNode* source) {
    if (!target || input_index >= target->input_count) return;
    target->inputs[input_index] = source;
    target->dirty = 1;
}

void math_graph_set_value(MathNode* node, float value) {
    if (!node || node->type != MATH_NODE_VALUE) return;
    if (fabsf(node->value - value) > 1e-6f) {
        node->value = value;
        node->dirty = 1;
    }
}

float math_graph_evaluate(MathNode* node) {
    if (!node) return 0.0f;
    if (!node->dirty) return node->value;
    
    // Recursive evaluation (naive, no cycle detection yet)
    float v0 = (node->input_count > 0 && node->inputs[0]) ? math_graph_evaluate(node->inputs[0]) : 0.0f;
    float v1 = (node->input_count > 1 && node->inputs[1]) ? math_graph_evaluate(node->inputs[1]) : 0.0f;
    
    switch (node->type) {
        case MATH_NODE_VALUE: break; // Already set
        case MATH_NODE_ADD: node->value = v0 + v1; break;
        case MATH_NODE_SUB: node->value = v0 - v1; break;
        case MATH_NODE_MUL: node->value = v0 * v1; break;
        case MATH_NODE_DIV: node->value = (v1 != 0.0f) ? v0 / v1 : 0.0f; break;
        case MATH_NODE_SIN: node->value = sinf(v0); break;
        case MATH_NODE_COS: node->value = cosf(v0); break;
        default: break;
    }
    
    node->dirty = 0;
    return node->value;
}

void math_graph_update(MathGraph* graph) {
    if (!graph) return;
    // In a real dependency graph, we would topologically sort or propagate dirty flags.
    // Here we just re-evaluate everything that is dirty, assuming tree structure or manual dirty propagation.
    // For a proper graph, we need to invalidate children when parents change. 
    // This simple implementation pulls from outputs, but doesn't push dirty flags down.
    // To make it fully reactive, setting a value should mark all outputs as dirty.
    
    // For now, let's just force re-evaluation of everything or let the user pull.
    // Actually, 'evaluate_node' handles the pull.
    // We just need to reset dirty if we want to support push updates later.
}
