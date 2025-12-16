#ifndef RENDER_RUNTIME_SERVICE_H
#define RENDER_RUNTIME_SERVICE_H

#include <stdbool.h>

#include <threads.h>

#include "app/app_services.h"
#include "services/assets/assets.h"
#include "services/render/backend/common/renderer_backend.h"
#include "services/render/backend/common/render_context.h"
#include "services/manager/service.h"
#include "core/state/state_manager.h"
#include "services/ui/ui_context.h"

typedef struct RenderFramePacket {
    WidgetArray widgets;
    DisplayList display_list;
    CoordinateTransformer transformer;
} RenderFramePacket;

typedef struct RenderRuntimeServiceContext {
    RenderRuntimeContext* render;
    const Assets* assets;
    UiContext* ui;
    
    // Packet / Double Buffering State
    RenderFramePacket packets[2];
    int front_packet_index; // Read by Render Thread
    int back_packet_index;  // Written by Main Thread
    mtx_t packet_mutex;
    bool packet_ready;

    // Legacy/Direct access fields (to be deprecated or used for filling packets)
    WidgetArray widgets;
    DisplayList display_list; // Kept for Main Thread state
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

// Thread-safe packet consumption for Render Service
const RenderFramePacket* render_runtime_service_acquire_packet(RenderRuntimeServiceContext* context);

#endif // RENDER_RUNTIME_SERVICE_H
