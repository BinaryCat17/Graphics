#ifndef RENDER_RUNTIME_SERVICE_H
#define RENDER_RUNTIME_SERVICE_H

#include <stdbool.h>

#include "app/app_services.h"
#include "assets/assets.h"
#include "render/common/renderer_backend.h"
#include "render/common/render_context.h"
#include "services/service.h"
#include "state/state_manager.h"
#include "ui/ui_context.h"

typedef struct RenderRuntimeServiceContext {
    RenderRuntimeContext* render;
    const Assets* assets;
    UiContext* ui;
    WidgetArray widgets;
    DisplayList display_list;
    Model* model;
    StateManager* state_manager;
    int assets_type_id;
    int ui_type_id;
    int model_type_id;
    int render_ready_type_id;
    bool renderer_ready;
    bool render_ready;
    RendererBackend* backend;
    RenderLoggerConfig logger_config;
} RenderRuntimeServiceContext;

const ServiceDescriptor* render_runtime_service_descriptor(void);
RenderRuntimeServiceContext* render_runtime_service_context(const ServiceDescriptor* descriptor);
void render_runtime_service_update_transformer(RenderRuntimeServiceContext* context, RenderRuntimeContext* render);
bool render_runtime_service_prepare(RenderRuntimeServiceContext* context);

#endif // RENDER_RUNTIME_SERVICE_H
