#ifndef RENDER_SYSTEM_H
#define RENDER_SYSTEM_H

#include <stdbool.h>
#include <threads.h>

#include "engine/assets/assets.h"
#include "engine/ui/ui_def.h"
#include "domains/math_model/math_graph.h"
#include "engine/render/backend/common/renderer_backend.h"
#include "engine/render/render_thread.h"
#include "engine/render/render_packet.h"

// Forward decl
struct RendererBackend;

typedef struct RenderSystem {
    // Dependencies (Injectable)
    Assets* assets;
    UiView* ui_root_view;
    MathGraph* math_graph;

    // Internal State
    RenderRuntimeContext render_context; // Thread context
    struct RendererBackend* backend;
    
    // Input State
    InputState input;
    
    // Packet buffering
    RenderFramePacket packets[2];
    int front_packet_index;
    int back_packet_index;
    bool packet_ready;
    mtx_t packet_mutex;
    
    // Config
    RenderLoggerConfig logger_config;
    
    // Thread control
    thrd_t thread;
    bool running;
    bool renderer_ready;
    
    uint64_t frame_count;
} RenderSystem;

typedef struct RenderSystemConfig {
    const char* backend_type; // "vulkan"
    RenderLogLevel log_level;
} RenderSystemConfig;

bool render_system_init(RenderSystem* sys, const RenderSystemConfig* config);
void render_system_shutdown(RenderSystem* sys);

// Connect dependencies
void render_system_bind_assets(RenderSystem* sys, Assets* assets);
void render_system_bind_ui(RenderSystem* sys, UiView* root_view);
void render_system_bind_math_graph(RenderSystem* sys, MathGraph* graph);

// Runtime
void render_system_update(RenderSystem* sys); // Called on main thread
void render_system_run(RenderSystem* sys);    // Called on main thread (blocking loop)

const RenderFramePacket* render_system_acquire_packet(RenderSystem* sys);

#endif // RENDER_SYSTEM_H