#ifndef ASSETS_SYSTEM_H
#define ASSETS_SYSTEM_H

#include <stdbool.h>
#include <stddef.h>

typedef struct Assets Assets;
typedef struct Mesh Mesh;
typedef struct Font Font;

typedef struct AssetData {
    void* data;
    size_t size;
} AssetData;

// Public API
Assets* assets_create(const char* assets_dir);
void assets_destroy(Assets* assets);

// Scene Loading (Cached)
// Path is relative to assets root (e.g. "ui/node.yaml")
typedef struct SceneAsset SceneAsset;
SceneAsset* assets_load_scene(Assets* assets, const char* relative_path);

// I/O
AssetData assets_load_file(const Assets* assets, const char* relative_path);
void assets_free_file(AssetData* data);

// Accessors
const char* assets_get_root_dir(const Assets* assets);
const Mesh* assets_get_unit_quad(const Assets* assets);
const Font* assets_get_font(const Assets* assets);

#endif // ASSETS_SYSTEM_H
