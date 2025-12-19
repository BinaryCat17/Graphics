#ifndef RENDERER_BACKEND_H
#define RENDERER_BACKEND_H

#include <stdbool.h>
#include <stddef.h>

#include "foundation/platform/platform.h"
#include "engine/graphics/scene/scene.h"

// Backend Initialization Parameters
typedef struct RenderBackendInit {
    PlatformWindow* window;
    PlatformSurface* surface;
    
    // Resources
    const char* vert_spv;
    const char* frag_spv;
    const char* font_path;
} RenderBackendInit;

// The Abstract Renderer Interface (V-Table)
typedef struct RendererBackend {
    const char* id;
    void* state; // Internal backend state (VulkanRendererState*, WebGpuState*, etc.)

    // Lifecycle
    bool (*init)(struct RendererBackend* backend, const RenderBackendInit* init);
    void (*cleanup)(struct RendererBackend* backend);

    // Core Loop
    void (*render_scene)(struct RendererBackend* backend, const Scene* scene);
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

} RendererBackend;

// Registry / Factory
bool renderer_backend_register(RendererBackend* backend);
RendererBackend* renderer_backend_get(const char* id);
RendererBackend* renderer_backend_default(void);

#endif // RENDERER_BACKEND_H