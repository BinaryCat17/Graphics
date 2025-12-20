#ifndef ASSETS_SYSTEM_H
#define ASSETS_SYSTEM_H

#include <stdbool.h>
#include "engine/scene/scene.h"

typedef struct Assets Assets;

// Public API
Assets* assets_create(const char* assets_dir);
void assets_destroy(Assets* assets);

// Accessors
const char* assets_get_root_dir(const Assets* assets);
const char* assets_get_ui_default_vert_shader_path(const Assets* assets);
const char* assets_get_ui_default_frag_shader_path(const Assets* assets);
const char* assets_get_font_path(const Assets* assets);
Mesh* assets_get_unit_quad(Assets* assets);

#endif // ASSETS_SYSTEM_H
