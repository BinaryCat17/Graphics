#include "engine/graphics/render_system.h"
#include "engine/scene/render_packet.h"
#include "engine/assets/assets.h"
#include "engine/graphics/internal/primitives.h"
#include "engine/graphics/graphics_types.h"
#include "foundation/logger/logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "foundation/thread/thread.h"

#include "foundation/platform/platform.h"
#include "engine/graphics/internal/backend/renderer_backend.h"
#include "engine/graphics/internal/backend/vulkan/vulkan_renderer.h"
#include "engine/graphics/stream.h"
#include "engine/graphics/compute_graph.h"
#include "engine/graphics/internal/render_system_internal.h"

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
    
    if (!sys->window) return;
    if (!sys->assets) return;
    if (!sys->backend) return;

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
        .surface = &surface, 
        .font = assets_get_font(sys->assets),
        .vert_shader = { .data = vert_shader.data, .size = vert_shader.size },
        .frag_shader = { .data = frag_shader.data, .size = frag_shader.size },
    };

    sys->renderer_ready = sys->backend->init(sys->backend, &init);
    
    if (sys->renderer_ready) {
        if (!sys->gpu_input_stream) {
            sys->gpu_input_stream = stream_create(sys, STREAM_CUSTOM, 1, sizeof(GpuInputState));
            if (sys->gpu_input_stream) stream_bind_compute(sys->gpu_input_stream, 1);
        }
    }
    
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
    
    sys->cmd_list.capacity = 2048;
    sys->cmd_list.commands = malloc(sizeof(RenderCommand) * sys->cmd_list.capacity);
    sys->cmd_list.count = 0;

    sys->packets[0].scene = scene_create();
    sys->packets[1].scene = scene_create();

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

    RenderFramePacket* dest = &sys->packets[sys->back_packet_index];
    render_packet_free_resources(dest); // scene_clear
    
    scene_set_frame_number(dest->scene, sys->frame_count);

    PlatformWindowSize size = platform_get_framebuffer_size(sys->window);
    float w = (float)size.width;
    float h = (float)size.height;
    if (w < 1.0f) w = 1.0f;
    if (h < 1.0f) h = 1.0f;

    SceneCamera camera = {0};
    camera.view_matrix = mat4_identity();
    // Swap near/far to ensure Z-layering works correctly (Higher Z = Closer/Lower Depth value)
    // Old: -100, 100. New: 100, -100. 
    // Z=-10 (Base) -> Farther. Z=-9 (Child) -> Closer.
    Mat4 proj = mat4_orthographic(0.0f, w, 0.0f, h, 100.0f, -100.0f);
    camera.proj_matrix = proj; 
    
    scene_set_camera(dest->scene, camera);
}

