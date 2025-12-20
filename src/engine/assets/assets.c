#include "engine/assets/assets.h"
#include "foundation/platform/platform.h"
#include "foundation/logger/logger.h"

#include <string.h>

bool assets_init(Assets* out_assets, const char* assets_dir, const char* ui_config_path) {
    if (!out_assets) return false;
    (void)ui_config_path; // Unused in modern system

    memset(out_assets, 0, sizeof(*out_assets));

    // Initialize Arena (4KB is likely enough for just paths)
    if (!arena_init(&out_assets->arena, 4096)) {
        LOG_FATAL("Assets: Failed to initialize memory arena.");
        return false;
    }

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

void assets_shutdown(Assets* assets) {
    if (!assets) return;
    arena_destroy(&assets->arena);
    memset(assets, 0, sizeof(*assets));
}