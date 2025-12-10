#ifndef SCENE_SERVICE_H
#define SCENE_SERVICE_H

#include <stdbool.h>

#include "core/context.h"

bool scene_service_load(CoreContext* core, const char* assets_dir, const char* scene_path);
void scene_service_unload(CoreContext* core);

#endif // SCENE_SERVICE_H
