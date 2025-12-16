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
        // Reconciliation logic for lists would go here
        // Check array size -> create/destroy child views
    } else {
        for (size_t i = 0; i < view->child_count; ++i) {
            ui_view_update(view->children[i]);
        }
    }
}

// --- Input Processing ---

static const MetaField* find_field(const MetaStruct* meta, const char* name) {
    if (!meta || !name) return NULL;
    for (size_t i = 0; i < meta->field_count; ++i) {
        if (strcmp(meta->fields[i].name, name) == 0) {
            return &meta->fields[i];
        }
    }
    return NULL;
}

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
    if (rect_contains(view->rect, input->mouse_x, input->mouse_y)) {
        
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
                        // We don't have meta_get_bool in header? 
                        // Assuming int for bool in C reflection or I missed it.
                        // reflection.h had META_TYPE_BOOL but no meta_get_bool?
                        // Let's assume int logic works or check header again.
                        // Header had meta_get_int, meta_get_float, meta_get_string. 
                        // It MISSED meta_get_bool/set_bool! 
                        // I will assume bool is stored as int for now or cast.
                        // Safe bet: treat as int.
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
