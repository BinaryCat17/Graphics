#include "ui_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static Color default_color(void) { Color c = { 0.6f, 0.6f, 0.6f, 1.0f }; return c; }

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
        if (strcmp(e->key, key) == 0 && e->is_string && e->string_value) return e->string_value;
    }
    return fallback;
}

void model_set_number(Model* model, const char* key, float value) {
    if (!model || !key) return;
    ModelEntry* e = model_get_or_create(model, key);
    if (!e) return;
    e->is_string = 0;
    e->number_value = value;
}

void model_set_string(Model* model, const char* key, const char* value) {
    if (!model || !key) return;
    ModelEntry* e = model_get_or_create(model, key);
    if (!e) return;
    e->is_string = 1;
    free(e->string_value);
    e->string_value = value ? strdup(value) : NULL;
}

static void append_entry_to_json(FILE* f, const ModelEntry* e, int* first) {
    if (!*first) fprintf(f, ",\n");
    *first = 0;
    if (e->is_string) fprintf(f, "    \"%s\": \"%s\"", e->key, e->string_value ? e->string_value : "");
    else fprintf(f, "    \"%s\": %g", e->key, e->number_value);
}

int save_model(const Model* model) {
    if (!model || !model->source_path) return -1;
    FILE* f = fopen(model->source_path, "wb");
    if (!f) return -1;
    fprintf(f, "{\n  \"model\": {\n");
    int first = 1;
    for (const ModelEntry* e = model->entries; e; e = e->next) {
        append_entry_to_json(f, e, &first);
    }
    fprintf(f, "\n  }\n}\n");
    fclose(f);
    return 0;
}

