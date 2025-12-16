#include "engine/render/render_system.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "foundation/platform/platform.h"
#include "engine/render/backend/common/renderer_backend.h"
#include "engine/render/backend/vulkan/vulkan_renderer.h"
#include "engine/ui/ui_service.h"

// --- Helper: Packet Management ---

static void render_packet_free_resources(RenderFramePacket* packet) {
    if (!packet) return;
    if (packet->display_list.items) {
        free(packet->display_list.items);
        packet->display_list.items = NULL;
    }
    packet->display_list.count = 0;
}

static void try_sync_packet(RenderSystem* sys) {
    if (!sys) return;
    
    mtx_lock(&sys->packet_mutex);
    
    RenderFramePacket* dest = &sys->packets[sys->back_packet_index];
    
    // Copy UI Data
    dest->widgets = sys->widgets; // Shallow copy of container, data owned by UI system?
    // Wait, UI System owns widgets array memory. Is it thread safe?
    // UI updates widgets on Main Thread. Render reads on Render Thread.
    // We need a deep copy or double buffering of widgets array.
    // For now: assume Main Thread is blocked while we sync? No.
    // The previous implementation did shallow copy of structure.
    
    // Deep copy Display List
    render_packet_free_resources(dest);
    if (sys->display_list.items && sys->display_list.count > 0) {
        dest->display_list.items = (DisplayItem*)malloc(sys->display_list.count * sizeof(DisplayItem));
        if (dest->display_list.items) {
            memcpy(dest->display_list.items, sys->display_list.items, sys->display_list.count * sizeof(DisplayItem));
            dest->display_list.count = sys->display_list.count;
        }
    }

    // Copy Transformer
    dest->transformer = sys->render_context.transformer;

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
    if (!sys->assets || !sys->assets->vert_spv_path) return;
    if (!sys->ui) return; // Need widgets/display list initial state
    if (!sys->backend) return;

    // Use current UI state for init
    sys->widgets = sys->ui->widgets;
    sys->display_list = sys->ui->display_list;

    RenderBackendInit init = {
        .window = sys->render_context.window,
        .surface = &sys->render_context.surface,
        .get_required_instance_extensions = platform_get_required_vulkan_instance_extensions,
        .create_surface = platform_create_vulkan_surface,
        .destroy_surface = platform_destroy_vulkan_surface,
        .get_framebuffer_size = platform_get_framebuffer_size,
        .wait_events = platform_wait_events,
        .poll_events = platform_poll_events,
        .vert_spv = sys->assets->vert_spv_path,
        .frag_spv = sys->assets->frag_spv_path,
        .font_path = sys->assets->font_path,
        .widgets = sys->widgets,
        .display_list = sys->display_list,
        .transformer = &sys->render_context.transformer,
        .logger_config = &sys->logger_config,
    };

    sys->renderer_ready = sys->backend->init(sys->backend, &init);
    
    if (sys->renderer_ready) {
        try_sync_packet(sys);
    }
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
        fprintf(stderr, "RenderSystem: Failed to load backend '%s'\n", backend_id);
        return false;
    }

    if (config) {
        sys->logger_config.level = config->log_level;
        sys->logger_config.sink_type = RENDER_LOG_SINK_STDOUT;
    } else {
        sys->logger_config.level = RENDER_LOG_INFO; // Default
    }

    // Init runtime thread context (window creation usually happens here or in runtime_init)
    // Note: runtime_init actually creates the window.
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

void render_system_bind_ui(RenderSystem* sys, UiContext* ui) {
    sys->ui = ui;
    try_bootstrap_renderer(sys);
}

void render_system_bind_model(RenderSystem* sys, Model* model) {
    sys->model = model;
}

void render_system_update(RenderSystem* sys) {
    if (!sys || !sys->renderer_ready) return;
    
    // Pull data from UI
    if (sys->ui) {
        sys->widgets = sys->ui->widgets;
        sys->display_list = sys->ui->display_list;
    }
    
    try_sync_packet(sys);
}

void render_system_update_transformer(RenderSystem* sys) {
    if (!sys) return;
    try_sync_packet(sys);
}

// Thread Loop (Unused for now, running on main thread)
/*
static int render_thread_func(void* arg) {
    RenderSystem* sys = (RenderSystem*)arg;
    
    while (sys->running && !platform_window_should_close(sys->render_context.window)) {
        // ...
    }
    return 0;
}
*/

void render_system_run(RenderSystem* sys) {
    if (!sys) return;
    
    sys->running = true;
    
    // For now, run on main thread to avoid GLFW threading issues until we fix platform layer
    // (GLFW requires poll on main thread).
    // The previous architecture had a complex event loop.
    
    while (sys->running && !platform_window_should_close(sys->render_context.window)) {
        platform_poll_events(); // Main thread poll
        
        if (sys->ui) {
            ui_system_update(sys->ui, 0.016f); // Update UI logic (scroll, animations)
        }

        render_system_update(sys); // Sync UI data to render packet
        
        // Draw (Immediate)
        if (sys->renderer_ready && sys->backend) {
             const RenderFramePacket* packet = render_system_acquire_packet(sys);
             if (packet) {
                if (sys->backend->update_transformer) 
                    sys->backend->update_transformer(sys->backend, &packet->transformer);
                if (sys->backend->update_ui)
                    sys->backend->update_ui(sys->backend, packet->widgets, packet->display_list);
                if (sys->backend->draw)
                    sys->backend->draw(sys->backend);
             }
        }
    }
}
