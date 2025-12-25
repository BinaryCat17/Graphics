#include "asset_loader.h"
#include "engine/graphics/internal/primitives.h"
#include "engine/text/font.h"
#include "foundation/platform/fs.h"
#include "foundation/logger/logger.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

AssetData asset_loader_read_file(const char* full_path) {
    AssetData ad = {0};
    if (!full_path) return ad;

    ad.data = fs_read_bin(NULL, full_path, &ad.size);
    if (!ad.data) {
        LOG_ERROR("Assets: Failed to load file '%s'", full_path);
    }
    return ad;
}

void asset_loader_free_file(AssetData* data) {
    if (data && data->data) {
        free(data->data);
        data->data = NULL;
        data->size = 0;
    }
}

void asset_loader_create_primitives(Assets* assets) {
    if (!assets) return;

    // Create Unit Quad (0,0 to 1,1)
    size_t pos_size = PRIM_QUAD_VERTEX_COUNT * 3 * sizeof(float);
    size_t uv_size = PRIM_QUAD_VERTEX_COUNT * 2 * sizeof(float);
    size_t idx_size = PRIM_QUAD_INDEX_COUNT * sizeof(unsigned int);

    assets->unit_quad.positions = (float*)arena_alloc(&assets->arena, pos_size);
    assets->unit_quad.uvs = (float*)arena_alloc(&assets->arena, uv_size);
    assets->unit_quad.indices = (unsigned int*)arena_alloc(&assets->arena, idx_size);

    if (assets->unit_quad.positions && assets->unit_quad.uvs && assets->unit_quad.indices) {
        for (int i = 0; i < PRIM_QUAD_VERTEX_COUNT; ++i) {
            int src_idx = i * PRIM_VERTEX_STRIDE;
            assets->unit_quad.positions[i*3 + 0] = PRIM_QUAD_VERTS[src_idx + 0];
            assets->unit_quad.positions[i*3 + 1] = PRIM_QUAD_VERTS[src_idx + 1];
            assets->unit_quad.positions[i*3 + 2] = PRIM_QUAD_VERTS[src_idx + 2];
            
            assets->unit_quad.uvs[i*2 + 0] = PRIM_QUAD_VERTS[src_idx + 3];
            assets->unit_quad.uvs[i*2 + 1] = PRIM_QUAD_VERTS[src_idx + 4];
        }
        memcpy(assets->unit_quad.indices, PRIM_QUAD_INDICES, idx_size);
        
        assets->unit_quad.position_count = PRIM_QUAD_VERTEX_COUNT * 3;
        assets->unit_quad.uv_count = PRIM_QUAD_VERTEX_COUNT * 2;
        assets->unit_quad.index_count = PRIM_QUAD_INDEX_COUNT;
    } else {
        LOG_ERROR("Assets: Failed to allocate memory for unit quad.");
    }
}

Font* asset_loader_load_font(const char* full_path) {
    AssetData font_data = asset_loader_read_file(full_path);
    Font* font = NULL;
    if (font_data.data) {
        font = font_create(font_data.data, font_data.size);
        if (!font) {
            LOG_ERROR("Assets: Failed to create font from '%s'", full_path);
        }
        asset_loader_free_file(&font_data);
    } else {
        LOG_WARN("Assets: Could not load font file '%s'. Text rendering will fail.", full_path);
    }
    return font;
}

SceneAsset* asset_loader_load_scene_from_disk(const char* full_path) {
    SceneAsset* asset = scene_asset_load_from_file(full_path);
    if (!asset) {
        LOG_ERROR("Assets: Failed to parse scene asset '%s'", full_path);
    }
    return asset;
}
