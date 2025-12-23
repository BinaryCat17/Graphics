#ifndef UI_INTERNAL_H
#define UI_INTERNAL_H

#include "../ui_core.h"
#include "../ui_input.h"
#include "../ui_renderer.h"
#include "engine/scene/scene.h"
#include "engine/scene/internal/scene_tree_internal.h"
#include "foundation/memory/arena.h"
#include "foundation/memory/pool.h"
#include "foundation/string/string_id.h"
#include "foundation/math/coordinate_systems.h"

// Note: UiLayoutSpec and UiStyleSpec are now in scene_tree_internal.h
// Note: UiNodeSpec and UiBindingSpec have been merged into SceneNodeSpec (scene_tree_internal.h)

// --- RUNTIME STATE ---

typedef struct UiBinding {
    UiBindingTarget target;
    const struct MetaField* source_field;
    size_t source_offset;
} UiBinding;

// Helper function needed by ui_input
void ui_bind_read_string(void* data, const struct MetaField* field, char* out_buf, size_t buf_size);

// --- Internal Binding API ---
UiBindingTarget ui_resolve_target_enum(const char* target);
void ui_apply_binding_value(SceneNode* el, UiBinding* b);
int ui_resolve_count(void* data, const struct MetaStruct* meta, const char* field_name);

#endif // UI_INTERNAL_H
