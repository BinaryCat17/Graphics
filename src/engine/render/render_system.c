#include "engine/render/render_system.h"
#include "foundation/logger/logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "foundation/platform/platform.h"
#include "engine/render/backend/common/renderer_backend.h"
#include "engine/render/backend/vulkan/vulkan_renderer.h"
#include "engine/ui/ui_scene_bridge.h"

// --- Helper: Packet Management ---

static void render_packet_free_resources(RenderFramePacket* packet) {
    if (!packet) return;
    // ui_draw_list_clear(&packet->ui_draw_list); // Removed legacy
    scene_clear(&packet->scene);
}

static void try_sync_packet(RenderSystem* sys) {
    if (!sys || !sys->ui_root_view) return;
    
    // 1. Unified Scene Generation
    // We need to populate sys->packets[sys->back_packet_index].scene
    
    mtx_lock(&sys->packet_mutex);
    
    RenderFramePacket* dest = &sys->packets[sys->back_packet_index];
    
    // Clear old scene
    render_packet_free_resources(dest);
    
    // Bridge: Convert UI View to Scene Objects
    if (sys->assets) {
        ui_build_scene(sys->ui_root_view, &dest->scene, sys->assets);
    }

    sys->packet_ready = true;
    mtx_unlock(&sys->packet_mutex);
}

const RenderFramePacket* render_system_acquire_packet(RenderSystem* sys) {
    if (!sys) return NULL;

    mtx_lock(&sys->packet_mutex);
    if (sys->packet_ready) {
        int temp = sys->front_packet_index;
        sys->front_packet_index = sys->back_packet_index;
        sys->back_packet_index = temp;
        sys->packet_ready = false;
    }
    mtx_unlock(&sys->packet_mutex);

    return &sys->packets[sys->front_packet_index];
}

// --- Init & Bootstrap ---

static void try_bootstrap_renderer(RenderSystem* sys) {
    if (!sys) return;
    if (sys->renderer_ready) return;
    
    // Dependencies Check
    if (!sys->render_context.window) return;
    if (!sys->assets || !sys->assets->unified_vert_spv) return;
    // We don't strictly need UI bound to start renderer, but it helps.
    if (!sys->backend) return;

    // Create init config
    // Note: Old backend might expect WidgetArray. We need to update backend interface too!
    // For now, we are breaking the backend interface. I will need to update vulkan backend next.
    // I will pass NULL/Empty for now to avoid compilation errors on old structs if they persist.
    
    RenderBackendInit init = {
        .window = sys->render_context.window,
        .surface = &sys->render_context.surface,
        .get_required_instance_extensions = platform_get_required_vulkan_instance_extensions,
        .create_surface = platform_create_vulkan_surface,
        .destroy_surface = platform_destroy_vulkan_surface,
        .get_framebuffer_size = platform_get_framebuffer_size,
        .wait_events = platform_wait_events,
        .poll_events = platform_poll_events,
        .vert_spv = sys->assets->unified_vert_spv,
        .frag_spv = sys->assets->unified_frag_spv,
        .font_path = sys->assets->font_path,
        // .widgets = ... (Removed)
        // .display_list = ... (Removed)
        // .transformer = ... (Removed from Init, passed in Frame/Scene)
        .logger_config = &sys->logger_config,
    };

    sys->renderer_ready = sys->backend->init(sys->backend, &init);
}

bool render_system_init(RenderSystem* sys, const RenderSystemConfig* config) {
    if (!sys) return false;
    
    memset(sys, 0, sizeof(RenderSystem));
    mtx_init(&sys->packet_mutex, mtx_plain);
    sys->back_packet_index = 1;

    // Register Backend
    renderer_backend_register(vulkan_renderer_backend());
    const char* backend_id = config ? config->backend_type : "vulkan";
    sys->backend = renderer_backend_get(backend_id);
    if (!sys->backend) {
        LOG_ERROR("RenderSystem: Failed to load backend '%s'", backend_id);
        return false;
    }

    if (config) {
        sys->logger_config.level = config->log_level;
        sys->logger_config.sink_type = RENDER_LOG_SINK_STDOUT;
    } else {
        sys->logger_config.level = RENDER_LOG_INFO;
    }

    if (!runtime_init(sys)) {
        return false;
    }

    return true;
}

void render_system_shutdown(RenderSystem* sys) {
    if (!sys) return;
    
    if (sys->backend && sys->backend->cleanup) {
        sys->backend->cleanup(sys->backend);
    }
    
    runtime_shutdown(&sys->render_context);
    
    render_packet_free_resources(&sys->packets[0]);
    render_packet_free_resources(&sys->packets[1]);
    mtx_destroy(&sys->packet_mutex);
}

void render_system_bind_assets(RenderSystem* sys, Assets* assets) {
    sys->assets = assets;
    try_bootstrap_renderer(sys);
}

void render_system_bind_ui(RenderSystem* sys, UiView* root_view) {
    sys->ui_root_view = root_view;
    try_bootstrap_renderer(sys);
}

void render_system_bind_math_graph(RenderSystem* sys, MathGraph* graph) {
    sys->math_graph = graph;
}

void render_system_update(RenderSystem* sys) {
    if (!sys || !sys->renderer_ready) return;
    try_sync_packet(sys);
}

void render_system_run(RenderSystem* sys) {
    if (!sys) return;
    
    sys->running = true;
    
    while (sys->running && !platform_window_should_close(sys->render_context.window)) {
        platform_poll_events();
        
        // 1. Update Domain Logic
        if (sys->math_graph) {
             // math_graph_update(sys->math_graph);
        }
        
        // 2. Update UI Logic (Reactivity)
        if (sys->ui_root_view) {
            ui_view_process_input(sys->ui_root_view, &sys->input);
            ui_view_update(sys->ui_root_view);
        }

        // Reset frame events
        sys->input.mouse_clicked = false;

        // 3. Render Prep
        render_system_update(sys); // Generates DrawList and pushes to packet
        
        // 4. Draw
        if (sys->renderer_ready && sys->backend) {
             const RenderFramePacket* packet = render_system_acquire_packet(sys);
             if (packet) {
                if (sys->backend->render_scene) {
                    sys->backend->render_scene(sys->backend, &packet->scene);
                }
             }
        }
    }
}