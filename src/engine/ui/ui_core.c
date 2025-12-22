#include "ui_core.h"
#include "internal/ui_internal.h"
#include "internal/ui_layout.h"   // Internal
#include "internal/ui_renderer.h" // Internal
#include "internal/ui_parser.h"   // Internal
#include "internal/ui_command_system.h" // Internal
#include "foundation/meta/reflection.h"
#include "foundation/memory/arena.h"
#include "foundation/memory/pool.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

// --- System Lifecycle ---

void ui_system_init(void) {
    ui_command_init();
}

void ui_system_shutdown(void) {
    ui_command_shutdown();
}

// --- SceneAsset (Memory Owner) ---

SceneAsset* scene_asset_create(size_t arena_size) {
    SceneAsset* asset = (SceneAsset*)calloc(1, sizeof(SceneAsset));
    if (!asset) return NULL;
    
    if (!arena_init(&asset->arena, arena_size)) {
        free(asset);
        return NULL;
    }
    
    return asset;
}

void scene_asset_destroy(SceneAsset* asset) {
    if (!asset) return;
    arena_destroy(&asset->arena);
    free(asset);
}

SceneNodeSpec* scene_asset_push_node(SceneAsset* asset) {
    if (!asset) return NULL;
    return (SceneNodeSpec*)arena_alloc_zero(&asset->arena, sizeof(SceneNodeSpec));
}

SceneNodeSpec* scene_asset_get_template(SceneAsset* asset, const char* name) {
    if (!asset || !name) return NULL;
    SceneTemplate* t = asset->templates;
    while (t) {
        if (t->name && strcmp(t->name, name) == 0) {
            return t->spec;
        }
        t = t->next;
    }
    return NULL;
}

SceneNodeSpec* scene_asset_get_root(const SceneAsset* asset) {
    return asset ? asset->root : NULL;
}

// --- SceneTree (Memory Owner for Runtime) ---

static void destroy_recursive(SceneTree* tree, SceneNode* el) {
    if (!el) return;
    
    // Destroy children first
    SceneNode* child = el->first_child;
    while (child) {
        SceneNode* next = child->next_sibling;
        destroy_recursive(tree, child);
        child = next;
    }
    
    pool_free(tree->element_pool, el);
}

SceneTree* scene_tree_create(SceneAsset* assets, size_t size) {
    SceneTree* tree = (SceneTree*)calloc(1, sizeof(SceneTree));
    if (!tree) return NULL;
    
    if (!arena_init(&tree->arena, size)) {
        free(tree);
        return NULL;
    }
    
    tree->assets = assets;
    tree->element_pool = pool_create(sizeof(SceneNode), 256);
    tree->root = NULL;
    return tree;
}

void scene_tree_destroy(SceneTree* tree) {
    if (!tree) return;
    if (tree->root) destroy_recursive(tree, tree->root);
    pool_destroy(tree->element_pool);
    arena_destroy(&tree->arena);
    free(tree);
}

SceneNode* scene_tree_get_root(const SceneTree* tree) {
    return tree ? tree->root : NULL;
}

void scene_tree_set_root(SceneTree* tree, SceneNode* root) {
    if (tree) tree->root = root;
}

// ... Accessors ...

// --- Accessors ---

StringId scene_node_get_id(const SceneNode* element) {
    if (element && element->spec) return element->spec->id;
    return 0;
}

SceneNode* scene_node_find_by_id(SceneNode* root, const char* id) {
    if (!root || !root->spec || !id) return NULL;
    StringId target = str_id(id);
    if (root->spec->id == target) return root;
    
    for (SceneNode* child = root->first_child; child; child = child->next_sibling) {
        SceneNode* found = scene_node_find_by_id(child, id);
        if (found) return found;
    }
    return NULL;
}

void* scene_node_get_data(const SceneNode* element) {
    return element ? element->data_ptr : NULL;
}

const MetaStruct* scene_node_get_meta(const SceneNode* element) {
    return element ? element->meta : NULL;
}

SceneNode* scene_node_get_parent(const SceneNode* element) {
    return element ? element->parent : NULL;
}

Rect scene_node_get_screen_rect(const SceneNode* element) {
    if (element) return element->screen_rect;
    return (Rect){0};
}

// --- SceneNode (Instance) ---

static SceneNode* element_alloc(SceneTree* tree, const SceneNodeSpec* spec) {
    // Pool allocation guarantees zero-init
    SceneNode* el = (SceneNode*)pool_alloc(tree->element_pool);
    el->spec = spec;
    return el;
}

