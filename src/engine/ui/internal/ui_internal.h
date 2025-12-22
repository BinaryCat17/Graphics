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

// Component: Transform (Shared with 3D)
typedef struct SceneTransformSpec {
    Vec3 local_position;    // REFLECT
    Vec3 local_rotation;    // REFLECT
    Vec3 local_scale;       // REFLECT
} SceneTransformSpec;

// Component: UI Layout
typedef struct UiLayoutSpec {
    UiLayoutStrategy type;  // REFLECT
    UiLayer layer;          // REFLECT
    
    // Flexbox / Manual Props
    float width;            // REFLECT
    float height;           // REFLECT
    float padding;          // REFLECT
    float spacing;          // REFLECT
    float split_ratio;      // REFLECT
    
    // Legacy / Manual offsets (will be deprecated or mapped to transform)
    float x;                // REFLECT
    float y;                // REFLECT
} UiLayoutSpec;

// Component: UI Styling
typedef struct UiStyleSpec {
    UiRenderMode render_mode; // REFLECT
    
    // Colors
    Vec4 color;             // REFLECT
    Vec4 hover_color;       // REFLECT
    Vec4 active_color;      // REFLECT
    Vec4 text_color;        // REFLECT
    Vec4 caret_color;       // REFLECT
    
    // Modifiers
    float active_tint;      // REFLECT
    float hover_tint;       // REFLECT
    float text_scale;       // REFLECT
    float caret_width;      // REFLECT
    float caret_height;     // REFLECT
    float animation_speed;  // REFLECT
    
    // Geometry / Shape
    float border_l;         // REFLECT
    float border_t;         // REFLECT
    float border_r;         // REFLECT
    float border_b;         // REFLECT
    float corner_radius;    // REFLECT
    
    // Texture
    float tex_w;            // REFLECT
    float tex_h;            // REFLECT
    StringId texture;       // REFLECT
} UiStyleSpec;

struct SceneNodeSpec {
    // 1. Identity
    StringId id;            // REFLECT
    UiKind kind;            // REFLECT
    uint32_t flags;         // REFLECT(UiFlags)
    
    // 2. Components
    SceneTransformSpec transform; // REFLECT
    UiLayoutSpec layout;          // REFLECT
    UiStyleSpec style;            // REFLECT
    
    // 3. Data Bindings
    char* bind_text;        // REFLECT
    char* bind;             // REFLECT
    char* bind_visible;     // REFLECT
    char* collection;       // REFLECT
    char* template_selector;// REFLECT
    
    // Layout Overrides (Bindings)
    char* bind_x;           // REFLECT
    char* bind_y;           // REFLECT
    char* bind_w;           // REFLECT
    char* bind_h;           // REFLECT

    // 4. Content
    char* text;             // REFLECT
    char* text_source;      // REFLECT

    // 5. Hierarchy
    struct SceneNodeSpec* item_template; // REFLECT
    struct SceneNodeSpec** children;     // REFLECT
    size_t child_count;               // REFLECT
    
    // 6. Commands
    StringId on_click;      // REFLECT
    StringId on_change;     // REFLECT
    
    // 7. Misc
    StringId provider_id;   // REFLECT
};

// --- UI ASSET (The Resource) ---
// Owns the memory. Created by the Parser.

typedef struct UiTemplate UiTemplate;

struct UiTemplate {
    char* name;
    SceneNodeSpec* spec;
    struct UiTemplate* next;
};

struct UiAsset {
    MemoryArena arena;
    SceneNodeSpec* root;
    UiTemplate* templates;
};

// --- UI INSTANCE (The Living Tree) ---
// Created from a UiAsset + Data Context.
// Managed by a UiInstance container.

struct UiElement {
    const SceneNodeSpec* spec; // The DNA
    
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
SceneNodeSpec* ui_asset_push_node(UiAsset* asset);

#endif // UI_INTERNAL_H
