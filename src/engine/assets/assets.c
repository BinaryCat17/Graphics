#include "engine/assets/assets.h"
#include "engine/assets/internal/assets_internal.h"
#include "engine/graphics/primitives.h"
#include "foundation/platform/platform.h"
#include "foundation/platform/fs.h"
#include "foundation/logger/logger.h"
#include "foundation/memory/arena.h"

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

    // Create Unit Quad (0,0 to 1,1)
    // De-interleave standardized primitive data
    size_t pos_size = PRIM_QUAD_VERTEX_COUNT * 3 * sizeof(float);
    size_t uv_size = PRIM_QUAD_VERTEX_COUNT * 2 * sizeof(float);
    size_t idx_size = PRIM_QUAD_INDEX_COUNT * sizeof(unsigned int);

    out_assets->unit_quad.positions = (float*)arena_alloc(&out_assets->arena, pos_size);
    out_assets->unit_quad.uvs = (float*)arena_alloc(&out_assets->arena, uv_size);
    out_assets->unit_quad.indices = (unsigned int*)arena_alloc(&out_assets->arena, idx_size);

    if (out_assets->unit_quad.positions && out_assets->unit_quad.uvs && out_assets->unit_quad.indices) {
        for (int i = 0; i < PRIM_QUAD_VERTEX_COUNT; ++i) {
            int src_idx = i * PRIM_VERTEX_STRIDE;
            out_assets->unit_quad.positions[i*3 + 0] = PRIM_QUAD_VERTS[src_idx + 0];
            out_assets->unit_quad.positions[i*3 + 1] = PRIM_QUAD_VERTS[src_idx + 1];
            out_assets->unit_quad.positions[i*3 + 2] = PRIM_QUAD_VERTS[src_idx + 2];
            
            out_assets->unit_quad.uvs[i*2 + 0] = PRIM_QUAD_VERTS[src_idx + 3];
            out_assets->unit_quad.uvs[i*2 + 1] = PRIM_QUAD_VERTS[src_idx + 4];
        }
        memcpy(out_assets->unit_quad.indices, PRIM_QUAD_INDICES, idx_size);
        
        out_assets->unit_quad.position_count = PRIM_QUAD_VERTEX_COUNT * 3;
        out_assets->unit_quad.uv_count = PRIM_QUAD_VERTEX_COUNT * 2;
        out_assets->unit_quad.index_count = PRIM_QUAD_INDEX_COUNT;
    } else {
        LOG_ERROR("Assets: Failed to allocate memory for unit quad.");
    }

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

Mesh* assets_get_unit_quad(Assets* assets) {
    return &assets->unit_quad;
}
