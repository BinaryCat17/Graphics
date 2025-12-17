#include "ui_def.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// --- Helper: String Utils ---

static char* strdup_safe(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char* copy = (char*)malloc(len + 1);
    if (copy) memcpy(copy, s, len + 1);
    return copy;
}

static const MetaField* find_field(const MetaStruct* meta, const char* name) {
    if (!meta || !name) return NULL;
    for (size_t i = 0; i < meta->field_count; ++i) {
        if (strcmp(meta->fields[i].name, name) == 0) {
            return &meta->fields[i];
        }
    }
    return NULL;
}

// --- UiDef Implementation ---

UiDef* ui_def_create(UiNodeType type) {
    UiDef* def = (UiDef*)calloc(1, sizeof(UiDef));
    if (def) {
        def->type = type;
        def->width = -1.0f; // Auto
        def->height = -1.0f; // Auto
    }
    return def;
}

void ui_def_free(UiDef* def) {
    if (!def) return;
    free(def->id);
    free(def->style_name);
    free(def->text);
    free(def->bind_source);
    free(def->data_source);
    free(def->count_source);
    
    if (def->item_template) ui_def_free(def->item_template);
    
    if (def->children) {
        for (size_t i = 0; i < def->child_count; ++i) {
            ui_def_free(def->children[i]);
        }
        free(def->children);
    }
    free(def);
}

// --- UiView Implementation ---

UiView* ui_view_create(const UiDef* def, void* root_data, const MetaStruct* root_type) {
    if (!def) return NULL;
    
    UiView* view = (UiView*)calloc(1, sizeof(UiView));
    if (!view) return NULL;
    
    view->def = def;
    view->data_ptr = root_data;
    view->meta = root_type;
    
    // Resolve Data Context shift if needed
    if (def->data_source && root_data && root_type) {
        // TBD: Find field in root_type by name (def->data_source)
        // For now, let's assume direct binding or implement simple field lookup
        // view->data_ptr = meta_get_field_ptr(...)
        // view->meta = meta_get_field_type(...)
    }
    
    // Initial Child Creation
    if (def->type == UI_NODE_LIST) {
        // TBD: Iterate over array and create children based on item_template
    } else if (def->child_count > 0) {
        view->children = (UiView**)calloc(def->child_count, sizeof(UiView*));
        view->child_capacity = def->child_count;
        view->child_count = def->child_count;
        for (size_t i = 0; i < def->child_count; ++i) {
            view->children[i] = ui_view_create(def->children[i], view->data_ptr, view->meta);
            if (view->children[i]) view->children[i]->parent = view;
        }
    }
    
    return view;
}

void ui_view_free(UiView* view) {
    if (!view) return;
    
    free(view->cached_text);
    
    if (view->children) {
        for (size_t i = 0; i < view->child_count; ++i) {
            ui_view_free(view->children[i]);
        }
        free(view->children);
    }
    free(view);
}

// --- Binding Logic ---

static void resolve_text_binding(UiView* view) {
    if (!view || !view->def->text) return;
    
    const char* pattern = view->def->text;
    // Check for "{name}" pattern
    const char* start = strchr(pattern, '{');
    const char* end = strchr(pattern, '}');
    
    if (start && end && end > start && view->data_ptr && view->meta) {
        // Extract key
        char key[64];
        size_t len = (size_t)(end - start - 1);
        if (len >= sizeof(key)) len = sizeof(key) - 1;
        memcpy(key, start + 1, len);
        key[len] = 0;
        
        // Find field
        // We need to implement meta_find_field in reflection or loop here
        const MetaField* field = NULL;
        for (size_t i = 0; i < view->meta->field_count; ++i) {
            if (strcmp(view->meta->fields[i].name, key) == 0) {
                field = &view->meta->fields[i];
                break;
            }
        }
        
        if (field) {
            char val_buf[64];
            if (field->type == META_TYPE_FLOAT) {
                snprintf(val_buf, sizeof(val_buf), "%.2f", meta_get_float(view->data_ptr, field));
            } else if (field->type == META_TYPE_INT) {
                snprintf(val_buf, sizeof(val_buf), "%d", meta_get_int(view->data_ptr, field));
            } else if (field->type == META_TYPE_STRING) {
                const char* s = meta_get_string(view->data_ptr, field);
                snprintf(val_buf, sizeof(val_buf), "%s", s ? s : "");
            } else {
                snprintf(val_buf, sizeof(val_buf), "<?>");
            }
            
            // Reconstruct string
            // This is a naive implementation that replaces the first occurrence
            size_t prefix_len = (size_t)(start - pattern);
            size_t val_len = strlen(val_buf);
            size_t suffix_len = strlen(end + 1);
            
            char* new_text = (char*)malloc(prefix_len + val_len + suffix_len + 1);
            if (new_text) {
                memcpy(new_text, pattern, prefix_len);
                memcpy(new_text + prefix_len, val_buf, val_len);
                strcpy(new_text + prefix_len + val_len, end + 1);
                
                free(view->cached_text);
                view->cached_text = new_text;
            }
        }
    } else if (!view->cached_text) {
        // Static text
        view->cached_text = strdup_safe(pattern);
    }
}

