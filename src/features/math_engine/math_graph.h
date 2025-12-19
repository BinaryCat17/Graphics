#ifndef MATH_GRAPH_H
#define MATH_GRAPH_H

#include "foundation/memory/arena.h"
#include "foundation/math/coordinate_systems.h" // For Vec2/Rect

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
    MATH_NODE_UV,   
    MATH_NODE_OUTPUT, 
    MATH_NODE_SURFACE_GRID,
    MATH_NODE_COUNT
} MathNodeType;

#define MATH_NODE_MAX_INPUTS 4
#define MATH_NODE_NAME_MAX 32

typedef struct MathNode {
    MathNodeId id;          // REFLECT
    MathNodeType type;      // REFLECT
    
    // Logic Data
    float value;            // REFLECT
    float x, y;             // REFLECT
    bool dirty;             // REFLECT
    float cached_output;    // Last calculated result
    
    // Connections (Dependencies)
    // Stores the IDs of the nodes connected to input slots.
    MathNodeId inputs[MATH_NODE_MAX_INPUTS]; 
    
    // UI / Editor Data
    float ui_x;             // REFLECT
    float ui_y;             // REFLECT
    char name[32];          // REFLECT
} MathNode;

typedef struct MathGraph {
    struct MemoryPool* node_pool; // From foundation/memory/pool.h
    
    // Indirection table: ID -> MathNode*
    // This array grows, but the nodes themselves stay stable in the pool.
    MathNode** node_ptrs;    // REFLECT
    uint32_t node_count;     // REFLECT
    uint32_t node_capacity;  // Capacity
} MathGraph;

// --- API ---

// Initialize graph using the provided arena for all internal allocations.
void math_graph_init(MathGraph* graph, MemoryArena* arena);

// No explicit dispose needed if the arena is managed externally (e.g., per level/app).
// But we can clear the graph struct.
void math_graph_clear(MathGraph* graph);

// Create a new node. Returns the ID of the created node.
MathNodeId math_graph_add_node(MathGraph* graph, MathNodeType type);

// Remove a node (marks slot as free).
void math_graph_remove_node(MathGraph* graph, MathNodeId id);

// Connect source_node's output to target_node's input_slot.
void math_graph_connect(MathGraph* graph, MathNodeId target, int input_index, MathNodeId source);

// Set local value (for Value nodes).
void math_graph_set_value(MathGraph* graph, MathNodeId id, float value);

// Set node name (allocates string in arena).
void math_graph_set_name(MathGraph* graph, MathNodeId id, const char* name);

// Evaluate a specific node.
float math_graph_evaluate(MathGraph* graph, MathNodeId id);

// Get a pointer to the node data (Unsafe: pointer may be invalidated on array resize).
// Use with care, prefer using IDs.
MathNode* math_graph_get_node(MathGraph* graph, MathNodeId id);

#endif // MATH_GRAPH_H