// Helper: Append child to linked list
void scene_node_add_child(SceneNode* parent, SceneNode* child) {
    if (!parent || !child) return;
    
    child->parent = parent;
    child->next_sibling = NULL;
    child->prev_sibling = parent->last_child;
    
    if (parent->last_child) {
        parent->last_child->next_sibling = child;
    } else {
        parent->first_child = child;
    }
    parent->last_child = child;
    parent->child_count++;
}

void scene_node_clear_children(SceneNode* parent, SceneTree* tree) {
    if (!parent || !tree) return;
    
    SceneNode* curr = parent->first_child;
    while (curr) {
        SceneNode* next = curr->next_sibling;
        destroy_recursive(tree, curr);
        curr = next;
    }
    parent->first_child = NULL;
    parent->last_child = NULL;
    parent->child_count = 0;
}

#include "foundation/logger/logger.h"

// --- Helper: Binding Target Resolution ---

static SceneBindingTarget ui_resolve_target_enum(const char* target) {
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
    
    // Legacy support (mapped by parser but safe to keep)
    if (strcmp(target, "x") == 0) return BINDING_TARGET_LAYOUT_X;
    if (strcmp(target, "y") == 0) return BINDING_TARGET_LAYOUT_Y;
    if (strcmp(target, "w") == 0) return BINDING_TARGET_LAYOUT_WIDTH;
    if (strcmp(target, "h") == 0) return BINDING_TARGET_LAYOUT_HEIGHT;

    return BINDING_TARGET_NONE;
}

