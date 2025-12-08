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

static Widget* append_widget(Widget** list, Widget* w) {
    if (!w) return NULL;
    w->next = *list;
    *list = w;
    return w;
}

static Widget* create_widget(void) {
    Widget* w = (Widget*)calloc(1, sizeof(Widget));
    if (!w) return NULL;
    w->color = default_color();
    w->text_color = (Color){ 1.0f, 1.0f, 1.0f, 1.0f };
    w->minv = 0.0f; w->maxv = 1.0f; w->value = 0.0f;
    return w;
}

static void apply_style(Widget* w, const Style* styles, const char* style_name) {
    if (!w || !style_name) return;
    const Style* st = style_find(styles, style_name);
    if (st) {
        w->color = st->background;
        w->text_color = st->text;
    }
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

static void parse_widget_object(Widget** list, const char* json, jsmntok_t* toks, unsigned int tokc, unsigned int start_idx, const Model* model, const Style* styles, float base_x, float base_y);

static unsigned int skip_container(const jsmntok_t* toks, unsigned int tokc, unsigned int idx) {
    if (idx >= tokc) return tokc;
    unsigned int i = idx + 1;
    while (i < tokc && toks[i].start >= toks[idx].start && toks[i].end <= toks[idx].end) {
        if (toks[i].type == JSMN_OBJECT || toks[i].type == JSMN_ARRAY) i = skip_container(toks, tokc, i);
        else i++;
    }
    return i;
}

static void place_child_widgets(Widget** list, const char* json, jsmntok_t* toks, unsigned int tokc, unsigned int start_idx, const Model* model, const Style* styles, float base_x, float base_y, const char* direction, float spacing, int columns) {
    jsmntok_t* obj = &toks[start_idx];
    float cursor_x = base_x;
    float cursor_y = base_y;
    int col_idx = 0;
    for (unsigned int k = start_idx + 1; k < tokc && toks[k].start >= obj->start && toks[k].end <= obj->end; k++) {
        if (tok_is_key(json, &toks[k], "children") && k + 1 < tokc && toks[k + 1].type == JSMN_ARRAY) {
            jsmntok_t* arr = &toks[k + 1];
            for (unsigned int c = k + 2; c < tokc && toks[c].start >= arr->start && toks[c].end <= arr->end; ) {
                if (toks[c].type == JSMN_OBJECT) {
                    parse_widget_object(list, json, toks, tokc, c, model, styles, cursor_x, cursor_y);
                    // find last added widget to read its width/height
                    float advance_x = 0.0f, advance_y = 0.0f;
                    // To compute advance, read w/h from object tokens
                    float child_w = 0.0f, child_h = 0.0f;
                    for (unsigned int t = c + 1; t < tokc && toks[t].start >= toks[c].start && toks[t].end <= toks[c].end; t++) {
                        if (tok_is_key(json, &toks[t], "w") && t + 1 < tokc) child_w = parse_number(json, &toks[t + 1], child_w);
                        if (tok_is_key(json, &toks[t], "h") && t + 1 < tokc) child_h = parse_number(json, &toks[t + 1], child_h);
                    }
                    if (child_w <= 0) child_w = 100.0f;
                    if (child_h <= 0) child_h = 30.0f;

                    if (direction && strcmp(direction, "row") == 0) {
                        cursor_x += child_w + spacing;
                    }
                    else if (direction && strcmp(direction, "table") == 0 && columns > 0) {
                        col_idx++;
                        if (col_idx >= columns) { col_idx = 0; cursor_x = base_x; cursor_y += child_h + spacing; }
                        else cursor_x += child_w + spacing;
                    }
                    else { // column
                        cursor_y += child_h + spacing;
                    }
                }
                c = skip_container(toks, tokc, c);
            }
        }
    }
}

static void parse_widget_object(Widget** list, const char* json, jsmntok_t* toks, unsigned int tokc, unsigned int start_idx, const Model* model, const Style* styles, float base_x, float base_y) {
    Widget* w = NULL;
    jsmntok_t* obj = &toks[start_idx];
    char* type = NULL;
    char* style_name = NULL;
    float spacing = 8.0f;
    int columns = 0;
    for (unsigned int k = start_idx + 1; k < tokc && toks[k].start >= obj->start && toks[k].end <= obj->end; k++) {
        if (toks[k].type != JSMN_STRING || k + 1 >= tokc) continue;
        jsmntok_t* val = &toks[k + 1];
        if (tok_is_key(json, &toks[k], "type") && val->type == JSMN_STRING) { type = tok_copy(json, val); }
        if (tok_is_key(json, &toks[k], "style") && val->type == JSMN_STRING) { style_name = tok_copy(json, val); }
        if (tok_is_key(json, &toks[k], "spacing")) spacing = parse_number(json, val, spacing);
        if (tok_is_key(json, &toks[k], "columns")) columns = (int)parse_number(json, val, (float)columns);
    }

    if (type && (strcmp(type, "row") == 0 || strcmp(type, "column") == 0 || strcmp(type, "table") == 0)) {
        place_child_widgets(list, json, toks, tokc, start_idx, model, styles, base_x, base_y, type, spacing, columns);
        free(type); free(style_name);
        return;
    }

    w = create_widget();
    if (!w) { free(type); free(style_name); return; }

    for (unsigned int k = start_idx + 1; k < tokc && toks[k].start >= obj->start && toks[k].end <= obj->end; k++) {
        if (toks[k].type != JSMN_STRING || k + 1 >= tokc) continue;
        jsmntok_t* val = &toks[k + 1];
        if (tok_is_key(json, &toks[k], "type") && val->type == JSMN_STRING) {
            char* s = tok_copy(json, val);
            if (strcmp(s, "panel") == 0) w->type = W_PANEL;
            else if (strcmp(s, "label") == 0) w->type = W_LABEL;
            else if (strcmp(s, "button") == 0) w->type = W_BUTTON;
            else if (strcmp(s, "hslider") == 0) w->type = W_HSLIDER;
            free(s);
        }
        else if (tok_is_key(json, &toks[k], "x")) w->rect.x = base_x + parse_number(json, val, w->rect.x);
        else if (tok_is_key(json, &toks[k], "y")) w->rect.y = base_y + parse_number(json, val, w->rect.y);
        else if (tok_is_key(json, &toks[k], "w")) w->rect.w = parse_number(json, val, w->rect.w);
        else if (tok_is_key(json, &toks[k], "h")) w->rect.h = parse_number(json, val, w->rect.h);
        else if (tok_is_key(json, &toks[k], "id") && val->type == JSMN_STRING) w->id = tok_copy(json, val);
        else if (tok_is_key(json, &toks[k], "text") && val->type == JSMN_STRING) w->text = tok_copy(json, val);
        else if (tok_is_key(json, &toks[k], "textBinding") && val->type == JSMN_STRING) w->text_binding = tok_copy(json, val);
        else if (tok_is_key(json, &toks[k], "valueBinding") && val->type == JSMN_STRING) w->value_binding = tok_copy(json, val);
        else if (tok_is_key(json, &toks[k], "min")) w->minv = parse_number(json, val, w->minv);
        else if (tok_is_key(json, &toks[k], "max")) w->maxv = parse_number(json, val, w->maxv);
        else if (tok_is_key(json, &toks[k], "value")) w->value = parse_number(json, val, w->value);
        else if (tok_is_key(json, &toks[k], "color")) {
            Color col = w->color;
            read_color_array(&col, json, val, toks, tokc);
            w->color = col;
        }
    }

    if (style_name) apply_style(w, styles, style_name);
    bind_from_model(w, model);
    append_widget(list, w);
    free(type);
    free(style_name);
}

Widget* parse_layout_json(const char* json, const Model* model, const Style* styles) {
    if (!json) { fprintf(stderr, "Error: layout JSON text is null\n"); return NULL; }
    Widget* widgets = NULL;
    jsmn_parser p; jsmn_init(&p);
    size_t tokc = 4096;
    jsmntok_t* toks = (jsmntok_t*)malloc(sizeof(jsmntok_t) * tokc);
    if (!toks) { fprintf(stderr, "Error: failed to allocate tokens for layout JSON\n"); return NULL; }
    for (size_t i = 0; i < tokc; i++) { toks[i].start = toks[i].end = -1; toks[i].size = 0; toks[i].type = JSMN_UNDEFINED; }
    int parse_ret = jsmn_parse(&p, json, strlen(json), toks, tokc);
    if (parse_ret < 0) { fprintf(stderr, "Error: failed to parse layout JSON (code %d)\n", parse_ret); free(toks); return NULL; }

    int sections_found = 0;
    int widgets_created = 0;
    for (unsigned int i = 0; i < p.toknext; i++) {
        if (tok_is_key(json, &toks[i], "layout")) {
            sections_found++;
            if (i + 1 < p.toknext && toks[i + 1].type == JSMN_OBJECT) {
                Widget* prev = widgets;
                parse_widget_object(&widgets, json, toks, p.toknext, i + 1, model, styles, 0.0f, 0.0f);
                if (widgets != prev) widgets_created = 1;
            }
            else {
                fprintf(stderr, "Error: 'layout' section is not an object in layout.json\n");
            }
        }
        if (tok_is_key(json, &toks[i], "floating") && i + 1 < p.toknext && toks[i + 1].type == JSMN_ARRAY) {
            sections_found++;
            jsmntok_t* arr = &toks[i + 1];
            for (unsigned int j = i + 2; j < p.toknext && toks[j].start >= arr->start && toks[j].end <= arr->end; ) {
                if (toks[j].type == JSMN_OBJECT) {
                    Widget* prev = widgets;
                    parse_widget_object(&widgets, json, toks, p.toknext, j, model, styles, 0.0f, 0.0f);
                    if (widgets != prev) widgets_created = 1;
                }
                else {
                    fprintf(stderr, "Warning: non-object entry inside 'floating' array ignored\n");
                }
                j = skip_container(toks, p.toknext, j);
            }
        }
    }
    if (sections_found == 0) fprintf(stderr, "Error: no 'layout' or 'floating' sections found in layout.json\n");
    if (!widgets_created) fprintf(stderr, "Error: no widgets could be constructed from layout.json\n");
    free(toks);
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
        free(widgets);
        widgets = n;
    }
}
