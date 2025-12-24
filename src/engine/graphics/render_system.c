#include "engine/graphics/render_system.h"
#include "engine/assets/assets.h"
#include "engine/graphics/internal/render_frame_packet.h"
#include "engine/graphics/primitives.h"
#include "engine/graphics/render_commands.h"
#include "foundation/logger/logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "foundation/thread/thread.h"

#include "foundation/platform/platform.h"
#include "engine/graphics/internal/renderer_backend.h"
#include "engine/graphics/internal/vulkan/vulkan_renderer.h"
#include "engine/graphics/stream.h"

struct RenderSystem {
    // Dependencies (Injectable)
    Assets* assets;

    // Internal State
    PlatformWindow* window;
    struct RendererBackend* backend;
    Stream* gpu_input_stream; // Global Input Stream (SSBO)
    Stream* ui_instance_stream; // NEW: UI Instance Buffer
    GpuInstanceData* ui_cpu_buffer;
    size_t ui_cpu_capacity;
    
    // Command Buffer
    RenderCommandList cmd_list; 
    
    // Packet buffering
    RenderFramePacket packets[2];
    int front_packet_index;
    int back_packet_index;
    bool packet_ready;
    Mutex* packet_mutex;
    
    // Thread control
    bool running;
    bool renderer_ready;
    bool show_compute_result;
    uint32_t active_compute_pipeline;
    double current_time;
    
    uint64_t frame_count;
};

// ... Helper: Packet Management ...

static void render_packet_free_resources(RenderFramePacket* packet) {
    if (!packet) return;
    scene_clear(packet->scene);
}

const RenderFramePacket* render_system_acquire_packet(RenderSystem* sys) {
    if (!sys) return NULL;

    mutex_lock(sys->packet_mutex);
    if (sys->packet_ready) {
        int temp = sys->front_packet_index;
        sys->front_packet_index = sys->back_packet_index;
        sys->back_packet_index = temp;
        sys->packet_ready = false;
    }
    mutex_unlock(sys->packet_mutex);

    return &sys->packets[sys->front_packet_index];
}

Scene* render_system_get_scene(RenderSystem* sys) {
    if (!sys) return NULL;
    return sys->packets[sys->back_packet_index].scene;
}

// ... Init & Bootstrap ...

static void try_bootstrap_renderer(RenderSystem* sys) {
    if (!sys) return;
    if (sys->renderer_ready) return;
    
    // Dependencies Check
    if (!sys->window) return;
    if (!sys->assets) return;
    if (!sys->backend) return;

    // Load Shaders into Memory
    AssetData vert_shader = assets_load_file(sys->assets, "shaders/ui_default.vert.spv");
    AssetData frag_shader = assets_load_file(sys->assets, "shaders/ui_default.frag.spv");
    
    if (!vert_shader.data || !frag_shader.data) {
        LOG_ERROR("RenderSystem: Failed to load default shaders from assets.");
        assets_free_file(&vert_shader);
        assets_free_file(&frag_shader);
        return;
    }

    PlatformSurface surface = {0};
    
    RenderBackendInit init = {
        .window = sys->window,
        .surface = &surface, // Pass pointer to empty surface struct, backend/platform fills it
        .font = assets_get_font(sys->assets),
        .vert_shader = { .data = vert_shader.data, .size = vert_shader.size },
        .frag_shader = { .data = frag_shader.data, .size = frag_shader.size },
    };

    sys->renderer_ready = sys->backend->init(sys->backend, &init);
    
    // Create Streams now that backend is ready
    if (sys->renderer_ready) {
        if (!sys->gpu_input_stream) {
            sys->gpu_input_stream = stream_create(sys, STREAM_CUSTOM, 1, sizeof(GpuInputState));
            if (sys->gpu_input_stream) stream_bind_compute(sys->gpu_input_stream, 1);
        }
        
        if (!sys->ui_instance_stream) {
            // Initial capacity was set in create, now we create the GPU resource
            sys->ui_instance_stream = stream_create(sys, STREAM_CUSTOM, sys->ui_cpu_capacity, sizeof(GpuInstanceData));
        }
    }
    
    // Cleanup loaded assets (Backend should have copied what it needs)
    assets_free_file(&vert_shader);
    assets_free_file(&frag_shader);
}

