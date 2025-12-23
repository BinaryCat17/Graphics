#ifndef UI_BINDING_H
#define UI_BINDING_H

#include "../ui_core.h" 
#include "foundation/meta/reflection.h"

// --- Binding Struct ---

typedef struct UiBinding {
    UiBindingTarget target;
    const struct MetaField* source_field;
    size_t source_offset;
} UiBinding;

// --- Binding Functions ---

void ui_bind_read_string(void* data, const struct MetaField* field, char* out_buf, size_t buf_size);
UiBindingTarget ui_resolve_target_enum(const char* target);
void ui_apply_binding_value(SceneNode* el, UiBinding* b);
int ui_resolve_count(void* data, const struct MetaStruct* meta, const char* field_name);

#endif // UI_BINDING_H
