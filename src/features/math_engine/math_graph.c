#include "math_graph.h"
#include "internal/math_graph_internal.h"
#include "foundation/memory/pool.h"
#include "foundation/logger/logger.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h> // for realloc

// --- Helper: Get Node ---
MathNode* math_graph_get_node(MathGraph* graph, MathNodeId id) {
    if (!graph || !graph->node_ptrs || id >= graph->node_count) return NULL;
    MathNode* node = graph->node_ptrs[id];
    if (!node) return NULL; // Should not happen if count is correct, unless deleted?
    if (node->type == MATH_NODE_NONE) return NULL;
    return node;
}

// --- Create / Destroy ---

MathGraph* math_graph_create(MemoryArena* arena) {
    if (!arena) {
        LOG_ERROR("MathGraph: Arena required for creation.");
        return NULL;
    }
    
    MathGraph* graph = (MathGraph*)arena_alloc_zero(arena, sizeof(MathGraph));
    // memset(graph, 0, sizeof(MathGraph)); // arena_alloc_zero already clears it
    
    // Create Pool: Elements are MathNode size, 256 per block
    graph->node_pool = pool_create(sizeof(MathNode), 256);
    
    // Initial capacity for ID table
    graph->node_capacity = 32; 
    graph->node_ptrs = (MathNode**)calloc(graph->node_capacity, sizeof(MathNode*));
    graph->node_count = 0;

    return graph;
}

void math_graph_destroy(MathGraph* graph) {
    if (!graph) return;
    
    if (graph->node_ptrs) {
        free((void*)graph->node_ptrs);
        graph->node_ptrs = NULL;
    }
    
    if (graph->node_pool) {
        pool_destroy(graph->node_pool);
        graph->node_pool = NULL;
    }
    
    graph->node_count = 0;
    graph->node_capacity = 0;
    // Struct itself is in arena, so we don't free it.
}

// --- Node Management ---

MathNodeId math_graph_add_node(MathGraph* graph, MathNodeType type) {
    if (!graph || !graph->node_pool) return MATH_NODE_INVALID_ID;

    // 1. Ensure ID table capacity
    if (graph->node_count >= graph->node_capacity) {
        uint32_t new_cap = graph->node_capacity * 2;
        if (new_cap == 0) new_cap = 32;
        
        MathNode** new_ptrs = (MathNode**)realloc((void*)graph->node_ptrs, sizeof(MathNode*) * new_cap);
        if (!new_ptrs) {
            LOG_ERROR("MathGraph: Out of memory for ID table!");
            return MATH_NODE_INVALID_ID;
        }
        
        // Zero out new slots
        memset((void*)(new_ptrs + graph->node_capacity), 0, (new_cap - graph->node_capacity) * sizeof(MathNode*));
        
        graph->node_ptrs = new_ptrs;
        graph->node_capacity = new_cap;
        LOG_INFO("MathGraph: Resized ID table to %d", new_cap);
    }

    // 2. Alloc from Pool
    MathNode* node = (MathNode*)pool_alloc(graph->node_pool);
    if (!node) {
        LOG_ERROR("MathGraph: Pool exhausted (System OOM)!");
        return MATH_NODE_INVALID_ID;
    }

    MathNodeId id = graph->node_count++;
    graph->node_ptrs[id] = node;
    
    node->id = id;
    node->type = type;
    node->dirty = true;

    // Set default output type
    switch (type) {
        case MATH_NODE_UV:
            node->output_type = MATH_DATA_TYPE_VEC2;
            break;
        case MATH_NODE_TEXTURE_PARAM:
            node->output_type = MATH_DATA_TYPE_SAMPLER2D;
            break;
        case MATH_NODE_TEXTURE_SAMPLE:
        case MATH_NODE_MOUSE:
        case MATH_NODE_SURFACE_GRID:
            node->output_type = MATH_DATA_TYPE_VEC4;
            break;
            node->output_type = MATH_DATA_TYPE_VEC4;
            break;
        default:
            node->output_type = MATH_DATA_TYPE_FLOAT;
            break;
    }
    
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

MathDataType math_graph_get_node_type(MathGraph* graph, MathNodeId id) {
    MathNode* node = math_graph_get_node(graph, id);
    if (!node) return MATH_DATA_TYPE_UNKNOWN;
    return node->output_type;
}

void math_graph_set_name(MathGraph* graph, MathNodeId id, const char* name) {
    MathNode* node = math_graph_get_node(graph, id);
    if (!node) return;
    
    if (name) {
        strncpy(node->name, name, 31);
        node->name[31] = '\0';
    } else {
        node->name[0] = '\0';
    }
}

void math_graph_remove_node(MathGraph* graph, MathNodeId id) {
    MathNode* node = math_graph_get_node(graph, id);
    if (!node) return;
    
    // Remove connections TO this node first
    for (uint32_t i = 0; i < graph->node_count; ++i) {
        // Direct pointer access for speed, skipping NULLs
        MathNode* other = graph->node_ptrs[i];
        if (!other || other->type == MATH_NODE_NONE) continue;
        
        for (int k = 0; k < MATH_NODE_MAX_INPUTS; ++k) {
            if (other->inputs[k] == id) {
                other->inputs[k] = MATH_NODE_INVALID_ID;
                other->dirty = true;
            }
        }
    }
    
    // Free memory
    pool_free(graph->node_pool, node);
    graph->node_ptrs[id] = NULL; // Mark as dead
}

void math_graph_clear(MathGraph* graph) {
    if (!graph) return;
    
    // 1. Reset Pool (Recycles all blocks)
    if (graph->node_pool) {
        pool_clear(graph->node_pool);
    }
    
    // 2. Clear ID Map
    if (graph->node_ptrs && graph->node_capacity > 0) {
        memset((void*)graph->node_ptrs, 0, graph->node_capacity * sizeof(MathNode*));
    }
    
    // 3. Reset Counters
    graph->node_count = 0;
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
        case MATH_NODE_TIME: 
        case MATH_NODE_MOUSE:
        case MATH_NODE_TEXTURE_PARAM:
        case MATH_NODE_TEXTURE_SAMPLE:
            result = 0.0f; break; // GPU only / Global context
        case MATH_NODE_UV:   result = 0.5f; break; // Needs global context
        default: break;
    }
    
    node->cached_output = result;
    node->dirty = false;
    return result;
}
