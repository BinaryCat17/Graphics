#ifndef MATH_EDITOR_INTERNAL_H
#define MATH_EDITOR_INTERNAL_H

#include "engine/ui/ui_core.h"
#include "engine/ui/ui_assets.h"
#include "engine/ui/ui_input.h"
#include "engine/ui/ui_renderer.h"
#include "features/math_engine/math_graph.h"

// ViewModel for a Node in the Editor
typedef struct MathNodeView {
    MathNodeId node_id;     // REFLECT
    float x;                // REFLECT
    float y;                // REFLECT
    
    // Cached Data for UI Binding (ViewModel pattern)
    char name[32];          // REFLECT
    float value;            // REFLECT (Input/Output preview)
} MathNodeView;

// The State of the Graph Editor Feature
typedef struct MathEditor {
    MathGraph* graph;
    MemoryArena graph_arena;
    
    // UI State
    UiAsset* ui_asset;
    UiInstance* ui_instance; // Manages UI Element memory
    UiInputContext* input_ctx;
    
    // ViewModel Data
    // We store views in a parallel array/pool. For simplicity, dynamic array in arena.
    MathNodeView* node_views;   // REFLECT
    uint32_t node_view_count;   // REFLECT
    uint32_t node_view_cap;

    // Selection
    MathNodeId selected_node_id;
    bool selection_dirty; 
    bool graph_dirty;
    uint32_t current_pipeline; // Vulkan Compute Pipeline ID

    // UI Binding for Inspector (Polymorphic List of 0 or 1 item)
    MathNode* selected_nodes[1]; // REFLECT
    int selected_nodes_count;    // REFLECT
} MathEditor;

#endif // MATH_EDITOR_INTERNAL_H
