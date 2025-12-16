#include "engine/assets/assets_service.h"
#include "foundation/platform/platform.h"
#include "foundation/logger/logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void free_paths(Assets* assets) {
    free(assets->vert_spv_path);
    free(assets->frag_spv_path);
    free(assets->font_path);
    assets->vert_spv_path = NULL;
    assets->frag_spv_path = NULL;
    assets->font_path = NULL;
}

bool assets_init(Assets* out_assets, const char* assets_dir, const char* ui_config_path) {
    if (!out_assets) return false;
    (void)ui_config_path; // Unused in modern system

    memset(out_assets, 0, sizeof(*out_assets));

    out_assets->vert_spv_path = join_path(assets_dir, "shaders/shader.vert.spv");
    out_assets->frag_spv_path = join_path(assets_dir, "shaders/shader.frag.spv");
    out_assets->font_path = join_path(assets_dir, "fonts/font.ttf");

    if (!out_assets->vert_spv_path || !out_assets->frag_spv_path || !out_assets->font_path) {
        LOG_FATAL("Failed to compose asset paths for directory '%s'", assets_dir);
        assets_shutdown(out_assets);
        return false;
    }

    return true;
}

void assets_shutdown(Assets* assets) {
    if (!assets) return;
    free_paths(assets);
    memset(assets, 0, sizeof(*assets));
}
