#include "engine/graphics/render_system.h"
#include "foundation/logger/logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "foundation/platform/platform.h"
#include "engine/graphics/backend/renderer_backend.h"
#include "engine/graphics/backend/vulkan/vulkan_renderer.h"
#include "engine/ui/ui_renderer.h"
#include "engine/ui/ui_core.h"
#include "engine/ui/ui_layout.h"
#include "engine/graphics/text/font.h"
#include "engine/graphics/text/text_renderer.h"

// --- Helper: Packet Management ---

static void render_packet_free_resources(RenderFramePacket* packet) {
    if (!packet) return;
    scene_clear(&packet->scene);
}

static void try_sync_packet(RenderSystem* sys) {
    if (!sys) return;
    
    // 1. Unified Scene Generation
    mtx_lock(&sys->packet_mutex);
    
    RenderFramePacket* dest = &sys->packets[sys->back_packet_index];
    
    // Clear old scene
    render_packet_free_resources(dest);
    
    dest->scene.frame_number = sys->frame_count;

    // Setup Camera (Ortho)
    PlatformWindowSize size = platform_get_framebuffer_size(sys->window);
    float w = (float)size.width;
    float h = (float)size.height;
    if (w < 1.0f) w = 1.0f;
    if (h < 1.0f) h = 1.0f;

    // View: Identity (Camera at 0,0)
    dest->scene.camera.view_matrix = mat4_identity();
    
    // Proj: Ortho 0..w, 0..h. 
    // Vulkan Clip: Y is down (-1 Top, +1 Bottom).
    // We want y=0 (Top) -> -1.
    // We want y=h (Bottom) -> +1.
    // mat4_orthographic(left, right, bottom, top, ...)
    // m[5] = 2/(top-bottom). m[13] = -(top+bottom)/(top-bottom).
    // If bottom=0, top=h: m[5]=2/h, m[13]=-1.
    // y=0 -> -1. y=h -> 1. Correct.
    
    Mat4 proj = mat4_orthographic(0.0f, w, 0.0f, h, -100.0f, 100.0f);
    
    dest->scene.camera.view_matrix = proj; 

    // 2. Generate UI Scene (if bound)
    if (sys->ui_root_view) {
        ui_renderer_build_scene(sys->ui_root_view, &dest->scene, sys->assets);
    }

    // DEBUG: Compute Result Visualization
    if (sys->show_compute_result) {
        SceneObject quad = {0};
        quad.id = 9999;
        quad.position = (Vec3){600.0f, 100.0f, 0.0f};
        quad.scale = (Vec3){512.0f, 512.0f, 1.0f};
        quad.color = (Vec4){1.0f, 1.0f, 1.0f, 1.0f};
        quad.shader_params_0.x = 2.0f; // User Texture
        quad.uv_rect = (Vec4){0.0f, 0.0f, 1.0f, 1.0f};
        scene_add_object(&dest->scene, quad);
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
    if (!sys->window) return;
    if (!sys->assets || !sys->assets->ui_default_vert_spv) return;
    if (!sys->backend) return;

    PlatformSurface surface = {0};
    
    RenderBackendInit init = {
        .window = sys->window,
        .surface = &surface, // Pass pointer to empty surface struct, backend/platform fills it
        .vert_spv = sys->assets->ui_default_vert_spv,
        .frag_spv = sys->assets->ui_default_frag_spv,
        .font_path = sys->assets->font_path,
    };

    sys->renderer_ready = sys->backend->init(sys->backend, &init);
}

bool render_system_init(RenderSystem* sys, const RenderSystemConfig* config) {
    if (!sys || !config) return false;
    
    memset(sys, 0, sizeof(RenderSystem));
    sys->window = config->window;
    
    mtx_init(&sys->packet_mutex, mtx_plain);
    sys->back_packet_index = 1;
    sys->frame_count = 0;

    // Register Backend
    renderer_backend_register(vulkan_renderer_backend());
    const char* backend_id = config->backend_type ? config->backend_type : "vulkan";
    sys->backend = renderer_backend_get(backend_id);
    if (!sys->backend) {
        LOG_ERROR("RenderSystem: Failed to load backend '%s'", backend_id);
        return false;
    }
    
    // Note: We delay bootstrap until assets are bound
    
    return true;
}

void render_system_shutdown(RenderSystem* sys) {
    if (!sys) return;
    
    if (sys->backend && sys->backend->cleanup) {
        sys->backend->cleanup(sys->backend);
    }
    
    render_packet_free_resources(&sys->packets[0]);
    render_packet_free_resources(&sys->packets[1]);
    mtx_destroy(&sys->packet_mutex);
}

void render_system_bind_assets(RenderSystem* sys, Assets* assets) {
    sys->assets = assets;
    try_bootstrap_renderer(sys);
}

void render_system_bind_ui(RenderSystem* sys, UiElement* root_view) {
    sys->ui_root_view = root_view;
    try_bootstrap_renderer(sys);
}

void render_system_update(RenderSystem* sys) {
    if (!sys || !sys->renderer_ready) return;

    if (sys->active_compute_pipeline > 0 && sys->backend && sys->backend->compute_dispatch) {
        // Must match generated GLSL push constant layout
        struct {
            float time;
            float width;
            float height;
        } push = {
            .time = (float)sys->current_time,
            .width = 512.0f,
            .height = 512.0f
        };
        
        // Dispatch Compute (Target is 512x512, assuming 16x16 workgroups)
        sys->backend->compute_dispatch(sys->backend, sys->active_compute_pipeline, 32, 32, 1, &push, sizeof(push));
    }

    try_sync_packet(sys);
}

void render_system_set_compute_pipeline(RenderSystem* sys, uint32_t pipeline_id) {
    if (!sys) return;
    
    // Cleanup old pipeline if it was dynamic? 
    // Actually, RenderSystem doesn't know if it's dynamic.
    // Let's just swap it.
    sys->active_compute_pipeline = pipeline_id;
    LOG_INFO("RenderSystem: Active compute pipeline set to %u", pipeline_id);
}

void render_system_request_screenshot(RenderSystem* sys, const char* filepath) {
    if (!sys || !sys->backend || !sys->backend->request_screenshot) return;
    sys->backend->request_screenshot(sys->backend, filepath);
}