Model* parse_model_json(const char* json, const char* source_path) {
    if (!json) { fprintf(stderr, "Error: model JSON text is null\n"); return NULL; }
    Model* model = (Model*)calloc(1, sizeof(Model));
    if (!model) { fprintf(stderr, "Error: failed to allocate Model\n"); return NULL; }
    model->source_path = source_path ? strdup(source_path) : NULL;

    jsmn_parser p; jsmn_init(&p);
    size_t tokc = 1024;
    jsmntok_t* toks = (jsmntok_t*)malloc(sizeof(jsmntok_t) * tokc);
    if (!toks) { fprintf(stderr, "Error: failed to allocate tokens for model JSON\n"); free(model); return NULL; }
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
                    st->background = default_color();
                    st->text = (Color){ 1.0f, 1.0f, 1.0f, 1.0f };
                    st->next = styles;
                    styles = st;
                    jsmntok_t* sobj = &toks[j + 1];
                    unsigned int k = j + 2;
                    while (k < p.toknext && toks[k].start >= sobj->start && toks[k].end <= sobj->end) {
                        if (tok_is_key(json, &toks[k], "color") && k + 1 < p.toknext) {
                            st->background = st->background;
                            read_color_array(&st->background, json, &toks[k + 1], toks, p.toknext);
                            k += 2;
                            continue;
                        }
                        if (tok_is_key(json, &toks[k], "textColor") && k + 1 < p.toknext) {
                            read_color_array(&st->text, json, &toks[k + 1], toks, p.toknext);
                            k += 2;
                            continue;
                        }
                        k++;
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

typedef struct WidgetDef {
    char* type;
    char* style_name;
    Rect rect;
    int has_x, has_y, has_w, has_h;
    float spacing;
    int columns;
    char* id;
    char* text;
    char* text_binding;
    char* value_binding;
    float minv, maxv, value;
    int has_min, has_max, has_value;
    char* scroll_area;
    int scroll_static;
    Color color;
    Color text_color;
    int has_color;
    struct WidgetDef* children;
    struct WidgetDef* next;
} WidgetDef;

static Widget* append_widget(Widget** list, Widget* w) {
    if (!w) return NULL;
    w->next = *list;
    *list = w;
    return w;
}

static WidgetDef* append_widget_def(WidgetDef** list, WidgetDef* d) {
    if (!d) return NULL;
    d->next = *list;
    *list = d;
    return d;
}

static Widget* create_widget(void) {
    Widget* w = (Widget*)calloc(1, sizeof(Widget));
    if (!w) return NULL;
    w->scroll_offset = 0.0f;
    w->scroll_static = 0;
    return w;
}

static WidgetDef* create_widget_def(void) {
    WidgetDef* d = (WidgetDef*)calloc(1, sizeof(WidgetDef));
    if (!d) return NULL;
    d->spacing = -1.0f;
    return d;
}

static void free_widget_defs(WidgetDef* d) {
    while (d) {
        WidgetDef* n = d->next;
        free_widget_defs(d->children);
        free(d->type);
        free(d->style_name);
        free(d->id);
        free(d->text);
        free(d->text_binding);
        free(d->value_binding);
        free(d->scroll_area);
        free(d);
        d = n;
    }
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

static WidgetDef* parse_widget_def(const char* json, jsmntok_t* toks, unsigned int tokc, unsigned int start_idx) {
    WidgetDef* def = create_widget_def();
    if (!def) return NULL;
    jsmntok_t* obj = &toks[start_idx];
    for (unsigned int k = start_idx + 1; k < tokc && toks[k].start >= obj->start && toks[k].end <= obj->end; k++) {
        if (toks[k].type != JSMN_STRING || k + 1 >= tokc) continue;
        jsmntok_t* val = &toks[k + 1];
        if (tok_is_key(json, &toks[k], "type") && val->type == JSMN_STRING) def->type = tok_copy(json, val);
        else if (tok_is_key(json, &toks[k], "style") && val->type == JSMN_STRING) def->style_name = tok_copy(json, val);
        else if (tok_is_key(json, &toks[k], "x")) { def->rect.x = parse_number(json, val, def->rect.x); def->has_x = 1; }
        else if (tok_is_key(json, &toks[k], "y")) { def->rect.y = parse_number(json, val, def->rect.y); def->has_y = 1; }
        else if (tok_is_key(json, &toks[k], "w")) { def->rect.w = parse_number(json, val, def->rect.w); def->has_w = 1; }
        else if (tok_is_key(json, &toks[k], "h")) { def->rect.h = parse_number(json, val, def->rect.h); def->has_h = 1; }
        else if (tok_is_key(json, &toks[k], "id") && val->type == JSMN_STRING) def->id = tok_copy(json, val);
        else if (tok_is_key(json, &toks[k], "text") && val->type == JSMN_STRING) def->text = tok_copy(json, val);
        else if (tok_is_key(json, &toks[k], "textBinding") && val->type == JSMN_STRING) def->text_binding = tok_copy(json, val);
        else if (tok_is_key(json, &toks[k], "valueBinding") && val->type == JSMN_STRING) def->value_binding = tok_copy(json, val);
        else if (tok_is_key(json, &toks[k], "min")) { def->minv = parse_number(json, val, def->minv); def->has_min = 1; }
        else if (tok_is_key(json, &toks[k], "max")) { def->maxv = parse_number(json, val, def->maxv); def->has_max = 1; }
        else if (tok_is_key(json, &toks[k], "value")) { def->value = parse_number(json, val, def->value); def->has_value = 1; }
        else if (tok_is_key(json, &toks[k], "scrollArea") && val->type == JSMN_STRING) def->scroll_area = tok_copy(json, val);
        else if (tok_is_key(json, &toks[k], "scrollStatic") && val->type == JSMN_PRIMITIVE) {
            int len = val->end - val->start;
            if (len == 4 && strncmp(json + val->start, "true", 4) == 0) def->scroll_static = 1;
            if (len == 5 && strncmp(json + val->start, "false", 5) == 0) def->scroll_static = 0;
        }
        else if (tok_is_key(json, &toks[k], "spacing")) def->spacing = parse_number(json, val, def->spacing);
        else if (tok_is_key(json, &toks[k], "columns")) def->columns = (int)parse_number(json, val, (float)def->columns);
        else if (tok_is_key(json, &toks[k], "color")) {
            def->color = def->color;
            read_color_array(&def->color, json, val, toks, tokc);
            def->has_color = 1;
        }
        else if (tok_is_key(json, &toks[k], "children") && val->type == JSMN_ARRAY) {
            jsmntok_t* arr = val;
            for (unsigned int c = k + 2; c < tokc && toks[c].start >= arr->start && toks[c].end <= arr->end; ) {
                if (toks[c].type == JSMN_OBJECT) {
                    WidgetDef* child = parse_widget_def(json, toks, tokc, c);
                    append_widget_def(&def->children, child);
                }
                c = skip_container(toks, tokc, c);
            }
        }
    }
    return def;
}

static int def_is_container(const WidgetDef* d) {
    if (!d || !d->type) return 0;
    return strcmp(d->type, "row") == 0 || strcmp(d->type, "column") == 0 || strcmp(d->type, "table") == 0;
}

static WidgetType def_to_widget_type(const WidgetDef* d) {
    if (!d || !d->type) return W_PANEL;
    if (strcmp(d->type, "label") == 0) return W_LABEL;
    if (strcmp(d->type, "button") == 0) return W_BUTTON;
    if (strcmp(d->type, "hslider") == 0) return W_HSLIDER;
    return W_PANEL;
}

static void resolve_styles_and_defaults(WidgetDef* defs, const Style* styles) {
    for (WidgetDef* d = defs; d; d = d->next) {
        if (!d->has_min) d->minv = 0.0f;
        if (!d->has_max) d->maxv = 1.0f;
        if (!d->has_value) d->value = 0.0f;
        if (d->spacing < 0.0f) d->spacing = 8.0f;
        d->text_color = (Color){ 1.0f, 1.0f, 1.0f, 1.0f };
        if (!d->has_color) d->color = default_color();
        if (d->style_name) {
            const Style* st = style_find(styles, d->style_name);
            if (st) {
                d->color = st->background;
                d->text_color = st->text;
            }
        }
        resolve_styles_and_defaults(d->children, styles);
    }
}

static void bind_model_values_to_defs(WidgetDef* defs, const Model* model) {
    if (!model) return;
    for (WidgetDef* d = defs; d; d = d->next) {
        if (d->text_binding) {
            const char* v = model_get_string(model, d->text_binding, NULL);
            if (v) {
                free(d->text);
                d->text = strdup(v);
            }
        }
        if (d->value_binding) {
            d->value = model_get_number(model, d->value_binding, d->value);
            d->has_value = 1;
        }
        bind_model_values_to_defs(d->children, model);
    }
}

static Widget* materialize_layout(const WidgetDef* defs, Widget** out, float origin_x, float origin_y) {
    for (const WidgetDef* d = defs; d; d = d->next) {
        if (def_is_container(d)) {
            float base_x = origin_x + (d->has_x ? d->rect.x : 0.0f);
            float base_y = origin_y + (d->has_y ? d->rect.y : 0.0f);
            float cursor_x = 0.0f, cursor_y = 0.0f;
            int col_idx = 0;
            for (WidgetDef* child = d->children; child; child = child->next) {
                float child_base_x = base_x + cursor_x;
                float child_base_y = base_y + cursor_y;
                materialize_layout(child, out, child_base_x, child_base_y);
                float adv_w = child->has_w ? child->rect.w : 100.0f;
                float adv_h = child->has_h ? child->rect.h : 30.0f;
                if (strcmp(d->type, "row") == 0) {
                    cursor_x += adv_w + d->spacing;
                } else if (strcmp(d->type, "table") == 0 && d->columns > 0) {
                    col_idx++;
                    if (col_idx >= d->columns) { col_idx = 0; cursor_x = 0.0f; cursor_y += adv_h + d->spacing; }
                    else cursor_x += adv_w + d->spacing;
                } else {
                    cursor_y += adv_h + d->spacing;
                }
            }
            continue;
        }

        Widget* w = create_widget();
        if (!w) continue;
        w->type = def_to_widget_type(d);
        w->rect.x = origin_x + (d->has_x ? d->rect.x : 0.0f);
        w->rect.y = origin_y + (d->has_y ? d->rect.y : 0.0f);
        if (d->has_w) w->rect.w = d->rect.w;
        if (d->has_h) w->rect.h = d->rect.h;
        w->color = d->color;
        w->text_color = d->text_color;
        w->minv = d->minv;
        w->maxv = d->maxv;
        w->value = d->value;
        w->scroll_static = d->scroll_static;
        if (d->id) w->id = strdup(d->id);
        if (d->text) w->text = strdup(d->text);
        if (d->text_binding) w->text_binding = strdup(d->text_binding);
        if (d->value_binding) w->value_binding = strdup(d->value_binding);
        if (d->scroll_area) w->scroll_area = strdup(d->scroll_area);
        append_widget(out, w);
    }
    return *out;
}

static WidgetDef* parse_layout_definitions(const char* json) {
    WidgetDef* defs = NULL;
    jsmn_parser p; jsmn_init(&p);
    size_t tokc = 4096;
    jsmntok_t* toks = (jsmntok_t*)malloc(sizeof(jsmntok_t) * tokc);
    if (!toks) { fprintf(stderr, "Error: failed to allocate tokens for layout JSON\n"); return NULL; }
    for (size_t i = 0; i < tokc; i++) { toks[i].start = toks[i].end = -1; toks[i].size = 0; toks[i].type = JSMN_UNDEFINED; }
    int parse_ret = jsmn_parse(&p, json, strlen(json), toks, tokc);
    if (parse_ret < 0) { fprintf(stderr, "Error: failed to parse layout JSON (code %d)\n", parse_ret); free(toks); return NULL; }

    int sections_found = 0;
    int defs_created = 0;
    for (unsigned int i = 0; i < p.toknext; i++) {
        if (tok_is_key(json, &toks[i], "layout") && i + 1 < p.toknext && toks[i + 1].type == JSMN_OBJECT) {
            sections_found++;
            WidgetDef* def = parse_widget_def(json, toks, p.toknext, i + 1);
            if (def) { append_widget_def(&defs, def); defs_created = 1; }
        }
        if (tok_is_key(json, &toks[i], "floating") && i + 1 < p.toknext && toks[i + 1].type == JSMN_ARRAY) {
            sections_found++;
            jsmntok_t* arr = &toks[i + 1];
            for (unsigned int j = i + 2; j < p.toknext && toks[j].start >= arr->start && toks[j].end <= arr->end; ) {
                if (toks[j].type == JSMN_OBJECT) {
                    WidgetDef* def = parse_widget_def(json, toks, p.toknext, j);
                    if (def) { append_widget_def(&defs, def); defs_created = 1; }
                }
                j = skip_container(toks, p.toknext, j);
            }
        }
    }
    if (sections_found == 0) fprintf(stderr, "Error: no 'layout' or 'floating' sections found in layout.json\n");
    if (!defs_created) fprintf(stderr, "Error: no widgets could be constructed from layout.json\n");
    free(toks);
    return defs;
}

static void bind_from_model(Widget* w, const Model* model) {
    if (!w || !model) return;
    if (w->text_binding) {
        const char* v = model_get_string(model, w->text_binding, NULL);
        if (v) {
            free(w->text);
            w->text = strdup(v);
        }
    }
    if (w->value_binding) {
        w->value = model_get_number(model, w->value_binding, w->value);
    }
}

void update_widget_bindings(Widget* widgets, const Model* model) {
    for (Widget* w = widgets; w; w = w->next) {
        bind_from_model(w, model);
    }
}

Widget* parse_layout_json(const char* json, const Model* model, const Style* styles) {
    if (!json) { fprintf(stderr, "Error: layout JSON text is null\n"); return NULL; }

    WidgetDef* defs = parse_layout_definitions(json);
    if (!defs) return NULL;

    resolve_styles_and_defaults(defs, styles);
    bind_model_values_to_defs(defs, model);

    Widget* widgets = NULL;
    materialize_layout(defs, &widgets, 0.0f, 0.0f);

    free_widget_defs(defs);
    return widgets;
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

void free_widgets(Widget* widgets) {
    while (widgets) {
        Widget* n = widgets->next;
        free(widgets->text);
        free(widgets->text_binding);
        free(widgets->value_binding);
        free(widgets->id);
        free(widgets->scroll_area);
        free(widgets);
        widgets = n;
    }
}
