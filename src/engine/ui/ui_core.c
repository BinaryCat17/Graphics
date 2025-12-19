#include "ui_core.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

// --- UiAsset (Memory Owner) ---

UiAsset* ui_asset_create(size_t arena_size) {
    UiAsset* asset = (UiAsset*)calloc(1, sizeof(UiAsset));
    if (!asset) return NULL;
    
    if (!arena_init(&asset->arena, arena_size)) {
        free(asset);
        return NULL;
    }
    
    return asset;
}

void ui_asset_free(UiAsset* asset) {
    if (!asset) return;
    arena_destroy(&asset->arena);
    free(asset);
}

UiNodeSpec* ui_asset_push_node(UiAsset* asset) {
    if (!asset) return NULL;
    return (UiNodeSpec*)arena_alloc_zero(&asset->arena, sizeof(UiNodeSpec));
}

UiNodeSpec* ui_asset_get_template(UiAsset* asset, const char* name) {
    if (!asset || !name) return NULL;
    UiTemplate* t = asset->templates;
    while (t) {
        if (t->name && strcmp(t->name, name) == 0) {
            return t->spec;
        }
        t = t->next;
    }
    return NULL;
}

// --- UiInstance (Memory Owner for Runtime) ---

void ui_instance_init(UiInstance* instance, size_t size) {
    if (!instance) return;
    arena_init(&instance->arena, size);
    instance->root = NULL;
}

void ui_instance_destroy(UiInstance* instance) {
    if (!instance) return;
    arena_destroy(&instance->arena);
    instance->root = NULL;
}

// --- UiElement (Instance) ---

static UiElement* element_alloc(UiInstance* instance, const UiNodeSpec* spec) {
    // Arena allocation guarantees zero-init
    UiElement* el = (UiElement*)arena_alloc_zero(&instance->arena, sizeof(UiElement));
    el->spec = spec;
    return el;
}

#include "foundation/logger/logger.h"

// --- Helper: Collection Resolution ---

static int ui_resolve_count(void* data, const MetaStruct* meta, const char* field_name) {
    if (!data || !meta || !field_name) return 0;
    
    char count_name[128];
    snprintf(count_name, sizeof(count_name), "%s_count", field_name);
    const MetaField* f = meta_find_field(meta, count_name);
    if (f && f->type == META_TYPE_INT) return meta_get_int(data, f);
    
    if (strstr(field_name, "_ptrs")) {
        strncpy(count_name, field_name, sizeof(count_name));
        char* p = strstr(count_name, "_ptrs");
        if (p) {
            strcpy(p, "_count");
            f = meta_find_field(meta, count_name);
            if (f && f->type == META_TYPE_INT) return meta_get_int(data, f);
        }
    }
    
    f = meta_find_field(meta, "count");
    if (f && f->type == META_TYPE_INT) return meta_get_int(data, f);
    
    return 0;
}

void ui_element_rebuild_children(UiElement* el, UiInstance* instance) {
    if (!el || !instance || !el->spec) return;
    
    // 1. Re-evaluate counts
    size_t static_count = el->spec->child_count;
    size_t dynamic_count = 0;
    const MetaField* collection_field = NULL;
    
    if (el->spec->bind_collection && el->meta && el->data_ptr) {
        collection_field = meta_find_field(el->meta, el->spec->bind_collection);
        if (collection_field) {
            int cnt = ui_resolve_count(el->data_ptr, el->meta, el->spec->bind_collection);
            if (cnt > 0) dynamic_count = (size_t)cnt;
            LOG_TRACE("UI Collection '%s': Count=%zu", el->spec->bind_collection, dynamic_count);
        } else {
            LOG_ERROR("UiCore: Collection field '%s' not found in struct '%s' (Node: %s)", 
                el->spec->bind_collection, el->meta->name, el->spec->id ? el->spec->id : "anon");
        }
    }
    
    size_t total_count = static_count + dynamic_count;
    // ...
    
    // 2. Re-allocate children array (Leak in Arena, accepted for now)
    el->children = (UiElement**)arena_alloc_zero(&instance->arena, total_count * sizeof(UiElement*));
    el->child_count = total_count;
    
    // 3. Re-create Static Children
    for (size_t i = 0; i < static_count; ++i) {
        el->children[i] = ui_element_create(instance, el->spec->children[i], el->data_ptr, el->meta);
        if (el->children[i]) el->children[i]->parent = el;
    }
    
    // 4. Create Dynamic Children
    if (dynamic_count > 0 && collection_field && el->spec->item_template) {
         const MetaStruct* item_meta = NULL;
         // Resolve Item Type
         if (collection_field->type == META_TYPE_POINTER_ARRAY) {
             item_meta = meta_get_struct(collection_field->type_name);
         }
         
         if (item_meta) {
             void** ptr_array = *(void***)((char*)el->data_ptr + collection_field->offset);
             
             size_t write_idx = static_count;
             for (size_t i = 0; i < dynamic_count; ++i) {
                 void* item_ptr = ptr_array[i];
                 if (item_ptr) {
                     UiElement* child = ui_element_create(instance, el->spec->item_template, item_ptr, item_meta);
                     if (child) {
                         child->parent = el;
                         el->children[write_idx++] = child;
                     }
                 }
             }
             el->child_count = write_idx; 
         }
    }
}