RenderSystem* render_system_create(const RenderSystemConfig* config) {
    if (!config) return NULL;
    RenderSystem* sys = calloc(1, sizeof(RenderSystem));
    if (!sys) return NULL;

    sys->window = config->window;
    
    sys->packet_mutex = mutex_create();
    sys->back_packet_index = 1;
    sys->frame_count = 0;

    // Create UI Instance Stream (CPU Only initially)
    sys->ui_cpu_capacity = 1024;
    sys->ui_cpu_buffer = malloc(sizeof(GpuInstanceData) * sys->ui_cpu_capacity);
    
    // Init Command List
    sys->cmd_list.capacity = 2048;
    sys->cmd_list.commands = malloc(sizeof(RenderCommand) * sys->cmd_list.capacity);
    sys->cmd_list.count = 0;

    // Create Scenes
    sys->packets[0].scene = scene_create();
    sys->packets[1].scene = scene_create();

    // Register Backend
    renderer_backend_register(vulkan_renderer_backend());
    const char* backend_id = config->backend_type ? config->backend_type : "vulkan";
    sys->backend = renderer_backend_get(backend_id);
    if (!sys->backend) {
        LOG_ERROR("RenderSystem: Failed to load backend '%s'", backend_id);
        scene_destroy(sys->packets[0].scene);
        scene_destroy(sys->packets[1].scene);
        mutex_destroy(sys->packet_mutex);
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
    
    stream_destroy(sys->gpu_input_stream);
    stream_destroy(sys->ui_instance_stream);
    if (sys->ui_cpu_buffer) free(sys->ui_cpu_buffer);
    
    if (sys->cmd_list.commands) free(sys->cmd_list.commands);

    render_packet_free_resources(&sys->packets[0]);
    scene_destroy(sys->packets[0].scene);
    
    render_packet_free_resources(&sys->packets[1]);
    scene_destroy(sys->packets[1].scene);
    
    mutex_destroy(sys->packet_mutex);
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
    
    scene_set_frame_number(dest->scene, sys->frame_count);

    // Setup Camera (Ortho)
    PlatformWindowSize size = platform_get_framebuffer_size(sys->window);
    float w = (float)size.width;
    float h = (float)size.height;
    if (w < 1.0f) w = 1.0f;
    if (h < 1.0f) h = 1.0f;

    // View: Identity (Camera at 0,0)
    SceneCamera camera = {0};
    camera.view_matrix = mat4_identity();
    
    Mat4 proj = mat4_orthographic(0.0f, w, 0.0f, h, -100.0f, 100.0f);
    camera.view_matrix = proj; // Matches original behavior (overwriting view with proj)
    
    scene_set_camera(dest->scene, camera);
}

void render_system_update(RenderSystem* sys) {
    if (!sys || !sys->renderer_ready) return;

    if (sys->active_compute_pipeline > 0 && sys->backend && sys->backend->compute_dispatch) {
        // Must match generated GLSL push constant layout in glsl_emitter.c
        // GLSL: float time(0), width(4), height(8), [padding(12)], vec4 mouse(16)
        struct {
            float time;
            float width;
            float height;
            float _padding; 
            float mouse[4];
        } push = {
            .time = (float)sys->current_time,
            .width = 512.0f,
            .height = 512.0f,
            ._padding = 0.0f,
            .mouse = {0, 0, 0, 0}
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
        quad.raw.params_0.x = (float)SCENE_MODE_USER_TEXTURE; // User Texture
        quad.uv_rect = (Vec4){0.0f, 0.0f, 1.0f, 1.0f};
        
        RenderFramePacket* dest = &sys->packets[sys->back_packet_index];
        scene_add_object(dest->scene, quad);
    }

    // Mark Packet Ready
    mutex_lock(sys->packet_mutex);
    sys->packet_ready = true;
    mutex_unlock(sys->packet_mutex);
}

static void cmd_list_add(RenderCommandList* list, RenderCommand cmd) {
    if (list->count >= list->capacity) {
        size_t new_cap = list->capacity > 0 ? list->capacity * 2 : 1024;
        list->capacity = (uint32_t)new_cap;
        
        RenderCommand* new_cmds = realloc(list->commands, sizeof(RenderCommand) * list->capacity);
        if (new_cmds) {
            list->commands = new_cmds;
        } else {
            LOG_ERROR("RenderSystem: Failed to grow command list!");
            return;
        }
    }
    list->commands[list->count++] = cmd;
}

void render_system_draw(RenderSystem* sys) {
    if (!sys || !sys->renderer_ready || !sys->backend) return;
    
    const RenderFramePacket* packet = render_system_acquire_packet(sys);
    if (!packet) return;
    
    Scene* scene = packet->scene;
    size_t count = 0;
    const SceneObject* objects = scene_get_all_objects(scene, &count);
    
    // Reset Command List
    sys->cmd_list.count = 0;
    
    // Ensure CPU buffer capacity
    size_t required_ui_slots = 0;
    for(size_t i=0; i<count; ++i) if (objects[i].prim_type != SCENE_PRIM_CUSTOM) required_ui_slots++;
    
    if (required_ui_slots > sys->ui_cpu_capacity) {
        size_t new_cap = required_ui_slots + 1024;
        GpuInstanceData* new_buf = realloc(sys->ui_cpu_buffer, sizeof(GpuInstanceData) * new_cap);
        
        if (new_buf) {
            sys->ui_cpu_capacity = new_cap;
            sys->ui_cpu_buffer = new_buf;
            
            if (sys->ui_instance_stream) stream_destroy(sys->ui_instance_stream);
            sys->ui_instance_stream = stream_create(sys, STREAM_CUSTOM, sys->ui_cpu_capacity, sizeof(GpuInstanceData));
        } else {
            LOG_ERROR("RenderSystem: Failed to grow UI buffer!");
        }
    }
    
    size_t ui_idx = 0;
    size_t ui_batch_start = 0;
    
    for (size_t i = 0; i < count; ++i) {
        const SceneObject* obj = &objects[i];
        
        if (obj->prim_type == SCENE_PRIM_CUSTOM) {
             // Flush UI
             if (ui_idx > ui_batch_start) {
                 RenderCommand cmd = {0};
                 cmd.type = RENDER_CMD_BIND_PIPELINE;
                 cmd.bind_pipeline.pipeline_id = 0; // Default
                 cmd_list_add(&sys->cmd_list, cmd);
                 
                 cmd.type = RENDER_CMD_BIND_BUFFER;
                 cmd.bind_buffer.slot = 0; // Instance Slot
                 cmd.bind_buffer.buffer_handle = stream_get_handle(sys->ui_instance_stream);
                 cmd_list_add(&sys->cmd_list, cmd);
                 
                 cmd.type = RENDER_CMD_DRAW_INDEXED;
                 cmd.draw_indexed.index_count = 6;
                 cmd.draw_indexed.instance_count = (uint32_t)(ui_idx - ui_batch_start);
                 cmd.draw_indexed.first_index = 0;
                 cmd.draw_indexed.vertex_offset = 0;
                 cmd.draw_indexed.first_instance = (uint32_t)ui_batch_start;
                 cmd_list_add(&sys->cmd_list, cmd);
                 
                 ui_batch_start = ui_idx;
             }
             
             CustomDrawData* data = (CustomDrawData*)obj->instance_buffer;
             if (data) {
                 RenderCommand cmd = {0};
                 cmd.type = RENDER_CMD_BIND_PIPELINE;
                 cmd.bind_pipeline.pipeline_id = data->pipeline_id;
                 cmd_list_add(&sys->cmd_list, cmd);
                 
                 for (int b=0; b<4; ++b) {
                     if (data->buffers[b]) {
                         cmd.type = RENDER_CMD_BIND_BUFFER;
                         cmd.bind_buffer.slot = b;
                         cmd.bind_buffer.buffer_handle = data->buffers[b];
                         cmd_list_add(&sys->cmd_list, cmd);
                     }
                 }
                 
                 cmd.type = RENDER_CMD_DRAW;
                 cmd.draw.vertex_count = data->vertex_count;
                 cmd.draw.instance_count = data->instance_count;
                 cmd.draw.first_vertex = 0;
                 cmd.draw.first_instance = 0;
                 cmd_list_add(&sys->cmd_list, cmd);
             }
        } else {
            // UI Object
            if (ui_idx < sys->ui_cpu_capacity) {
                Mat4 m;
                Mat4 s = mat4_scale(obj->scale);
                Mat4 t = mat4_translation(obj->position);
                m = mat4_multiply(&t, &s);
                
                GpuInstanceData* inst = &sys->ui_cpu_buffer[ui_idx];
                inst->model = m;
                inst->color = obj->color;
                inst->uv_rect = obj->uv_rect;
                inst->params_1 = obj->raw.params_0;
                inst->params_2 = obj->raw.params_1;
                inst->clip_rect = obj->ui.clip_rect;
                
                ui_idx++;
            }
        }
    }
    
    // Flush Final Batch
    if (ui_idx > ui_batch_start) {
         RenderCommand cmd = {0};
         cmd.type = RENDER_CMD_BIND_PIPELINE;
         cmd.bind_pipeline.pipeline_id = 0;
         cmd_list_add(&sys->cmd_list, cmd);
         
         cmd.type = RENDER_CMD_BIND_BUFFER;
         cmd.bind_buffer.slot = 0;
         cmd.bind_buffer.buffer_handle = stream_get_handle(sys->ui_instance_stream);
         cmd_list_add(&sys->cmd_list, cmd);
         
         cmd.type = RENDER_CMD_DRAW_INDEXED;
         cmd.draw_indexed.index_count = 6;
         cmd.draw_indexed.instance_count = (uint32_t)(ui_idx - ui_batch_start);
         cmd.draw_indexed.first_index = 0;
         cmd.draw_indexed.vertex_offset = 0;
         cmd.draw_indexed.first_instance = (uint32_t)ui_batch_start;
         cmd_list_add(&sys->cmd_list, cmd);
    }
    
    // Upload Data
    if (ui_idx > 0) {
        stream_set_data(sys->ui_instance_stream, sys->ui_cpu_buffer, ui_idx);
    }
    
    // Submit
    sys->backend->submit_commands(sys->backend, &sys->cmd_list);
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

uint32_t render_system_create_compute_pipeline_from_source(RenderSystem* sys, const char* source) {
    if (!sys || !sys->backend) return 0;
    
    // 1. Compile
    void* spv_code = NULL;
    size_t spv_size = 0;
    
    if (!sys->backend->compile_shader) {
        LOG_ERROR("Backend does not support runtime shader compilation.");
        return 0;
    }
    
    if (!sys->backend->compile_shader(sys->backend, source, strlen(source), "compute", &spv_code, &spv_size)) {
        LOG_ERROR("Shader compilation failed.");
        return 0;
    }
    
    // 2. Create Pipeline
    uint32_t id = 0;
    if (sys->backend->compute_pipeline_create) {
        id = sys->backend->compute_pipeline_create(sys->backend, spv_code, spv_size, 0);
    }
    
    // 3. Free SPIR-V
    free(spv_code);
    
    return id;
}

void render_system_destroy_compute_pipeline(RenderSystem* sys, uint32_t pipeline_id) {
    if (!sys || !sys->backend || !sys->backend->compute_pipeline_destroy) return;
    sys->backend->compute_pipeline_destroy(sys->backend, pipeline_id);
}

uint32_t render_system_create_graphics_pipeline(RenderSystem* sys, const void* vert_code, size_t vert_size, const void* frag_code, size_t frag_size, int layout_index) {
    if (!sys || !sys->backend || !sys->backend->graphics_pipeline_create) return 0;
    return sys->backend->graphics_pipeline_create(sys->backend, vert_code, vert_size, frag_code, frag_size, layout_index);
}

void render_system_destroy_graphics_pipeline(RenderSystem* sys, uint32_t pipeline_id) {
    if (!sys || !sys->backend || !sys->backend->graphics_pipeline_destroy) return;
    sys->backend->graphics_pipeline_destroy(sys->backend, pipeline_id);
}

void render_system_request_screenshot(RenderSystem* sys, const char* filepath) {
    if (!sys || !sys->backend || !sys->backend->request_screenshot) return;
    sys->backend->request_screenshot(sys->backend, filepath);
}

double render_system_get_time(RenderSystem* sys) { return sys ? sys->current_time : 0.0; }
uint64_t render_system_get_frame_count(RenderSystem* sys) { return sys ? sys->frame_count : 0; }
bool render_system_is_ready(RenderSystem* sys) { return sys ? sys->renderer_ready : false; }
void render_system_set_show_compute(RenderSystem* sys, bool show) { if(sys) sys->show_compute_result = show; }

RendererBackend* render_system_get_backend(RenderSystem* sys) {
    return sys ? sys->backend : NULL;
}

Stream* render_system_get_input_stream(RenderSystem* sys) {
    return sys ? sys->gpu_input_stream : NULL;
}

void render_system_update_gpu_input(RenderSystem* sys, const GpuInputState* state) {
    if (!sys || !state || !sys->gpu_input_stream) return;
    stream_set_data(sys->gpu_input_stream, state, 1);
}