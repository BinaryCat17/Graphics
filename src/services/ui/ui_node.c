#include "services/ui/ui_node.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/config/config_io.h"
#include "core/platform/fs.h"
#include "services/scene/cad_scene.h"
#include "services/ui/scene_ui.h"
#include "core/platform/platform.h"

static int ascii_strcasecmp(const char* a, const char* b) {
    if (a == b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    while (*a && *b) {
        int da = tolower((unsigned char)*a);
        int db = tolower((unsigned char)*b);
        if (da != db) return da - db;
        ++a;
        ++b;
    }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

static char* basename_no_ext(const char* path) {
    if (!path) return NULL;
    const char* slash = strrchr(path, '/');
    if (!slash) slash = strrchr(path, '\\');
    const char* name = slash ? slash + 1 : path;
    const char* dot = strrchr(name, '.');
    size_t len = dot ? (size_t)(dot - name) : strlen(name);
    char* out = (char*)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, name, len);
    out[len] = 0;
    return out;
}

static char* dirname_dup(const char* path) {
    if (!path) return NULL;
    const char* slash = strrchr(path, '/');
    if (!slash) slash = strrchr(path, '\\');
    if (!slash) return NULL;
    size_t len = (size_t)(slash - path);
    char* out = (char*)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, path, len);
    out[len] = 0;
    return out;
}

static char* join_path(const char* dir, const char* leaf) {
    if (!dir || !leaf) return NULL;
    size_t dir_len = strlen(dir);
    while (dir_len > 0 && (dir[dir_len - 1] == '/' || dir[dir_len - 1] == '\\')) dir_len--;
    size_t leaf_len = strlen(leaf);
    size_t total = dir_len + 1 + leaf_len + 1;
    char* out = (char*)malloc(total);
    if (!out) return NULL;
    memcpy(out, dir, dir_len);
    out[dir_len] = '/';
    memcpy(out + dir_len + 1, leaf, leaf_len);
    out[total - 1] = 0;
    return out;
}

static float parse_scalar_number(const ConfigNode* node, float fallback) {
    if (!node || node->type != CONFIG_NODE_SCALAR || !node->scalar) return fallback;
    if (node->scalar_type == CONFIG_SCALAR_BOOL) return (ascii_strcasecmp(node->scalar, "true") == 0) ? 1.0f : 0.0f;
    char* end = NULL;
    float v = (float)strtof(node->scalar, &end);
    if (!end || end == node->scalar || *end != '\0') return fallback;
    return v;
}

static int parse_scalar_bool(const ConfigNode* node, int fallback) {
    if (!node || node->type != CONFIG_NODE_SCALAR || !node->scalar) return fallback;
    if (node->scalar_type == CONFIG_SCALAR_BOOL) return ascii_strcasecmp(node->scalar, "true") == 0;
    if (node->scalar_type == CONFIG_SCALAR_NUMBER) return parse_scalar_number(node, (float)fallback) != 0.0f;
    return fallback;
}

static const char* scalar_text(const ConfigNode* node) {
    if (!node || node->type != CONFIG_NODE_SCALAR) return NULL;
    return node->scalar;
}

static UiNode* parse_ui_node_config(const ConfigNode* obj);

typedef struct Prototype {
    char* name;
    UiNode* node;
    struct Prototype* next;
} Prototype;

static int ends_with_yaml(const char* name) {
    if (!name) return 0;
    size_t len = strlen(name);
    return len >= 5 && strcmp(name + len - 5, ".yaml") == 0;
}

static char* relative_component_name(const char* path, const char* base_dir) {
    if (!path) return NULL;
    const char* rel = path;
    if (base_dir) {
        size_t base_len = strlen(base_dir);
        if (strncmp(path, base_dir, base_len) == 0) {
            rel = path + base_len;
            if (*rel == '/' || *rel == '\\') rel++;
        }
    }
    char* trimmed = platform_strdup(rel);
    if (!trimmed) return NULL;
    for (char* p = trimmed; *p; ++p) {
        if (*p == '\\') *p = '/';
    }
    char* dot = strrchr(trimmed, '.');
    if (dot) *dot = 0;
    return trimmed;
}

static void append_prototype(Prototype** list, const char* name, UiNode* node) {
    if (!list || !name || !node) return;
    Prototype* pnode = (Prototype*)calloc(1, sizeof(Prototype));
    if (!pnode) {
        free_ui_tree(node);
        return;
    }
    pnode->name = platform_strdup(name);
    pnode->node = node;
    pnode->next = *list;
    *list = pnode;
}

static void load_component_file(const char* path, const char* base_dir, Prototype** prototypes) {
    if (!path || !prototypes) return;
    ConfigError err = {0};
    ConfigDocument doc = {0};
    if (!load_config_document(path, CONFIG_FORMAT_YAML, &doc, &err)) {
        fprintf(stderr, "Warning: failed to read component %s: %s\n", path, err.message);
        return;
    }

    const ConfigNode* type_node = config_node_get_scalar(doc.root, "type");
    int is_component = type_node && type_node->scalar && strcmp(type_node->scalar, "component") == 0;
    int hinted_by_path = strstr(path, "components/") != NULL || strstr(path, "components\\") != NULL;
    if (!is_component && !hinted_by_path) {
        config_document_free(&doc);
        return;
    }

    const ConfigNode* key_node = config_node_get_scalar(doc.root, "key");
    char* derived = relative_component_name(path, base_dir);
    const char* key = key_node && key_node->scalar ? key_node->scalar : (derived ? derived : NULL);
    const ConfigNode* comp_node = config_node_get_map(doc.root, "component");
    if (!comp_node) comp_node = config_node_get_map(doc.root, "layout");
    if (!comp_node) comp_node = doc.root;

    UiNode* def = parse_ui_node_config(comp_node);
    if (def && key) {
        append_prototype(prototypes, key, def);
    } else {
        free_ui_tree(def);
    }

    free(derived);
    config_document_free(&doc);
}

