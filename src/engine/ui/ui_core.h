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
} UiLayoutStrategy; // REFLECT

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
} UiKind; // REFLECT

typedef enum UiLayer {
    UI_LAYER_NORMAL = 0,
    UI_LAYER_OVERLAY  // Renders last, ignores parent clipping (popups)
} UiLayer; // REFLECT

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

typedef struct UiNodeSpec UiNodeSpec;

// --- UI ASSET (The Resource) ---
// Owns the memory. Created by the Parser.

typedef struct UiTemplate UiTemplate;

typedef struct UiAsset UiAsset;

// API for Asset
UiAsset* ui_asset_create(size_t arena_size);
void ui_asset_free(UiAsset* asset);
UiNodeSpec* ui_asset_push_node(UiAsset* asset);
UiNodeSpec* ui_asset_get_template(UiAsset* asset, const char* name);
UiNodeSpec* ui_asset_get_root(const UiAsset* asset);


// --- UI INSTANCE (The Living Tree) ---
// Created from a UiAsset + Data Context.
// Managed by a UiInstance container.

typedef struct UiElement UiElement;


typedef struct UiInstance UiInstance;

// Forward Declarations for Render Pipeline
typedef struct Scene Scene;
typedef struct Assets Assets;

// API for Instance
UiInstance* ui_instance_create(size_t size);
void ui_instance_free(UiInstance* instance);
UiElement* ui_instance_get_root(const UiInstance* instance);
void ui_instance_set_root(UiInstance* instance, UiElement* root);

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

// --- Accessors ---
StringId ui_element_get_id(const UiElement* element);
UiElement* ui_element_find_by_id(UiElement* root, const char* id);
void* ui_element_get_data(const UiElement* element);
const MetaStruct* ui_element_get_meta(const UiElement* element);
UiElement* ui_element_get_parent(const UiElement* element);

// --- Utils ---
void ui_bind_read_string(void* data, const MetaField* field, char* out_buf, size_t buf_size);

// --- High-Level Pipeline API ---

typedef float (*UiTextMeasureFunc)(const char* text, void* user_data);

// Layout & Render Pipeline
// frame_number: used to optimize layout caching (recalc only once per frame per node)
void ui_instance_layout(UiInstance* instance, float window_w, float window_h, uint64_t frame_number, UiTextMeasureFunc measure_func, void* measure_data);

// Generates render commands into the Scene.
// arena: Frame allocator for temporary render structures (e.g. overlay lists)
void ui_instance_render(UiInstance* instance, Scene* scene, const Assets* assets, MemoryArena* arena);

// --- Public Subsystem API ---

// Parser
UiAsset* ui_parser_load_from_file(const char* path);

// Command System
typedef void (*UiCommandCallback)(void* user_data, UiElement* target);

void ui_command_init(void);
void ui_command_shutdown(void);
void ui_command_register(const char* name, UiCommandCallback callback, void* user_data);
void ui_command_execute_id(StringId id, UiElement* target);

// Input
typedef struct UiInputContext UiInputContext;

UiInputContext* ui_input_create(void);
void ui_input_destroy(UiInputContext* ctx);
void ui_input_init(UiInputContext* ctx);
void ui_input_update(UiInputContext* ctx, UiElement* root, const InputSystem* input);
bool ui_input_pop_event(UiInputContext* ctx, UiEvent* out_event);

#endif // UI_CORE_H
