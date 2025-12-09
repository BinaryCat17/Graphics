#include "ui_json.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stb_truetype.h"
#include "scene_ui.h"

typedef enum { JSMN_UNDEFINED = 0, JSMN_OBJECT = 1, JSMN_ARRAY = 2, JSMN_STRING = 3, JSMN_PRIMITIVE = 4 } jsmntype_t;
typedef struct { jsmntype_t type; int start; int end; int size; } jsmntok_t;
typedef struct { unsigned int pos; unsigned int toknext; int toksuper; } jsmn_parser;

static void jsmn_init(jsmn_parser* p) { p->pos = 0; p->toknext = 0; p->toksuper = -1; }
static int jsmn_alloc(jsmn_parser* p, jsmntok_t* toks, size_t nt) {
    if (p->toknext >= nt) return -1;
    toks[p->toknext].start = toks[p->toknext].end = -1;
    toks[p->toknext].size = 0;
    toks[p->toknext].type = JSMN_UNDEFINED;
    return (int)p->toknext++;
}
static int jsmn_parse(jsmn_parser* p, const char* js, size_t len, jsmntok_t* toks, size_t nt) {
    for (size_t i = p->pos; i < len; i++) {
        char c = js[i];
        switch (c) {
        case '{': case '[': {
            int tk = jsmn_alloc(p, toks, nt);
            if (tk < 0) return -1;
            toks[tk].type = (c == '{') ? JSMN_OBJECT : JSMN_ARRAY;
            toks[tk].start = (int)i;
            toks[tk].size = 0;
            p->toksuper = tk;
            break;
        }
        case '}': case ']': {
            jsmntype_t want = (c == '}') ? JSMN_OBJECT : JSMN_ARRAY;
            int found = -1;
            for (int j = (int)p->toknext - 1; j >= 0; j--) {
                if (toks[j].start != -1 && toks[j].end == -1 && toks[j].type == want) { found = j; break; }
            }
            if (found == -1) return -1;
            toks[found].end = (int)i + 1;
            p->toksuper = -1;
            break;
        }
        case '"': {
            int tk = jsmn_alloc(p, toks, nt);
            if (tk < 0) return -1;
            toks[tk].type = JSMN_STRING;
            toks[tk].start = (int)i + 1;
            i++;
            while (i < len && js[i] != '"') i++;
            if (i >= len) return -1;
            toks[tk].end = (int)i;
            break;
        }
        default:
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ':' || c == ',') break;
            {
                int tk = jsmn_alloc(p, toks, nt);
                if (tk < 0) return -1;
                toks[tk].type = JSMN_PRIMITIVE;
                toks[tk].start = (int)i;
                while (i < len && js[i] != ',' && js[i] != ']' && js[i] != '}' && js[i] != '\n' && js[i] != '\r' && js[i] != '\t' && js[i] != ' ') i++;
                toks[tk].end = (int)i;
                i--;
            }
        }
    }
    p->pos = (unsigned int)len;
    return 0;
}

static char* tok_copy(const char* js, const jsmntok_t* t) {
    int n = t->end - t->start;
    char* s = (char*)malloc((size_t)n + 1);
    memcpy(s, js + t->start, (size_t)n);
    s[n] = 0;
    return s;
}

static int tok_is_key(const char* json, const jsmntok_t* tok, const char* key) {
    if (tok->type != JSMN_STRING) return 0;
    int len = tok->end - tok->start;
    return (int)strlen(key) == len && strncmp(json + tok->start, key, (size_t)len) == 0;
}

static float parse_number(const char* json, const jsmntok_t* tok, float fallback) {
    char* s = tok_copy(json, tok);
    float v = fallback;
    if (s) {
        v = (float)atof(s);
        free(s);
    }
    return v;
}

static const Style DEFAULT_STYLE = { .name = NULL, .background = {0.6f, 0.6f, 0.6f, 1.0f}, .text = {1.0f, 1.0f, 1.0f, 1.0f}, .border_color = {1.0f, 1.0f, 1.0f, 1.0f}, .scrollbar_track_color = {0.6f, 0.6f, 0.6f, 0.4f}, .scrollbar_thumb_color = {1.0f, 1.0f, 1.0f, 0.7f}, .padding = 6.0f, .border_thickness = 0.0f, .scrollbar_width = 0.0f, .has_scrollbar_width = 0, .next = NULL };
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