static void gather_component_tree(const char* dir, const char* base_dir, const char* skip, Prototype** prototypes) {
    if (!dir || !prototypes) return;
    PlatformDir* d = platform_dir_open(dir);
    if (!d) return;
    PlatformDirEntry ent;
    while (platform_dir_read(d, &ent)) {
        char* path = join_path(dir, ent.name);
        if (!path) {
            free(ent.name);
            continue;
        }
        if (ent.is_dir) {
            gather_component_tree(path, base_dir, skip, prototypes);
            free(path);
            free(ent.name);
            continue;
        }
        if (!ends_with_yaml(ent.name)) {
            free(path);
            free(ent.name);
            continue;
        }
        if (skip && strcmp(skip, path) == 0) {
            free(path);
            free(ent.name);
            continue;
        }
        load_component_file(path, base_dir, prototypes);
        free(path);
        free(ent.name);
    }
    platform_dir_close(d);
}

static void gather_component_prototypes(const ConfigDocument* layout_doc, Prototype** prototypes) {
    if (!layout_doc || !layout_doc->source_path || !prototypes) return;
    char* base_dir = dirname_dup(layout_doc->source_path);
    if (!base_dir) return;
    gather_component_tree(base_dir, base_dir, layout_doc->source_path, prototypes);
    char* components_dir = join_path(base_dir, "components");
    if (components_dir) {
        gather_component_tree(components_dir, base_dir, layout_doc->source_path, prototypes);
    }
    free(components_dir);
    free(base_dir);
}

static Style* style_find(const Style* styles, const char* name) {
    for (const Style* s = styles; s; s = s->next) {
        if (strcmp(s->name, name) == 0) return (Style*)s;
    }
    return NULL;
}

static void read_color_node(Color* out, const ConfigNode* node) {
    if (!out || !node) return;
    if (node->type != CONFIG_NODE_SEQUENCE) return;
    float cols[4] = { out->r, out->g, out->b, out->a };
    size_t idx = 0;
    for (size_t i = 0; i < node->item_count && idx < 4; i++) {
        const ConfigNode* it = node->items[i];
        cols[idx] = parse_scalar_number(it, cols[idx]);
        idx++;
    }
    out->r = cols[0]; out->g = cols[1]; out->b = cols[2]; out->a = cols[3];
}

static UiNode* create_node(void) {
    UiNode* node = (UiNode*)calloc(1, sizeof(UiNode));
    if (!node) return NULL;
    node->layout = UI_LAYOUT_NONE;
    node->widget_type = W_PANEL;
    node->z_index = 0;
    node->has_z_index = 0;
    node->z_group = 0;
    node->has_z_group = 0;
    node->spacing = -1.0f;
    node->has_spacing = 0;
    node->columns = 0;
    node->has_columns = 0;
    const Style* default_style = ui_default_style();
    node->style = default_style;
    node->padding_override = 0.0f;
    node->has_padding_override = 0;
    node->border_thickness = 0.0f;
    node->has_border_thickness = 0;
    node->has_border_color = 0;
    node->border_color = default_style->border_color;
    node->color = default_style->background;
    node->text_color = default_style->text;
    node->scrollbar_enabled = 0;
    node->scrollbar_width = 0.0f;
    node->has_scrollbar_width = 0;
    node->scrollbar_track_color = default_style->scrollbar_track_color;
    node->scrollbar_thumb_color = default_style->scrollbar_thumb_color;
    node->has_scrollbar_track_color = 0;
    node->has_scrollbar_thumb_color = 0;
    node->clip_to_viewport = 0;
    node->has_clip_to_viewport = 0;
    node->has_min = node->has_max = node->has_value = 0;
    node->minv = 0.0f; node->maxv = 1.0f; node->value = 0.0f;
    node->min_w = 0.0f; node->min_h = 0.0f;
    node->has_min_w = node->has_min_h = 0;
    node->max_w = 0.0f; node->max_h = 0.0f;
    node->has_max_w = node->has_max_h = 0;
    node->floating_rect = (Rect){0};
    node->has_floating_rect = 0;
    node->floating_min_w = node->floating_min_h = 0.0f;
    node->floating_max_w = node->floating_max_h = 0.0f;
    node->has_floating_min = node->has_floating_max = 0;
    node->has_color = 0;
    node->has_text_color = 0;
    node->docking = NULL;
    node->resizable = 0;
    node->has_resizable = 0;
    node->draggable = 0;
    node->has_draggable = 0;
    node->modal = 0;
    node->has_modal = 0;
    node->on_focus = NULL;
    return node;
}

static int append_child(UiNode* parent, UiNode* child) {
    if (!parent || !child) return 0;
    size_t new_count = parent->child_count + 1;
    UiNode* arr = (UiNode*)realloc(parent->children, new_count * sizeof(UiNode));
    if (!arr) return 0;
    parent->children = arr;
    parent->children[parent->child_count] = *child;
    parent->child_count = new_count;
    free(child);
    return 1;
}

