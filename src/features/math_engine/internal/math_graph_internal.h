#ifndef MATH_GRAPH_INTERNAL_H
#define MATH_GRAPH_INTERNAL_H

#include "../math_graph.h"
#include "foundation/memory/pool.h"
#include "foundation/math/coordinate_systems.h"

// --- Internal Types ---

struct MathNode {
    MathNodeId id;          // REFLECT
    MathNodeType type;      // REFLECT
    
    // Logic Data
    float value;            // REFLECT
    bool dirty;             // REFLECT
    float cached_output;    // Last calculated result
    
    // Connections (Dependencies)
    // Stores the IDs of the nodes connected to input slots.
    MathNodeId inputs[MATH_NODE_MAX_INPUTS]; 
    
    // UI / Editor Data (Moved to MathNodeView in Editor)
    char name[32];          // REFLECT
};

struct MathGraph {
    struct MemoryPool* node_pool; // From foundation/memory/pool.h
    
    // Indirection table: ID -> MathNode*
    // This array grows, but the nodes themselves stay stable in the pool.
    MathNode** node_ptrs;    // REFLECT
    uint32_t node_count;     // REFLECT
    uint32_t node_capacity;  // Capacity
};

// --- Internal API ---

// Get a pointer to the node data.
// Exposed internally for the Editor/Feature implementation, but hidden from public API.
MathNode* math_graph_get_node(MathGraph* graph, MathNodeId id);

#endif // MATH_GRAPH_INTERNAL_H