static unsigned int skip_container(const jsmntok_t* toks, unsigned int tokc, unsigned int idx);

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

int save_model(const Model* model) {
    if (!model || !model->source_path) return 0;
    FILE* f = fopen(model->source_path, "wb");
    if (!f) return 0;
    fputs("{\n  \"model\": {\n", f);
    int first = 1;
    for (ModelEntry* e = model->entries; e; e = e->next) {
        if (!first) fputs(",\n", f);
        first = 0;
        fprintf(f, "    \"%s\": ", e->key);
        if (e->is_string) fprintf(f, "\"%s\"", e->string_value ? e->string_value : "");
        else fprintf(f, "%f", e->number_value);
    }
    fputs("\n  }\n}\n", f);
    fclose(f);
    return 1;
}

static Style* style_find(const Style* styles, const char* name) {
    for (const Style* s = styles; s; s = s->next) {
        if (strcmp(s->name, name) == 0) return (Style*)s;
    }
    return NULL;
}

static void read_color_array(Color* out, const char* json, const jsmntok_t* val, const jsmntok_t* toks, unsigned int tokc) {
    if (!out || val->type != JSMN_ARRAY) return;
    float cols[4] = { out->r, out->g, out->b, out->a };
    int cc = 0;
    for (unsigned int z = (unsigned int)(val - toks) + 1; z < tokc && toks[z].start >= val->start && toks[z].end <= val->end; z++) {
        if (toks[z].type == JSMN_PRIMITIVE) {
            cols[cc < 4 ? cc : 3] = parse_number(json, &toks[z], cols[cc < 4 ? cc : 3]);
            cc++;
        }
    }
    out->r = cols[0]; out->g = cols[1]; out->b = cols[2]; out->a = cols[3];
}

Model* parse_model_json(const char* json, const char* source_path) {
    if (!json) { fprintf(stderr, "Error: model JSON text is null\n"); return NULL; }
    Model* model = (Model*)calloc(1, sizeof(Model));
    if (!model) return NULL;
    model->source_path = strdup(source_path ? source_path : "model.json");
    jsmn_parser p; jsmn_init(&p);
    size_t tokc = 1024;
    jsmntok_t* toks = (jsmntok_t*)malloc(sizeof(jsmntok_t) * tokc);
    if (!toks) { free(model); return NULL; }
    for (size_t i = 0; i < tokc; i++) { toks[i].start = toks[i].end = -1; toks[i].size = 0; toks[i].type = JSMN_UNDEFINED; }
    int parse_ret = jsmn_parse(&p, json, strlen(json), toks, tokc);
    if (parse_ret < 0) { fprintf(stderr, "Error: failed to parse model JSON (code %d)\n", parse_ret); free(model); free(toks); return NULL; }

    for (unsigned int i = 0; i + 1 < p.toknext; i++) {
        if (tok_is_key(json, &toks[i], "model") && toks[i + 1].type == JSMN_OBJECT) {
            jsmntok_t* obj = &toks[i + 1];
            for (unsigned int j = i + 2; j < p.toknext && toks[j].start >= obj->start && toks[j].end <= obj->end; j += 2) {
                if (toks[j].type != JSMN_STRING || j + 1 >= p.toknext) continue;
                char* key = tok_copy(json, &toks[j]);
                jsmntok_t* val = &toks[j + 1];
                if (val->type == JSMN_STRING) {
                    char* sv = tok_copy(json, val);
                    model_set_string(model, key, sv);
                    free(sv);
                }
                else if (val->type == JSMN_PRIMITIVE) {
                    float num = parse_number(json, val, 0.0f);
                    model_set_number(model, key, num);
                }
                free(key);
            }
            break;
        }
    }
    free(toks);
    return model;
}

