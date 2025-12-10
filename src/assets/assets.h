#ifndef ASSETS_H
#define ASSETS_H

#include "config/config_document.h"

typedef struct Assets {
    char* model_path;
    char* layout_path;
    char* styles_path;
    char* vert_spv_path;
    char* frag_spv_path;
    char* font_path;

    ConfigDocument model_doc;
    ConfigDocument layout_doc;
    ConfigDocument styles_doc;
} Assets;

int load_assets(const char* assets_dir, Assets* out_assets);
void free_assets(Assets* assets);

#endif // ASSETS_H
