#ifndef MATH_EDITOR_INTERNAL_H
#define MATH_EDITOR_INTERNAL_H

#include "engine/ui/ui_core.h"
#include "engine/ui/ui_input.h"
#include "engine/ui/ui_renderer.h"
#include "features/math_engine/math_graph.h"

// --- Layout Constants ---
#define NODE_WIDTH          150.0f
#define NODE_HEADER_HEIGHT  32.0f
#define NODE_PORT_SPACING   25.0f
#define NODE_PORT_SIZE      10.0f

// --- ViewModel for a Node in the Editor
typedef struct MathPortView {
    int index;              // REFLECT
} MathPortView;

typedef struct MathNodeView {
    MathNodeId node_id;     // REFLECT
    float x;                // REFLECT
    float y;                // REFLECT
    
    // Cached Data for UI Binding (ViewModel pattern)
    char name[32];          // REFLECT
    float value;            // REFLECT (Input/Output preview)

    MathPortView input_ports[4];  // REFLECT
    int input_ports_count;        // REFLECT

    MathPortView output_ports[1]; // REFLECT
    int output_ports_count;       // REFLECT
} MathNodeView;

typedef struct MathWireView {
    Vec2 start;            // REFLECT
    Vec2 end;              // REFLECT
    Vec4 color;            // REFLECT
    float thickness;       // REFLECT
} MathWireView;

// --- Serialization DTOs (Strict Separation) ---

typedef struct MathNodeLogicBP {
    MathNodeType type;      // REFLECT
    float value;            // REFLECT (Default value)
    
    // Connections: Indices in the blueprint array (-1 = none)
    int input_0;            // REFLECT 
    int input_1;            // REFLECT
    int input_2;            // REFLECT
    int input_3;            // REFLECT
} MathNodeLogicBP;

typedef struct MathNodeLayoutBP {
    float x;                // REFLECT
    float y;                // REFLECT
    char name[32];          // REFLECT
} MathNodeLayoutBP;

typedef struct MathNodeBlueprint {
    MathNodeLogicBP logic;   // REFLECT
    MathNodeLayoutBP layout; // REFLECT
} MathNodeBlueprint;

typedef struct MathGraphBlueprint {
    MathNodeBlueprint** nodes;  // REFLECT
    size_t node_count;          // REFLECT
} MathGraphBlueprint;

typedef struct MathNodePaletteItem {
    char label[32];         // REFLECT
    int type;               // REFLECT (MathNodeType)
} MathNodePaletteItem;

// --- Graph View (The "V" in MVC) ---
typedef struct MathGraphView {
    // ViewModel Data
    MathNodeView* node_views;   // REFLECT
    uint32_t node_views_count;  // REFLECT
    uint32_t node_view_cap;

    MathWireView* wires;        // REFLECT
    uint32_t wires_count;       // REFLECT
    uint32_t wires_cap;
    
    // UI State (Owned by View)
    SceneAsset* ui_asset;
    SceneTree* ui_instance; // Manages UI Element memory
    UiInputContext* input_ctx;

    // Selection
    MathNodeId selected_node_id;
    bool selection_dirty;

    // UI Binding for Inspector (Polymorphic List of 0 or 1 item)
    MathNode** selected_nodes;   // REFLECT
    int selected_nodes_count;    // REFLECT
    
    bool has_selection;          // REFLECT (helper for UI visibility)
    bool no_selection;           // REFLECT (helper for UI visibility)
} MathGraphView;

// The State of the Graph Editor Feature (The "C" in MVC)
typedef struct MathEditor {
    MathGraph* graph;
    MemoryArena graph_arena;
    
    MathGraphView* view; // REFLECT
    
    // Palette Data (Logic/Toolbox)
    MathNodePaletteItem** palette_items; // REFLECT
    size_t palette_items_count;          // REFLECT

    bool graph_dirty;
    uint32_t current_pipeline; // Vulkan Compute Pipeline ID
} MathEditor;

#endif // MATH_EDITOR_INTERNAL_H
