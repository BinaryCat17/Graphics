#ifndef SCENE_SERVICE_H
#define SCENE_SERVICE_H

#include <stdbool.h>

#include "app/app_services.h"
#include "service.h"

bool scene_service_load(AppServices* services, const ServiceConfig* config);
void scene_service_unload(AppServices* services);
const ServiceDescriptor* scene_service_descriptor(void);

#endif // SCENE_SERVICE_H
