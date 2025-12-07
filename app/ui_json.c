#include "ui_json.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef enum { JSMN_UNDEFINED = 0, JSMN_OBJECT = 1, JSMN_ARRAY = 2, JSMN_STRING = 3, JSMN_PRIMITIVE = 4 } jsmntype_t;
typedef struct { jsmntype_t type; int start; int end; int size; } jsmntok_t;
typedef struct { unsigned int pos; unsigned int toknext; int toksuper; } jsmn_parser;

static void jsmn_init(jsmn_parser * p) { p->pos = 0; p->toknext = 0; p->toksuper = -1; }
static int jsmn_alloc(jsmn_parser * p, jsmntok_t * toks, size_t nt) {
    if (p->toknext >= nt) return -1;
    toks[p->toknext].start = toks[p->toknext].end = -1;
    toks[p->toknext].size = 0;
    toks[p->toknext].type = JSMN_UNDEFINED;
    return p->toknext++;
}
static int jsmn_parse(jsmn_parser * p, const char* js, size_t len, jsmntok_t * toks, size_t nt) {
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

static char* tok_copy(const char* js, jsmntok_t * t) {
    int n = t->end - t->start;
    char* s = (char*)malloc((size_t)n + 1);
    memcpy(s, js + t->start, (size_t)n);
    s[n] = 0; return s;
}

static void add_widget(Widget **list, Widget * w) {
    w->next = *list; *list = w;
}

Widget* parse_ui_json(const char* json) {
    if (!json) return NULL;
    Widget* widgets = NULL;
    jsmn_parser p; jsmn_init(&p);
    size_t tokc = 2048;
    jsmntok_t* toks = (jsmntok_t*)malloc(sizeof(jsmntok_t) * tokc);
    if (!toks) return NULL;
    for (size_t i = 0; i < tokc; i++) { toks[i].start = toks[i].end = -1; toks[i].size = 0; toks[i].type = JSMN_UNDEFINED; }
    if (jsmn_parse(&p, json, strlen(json), toks, tokc) < 0) { fprintf(stderr, "JSON parse problem\n"); free(toks); return NULL; }
    for (unsigned int i = 0; i < p.toknext; i++) {
        if (toks[i].type == JSMN_STRING) {
            char* k = tok_copy(json, &toks[i]);
            if (strcmp(k, "ui") == 0 || strcmp(k, "widgets") == 0) {
                free(k);
                if (i + 1 < p.toknext && toks[i + 1].type == JSMN_ARRAY) {
                    int arrstart = toks[i + 1].start, arrend = toks[i + 1].end;
                    for (unsigned int j = 0; j < p.toknext; j++) {
                        if (toks[j].type == JSMN_OBJECT && toks[j].start > arrstart && toks[j].end < arrend) {
                            Widget* w = (Widget*)calloc(1, sizeof(Widget));
                            if (!w) { free_widgets(widgets); widgets = NULL; break; }
                            w->color.r = w->color.g = w->color.b = 0.6f; w->color.a = 1.0f;
                            w->rect.x = w->rect.y = w->rect.w = w->rect.h = 0;
                            w->minv = 0; w->maxv = 1; w->value = 0;
                            unsigned int kidx = j + 1;
                            while (kidx < p.toknext && toks[kidx].start >= toks[j].start && toks[kidx].end <= toks[j].end) {
                                if (toks[kidx].type == JSMN_STRING) {
                                    char* key = tok_copy(json, &toks[kidx]);
                                    if (kidx + 1 >= p.toknext) { free(key); break; }
                                    jsmntok_t* val = &toks[kidx + 1];
                                    if (strcmp(key, "type") == 0 && val->type == JSMN_STRING) {
                                        char* s = tok_copy(json, val);
                                        if (strcmp(s, "panel") == 0) w->type = W_PANEL;
                                        else if (strcmp(s, "label") == 0) w->type = W_LABEL;
                                        else if (strcmp(s, "button") == 0) w->type = W_BUTTON;
                                        else if (strcmp(s, "hslider") == 0) w->type = W_HSLIDER;
                                        free(s);
                                    }
                                    else if (strcmp(key, "x") == 0) { char* s = tok_copy(json, val); w->rect.x = (float)atof(s); free(s); }
                                    else if (strcmp(key, "y") == 0) { char* s = tok_copy(json, val); w->rect.y = (float)atof(s); free(s); }
                                    else if (strcmp(key, "w") == 0) { char* s = tok_copy(json, val); w->rect.w = (float)atof(s); free(s); }
                                    else if (strcmp(key, "h") == 0) { char* s = tok_copy(json, val); w->rect.h = (float)atof(s); free(s); }
                                    else if (strcmp(key, "id") == 0 && val->type == JSMN_STRING) { char* s = tok_copy(json, val); w->id = s; }
                                    else if (strcmp(key, "text") == 0 && val->type == JSMN_STRING) { char* s = tok_copy(json, val); w->text = s; }
                                    else if (strcmp(key, "min") == 0) { char* s = tok_copy(json, val); w->minv = (float)atof(s); free(s); }
                                    else if (strcmp(key, "max") == 0) { char* s = tok_copy(json, val); w->maxv = (float)atof(s); free(s); }
                                    else if (strcmp(key, "value") == 0) { char* s = tok_copy(json, val); w->value = (float)atof(s); free(s); }
                                    else if (strcmp(key, "color") == 0 && val->type == JSMN_ARRAY) {
                                        float cols[4] = { 0.6f,0.6f,0.6f,1.0f }; int cc = 0;
                                        for (unsigned int z = kidx + 1; z < p.toknext && toks[z].start >= val->start && toks[z].end <= val->end; z++) {
                                            if (toks[z].type == JSMN_PRIMITIVE) {
                                                char* s = tok_copy(json, &toks[z]);
                                                cols[cc++] = (float)atof(s);
                                                free(s);
                                            }
                                        }
                                        w->color.r = cols[0]; w->color.g = cols[1]; w->color.b = cols[2];
                                    }
                                    free(key);
                                    kidx += 2;
                                }
                                else kidx++;
                            }
                            add_widget(&widgets, w);
                        }
                    }
                }
            }
            else free(k);
        }
    }
    free(toks);
    return widgets;
}

void free_widgets(Widget* widgets) {
    while (widgets) {
        Widget* n = widgets->next;
        free(widgets->text);
        free(widgets->id);
        free(widgets);
        widgets = n;
    }
}
