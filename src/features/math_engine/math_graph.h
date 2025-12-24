#ifndef MATH_GRAPH_H
#define MATH_GRAPH_H

#include "foundation/memory/arena.h"
#include <stdint.h>
#include <stdbool.h>

// --- Types ---

typedef uint32_t MathNodeId;
#define MATH_NODE_INVALID_ID 0xFFFFFFFF

typedef enum MathNodeType {
    MATH_NODE_NONE = 0, // Slot is empty/free
    MATH_NODE_VALUE,
    MATH_NODE_ADD,
    MATH_NODE_SUB,
    MATH_NODE_MUL,
    MATH_NODE_DIV,
    MATH_NODE_SIN,
    MATH_NODE_COS,
    MATH_NODE_TIME, 
    MATH_NODE_MOUSE,
    MATH_NODE_UV,
    MATH_NODE_TEXTURE_PARAM,
    MATH_NODE_TEXTURE_SAMPLE,
    MATH_NODE_OUTPUT, 
    MATH_NODE_SURFACE_GRID,
    MATH_NODE_COUNT
} MathNodeType; // REFLECT

typedef enum MathDataType {
    MATH_DATA_TYPE_UNKNOWN = 0,
    MATH_DATA_TYPE_FLOAT,
    MATH_DATA_TYPE_VEC2,
    MATH_DATA_TYPE_VEC3,
    MATH_DATA_TYPE_VEC4,
    MATH_DATA_TYPE_SAMPLER2D
} MathDataType; // REFLECT

#define MATH_NODE_MAX_INPUTS 4
#define MATH_NODE_NAME_MAX 32

// Opaque Handles
typedef struct MathNode MathNode;
typedef struct MathGraph MathGraph;

// --- API ---

// Create a new graph instance.
// Allocates the MathGraph struct from the provided arena.
MathGraph* math_graph_create(MemoryArena* arena);

// Destroy the graph and free internal resources (pool, lookup tables).
// Does NOT free the MathGraph struct itself if it was allocated in an arena.
void math_graph_destroy(MathGraph* graph);

// Create a new node. Returns the ID of the created node.
MathNodeId math_graph_add_node(MathGraph* graph, MathNodeType type);

// Remove a node (marks slot as free).
void math_graph_remove_node(MathGraph* graph, MathNodeId id);

// Remove all nodes.
void math_graph_clear(MathGraph* graph);

// Connect source_node's output to target_node's input_slot.
void math_graph_connect(MathGraph* graph, MathNodeId target, int input_index, MathNodeId source);

// Set local value (for Value nodes).
void math_graph_set_value(MathGraph* graph, MathNodeId id, float value);

// Set node name (allocates string in arena).
void math_graph_set_name(MathGraph* graph, MathNodeId id, const char* name);

// Get the resolved output type of a node.
MathDataType math_graph_get_node_type(MathGraph* graph, MathNodeId id);

// Evaluate a specific node.
float math_graph_evaluate(MathGraph* graph, MathNodeId id);

#endif // MATH_GRAPH_H
