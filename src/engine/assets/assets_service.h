#ifndef ASSETS_SYSTEM_H
#define ASSETS_SYSTEM_H

#include <stdbool.h>
#include "foundation/config/config_document.h"

// The data structure (Context)
typedef struct Assets {
    char* ui_path;
    char* vert_spv_path;
    char* frag_spv_path;
    char* font_path;

    ConfigDocument ui_doc;
} Assets;

// Public API
bool assets_init(Assets* assets, const char* assets_dir, const char* ui_config_path);
void assets_shutdown(Assets* assets);

#endif // ASSETS_SYSTEM_H