static UiNode* parse_ui_node_config(const ConfigNode* obj) {
    UiNode* node = create_node();
    if (!node) return NULL;
    if (!obj || obj->type != CONFIG_NODE_MAP) return node;

    for (size_t i = 0; i < obj->pair_count; ++i) {
        const ConfigPair* pair = &obj->pairs[i];
        const char* key = pair->key;
        const ConfigNode* val = pair->value;
        if (!key || !val) continue;
        const char* sval = scalar_text(val);
        if (strcmp(key, "type") == 0 && sval) { node->type = platform_strdup(sval); continue; }
        if (strcmp(key, "style") == 0 && sval) { node->style_name = platform_strdup(sval); continue; }
        if (strcmp(key, "x") == 0) { node->rect.x = parse_scalar_number(val, node->rect.x); node->has_x = 1; continue; }
        if (strcmp(key, "y") == 0) { node->rect.y = parse_scalar_number(val, node->rect.y); node->has_y = 1; continue; }
        if (strcmp(key, "w") == 0) { node->rect.w = parse_scalar_number(val, node->rect.w); node->has_w = 1; continue; }
        if (strcmp(key, "h") == 0) { node->rect.h = parse_scalar_number(val, node->rect.h); node->has_h = 1; continue; }
        if (strcmp(key, "z") == 0) { node->z_index = (int)parse_scalar_number(val, (float)node->z_index); node->has_z_index = 1; continue; }
        if (strcmp(key, "zGroup") == 0 || strcmp(key, "z_group") == 0) { node->z_group = (int)parse_scalar_number(val, (float)node->z_group); node->has_z_group = 1; continue; }
        if (strcmp(key, "id") == 0 && sval) { node->id = platform_strdup(sval); continue; }
        if (strcmp(key, "use") == 0 && sval) { node->use = platform_strdup(sval); continue; }
        if (strcmp(key, "text") == 0 && sval) { node->text = platform_strdup(sval); continue; }
        if (strcmp(key, "textBinding") == 0 && sval) { node->text_binding = platform_strdup(sval); continue; }
        if (strcmp(key, "valueBinding") == 0 && sval) { node->value_binding = platform_strdup(sval); continue; }
        if (strcmp(key, "onClick") == 0 && sval) { node->click_binding = platform_strdup(sval); continue; }
        if (strcmp(key, "clickValue") == 0 && sval) { node->click_value = platform_strdup(sval); continue; }
        if (strcmp(key, "min") == 0) { node->minv = parse_scalar_number(val, node->minv); node->has_min = 1; continue; }
        if (strcmp(key, "max") == 0) { node->maxv = parse_scalar_number(val, node->maxv); node->has_max = 1; continue; }
        if (strcmp(key, "value") == 0) { node->value = parse_scalar_number(val, node->value); node->has_value = 1; continue; }
        if (strcmp(key, "minWidth") == 0) { node->min_w = parse_scalar_number(val, node->min_w); node->has_min_w = 1; continue; }
        if (strcmp(key, "minHeight") == 0) { node->min_h = parse_scalar_number(val, node->min_h); node->has_min_h = 1; continue; }
        if (strcmp(key, "maxWidth") == 0) { node->max_w = parse_scalar_number(val, node->max_w); node->has_max_w = 1; continue; }
        if (strcmp(key, "maxHeight") == 0) { node->max_h = parse_scalar_number(val, node->max_h); node->has_max_h = 1; continue; }
        if (strcmp(key, "scrollArea") == 0 && sval) { node->scroll_area = platform_strdup(sval); continue; }
        if (strcmp(key, "scrollStatic") == 0) {
            fprintf(stderr, "Warning: 'scrollStatic' is deprecated; wrap the node in a 'scrollbar' container instead.\n");
            node->scroll_static = parse_scalar_bool(val, node->scroll_static);
            continue;
        }
        if (strcmp(key, "scrollbar") == 0) {
            fprintf(stderr, "Warning: 'scrollbar' boolean is deprecated; use a 'scrollbar' wrapper node instead.\n");
            node->scrollbar_enabled = parse_scalar_bool(val, node->scrollbar_enabled);
            continue;
        }
        if (strcmp(key, "scrollbarWidth") == 0) { node->scrollbar_width = parse_scalar_number(val, node->scrollbar_width); node->has_scrollbar_width = 1; continue; }
        if (strcmp(key, "spacing") == 0) { node->spacing = parse_scalar_number(val, node->spacing); node->has_spacing = 1; continue; }
        if (strcmp(key, "columns") == 0) { node->columns = (int)parse_scalar_number(val, (float)node->columns); node->has_columns = 1; continue; }
        if (strcmp(key, "clipToViewport") == 0) { node->clip_to_viewport = parse_scalar_bool(val, node->clip_to_viewport); node->has_clip_to_viewport = 1; continue; }
        if (strcmp(key, "padding") == 0) { node->padding_override = parse_scalar_number(val, node->padding_override); node->has_padding_override = 1; continue; }
        if (strcmp(key, "borderThickness") == 0) { node->border_thickness = parse_scalar_number(val, node->border_thickness); node->has_border_thickness = 1; continue; }
        if (strcmp(key, "color") == 0) { read_color_node(&node->color, val); node->has_color = 1; continue; }
        if (strcmp(key, "borderColor") == 0) { read_color_node(&node->border_color, val); node->has_border_color = 1; continue; }
        if (strcmp(key, "textColor") == 0) { read_color_node(&node->text_color, val); node->has_text_color = 1; continue; }
        if (strcmp(key, "scrollbarTrackColor") == 0) { read_color_node(&node->scrollbar_track_color, val); node->has_scrollbar_track_color = 1; continue; }
        if (strcmp(key, "scrollbarThumbColor") == 0) { read_color_node(&node->scrollbar_thumb_color, val); node->has_scrollbar_thumb_color = 1; continue; }
        if (strcmp(key, "docking") == 0 && sval) { node->docking = platform_strdup(sval); continue; }
        if (strcmp(key, "resizable") == 0) { node->resizable = parse_scalar_bool(val, node->resizable); node->has_resizable = 1; continue; }
        if (strcmp(key, "draggable") == 0) { node->draggable = parse_scalar_bool(val, node->draggable); node->has_draggable = 1; continue; }
        if (strcmp(key, "modal") == 0) { node->modal = parse_scalar_bool(val, node->modal); node->has_modal = 1; continue; }
        if (strcmp(key, "onFocus") == 0 && sval) { node->on_focus = platform_strdup(sval); continue; }
        if (strcmp(key, "floating") == 0 && val->type == CONFIG_NODE_MAP) {
            const ConfigNode* fx = config_node_get_scalar(val, "x"); if (fx) { node->floating_rect.x = parse_scalar_number(fx, node->floating_rect.x); node->has_floating_rect = 1; }
            const ConfigNode* fy = config_node_get_scalar(val, "y"); if (fy) { node->floating_rect.y = parse_scalar_number(fy, node->floating_rect.y); node->has_floating_rect = 1; }
            const ConfigNode* fw = config_node_get_scalar(val, "w"); if (fw) { node->floating_rect.w = parse_scalar_number(fw, node->floating_rect.w); node->has_floating_rect = 1; }
            const ConfigNode* fh = config_node_get_scalar(val, "h"); if (fh) { node->floating_rect.h = parse_scalar_number(fh, node->floating_rect.h); node->has_floating_rect = 1; }
            const ConfigNode* fminw = config_node_get_scalar(val, "minWidth"); if (fminw) { node->floating_min_w = parse_scalar_number(fminw, node->floating_min_w); node->has_floating_min = 1; }
            const ConfigNode* fminh = config_node_get_scalar(val, "minHeight"); if (fminh) { node->floating_min_h = parse_scalar_number(fminh, node->floating_min_h); node->has_floating_min = 1; }
            const ConfigNode* fmaxw = config_node_get_scalar(val, "maxWidth"); if (fmaxw) { node->floating_max_w = parse_scalar_number(fmaxw, node->floating_max_w); node->has_floating_max = 1; }
            const ConfigNode* fmaxh = config_node_get_scalar(val, "maxHeight"); if (fmaxh) { node->floating_max_h = parse_scalar_number(fmaxh, node->floating_max_h); node->has_floating_max = 1; }
            continue;
        }
        if (strcmp(key, "children") == 0 && val->type == CONFIG_NODE_SEQUENCE) {
            for (size_t c = 0; c < val->item_count; ++c) {
                UiNode* child = parse_ui_node_config(val->items[c]);
                append_child(node, child);
            }
            continue;
        }
        fprintf(stderr, "Error: unknown layout field '%s'\n", key);
    }
    return node;
}

