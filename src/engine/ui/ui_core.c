#include "ui_core.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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

void ui_instance_reset(UiInstance* instance) {
    if (!instance) return;
    arena_reset(&instance->arena);
    instance->root = NULL;
}

// --- UiElement (Instance) ---

static UiElement* element_alloc(UiInstance* instance, const UiNodeSpec* spec) {
    // Arena allocation guarantees zero-init
    UiElement* el = (UiElement*)arena_alloc_zero(&instance->arena, sizeof(UiElement));
    el->spec = spec;
    return el;
}

UiElement* ui_element_create(UiInstance* instance, const UiNodeSpec* spec, void* data, const MetaStruct* meta) {
    if (!instance || !spec) return NULL;

    UiElement* el = element_alloc(instance, spec);
    el->data_ptr = data;
    el->meta = meta;

    // Cache Bindings
    if (meta) {
        if (spec->text_source)  el->bind_text  = meta_find_field(meta, spec->text_source);
        if (spec->value_source) el->bind_value = meta_find_field(meta, spec->value_source);
        if (spec->x_source)     el->bind_x     = meta_find_field(meta, spec->x_source);
        if (spec->y_source)     el->bind_y     = meta_find_field(meta, spec->y_source);
        if (spec->w_source)     el->bind_w     = meta_find_field(meta, spec->w_source);
        if (spec->h_source)     el->bind_h     = meta_find_field(meta, spec->h_source);
    }

    // Create Children
    if (spec->child_count > 0) {
        el->child_count = spec->child_count;
        // Allocate array in arena
        el->children = (UiElement**)arena_alloc_zero(&instance->arena, spec->child_count * sizeof(UiElement*));
        
        for (size_t i = 0; i < spec->child_count; ++i) {
            el->children[i] = ui_element_create(instance, spec->children[i], data, meta);
            if (el->children[i]) {
                el->children[i]->parent = el;
            }
        }
    }
    
    return el;
}

void ui_element_update(UiElement* element) {
    if (!element || !element->spec) return;
    
    // 1. Resolve Text Binding (Cached)
    if (element->bind_text && element->data_ptr) {
        char new_text[128] = {0};
        ui_bind_read_string(element->data_ptr, element->bind_text, new_text, sizeof(new_text));
        
        if (strncmp(element->cached_text, new_text, 128) != 0) {
            strncpy(element->cached_text, new_text, 128 - 1); // safe copy
            element->cached_text[127] = '\0';
            // Text change affects layout (size) and redraw
            element->dirty_flags |= (UI_DIRTY_LAYOUT | UI_DIRTY_REDRAW);
        }
    } else if (element->spec->static_text) {
        // Init cache from static text if empty (first run or if static text is used without binding)
        // Optimization: Check if match first
        if (strncmp(element->cached_text, element->spec->static_text, 128) != 0) {
            strncpy(element->cached_text, element->spec->static_text, 128 - 1);
            element->cached_text[127] = '\0';
            element->dirty_flags |= UI_DIRTY_LAYOUT;
        }
    }
    
    // 2. Resolve Geometry Bindings (X/Y)
    if (element->data_ptr) {
        if (element->bind_x) {
            float val = meta_get_float(element->data_ptr, element->bind_x);
            if (element->rect.x != val) {
                element->rect.x = val;
                element->dirty_flags |= UI_DIRTY_LAYOUT;
            }
        }
        if (element->bind_y) {
            float val = meta_get_float(element->data_ptr, element->bind_y);
            if (element->rect.y != val) {
                element->rect.y = val;
                element->dirty_flags |= UI_DIRTY_LAYOUT;
            }
        }
        if (element->bind_w) {
            float val = meta_get_float(element->data_ptr, element->bind_w);
            if (element->rect.w != val) {
                element->rect.w = val;
                element->dirty_flags |= UI_DIRTY_LAYOUT;
            }
        }
        if (element->bind_h) {
            float val = meta_get_float(element->data_ptr, element->bind_h);
            if (element->rect.h != val) {
                element->rect.h = val;
                element->dirty_flags |= UI_DIRTY_LAYOUT;
            }
        }
    }

    // Recurse
    for (size_t i = 0; i < element->child_count; ++i) {
        ui_element_update(element->children[i]);
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
