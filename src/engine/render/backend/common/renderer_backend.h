#ifndef RENDERER_BACKEND_H
#define RENDERER_BACKEND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "foundation/platform/platform.h"
#include "engine/ui/ui_renderer.h"

typedef struct CoordinateSystem2D CoordinateTransformer;

typedef enum RenderLogSinkType {
    RENDER_LOG_SINK_STDOUT,
    RENDER_LOG_SINK_FILE,
    RENDER_LOG_SINK_RING_BUFFER,
} RenderLogSinkType;

typedef enum RenderLogLevel {
    RENDER_LOG_NONE = 0,
    RENDER_LOG_INFO = 1,    // Initialization, Resizing, Recreation
    RENDER_LOG_VERBOSE = 2  // Per-frame commands (Draw, Present)
} RenderLogLevel;

typedef struct RenderLogEntry {
    const char* backend_id;
    const char* command;
    const char* parameters;
    double duration_ms;
} RenderLogEntry;

typedef struct RenderLoggerConfig {
    RenderLogSinkType sink_type;
    const char* sink_target;
    size_t ring_capacity;
    RenderLogLevel level; // Replaces 'enabled'
} RenderLoggerConfig;

typedef struct RenderLogger {
    const char* backend_id;
    RenderLogSinkType sink_type;
    FILE* file;
    RenderLogEntry* ring_entries;
    size_t ring_capacity;
    size_t ring_head;
    RenderLogLevel level;
} RenderLogger;

typedef struct RenderBackendInit {
    PlatformWindow* window;
    PlatformSurface* surface;
    bool (*get_required_instance_extensions)(const char*** names, uint32_t* count);
    bool (*create_surface)(PlatformWindow* window, void* instance, const void* allocation_callbacks,
                           PlatformSurface* out_surface);
    void (*destroy_surface)(void* instance, const void* allocation_callbacks, PlatformSurface* surface);
    PlatformWindowSize (*get_framebuffer_size)(PlatformWindow* window);
    void (*wait_events)(void);
    void (*poll_events)(void);
    const char* vert_spv;
    const char* frag_spv;
    const char* font_path;
    const CoordinateTransformer* transformer;
    const RenderLoggerConfig* logger_config;
} RenderBackendInit;

typedef struct RendererBackend {
    const char* id;
    RenderLogger logger;
    void* state;
    bool (*init)(struct RendererBackend* backend, const RenderBackendInit* init);
    void (*update_transformer)(struct RendererBackend* backend, const CoordinateTransformer* transformer);
    void (*update_ui)(struct RendererBackend* backend, const UiDrawList* draw_list);
    void (*draw)(struct RendererBackend* backend);
    void (*cleanup)(struct RendererBackend* backend);
} RendererBackend;

bool render_logger_init(RenderLogger* logger, const RenderLoggerConfig* config, const char* backend_id);
void render_logger_log(RenderLogger* logger, RenderLogLevel level, const char* command, const char* parameters, double duration_ms);
void render_logger_cleanup(RenderLogger* logger);

bool renderer_backend_register(RendererBackend* backend);
RendererBackend* renderer_backend_get(const char* id);
RendererBackend* renderer_backend_default(void);

#endif // RENDERER_BACKEND_H