static void free_prototypes(Prototype* list) {
    while (list) {
        Prototype* next = list->next;
        free(list->name);
        free_ui_tree(list->node);
        free(list);
        list = next;
    }
}

static const Prototype* find_prototype(const Prototype* list, const char* name) {
    for (const Prototype* p = list; p; p = p->next) {
        if (strcmp(p->name, name) == 0) return p;
    }
    return NULL;
}

static UiNode* clone_node(const UiNode* src);

static void merge_node(UiNode* node, const UiNode* proto) {
    if (!node || !proto) return;
    if (!node->type && proto->type) node->type = platform_strdup(proto->type);
    if (!node->style_name && proto->style_name) node->style_name = platform_strdup(proto->style_name);
    if (!node->use && proto->use) node->use = platform_strdup(proto->use);
    if (node->layout == UI_LAYOUT_NONE && proto->layout != UI_LAYOUT_NONE) node->layout = proto->layout;
    if (node->widget_type == W_PANEL && proto->widget_type != W_PANEL && proto->type) node->widget_type = proto->widget_type;
    if (!node->has_x && proto->has_x) { node->rect.x = proto->rect.x; node->has_x = 1; }
    if (!node->has_y && proto->has_y) { node->rect.y = proto->rect.y; node->has_y = 1; }
    if (!node->has_w && proto->has_w) { node->rect.w = proto->rect.w; node->has_w = 1; }
    if (!node->has_h && proto->has_h) { node->rect.h = proto->rect.h; node->has_h = 1; }
    if (!node->has_z_index && proto->has_z_index) { node->z_index = proto->z_index; node->has_z_index = 1; }
    if (!node->has_z_group && proto->has_z_group) { node->z_group = proto->z_group; node->has_z_group = 1; }
    if (!node->has_spacing && proto->has_spacing) { node->spacing = proto->spacing; node->has_spacing = 1; }
    if (!node->has_columns && proto->has_columns) { node->columns = proto->columns; node->has_columns = 1; }
    if (node->style == ui_default_style() && proto->style) node->style = proto->style;
    if (!node->has_padding_override && proto->has_padding_override) { node->padding_override = proto->padding_override; node->has_padding_override = 1; }
    if (!node->has_border_thickness && proto->has_border_thickness) { node->border_thickness = proto->border_thickness; node->has_border_thickness = 1; }
    if (!node->has_border_color && proto->has_border_color) { node->border_color = proto->border_color; node->has_border_color = 1; }
    if (!node->has_color && proto->has_color) { node->color = proto->color; node->has_color = 1; }
    if (!node->has_text_color && proto->has_text_color) { node->text_color = proto->text_color; node->has_text_color = 1; }
    if (!node->has_scrollbar_width && proto->has_scrollbar_width) { node->scrollbar_width = proto->scrollbar_width; node->has_scrollbar_width = 1; }
    if (!node->has_scrollbar_track_color && proto->has_scrollbar_track_color) { node->scrollbar_track_color = proto->scrollbar_track_color; node->has_scrollbar_track_color = 1; }
    if (!node->has_scrollbar_thumb_color && proto->has_scrollbar_thumb_color) { node->scrollbar_thumb_color = proto->scrollbar_thumb_color; node->has_scrollbar_thumb_color = 1; }
    if (!node->has_clip_to_viewport && proto->has_clip_to_viewport) { node->clip_to_viewport = proto->clip_to_viewport; node->has_clip_to_viewport = 1; }
    if (!proto->scrollbar_enabled) node->scrollbar_enabled = 0;
    if (!node->id && proto->id) node->id = platform_strdup(proto->id);
    if (!node->text && proto->text) node->text = platform_strdup(proto->text);
    if (!node->text_binding && proto->text_binding) node->text_binding = platform_strdup(proto->text_binding);
    if (!node->value_binding && proto->value_binding) node->value_binding = platform_strdup(proto->value_binding);
    if (!node->click_binding && proto->click_binding) node->click_binding = platform_strdup(proto->click_binding);
    if (!node->click_value && proto->click_value) node->click_value = platform_strdup(proto->click_value);
    if (!node->has_min && proto->has_min) { node->minv = proto->minv; node->has_min = 1; }
    if (!node->has_max && proto->has_max) { node->maxv = proto->maxv; node->has_max = 1; }
    if (!node->has_value && proto->has_value) { node->value = proto->value; node->has_value = 1; }
    if (!node->has_min_w && proto->has_min_w) { node->min_w = proto->min_w; node->has_min_w = 1; }
    if (!node->has_min_h && proto->has_min_h) { node->min_h = proto->min_h; node->has_min_h = 1; }
    if (!node->has_max_w && proto->has_max_w) { node->max_w = proto->max_w; node->has_max_w = 1; }
    if (!node->has_max_h && proto->has_max_h) { node->max_h = proto->max_h; node->has_max_h = 1; }
    if (!node->has_floating_rect && proto->has_floating_rect) { node->floating_rect = proto->floating_rect; node->has_floating_rect = 1; }
    if (!node->has_floating_min && proto->has_floating_min) {
        node->floating_min_w = proto->floating_min_w;
        node->floating_min_h = proto->floating_min_h;
        node->has_floating_min = 1;
    }
    if (!node->has_floating_max && proto->has_floating_max) {
        node->floating_max_w = proto->floating_max_w;
        node->floating_max_h = proto->floating_max_h;
        node->has_floating_max = 1;
    }
    if (!node->scroll_area && proto->scroll_area) node->scroll_area = platform_strdup(proto->scroll_area);
    if (!node->scroll_static && proto->scroll_static) node->scroll_static = 1;
    if (!node->docking && proto->docking) node->docking = platform_strdup(proto->docking);
    if (!node->has_resizable && proto->has_resizable) { node->resizable = proto->resizable; node->has_resizable = 1; }
    if (!node->has_draggable && proto->has_draggable) { node->draggable = proto->draggable; node->has_draggable = 1; }
    if (!node->has_modal && proto->has_modal) { node->modal = proto->modal; node->has_modal = 1; }
    if (!node->on_focus && proto->on_focus) node->on_focus = platform_strdup(proto->on_focus);

    if (node->child_count == 0 && proto->child_count > 0) {
        node->children = (UiNode*)calloc(proto->child_count, sizeof(UiNode));
        node->child_count = proto->child_count;
        for (size_t i = 0; i < proto->child_count; i++) {
            UiNode* c = clone_node(&proto->children[i]);
            if (c) { node->children[i] = *c; free(c); }
        }
    }
}

