#ifndef ASSETS_SYSTEM_H
#define ASSETS_SYSTEM_H

#include <stdbool.h>
#include "engine/scene/scene.h"

typedef struct Assets Assets;

typedef struct AssetData {
    void* data;
    size_t size;
} AssetData;

// Public API
Assets* assets_create(const char* assets_dir);
void assets_destroy(Assets* assets);

// I/O
AssetData assets_load_file(const Assets* assets, const char* relative_path);
void assets_free_file(AssetData* data);

// Accessors
const char* assets_get_root_dir(const Assets* assets);
Mesh* assets_get_unit_quad(Assets* assets);

#endif // ASSETS_SYSTEM_H
