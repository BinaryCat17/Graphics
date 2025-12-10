#ifndef RENDERER_BACKEND_H
#define RENDERER_BACKEND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef struct GLFWwindow GLFWwindow;

typedef struct CoordinateTransformer CoordinateTransformer;
typedef struct WidgetArray WidgetArray;

typedef enum RenderLogSinkType {
    RENDER_LOG_SINK_STDOUT,
    RENDER_LOG_SINK_FILE,
    RENDER_LOG_SINK_RING_BUFFER,
} RenderLogSinkType;

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
    bool enabled;
} RenderLoggerConfig;

typedef struct RenderLogger {
    const char* backend_id;
    RenderLogSinkType sink_type;
    FILE* file;
    RenderLogEntry* ring_entries;
    size_t ring_capacity;
    size_t ring_head;
    bool enabled;
} RenderLogger;

typedef struct RenderBackendInit {
    GLFWwindow* window;
    const char* vert_spv;
    const char* frag_spv;
    const char* font_path;
    WidgetArray widgets;
    const CoordinateTransformer* transformer;
    const RenderLoggerConfig* logger_config;
} RenderBackendInit;

typedef struct RendererBackend {
    const char* id;
    RenderLogger logger;
    void* state;
    bool (*init)(struct RendererBackend* backend, const RenderBackendInit* init);
    void (*update_transformer)(struct RendererBackend* backend, const CoordinateTransformer* transformer);
    void (*draw)(struct RendererBackend* backend);
    void (*cleanup)(struct RendererBackend* backend);
} RendererBackend;

bool render_logger_init(RenderLogger* logger, const RenderLoggerConfig* config, const char* backend_id);
void render_logger_log(RenderLogger* logger, const char* command, const char* parameters, double duration_ms);
void render_logger_cleanup(RenderLogger* logger);

bool renderer_backend_register(RendererBackend* backend);
RendererBackend* renderer_backend_get(const char* id);
RendererBackend* renderer_backend_default(void);

#endif // RENDERER_BACKEND_H
