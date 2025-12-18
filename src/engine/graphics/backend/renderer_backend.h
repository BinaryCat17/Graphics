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
} RendererBackend;

// Registry / Factory
bool renderer_backend_register(RendererBackend* backend);
RendererBackend* renderer_backend_get(const char* id);
RendererBackend* renderer_backend_default(void);

#endif // RENDERER_BACKEND_H