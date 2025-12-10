#ifndef RENDER_SERVICE_H
#define RENDER_SERVICE_H

#include <stdbool.h>

#include "core/Graphics.h"
#include "assets/assets.h"
#include "render/render_context.h"
#include "state/state_manager.h"
#include "ui/ui_context.h"
#include "ui/ui_json.h"

bool render_service_bind(RenderRuntimeContext* render, StateManager* state_manager, int assets_type_id, int ui_type_id,
                        int model_type_id);
void render_service_update_transformer(RenderRuntimeContext* render);
void render_loop(RenderRuntimeContext* render, StateManager* state_manager);
void render_service_shutdown(RenderRuntimeContext* render);

#endif // RENDER_SERVICE_H
