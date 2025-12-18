#include "ui_loader.h"
#include "foundation/config/config_document.h"
#include "foundation/logger/logger.h"
#include "foundation/memory/arena.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// --- Helper Functions ---

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
    if (strcmp(type_str, "curve") == 0) return UI_NODE_CURVE;
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

// Internal function that propagates the arena
static UiDef* load_recursive(MemoryArena* arena, const void* node_ptr) {
    const ConfigNode* node = (const ConfigNode*)node_ptr;
    if (!node || node->type != CONFIG_NODE_MAP) return NULL;

    const ConfigNode* type_node = config_node_get_scalar(node, "type");
    const char* type_str = type_node ? type_node->scalar : "panel";
    UiNodeType type = parse_node_type(type_str);

    // Identity & Style (Pre-load for logging)
    const ConfigNode* id_node = config_node_get_scalar(node, "id");
    const char* id_str = id_node ? id_node->scalar : "(anon)";

    // Allocate generic node from Arena
    UiDef* def = ui_def_create(arena, type);
    if (!def) return NULL;

    // 1. Identity & Style
    if (id_node) {
        def->id = arena_push_string(arena, id_node->scalar);
    } else {
        static int anon_counter = 0;
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "%s_%d", type_str, ++anon_counter);
        def->id = arena_push_string(arena, buffer);
    }

    const ConfigNode* style = config_node_get_scalar(node, "style");
    if (style) def->style_name = arena_push_string(arena, style->scalar);

    // 2. Layout Props
    const ConfigNode* layout = config_node_get_scalar(node, "layout");
    def->layout = parse_layout_type(layout ? layout->scalar : NULL);
    
    if (type == UI_NODE_LIST && !layout) def->layout = UI_LAYOUT_COLUMN;

    def->width = parse_float(config_node_get_scalar(node, "width"), -1.0f);
    def->height = parse_float(config_node_get_scalar(node, "height"), -1.0f);
    def->padding = parse_float(config_node_get_scalar(node, "padding"), 0.0f);
    def->spacing = parse_float(config_node_get_scalar(node, "spacing"), 0.0f);
    
    const ConfigNode* draggable = config_node_get_scalar(node, "draggable");
    if (draggable) def->draggable = (strcmp(draggable->scalar, "true") == 0);

    def->min_value = parse_float(config_node_get_scalar(node, "min"), 0.0f);
    def->max_value = parse_float(config_node_get_scalar(node, "max"), 1.0f);

    // 3. Data Binding
    const ConfigNode* text = config_node_get_scalar(node, "text");
    if (text) def->text = arena_push_string(arena, text->scalar);

    const ConfigNode* bind = config_node_get_scalar(node, "bind");
    if (bind) def->bind_source = arena_push_string(arena, bind->scalar);
    
    const ConfigNode* items = config_node_get_scalar(node, "items");
    if (items) {
        def->data_source = arena_push_string(arena, items->scalar);
    } else {
        const ConfigNode* data = config_node_get_scalar(node, "data");
        if (data) def->data_source = arena_push_string(arena, data->scalar);
    }

    const ConfigNode* count = config_node_get_scalar(node, "count");
    if (count) def->count_source = arena_push_string(arena, count->scalar);

    // Geometry Bindings
    const ConfigNode* bx = config_node_get_scalar(node, "bind_x");
    if (bx) def->x_source = arena_push_string(arena, bx->scalar);
    const ConfigNode* by = config_node_get_scalar(node, "bind_y");
    if (by) def->y_source = arena_push_string(arena, by->scalar);
    const ConfigNode* bw = config_node_get_scalar(node, "bind_w");
    if (bw) def->w_source = arena_push_string(arena, bw->scalar);
    const ConfigNode* bh = config_node_get_scalar(node, "bind_h");
    if (bh) def->h_source = arena_push_string(arena, bh->scalar);

    // Curve Bindings
    const ConfigNode* bu1 = config_node_get_scalar(node, "bind_u1");
    if (bu1) def->u1_source = arena_push_string(arena, bu1->scalar);
    const ConfigNode* bv1 = config_node_get_scalar(node, "bind_v1");
    if (bv1) def->v1_source = arena_push_string(arena, bv1->scalar);
    const ConfigNode* bu2 = config_node_get_scalar(node, "bind_u2");
    if (bu2) def->u2_source = arena_push_string(arena, bu2->scalar);
    const ConfigNode* bv2 = config_node_get_scalar(node, "bind_v2");
    if (bv2) def->v2_source = arena_push_string(arena, bv2->scalar);

    // 4. List Template
    if (type == UI_NODE_LIST) {
        const ConfigNode* item_template = config_map_get(node, "item_template");
        if (item_template) {
            def->item_template = load_recursive(arena, item_template);
        }
    }

    // 5. Children
    const ConfigNode* children = config_node_get_sequence(node, "children");
    if (children) {
        def->child_count = children->item_count;
        // Allocate array of pointers from arena
        def->children = (UiDef**)arena_alloc_zero(arena, def->child_count * sizeof(UiDef*));
        if (def->children) {
            for (size_t i = 0; i < def->child_count; ++i) {
                def->children[i] = load_recursive(arena, children->items[i]);
            }
        }
    }

    return def;
}

UiDef* ui_loader_load_from_file(const char* path) {
    if (!path) return NULL;

    LOG_INFO("UiLoader: Loading UI definition from file: %s", path);

    ConfigError err;
    ConfigDocument doc;
    if (!load_config_document(path, CONFIG_FORMAT_YAML, &doc, &err)) {
        LOG_ERROR("UiLoader: Failed to load %s: %s (line %d)", path, err.message, err.line);
        return NULL;
    }

    // Initialize Arena for this UI Definition Tree
    // 64KB should be plenty for typical UI
    MemoryArena arena;
    if (!arena_init(&arena, 64 * 1024)) {
        config_document_free(&doc);
        return NULL;
    }

    // The root node gets the ownership of the arena
    UiDef* root = load_recursive(&arena, doc.root);
    
    if (root) {
        // Transfer arena ownership to the root node struct
        // Note: root itself is allocated INSIDE the arena, but we copy the struct content (base ptr) here.
        root->arena = arena;
    } else {
        arena_destroy(&arena);
    }

    config_document_free(&doc);
    return root;
}

// Deprecated or redirect to internal
UiDef* ui_loader_load_from_node(const void* node_ptr) {
    // This function cannot work without an arena context in the new system.
    // It should probably be removed or take an arena.
    // For now, return NULL to signal it's not supported directly.
    LOG_ERROR("ui_loader_load_from_node called without arena context");
    return NULL;
}