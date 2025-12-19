#pragma once

#include "engine/core/engine.h"
#include "features/math_engine/math_graph.h"
#include "engine/ui/ui_core.h"
#include "engine/ui/ui_input.h"

// The State of the Graph Editor Feature
typedef struct MathEditorState {
    MathGraph graph;
    MemoryArena graph_arena;
    
    // UI State
    UiAsset* ui_asset;
    UiInstance ui_instance; // Manages UI Element memory
    UiInputContext input_ctx;
    
    // Selection
    MathNodeId selected_node_id;
    bool selection_dirty; 
    bool graph_dirty;
    uint32_t current_pipeline; // Vulkan Compute Pipeline ID
} MathEditorState;

// API
void math_editor_init(MathEditorState* state, Engine* engine);
void math_editor_update(MathEditorState* state, Engine* engine);
void math_editor_shutdown(MathEditorState* state, Engine* engine);
