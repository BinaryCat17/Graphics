#ifndef RENDER_SYSTEM_INTERNAL_H
#define RENDER_SYSTEM_INTERNAL_H

#include "engine/graphics/render_system.h"
#include "engine/graphics/internal/primitives.h"
#include "engine/graphics/graphics_types.h"
#include "engine/graphics/pipeline.h"
#include "foundation/thread/thread.h"

// Forward declarations
// ... (existing)

typedef struct PassRegistryEntry {
    char name[PIPELINE_MAX_NAME_LENGTH];
    PipelinePassCallback callback;
} PassRegistryEntry;

typedef struct RenderFramePacket {
    Scene* scene;
} RenderFramePacket;

struct RenderSystem {
    // Dependencies
    Assets* assets;

    // Internal State
    PlatformWindow* window;
    RendererBackend* backend;
    Stream* gpu_input_stream; 
    
    RenderCommandList cmd_list; 
    
    RenderFramePacket packets[2];
    int front_packet_index;
    int back_packet_index;
    bool packet_ready;
    Mutex* packet_mutex;
    
    // Compute Graphs
    ComputeGraph** compute_graphs;
    size_t compute_graph_count;
    size_t compute_graph_capacity;

    // Pipeline Pass Registry
    PassRegistryEntry* pass_registry;
    size_t pass_registry_count;
    size_t pass_registry_capacity;

    // Active Pipeline Definition
    PipelineDefinition pipeline_def;
    bool pipeline_dirty;
    
    // Runtime Resources (Map 1:1 with pipeline_def.resources)
    struct {
        uint32_t handle; // Backend handle (Texture ID or Buffer ID)
        void* stream_ptr; // For Buffers (Stream*)
    } pipeline_resources[PIPELINE_MAX_RESOURCES];
    
    bool running;
    bool renderer_ready;
    double current_time;
    
    uint64_t frame_count;
};

#endif // RENDER_SYSTEM_INTERNAL_H
