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

// Component: 3D Mesh
typedef struct SceneMeshSpec {
    StringId mesh_id;     // REFLECT
    StringId material_id; // REFLECT
} SceneMeshSpec;

// Binding V2
typedef struct SceneBindingSpec {
    char* target; // REFLECT ("text", "style.color", "transform.position.x")
    char* source; // REFLECT ("my_data.value")
} SceneBindingSpec;

struct SceneNodeSpec {
    // 1. Identity
    StringId id;            // REFLECT
    UiKind kind;            // REFLECT
    uint32_t flags;         // REFLECT(UiFlags)
    
    // 2. Components
    SceneTransformSpec transform; // REFLECT
    UiLayoutSpec layout;          // REFLECT
    UiStyleSpec style;            // REFLECT
    SceneMeshSpec mesh;           // REFLECT
    
    // 3. Data Bindings (V2)
    struct SceneBindingSpec* bindings; // REFLECT
    size_t binding_count;              // REFLECT
    
    // Legacy / Collections (To be refactored later, keeping for now as they are structural)
    char* collection;       // REFLECT
    char* template_selector;// REFLECT

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

typedef struct SceneTemplate SceneTemplate;

struct SceneTemplate {
    char* name;
    SceneNodeSpec* spec;
    struct SceneTemplate* next;
};

struct SceneAsset {
    MemoryArena arena;
    SceneNodeSpec* root;
    SceneTemplate* templates;
};

// --- SCENE TREE (The Living Tree) ---
// Created from a SceneAsset + Data Context.
// Managed by a SceneTree container.

// Optimized Runtime Binding
typedef enum SceneBindingTarget {
    BINDING_TARGET_NONE = 0,
    BINDING_TARGET_TEXT,
    BINDING_TARGET_VISIBLE,
    
    // Layout
    BINDING_TARGET_LAYOUT_X,
    BINDING_TARGET_LAYOUT_Y,
    BINDING_TARGET_LAYOUT_WIDTH,
    BINDING_TARGET_LAYOUT_HEIGHT,
    
    // Style
    BINDING_TARGET_STYLE_COLOR,
    
    // Transform
    BINDING_TARGET_TRANSFORM_POS_X,
    BINDING_TARGET_TRANSFORM_POS_Y,
    BINDING_TARGET_TRANSFORM_POS_Z,
    BINDING_TARGET_TRANSFORM_SCALE_X,
    BINDING_TARGET_TRANSFORM_SCALE_Y,
    BINDING_TARGET_TRANSFORM_SCALE_Z
} SceneBindingTarget;

typedef struct SceneBinding {
    SceneBindingTarget target;
    const struct MetaField* source_field;
    size_t source_offset;
} SceneBinding;

struct SceneNode {
    const SceneNodeSpec* spec; // The DNA
    
    // Hierarchy
    struct SceneNode* parent;
    struct SceneNode* first_child;
    struct SceneNode* last_child;
    struct SceneNode* next_sibling;
    struct SceneNode* prev_sibling;
    size_t child_count;
    
    // Data Context
    void* data_ptr;         // Pointer to C struct
    const struct MetaStruct* meta; // Type info

    // Bindings (V2)
    SceneBinding* bindings;
    size_t binding_count;
    
    // Commands (Resolved at creation)
    StringId on_click_cmd_id;
    StringId on_change_cmd_id;

    // --- TRANSFORM SYSTEM (Phase 3) ---
    Mat4 local_matrix;    // T * R * S
    Mat4 world_matrix;    // ParentWorld * Local
    bool is_dirty;        // If true, recalculate world_matrix
    
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


struct SceneTree {
    MemoryArena arena;
    MemoryPool* element_pool;
    SceneNode* root;
    SceneAsset* assets;
};

// --- Internal Helper Functions ---
void ui_bind_read_string(void* data, const MetaField* field, char* out_buf, size_t buf_size);
SceneNodeSpec* scene_asset_push_node(SceneAsset* asset);

// Binding V2 Helpers
const SceneBinding* scene_node_get_binding(const SceneNode* node, SceneBindingTarget target);
void scene_node_write_binding_float(SceneNode* node, SceneBindingTarget target, float value);
void scene_node_write_binding_string(SceneNode* node, SceneBindingTarget target, const char* value);

#endif // UI_INTERNAL_H
