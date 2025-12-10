#include "assets.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "module_yaml_loader.h"

static char* join_path(const char* dir, const char* leaf) {
    if (!dir || !leaf) return NULL;
    size_t dir_len = strlen(dir);
    while (dir_len > 0 && dir[dir_len - 1] == '/') dir_len--;
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

static char* read_file_text(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Failed open %s\n", path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* b = (char*)malloc((size_t)len + 1);
    if (!b) {
        fclose(f);
        return NULL;
    }

    fread(b, 1, (size_t)len, f);
    b[len] = 0;
    fclose(f);
    return b;
}

static void free_paths(Assets* assets) {
    free(assets->model_path);
    free(assets->layout_path);
    free(assets->styles_path);
    free(assets->vert_spv_path);
    free(assets->frag_spv_path);
    free(assets->font_path);
}

static void free_texts(Assets* assets) {
    free(assets->model_text);
    free(assets->layout_text);
    free(assets->styles_text);
}

int load_assets(const char* assets_dir, Assets* out_assets) {
    if (!out_assets) return 0;

    memset(out_assets, 0, sizeof(*out_assets));

    out_assets->model_path = join_path(assets_dir, "ui/config/model.yaml");
    out_assets->layout_path = join_path(assets_dir, "ui/config/layout.yaml");
    out_assets->styles_path = join_path(assets_dir, "ui/config/styles.yaml");
    out_assets->vert_spv_path = join_path(assets_dir, "shaders/shader.vert.spv");
    out_assets->frag_spv_path = join_path(assets_dir, "shaders/shader.frag.spv");
    out_assets->font_path = join_path(assets_dir, "font.ttf");

    if (!out_assets->model_path || !out_assets->layout_path || !out_assets->styles_path ||
        !out_assets->vert_spv_path || !out_assets->frag_spv_path || !out_assets->font_path) {
        fprintf(stderr, "Fatal: failed to compose asset paths for directory '%s'\n", assets_dir);
        free_assets(out_assets);
        return 0;
    }

    SimpleYamlError err = {0};
    if (!load_yaml_file_as_json(out_assets->model_path, &out_assets->model_text, &err)) {
        fprintf(stderr, "Failed to load %s: %s\n", out_assets->model_path, err.message);
        free_assets(out_assets);
        return 0;
    }
    err = (SimpleYamlError){0};
    if (!load_yaml_file_as_json(out_assets->layout_path, &out_assets->layout_text, &err)) {
        fprintf(stderr, "Failed to load %s: %s\n", out_assets->layout_path, err.message);
        free_assets(out_assets);
        return 0;
    }
    err = (SimpleYamlError){0};
    if (!load_yaml_file_as_json(out_assets->styles_path, &out_assets->styles_text, &err)) {
        fprintf(stderr, "Failed to load %s: %s\n", out_assets->styles_path, err.message);
        free_assets(out_assets);
        return 0;
    }
    return 1;
}

void free_assets(Assets* assets) {
    if (!assets) return;
    free_paths(assets);
    free_texts(assets);
}
