#include "../ui_core.h"
#include "ui_internal.h"
#include "foundation/meta/reflection.h"
#include "engine/scene/scene.h"
#include <string.h>
#include <stdio.h>

// --- Binding Target Resolution ---

UiBindingTarget ui_resolve_target_enum(const char* target) {
    if (!target) return BINDING_TARGET_NONE;
    if (strcmp(target, "text") == 0) return BINDING_TARGET_TEXT;
    if (strcmp(target, "visible") == 0) return BINDING_TARGET_VISIBLE;
    
    // Layout
    if (strcmp(target, "layout.x") == 0) return BINDING_TARGET_LAYOUT_X;
    if (strcmp(target, "layout.y") == 0) return BINDING_TARGET_LAYOUT_Y;
    if (strcmp(target, "layout.width") == 0) return BINDING_TARGET_LAYOUT_WIDTH;
    if (strcmp(target, "layout.height") == 0) return BINDING_TARGET_LAYOUT_HEIGHT;
    
    // Style
    if (strcmp(target, "style.color") == 0) return BINDING_TARGET_STYLE_COLOR;
    
    // Transform
    if (strcmp(target, "transform.position.x") == 0) return BINDING_TARGET_TRANSFORM_POS_X;
    if (strcmp(target, "transform.position.y") == 0) return BINDING_TARGET_TRANSFORM_POS_Y;
    if (strcmp(target, "transform.position.z") == 0) return BINDING_TARGET_TRANSFORM_POS_Z;
    
    // Legacy support
    if (strcmp(target, "x") == 0) return BINDING_TARGET_LAYOUT_X;
    if (strcmp(target, "y") == 0) return BINDING_TARGET_LAYOUT_Y;
    if (strcmp(target, "w") == 0) return BINDING_TARGET_LAYOUT_WIDTH;
    if (strcmp(target, "h") == 0) return BINDING_TARGET_LAYOUT_HEIGHT;

    return BINDING_TARGET_NONE;
}

void ui_apply_binding_value(SceneNode* el, UiBinding* b) {
    void* ptr = (char*)scene_node_get_data(el) + b->source_offset;
    const MetaField* f = b->source_field;
    if (!ptr || !f) return;
    
    switch (b->target) {
        case BINDING_TARGET_TEXT: {
            char buf[128];
            buf[0] = '\0';
            
            if (f->type == META_TYPE_STRING) {
                char* s = *(char**)ptr;
                if (s) strncpy(buf, s, 127);
            } else if (f->type == META_TYPE_STRING_ARRAY) {
                strncpy(buf, (char*)ptr, 127);
            } else if (f->type == META_TYPE_FLOAT) {
                snprintf(buf, 128, "%.2f", *(float*)ptr);
            } else if (f->type == META_TYPE_INT) {
                snprintf(buf, 128, "%d", *(int*)ptr);
            } else if (f->type == META_TYPE_BOOL) {
                snprintf(buf, 128, "%s", (*(bool*)ptr) ? "true" : "false");
            }
            
            if (strncmp(el->cached_text, buf, 128) != 0) {
                strncpy(el->cached_text, buf, 127);
                el->cached_text[127] = '\0';
            }
            break;
        }
        case BINDING_TARGET_VISIBLE: {
            bool vis = false;
            if (f->type == META_TYPE_BOOL) vis = *(bool*)ptr;
            else if (f->type == META_TYPE_INT) vis = (*(int*)ptr) != 0;
            
            if (vis) el->flags &= ~SCENE_FLAG_HIDDEN;
            else el->flags |= SCENE_FLAG_HIDDEN;
            break;
        }
        case BINDING_TARGET_LAYOUT_X:
            if (f->type == META_TYPE_FLOAT) el->rect.x = *(float*)ptr;
            break;
        case BINDING_TARGET_LAYOUT_Y:
            if (f->type == META_TYPE_FLOAT) el->rect.y = *(float*)ptr;
            break;
        case BINDING_TARGET_LAYOUT_WIDTH:
            if (f->type == META_TYPE_FLOAT) el->rect.w = *(float*)ptr;
            break;
        case BINDING_TARGET_LAYOUT_HEIGHT:
            if (f->type == META_TYPE_FLOAT) el->rect.h = *(float*)ptr;
            break;
        case BINDING_TARGET_STYLE_COLOR:
            if (f->type == META_TYPE_VEC4) el->render_color = *(Vec4*)ptr;
            break;
        default: break;
    }
}

