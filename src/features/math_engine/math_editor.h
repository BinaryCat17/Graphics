#pragma once

#include "engine/core/engine.h"
#include "features/math_engine/math_graph.h"
#include "engine/ui/ui_core.h"

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
typedef struct MathEditorState {
    MathGraph graph;
    MemoryArena graph_arena;
    
    // UI State
    UiAsset* ui_asset;
    UiInstance ui_instance; // Manages UI Element memory
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
} MathEditorState;

// API
void math_editor_init(MathEditorState* state, Engine* engine);
// Updates the editor state (Input, UI Layout, Transpilation)
void math_editor_update(MathEditorState* state, Engine* engine);

// Renders the editor UI to the provided scene
void math_editor_render(MathEditorState* state, Scene* scene, const Assets* assets);

// Shuts down the editor and frees resources
void math_editor_shutdown(MathEditorState* state, Engine* engine);