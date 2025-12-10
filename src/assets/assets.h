#ifndef ASSETS_H
#define ASSETS_H

#include "config/config_document.h"

typedef struct Assets {
    char* ui_path;
    char* vert_spv_path;
    char* frag_spv_path;
    char* font_path;

    ConfigDocument ui_doc;
} Assets;

int load_assets(const char* assets_dir, const char* ui_config_path, Assets* out_assets);
void free_assets(Assets* assets);

#endif // ASSETS_H
