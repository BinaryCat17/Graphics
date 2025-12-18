#ifndef UI_DEF_H
#define UI_DEF_H

#include <stdbool.h>
#include "foundation/meta/reflection.h"
#include "foundation/memory/arena.h"

typedef struct { float x, y, w, h; } Rect;

typedef struct InputState {
    float mouse_x, mouse_y;
    bool mouse_down;
    bool mouse_clicked;
    float scroll_dx, scroll_dy;
} InputState;

// --- UI DEFINITION (TEMPLATE) ---
// This structure maps 1:1 to the YAML configuration. 
// It is immutable at runtime.

typedef enum UiNodeType {
    UI_NODE_PANEL,
    UI_NODE_LABEL,
    UI_NODE_BUTTON,
    UI_NODE_SLIDER,
    UI_NODE_CHECKBOX,
    UI_NODE_LIST,      // New: For iterating over arrays
    UI_NODE_CONTAINER, // For grouping without rendering
    UI_NODE_CURVE      // SDF Bezier Curve (replaces CUSTOM)
} UiNodeType;

typedef enum UiLayoutType {
    UI_LAYOUT_COLUMN,
    UI_LAYOUT_ROW,
    UI_LAYOUT_OVERLAY, // Stack on top of each other
    UI_LAYOUT_DOCK     // Fill available space
} UiLayoutType;

typedef struct UiDef {
    // Memory Management (Root owns the arena)
    MemoryArena arena; 

    UiNodeType type;
    UiLayoutType layout;
    
    // Identity
    char* id;
    
    // Appearance (Static)
    char* style_name;
    
    // Content / Binding (Strings can contain "{binding}")
    char* text; 
    char* bind_source; // For Sliders/Inputs: property name to bind to
    char* data_source; // For Lists/Containers: property name to use as context
    char* count_source; // For Lists: property name for the item count
    
    // Geometry Bindings (Overrides layout props if set)
    char* x_source;
    char* y_source;
    char* w_source;
    char* h_source;
    
    // Curve Bindings (P0=Start, P3=End)
    char* u1_source;
    char* v1_source;
    char* u2_source;
    char* v2_source;

    // List specific
    struct UiDef* item_template; // Template for list items
    
    // Layout props
    float width, height; // < 0 means auto/fill
    float padding;
    float spacing;
    
    bool draggable; // Enables drag interaction

    // Slider props
    float min_value;
    float max_value;
    
    // Children
    struct UiDef** children;
    size_t child_count;
    
} UiDef;

// API
UiDef* ui_def_create(MemoryArena* arena, UiNodeType type);
void ui_def_free(UiDef* def);

// --- UI VIEW (INSTANCE) ---
// This is the live graph that represents the current frame state.

typedef struct UiView {
    const UiDef* def;     // Pointer to the template that spawned this view
    
    // Hierarchy
    struct UiView* parent;
    struct UiView** children;
    size_t child_count;
    size_t child_capacity;
    
    // Data Context (Scope)
    void* data_ptr;       // Pointer to the C struct instance for this view
    const MetaStruct* meta; // Type info for data_ptr
    
    // State
    int id_hash;          // Unique ID for IMGUI interaction
    Rect rect;            // Computed layout rect
    
    // Bindings Cache
    char* cached_text;    // Resolved text (e.g. "Value: 5.0")
    float cached_value;   // Last read float value (for diffing)
    
    // Interaction State
    bool is_hovered;
    bool is_pressed;
    
} UiView;

UiView* ui_view_create(const UiDef* def, void* root_data, const MetaStruct* root_type);
void ui_view_free(UiView* view);

// The core function: Synchronizes View with Data
void ui_view_update(UiView* view);

// Process Input (Hit Testing & Interaction)
void ui_view_process_input(UiView* view, const InputState* input);

#endif // UI_DEF_H
