#ifndef RENDER_SYSTEM_INTERNAL_H
#define RENDER_SYSTEM_INTERNAL_H

#include "engine/graphics/render_system.h"
#include "engine/graphics/internal/render_frame_packet.h"
#include "engine/graphics/internal/resources/primitives.h"
#include "engine/graphics/graphics_types.h"
#include "foundation/thread/thread.h"

// Forward declarations
typedef struct Assets Assets;
typedef struct PlatformWindow PlatformWindow;
typedef struct RendererBackend RendererBackend;
typedef struct Stream Stream;
typedef struct ComputeGraph ComputeGraph;

struct RenderSystem {
    // Dependencies
    Assets* assets;

    // Internal State
    PlatformWindow* window;
    RendererBackend* backend;
    Stream* gpu_input_stream; 
    Stream* ui_instance_stream; 
    GpuInstanceData* ui_cpu_buffer;
    size_t ui_cpu_capacity;
    
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
    
    bool running;
    bool renderer_ready;
    double current_time;
    
    uint64_t frame_count;
};

#endif // RENDER_SYSTEM_INTERNAL_H