static UiNode* clone_node(const UiNode* src) {
    if (!src) return NULL;
    UiNode* n = create_node();
    if (!n) return NULL;
    n->type = src->type ? platform_strdup(src->type) : NULL;
    n->layout = src->layout;
    n->widget_type = src->widget_type;
    n->rect = src->rect;
    n->has_x = src->has_x; n->has_y = src->has_y; n->has_w = src->has_w; n->has_h = src->has_h;
    n->z_index = src->z_index; n->has_z_index = src->has_z_index;
    n->z_group = src->z_group; n->has_z_group = src->has_z_group;
    n->spacing = src->spacing; n->has_spacing = src->has_spacing;
    n->columns = src->columns; n->has_columns = src->has_columns;
    n->style = src->style;
    n->padding_override = src->padding_override; n->has_padding_override = src->has_padding_override;
    n->color = src->color; n->text_color = src->text_color;
    n->has_color = src->has_color; n->has_text_color = src->has_text_color;
    n->scrollbar_enabled = src->scrollbar_enabled;
    n->scrollbar_width = src->scrollbar_width;
    n->has_scrollbar_width = src->has_scrollbar_width;
    n->scrollbar_track_color = src->scrollbar_track_color;
    n->scrollbar_thumb_color = src->scrollbar_thumb_color;
    n->has_scrollbar_track_color = src->has_scrollbar_track_color;
    n->has_scrollbar_thumb_color = src->has_scrollbar_thumb_color;
    n->clip_to_viewport = src->clip_to_viewport;
    n->has_clip_to_viewport = src->has_clip_to_viewport;
    n->style_name = src->style_name ? platform_strdup(src->style_name) : NULL;
    n->use = src->use ? platform_strdup(src->use) : NULL;
    n->id = src->id ? platform_strdup(src->id) : NULL;
    n->text = src->text ? platform_strdup(src->text) : NULL;
    n->text_binding = src->text_binding ? platform_strdup(src->text_binding) : NULL;
    n->value_binding = src->value_binding ? platform_strdup(src->value_binding) : NULL;
    n->click_binding = src->click_binding ? platform_strdup(src->click_binding) : NULL;
    n->click_value = src->click_value ? platform_strdup(src->click_value) : NULL;
    n->minv = src->minv; n->maxv = src->maxv; n->value = src->value;
    n->has_min = src->has_min; n->has_max = src->has_max; n->has_value = src->has_value;
    n->min_w = src->min_w; n->min_h = src->min_h;
    n->has_min_w = src->has_min_w; n->has_min_h = src->has_min_h;
    n->max_w = src->max_w; n->max_h = src->max_h;
    n->has_max_w = src->has_max_w; n->has_max_h = src->has_max_h;
    n->floating_rect = src->floating_rect; n->has_floating_rect = src->has_floating_rect;
    n->floating_min_w = src->floating_min_w; n->floating_min_h = src->floating_min_h;
    n->floating_max_w = src->floating_max_w; n->floating_max_h = src->floating_max_h;
    n->has_floating_min = src->has_floating_min; n->has_floating_max = src->has_floating_max;
    n->scroll_area = src->scroll_area ? platform_strdup(src->scroll_area) : NULL;
    n->scroll_static = src->scroll_static;
    n->docking = src->docking ? platform_strdup(src->docking) : NULL;
    n->resizable = src->resizable;
    n->has_resizable = src->has_resizable;
    n->draggable = src->draggable;
    n->has_draggable = src->has_draggable;
    n->modal = src->modal;
    n->has_modal = src->has_modal;
    n->on_focus = src->on_focus ? platform_strdup(src->on_focus) : NULL;

    if (src->child_count > 0) {
        n->children = (UiNode*)calloc(src->child_count, sizeof(UiNode));
        n->child_count = src->child_count;
        for (size_t i = 0; i < src->child_count; i++) {
            UiNode* c = clone_node(&src->children[i]);
            if (c) { n->children[i] = *c; free(c); }
        }
    }
    return n;
}

