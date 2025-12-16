#ifndef RENDER_SYSTEM_H
#define RENDER_SYSTEM_H

#include <stdbool.h>
#include <threads.h>

#include "engine/assets/assets_service.h"
#include "engine/ui/ui_context.h"
#include "engine/render/backend/common/renderer_backend.h"
#include "engine/render/render_thread.h"

// Forward decl
struct RendererBackend;

typedef struct RenderFramePacket {
    WidgetArray widgets;
    DisplayList display_list;
    CoordinateSystem2D transformer;
} RenderFramePacket;

typedef struct RenderSystem {
    // Dependencies (Injectable)
    Assets* assets;
    UiContext* ui;
    Model* model;

    // Internal State
    RenderRuntimeContext render_context; // Thread context
    struct RendererBackend* backend;
    
    // Packet buffering
    RenderFramePacket packets[2];
    int front_packet_index;
    int back_packet_index;
    bool packet_ready;
    mtx_t packet_mutex;
    
    // UI Cache
    WidgetArray widgets;
    DisplayList display_list; // Persistent allocation
    
    // Config
    RenderLoggerConfig logger_config;
    
    // Thread control
    thrd_t thread;
    bool running;
    bool renderer_ready;
} RenderSystem;

typedef struct RenderSystemConfig {
    const char* backend_type; // "vulkan"
    bool render_log_enabled;
} RenderSystemConfig;

bool render_system_init(RenderSystem* sys, const RenderSystemConfig* config);
void render_system_shutdown(RenderSystem* sys);

// Connect dependencies
void render_system_bind_assets(RenderSystem* sys, Assets* assets);
void render_system_bind_ui(RenderSystem* sys, UiContext* ui);
void render_system_bind_model(RenderSystem* sys, Model* model);

// Runtime
void render_system_update(RenderSystem* sys); // Called on main thread
void render_system_update_transformer(RenderSystem* sys);
void render_system_run(RenderSystem* sys);    // Called on main thread (blocking loop) or starts thread

const RenderFramePacket* render_system_acquire_packet(RenderSystem* sys);

#endif // RENDER_SYSTEM_H
