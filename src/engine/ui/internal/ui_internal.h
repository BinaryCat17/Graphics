#ifndef UI_INTERNAL_H
#define UI_INTERNAL_H

#include "../ui_core.h"
#include "../ui_assets.h"
#include "../ui_input.h"
#include "../ui_renderer.h"
#include "foundation/memory/arena.h"
#include "foundation/memory/pool.h"
#include "foundation/string/string_id.h"
#include "foundation/math/coordinate_systems.h"

// --- UI SPECIFICATION (The DNA) ---
// Pure data. Allocated inside a UiAsset arena. Read-only at runtime.

struct UiNodeSpec {
    // 1. Identity & Behavior
    StringId id;            // REFLECT
    UiKind kind;            // REFLECT
    UiLayoutStrategy layout;// REFLECT
    UiLayer layer;          // REFLECT
    uint32_t flags;         // REFLECT
    
    // 3. Styling (Reference to style sheet, not implemented yet)
    Vec4 color;             // REFLECT
    Vec4 hover_color;       // REFLECT
    Vec4 active_color;      // REFLECT
    float active_tint;      // REFLECT
    float hover_tint;       // REFLECT
    Vec4 text_color;        // REFLECT
    float text_scale;       // REFLECT
    Vec4 caret_color;       // REFLECT
    float caret_width;      // REFLECT
    float caret_height;     // REFLECT
    float animation_speed;  // REFLECT
    
    // 9-Slice Sizing (if kind == UI_KIND_CONTAINER and texture is used)
    float border_l, border_t, border_r, border_b; // REFLECT
    float corner_radius;    // REFLECT
    float tex_w, tex_h;     // REFLECT
    StringId texture_id;    // REFLECT
    
    // 4. Data Bindings (Sources)
    char* text_source;      // REFLECT
    char* value_source;     // REFLECT
    char* visible_source;   // REFLECT
    char* bind_collection;  // REFLECT
    char* template_selector;// REFLECT
    
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
    float split_ratio;      // REFLECT

    // 6. Hierarchy
    struct UiNodeSpec* item_template; // REFLECT
    struct UiNodeSpec** children;     // REFLECT
    size_t child_count;               // REFLECT
    
    // 7. Commands
    StringId on_click_cmd;  // REFLECT
    StringId on_change_cmd; // REFLECT
};

// --- UI ASSET (The Resource) ---
// Owns the memory. Created by the Parser.

struct UiTemplate {
    char* name;
    UiNodeSpec* spec;
    struct UiTemplate* next;
};

struct UiAsset {
    MemoryArena arena;
    UiNodeSpec* root;
    UiTemplate* templates;
};

// --- UI INSTANCE (The Living Tree) ---
// Created from a UiAsset + Data Context.
// Managed by a UiInstance container.

struct UiElement {
    const UiNodeSpec* spec; // The DNA
    
    // Hierarchy
    struct UiElement* parent;
    struct UiElement* first_child;
    struct UiElement* last_child;
    struct UiElement* next_sibling;
    struct UiElement* prev_sibling;
    size_t child_count;
    
    // Data Context
    void* data_ptr;         // Pointer to C struct
    const struct MetaStruct* meta; // Type info

    // Cached Bindings (Resolved at creation)
    const struct MetaField* bind_text;
    const struct MetaField* bind_visible;
    const struct MetaField* bind_x;
    const struct MetaField* bind_y;
    const struct MetaField* bind_w;
    const struct MetaField* bind_h;
    
    // Commands (Resolved at creation)
    StringId on_click_cmd_id;
    StringId on_change_cmd_id;
    
    // State
    uint32_t flags;       // Runtime flags (copy of spec->flags)
    Rect rect;            // Computed layout relative to parent
    Rect screen_rect;     // Computed screen space (for hit testing)
    
    // Interaction
    bool is_hovered;
    bool is_active;       // Pressed
    bool is_focused;      // Keyboard focus
    
    // Animation State
    Vec4 render_color;    // Animated color
    float hover_t;        // 0.0 -> 1.0 (Interpolation factor)
    
    int cursor_idx;       // Text Input Cursor
    
    // Scrolling State (Internal or Bound)
    float scroll_x;
    float scroll_y;
    
    // Layout State
    float content_w; // Total width of children
    float content_h; // Total height of children

    // Caching
    char cached_text[128]; // For text binding

};


struct UiInstance {
    MemoryArena arena;
    MemoryPool* element_pool;
    UiElement* root;
    UiAsset* assets;
};

// --- Internal Helper Functions ---
void ui_bind_read_string(void* data, const MetaField* field, char* out_buf, size_t buf_size);
UiNodeSpec* ui_asset_push_node(UiAsset* asset);

#endif // UI_INTERNAL_H
