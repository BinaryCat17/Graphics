#ifndef MATH_GRAPH_H
#define MATH_GRAPH_H

#include <stddef.h>

typedef enum MathNodeType {
    MATH_NODE_VALUE = 0,
    MATH_NODE_ADD,
    MATH_NODE_SUB,
    MATH_NODE_MUL,
    MATH_NODE_DIV,
    MATH_NODE_SIN,
    MATH_NODE_COS,
    // Add more types as needed
    MATH_NODE_SURFACE_GRID // Generates geometry
} MathNodeType;

typedef struct MathNode {
    int id; // REFLECT
    MathNodeType type; // REFLECT
    float value; // REFLECT
    int dirty;   // REFLECT
    
    // Inputs (dependencies)
    struct MathNode** inputs;
    size_t input_count;
    
    // User data (e.g. name for UI)
    char* name; // REFLECT
} MathNode;

typedef struct MathGraph {
    MathNode** nodes;
    size_t node_count;
    size_t node_capacity;
} MathGraph;

void math_graph_init(MathGraph* graph);
void math_graph_dispose(MathGraph* graph);

MathNode* math_graph_add_node(MathGraph* graph, MathNodeType type);
void math_graph_connect(MathNode* target, size_t input_index, MathNode* source);
void math_graph_set_value(MathNode* node, float value);

// Propagates dirty flags and recomputes values
void math_graph_update(MathGraph* graph);

#endif // MATH_GRAPH_H