Style* parse_styles_json(const char* json) {
    if (!json) { fprintf(stderr, "Error: styles JSON text is null\n"); return NULL; }
    Style* styles = NULL;
    jsmn_parser p; jsmn_init(&p);
    size_t tokc = 1024;
    jsmntok_t* toks = (jsmntok_t*)malloc(sizeof(jsmntok_t) * tokc);
    if (!toks) { fprintf(stderr, "Error: failed to allocate tokens for styles JSON\n"); return NULL; }
    for (size_t i = 0; i < tokc; i++) { toks[i].start = toks[i].end = -1; toks[i].size = 0; toks[i].type = JSMN_UNDEFINED; }
    int parse_ret = jsmn_parse(&p, json, strlen(json), toks, tokc);
    if (parse_ret < 0) { fprintf(stderr, "Error: failed to parse styles JSON (code %d)\n", parse_ret); free(toks); return NULL; }

    for (unsigned int i = 0; i + 1 < p.toknext; i++) {
        if (tok_is_key(json, &toks[i], "styles") && toks[i + 1].type == JSMN_OBJECT) {
            jsmntok_t* obj = &toks[i + 1];
            for (unsigned int j = i + 2; j < p.toknext && toks[j].start >= obj->start && toks[j].end <= obj->end; ) {
                if (toks[j].type == JSMN_STRING && j + 1 < p.toknext && toks[j + 1].type == JSMN_OBJECT) {
                    char* style_name = tok_copy(json, &toks[j]);
                    Style* st = (Style*)calloc(1, sizeof(Style));
                    if (!st) { free(style_name); break; }
                    st->name = style_name;
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
                    jsmntok_t* sobj = &toks[j + 1];
                    unsigned int k = j + 2;
                    while (k < p.toknext && toks[k].start >= sobj->start && toks[k].end <= sobj->end) {
                        if (toks[k].type != JSMN_STRING || k + 1 >= p.toknext) { k++; continue; }
                        jsmntok_t* val = &toks[k + 1];
                        if (tok_is_key(json, &toks[k], "color")) {
                            read_color_array(&st->background, json, val, toks, p.toknext);
                            k = skip_container(toks, p.toknext, (unsigned int)(val - toks));
                            continue;
                        }
                        if (tok_is_key(json, &toks[k], "textColor")) {
                            read_color_array(&st->text, json, val, toks, p.toknext);
                            k = skip_container(toks, p.toknext, (unsigned int)(val - toks));
                            continue;
                        }
                        if (tok_is_key(json, &toks[k], "borderColor")) {
                            read_color_array(&st->border_color, json, val, toks, p.toknext);
                            k = skip_container(toks, p.toknext, (unsigned int)(val - toks));
                            continue;
                        }
                        if (tok_is_key(json, &toks[k], "padding") && val->type == JSMN_PRIMITIVE) {
                            st->padding = parse_number(json, val, st->padding);
                            k += 2;
                            continue;
                        }
                        if (tok_is_key(json, &toks[k], "borderThickness") && val->type == JSMN_PRIMITIVE) {
                            st->border_thickness = parse_number(json, val, st->border_thickness);
                            k += 2;
                            continue;
                        }
                        if (tok_is_key(json, &toks[k], "scrollbarTrackColor")) {
                            read_color_array(&st->scrollbar_track_color, json, val, toks, p.toknext);
                            k = skip_container(toks, p.toknext, (unsigned int)(val - toks));
                            continue;
                        }
                        if (tok_is_key(json, &toks[k], "scrollbarThumbColor")) {
                            read_color_array(&st->scrollbar_thumb_color, json, val, toks, p.toknext);
                            k = skip_container(toks, p.toknext, (unsigned int)(val - toks));
                            continue;
                        }
                        if (tok_is_key(json, &toks[k], "scrollbarWidth") && val->type == JSMN_PRIMITIVE) {
                            st->scrollbar_width = parse_number(json, val, st->scrollbar_width);
                            st->has_scrollbar_width = 1;
                            k += 2;
                            continue;
                        }
                        char* key_name = tok_copy(json, &toks[k]);
                        fprintf(stderr, "Error: unknown style field '%s' in style '%s'\n", key_name ? key_name : "<null>", st->name);
                        free(key_name);
                        k = skip_container(toks, p.toknext, (unsigned int)(val - toks));
                    }
                    j += 2;
                }
                else j++;
            }
            break;
        }
    }
    free(toks);
    return styles;
}

