#ifndef SCENE_SERVICE_H
#define SCENE_SERVICE_H

#include <stdbool.h>

#include "runtime/app_services.h"
#include "service.h"

bool scene_service_load(AppServices* services, const char* assets_dir, const char* scene_path);
void scene_service_unload(AppServices* services);
const ServiceDescriptor* scene_service_descriptor(void);

#endif // SCENE_SERVICE_H
