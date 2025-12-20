#ifndef RENDER_SYSTEM_H
#define RENDER_SYSTEM_H

#include <stdbool.h>

#include "engine/assets/assets.h"
#include "engine/ui/ui_core.h"
#include "engine/graphics/scene/render_packet.h"
#include "foundation/platform/platform.h"

typedef struct RenderSystem RenderSystem;

typedef struct RenderSystemConfig {
    PlatformWindow* window;
    const char* backend_type; // "vulkan"
} RenderSystemConfig;

RenderSystem* render_system_create(const RenderSystemConfig* config);
void render_system_destroy(RenderSystem* sys);

// Connect dependencies
void render_system_bind_assets(RenderSystem* sys, Assets* assets);
void render_system_bind_ui(RenderSystem* sys, UiElement* root_view);

// Updates the render system (Syncs logic to render packet)
void render_system_update(RenderSystem* sys);

// Begins a new frame (updates time and frame count)
void render_system_begin_frame(RenderSystem* sys, double time);

// Draws the current packet (Executes backend render)
void render_system_draw(RenderSystem* sys);

// Notifies system of resize
void render_system_resize(RenderSystem* sys, int width, int height);

// Sets the active compute pipeline for the next frames
void render_system_set_compute_pipeline(RenderSystem* sys, uint32_t pipeline_id);
uint32_t render_system_create_compute_pipeline(RenderSystem* sys, uint32_t* spv_code, size_t spv_size);
void render_system_destroy_compute_pipeline(RenderSystem* sys, uint32_t pipeline_id);

// Request a screenshot to be saved to the specified path
void render_system_request_screenshot(RenderSystem* sys, const char* filepath);

const RenderFramePacket* render_system_acquire_packet(RenderSystem* sys);

// --- Public State Accessors (for read-only access if needed) ---
double render_system_get_time(RenderSystem* sys);
uint64_t render_system_get_frame_count(RenderSystem* sys);
bool render_system_is_ready(RenderSystem* sys);
void render_system_set_show_compute(RenderSystem* sys, bool show);

#endif // RENDER_SYSTEM_H
