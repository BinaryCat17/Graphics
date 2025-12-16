#ifndef ASSETS_SERVICE_H
#define ASSETS_SERVICE_H

#include "core/config/config_document.h"
#include "core/service_manager/service.h"

// The data structure (Context)
typedef struct Assets {
    char* ui_path;
    char* vert_spv_path;
    char* frag_spv_path;
    char* font_path;

    ConfigDocument ui_doc;
} Assets;

// Service Descriptor
const ServiceDescriptor* assets_service_descriptor(void);

// Public API
int load_assets(const char* assets_dir, const char* ui_config_path, Assets* out_assets);
void free_assets(Assets* assets);

#endif // ASSETS_SERVICE_H