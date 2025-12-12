#include "ui/ui_config.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stb_truetype.h"
#include "ui/scene_ui.h"

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

/* Default style mirrors the vivid palette defined in assets/ui/config/ui.yaml to avoid a grayscale fallback. */
static const Style DEFAULT_STYLE = {
    .name = NULL,
    .background = {0.12f, 0.16f, 0.24f, 0.96f},
    .text = {0.94f, 0.97f, 1.0f, 1.0f},
    .border_color = {0.33f, 0.56f, 0.88f, 1.0f},
    .scrollbar_track_color = {0.16f, 0.25f, 0.36f, 0.9f},
    .scrollbar_thumb_color = {0.58f, 0.82f, 1.0f, 1.0f},
    .padding = 10.0f,
    .border_thickness = 2.0f,
    .scrollbar_width = 10.0f,
    .has_scrollbar_width = 1,
    .next = NULL
};
static const Style ROOT_STYLE = { .name = NULL, .background = {0.0f, 0.0f, 0.0f, 0.0f}, .text = {1.0f, 1.0f, 1.0f, 1.0f}, .border_color = {1.0f, 1.0f, 1.0f, 0.0f}, .scrollbar_track_color = {0.6f, 0.6f, 0.6f, 0.4f}, .scrollbar_thumb_color = {1.0f, 1.0f, 1.0f, 0.7f}, .padding = 0.0f, .border_thickness = 0.0f, .scrollbar_width = 0.0f, .has_scrollbar_width = 0, .next = NULL };

static unsigned char* g_font_buffer = NULL;
static stbtt_fontinfo g_font_info;
static float g_font_scale = 0.0f;
static int g_font_ascent = 0;
static int g_font_descent = 0;
static int g_font_ready = 0;

static float fallback_line_height(void) {
    float line = (float)(g_font_ascent - g_font_descent);
    return line > 0.0f ? line : 18.0f;
}

static const char* scalar_text(const ConfigNode* node) {
    if (!node || node->type != CONFIG_NODE_SCALAR) return NULL;
    return node->scalar;
}

static int ensure_font_metrics(const char* font_path) {
    if (g_font_ready) return 1;
    if (!font_path) return 0;

    FILE* f = fopen(font_path, "rb");
    if (!f) {
        fprintf(stderr, "Warning: unable to open font at %s\n", font_path);
        return 0;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return 0; }

    g_font_buffer = (unsigned char*)malloc((size_t)sz);
    if (!g_font_buffer) { fclose(f); return 0; }
    fread(g_font_buffer, 1, (size_t)sz, f);
    fclose(f);

    if (!stbtt_InitFont(&g_font_info, g_font_buffer, 0)) {
        fprintf(stderr, "Warning: failed to init font metrics\n");
        free(g_font_buffer); g_font_buffer = NULL;
        return 0;
    }

    g_font_scale = stbtt_ScaleForPixelHeight(&g_font_info, 32.0f);
    int ascent = 0, descent = 0, gap = 0;
    stbtt_GetFontVMetrics(&g_font_info, &ascent, &descent, &gap);
    g_font_ascent = (int)roundf((float)ascent * g_font_scale);
    g_font_descent = (int)roundf((float)descent * g_font_scale);
    g_font_ready = 1;
    return 1;
}

static int utf8_decode(const char* s, int* out_advance) {
    unsigned char c = (unsigned char)*s;
    if (c < 0x80) { *out_advance = 1; return c; }
    if ((c >> 5) == 0x6) { *out_advance = 2; return ((int)(c & 0x1F) << 6) | ((int)(s[1] & 0x3F)); }
    if ((c >> 4) == 0xE) { *out_advance = 3; return ((int)(c & 0x0F) << 12) | (((int)s[1] & 0x3F) << 6) | ((int)(s[2] & 0x3F)); }
    if ((c >> 3) == 0x1E) { *out_advance = 4; return ((int)(c & 0x07) << 18) | (((int)s[1] & 0x3F) << 12) |
                                        (((int)s[2] & 0x3F) << 6) | ((int)(s[3] & 0x3F)); }
    *out_advance = 1;
    return '?';
}

static void measure_text(const char* text, float* out_w, float* out_h) {
    float width = 0.0f;
    float height = fallback_line_height();

    if (g_font_ready && text && *text) {
        int prev = 0;
        for (const char* c = text; *c; ) {
            int adv = 0;
            int ch = utf8_decode(c, &adv);
            if (adv <= 0) break;
            int advance = 0, lsb = 0;
            stbtt_GetCodepointHMetrics(&g_font_info, ch, &advance, &lsb);
            width += advance * g_font_scale;
            if (prev) width += stbtt_GetCodepointKernAdvance(&g_font_info, prev, ch) * g_font_scale;
            prev = ch;
            c += adv;
        }
    }

    if (out_w) *out_w = width;
    if (out_h) *out_h = height;
}

typedef struct Prototype {
    char* name;
    UiNode* node;
    struct Prototype* next;
} Prototype;

