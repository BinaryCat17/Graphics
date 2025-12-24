#ifndef RENDERER_BACKEND_H
#define RENDERER_BACKEND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "engine/graphics/render_commands.h"

// Forward Declarations
typedef struct PlatformWindow PlatformWindow;
typedef struct PlatformSurface PlatformSurface;
typedef struct Scene Scene;
typedef struct Font Font;
typedef struct Stream Stream;

// Backend Initialization Parameters
typedef struct RenderBackendInit {
    PlatformWindow* window;
    PlatformSurface* surface;
    const Font* font;
    
    // Resources (Data Blobs)
    struct {
        const void* data;
        size_t size;
    } vert_shader;
    
    struct {
        const void* data;
        size_t size;
    } frag_shader;
} RenderBackendInit;

// The Abstract Renderer Interface (V-Table)
typedef struct RendererBackend {
    const char* id;
    void* state; // Internal backend state (VulkanRendererState*, WebGpuState*, etc.)

    // Lifecycle
    bool (*init)(struct RendererBackend* backend, const RenderBackendInit* init);
    void (*cleanup)(struct RendererBackend* backend);

    // Core Loop
    void (*submit_commands)(struct RendererBackend* backend, const RenderCommandList* commands);
    void (*update_viewport)(struct RendererBackend* backend, int width, int height);

    // Utilities
    void (*request_screenshot)(struct RendererBackend* backend, const char* filepath);

    // --- Compute Subsystem ---
    
    // Create a compute pipeline from SPIR-V bytecode.
    // Returns a handle > 0 on success, 0 on failure.
    // 'layout_index' allows selecting pre-defined layouts (0 = Default: Output Image + UBO).
    uint32_t (*compute_pipeline_create)(struct RendererBackend* backend, const void* spirv_code, size_t size, int layout_index);
    
    // Destroy a compute pipeline.
    void (*compute_pipeline_destroy)(struct RendererBackend* backend, uint32_t pipeline_id);
    
    // Dispatch a compute shader.
    // 'work_group_x/y/z': Number of local workgroups.
    // 'push_constants': Pointer to data (max 128 bytes usually).
    // 'push_constants_size': Size of data.
    // The backend handles binding the output image associated with the context or graph.
    void (*compute_dispatch)(struct RendererBackend* backend, uint32_t pipeline_id, uint32_t group_x, uint32_t group_y, uint32_t group_z, void* push_constants, size_t push_constants_size);
    
    // Sync: Wait for compute to finish (memory barrier).
    void (*compute_wait)(struct RendererBackend* backend);

    // Optional: Compile high-level shader source to bytecode
    // Returns true on success. Allocates out_spv (caller must free).
    // stage: "compute", "vertex", "fragment"
    bool (*compile_shader)(struct RendererBackend* backend, const char* source, size_t size, const char* stage, void** out_spv, size_t* out_spv_size);

    // --- Buffer Management (SSBO / Vertex) ---
    // Create a GPU buffer. type: 0=SSBO/Vertex, 1=Staging/Transfer.
    // The 'stream' struct must be partially initialized (total_size set).
    // The backend will allocate the handle and store it in stream->buffer_handle.
    bool (*buffer_create)(struct RendererBackend* backend, Stream* stream);
    void (*buffer_destroy)(struct RendererBackend* backend, Stream* stream);
    void* (*buffer_map)(struct RendererBackend* backend, Stream* stream);
    void (*buffer_unmap)(struct RendererBackend* backend, Stream* stream);
    bool (*buffer_upload)(struct RendererBackend* backend, Stream* stream, const void* data, size_t size, size_t offset);
    bool (*buffer_read)(struct RendererBackend* backend, Stream* stream, void* dst, size_t size, size_t offset);

    // --- Compute Binding ---
    // Bind a buffer to a specific binding slot for the next compute dispatch.
    // 'slot': The binding index in the shader (layout(binding = slot)).
    void (*compute_bind_buffer)(struct RendererBackend* backend, Stream* stream, uint32_t slot);

    // --- Graphics Subsystem (Zero-Copy) ---
    // Create a graphics pipeline.
    // 'layout_index': 0 = UI (Default), 1 = Zero-Copy (No vertex input, SSBO bindings)
    uint32_t (*graphics_pipeline_create)(struct RendererBackend* backend, const void* vert_code, size_t vert_size, const void* frag_code, size_t frag_size, int layout_index);
    
    void (*graphics_pipeline_destroy)(struct RendererBackend* backend, uint32_t pipeline_id);
    
    // Bind a buffer to a specific binding slot (Set 1) for the next draw call.
    void (*graphics_bind_buffer)(struct RendererBackend* backend, Stream* stream, uint32_t slot);
    
    // Draw Instanced (Zero-Copy)
    // Uses the bound pipeline and buffers.
    void (*graphics_draw)(struct RendererBackend* backend, uint32_t pipeline_id, uint32_t vertex_count, uint32_t instance_count);

} RendererBackend;

// Registry / Factory
bool renderer_backend_register(RendererBackend* backend);
RendererBackend* renderer_backend_get(const char* id);
RendererBackend* renderer_backend_default(void);

#endif // RENDERER_BACKEND_H