void render_system_update(RenderSystem* sys) {
    if (!sys || !sys->renderer_ready) return;

    // 1. Execute Registered Compute Graphs
    if (sys->compute_graphs) {
        for (size_t i = 0; i < sys->compute_graph_count; ++i) {
            ComputeGraph* graph = sys->compute_graphs[i];
            if (graph) {
                compute_graph_execute(graph, sys);
            }
        }
    }

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
    
    size_t batch_count = 0;
    const RenderBatch* batches = scene_get_render_batches(scene, &batch_count);
    
    if (batch_count > 0) {
        static double last_log_time = 0.0;
        if (sys->current_time - last_log_time >= logger_get_trace_interval()) {
            LOG_DEBUG("RenderSystem: Processing %zu batches", batch_count);
            last_log_time = sys->current_time;
        }
    }
    
    // Reset Command List
    sys->cmd_list.count = 0;
    
    // Calculate ViewProj
    SceneCamera cam = scene_get_camera(scene);
    Mat4 view_proj = mat4_multiply(&cam.view_matrix, &cam.proj_matrix);

    // Push Constants Command
    RenderCommand pc_cmd = {0};
    pc_cmd.type = RENDER_CMD_PUSH_CONSTANTS;
    pc_cmd.push_constants.data = &view_proj;
    pc_cmd.push_constants.size = sizeof(Mat4);
    pc_cmd.push_constants.stage_flags = 3; // VERTEX | FRAGMENT
    
    cmd_list_add(&sys->cmd_list, pc_cmd);

    // Process Render Batches
    uint32_t current_pipeline = (uint32_t)-1;

    for (size_t i = 0; i < batch_count; ++i) {
        const RenderBatch* batch = &batches[i];
        
        // 1. Pipeline
        if (batch->pipeline_id != current_pipeline) {
             RenderCommand cmd = {0};
             cmd.type = RENDER_CMD_BIND_PIPELINE;
             cmd.bind_pipeline.pipeline_id = batch->pipeline_id;
             cmd_list_add(&sys->cmd_list, cmd);
             current_pipeline = batch->pipeline_id;
        }
        
        // 2. Custom Bindings
        for (uint32_t b = 0; b < batch->bind_count && b < 4; ++b) {
            if (batch->bind_buffers[b]) {
                RenderCommand cmd = {0};
                cmd.type = RENDER_CMD_BIND_BUFFER;
                cmd.bind_buffer.slot = batch->bind_slots[b];
                cmd.bind_buffer.stream = batch->bind_buffers[b];
                cmd_list_add(&sys->cmd_list, cmd);
            }
        }
        
        // 3. Draw
        if (batch->vertex_stream) {
             RenderCommand cmd = {0};
             cmd.type = RENDER_CMD_BIND_VERTEX_BUFFER;
             cmd.bind_buffer.stream = batch->vertex_stream;
             cmd_list_add(&sys->cmd_list, cmd);
        }

        if (batch->index_stream) {
             RenderCommand cmd = {0};
             cmd.type = RENDER_CMD_BIND_INDEX_BUFFER;
             cmd.bind_buffer.stream = batch->index_stream;
             cmd_list_add(&sys->cmd_list, cmd);
             
             RenderCommand draw_cmd = {0};
             draw_cmd.type = RENDER_CMD_DRAW_INDEXED;
             draw_cmd.draw_indexed.index_count = batch->index_count;
             draw_cmd.draw_indexed.instance_count = batch->instance_count > 0 ? batch->instance_count : 1;
             draw_cmd.draw_indexed.first_instance = batch->first_instance;
             cmd_list_add(&sys->cmd_list, draw_cmd);
        } else if (batch->mesh) {
            // TODO: Implement Mesh Binding (Vertex Buffers) in Backend or via Commands
        } else {
            // Draw Arrays/Custom/Indexed (without explicit stream binding, e.g. Instancing of Quad)
            if (batch->index_count > 0 && !batch->index_stream) {
                 RenderCommand cmd = {0};
                 cmd.type = RENDER_CMD_DRAW_INDEXED;
                 cmd.draw_indexed.index_count = batch->index_count;
                 cmd.draw_indexed.instance_count = batch->instance_count > 0 ? batch->instance_count : 1;
                 cmd.draw_indexed.first_index = 0;
                 cmd.draw_indexed.vertex_offset = 0;
                 cmd.draw_indexed.first_instance = batch->first_instance;
                 cmd_list_add(&sys->cmd_list, cmd);
            } else {
                 RenderCommand cmd = {0};
                 cmd.type = RENDER_CMD_DRAW;
                 cmd.draw.vertex_count = batch->vertex_count;
                 cmd.draw.instance_count = batch->instance_count > 0 ? batch->instance_count : 1;
                 cmd.draw.first_vertex = 0;
                 cmd.draw.first_instance = batch->first_instance;
                 cmd_list_add(&sys->cmd_list, cmd);
            }
        }
    }
    
    // Submit
    sys->backend->submit_commands(sys->backend, &sys->cmd_list);
}

void render_system_resize(RenderSystem* sys, int width, int height) {
    if (sys && sys->backend && sys->backend->update_viewport) {
        sys->backend->update_viewport(sys->backend, width, height);
    }
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

void render_system_register_compute_graph(RenderSystem* sys, ComputeGraph* graph) {
    if (!sys || !graph) return;
    
    // Check duplicates
    for (size_t i = 0; i < sys->compute_graph_count; ++i) {
        if (sys->compute_graphs[i] == graph) return;
    }
    
    if (sys->compute_graph_count >= sys->compute_graph_capacity) {
        size_t new_cap = sys->compute_graph_capacity > 0 ? sys->compute_graph_capacity * 2 : 4;
        ComputeGraph** new_arr = (ComputeGraph**)realloc((void*)sys->compute_graphs, new_cap * sizeof(ComputeGraph*));
        if (!new_arr) return;
        sys->compute_graphs = new_arr;
        sys->compute_graph_capacity = new_cap;
    }
    
    sys->compute_graphs[sys->compute_graph_count++] = graph;
    LOG_INFO("RenderSystem: Registered compute graph.");
}

void render_system_unregister_compute_graph(RenderSystem* sys, ComputeGraph* graph) {
    if (!sys || !graph) return;
    
    for (size_t i = 0; i < sys->compute_graph_count; ++i) {
        if (sys->compute_graphs[i] == graph) {
            // Swap with last
            sys->compute_graphs[i] = sys->compute_graphs[sys->compute_graph_count - 1];
            sys->compute_graph_count--;
            LOG_INFO("RenderSystem: Unregistered compute graph.");
            return;
        }
    }
}

