#include "math_graph.h"
#include "foundation/logger/logger.h"
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
    for (int i = 0; i < graph->node_count; ++i) {
        free_node(graph->nodes[i]);
    }
    free(graph->nodes);
    free(graph->_wire_pool);
    free(graph->wires);
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
    // We always evaluate to ensure changes propagate. 
    // (Optimization: Could use a frame-ID based cache if needed later)
    
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
        case MATH_NODE_UV:  node->value = 0.5f; break; // Dummy preview value
        case MATH_NODE_TIME: node->value = 0.0f; break; // Placeholder
        default: break;
    }
    
    node->dirty = 0;
    return node->value;
}

void math_graph_update(MathGraph* graph) {
    if (!graph) return;
    for (int i = 0; i < graph->node_count; ++i) {
        if (graph->nodes[i]) {
            // Evaluate dirty nodes (simple propagation)
            math_graph_evaluate(graph->nodes[i]);
        }
    }
}

void math_graph_update_visuals(MathGraph* graph, bool log_debug) {
    if (!graph) return;
    
    // 1. Count Wires
    int count = 0;
    for (int i = 0; i < graph->node_count; ++i) {
        if (graph->nodes[i]) {
            count += (int)graph->nodes[i]->input_count;
        }
    }
    
    // 2. Reallocate if needed
    if (count != graph->wire_count) {
        free(graph->_wire_pool);
        free(graph->wires);
        
        if (count > 0) {
            graph->_wire_pool = (VisualWire*)malloc(sizeof(VisualWire) * count);
            graph->wires = (VisualWire**)malloc(sizeof(VisualWire*) * count);
        } else {
            graph->_wire_pool = NULL;
            graph->wires = NULL;
        }
        graph->wire_count = count;
    }
    
    if (count == 0) return;
    
    // 3. Populate
    int idx = 0;
    const float node_w = 150.0f; // Matches editor.yaml
    const float port_y_off = 45.0f; // Header (25) + Padding
    const float port_spacing = 25.0f; // Approx line height
    
    for (int i = 0; i < graph->node_count; ++i) {
        MathNode* target = graph->nodes[i];
        if (!target) continue;
        
        for (size_t k = 0; k < target->input_count; ++k) {
            MathNode* source = target->inputs[k];
            if (source) {
                // Calculate endpoints
                // Source (Output): Right side, middle-ish
                float sx = source->x + node_w;
                float sy = source->y + port_y_off; 
                
                // Target (Input): Left side, stacked
                float tx = target->x;
                float ty = target->y + port_y_off + (k * port_spacing);
                
                // Bounding Box
                float min_x = (sx < tx ? sx : tx) - 50.0f; // Padding for curve
                float min_y = (sy < ty ? sy : ty) - 50.0f;
                float max_x = (sx > tx ? sx : tx) + 50.0f;
                float max_y = (sy > ty ? sy : ty) + 50.0f;
                
                float w = max_x - min_x;
                float h = max_y - min_y;
                
                VisualWire* wire = &graph->_wire_pool[idx];
                graph->wires[idx] = wire;
                
                wire->x = min_x;
                wire->y = min_y;
                wire->width = w;
                wire->height = h;
                
                // UVs (Normalized 0..1 in BBox)
                wire->u1 = (sx - min_x) / w;
                wire->v1 = (sy - min_y) / h;
                wire->u2 = (tx - min_x) / w;
                wire->v2 = (ty - min_y) / h;

                if (log_debug) {
                    LOG_DEBUG("Wire [%d]: Start(%.1f, %.1f) End(%.1f, %.1f) BBox(%.1f, %.1f, %.1f, %.1f)",
                        idx, sx, sy, tx, ty, min_x, min_y, w, h);
                }
                
                idx++;
            }
        }
    }
}
