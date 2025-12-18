#include "ui_parser.h"
#include "foundation/config/config_document.h"
#include "foundation/logger/logger.h"
#include "foundation/memory/arena.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// --- Helper Functions ---

static UiLayoutStrategy parse_layout_strategy(const char* str) {
    if (!str) return UI_LAYOUT_FLEX_COLUMN;
    if (strcmp(str, "row") == 0) return UI_LAYOUT_FLEX_ROW;
    if (strcmp(str, "column") == 0) return UI_LAYOUT_FLEX_COLUMN;
    if (strcmp(str, "canvas") == 0) return UI_LAYOUT_CANVAS;
    if (strcmp(str, "overlay") == 0) return UI_LAYOUT_OVERLAY;
    return UI_LAYOUT_FLEX_COLUMN;
}

static UiKind parse_kind(const char* type_str, uint32_t* out_flags) {
    *out_flags = UI_FLAG_NONE;
    if (!type_str) return UI_KIND_CONTAINER;

    if (strcmp(type_str, "panel") == 0) return UI_KIND_CONTAINER;
    if (strcmp(type_str, "container") == 0) return UI_KIND_CONTAINER;
    
    if (strcmp(type_str, "label") == 0) return UI_KIND_TEXT;
    if (strcmp(type_str, "text") == 0) return UI_KIND_TEXT;
    
    if (strcmp(type_str, "button") == 0) {
        *out_flags |= UI_FLAG_CLICKABLE | UI_FLAG_FOCUSABLE;
        return UI_KIND_CONTAINER; // A button is just a clickable container usually
    }
    
    if (strcmp(type_str, "checkbox") == 0) {
        *out_flags |= UI_FLAG_CLICKABLE;
        return UI_KIND_ICON; // TODO: Needs specific rendering logic
    }

    if (strcmp(type_str, "slider") == 0) {
        *out_flags |= UI_FLAG_CLICKABLE | UI_FLAG_DRAGGABLE;
        return UI_KIND_CONTAINER; // Container with specific behavior
    }

    if (strcmp(type_str, "curve") == 0) return UI_KIND_CUSTOM;

    return UI_KIND_CONTAINER;
}

static float parse_float(const ConfigNode* node, float fallback) {
    if (node && node->scalar) {
        return (float)atof(node->scalar);
    }
    return fallback;
}

// --- Recursive Loader ---

static UiNodeSpec* load_recursive(UiAsset* asset, const void* node_ptr) {
    const ConfigNode* node = (const ConfigNode*)node_ptr;
    if (!node || node->type != CONFIG_NODE_MAP) return NULL;

    // Allocate Spec from Asset's Arena
    UiNodeSpec* spec = ui_asset_push_node(asset);
    if (!spec) return NULL;

    // 1. Type & Flags
    const ConfigNode* type_node = config_node_get_scalar(node, "type");
    const char* type_str = type_node ? type_node->scalar : "panel";
    spec->kind = parse_kind(type_str, &spec->flags);

    // Explicit flags overrides
    const ConfigNode* draggable = config_node_get_scalar(node, "draggable");
    if (draggable && strcmp(draggable->scalar, "true") == 0) spec->flags |= UI_FLAG_DRAGGABLE;

    const ConfigNode* clickable = config_node_get_scalar(node, "clickable");
    if (clickable && strcmp(clickable->scalar, "true") == 0) spec->flags |= UI_FLAG_CLICKABLE;

    // 2. Identity
    const ConfigNode* id_node = config_node_get_scalar(node, "id");
    if (id_node) spec->id = arena_push_string(&asset->arena, id_node->scalar);

    const ConfigNode* style = config_node_get_scalar(node, "style");
    if (style) spec->style_name = arena_push_string(&asset->arena, style->scalar);

    // 3. Layout
    const ConfigNode* layout = config_node_get_scalar(node, "layout");
    spec->layout = parse_layout_strategy(layout ? layout->scalar : NULL);

    spec->width = parse_float(config_node_get_scalar(node, "width"), -1.0f);
    spec->height = parse_float(config_node_get_scalar(node, "height"), -1.0f);
    spec->padding = parse_float(config_node_get_scalar(node, "padding"), 0.0f);
    spec->spacing = parse_float(config_node_get_scalar(node, "spacing"), 0.0f);

    // 4. Data Bindings
    const ConfigNode* text = config_node_get_scalar(node, "text");
    if (text) {
        // Check if it's a binding "{...}" or static text
        if (text->scalar[0] == '{') {
             // quick hack, remove braces
             // In real engine, we'd parse this better.
             size_t len = strlen(text->scalar);
             if (len > 2) {
                 char* buf = arena_push_string(&asset->arena, text->scalar); // copy first
                 buf[len-1] = '\0'; // trim }
                 spec->text_source = buf + 1; // skip {
             }
        } else {
             spec->static_text = arena_push_string(&asset->arena, text->scalar);
        }
    }

    const ConfigNode* bind = config_node_get_scalar(node, "bind");
    if (bind) spec->value_source = arena_push_string(&asset->arena, bind->scalar);
    
    // Geometry Bindings
    const ConfigNode* bx = config_node_get_scalar(node, "bind_x");
    if (bx) spec->x_source = arena_push_string(&asset->arena, bx->scalar);
    const ConfigNode* by = config_node_get_scalar(node, "bind_y");
    if (by) spec->y_source = arena_push_string(&asset->arena, by->scalar);

    // List/Repeater Support
    const ConfigNode* items = config_node_get_scalar(node, "items");
    if (items) spec->data_source = arena_push_string(&asset->arena, items->scalar);

    const ConfigNode* item_template = config_map_get(node, "item_template");
    if (item_template) {
        spec->item_template = load_recursive(asset, item_template);
    }

    // 5. Children
    const ConfigNode* children = config_node_get_sequence(node, "children");
    if (children) {
        spec->child_count = children->item_count;
        spec->children = (UiNodeSpec**)arena_alloc_zero(&asset->arena, spec->child_count * sizeof(UiNodeSpec*));
        for (size_t i = 0; i < spec->child_count; ++i) {
            spec->children[i] = load_recursive(asset, children->items[i]);
        }
    }

    return spec;
}

UiAsset* ui_parser_load_from_file(const char* path) {
    if (!path) return NULL;

    LOG_INFO("UiParser: Loading UI definition from file: %s", path);

    ConfigError err;
    ConfigDocument doc;
    if (!load_config_document(path, CONFIG_FORMAT_YAML, &doc, &err)) {
        LOG_ERROR("UiParser: Failed to load %s: %s (line %d)", path, err.message, err.line);
        return NULL;
    }

    // Create Asset (Owner)
    UiAsset* asset = ui_asset_create(64 * 1024);
    if (!asset) {
        config_document_free(&doc);
        return NULL;
    }

    asset->root = load_recursive(asset, doc.root);
    
    config_document_free(&doc);
    return asset;
}