static ModelEntry* model_find_entry(Model* model, const char* key) {
    for (ModelEntry* e = model ? model->entries : NULL; e; e = e->next) {
        if (strcmp(e->key, key) == 0) return e;
    }
    return NULL;
}

static ModelEntry* model_get_or_create(Model* model, const char* key) {
    ModelEntry* e = model_find_entry(model, key);
    if (e) return e;
    e = (ModelEntry*)calloc(1, sizeof(ModelEntry));
    if (!e) return NULL;
    e->key = strdup(key);
    e->string_value = NULL;
    e->number_value = 0.0f;
    e->is_string = 0;
    e->next = model->entries;
    model->entries = e;
    return e;
}

float model_get_number(const Model* model, const char* key, float fallback) {
    for (const ModelEntry* e = model ? model->entries : NULL; e; e = e->next) {
        if (strcmp(e->key, key) == 0 && !e->is_string) return e->number_value;
    }
    return fallback;
}

const char* model_get_string(const Model* model, const char* key, const char* fallback) {
    for (const ModelEntry* e = model ? model->entries : NULL; e; e = e->next) {
        if (strcmp(e->key, key) == 0 && e->is_string) return e->string_value;
    }
    return fallback;
}

void model_set_number(Model* model, const char* key, float value) {
    if (!model || !key) return;
    ModelEntry* e = model_get_or_create(model, key);
    if (!e) return;
    e->number_value = value;
    e->is_string = 0;
}

void model_set_string(Model* model, const char* key, const char* value) {
    if (!model || !key || !value) return;
    ModelEntry* e = model_get_or_create(model, key);
    if (!e) return;
    free(e->string_value);
    e->string_value = strdup(value);
    e->is_string = 1;
}

static ConfigNode* config_node_new_map(void) {
    ConfigNode* n = (ConfigNode*)calloc(1, sizeof(ConfigNode));
    if (!n) return NULL;
    n->type = CONFIG_NODE_MAP;
    n->scalar_type = CONFIG_SCALAR_STRING;
    return n;
}

static ConfigNode* config_node_from_string(const char* value, ConfigScalarType type) {
    ConfigNode* n = (ConfigNode*)calloc(1, sizeof(ConfigNode));
    if (!n) return NULL;
    n->type = CONFIG_NODE_SCALAR;
    n->scalar_type = type;
    if (value) n->scalar = strdup(value);
    return n;
}

static int config_map_reserve(ConfigNode* map, size_t desired) {
    if (!map || map->type != CONFIG_NODE_MAP) return 0;
    if (desired <= map->pair_capacity) return 1;
    size_t new_cap = map->pair_capacity == 0 ? 4 : map->pair_capacity;
    while (new_cap < desired) new_cap *= 2;
    ConfigPair* resized = (ConfigPair*)realloc(map->pairs, new_cap * sizeof(ConfigPair));
    if (!resized) return 0;
    map->pairs = resized;
    map->pair_capacity = new_cap;
    return 1;
}

static int config_map_set(ConfigNode* map, const char* key, ConfigNode* value) {
    if (!map || map->type != CONFIG_NODE_MAP || !key) return 0;
    for (size_t i = 0; i < map->pair_count; ++i) {
        if (map->pairs[i].key && strcmp(map->pairs[i].key, key) == 0) {
            config_node_free(map->pairs[i].value);
            map->pairs[i].value = value;
            return 1;
        }
    }
    if (!config_map_reserve(map, map->pair_count + 1)) return 0;
    map->pairs[map->pair_count].key = strdup(key);
    map->pairs[map->pair_count].value = value;
    map->pair_count++;
    return 1;
}

static ConfigNode* config_map_get_mut(ConfigNode* map, const char* key) {
    if (!map || map->type != CONFIG_NODE_MAP || !key) return NULL;
    for (size_t i = 0; i < map->pair_count; ++i) {
        if (map->pairs[i].key && strcmp(map->pairs[i].key, key) == 0) return map->pairs[i].value;
    }
    return NULL;
}

static ConfigNode* ensure_map_entry(ConfigNode* map, const char* key) {
    ConfigNode* existing = config_map_get_mut(map, key);
    if (existing && existing->type == CONFIG_NODE_MAP) return existing;
    ConfigNode* fresh = config_node_new_map();
    if (!fresh) return NULL;
    if (!config_map_set(map, key, fresh)) {
        config_node_free(fresh);
        return NULL;
    }
    return fresh;
}

static void config_map_clear(ConfigNode* map) {
    if (!map || map->type != CONFIG_NODE_MAP) return;
    for (size_t i = 0; i < map->pair_count; ++i) {
        free(map->pairs[i].key);
        config_node_free(map->pairs[i].value);
    }
    free(map->pairs);
    map->pairs = NULL;
    map->pair_count = 0;
    map->pair_capacity = 0;
}

