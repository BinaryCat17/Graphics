#ifndef SCENE_TREE_INTERNAL_H
#define SCENE_TREE_INTERNAL_H

#include "../scene.h"
#include "foundation/memory/arena.h"
#include "foundation/memory/pool.h"

// --- COMPONENTS ---

// Component: Transform (Shared with 3D)
typedef struct SceneTransformSpec {
    Vec3 local_position;    // REFLECT
    Vec3 local_rotation;    // REFLECT
    Vec3 local_scale;       // REFLECT
} SceneTransformSpec;

// Component: UI Layout (Shared with 2D)
typedef struct UiLayoutSpec {
    int type;               // REFLECT (UiLayoutStrategy)
    int layer;              // REFLECT (UiLayer)
    float width;            // REFLECT
    float height;           // REFLECT
    float padding;          // REFLECT
    float spacing;          // REFLECT
    float split_ratio;      // REFLECT
    float x;                // REFLECT
    float y;                // REFLECT
} UiLayoutSpec;

// Component: UI Styling
typedef struct UiStyleSpec {
    int render_mode;        // REFLECT (UiRenderMode)
    Vec4 color;             // REFLECT
    Vec4 hover_color;       // REFLECT
    Vec4 active_color;      // REFLECT
    Vec4 text_color;        // REFLECT
    Vec4 caret_color;       // REFLECT
    float active_tint;      // REFLECT
    float hover_tint;       // REFLECT
    float text_scale;       // REFLECT
    float caret_width;      // REFLECT
    float caret_height;     // REFLECT
    float animation_speed;  // REFLECT
    float border_l;         // REFLECT
    float border_t;         // REFLECT
    float border_r;         // REFLECT
    float border_b;         // REFLECT
    float corner_radius;    // REFLECT
    float tex_w;            // REFLECT
    float tex_h;            // REFLECT
    StringId texture;       // REFLECT
} UiStyleSpec;

// Component: 3D Mesh
typedef struct SceneMeshSpec {
    StringId mesh_id;     // REFLECT
    StringId material_id; // REFLECT
} SceneMeshSpec;

// Data Binding V2
typedef struct SceneBindingSpec {
    char* target; // REFLECT
    char* source; // REFLECT
} SceneBindingSpec;

// --- SPECIFICATION ---

struct SceneNodeSpec {
    // 1. Identity
    StringId id;            // REFLECT
    int kind;               // REFLECT (UiKind)
    uint32_t flags;         // REFLECT (UiFlags/SceneNodeFlags)
    
    // 2. Components (Fat Spec for now to support parser)
    SceneTransformSpec transform; // REFLECT
    UiLayoutSpec layout;          // REFLECT
    UiStyleSpec style;            // REFLECT
    SceneMeshSpec mesh;           // REFLECT
    
    // 3. Data Bindings
    SceneBindingSpec* bindings;   // REFLECT
    size_t binding_count;         // REFLECT
    
    // 4. Content & Collections
    char* collection;             // REFLECT
    char* template_selector;      // REFLECT
    char* text;                   // REFLECT
    char* text_source;            // REFLECT

    // 5. Hierarchy
    struct SceneNodeSpec* item_template; // REFLECT
    struct SceneNodeSpec** children;     // REFLECT
    size_t child_count;                  // REFLECT
    
    // 6. Commands
    StringId on_click;            // REFLECT
    StringId on_change;           // REFLECT
    
    // 7. Misc
    StringId provider_id;         // REFLECT
    
    // Extensibility
    void* system_spec; 
};

// --- RUNTIME NODE ---

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

    // --- TRANSFORM SYSTEM ---
    Mat4 local_matrix;    // T * R * S
    Mat4 world_matrix;    // ParentWorld * Local
    
    // --- UI / INTERACTION ---
    Rect rect;            // Computed layout relative to parent
    Rect screen_rect;     // Computed screen space
    Vec4 render_color;    // Animated color
    
    StringId on_click_cmd_id;
    StringId on_change_cmd_id;

    bool is_hovered;
    bool is_active;
    bool is_focused;
    float hover_t;
    int cursor_idx;

    // Scrolling & Content
    float scroll_x;
    float scroll_y;
    float content_w;
    float content_h;

    // Data Binding (Runtime Cache)
    void* ui_bindings; 
    size_t ui_binding_count;
    char cached_text[128];

    // State
    uint32_t flags;       // Runtime flags
};

// --- SCENE TREE ---

struct SceneTree {
    MemoryArena arena;
    MemoryPool* node_pool; 
    SceneNode* root;
    SceneAsset* assets;
};

// --- ASSET ---

typedef struct SceneTemplate {
    char* name;
    SceneNodeSpec* spec;
    struct SceneTemplate* next;
} SceneTemplate;

struct SceneAsset {
    MemoryArena arena;
    SceneNodeSpec* root;
    SceneTemplate* templates;
};

#endif // SCENE_TREE_INTERNAL_H
