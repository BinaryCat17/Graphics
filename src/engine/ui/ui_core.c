#include "ui_core.h"
#include "ui_renderer.h"
#include "ui_input.h"
#include "internal/ui_internal.h"
#include "internal/ui_command_system.h"
#include "internal/ui_layout.h"
#include "foundation/logger/logger.h"
#include "foundation/memory/arena.h"
#include "foundation/memory/pool.h"
#include "foundation/meta/reflection.h"
#include "engine/scene/scene.h"
#include "engine/scene/internal/scene_tree_internal.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

static bool s_ui_initialized = false;

void ui_system_init(void) {
    if (s_ui_initialized) return;
    s_ui_initialized = true;
    LOG_INFO("UI System Initialized");
}

void ui_system_shutdown(void) {
    ui_command_shutdown();
}

// --- Helper: Binding Target Resolution ---

static UiBindingTarget ui_resolve_target_enum(const char* target) {
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

static void ui_apply_binding_value(SceneNode* el, UiBinding* b) {
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

// --- Helper: Collection Resolution ---

static int ui_resolve_count(void* data, const MetaStruct* meta, const char* field_name) {
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

SceneNode* ui_node_create(SceneTree* tree, const SceneNodeSpec* spec, void* data, const MetaStruct* meta) {
    if (!tree || !spec) return NULL;

    // 1. Basic Scene Node
    SceneNode* el = scene_node_create(tree, spec, data, meta);
    if (!el) return NULL;

    // 2. UI-specific init
    el->render_color = spec->style.color;
    el->rect.x = spec->layout.x;
    el->rect.y = spec->layout.y;
    el->on_click_cmd_id = spec->on_click;
    el->on_change_cmd_id = spec->on_change;

    // Cache Bindings
    if (meta && spec->binding_count > 0) {
        el->ui_bindings = arena_alloc_zero(&tree->arena, spec->binding_count * sizeof(UiBinding));
        el->ui_binding_count = spec->binding_count;
        UiBinding* bindings = (UiBinding*)el->ui_bindings;
        
        for (size_t i = 0; i < spec->binding_count; ++i) {
             SceneBindingSpec* b_spec = &spec->bindings[i];
             size_t total_offset = 0;
             const MetaField* f = meta_find_field_by_path(meta, b_spec->source, &total_offset);
             
             if (f) {
                 bindings[i].source_field = f;
                 bindings[i].source_offset = total_offset;
                 bindings[i].target = ui_resolve_target_enum(b_spec->target);
             }
        }
    }

    // Populate Children (UI-way handles collections)
    ui_node_rebuild_children(el, tree);
    
    return el;
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

void ui_node_update(SceneNode* element, float dt) {
    if (!element || !element->spec) return;
    
    // 1. Data Binding Sync
    UiBinding* bindings = (UiBinding*)element->ui_bindings;
    if (element->data_ptr && bindings) {
        for (size_t i = 0; i < element->ui_binding_count; ++i) {
             ui_apply_binding_value(element, &bindings[i]);
        }
    }

    // 2. Animation Interpolation
    const SceneNodeSpec* spec = element->spec;
    float target_t = element->is_hovered ? 1.0f : 0.0f;
    float speed = spec->style.animation_speed > 0 ? spec->style.animation_speed : 10.0f;
    
    if (element->hover_t != target_t) {
        float diff = target_t - element->hover_t;
        float step = speed * dt;
        if (fabsf(diff) < step) element->hover_t = target_t;
        else element->hover_t += (diff > 0 ? 1.0f : -1.0f) * step;
        
        // Update Animated Color
        if (spec->style.hover_color.w > 0) {
            element->render_color.x = spec->style.color.x + (spec->style.hover_color.x - spec->style.color.x) * element->hover_t;
            element->render_color.y = spec->style.color.y + (spec->style.hover_color.y - spec->style.color.y) * element->hover_t;
            element->render_color.z = spec->style.color.z + (spec->style.hover_color.z - spec->style.color.z) * element->hover_t;
            element->render_color.w = spec->style.color.w + (spec->style.hover_color.w - spec->style.color.w) * element->hover_t;
        }
    }

    // 3. Recurse
    for (SceneNode* child = element->first_child; child; child = child->next_sibling) {
        ui_node_update(child, dt);
    }
}

void ui_system_layout(SceneTree* tree, float window_w, float window_h, uint64_t frame_number, UiTextMeasureFunc measure_func, void* measure_data) {
    if (!tree || !tree->root) return;
    ui_layout_root(tree->root, window_w, window_h, frame_number, false, measure_func, measure_data);
}

void ui_system_render(SceneTree* tree, struct Scene* scene, const struct Assets* assets, struct MemoryArena* arena) {
    if (!tree || !tree->root) return;
    scene_tree_render(tree, scene, assets, arena);
}

Rect ui_node_get_screen_rect(const SceneNode* node) {
    return node ? node->screen_rect : (Rect){0};
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