static LayoutType type_to_layout(const char* type) {
    if (!type) return UI_LAYOUT_NONE;
    if (strcmp(type, "row") == 0) return UI_LAYOUT_ROW;
    if (strcmp(type, "column") == 0) return UI_LAYOUT_COLUMN;
    if (strcmp(type, "table") == 0) return UI_LAYOUT_TABLE;
    return UI_LAYOUT_NONE;
}

static WidgetType type_to_widget_type(const char* type) {
    if (!type) return W_PANEL;
    if (strcmp(type, "label") == 0) return W_LABEL;
    if (strcmp(type, "button") == 0) return W_BUTTON;
    if (strcmp(type, "hslider") == 0) return W_HSLIDER;
    if (strcmp(type, "rect") == 0) return W_RECT;
    if (strcmp(type, "spacer") == 0) return W_SPACER;
    if (strcmp(type, "checkbox") == 0) return W_CHECKBOX;
    if (strcmp(type, "progress") == 0) return W_PROGRESS;
    if (strcmp(type, "scrollbar") == 0) return W_SCROLLBAR;
    return W_PANEL;
}

static void apply_prototypes(UiNode* node, const Prototype* prototypes) {
    if (!node) return;
    if (node->use) {
        const Prototype* proto = find_prototype(prototypes, node->use);
        if (proto && proto->node) {
            merge_node(node, proto->node);
        }
    }
    for (size_t i = 0; i < node->child_count; i++) {
        apply_prototypes(&node->children[i], prototypes);
    }
}

static void resolve_styles_and_defaults(UiNode* node, const Style* styles, int* missing_styles) {
    if (!node) return;
    LayoutType inferred = type_to_layout(node->type);
    if (inferred != UI_LAYOUT_NONE || node->layout == UI_LAYOUT_NONE) {
        node->layout = inferred;
    }
    node->widget_type = type_to_widget_type(node->type);
    if (!node->has_spacing) {
        node->spacing = (node->layout == UI_LAYOUT_NONE) ? 0.0f : 8.0f;
        node->has_spacing = 1;
    }
    if (!node->has_columns) node->columns = 0;

    const Style* st = node->style ? node->style : ui_default_style();
    if (node->style_name) {
        const Style* found = style_find(styles, node->style_name);
        if (found) {
            st = found;
        } else if (missing_styles) {
            fprintf(stderr, "Error: style '%s' referenced but not defined in UI config\n", node->style_name);
            *missing_styles = 1;
        }
    }
    node->style = st;
    if (!node->has_color) node->color = st->background;
    if (!node->has_text_color) node->text_color = st->text;
    if (!node->has_border_color) node->border_color = st->border_color;
    if (!node->has_border_thickness) node->border_thickness = st->border_thickness;
    if (!node->has_scrollbar_width && st->has_scrollbar_width) { node->scrollbar_width = st->scrollbar_width; node->has_scrollbar_width = 1; }
    if (!node->has_scrollbar_track_color) node->scrollbar_track_color = st->scrollbar_track_color;
    if (!node->has_scrollbar_thumb_color) node->scrollbar_thumb_color = st->scrollbar_thumb_color;

    if (!node->has_min) node->minv = 0.0f;
    if (!node->has_max) node->maxv = 1.0f;
    if (!node->has_value) node->value = 0.0f;

    for (size_t i = 0; i < node->child_count; i++) {
        resolve_styles_and_defaults(&node->children[i], styles, missing_styles);
    }
}

static char* next_scroll_area_name(int* counter) {
    if (!counter) return NULL;
    char buf[32];
    snprintf(buf, sizeof(buf), "scrollArea%d", *counter);
    *counter += 1;
    return platform_strdup(buf);
}

