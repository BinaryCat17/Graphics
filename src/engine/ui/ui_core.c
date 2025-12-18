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
    
    // 1. Resolve Text Binding
    if (element->spec->text_source && element->data_ptr && element->meta) {
        // Find field
        const MetaField* field = meta_find_field(element->meta, element->spec->text_source);
        if (field) {
            char buffer[64];
            ui_bind_read_string(element->data_ptr, field, buffer, sizeof(buffer));
            
            // Update cache if changed
            if (strcmp(element->cached_text, buffer) != 0) {
                strncpy(element->cached_text, buffer, sizeof(element->cached_text) - 1);
                element->cached_text[sizeof(element->cached_text) - 1] = '\0';
            }
        }
    }

    // 2. Resolve Geometry Bindings (X/Y)
    if (element->data_ptr && element->meta) {
        if (element->spec->x_source) {
            const MetaField* fx = meta_find_field(element->meta, element->spec->x_source);
            if (fx) element->rect.x = meta_get_float(element->data_ptr, fx);
        }
        if (element->spec->y_source) {
            const MetaField* fy = meta_find_field(element->meta, element->spec->y_source);
            if (fy) element->rect.y = meta_get_float(element->data_ptr, fy);
        }
        if (element->spec->w_source) {
            const MetaField* fw = meta_find_field(element->meta, element->spec->w_source);
            if (fw) element->rect.w = meta_get_float(element->data_ptr, fw);
        }
        if (element->spec->h_source) {
            const MetaField* fh = meta_find_field(element->meta, element->spec->h_source);
            if (fh) element->rect.h = meta_get_float(element->data_ptr, fh);
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

    if (field->type == META_TYPE_STRING) {
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
