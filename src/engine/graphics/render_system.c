#include "engine/graphics/render_system.h"
#include "foundation/logger/logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include "foundation/platform/platform.h"
#include "engine/graphics/backend/renderer_backend.h"
#include "engine/graphics/backend/vulkan/vulkan_renderer.h"
#include "engine/graphics/text/font.h"
#include "engine/graphics/text/text_renderer.h"

struct RenderSystem {
    // Dependencies (Injectable)
    Assets* assets;

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
};

// --- Helper: Packet Management ---

static void render_packet_free_resources(RenderFramePacket* packet) {
    if (!packet) return;
    scene_clear(&packet->scene);
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

Scene* render_system_get_scene(RenderSystem* sys) {
    if (!sys) return NULL;
    return &sys->packets[sys->back_packet_index].scene;
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

RenderSystem* render_system_create(const RenderSystemConfig* config) {
    if (!config) return NULL;
    RenderSystem* sys = calloc(1, sizeof(RenderSystem));
    if (!sys) return NULL;

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
        free(sys);
        return NULL;
    }
    
    return sys;
}

void render_system_destroy(RenderSystem* sys) {
    if (!sys) return;
    
    if (sys->backend && sys->backend->cleanup) {
        sys->backend->cleanup(sys->backend);
    }
    
    render_packet_free_resources(&sys->packets[0]);
    render_packet_free_resources(&sys->packets[1]);
    mtx_destroy(&sys->packet_mutex);
    free(sys);
}

void render_system_bind_assets(RenderSystem* sys, Assets* assets) {
    sys->assets = assets;
    try_bootstrap_renderer(sys);
}

void render_system_begin_frame(RenderSystem* sys, double time) {
    if (!sys) return;
    sys->frame_count++;
    sys->current_time = time;

    // Prepare Back Packet
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
    
    Mat4 proj = mat4_orthographic(0.0f, w, 0.0f, h, -100.0f, 100.0f);
    dest->scene.camera.view_matrix = proj; 
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

    // DEBUG: Compute Result Visualization
    if (sys->show_compute_result) {
        SceneObject quad = {0};
        quad.id = 9999;
        quad.position = (Vec3){600.0f, 100.0f, 0.0f};
        quad.scale = (Vec3){512.0f, 512.0f, 1.0f};
        quad.color = (Vec4){1.0f, 1.0f, 1.0f, 1.0f};
        quad.shader_params_0.x = 2.0f; // User Texture
        quad.uv_rect = (Vec4){0.0f, 0.0f, 1.0f, 1.0f};
        
        RenderFramePacket* dest = &sys->packets[sys->back_packet_index];
        scene_add_object(&dest->scene, quad);
    }

    // Mark Packet Ready
    mtx_lock(&sys->packet_mutex);
    sys->packet_ready = true;
    mtx_unlock(&sys->packet_mutex);
}

void render_system_draw(RenderSystem* sys) {
    if (!sys || !sys->renderer_ready || !sys->backend) return;
    
    const RenderFramePacket* packet = render_system_acquire_packet(sys);
    if (packet && sys->backend->render_scene) {
        sys->backend->render_scene(sys->backend, &packet->scene);
    }
}

void render_system_resize(RenderSystem* sys, int width, int height) {
    if (sys && sys->backend && sys->backend->update_viewport) {
        sys->backend->update_viewport(sys->backend, width, height);
    }
}

void render_system_set_compute_pipeline(RenderSystem* sys, uint32_t pipeline_id) {
    if (!sys) return;
    sys->active_compute_pipeline = pipeline_id;
    LOG_INFO("RenderSystem: Active compute pipeline set to %u", pipeline_id);
}

uint32_t render_system_create_compute_pipeline(RenderSystem* sys, uint32_t* spv_code, size_t spv_size) {
    if (!sys || !sys->backend || !sys->backend->compute_pipeline_create) return 0;
    return sys->backend->compute_pipeline_create(sys->backend, spv_code, spv_size, 0);
}

void render_system_destroy_compute_pipeline(RenderSystem* sys, uint32_t pipeline_id) {
    if (!sys || !sys->backend || !sys->backend->compute_pipeline_destroy) return;
    sys->backend->compute_pipeline_destroy(sys->backend, pipeline_id);
}

void render_system_request_screenshot(RenderSystem* sys, const char* filepath) {
    if (!sys || !sys->backend || !sys->backend->request_screenshot) return;
    sys->backend->request_screenshot(sys->backend, filepath);
}

double render_system_get_time(RenderSystem* sys) { return sys ? sys->current_time : 0.0; }
uint64_t render_system_get_frame_count(RenderSystem* sys) { return sys ? sys->frame_count : 0; }
bool render_system_is_ready(RenderSystem* sys) { return sys ? sys->renderer_ready : false; }
void render_system_set_show_compute(RenderSystem* sys, bool show) { if(sys) sys->show_compute_result = show; }