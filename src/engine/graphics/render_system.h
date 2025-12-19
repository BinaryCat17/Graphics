#ifndef RENDER_SYSTEM_H
#define RENDER_SYSTEM_H

#include <stdbool.h>
#include <threads.h>

#include "engine/assets/assets.h"
#include "engine/ui/ui_core.h"
#include "engine/graphics/backend/renderer_backend.h"
#include "engine/graphics/scene/render_packet.h"
#include "foundation/platform/platform.h"

// Forward decl
struct RendererBackend;

typedef struct RenderSystem {
    // Dependencies (Injectable)
    Assets* assets;
    UiElement* ui_root_view;

    // Internal State
    PlatformWindow* window;
    struct RendererBackend* backend;
    
    // Packet buffering
    RenderFramePacket packets[2];
    int front_packet_index;
    int back_packet_index;
    bool packet_ready;
    mtx_t packet_mutex;
    
    // Thread control
    bool running;
    bool renderer_ready;
    bool show_compute_result;
    uint32_t active_compute_pipeline;
    double current_time;
    
    uint64_t frame_count;
} RenderSystem;

typedef struct RenderSystemConfig {
    PlatformWindow* window;
    const char* backend_type; // "vulkan"
} RenderSystemConfig;

bool render_system_init(RenderSystem* sys, const RenderSystemConfig* config);
void render_system_shutdown(RenderSystem* sys);

// Connect dependencies
void render_system_bind_assets(RenderSystem* sys, Assets* assets);
void render_system_bind_ui(RenderSystem* sys, UiElement* root_view);

// Updates the render system (Syncs logic to render packet)
void render_system_update(RenderSystem* sys);

// Sets the active compute pipeline for the next frames
void render_system_set_compute_pipeline(RenderSystem* sys, uint32_t pipeline_id);

// Shuts down the system
void render_system_shutdown(RenderSystem* sys);

// Request a screenshot to be saved to the specified path
void render_system_request_screenshot(RenderSystem* sys, const char* filepath);

const RenderFramePacket* render_system_acquire_packet(RenderSystem* sys);

#endif // RENDER_SYSTEM_H
