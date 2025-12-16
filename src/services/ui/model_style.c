#include "services/ui/model_style.h"
#include "core/platform/platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const Style DEFAULT_STYLE = {
    .name = NULL,
    .background = {0.12f, 0.16f, 0.24f, 0.96f},
    .text = {0.94f, 0.97f, 1.0f, 1.0f},
    .border_color = {0.33f, 0.56f, 0.88f, 1.0f},
    .scrollbar_track_color = {0.16f, 0.25f, 0.36f, 0.9f},
    .scrollbar_thumb_color = {0.58f, 0.82f, 1.0f, 1.0f},
    .padding = 10.0f,
    .border_thickness = 2.0f,
    .scrollbar_width = 0.0f,
    .has_scrollbar_width = 0,
    .next = NULL
};

static const Style ROOT_STYLE = { .name = NULL, .background = {0.0f, 0.0f, 0.0f, 0.0f}, .text = {1.0f, 1.0f, 1.0f, 1.0f},
                                  .border_color = {1.0f, 1.0f, 1.0f, 0.0f}, .scrollbar_track_color = {0.6f, 0.6f, 0.6f, 0.4f},
                                  .scrollbar_thumb_color = {1.0f, 1.0f, 1.0f, 0.7f}, .padding = 0.0f, .border_thickness = 0.0f,
                                  .scrollbar_width = 0.0f, .has_scrollbar_width = 0, .next = NULL };

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
    e->key = platform_strdup(key);
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
    e->string_value = platform_strdup(value);
    e->is_string = 1;
}

int save_model(const Model* model) {
    if (!model || !model->source_path) return 0;
    FILE* f = platform_fopen(model->source_path, "wb");
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

const Style* ui_default_style(void) { return &DEFAULT_STYLE; }
const Style* ui_root_style(void) { return &ROOT_STYLE; }

