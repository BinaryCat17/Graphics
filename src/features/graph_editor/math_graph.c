#include "math_graph.h"
#include "foundation/logger/logger.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

// --- Helper: Get Node ---
MathNode* math_graph_get_node(MathGraph* graph, MathNodeId id) {
    if (!graph || !graph->nodes || id >= graph->node_count) return NULL;
    // Check if slot is actually valid
    if (graph->nodes[id].type == MATH_NODE_NONE) return NULL;
    return &graph->nodes[id];
}

// --- Init / Clear ---

void math_graph_init(MathGraph* graph, MemoryArena* arena) {
    if (!graph || !arena) return;
    memset(graph, 0, sizeof(MathGraph));
    graph->arena = arena;
    
    // Initial capacity
    graph->node_capacity = 32; 
    graph->nodes = arena_alloc_zero(arena, sizeof(MathNode) * graph->node_capacity);
    graph->node_count = 0;
}

void math_graph_clear(MathGraph* graph) {
    if (!graph) return;
    // We can't free arena memory individually, but we can reset the struct.
    // The arena itself must be reset externally if we want to reclaim memory.
    // However, if we just want to clear the graph logic:
    if (graph->nodes) {
        memset(graph->nodes, 0, sizeof(MathNode) * graph->node_capacity);
    }
    graph->node_count = 0;
}

// --- Node Management ---

MathNodeId math_graph_add_node(MathGraph* graph, MathNodeType type) {
    if (!graph || !graph->arena) return MATH_NODE_INVALID_ID;

    // 1. Find a free slot (simple linear search for now, optimization: free list)
    /* 
       Optimization Note: In a professional engine, we'd use a free-list.
       For <1000 nodes, we can just append or linear scan. 
       Let's Stick to Append-Only + Re-use if at end for now to keep indices stable and code simple.
       Actually, let's just Append. Re-using slots requires Generation IDs to avoid stale handle bugs.
       Since we don't have Generation IDs in MathNodeId yet, we strictly Append.
    */

    if (graph->node_count >= graph->node_capacity) {
        // Resize
        uint32_t new_cap = graph->node_capacity * 2;
        MathNode* new_nodes = arena_alloc_zero(graph->arena, sizeof(MathNode) * new_cap);
        
        if (!new_nodes) {
            LOG_ERROR("MathGraph: Out of memory during resize!");
            return MATH_NODE_INVALID_ID;
        }

        // Copy old data
        memcpy(new_nodes, graph->nodes, sizeof(MathNode) * graph->node_count);
        
        // Update graph
        graph->nodes = new_nodes;
        graph->node_capacity = new_cap;
        LOG_INFO("MathGraph: Resized to capacity %d", new_cap);
    }

    MathNodeId id = graph->node_count++;
    MathNode* node = &graph->nodes[id];
    
    node->id = id;
    node->type = type;
    node->dirty = true;
    
    // Initialize defaults
    for(int i=0; i<MATH_NODE_MAX_INPUTS; ++i) {
        node->inputs[i] = MATH_NODE_INVALID_ID;
    }
    
    // Set default name
    char buf[32];
    snprintf(buf, 32, "Node_%d", id);
    math_graph_set_name(graph, id, buf);

    return id;
}

void math_graph_set_name(MathGraph* graph, MathNodeId id, const char* name) {
    MathNode* node = math_graph_get_node(graph, id);
    if (!node || !graph->arena) return;
    
    node->name = arena_push_string(graph->arena, name);
}

void math_graph_remove_node(MathGraph* graph, MathNodeId id) {
    MathNode* node = math_graph_get_node(graph, id);
    if (!node) return;
    
    // Mark as free
    node->type = MATH_NODE_NONE;
    
    // Iterate all nodes to remove connections TO this node
    // (Optimization: Maintain back-links or connection list)
    for (uint32_t i = 0; i < graph->node_count; ++i) {
        if (graph->nodes[i].type == MATH_NODE_NONE) continue;
        
        for (int k = 0; k < MATH_NODE_MAX_INPUTS; ++k) {
            if (graph->nodes[i].inputs[k] == id) {
                graph->nodes[i].inputs[k] = MATH_NODE_INVALID_ID;
                graph->nodes[i].dirty = true;
            }
        }
    }
}

void math_graph_connect(MathGraph* graph, MathNodeId target_id, int input_index, MathNodeId source_id) {
    if (input_index < 0 || input_index >= MATH_NODE_MAX_INPUTS) return;
    
    MathNode* target = math_graph_get_node(graph, target_id);
    MathNode* source = math_graph_get_node(graph, source_id); // Verify source exists
    
    if (target && source) {
        target->inputs[input_index] = source_id;
        target->dirty = true;
    } else if (target && source_id == MATH_NODE_INVALID_ID) {
        // Disconnect
        target->inputs[input_index] = MATH_NODE_INVALID_ID;
        target->dirty = true;
    }
}

void math_graph_set_value(MathGraph* graph, MathNodeId id, float value) {
    MathNode* node = math_graph_get_node(graph, id);
    if (!node) return;
    
    if (fabsf(node->value - value) > 1e-6f) {
        node->value = value;
        node->dirty = true;
    }
}

// --- Evaluation ---

float math_graph_evaluate(MathGraph* graph, MathNodeId id) {
    MathNode* node = math_graph_get_node(graph, id);
    if (!node) return 0.0f;
    
    // Simple caching could go here, checking dirty flags.
    // For now, let's just evaluate inputs recursively.
    
    float v[MATH_NODE_MAX_INPUTS] = {0};
    for(int i=0; i<MATH_NODE_MAX_INPUTS; ++i) {
        if (node->inputs[i] != MATH_NODE_INVALID_ID) {
            v[i] = math_graph_evaluate(graph, node->inputs[i]);
        }
    }
    
    float result = 0.0f;
    switch (node->type) {
        case MATH_NODE_VALUE: result = node->value; break;
        case MATH_NODE_ADD: result = v[0] + v[1]; break;
        case MATH_NODE_SUB: result = v[0] - v[1]; break;
        case MATH_NODE_MUL: result = v[0] * v[1]; break;
        case MATH_NODE_DIV: result = (v[1] != 0.0f) ? v[0] / v[1] : 0.0f; break;
        case MATH_NODE_SIN: result = sinf(v[0]); break;
        case MATH_NODE_COS: result = cosf(v[0]); break;
        case MATH_NODE_TIME: result = 0.0f; break; // Needs global context
        case MATH_NODE_UV:   result = 0.5f; break; // Needs global context
        default: break;
    }
    
    node->cached_output = result;
    node->dirty = false;
    return result;
}