int save_model(const Model* model) {
    if (!model || !model->source_path) return 0;
    FILE* f = fopen(model->source_path, "wb");
    if (!f) return 0;

    const char* store = model->store ? model->store : "model";
    const char* key = model->key ? model->key : "default";

    fprintf(f, "store: %s\n", store);
    fprintf(f, "key: %s\n", key);
    fputs("data:\n  model:\n", f);

    for (ModelEntry* e = model->entries; e; e = e->next) {
        fprintf(f, "    %s: ", e->key);
        if (e->is_string) {
            fprintf(f, "\"%s\"", e->string_value ? e->string_value : "");
        } else {
            fprintf(f, "%g", e->number_value);
        }
        fputc('\n', f);
    }

    fclose(f);
    return 1;
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
    node->style = &DEFAULT_STYLE;
    node->padding_override = 0.0f;
    node->has_padding_override = 0;
    node->border_thickness = 0.0f;
    node->has_border_thickness = 0;
    node->has_border_color = 0;
    node->border_color = DEFAULT_STYLE.border_color;
    node->color = DEFAULT_STYLE.background;
    node->text_color = DEFAULT_STYLE.text;
    node->scrollbar_enabled = 1;
    node->scrollbar_width = 0.0f;
    node->has_scrollbar_width = 0;
    node->scrollbar_track_color = DEFAULT_STYLE.scrollbar_track_color;
    node->scrollbar_thumb_color = DEFAULT_STYLE.scrollbar_thumb_color;
    node->has_scrollbar_track_color = 0;
    node->has_scrollbar_thumb_color = 0;
    node->clip_to_viewport = 1;
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
        if (strcmp(key, "type") == 0 && sval) { node->type = strdup(sval); continue; }
        if (strcmp(key, "style") == 0 && sval) { node->style_name = strdup(sval); continue; }
        if (strcmp(key, "x") == 0) { node->rect.x = parse_scalar_number(val, node->rect.x); node->has_x = 1; continue; }
        if (strcmp(key, "y") == 0) { node->rect.y = parse_scalar_number(val, node->rect.y); node->has_y = 1; continue; }
        if (strcmp(key, "w") == 0) { node->rect.w = parse_scalar_number(val, node->rect.w); node->has_w = 1; continue; }
        if (strcmp(key, "h") == 0) { node->rect.h = parse_scalar_number(val, node->rect.h); node->has_h = 1; continue; }
        if (strcmp(key, "z") == 0) { node->z_index = (int)parse_scalar_number(val, (float)node->z_index); node->has_z_index = 1; continue; }
        if (strcmp(key, "zGroup") == 0 || strcmp(key, "z_group") == 0) { node->z_group = (int)parse_scalar_number(val, (float)node->z_group); node->has_z_group = 1; continue; }
        if (strcmp(key, "id") == 0 && sval) { node->id = strdup(sval); continue; }
        if (strcmp(key, "use") == 0 && sval) { node->use = strdup(sval); continue; }
        if (strcmp(key, "text") == 0 && sval) { node->text = strdup(sval); continue; }
        if (strcmp(key, "textBinding") == 0 && sval) { node->text_binding = strdup(sval); continue; }
        if (strcmp(key, "valueBinding") == 0 && sval) { node->value_binding = strdup(sval); continue; }
        if (strcmp(key, "onClick") == 0 && sval) { node->click_binding = strdup(sval); continue; }
        if (strcmp(key, "clickValue") == 0 && sval) { node->click_value = strdup(sval); continue; }
        if (strcmp(key, "min") == 0) { node->minv = parse_scalar_number(val, node->minv); node->has_min = 1; continue; }
        if (strcmp(key, "max") == 0) { node->maxv = parse_scalar_number(val, node->maxv); node->has_max = 1; continue; }
        if (strcmp(key, "value") == 0) { node->value = parse_scalar_number(val, node->value); node->has_value = 1; continue; }
        if (strcmp(key, "minWidth") == 0) { node->min_w = parse_scalar_number(val, node->min_w); node->has_min_w = 1; continue; }
        if (strcmp(key, "minHeight") == 0) { node->min_h = parse_scalar_number(val, node->min_h); node->has_min_h = 1; continue; }
        if (strcmp(key, "maxWidth") == 0) { node->max_w = parse_scalar_number(val, node->max_w); node->has_max_w = 1; continue; }
        if (strcmp(key, "maxHeight") == 0) { node->max_h = parse_scalar_number(val, node->max_h); node->has_max_h = 1; continue; }
        if (strcmp(key, "scrollArea") == 0 && sval) { node->scroll_area = strdup(sval); continue; }
        if (strcmp(key, "scrollStatic") == 0) { node->scroll_static = parse_scalar_bool(val, node->scroll_static); continue; }
        if (strcmp(key, "scrollbar") == 0) { node->scrollbar_enabled = parse_scalar_bool(val, node->scrollbar_enabled); continue; }
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
        if (strcmp(key, "docking") == 0 && sval) { node->docking = strdup(sval); continue; }
        if (strcmp(key, "resizable") == 0) { node->resizable = parse_scalar_bool(val, node->resizable); node->has_resizable = 1; continue; }
        if (strcmp(key, "draggable") == 0) { node->draggable = parse_scalar_bool(val, node->draggable); node->has_draggable = 1; continue; }
        if (strcmp(key, "modal") == 0) { node->modal = parse_scalar_bool(val, node->modal); node->has_modal = 1; continue; }
        if (strcmp(key, "onFocus") == 0 && sval) { node->on_focus = strdup(sval); continue; }
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
    if (!node->type && proto->type) node->type = strdup(proto->type);
    if (!node->style_name && proto->style_name) node->style_name = strdup(proto->style_name);
    if (!node->use && proto->use) node->use = strdup(proto->use);
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
    if (node->style == &DEFAULT_STYLE && proto->style) node->style = proto->style;
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
    if (!node->id && proto->id) node->id = strdup(proto->id);
    if (!node->text && proto->text) node->text = strdup(proto->text);
    if (!node->text_binding && proto->text_binding) node->text_binding = strdup(proto->text_binding);
    if (!node->value_binding && proto->value_binding) node->value_binding = strdup(proto->value_binding);
    if (!node->click_binding && proto->click_binding) node->click_binding = strdup(proto->click_binding);
    if (!node->click_value && proto->click_value) node->click_value = strdup(proto->click_value);
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
    if (!node->scroll_area && proto->scroll_area) node->scroll_area = strdup(proto->scroll_area);
    if (!node->scroll_static && proto->scroll_static) node->scroll_static = 1;
    if (!node->docking && proto->docking) node->docking = strdup(proto->docking);
    if (!node->has_resizable && proto->has_resizable) { node->resizable = proto->resizable; node->has_resizable = 1; }
    if (!node->has_draggable && proto->has_draggable) { node->draggable = proto->draggable; node->has_draggable = 1; }
    if (!node->has_modal && proto->has_modal) { node->modal = proto->modal; node->has_modal = 1; }
    if (!node->on_focus && proto->on_focus) node->on_focus = strdup(proto->on_focus);

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
    n->type = src->type ? strdup(src->type) : NULL;
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
    n->style_name = src->style_name ? strdup(src->style_name) : NULL;
    n->use = src->use ? strdup(src->use) : NULL;
    n->id = src->id ? strdup(src->id) : NULL;
    n->text = src->text ? strdup(src->text) : NULL;
    n->text_binding = src->text_binding ? strdup(src->text_binding) : NULL;
    n->value_binding = src->value_binding ? strdup(src->value_binding) : NULL;
    n->click_binding = src->click_binding ? strdup(src->click_binding) : NULL;
    n->click_value = src->click_value ? strdup(src->click_value) : NULL;
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
    n->scroll_area = src->scroll_area ? strdup(src->scroll_area) : NULL;
    n->scroll_static = src->scroll_static;
    n->docking = src->docking ? strdup(src->docking) : NULL;
    n->resizable = src->resizable;
    n->has_resizable = src->has_resizable;
    n->draggable = src->draggable;
    n->has_draggable = src->has_draggable;
    n->modal = src->modal;
    n->has_modal = src->has_modal;
    n->on_focus = src->on_focus ? strdup(src->on_focus) : NULL;

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

    const Style* st = node->style ? node->style : &DEFAULT_STYLE;
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

static void auto_assign_scroll_areas(UiNode* node, int* counter, const char* inherited) {
    if (!node || !counter) return;
    const char* active = inherited;
    if (node->scroll_static && !node->scroll_area) {
        char buf[32];
        snprintf(buf, sizeof(buf), "scrollArea%d", *counter);
        *counter += 1;
        node->scroll_area = strdup(buf);
    }
    if (node->scroll_area) active = node->scroll_area;
    for (size_t i = 0; i < node->child_count; i++) {
        auto_assign_scroll_areas(&node->children[i], counter, active);
    }
}

static void bind_model_values_to_nodes(UiNode* node, const Model* model) {
    if (!node || !model) return;
    if (node->text_binding) {
        const char* v = model_get_string(model, node->text_binding, NULL);
        if (v) {
            free(node->text);
            node->text = strdup(v);
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
    const ConfigNode* data_node = config_node_get_map(doc->root, "data");
    const ConfigNode* model_node = data_node ? config_node_get_map(data_node, "model") : config_node_get_map(doc->root, "model");

    if (!model_node) {
        fprintf(stderr, "Error: model section missing in UI config %s\n", doc->source_path ? doc->source_path : "(unknown)");
        return NULL;
    }

    Model* model = (Model*)calloc(1, sizeof(Model));
    if (!model) return NULL;

    model->store = strdup(store_node && store_node->scalar ? store_node->scalar : "model");
    model->key = strdup(key_node && key_node->scalar ? key_node->scalar : "default");
    model->source_path = strdup(doc->source_path ? doc->source_path : "model.yaml");

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
    return model;
}

Style* ui_config_load_styles(const ConfigNode* root) {
    const ConfigNode* data_node = config_node_get_map(root, "data");
    const ConfigNode* styles_node = data_node ? config_node_get_map(data_node, "styles") : config_node_get_map(root, "styles");
    if (!styles_node || styles_node->type != CONFIG_NODE_MAP) {
        fprintf(stderr, "Error: styles section missing in UI config\n");
        return NULL;
    }

    Style* styles = NULL;
    for (size_t i = 0; i < styles_node->pair_count; ++i) {
        const ConfigPair* pair = &styles_node->pairs[i];
        const ConfigNode* val = pair->value;
        if (!pair->key || !val || val->type != CONFIG_NODE_MAP) continue;
        Style* st = (Style*)calloc(1, sizeof(Style));
        if (!st) break;
        st->name = strdup(pair->key);
        st->background = DEFAULT_STYLE.background;
        st->text = DEFAULT_STYLE.text;
        st->border_color = DEFAULT_STYLE.border_color;
        st->padding = DEFAULT_STYLE.padding;
        st->border_thickness = DEFAULT_STYLE.border_thickness;
        st->scrollbar_track_color = DEFAULT_STYLE.scrollbar_track_color;
        st->scrollbar_thumb_color = DEFAULT_STYLE.scrollbar_thumb_color;
        st->scrollbar_width = DEFAULT_STYLE.scrollbar_width;
        st->has_scrollbar_width = DEFAULT_STYLE.has_scrollbar_width;
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

UiNode* ui_config_load_layout(const ConfigNode* root, const Model* model, const Style* styles, const char* font_path, const Scene* scene) {
    if (!root) return NULL;

    const ConfigNode* data_node = config_node_get_map(root, "data");
    const ConfigNode* layout_node = data_node ? config_node_get_map(data_node, "layout") : config_node_get_map(root, "layout");
    const ConfigNode* widgets_node = data_node ? config_node_get_map(data_node, "widgets") : config_node_get_map(root, "widgets");
    const ConfigNode* floating_node = data_node ? config_node_get_sequence(data_node, "floating") : config_node_get_sequence(root, "floating");

    if (!layout_node && !widgets_node) {
        fprintf(stderr, "Error: layout or widgets section missing in UI config\n");
        return NULL;
    }

    ensure_font_metrics(font_path);

    Prototype* prototypes = NULL;
    if (widgets_node) {
        for (size_t i = 0; i < widgets_node->pair_count; ++i) {
            const ConfigPair* pair = &widgets_node->pairs[i];
            if (!pair->key || !pair->value) continue;
            UiNode* def = parse_ui_node_config(pair->value);
            Prototype* pnode = (Prototype*)calloc(1, sizeof(Prototype));
            if (pnode) {
                pnode->name = strdup(pair->key);
                pnode->node = def;
                pnode->next = prototypes;
                prototypes = pnode;
            } else {
                free_ui_tree(def);
            }
        }
    }

    UiNode* root_node = create_node();
    if (!root_node) { free_prototypes(prototypes); return NULL; }
    root_node->layout = UI_LAYOUT_ABSOLUTE;
    root_node->style = &ROOT_STYLE;
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
    auto_assign_scroll_areas(root_node, &scroll_counter, NULL);
    free_prototypes(prototypes);
    return root_node;
}

static LayoutNode* build_layout_tree_recursive(const UiNode* node) {
    if (!node) return NULL;
    LayoutNode* l = (LayoutNode*)calloc(1, sizeof(LayoutNode));
    if (!l) return NULL;
    l->source = node;
    l->child_count = node->child_count;
    if (node->child_count > 0) {
        l->children = (LayoutNode*)calloc(node->child_count, sizeof(LayoutNode));
        if (!l->children) { free(l); return NULL; }
        for (size_t i = 0; i < node->child_count; i++) {
            LayoutNode* child = build_layout_tree_recursive(&node->children[i]);
            if (child) {
                l->children[i] = *child;
                free(child);
            }
        }
    }
    return l;
}

LayoutNode* build_layout_tree(const UiNode* root) {
    return build_layout_tree_recursive(root);
}

static void free_layout_node(LayoutNode* root) {
    if (!root) return;
    for (size_t i = 0; i < root->child_count; i++) {
        free_layout_node(&root->children[i]);
    }
    free(root->children);
}

void free_layout_tree(LayoutNode* root) {
    if (!root) return;
    free_layout_node(root);
    free(root);
}

static void measure_node(LayoutNode* node) {
    if (!node || !node->source) return;
    float padding = node->source->has_padding_override ? node->source->padding_override : (node->source->style ? node->source->style->padding : DEFAULT_STYLE.padding);
    float border = node->source->border_thickness;
    for (size_t i = 0; i < node->child_count; i++) measure_node(&node->children[i]);

    if (node->source->layout == UI_LAYOUT_ROW) {
        float content_w = 0.0f;
        float content_h = 0.0f;
        for (size_t i = 0; i < node->child_count; i++) {
            LayoutNode* ch = &node->children[i];
            content_w += ch->rect.w;
            if (i + 1 < node->child_count) content_w += node->source->spacing;
            if (ch->rect.h > content_h) content_h = ch->rect.h;
        }
        node->rect.w = content_w + padding * 2.0f + border * 2.0f;
        node->rect.h = content_h + padding * 2.0f + border * 2.0f;
        if (node->source->has_max_w && node->rect.w > node->source->max_w) node->rect.w = node->source->max_w;
    } else if (node->source->layout == UI_LAYOUT_COLUMN) {
        float content_w = 0.0f;
        float content_h = 0.0f;
        for (size_t i = 0; i < node->child_count; i++) {
            LayoutNode* ch = &node->children[i];
            if (ch->rect.w > content_w) content_w = ch->rect.w;
            content_h += ch->rect.h;
            if (i + 1 < node->child_count) content_h += node->source->spacing;
        }
        node->rect.w = content_w + padding * 2.0f + border * 2.0f;
        node->rect.h = content_h + padding * 2.0f + border * 2.0f;
        if (node->source->has_max_h && node->rect.h > node->source->max_h) node->rect.h = node->source->max_h;
    } else if (node->source->layout == UI_LAYOUT_TABLE && node->source->columns > 0) {
        int cols = node->source->columns;
        int rows = (int)((node->child_count + (size_t)cols - 1) / (size_t)cols);
        float* col_w = (float*)calloc((size_t)cols, sizeof(float));
        float* row_h = (float*)calloc((size_t)rows, sizeof(float));
        if (col_w && row_h) {
            for (size_t i = 0; i < node->child_count; i++) {
                int col = (int)(i % (size_t)cols);
                int row = (int)(i / (size_t)cols);
                LayoutNode* ch = &node->children[i];
                if (ch->rect.w > col_w[col]) col_w[col] = ch->rect.w;
                if (ch->rect.h > row_h[row]) row_h[row] = ch->rect.h;
            }
            float content_w = 0.0f;
            float content_h = 0.0f;
            for (int c = 0; c < cols; c++) {
                content_w += col_w[c];
                if (c + 1 < cols) content_w += node->source->spacing;
            }
            for (int r = 0; r < rows; r++) {
                content_h += row_h[r];
                if (r + 1 < rows) content_h += node->source->spacing;
            }
            node->rect.w = content_w + padding * 2.0f + border * 2.0f;
            node->rect.h = content_h + padding * 2.0f + border * 2.0f;
        }
        free(col_w);
        free(row_h);
    } else if (node->child_count > 0) { /* absolute container */
        float max_w = 0.0f, max_h = 0.0f;
        for (size_t i = 0; i < node->child_count; i++) {
            LayoutNode* ch = &node->children[i];
            float child_x = ch->rect.x;
            float child_y = ch->rect.y;
            if (ch->source) {
                if (ch->source->has_x) child_x = ch->source->rect.x;
                if (ch->source->has_y) child_y = ch->source->rect.y;
            }

            float right = child_x + ch->rect.w;
            float bottom = child_y + ch->rect.h;
            if (right > max_w) max_w = right;
            if (bottom > max_h) max_h = bottom;
        }
        node->rect.w = max_w + padding * 2.0f + border * 2.0f;
        node->rect.h = max_h + padding * 2.0f + border * 2.0f;
    } else {
        if (node->source->widget_type == W_SPACER) {
            node->rect.w = node->source->has_w ? node->source->rect.w : 0.0f;
            node->rect.h = node->source->has_h ? node->source->rect.h : 0.0f;
        } else {
            float text_w = 0.0f, text_h = fallback_line_height();
            if (node->source->text) {
                measure_text(node->source->text, &text_w, &text_h);
            }
            node->rect.w = node->source->has_w ? node->source->rect.w : text_w + padding * 2.0f + border * 2.0f;
            node->rect.h = node->source->has_h ? node->source->rect.h : text_h + padding * 2.0f + border * 2.0f;
        }
    }

    if (node->source->has_floating_rect) {
        if (node->source->floating_rect.w > 0.0f) node->rect.w = node->source->floating_rect.w;
        if (node->source->floating_rect.h > 0.0f) node->rect.h = node->source->floating_rect.h;
    }

    if (node->source->has_min_w && node->rect.w < node->source->min_w) node->rect.w = node->source->min_w;
    if (node->source->has_min_h && node->rect.h < node->source->min_h) node->rect.h = node->source->min_h;
    if (node->source->has_w) node->rect.w = node->source->rect.w;
    if (node->source->has_h) node->rect.h = node->source->rect.h;
    if (node->source->has_max_w && node->rect.w > node->source->max_w) node->rect.w = node->source->max_w;
    if (node->source->has_max_h && node->rect.h > node->source->max_h) node->rect.h = node->source->max_h;

    if (node->source->has_floating_min) {
        if (node->rect.w < node->source->floating_min_w) node->rect.w = node->source->floating_min_w;
        if (node->rect.h < node->source->floating_min_h) node->rect.h = node->source->floating_min_h;
    }
    if (node->source->has_floating_max) {
        if (node->rect.w > node->source->floating_max_w) node->rect.w = node->source->floating_max_w;
        if (node->rect.h > node->source->floating_max_h) node->rect.h = node->source->floating_max_h;
    }
}

void measure_layout(LayoutNode* root) { measure_node(root); }

static void layout_node(LayoutNode* node, float origin_x, float origin_y, const Vec2* parent_transform) {
    if (!node || !node->source) return;
    float padding = node->source->has_padding_override ? node->source->padding_override : (node->source->style ? node->source->style->padding : DEFAULT_STYLE.padding);
    float border = node->source->border_thickness;
    float local_x = 0.0f;
    float local_y = 0.0f;
    if (node->source->has_floating_rect) {
        local_x = node->source->floating_rect.x;
        local_y = node->source->floating_rect.y;
    } else {
        if (node->source->has_x) local_x = node->source->rect.x;
        if (node->source->has_y) local_y = node->source->rect.y;
    }
    float base_x = origin_x + local_x;
    float base_y = origin_y + local_y;
    node->rect.x = base_x;
    node->rect.y = base_y;
    node->local_rect = (Rect){0.0f, 0.0f, node->rect.w, node->rect.h};
    node->transform = parent_transform ? (Vec2){parent_transform->x + base_x, parent_transform->y + base_y} : (Vec2){base_x, base_y};
    node->wants_clip = node->source->has_clip_to_viewport ? node->source->clip_to_viewport : 0;

    if (node->source->layout == UI_LAYOUT_ROW) {
        float cursor_x = base_x + padding + border;
        float cursor_y = base_y + padding + border;
        for (size_t i = 0; i < node->child_count; i++) {
            layout_node(&node->children[i], cursor_x, cursor_y, &node->transform);
            cursor_x += node->children[i].rect.w + node->source->spacing;
        }
    } else if (node->source->layout == UI_LAYOUT_COLUMN) {
        float cursor_x = base_x + padding + border;
        float cursor_y = base_y + padding + border;
        for (size_t i = 0; i < node->child_count; i++) {
            layout_node(&node->children[i], cursor_x, cursor_y, &node->transform);
            cursor_y += node->children[i].rect.h + node->source->spacing;
        }
    } else if (node->source->layout == UI_LAYOUT_TABLE && node->source->columns > 0) {
        int cols = node->source->columns;
        int rows = (int)((node->child_count + (size_t)cols - 1) / (size_t)cols);
        float* col_w = (float*)calloc((size_t)cols, sizeof(float));
        float* row_h = (float*)calloc((size_t)rows, sizeof(float));
        if (col_w && row_h) {
            for (size_t i = 0; i < node->child_count; i++) {
                int col = (int)(i % (size_t)cols);
                int row = (int)(i / (size_t)cols);
                LayoutNode* ch = &node->children[i];
                if (ch->rect.w > col_w[col]) col_w[col] = ch->rect.w;
                if (ch->rect.h > row_h[row]) row_h[row] = ch->rect.h;
            }
            float y = base_y + padding + border;
            size_t idx = 0;
            for (int r = 0; r < rows; r++) {
                float x = base_x + padding + border;
                for (int c = 0; c < cols && idx < node->child_count; c++, idx++) {
                    layout_node(&node->children[idx], x, y, &node->transform);
                    x += col_w[c] + node->source->spacing;
                }
                y += row_h[r] + node->source->spacing;
            }
        }
        free(col_w);
        free(row_h);
    } else if (node->child_count > 0) {
        float offset_x = base_x + padding;
        float offset_y = base_y + padding;
        for (size_t i = 0; i < node->child_count; i++) {
            layout_node(&node->children[i], offset_x, offset_y, &node->transform);
        }
    }
}

void assign_layout(LayoutNode* root, float origin_x, float origin_y) {
    layout_node(root, origin_x, origin_y, NULL);
}

static void copy_base_rect(LayoutNode* node) {
    if (!node) return;
    node->base_rect = node->rect;
    for (size_t i = 0; i < node->child_count; i++) copy_base_rect(&node->children[i]);
}

void capture_layout_base(LayoutNode* root) { copy_base_rect(root); }

size_t count_layout_widgets(const LayoutNode* root) {
    if (!root) return 0;
    size_t total = 0;
    if (root->source && (root->source->layout == UI_LAYOUT_NONE || root->source->scroll_static)) total += 1;
    for (size_t i = 0; i < root->child_count; i++) total += count_layout_widgets(&root->children[i]);
    return total;
}

static int compute_z_index(const UiNode* source, size_t appearance_order) {
    int explicit_z = (source && source->has_z_index) ? source->z_index : 0;
    int group = (source && source->has_z_group) ? source->z_group : 0;
    int composite = explicit_z + group * UI_Z_ORDER_SCALE;
    return composite * UI_Z_ORDER_SCALE + (int)appearance_order;
}

static void populate_widgets_recursive(const LayoutNode* node, Widget* widgets, size_t widget_count, size_t* idx, size_t* order, char* inherited_scroll_area) {
    if (!node || !widgets || !idx || *idx >= widget_count || !order) return;
    char* active_scroll_area = node->source && node->source->scroll_area ? node->source->scroll_area : inherited_scroll_area;
    if (node->source && (node->source->layout == UI_LAYOUT_NONE || node->source->scroll_static)) {
        Widget* w = &widgets[*idx];
        (*idx)++;
        size_t appearance_order = *order;
        (*order)++;
        w->type = node->source->widget_type;
        w->rect = node->rect;
        w->scroll_offset = 0.0f;
        w->z_index = compute_z_index(node->source, appearance_order);
        w->base_z_index = w->z_index;
        w->z_group = node->source->has_z_group ? node->source->z_group : 0;
        w->color = node->source->color;
        w->text_color = node->source->text_color;
        w->base_border_thickness = node->source->border_thickness;
        w->border_thickness = w->base_border_thickness;
        w->border_color = node->source->border_color;
        w->scrollbar_enabled = node->source->scrollbar_enabled;
        w->scrollbar_width = node->source->scrollbar_width;
        w->scrollbar_track_color = node->source->scrollbar_track_color;
        w->scrollbar_thumb_color = node->source->scrollbar_thumb_color;
        w->base_padding = (node->source->has_padding_override ? node->source->padding_override : (node->source->style ? node->source->style->padding : DEFAULT_STYLE.padding)) + w->base_border_thickness;
        w->padding = w->base_padding;
        w->text = node->source->text;
        w->text_binding = node->source->text_binding;
        w->value_binding = node->source->value_binding;
        w->click_binding = node->source->click_binding;
        w->click_value = node->source->click_value;
        w->minv = node->source->minv;
        w->maxv = node->source->maxv;
        w->value = node->source->value;
        w->id = node->source->id;
        w->docking = node->source->docking;
        w->resizable = node->source->resizable;
        w->has_resizable = node->source->has_resizable;
        w->draggable = node->source->draggable;
        w->has_draggable = node->source->has_draggable;
        w->modal = node->source->modal;
        w->has_floating_rect = node->source->has_floating_rect;
        w->floating_rect = node->source->floating_rect;
        w->floating_min_w = node->source->floating_min_w;
        w->floating_min_h = node->source->floating_min_h;
        w->floating_max_w = node->source->floating_max_w;
        w->floating_max_h = node->source->floating_max_h;
        w->has_floating_min = node->source->has_floating_min;
        w->has_floating_max = node->source->has_floating_max;
        w->on_focus = node->source->on_focus;
        w->scroll_area = node->source->scroll_area ? node->source->scroll_area : active_scroll_area;
        w->scroll_static = node->source->scroll_static;
        w->clip_to_viewport = node->source->clip_to_viewport;
        w->has_clip_to_viewport = node->source->has_clip_to_viewport;
        if (!w->has_clip_to_viewport && w->scroll_static) {
            w->clip_to_viewport = 0;
        }
        w->scroll_viewport = 0.0f;
        w->scroll_content = 0.0f;
        w->show_scrollbar = 0;
        if (node->source->layout != UI_LAYOUT_NONE) {
            w->type = node->source->widget_type == W_PANEL ? node->source->widget_type : W_PANEL;
        }
        if (node->source->layout == UI_LAYOUT_NONE) return;
    }
    for (size_t i = 0; i < node->child_count; i++) populate_widgets_recursive(&node->children[i], widgets, widget_count, idx, order, active_scroll_area);
}

void populate_widgets_from_layout(const LayoutNode* root, Widget* widgets, size_t widget_count) {
    size_t idx = 0;
    size_t order = 0;
    populate_widgets_recursive(root, widgets, widget_count, &idx, &order, NULL);
}

WidgetArray materialize_widgets(const LayoutNode* root) {
    WidgetArray arr = {0};
    size_t count = count_layout_widgets(root);
    if (count == 0) return arr;
    Widget* widgets = (Widget*)calloc(count, sizeof(Widget));
    if (!widgets) return arr;
    populate_widgets_from_layout(root, widgets, count);
    arr.items = widgets;
    arr.count = count;
    return arr;
}

void apply_widget_padding_scale(WidgetArray* widgets, float scale) {
    if (!widgets) return;
    for (size_t i = 0; i < widgets->count; i++) {
        widgets->items[i].padding = widgets->items[i].base_padding * scale;
        widgets->items[i].border_thickness = widgets->items[i].base_border_thickness * scale;
    }
}

void free_model(Model* model) {
    if (!model) return;
    ModelEntry* e = model->entries;
    while (e) {
        ModelEntry* n = e->next;
        free(e->key);
        free(e->string_value);
        free(e);
        e = n;
    }
    free(model->store);
    free(model->key);
    free(model->source_path);
    free(model);
}

void free_styles(Style* styles) {
    while (styles) {
        Style* n = styles->next;
        free(styles->name);
        free(styles);
        styles = n;
    }
}

void free_widgets(WidgetArray widgets) {
    free(widgets.items);
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
