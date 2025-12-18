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

// --- UiElement (Instance) ---

static UiElement* element_alloc(const UiNodeSpec* spec) {
    UiElement* el = (UiElement*)calloc(1, sizeof(UiElement));
    el->spec = spec;
    return el;
}

UiElement* ui_element_create(const UiNodeSpec* spec, void* data, const MetaStruct* meta) {
    if (!spec) return NULL;

    UiElement* el = element_alloc(spec);
    el->data_ptr = data;
    el->meta = meta;

    // Create Children
    if (spec->child_count > 0) {
        el->child_count = spec->child_count;
        el->children = (UiElement**)calloc(el->child_count, sizeof(UiElement*));
        
        for (size_t i = 0; i < spec->child_count; ++i) {
            el->children[i] = ui_element_create(spec->children[i], data, meta);
            if (el->children[i]) {
                el->children[i]->parent = el;
            }
        }
    }
    
    return el;
}

void ui_element_free(UiElement* element) {
    if (!element) return;
    
    for (size_t i = 0; i < element->child_count; ++i) {
        ui_element_free(element->children[i]);
    }
    free(element->children);
    
    if (element->cached_text) free(element->cached_text);
    free(element);
}

void ui_element_update(UiElement* element) {
    if (!element || !element->spec) return;
    
    // 1. Resolve Text Binding
    // Only resolve if binding source exists
    if (element->spec->text_source && element->data_ptr && element->meta) {
        // Find field
        const MetaField* field = meta_find_field(element->meta, element->spec->text_source);
        if (field) {
            if (field->type == META_TYPE_FLOAT) {
                float val = meta_get_float(element->data_ptr, field);
                char buffer[32];
                snprintf(buffer, sizeof(buffer), "%.2f", val);
                
                // Update cache if changed
                if (!element->cached_text || strcmp(element->cached_text, buffer) != 0) {
                    if (element->cached_text) free(element->cached_text);
                    element->cached_text = strdup(buffer);
                }
            } else if (field->type == META_TYPE_STRING) {
                const char* val = meta_get_string(element->data_ptr, field);
                 if (val) {
                    if (!element->cached_text || strcmp(element->cached_text, val) != 0) {
                        if (element->cached_text) free(element->cached_text);
                        element->cached_text = strdup(val);
                    }
                 }
            }
        }
    }

    // 2. Resolve Geometry Bindings (X/Y)
    // Critical for Canvas/Floating Nodes
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

    