static void wrap_node_with_scrollbar(UiNode* node, int* counter) {
    if (!node) return;
    UiNode content = *node;
    char* original_scroll_area = content.scroll_area;
    UiNode* defaults = create_node();
    if (!defaults) return;
    *node = *defaults;
    free(defaults);

    node->type = platform_strdup("scrollbar");
    node->widget_type = W_SCROLLBAR;
    node->layout = UI_LAYOUT_NONE;
    node->style = content.style;
    node->style_name = content.style_name ? platform_strdup(content.style_name) : NULL;
    node->rect = content.rect;
    node->has_x = content.has_x; node->has_y = content.has_y;
    node->has_w = content.has_w; node->has_h = content.has_h;
    node->has_min_w = content.has_min_w; node->min_w = content.min_w;
    node->has_min_h = content.has_min_h; node->min_h = content.min_h;
    node->has_max_w = content.has_max_w; node->max_w = content.max_w;
    node->has_max_h = content.has_max_h; node->max_h = content.max_h;
    node->has_z_index = content.has_z_index; node->z_index = content.z_index;
    node->has_z_group = content.has_z_group; node->z_group = content.z_group;
    node->padding_override = content.padding_override; node->has_padding_override = content.has_padding_override;
    node->border_thickness = content.border_thickness; node->has_border_thickness = content.has_border_thickness;
    node->border_color = content.border_color; node->has_border_color = content.has_border_color;
    node->color = content.color; node->has_color = content.has_color;
    node->text_color = content.text_color; node->has_text_color = content.has_text_color;
    node->clip_to_viewport = content.clip_to_viewport;
    node->has_clip_to_viewport = content.has_clip_to_viewport;
    node->scrollbar_enabled = content.scrollbar_enabled;
    node->scrollbar_width = content.scrollbar_width; node->has_scrollbar_width = content.has_scrollbar_width;
    node->scrollbar_track_color = content.scrollbar_track_color; node->has_scrollbar_track_color = content.has_scrollbar_track_color;
    node->scrollbar_thumb_color = content.scrollbar_thumb_color; node->has_scrollbar_thumb_color = content.has_scrollbar_thumb_color;
    node->children = (UiNode*)calloc(1, sizeof(UiNode));
    node->child_count = 1;
    content.scroll_static = 0;
    content.scrollbar_enabled = 0;
    node->children[0] = content;

    if (!node->scroll_area) {
        node->scroll_area = original_scroll_area ? platform_strdup(original_scroll_area) : next_scroll_area_name(counter);
    }
    node->scroll_static = 1;
    node->scrollbar_enabled = 1;
    free(node->children[0].scroll_area);
    node->children[0].scroll_area = node->scroll_area ? platform_strdup(node->scroll_area) : NULL;
    free(original_scroll_area);
}

static void normalize_scrollbars(UiNode* node, int* counter) {
    if (!node || !counter) return;

    for (size_t i = 0; i < node->child_count; ++i) {
        normalize_scrollbars(&node->children[i], counter);
    }

    int legacy_scroll = node->scroll_static || node->scroll_area || node->has_scrollbar_width || node->has_scrollbar_track_color || node->has_scrollbar_thumb_color;
    if (node->widget_type != W_SCROLLBAR && (legacy_scroll || node->scrollbar_enabled)) {
        wrap_node_with_scrollbar(node, counter);
        return;
    }

    if (node->widget_type == W_SCROLLBAR) {
        node->scroll_static = 1;
        node->scrollbar_enabled = 1;
        if (!node->scroll_area) node->scroll_area = next_scroll_area_name(counter);
        if (node->child_count > 0) {
            for (size_t i = 0; i < node->child_count; ++i) {
                free(node->children[i].scroll_area);
                node->children[i].scroll_area = node->scroll_area ? platform_strdup(node->scroll_area) : NULL;
                node->children[i].scroll_static = 0;
            }
        } else {
            fprintf(stderr, "Warning: scrollbar node declared without a child; scrolling will be disabled.\n");
        }
    }
}

static void bind_model_values_to_nodes(UiNode* node, const Model* model) {
    if (!node || !model) return;
    if (node->text_binding) {
        const char* v = model_get_string(model, node->text_binding, NULL);
        if (v) {
            free(node->text);
            node->text = platform_strdup(v);
        }
    }
    if (node->value_binding) {
        node->value = model_get_number(model, node->value_binding, node->value);
        node->has_value = 1;
    }
    for (size_t i = 0; i < node->child_count; i++) {
        bind_model_values_to_nodes(&node->children[i], model);
    }
}

void update_widget_bindings(UiNode* root, const Model* model) {
    bind_model_values_to_nodes(root, model);
}

Model* ui_config_load_model(const ConfigDocument* doc) {
    if (!doc || !doc->root) return NULL;

    const ConfigNode* store_node = config_node_get_scalar(doc->root, "store");
    const ConfigNode* key_node = config_node_get_scalar(doc->root, "key");
    const ConfigNode* model_node = config_node_get_map(doc->root, "model");

    if (!model_node) {
        fprintf(stderr, "Error: model section missing in UI config %s\n", doc->source_path ? doc->source_path : "(unknown)");
        return NULL;
    }

    Model* model = (Model*)calloc(1, sizeof(Model));
    if (!model) return NULL;

    char* derived_key = key_node && key_node->scalar ? NULL : basename_no_ext(doc->source_path);
    model->store = platform_strdup(store_node && store_node->scalar ? store_node->scalar : "layout");
    model->key = platform_strdup(key_node && key_node->scalar ? key_node->scalar : (derived_key ? derived_key : "default"));
    model->source_path = platform_strdup(doc->source_path ? doc->source_path : "model.yaml");

    if (!model->store || !model->key || !model->source_path) { free_model(model); return NULL; }

    for (size_t i = 0; i < model_node->pair_count; ++i) {
        const ConfigPair* pair = &model_node->pairs[i];
        if (!pair->key || !pair->value) continue;
        const ConfigNode* val = pair->value;
        if (val->type != CONFIG_NODE_SCALAR) continue;
        if (val->scalar_type == CONFIG_SCALAR_STRING) {
            model_set_string(model, pair->key, val->scalar ? val->scalar : "");
        } else {
            model_set_number(model, pair->key, parse_scalar_number(val, 0.0f));
        }
    }

    model->source_doc = doc;
    free(derived_key);
    return model;
}

