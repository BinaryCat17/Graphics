#ifndef UI_CORE_H
#define UI_CORE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "foundation/string/string_id.h"

// Forward Declarations
typedef struct MetaStruct MetaStruct;
typedef struct MetaField MetaField;
typedef struct UiNodeSpec UiNodeSpec; // From ui_assets.h

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

// --- UI INSTANCE (The Living Tree) ---
// Created from a UiAsset + Data Context.
// Managed by a UiInstance container.

typedef struct UiInstance UiInstance;

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

#endif // UI_CORE_H
