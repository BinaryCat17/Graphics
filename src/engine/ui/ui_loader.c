#include "ui_loader.h"
#include "foundation/config/config_document.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// --- Helper Functions ---

static char* strdup_safe(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char* copy = (char*)malloc(len + 1);
    if (copy) memcpy(copy, s, len + 1);
    return copy;
}

static UiNodeType parse_node_type(const char* type_str) {
    if (!type_str) return UI_NODE_PANEL;
    if (strcmp(type_str, "panel") == 0) return UI_NODE_PANEL;
    if (strcmp(type_str, "label") == 0) return UI_NODE_LABEL;
    if (strcmp(type_str, "text") == 0) return UI_NODE_LABEL;
    if (strcmp(type_str, "button") == 0) return UI_NODE_BUTTON;
    if (strcmp(type_str, "slider") == 0) return UI_NODE_SLIDER;
    if (strcmp(type_str, "checkbox") == 0) return UI_NODE_CHECKBOX;
    if (strcmp(type_str, "list") == 0) return UI_NODE_LIST;
    if (strcmp(type_str, "container") == 0) return UI_NODE_CONTAINER;
    if (strcmp(type_str, "custom") == 0) return UI_NODE_CUSTOM;
    return UI_NODE_PANEL;
}

static UiLayoutType parse_layout_type(const char* layout_str) {
    if (!layout_str) return UI_LAYOUT_COLUMN;
    if (strcmp(layout_str, "column") == 0) return UI_LAYOUT_COLUMN;
    if (strcmp(layout_str, "row") == 0) return UI_LAYOUT_ROW;
    if (strcmp(layout_str, "overlay") == 0) return UI_LAYOUT_OVERLAY;
    if (strcmp(layout_str, "dock") == 0) return UI_LAYOUT_DOCK;
    return UI_LAYOUT_COLUMN;
}

static float parse_float(const ConfigNode* node, float fallback) {
    if (node && node->scalar) {
        return (float)atof(node->scalar);
    }
    return fallback;
}

// --- Recursive Loader ---

UiDef* ui_loader_load_from_node(const void* node_ptr) {
    const ConfigNode* node = (const ConfigNode*)node_ptr;
    if (!node || node->type != CONFIG_NODE_MAP) return NULL;

    const ConfigNode* type_node = config_node_get_scalar(node, "type");
    UiNodeType type = parse_node_type(type_node ? type_node->scalar : "panel");

    UiDef* def = ui_def_create(type);
    if (!def) return NULL;

    // 1. Identity & Style
    const ConfigNode* id = config_node_get_scalar(node, "id");
    if (id) def->id = strdup_safe(id->scalar);

    const ConfigNode* style = config_node_get_scalar(node, "style");
    if (style) def->style_name = strdup_safe(style->scalar);

    // 2. Layout Props
    const ConfigNode* layout = config_node_get_scalar(node, "layout");
    def->layout = parse_layout_type(layout ? layout->scalar : NULL);
    
    // Default layout for lists is usually column
    if (type == UI_NODE_LIST && !layout) def->layout = UI_LAYOUT_COLUMN;

    def->width = parse_float(config_node_get_scalar(node, "width"), -1.0f);
    def->height = parse_float(config_node_get_scalar(node, "height"), -1.0f);
    def->padding = parse_float(config_node_get_scalar(node, "padding"), 0.0f);
    def->spacing = parse_float(config_node_get_scalar(node, "spacing"), 0.0f);

    // 3. Data Binding
    const ConfigNode* text = config_node_get_scalar(node, "text");
    if (text) def->text = strdup_safe(text->scalar);

    const ConfigNode* bind = config_node_get_scalar(node, "bind");
    if (bind) def->bind_source = strdup_safe(bind->scalar);
    
    // "items" or "data_context" for lists
    const ConfigNode* items = config_node_get_scalar(node, "items");
    if (items) def->data_source = strdup_safe(items->scalar);

    // 4. List Template
    if (type == UI_NODE_LIST) {
        const ConfigNode* item_template = config_map_get(node, "item_template");
        if (item_template) {
            def->item_template = ui_loader_load_from_node(item_template);
        }
    }

    // 5. Children
    const ConfigNode* children = config_node_get_sequence(node, "children");
    if (children) {
        def->child_count = children->item_count;
        def->children = (UiDef**)calloc(def->child_count, sizeof(UiDef*));
        if (def->children) {
            for (size_t i = 0; i < def->child_count; ++i) {
                def->children[i] = ui_loader_load_from_node(children->items[i]);
            }
        }
    }

    return def;
}

UiDef* ui_loader_load_from_file(const char* path) {
    if (!path) return NULL;

    ConfigError err;
    ConfigDocument doc;
    if (!load_config_document(path, CONFIG_FORMAT_YAML, &doc, &err)) {
        fprintf(stderr, "UiLoader: Failed to load %s: %s (line %d)\n", path, err.message, err.line);
        return NULL;
    }

    UiDef* root = ui_loader_load_from_node(doc.root);
    config_document_free(&doc);
    return root;
}
