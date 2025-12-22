#ifndef UI_INTERNAL_H
#define UI_INTERNAL_H

#include "../ui_core.h"
#include "../ui_assets.h"
#include "../ui_input.h"
#include "../ui_renderer.h"
#include "engine/scene/scene.h"
#include "engine/scene/internal/scene_tree_internal.h"
#include "foundation/memory/arena.h"
#include "foundation/memory/pool.h"
#include "foundation/string/string_id.h"
#include "foundation/math/coordinate_systems.h"

// Note: UiLayoutSpec and UiStyleSpec are now in scene_tree_internal.h

typedef struct UiBindingSpec {
    char* target; // REFLECT
    char* source; // REFLECT
} UiBindingSpec;

// This will be stored in SceneNodeSpec->system_spec
typedef struct UiNodeSpec {
    UiKind kind;            // REFLECT
    uint32_t ui_flags;      // REFLECT(UiFlags)
    
    UiLayoutSpec layout;    // REFLECT
    UiStyleSpec style;      // REFLECT
    
    UiBindingSpec* bindings;// REFLECT
    size_t binding_count;   // REFLECT
    
    char* collection;       // REFLECT
    char* template_selector;// REFLECT

    char* text;             // REFLECT
    char* text_source;      // REFLECT

    SceneNodeSpec* item_template; // REFLECT
    
    StringId on_click;      // REFLECT
    StringId on_change;     // REFLECT
    
    StringId provider_id;   // REFLECT
} UiNodeSpec;

// --- RUNTIME STATE ---

typedef struct UiBinding {
    UiBindingTarget target;
    const struct MetaField* source_field;
    size_t source_offset;
} UiBinding;

// Helper function needed by ui_input
void ui_bind_read_string(void* data, const struct MetaField* field, char* out_buf, size_t buf_size);

#endif // UI_INTERNAL_H
