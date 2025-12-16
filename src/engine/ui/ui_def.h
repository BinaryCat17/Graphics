#ifndef UI_DEF_H
#define UI_DEF_H

#include "foundation/meta/reflection.h"

typedef struct { float x, y, w, h; } Rect;

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
    UI_NODE_CUSTOM     // For custom rendering callbacks
} UiNodeType;

typedef enum UiLayoutType {
    UI_LAYOUT_COLUMN,
    UI_LAYOUT_ROW,
    UI_LAYOUT_OVERLAY, // Stack on top of each other
    UI_LAYOUT_DOCK     // Fill available space
} UiLayoutType;

typedef struct UiDef {
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
    
    // List specific
    struct UiDef* item_template; // Template for list items
    
    // Layout props
    float width, height; // < 0 means auto/fill
    float padding;
    float spacing;
    
    // Children
    struct UiDef** children;
    size_t child_count;
    
} UiDef;

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
    
} UiView;

// API
UiDef* ui_def_create(UiNodeType type);
void ui_def_free(UiDef* def);

UiView* ui_view_create(const UiDef* def, void* root_data, const MetaStruct* root_type);
void ui_view_free(UiView* view);

// The core function: Synchronizes View with Data
void ui_view_update(UiView* view);

#endif // UI_DEF_H