Style* ui_config_load_styles(const ConfigNode* root) {
    const ConfigNode* styles_node = config_node_get_map(root, "styles");
    if (!styles_node || styles_node->type != CONFIG_NODE_MAP) {
        fprintf(stderr, "Error: styles section missing in UI config\n");
        return NULL;
    }

    Style* styles = NULL;
    const Style* default_style = ui_default_style();
    for (size_t i = 0; i < styles_node->pair_count; ++i) {
        const ConfigPair* pair = &styles_node->pairs[i];
        const ConfigNode* val = pair->value;
        if (!pair->key || !val || val->type != CONFIG_NODE_MAP) continue;
        Style* st = (Style*)calloc(1, sizeof(Style));
        if (!st) break;
        st->name = platform_strdup(pair->key);
        st->background = default_style->background;
        st->text = default_style->text;
        st->border_color = default_style->border_color;
        st->padding = default_style->padding;
        st->border_thickness = default_style->border_thickness;
        st->scrollbar_track_color = default_style->scrollbar_track_color;
        st->scrollbar_thumb_color = default_style->scrollbar_thumb_color;
        st->scrollbar_width = default_style->scrollbar_width;
        st->has_scrollbar_width = default_style->has_scrollbar_width;
        st->next = styles;
        styles = st;

        for (size_t j = 0; j < val->pair_count; ++j) {
            const ConfigPair* field = &val->pairs[j];
            const char* fname = field->key;
            const ConfigNode* fval = field->value;
            if (!fname || !fval) continue;
            if (strcmp(fname, "color") == 0) { read_color_node(&st->background, fval); continue; }
            if (strcmp(fname, "textColor") == 0) { read_color_node(&st->text, fval); continue; }
            if (strcmp(fname, "borderColor") == 0) { read_color_node(&st->border_color, fval); continue; }
            if (strcmp(fname, "padding") == 0) { st->padding = parse_scalar_number(fval, st->padding); continue; }
            if (strcmp(fname, "borderThickness") == 0) { st->border_thickness = parse_scalar_number(fval, st->border_thickness); continue; }
            if (strcmp(fname, "scrollbarTrackColor") == 0) { read_color_node(&st->scrollbar_track_color, fval); continue; }
            if (strcmp(fname, "scrollbarThumbColor") == 0) { read_color_node(&st->scrollbar_thumb_color, fval); continue; }
            if (strcmp(fname, "scrollbarWidth") == 0) { st->scrollbar_width = parse_scalar_number(fval, st->scrollbar_width); st->has_scrollbar_width = 1; continue; }
            fprintf(stderr, "Error: unknown style field '%s' in style '%s'\n", fname, st->name);
        }
    }

    return styles;
}

UiNode* ui_config_load_layout(const ConfigDocument* doc, const Model* model, const Style* styles, const Scene* scene) {
    if (!doc || !doc->root) return NULL;

    const ConfigNode* root = doc->root;
    const ConfigNode* layout_node = config_node_get_map(root, "layout");
    const ConfigNode* floating_node = config_node_get_sequence(root, "floating");

    if (!layout_node && !floating_node) {
        fprintf(stderr, "Error: layout or floating section missing in UI config\n");
        return NULL;
    }

    Prototype* prototypes = NULL;
    gather_component_prototypes(doc, &prototypes);

    UiNode* root_node = create_node();
    if (!root_node) { free_prototypes(prototypes); return NULL; }
    root_node->layout = UI_LAYOUT_ABSOLUTE;
    root_node->style = ui_root_style();
    root_node->spacing = 0.0f;

    int sections_found = 0;
    if (layout_node) {
        UiNode* def = parse_ui_node_config(layout_node);
        append_child(root_node, def);
        sections_found++;
    }
    if (floating_node && floating_node->type == CONFIG_NODE_SEQUENCE) {
        for (size_t i = 0; i < floating_node->item_count; ++i) {
            UiNode* def = parse_ui_node_config(floating_node->items[i]);
            append_child(root_node, def);
        }
        sections_found++;
    }

    if (sections_found == 0) fprintf(stderr, "Error: no 'layout' or 'floating' sections found in layout config\n");

    if (scene) {
        scene_ui_inject(root_node, scene);
    }

    apply_prototypes(root_node, prototypes);
    int missing_styles = 0;
    resolve_styles_and_defaults(root_node, styles, &missing_styles);
    if (missing_styles) {
        fprintf(stderr, "Failed to resolve styles: remove or define the missing styles to continue.\n");
        free_prototypes(prototypes);
        free_ui_tree(root_node);
        return NULL;
    }
    bind_model_values_to_nodes(root_node, model);
    int scroll_counter = 0;
    normalize_scrollbars(root_node, &scroll_counter);
    free_prototypes(prototypes);
    return root_node;
}

static void free_ui_node(UiNode* node) {
    if (!node) return;
    for (size_t i = 0; i < node->child_count; i++) {
        free_ui_node(&node->children[i]);
    }
    free(node->children);
    free(node->type);
    free(node->style_name);
    free(node->use);
    free(node->id);
    free(node->text);
    free(node->text_binding);
    free(node->value_binding);
    free(node->click_binding);
    free(node->click_value);
    free(node->scroll_area);
    free(node->docking);
    free(node->on_focus);
}

void free_ui_tree(UiNode* node) {
    if (!node) return;
    free_ui_node(node);
    free(node);
}