static unsigned int skip_container(const jsmntok_t* toks, unsigned int tokc, unsigned int idx) {
    if (idx >= tokc) return tokc;
    unsigned int i = idx + 1;
    while (i < tokc && toks[i].start >= toks[idx].start && toks[i].end <= toks[idx].end) {
        if (toks[i].type == JSMN_OBJECT || toks[i].type == JSMN_ARRAY) i = skip_container(toks, tokc, i);
        else i++;
    }
    return i;
}

static UiNode* create_node(void) {
    UiNode* node = (UiNode*)calloc(1, sizeof(UiNode));
    if (!node) return NULL;
    node->layout = UI_LAYOUT_NONE;
    node->widget_type = W_PANEL;
    node->z_index = 0;
    node->has_z_index = 0;
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
    node->has_min = node->has_max = node->has_value = 0;
    node->minv = 0.0f; node->maxv = 1.0f; node->value = 0.0f;
    node->max_w = 0.0f; node->max_h = 0.0f;
    node->has_max_w = node->has_max_h = 0;
    node->has_color = 0;
    node->has_text_color = 0;
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

static UiNode* parse_ui_node(const char* json, jsmntok_t* toks, unsigned int tokc, unsigned int start_idx) {
    UiNode* node = create_node();
    if (!node) return NULL;
    jsmntok_t* obj = &toks[start_idx];
    for (unsigned int k = start_idx + 1; k < tokc && toks[k].start >= obj->start && toks[k].end <= obj->end; ) {
        if (toks[k].type != JSMN_STRING || k + 1 >= tokc) { k++; continue; }
        jsmntok_t* val = &toks[k + 1];
        if (tok_is_key(json, &toks[k], "type") && val->type == JSMN_STRING) { node->type = tok_copy(json, val); k += 2; continue; }
        if (tok_is_key(json, &toks[k], "style") && val->type == JSMN_STRING) { node->style_name = tok_copy(json, val); k += 2; continue; }
        if (tok_is_key(json, &toks[k], "x")) { node->rect.x = parse_number(json, val, node->rect.x); node->has_x = 1; k += 2; continue; }
        if (tok_is_key(json, &toks[k], "y")) { node->rect.y = parse_number(json, val, node->rect.y); node->has_y = 1; k += 2; continue; }
        if (tok_is_key(json, &toks[k], "w")) { node->rect.w = parse_number(json, val, node->rect.w); node->has_w = 1; k += 2; continue; }
        if (tok_is_key(json, &toks[k], "h")) { node->rect.h = parse_number(json, val, node->rect.h); node->has_h = 1; k += 2; continue; }
        if (tok_is_key(json, &toks[k], "z")) { node->z_index = (int)parse_number(json, val, (float)node->z_index); node->has_z_index = 1; k += 2; continue; }
        if (tok_is_key(json, &toks[k], "id") && val->type == JSMN_STRING) { node->id = tok_copy(json, val); k += 2; continue; }
        if (tok_is_key(json, &toks[k], "use") && val->type == JSMN_STRING) { node->use = tok_copy(json, val); k += 2; continue; }
        if (tok_is_key(json, &toks[k], "text") && val->type == JSMN_STRING) { node->text = tok_copy(json, val); k += 2; continue; }
        if (tok_is_key(json, &toks[k], "textBinding") && val->type == JSMN_STRING) { node->text_binding = tok_copy(json, val); k += 2; continue; }
        if (tok_is_key(json, &toks[k], "valueBinding") && val->type == JSMN_STRING) { node->value_binding = tok_copy(json, val); k += 2; continue; }
        if (tok_is_key(json, &toks[k], "onClick") && val->type == JSMN_STRING) { node->click_binding = tok_copy(json, val); k += 2; continue; }
        if (tok_is_key(json, &toks[k], "clickValue") && val->type == JSMN_STRING) { node->click_value = tok_copy(json, val); k += 2; continue; }
        if (tok_is_key(json, &toks[k], "min")) { node->minv = parse_number(json, val, node->minv); node->has_min = 1; k += 2; continue; }
        if (tok_is_key(json, &toks[k], "max")) { node->maxv = parse_number(json, val, node->maxv); node->has_max = 1; k += 2; continue; }
        if (tok_is_key(json, &toks[k], "value")) { node->value = parse_number(json, val, node->value); node->has_value = 1; k += 2; continue; }
        if (tok_is_key(json, &toks[k], "maxWidth")) { node->max_w = parse_number(json, val, node->max_w); node->has_max_w = 1; k += 2; continue; }
        if (tok_is_key(json, &toks[k], "maxHeight")) { node->max_h = parse_number(json, val, node->max_h); node->has_max_h = 1; k += 2; continue; }
        if (tok_is_key(json, &toks[k], "scrollArea") && val->type == JSMN_STRING) { node->scroll_area = tok_copy(json, val); k += 2; continue; }
        if (tok_is_key(json, &toks[k], "scrollStatic") && val->type == JSMN_PRIMITIVE) {
            int len = val->end - val->start;
            if (len == 4 && strncmp(json + val->start, "true", 4) == 0) node->scroll_static = 1;
            if (len == 5 && strncmp(json + val->start, "false", 5) == 0) node->scroll_static = 0;
            k += 2;
            continue;
        }
        if (tok_is_key(json, &toks[k], "scrollbar") && val->type == JSMN_PRIMITIVE) {
            int len = val->end - val->start;
            if (len == 4 && strncmp(json + val->start, "true", 4) == 0) node->scrollbar_enabled = 1;
            if (len == 5 && strncmp(json + val->start, "false", 5) == 0) node->scrollbar_enabled = 0;
            k += 2;
            continue;
        }
        if (tok_is_key(json, &toks[k], "scrollbarWidth")) {
            node->scrollbar_width = parse_number(json, val, node->scrollbar_width);
            node->has_scrollbar_width = 1;
            k += 2;
            continue;
        }
        if (tok_is_key(json, &toks[k], "spacing")) { node->spacing = parse_number(json, val, node->spacing); node->has_spacing = 1; k += 2; continue; }
        if (tok_is_key(json, &toks[k], "columns")) { node->columns = (int)parse_number(json, val, (float)node->columns); node->has_columns = 1; k += 2; continue; }
        if (tok_is_key(json, &toks[k], "padding")) { node->padding_override = parse_number(json, val, node->padding_override); node->has_padding_override = 1; k += 2; continue; }
        if (tok_is_key(json, &toks[k], "borderThickness")) { node->border_thickness = parse_number(json, val, node->border_thickness); node->has_border_thickness = 1; k += 2; continue; }
        if (tok_is_key(json, &toks[k], "color")) {
            read_color_array(&node->color, json, val, toks, tokc);
            node->has_color = 1;
            k = skip_container(toks, tokc, (unsigned int)(val - toks));
            continue;
        }
        if (tok_is_key(json, &toks[k], "borderColor")) {
            read_color_array(&node->border_color, json, val, toks, tokc);
            node->has_border_color = 1;
            k = skip_container(toks, tokc, (unsigned int)(val - toks));
            continue;
        }
        if (tok_is_key(json, &toks[k], "textColor")) {
            read_color_array(&node->text_color, json, val, toks, tokc);
            node->has_text_color = 1;
            k = skip_container(toks, tokc, (unsigned int)(val - toks));
            continue;
        }
        if (tok_is_key(json, &toks[k], "scrollbarTrackColor")) {
            read_color_array(&node->scrollbar_track_color, json, val, toks, tokc);
            node->has_scrollbar_track_color = 1;
            k = skip_container(toks, tokc, (unsigned int)(val - toks));
            continue;
        }
        if (tok_is_key(json, &toks[k], "scrollbarThumbColor")) {
            read_color_array(&node->scrollbar_thumb_color, json, val, toks, tokc);
            node->has_scrollbar_thumb_color = 1;
            k = skip_container(toks, tokc, (unsigned int)(val - toks));
            continue;
        }
        if (tok_is_key(json, &toks[k], "children") && val->type == JSMN_ARRAY) {
            jsmntok_t* arr = val;
            for (unsigned int c = k + 2; c < tokc && toks[c].start >= arr->start && toks[c].end <= arr->end; ) {
                if (toks[c].type == JSMN_OBJECT) {
                    UiNode* child = parse_ui_node(json, toks, tokc, c);
                    append_child(node, child);
                }
                c = skip_container(toks, tokc, c);
            }
            k = skip_container(toks, tokc, (unsigned int)(val - toks));
            continue;
        }
        char* key_name = tok_copy(json, &toks[k]);
        fprintf(stderr, "Error: unknown layout field '%s'\n", key_name ? key_name : "<null>");
        free(key_name);
        k = skip_container(toks, tokc, (unsigned int)(val - toks));
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
    if (!node->has_max_w && proto->has_max_w) { node->max_w = proto->max_w; node->has_max_w = 1; }
    if (!node->has_max_h && proto->has_max_h) { node->max_h = proto->max_h; node->has_max_h = 1; }
    if (!node->scroll_area && proto->scroll_area) node->scroll_area = strdup(proto->scroll_area);
    if (!node->scroll_static && proto->scroll_static) node->scroll_static = 1;

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
    n->max_w = src->max_w; n->max_h = src->max_h;
    n->has_max_w = src->has_max_w; n->has_max_h = src->has_max_h;
    n->scroll_area = src->scroll_area ? strdup(src->scroll_area) : NULL;
    n->scroll_static = src->scroll_static;

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

static void resolve_styles_and_defaults(UiNode* node, const Style* styles) {
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
        if (found) st = found;
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
        resolve_styles_and_defaults(&node->children[i], styles);
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

static UiNode* parse_layout_definitions(const char* json, Prototype** out_prototypes) {
    UiNode* root = create_node();
    if (!root) return NULL;
    root->layout = UI_LAYOUT_ABSOLUTE;
    root->style = &ROOT_STYLE;
    root->spacing = 0.0f;

    jsmn_parser p; jsmn_init(&p);
    size_t tokc = 4096;
    jsmntok_t* toks = (jsmntok_t*)malloc(sizeof(jsmntok_t) * tokc);
    if (!toks) { fprintf(stderr, "Error: failed to allocate tokens for layout JSON\n"); free(root); return NULL; }
    for (size_t i = 0; i < tokc; i++) { toks[i].start = toks[i].end = -1; toks[i].size = 0; toks[i].type = JSMN_UNDEFINED; }
    int parse_ret = jsmn_parse(&p, json, strlen(json), toks, tokc);
    if (parse_ret < 0) { fprintf(stderr, "Error: failed to parse layout JSON (code %d)\n", parse_ret); free(toks); free(root); return NULL; }

    int sections_found = 0;
    Prototype* prototypes = out_prototypes ? *out_prototypes : NULL;
    for (unsigned int i = 0; i < p.toknext; i++) {
        if (tok_is_key(json, &toks[i], "widgets") && i + 1 < p.toknext && toks[i + 1].type == JSMN_OBJECT) {
            jsmntok_t* obj = &toks[i + 1];
            for (unsigned int j = i + 2; j < p.toknext && toks[j].start >= obj->start && toks[j].end <= obj->end; ) {
                if (toks[j].type == JSMN_STRING && j + 1 < p.toknext && toks[j + 1].type == JSMN_OBJECT) {
                    char* name = tok_copy(json, &toks[j]);
                    UiNode* def = parse_ui_node(json, toks, p.toknext, j + 1);
                    Prototype* pnode = (Prototype*)calloc(1, sizeof(Prototype));
                    if (pnode) {
                        pnode->name = name;
                        pnode->node = def;
                        pnode->next = prototypes;
                        prototypes = pnode;
                    } else {
                        free(name);
                        free_ui_tree(def);
                    }
                    j = skip_container(toks, p.toknext, j + 1);
                    continue;
                }
                j++;
            }
        }
        if (tok_is_key(json, &toks[i], "layout") && i + 1 < p.toknext && toks[i + 1].type == JSMN_OBJECT) {
            sections_found++;
            UiNode* def = parse_ui_node(json, toks, p.toknext, i + 1);
            append_child(root, def);
        }
        if (tok_is_key(json, &toks[i], "floating") && i + 1 < p.toknext && toks[i + 1].type == JSMN_ARRAY) {
            sections_found++;
            jsmntok_t* arr = &toks[i + 1];
            for (unsigned int j = i + 2; j < p.toknext && toks[j].start >= arr->start && toks[j].end <= arr->end; ) {
                if (toks[j].type == JSMN_OBJECT) {
                    UiNode* def = parse_ui_node(json, toks, p.toknext, j);
                    append_child(root, def);
                }
                j = skip_container(toks, p.toknext, j);
            }
        }
    }
    if (out_prototypes) *out_prototypes = prototypes;
    if (sections_found == 0) fprintf(stderr, "Error: no 'layout' or 'floating' sections found in layout.json\n");
    free(toks);
    return root;
}

void update_widget_bindings(UiNode* root, const Model* model) {
    bind_model_values_to_nodes(root, model);
}

UiNode* parse_layout_json(const char* json, const Model* model, const Style* styles, const char* font_path, const Scene* scene) {
    if (!json) { fprintf(stderr, "Error: layout JSON text is null\n"); return NULL; }

    ensure_font_metrics(font_path);

    Prototype* prototypes = NULL;
    UiNode* root = parse_layout_definitions(json, &prototypes);
    if (!root) return NULL;

    if (scene) {
        scene_ui_inject(root, scene);
    }

    apply_prototypes(root, prototypes);
    resolve_styles_and_defaults(root, styles);
    bind_model_values_to_nodes(root, model);
    int scroll_counter = 0;
    auto_assign_scroll_areas(root, &scroll_counter, NULL);
    free_prototypes(prototypes);
    return root;
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

    if (node->source->has_w) node->rect.w = node->source->rect.w;
    if (node->source->has_h) node->rect.h = node->source->rect.h;
}

void measure_layout(LayoutNode* root) { measure_node(root); }

static void layout_node(LayoutNode* node, float origin_x, float origin_y) {
    if (!node || !node->source) return;
    float padding = node->source->has_padding_override ? node->source->padding_override : (node->source->style ? node->source->style->padding : DEFAULT_STYLE.padding);
    float border = node->source->border_thickness;
    float base_x = origin_x + (node->source->has_x ? node->source->rect.x : 0.0f);
    float base_y = origin_y + (node->source->has_y ? node->source->rect.y : 0.0f);
    node->rect.x = base_x;
    node->rect.y = base_y;

    if (node->source->layout == UI_LAYOUT_ROW) {
        float cursor_x = base_x + padding + border;
        float cursor_y = base_y + padding + border;
        for (size_t i = 0; i < node->child_count; i++) {
            layout_node(&node->children[i], cursor_x, cursor_y);
            cursor_x += node->children[i].rect.w + node->source->spacing;
        }
    } else if (node->source->layout == UI_LAYOUT_COLUMN) {
        float cursor_x = base_x + padding + border;
        float cursor_y = base_y + padding + border;
        for (size_t i = 0; i < node->child_count; i++) {
            layout_node(&node->children[i], cursor_x, cursor_y);
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
                    layout_node(&node->children[idx], x, y);
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
            layout_node(&node->children[i], offset_x, offset_y);
        }
    }
}

void assign_layout(LayoutNode* root, float origin_x, float origin_y) { layout_node(root, origin_x, origin_y); }

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
    return explicit_z * UI_Z_ORDER_SCALE - (int)appearance_order;
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
        w->scroll_area = node->source->scroll_area ? node->source->scroll_area : active_scroll_area;
        w->scroll_static = node->source->scroll_static;
        w->has_clip = 0;
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
}

void free_ui_tree(UiNode* node) {
    if (!node) return;
    free_ui_node(node);
    free(node);
}