void ui_view_update(UiView* view) {
    if (!view) return;
    
    // 1. Resolve Bindings
    resolve_text_binding(view);
    
    // 2. Update Children
    if (view->def->type == UI_NODE_LIST) {
        // Reconciliation logic for lists
        int count = 0;
        void* array_base = NULL;
        const MetaStruct* item_meta = NULL;

        if (view->def->count_source && view->def->data_source && view->data_ptr && view->meta) {
            const MetaField* count_field = find_field(view->meta, view->def->count_source);
            const MetaField* array_field = find_field(view->meta, view->def->data_source);
            
            if (count_field && count_field->type == META_TYPE_INT) {
                count = meta_get_int(view->data_ptr, count_field);
            }
            
            if (array_field && array_field->type == META_TYPE_POINTER) {
                void* field_addr = meta_get_field_ptr(view->data_ptr, array_field);
                if (field_addr) {
                    array_base = *(void**)field_addr;
                    item_meta = meta_get_struct(array_field->type_name);
                }
            }
        }

        // Resize Children Array
        if (view->child_capacity < (size_t)count) {
            size_t new_cap = (size_t)count;
            UiView** new_children = (UiView**)realloc(view->children, sizeof(UiView*) * new_cap);
            if (new_children) {
                view->children = new_children;
                // Init new slots to NULL
                for (size_t i = view->child_capacity; i < new_cap; ++i) view->children[i] = NULL;
                view->child_capacity = new_cap;
            } else {
                count = (int)view->child_capacity; // Allocation failed, cap count
            }
        }

        // Update / Create Loop
        if (array_base && item_meta) {
            void** ptrs = (void**)array_base; // Assume array of pointers
            for (int i = 0; i < count; ++i) {
                void* item_ptr = ptrs[i];
                
                if (!view->children[i]) {
                    // Create new
                    if (view->def->item_template) {
                        view->children[i] = ui_view_create(view->def->item_template, item_ptr, item_meta);
                        if (view->children[i]) view->children[i]->parent = view;
                    }
                } else {
                    // Update Context
                    view->children[i]->data_ptr = item_ptr;
                    view->children[i]->meta = item_meta;
                }
                
                if (view->children[i]) {
                    ui_view_update(view->children[i]);
                }
            }
        }

        // Cleanup excess
        for (size_t i = (size_t)count; i < view->child_count; ++i) {
            ui_view_free(view->children[i]);
            view->children[i] = NULL;
        }
        view->child_count = (size_t)count;

    } else {
        for (size_t i = 0; i < view->child_count; ++i) {
            ui_view_update(view->children[i]);
        }
    }
}

// --- Input Processing ---

static bool rect_contains(Rect r, float x, float y) {
    return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
}

void ui_view_process_input(UiView* view, const InputState* input) {
    if (!view || !input || !view->def) return;

    // 1. Process Children (Top-down for hit testing usually, but for simple UI order doesn't matter much)
    // We process children first so they are "on top" if z-order matches tree order
    for (size_t i = 0; i < view->child_count; ++i) {
        ui_view_process_input(view->children[i], input);
    }

    // 2. Hit Test
    bool hover = rect_contains(view->rect, input->mouse_x, input->mouse_y);
    view->is_hovered = hover;
    view->is_pressed = hover && input->mouse_down;

    if (hover) {
        
        // Button Click
        if (view->def->type == UI_NODE_BUTTON && input->mouse_clicked) {
            if (view->def->bind_source && view->data_ptr && view->meta) {
                const MetaField* field = find_field(view->meta, view->def->bind_source);
                if (field) {
                    if (field->type == META_TYPE_INT) {
                        // Toggle logic or Action signal
                        int current = meta_get_int(view->data_ptr, field);
                        meta_set_int(view->data_ptr, field, !current); 
                    } else if (field->type == META_TYPE_BOOL) {
                        int current = meta_get_int(view->data_ptr, field);
                        meta_set_int(view->data_ptr, field, !current); 
                    }
                }
            }
        }
        
        // Slider Drag
        if (view->def->type == UI_NODE_SLIDER && input->mouse_down) {
            float relative = (input->mouse_x - view->rect.x) / view->rect.w;
            if (relative < 0) relative = 0.0f;
            if (relative > 1) relative = 1.0f;
            
            float min = view->def->min_value;
            float max = view->def->max_value;
            float new_val = min + relative * (max - min);
            
            if (view->def->bind_source && view->data_ptr && view->meta) {
                const MetaField* field = find_field(view->meta, view->def->bind_source);
                if (field && field->type == META_TYPE_FLOAT) {
                     meta_set_float(view->data_ptr, field, new_val);
                }
            }
        }
    }
}
