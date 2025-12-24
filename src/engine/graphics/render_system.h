#ifndef RENDER_SYSTEM_H
#define RENDER_SYSTEM_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "engine/graphics/gpu_input.h" // Added for GpuInputState

typedef struct RenderSystem RenderSystem;
typedef struct RenderFramePacket RenderFramePacket;
typedef struct Stream Stream; // Forward declaration

typedef struct Assets Assets;
typedef struct PlatformWindow PlatformWindow;
typedef struct Scene Scene;
typedef struct RendererBackend RendererBackend; // Forward declaration
typedef struct ComputeGraph ComputeGraph;

typedef struct RenderSystemConfig {
    PlatformWindow* window;
    const char* backend_type; // "vulkan"
} RenderSystemConfig;

RenderSystem* render_system_create(const RenderSystemConfig* config);
void render_system_destroy(RenderSystem* sys);

// Connect dependencies
void render_system_bind_assets(RenderSystem* sys, Assets* assets);

// Updates the render system (Syncs logic to render packet)
void render_system_update(RenderSystem* sys);

// Begins a new frame (updates time and frame count, clears scene)
void render_system_begin_frame(RenderSystem* sys, double time);

// Gets the current mutable scene for the frame being prepared
Scene* render_system_get_scene(RenderSystem* sys);

// Draws the current packet (Executes backend render)
void render_system_draw(RenderSystem* sys);

// Notifies system of resize
void render_system_resize(RenderSystem* sys, int width, int height);

uint32_t render_system_create_compute_pipeline(RenderSystem* sys, uint32_t* spv_code, size_t spv_size);
// Compiles GLSL (if supported by backend) and creates pipeline
uint32_t render_system_create_compute_pipeline_from_source(RenderSystem* sys, const char* source);
void render_system_destroy_compute_pipeline(RenderSystem* sys, uint32_t pipeline_id);

// Registers a Compute Graph for automatic execution each frame
void render_system_register_compute_graph(RenderSystem* sys, ComputeGraph* graph);
void render_system_unregister_compute_graph(RenderSystem* sys, ComputeGraph* graph);

// Creates a graphics pipeline from SPIR-V bytecode.
// layout_index: 0 = UI (Default), 1 = Zero-Copy (No vertex input, SSBO bindings)
uint32_t render_system_create_graphics_pipeline(RenderSystem* sys, const void* vert_code, size_t vert_size, const void* frag_code, size_t frag_size, int layout_index);
void render_system_destroy_graphics_pipeline(RenderSystem* sys, uint32_t pipeline_id);

// Request a screenshot to be saved to the specified path
void render_system_request_screenshot(RenderSystem* sys, const char* filepath);

const RenderFramePacket* render_system_acquire_packet(RenderSystem* sys);

// --- Public State Accessors (for read-only access if needed) ---
double render_system_get_time(RenderSystem* sys);
uint64_t render_system_get_frame_count(RenderSystem* sys);
bool render_system_is_ready(RenderSystem* sys);

// Internal Access
RendererBackend* render_system_get_backend(RenderSystem* sys);
Stream* render_system_get_input_stream(RenderSystem* sys);
void render_system_update_gpu_input(RenderSystem* sys, const GpuInputState* state);

#endif // RENDER_SYSTEM_H