UiElement* ui_element_create(UiInstance* instance, const UiNodeSpec* spec, void* data, const MetaStruct* meta) {
    if (!instance || !spec) return NULL;

    UiElement* el = element_alloc(instance, spec);
    el->data_ptr = data;
    el->meta = meta;
    el->render_color = spec->color;
    el->flags = spec->flags; // Init Flags

    // Resolve Commands
    if (spec->on_click_cmd) {
        el->on_click_cmd_id = str_id(spec->on_click_cmd);
    }
    if (spec->on_change_cmd) {
        el->on_change_cmd_id = str_id(spec->on_change_cmd);
    }

    // Cache Bindings
    if (meta) {
        if (spec->text_source) {
            el->bind_text = meta_find_field(meta, spec->text_source);
            if (!el->bind_text) LOG_ERROR("UiCore: Failed to bind 'text: %s' on Node '%s'. Field not found in struct '%s'", spec->text_source, spec->id ? spec->id : "anon", meta->name);
        }
        if (spec->visible_source) {
            el->bind_visible = meta_find_field(meta, spec->visible_source);
            if (!el->bind_visible) LOG_ERROR("UiCore: Failed to bind 'visible: %s' on Node '%s'. Field not found in struct '%s'", spec->visible_source, spec->id ? spec->id : "anon", meta->name);
        }
        if (spec->x_source) {
            el->bind_x = meta_find_field(meta, spec->x_source);
            if (!el->bind_x) LOG_ERROR("UiCore: Failed to bind 'x: %s' on Node '%s'. Field not found in struct '%s'", spec->x_source, spec->id ? spec->id : "anon", meta->name);
        }
        if (spec->y_source) {
            el->bind_y = meta_find_field(meta, spec->y_source);
            if (!el->bind_y) LOG_ERROR("UiCore: Failed to bind 'y: %s' on Node '%s'. Field not found in struct '%s'", spec->y_source, spec->id ? spec->id : "anon", meta->name);
        }
        if (spec->w_source) {
            el->bind_w = meta_find_field(meta, spec->w_source);
            if (!el->bind_w) LOG_ERROR("UiCore: Failed to bind 'w: %s' on Node '%s'. Field not found in struct '%s'", spec->w_source, spec->id ? spec->id : "anon", meta->name);
        }
        if (spec->h_source) {
            el->bind_h = meta_find_field(meta, spec->h_source);
            if (!el->bind_h) LOG_ERROR("UiCore: Failed to bind 'h: %s' on Node '%s'. Field not found in struct '%s'", spec->h_source, spec->id ? spec->id : "anon", meta->name);
        }
    }

    // Populate Children
    ui_element_rebuild_children(el, instance);
    
    return el;
}

void ui_element_update(UiElement* element, float dt) {
    if (!element || !element->spec) return;
    
    // 0. Animation Interpolation
    const UiNodeSpec* spec = element->spec;
    float target_t = element->is_hovered ? 1.0f : 0.0f;
    float speed = spec->animation_speed > 0 ? spec->animation_speed : 10.0f; // Default speed
    
    if (element->hover_t != target_t) {
        float diff = target_t - element->hover_t;
        float step = speed * dt;
        if (fabsf(diff) < step) element->hover_t = target_t;
        else element->hover_t += (diff > 0 ? 1.0f : -1.0f) * step;
        
        // Update Animated Color
        if (spec->hover_color.w > 0 || spec->hover_color.x > 0 || spec->hover_color.y > 0 || spec->hover_color.z > 0) {
            element->render_color.x = spec->color.x + (spec->hover_color.x - spec->color.x) * element->hover_t;
            element->render_color.y = spec->color.y + (spec->hover_color.y - spec->color.y) * element->hover_t;
            element->render_color.z = spec->color.z + (spec->hover_color.z - spec->color.z) * element->hover_t;
            element->render_color.w = spec->color.w + (spec->hover_color.w - spec->color.w) * element->hover_t;
        }
    }

    // 1. Resolve Text Binding (Cached)
    if (element->bind_text && element->data_ptr) {
        char new_text[128] = {0};
        ui_bind_read_string(element->data_ptr, element->bind_text, new_text, sizeof(new_text));
        
        if (strncmp(element->cached_text, new_text, 128) != 0) {
            strncpy(element->cached_text, new_text, 128 - 1); // safe copy
            element->cached_text[127] = '\0';
        }
    } else if (element->spec->static_text) {
        // Init cache from static text if empty (first run or if static text is used without binding)
        // Optimization: Check if match first
        if (strncmp(element->cached_text, element->spec->static_text, 128) != 0) {
            strncpy(element->cached_text, element->spec->static_text, 128 - 1);
            element->cached_text[127] = '\0';
        }
    }
    
    // 2. Resolve Geometry Bindings (X/Y)
    if (element->data_ptr) {
        if (element->bind_x) {
            float val = meta_get_float(element->data_ptr, element->bind_x);
            if (element->rect.x != val) {
                element->rect.x = val;
            }
        }
        if (element->bind_y) {
            float val = meta_get_float(element->data_ptr, element->bind_y);
            if (element->rect.y != val) {
                element->rect.y = val;
            }
        }
        if (element->bind_w) {
            float val = meta_get_float(element->data_ptr, element->bind_w);
            if (element->rect.w != val) {
                element->rect.w = val;
            }
        }
        if (element->bind_h) {
            float val = meta_get_float(element->data_ptr, element->bind_h);
            if (element->rect.h != val) {
                element->rect.h = val;
            }
        }
        // 3. Resolve Visibility
        if (element->bind_visible) {
             bool visible = meta_get_bool(element->data_ptr, element->bind_visible);
             if (visible) element->flags &= ~UI_FLAG_HIDDEN;
             else element->flags |= UI_FLAG_HIDDEN;
        }
    }

    // Recurse
    for (size_t i = 0; i < element->child_count; ++i) {
        ui_element_update(element->children[i], dt);
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