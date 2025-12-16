#ifndef SCENE_SERVICE_H
#define SCENE_SERVICE_H

#include <stdbool.h>

#include "services/core/core_context.h"
#include "core/service_manager/service.h"
#include "core/state/state_manager.h"

bool scene_service_load(CoreContext* core, StateManager* state_manager, int scene_type_id, int assets_type_id,
                        int model_type_id, const ServiceConfig* config);
void scene_service_unload(CoreContext* core);
const ServiceDescriptor* scene_service_descriptor(void);

#endif // SCENE_SERVICE_H