// --- Collection Resolution ---

int ui_resolve_count(void* data, const MetaStruct* meta, const char* field_name) {
    if (!data || !meta || !field_name) return 0;
    
    char count_name[128];
    snprintf(count_name, sizeof(count_name), "%s_count", field_name);
    const MetaField* f = meta_find_field(meta, count_name);
    if (f && f->type == META_TYPE_INT) return meta_get_int(data, f);
    
    f = meta_find_field(meta, "count");
    if (f && f->type == META_TYPE_INT) return meta_get_int(data, f);
    
    return 0;
}

void ui_node_rebuild_children(SceneNode* el, SceneTree* tree) {
    if (!el || !tree || !el->spec) return;
    
    scene_node_clear_children(el, tree);

    const SceneNodeSpec* spec = el->spec;
    if (!spec->collection || !el->meta || !el->data_ptr) return;

    const MetaField* collection_field = meta_find_field(el->meta, spec->collection);
    if (!collection_field) return;

    int dynamic_count = ui_resolve_count(el->data_ptr, el->meta, spec->collection);
    if (dynamic_count <= 0) return;

    const MetaStruct* item_meta = meta_get_struct(collection_field->type_name);
    if (!item_meta) return;

    void* base_ptr = *(void**)((char*)el->data_ptr + collection_field->offset);
    if (!base_ptr) return;

    bool is_pointer_array = (collection_field->type == META_TYPE_POINTER_ARRAY);

    for (size_t i = 0; i < (size_t)dynamic_count; ++i) {
        void* item_ptr = is_pointer_array ? ((void**)base_ptr)[i] : (char*)base_ptr + (i * item_meta->size);
        if (!item_ptr) continue;

        const SceneNodeSpec* child_spec = spec->item_template;
        // Template selector logic (Omitted for brevity in this step, but should be added back)

        SceneNode* child = ui_node_create(tree, child_spec, item_ptr, item_meta);
        if (child) {
            scene_node_add_child(el, child);
        }
    }
}

void ui_bind_read_string(void* data, const MetaField* field, char* out_buf, size_t buf_size) {
    if (!data || !field || !out_buf || buf_size == 0) return;
    
    out_buf[0] = '\0';

    if (field->type == META_TYPE_STRING || field->type == META_TYPE_STRING_ARRAY) {
        const char* current = meta_get_string(data, field);
        if (current) strncpy(out_buf, current, buf_size - 1);
        out_buf[buf_size - 1] = '\0';
    } else if (field->type == META_TYPE_FLOAT) {
        float val = meta_get_float(data, field);
        snprintf(out_buf, buf_size, "%.2f", val);
    } else if (field->type == META_TYPE_INT) {
        int val = meta_get_int(data, field);
        snprintf(out_buf, buf_size, "%d", val);
    }
}

const UiBinding* ui_node_get_binding(const SceneNode* node, UiBindingTarget target) {
    if (!node || !node->ui_bindings) return NULL;
    const UiBinding* bindings = (const UiBinding*)node->ui_bindings;
    for (size_t i = 0; i < node->ui_binding_count; ++i) {
        if (bindings[i].target == target) return &bindings[i];
    }
    return NULL;
}

void ui_node_write_binding_float(SceneNode* node, UiBindingTarget target, float value) {
    const UiBinding* b = ui_node_get_binding(node, target);
    if (b && b->source_field && node->data_ptr) {
        void* ptr = (char*)node->data_ptr + b->source_offset;
        if (b->source_field->type == META_TYPE_FLOAT) {
            *(float*)ptr = value;
        } else if (b->source_field->type == META_TYPE_INT) {
            *(int*)ptr = (int)value;
        }
    }
}

void ui_node_write_binding_string(SceneNode* node, UiBindingTarget target, const char* value) {
    const UiBinding* b = ui_node_get_binding(node, target);
    if (b && b->source_field && node->data_ptr) {
        void* parent = (char*)node->data_ptr + (b->source_offset - b->source_field->offset);
        meta_set_string(parent, b->source_field, value);
    }
}
