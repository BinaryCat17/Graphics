#ifndef UI_CORE_H
#define UI_CORE_H

#include <stdbool.h>
#include <stdint.h>
#include "foundation/meta/reflection.h"
#include "foundation/memory/arena.h"
#include "foundation/math/coordinate_systems.h"

// --- CONSTANTS & FLAGS ---

typedef enum UiLayoutStrategy {
    UI_LAYOUT_FLEX_COLUMN, // Vertical stack
    UI_LAYOUT_FLEX_ROW,    // Horizontal stack
    UI_LAYOUT_CANVAS,      // Absolute positioning (Floating nodes)
    UI_LAYOUT_OVERLAY      // Z-stack (Layers)
} UiLayoutStrategy;

typedef enum UiFlags {
    UI_FLAG_NONE        = 0,
    UI_FLAG_CLICKABLE   = 1 << 0,
    UI_FLAG_DRAGGABLE   = 1 << 1, // Updates X/Y bindings on drag
    UI_FLAG_SCROLLABLE  = 1 << 2,
    UI_FLAG_FOCUSABLE   = 1 << 3,
    UI_FLAG_HIDDEN      = 1 << 4,
    UI_FLAG_CLIPPED     = 1 << 5, // Masks children outside bounds
    UI_FLAG_EDITABLE    = 1 << 6  // Supports text input
} UiFlags;

// "Kind" helps the renderer choose a default visual style, 
// but functional behavior is driven by flags.
typedef enum UiKind {
    UI_KIND_CONTAINER, // Generic Rect
    UI_KIND_TEXT,      // Renders text
    UI_KIND_TEXT_INPUT, // Text Field
    UI_KIND_ICON,      // Renders an image/icon
    UI_KIND_CUSTOM     // Custom render callback (e.g. Curves)
} UiKind;

typedef struct Rect { float x, y, w, h; } Rect;

typedef struct InputState {
    float mouse_x, mouse_y;
    bool mouse_down;
    bool mouse_clicked;
    float scroll_dx, scroll_dy;
    
    // Keyboard
    uint32_t last_char; // Unicode codepoint
    int last_key;       // Platform key code
    int last_action;    // Press/Release/Repeat
} InputState;

// --- UI SPECIFICATION (The DNA) ---
// Pure data. Allocated inside a UiAsset arena. Read-only at runtime.

typedef struct UiNodeSpec {
    // 1. Identity & Behavior
    char* id;               // REFLECT
    UiKind kind;            // REFLECT
    UiLayoutStrategy layout;// REFLECT
    uint32_t flags;         // REFLECT
    
    // 3. Styling (Reference to style sheet, not implemented yet)
    char* style_name;       // REFLECT
    Vec4 color;             // REFLECT
    
    // 9-Slice Sizing (if kind == UI_KIND_CONTAINER and texture is used)
    float border_l, border_t, border_r, border_b; // REFLECT
    float tex_w, tex_h;     // REFLECT
    char* texture_path;     // REFLECT
    
    // 4. Data Bindings (Sources)
    char* text_source;      // REFLECT
    char* value_source;     // REFLECT
    char* data_source;      // REFLECT
    
    // 4. Geometry Bindings (For CANVAS layout or manual overrides)
    char* x_source;         // REFLECT
    char* y_source;         // REFLECT
    char* w_source;         // REFLECT
    char* h_source;         // REFLECT

    // 5. Properties (Static defaults)
    char* static_text;      // REFLECT
    float width, height;    // REFLECT
    float padding;          // REFLECT
    float spacing;          // REFLECT

    // 6. Hierarchy
    struct UiNodeSpec* item_template; // REFLECT
    struct UiNodeSpec** children;     // REFLECT
    size_t child_count;               // REFLECT
    
} UiNodeSpec;

// --- UI ASSET (The Resource) ---
// Owns the memory. Created by the Parser.

typedef struct UiAsset {
    MemoryArena arena;
    UiNodeSpec* root;
} UiAsset;

// API for Asset
UiAsset* ui_asset_create(size_t arena_size);
void ui_asset_free(UiAsset* asset);
UiNodeSpec* ui_asset_push_node(UiAsset* asset);


// --- UI INSTANCE (The Living Tree) ---
// Created from a UiAsset + Data Context.
// Managed by a UiInstance container.

typedef struct UiElement {
    const UiNodeSpec* spec; // The DNA
    
    // Hierarchy
    struct UiElement* parent;
    struct UiElement** children;
    size_t child_count;
    // size_t child_capacity; // Not needed if fixed by spec, but needed for dynamic add?
    // If we use Arena, realloc is hard. 
    // Usually we build the tree once. Dynamic children are usually rebuilt from scratch.
    
    // Data Context
    void* data_ptr;         // Pointer to C struct
    const MetaStruct* meta; // Type info
    
    // State
    Rect rect;            // Computed layout relative to parent
    Rect screen_rect;     // Computed screen space (for hit testing)
    
    // Cached Bindings (to detect changes)
    char cached_text[64];
    float cached_value;
    
    // Interaction
    bool is_hovered;
    bool is_active;       // Pressed
    bool is_focused;      // Keyboard focus
    
    int cursor_idx;       // Text Input Cursor
    
    // Scrolling State (Internal or Bound)
    float scroll_x;
    float scroll_y;
    
    // Layout State
    float content_w; // Total width of children
    float content_h; // Total height of children

} UiElement;

typedef struct UiInstance {
    MemoryArena arena;
    UiElement* root;
} UiInstance;

// API for Instance
void ui_instance_init(UiInstance* instance, size_t size);
void ui_instance_destroy(UiInstance* instance);
void ui_instance_reset(UiInstance* instance); // Clears all elements

// Allocates element from instance arena.
UiElement* ui_element_create(UiInstance* instance, const UiNodeSpec* spec, void* data, const MetaStruct* meta);

// Core Loop
void ui_element_update(UiElement* element); // Syncs data

// --- Utils ---
void ui_bind_read_string(void* data, const MetaField* field, char* out_buf, size_t buf_size);

#endif // UI_CORE_H
