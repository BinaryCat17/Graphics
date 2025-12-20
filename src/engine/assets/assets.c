#include "engine/assets/assets.h"
#include "engine/assets/internal/assets_internal.h"
#include "foundation/platform/platform.h"
#include "foundation/platform/fs.h"
#include "foundation/logger/logger.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

AssetData assets_load_file(const Assets* assets, const char* relative_path) {
    AssetData ad = {0};
    if (!assets || !relative_path) return ad;
    
    // Construct full path
    // We use a small temporary buffer on stack, assuming paths are reasonable
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s/%s", assets->root_dir, relative_path);
    
    ad.data = fs_read_bin(NULL, full_path, &ad.size);
    if (!ad.data) {
        LOG_ERROR("Assets: Failed to load file '%s'", full_path);
    }
    return ad;
}

void assets_free_file(AssetData* data) {
    if (data && data->data) {
        free(data->data);
        data->data = NULL;
        data->size = 0;
    }
}

bool assets_init_internal(Assets* out_assets, const char* assets_dir) {
    if (!out_assets) return false;

    memset(out_assets, 0, sizeof(*out_assets));

    // Initialize Arena (4KB is likely enough for just paths)
    if (!arena_init(&out_assets->arena, 4096)) {
        LOG_FATAL("Assets: Failed to initialize memory arena.");
        return false;
    }

    out_assets->root_dir = arena_push_string(&out_assets->arena, assets_dir);
    out_assets->font_path = arena_sprintf(&out_assets->arena, "%s/fonts/font.ttf", assets_dir);

    // Create Unit Quad (0,0 to 1,1)
    static float quad_verts[] = {
        0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        1.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f
    };
    static unsigned int quad_indices[] = {
        0, 1, 2, 0, 2, 3
    };
    
    out_assets->unit_quad.positions = quad_verts;
    out_assets->unit_quad.position_count = 12; 
    out_assets->unit_quad.indices = quad_indices;
    out_assets->unit_quad.index_count = 6;

    LOG_INFO("Assets: Initialized with root '%s'", assets_dir);
    return true;
}

Assets* assets_create(const char* assets_dir) {
    Assets* assets = (Assets*)calloc(1, sizeof(Assets));
    if (!assets) return NULL;
    
    if (!assets_init_internal(assets, assets_dir)) {
        free(assets);
        return NULL;
    }
    return assets;
}

void assets_destroy(Assets* assets) {
    if (!assets) return;
    arena_destroy(&assets->arena);
    free(assets);
}

const char* assets_get_root_dir(const Assets* assets) {
    return assets->root_dir;
}

const char* assets_get_font_path(const Assets* assets) {
    return assets->font_path;
}

Mesh* assets_get_unit_quad(Assets* assets) {
    return &assets->unit_quad;
}