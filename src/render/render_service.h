#ifndef RENDER_SERVICE_H
#define RENDER_SERVICE_H

#include <stdbool.h>

#include "Graphics.h"
#include "assets/assets.h"
#include "render/render_context.h"
#include "ui/ui_context.h"
#include "ui/widgets.h"

bool render_service_init(RenderRuntimeContext* render, const Assets* assets, WidgetArray widgets);
void render_service_update_transformer(RenderRuntimeContext* render);
void render_loop(RenderRuntimeContext* render, UiContext* ui, Model* model);
void render_service_shutdown(RenderRuntimeContext* render);

#endif // RENDER_SERVICE_H
