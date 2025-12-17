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
    float value; // REFLECT(Observable)
    float x;     // REFLECT(Observable)
    float y;     // REFLECT(Observable)
    int dirty;   // REFLECT
    
    // Inputs (dependencies)
    struct MathNode** inputs;
    size_t input_count;
    
    // User data (e.g. name for UI)
    char* name; // REFLECT
} MathNode;

typedef struct VisualWire {
    float x, y, width, height; // REFLECT
    float u1, v1, u2, v2;      // REFLECT
} VisualWire;

typedef struct MathGraph {
    MathNode** nodes; // REFLECT
    int node_count;   // REFLECT
    int node_capacity;
    
    VisualWire** wires; // REFLECT
    int wire_count;    // REFLECT
    
    VisualWire* _wire_pool; // Internal storage
} MathGraph;

void math_graph_init(MathGraph* graph);
void math_graph_dispose(MathGraph* graph);

MathNode* math_graph_add_node(MathGraph* graph, MathNodeType type);
void math_graph_connect(MathNode* target, size_t input_index, MathNode* source);
void math_graph_set_value(MathNode* node, float value);
float math_graph_evaluate(MathNode* node);

// Propagates dirty flags and recomputes values
void math_graph_update(MathGraph* graph);

// Updates VisualWire array based on node connections
void math_graph_update_visuals(MathGraph* graph);

#endif // MATH_GRAPH_H
