#ifndef SCENE_SERVICE_H
#define SCENE_SERVICE_H

#include <stdbool.h>

#include "runtime/app_services.h"

bool scene_service_load(AppServices* services, const char* assets_dir, const char* scene_path);
void scene_service_unload(AppServices* services);

#endif // SCENE_SERVICE_H