static void ui_apply_binding_value(SceneNode* el, SceneBinding* b) {
    void* ptr = (char*)el->data_ptr + b->source_offset;
    const MetaField* f = b->source_field;
    
    switch (b->target) {
        case BINDING_TARGET_TEXT: {
            // Re-use existing helper but need to adapt args
            // ui_bind_read_string expects (instance, field). 
            // We have direct ptr. Construct a dummy call or refactor helper.
            // Let's do direct read here for speed.
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
            
            // Update cache
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
            
            if (vis) el->flags &= ~UI_FLAG_HIDDEN;
            else el->flags |= UI_FLAG_HIDDEN;
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
    
    if (strstr(field_name, "_ptrs")) {
        strncpy(count_name, field_name, sizeof(count_name));
        count_name[sizeof(count_name) - 1] = '\0';
        char* p = strstr(count_name, "_ptrs");
        if (p) {
            strncpy(p, "_count", sizeof(count_name) - (p - count_name));
            f = meta_find_field(meta, count_name);
            if (f && f->type == META_TYPE_INT) return meta_get_int(data, f);
        }
    }
    
    f = meta_find_field(meta, "count");
    if (f && f->type == META_TYPE_INT) return meta_get_int(data, f);
    
    LOG_WARN("UiCore: Failed to resolve count for collection '%s' in struct '%s'. Expected '%s_count' or 'count'.", 
             field_name, meta->name, field_name);
    return 0;
}

void scene_node_rebuild_children(SceneNode* el, SceneTree* tree) {
    if (!el || !tree || !el->spec) return;
    
    // 1. Clear existing
    scene_node_clear_children(el, tree);

    // 2. Resolve Dynamic Count
    size_t static_count = el->spec->child_count;
    size_t dynamic_count = 0;
    const MetaField* collection_field = NULL;
    
    if (el->spec->collection && el->meta && el->data_ptr) {
        collection_field = meta_find_field(el->meta, el->spec->collection);
        if (collection_field) {
            int cnt = ui_resolve_count(el->data_ptr, el->meta, el->spec->collection);
            if (cnt > 0) dynamic_count = (size_t)cnt;
            // LOG_TRACE("UI Collection '%s': Count=%zu", el->spec->collection, dynamic_count);
        } else {
            LOG_ERROR("UiCore: Collection field '%s' not found in struct '%s' (Node ID:%u)", 
                el->spec->collection, el->meta->name, el->spec->id);
        }
    }
    
    // 3. Create Static Children
    for (size_t i = 0; i < static_count; ++i) {
        SceneNode* child = scene_node_create(tree, el->spec->children[i], el->data_ptr, el->meta);
        if (child) {
            scene_node_add_child(el, child);
        }
    }
    
    // 4. Create Dynamic Children
    if (dynamic_count > 0 && collection_field && el->spec->item_template) {
         const MetaStruct* item_meta = NULL;
         bool is_pointer_array = (collection_field->type == META_TYPE_POINTER_ARRAY);
         bool is_flat_array = (collection_field->type == META_TYPE_POINTER); 

         if (is_pointer_array || is_flat_array) {
             item_meta = meta_get_struct(collection_field->type_name);
         }
         
         if (item_meta) {
             void* base_ptr = *(void**)((char*)el->data_ptr + collection_field->offset);
             
             for (size_t i = 0; i < dynamic_count; ++i) {
                 void* item_ptr = NULL;
                 if (is_pointer_array) {
                     // T** -> dereference to get T*
                     item_ptr = ((void**)base_ptr)[i];
                 } else {
                     // T* -> pointer arithmetic (T[])
                     item_ptr = (char*)base_ptr + (i * item_meta->size);
                 }

                 if (item_ptr) {
                     const SceneNodeSpec* child_spec = el->spec->item_template;

                     // Conditional Template Selector
                     if (el->spec->template_selector && tree->assets) {
                         const MetaField* sel_field = meta_find_field(item_meta, el->spec->template_selector);
                         if (sel_field && sel_field->type == META_TYPE_ENUM) {
                             int val = meta_get_int(item_ptr, sel_field);
                             const MetaEnum* e = meta_get_enum(sel_field->type_name);
                             const char* t_name = meta_enum_get_name(e, val);
                             if (t_name) {
                                 SceneNodeSpec* t = scene_asset_get_template(tree->assets, t_name);
                                 if (t) child_spec = t;
                             }
                         }
                     }

                     SceneNode* child = scene_node_create(tree, child_spec, item_ptr, item_meta);
                     if (child) {
                        scene_node_add_child(el, child);
                     }
                 }
             }
         }
    }
}

const SceneBinding* scene_node_get_binding(const SceneNode* node, SceneBindingTarget target) {
    if (!node || !node->bindings) return NULL;
    for (size_t i = 0; i < node->binding_count; ++i) {
        if (node->bindings[i].target == target) return &node->bindings[i];
    }
    return NULL;
}

void scene_node_write_binding_float(SceneNode* node, SceneBindingTarget target, float value) {
    const SceneBinding* b = scene_node_get_binding(node, target);
    if (b && b->source_field && node->data_ptr) {
        void* ptr = (char*)node->data_ptr + b->source_offset;
        if (b->source_field->type == META_TYPE_FLOAT) {
            *(float*)ptr = value;
        } else if (b->source_field->type == META_TYPE_INT) {
            *(int*)ptr = (int)value;
        }
    }
}

void scene_node_write_binding_string(SceneNode* node, SceneBindingTarget target, const char* value) {
    const SceneBinding* b = scene_node_get_binding(node, target);
    if (b && b->source_field && node->data_ptr) {
        // Calculate the base instance pointer for this field
        // source_offset = accumulated_offset_to_field
        // field->offset = offset_within_parent_struct
        // parent_struct = source_offset - field->offset
        void* parent = (char*)node->data_ptr + (b->source_offset - b->source_field->offset);
        meta_set_string(parent, b->source_field, value);
    }
}

SceneNode* scene_node_create(SceneTree* tree, const SceneNodeSpec* spec, void* data, const MetaStruct* meta) {
    if (!tree || !spec) return NULL;

    SceneNode* el = element_alloc(tree, spec);
    el->data_ptr = data;
    el->meta = meta;
    el->render_color = spec->style.color;
    el->flags = spec->flags; // Init Flags
    el->rect.x = spec->layout.x;
    el->rect.y = spec->layout.y;

    // Resolve Commands
    if (spec->on_click) {
        el->on_click_cmd_id = spec->on_click;
    }
    if (spec->on_change) {
        el->on_change_cmd_id = spec->on_change;
    }

    // Cache Bindings (V2)
    if (meta && spec->binding_count > 0) {
        el->bindings = (SceneBinding*)arena_alloc_zero(&tree->arena, spec->binding_count * sizeof(SceneBinding));
        el->binding_count = spec->binding_count;
        
        for (size_t i = 0; i < spec->binding_count; ++i) {
             SceneBindingSpec* b_spec = &spec->bindings[i];
             size_t total_offset = 0;
             const MetaField* f = meta_find_field_by_path(meta, b_spec->source, &total_offset);
             
             if (f) {
                 el->bindings[i].source_field = f;
                 el->bindings[i].source_offset = total_offset;
                 el->bindings[i].target = ui_resolve_target_enum(b_spec->target);
                 
                 // Warn if target invalid
                 if (el->bindings[i].target == BINDING_TARGET_NONE) {
                      LOG_WARN("UiCore: Invalid binding target '%s' on Node ID:%u", b_spec->target, spec->id);
                 }
             } else {
                 LOG_ERROR("UiCore: Failed to resolve binding source '%s' (Node ID:%u)", b_spec->source, spec->id);
             }
        }
    }

    // Populate Children
    scene_node_rebuild_children(el, tree);
    
    return el;
}

void scene_node_update(SceneNode* element, float dt) {
    if (!element || !element->spec) return;
    
    // --- 3.2: Unified Scene Graph Matrix Update ---
    // 1. Build Local Matrix: T(Layout) * T(Local) * R(Local) * S(Local)
    // Note: Layout translation (rect.x, rect.y) is added to Local Translation
    
    const SceneTransformSpec* trans = &element->spec->transform;
    
    // Scale
    Vec3 scale = { 
        trans->local_scale.x == 0 ? 1.0f : trans->local_scale.x, 
        trans->local_scale.y == 0 ? 1.0f : trans->local_scale.y, 
        trans->local_scale.z == 0 ? 1.0f : trans->local_scale.z 
    };
    Mat4 mat_s = mat4_scale(scale);
    
    // Rotation
    EulerAngles euler = { trans->local_rotation.x, trans->local_rotation.y, trans->local_rotation.z };
    Mat4 mat_r = mat4_rotation_euler(euler);
    
    // Translation (Layout + Local)
    // Layout provides X/Y (2D), Local provides X/Y/Z (3D) offset
    Vec3 translation = {
        element->rect.x + trans->local_position.x,
        element->rect.y + trans->local_position.y,
        trans->local_position.z
    };
    Mat4 mat_t = mat4_translation(translation);
    
    // Local = T * R * S
    Mat4 mat_rs = mat4_multiply(&mat_r, &mat_s);
    element->local_matrix = mat4_multiply(&mat_t, &mat_rs);
    
    // 2. Build World Matrix
    if (element->parent) {
        element->world_matrix = mat4_multiply(&element->parent->world_matrix, &element->local_matrix);
    } else {
        element->world_matrix = element->local_matrix;
    }

    // 0. Animation Interpolation
    const SceneNodeSpec* spec = element->spec;
    float target_t = element->is_hovered ? 1.0f : 0.0f;
    float speed = spec->style.animation_speed > 0 ? spec->style.animation_speed : 10.0f; // Default speed
    
    if (element->hover_t != target_t) {
        float diff = target_t - element->hover_t;
        float step = speed * dt;
        if (fabsf(diff) < step) element->hover_t = target_t;
        else element->hover_t += (diff > 0 ? 1.0f : -1.0f) * step;
        
        // Update Animated Color
        if (spec->style.hover_color.w > 0 || spec->style.hover_color.x > 0 || spec->style.hover_color.y > 0 || spec->style.hover_color.z > 0) {
            element->render_color.x = spec->style.color.x + (spec->style.hover_color.x - spec->style.color.x) * element->hover_t;
            element->render_color.y = spec->style.color.y + (spec->style.hover_color.y - spec->style.color.y) * element->hover_t;
            element->render_color.z = spec->style.color.z + (spec->style.hover_color.z - spec->style.color.z) * element->hover_t;
            element->render_color.w = spec->style.color.w + (spec->style.hover_color.w - spec->style.color.w) * element->hover_t;
        }
    }

    // 1. Apply Bindings (V2)
    if (element->data_ptr && element->bindings) {
        for (size_t i = 0; i < element->binding_count; ++i) {
             ui_apply_binding_value(element, &element->bindings[i]);
        }
    } else if (element->spec->text) {
        // Fallback: Static Text
        if (strncmp(element->cached_text, element->spec->text, 128) != 0) {
            strncpy(element->cached_text, element->spec->text, 128 - 1);
            element->cached_text[127] = '\0';
        }
    }

    // Recurse Linked List
    for (SceneNode* child = element->first_child; child; child = child->next_sibling) {
        scene_node_update(child, dt);
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

// --- High-Level Pipeline API ---

void scene_tree_layout(SceneTree* instance, float window_w, float window_h, uint64_t frame_number, UiTextMeasureFunc measure_func, void* measure_data) {
    if (!instance || !instance->root) return;
    // Log debug info only for specific frames or conditions if needed. Currently false.
    ui_layout_root(instance->root, window_w, window_h, frame_number, false, measure_func, measure_data);
}

void scene_tree_render(SceneTree* instance, Scene* scene, const Assets* assets, MemoryArena* arena) {
    if (!instance || !instance->root || !scene || !assets || !arena) return;
    scene_builder_build(instance->root, scene, assets, arena);
}

// --- Public Subsystem API ---

SceneAsset* scene_asset_load_from_file(const char* path) {
    return scene_asset_load_internal(path);
}
