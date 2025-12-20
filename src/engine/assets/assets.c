#include "engine/assets/assets.h"
#include "engine/assets/internal/assets_internal.h"
#include "foundation/platform/platform.h"
#include "foundation/logger/logger.h"

#include <string.h>
#include <stdlib.h>

bool assets_init_internal(Assets* out_assets, const char* assets_dir) {
    if (!out_assets) return false;

    memset(out_assets, 0, sizeof(*out_assets));

    // Initialize Arena (4KB is likely enough for just paths)
    if (!arena_init(&out_assets->arena, 4096)) {
        LOG_FATAL("Assets: Failed to initialize memory arena.");
        return false;
    }

    out_assets->root_dir = arena_push_string(&out_assets->arena, assets_dir);
    out_assets->ui_default_vert_spv = arena_sprintf(&out_assets->arena, "%s/shaders/ui_default.vert.spv", assets_dir);
    out_assets->ui_default_frag_spv = arena_sprintf(&out_assets->arena, "%s/shaders/ui_default.frag.spv", assets_dir);
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

const char* assets_get_ui_default_vert_shader_path(const Assets* assets) {
    return assets->ui_default_vert_spv;
}

const char* assets_get_ui_default_frag_shader_path(const Assets* assets) {
    return assets->ui_default_frag_spv;
}

const char* assets_get_font_path(const Assets* assets) {
    return assets->font_path;
}

Mesh* assets_get_unit_quad(Assets* assets) {
    return &assets->unit_quad;
}