#ifndef UI_CORE_H
#define UI_CORE_H

#include <stdbool.h>
#include <stdint.h>
#include "foundation/meta/reflection.h"
#include "foundation/memory/arena.h"
#include "foundation/math/coordinate_systems.h"

#include "foundation/memory/pool.h"
#include "foundation/string/string_id.h"

// --- CONSTANTS & FLAGS ---

typedef enum UiLayoutStrategy {
    UI_LAYOUT_FLEX_COLUMN, // Vertical stack
    UI_LAYOUT_FLEX_ROW,    // Horizontal stack
    UI_LAYOUT_CANVAS,      // Absolute positioning (Floating nodes)
    UI_LAYOUT_SPLIT_H,     // 2-child horizontal split
    UI_LAYOUT_SPLIT_V      // 2-child vertical split
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
    UI_KIND_TEXT_INPUT // Text Field
} UiKind;

typedef enum UiLayer {
    UI_LAYER_NORMAL = 0,
    UI_LAYER_OVERLAY  // Renders last, ignores parent clipping (popups)
} UiLayer;

typedef struct UiElement UiElement;

typedef enum UiEventType {
    UI_EVENT_NONE = 0,
    UI_EVENT_CLICK,         // Triggered on mouse up (if active)
    UI_EVENT_VALUE_CHANGE,  // Triggered when input modifies data
    UI_EVENT_DRAG_START,
    UI_EVENT_DRAG_END
} UiEventType;

typedef struct UiEvent {
    UiEventType type;
    UiElement* target;
} UiEvent;

#include "engine/input/input.h"

// --- UI SPECIFICATION (The DNA) ---
// Pure data. Allocated inside a UiAsset arena. Read-only at runtime.

typedef struct UiNodeSpec {
    // 1. Identity & Behavior
    char* id;               // REFLECT
    UiKind kind;            // REFLECT
    UiLayoutStrategy layout;// REFLECT
    UiLayer layer;          // REFLECT
    uint32_t flags;         // REFLECT
    
    // 3. Styling (Reference to style sheet, not implemented yet)
    Vec4 color;             // REFLECT
    Vec4 hover_color;       // REFLECT
    float animation_speed;  // REFLECT
    
    // 9-Slice Sizing (if kind == UI_KIND_CONTAINER and texture is used)
    float border_l, border_t, border_r, border_b; // REFLECT
    float corner_radius;    // REFLECT
    float tex_w, tex_h;     // REFLECT
    char* texture_path;     // REFLECT
    
    // 4. Data Bindings (Sources)
    char* text_source;      // REFLECT
    char* value_source;     // REFLECT
    char* visible_source;   // REFLECT
    char* bind_collection;  // REFLECT
    
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
    char* on_click_cmd;     // REFLECT
    char* on_change_cmd;    // REFLECT
} UiNodeSpec;

// --- UI ASSET (The Resource) ---
// Owns the memory. Created by the Parser.

typedef struct UiTemplate {
    char* name;
    UiNodeSpec* spec;
    struct UiTemplate* next;
} UiTemplate;

typedef struct UiAsset {
    MemoryArena arena;
    UiNodeSpec* root;
    UiTemplate* templates;
} UiAsset;

// API for Asset
UiAsset* ui_asset_create(size_t arena_size);
void ui_asset_free(UiAsset* asset);
UiNodeSpec* ui_asset_push_node(UiAsset* asset);
UiNodeSpec* ui_asset_get_template(UiAsset* asset, const char* name);


// --- UI INSTANCE (The Living Tree) ---
// Created from a UiAsset + Data Context.
// Managed by a UiInstance container.

typedef struct UiElement {
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
    const MetaStruct* meta; // Type info

    // Cached Bindings (Resolved at creation)
    const MetaField* bind_text;
    const MetaField* bind_visible;
    const MetaField* bind_x;
    const MetaField* bind_y;
    const MetaField* bind_w;
    const MetaField* bind_h;
    
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

} UiElement;


typedef struct UiInstance {
    MemoryArena arena;
    MemoryPool* element_pool;
    UiElement* root;
} UiInstance;

// API for Instance
void ui_instance_init(UiInstance* instance, size_t size);
void ui_instance_destroy(UiInstance* instance);

// Allocates element from instance arena.
UiElement* ui_element_create(UiInstance* instance, const UiNodeSpec* spec, void* data, const MetaStruct* meta);

// Rebuilds children (Static + Dynamic) for an existing element.
void ui_element_rebuild_children(UiElement* element, UiInstance* instance);

// Manually add a child to an element (for dynamic editors)
void ui_element_add_child(UiElement* parent, UiElement* child);

// Clear all children (destroys them)
void ui_element_clear_children(UiElement* parent, UiInstance* instance);

// Core Loop
void ui_element_update(UiElement* element, float dt); // Syncs data

// --- Utils ---
void ui_bind_read_string(void* data, const MetaField* field, char* out_buf, size_t buf_size);

// --- High-Level Pipeline API ---

typedef float (*UiTextMeasureFunc)(const char* text, void* user_data);

// Performs layout calculation for the entire UI tree.
// Should be called before rendering.
void ui_instance_layout(UiInstance* instance, float window_w, float window_h, uint64_t frame_number, UiTextMeasureFunc measure_func, void* measure_data);

#include "engine/scene/scene.h"
#include "engine/assets/assets.h"

// Builds the render packets for the scene.
// Should be called after layout.
void ui_instance_render(UiInstance* instance, Scene* scene, const Assets* assets);

// --- Public Subsystem API ---

// Parser
UiAsset* ui_parser_load_from_file(const char* path);

// Command System
typedef void (*UiCommandCallback)(void* user_data, UiElement* target);

void ui_command_init(void);
void ui_command_shutdown(void);
void ui_command_register(const char* name, UiCommandCallback callback, void* user_data);
void ui_command_execute_id(StringId id, UiElement* target);

// Input System
typedef struct UiInputContext UiInputContext;

UiInputContext* ui_input_create(void);
void ui_input_destroy(UiInputContext* ctx);
void ui_input_update(UiInputContext* ctx, UiElement* root, const InputState* input, const InputEventQueue* events);
bool ui_input_pop_event(UiInputContext* ctx, UiEvent* out_event);

#endif // UI_CORE_H
