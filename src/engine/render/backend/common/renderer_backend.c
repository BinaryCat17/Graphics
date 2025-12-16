#include "engine/render/backend/common/renderer_backend.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef RENDER_LOG_RING_DEFAULT
#define RENDER_LOG_RING_DEFAULT 128
#endif

static RendererBackend* g_registered_backends[4] = {0};
static size_t g_registered_count = 0;

bool render_logger_init(RenderLogger* logger, const RenderLoggerConfig* config, const char* backend_id) {
    if (!logger) return false;

    RenderLogSinkType sink = config ? config->sink_type : RENDER_LOG_SINK_STDOUT;
    const char* target = config ? config->sink_target : NULL;
    size_t ring_capacity = config && config->ring_capacity ? config->ring_capacity : RENDER_LOG_RING_DEFAULT;
    bool enabled = config ? config->enabled : false;

    *logger = (RenderLogger){
        .backend_id = backend_id,
        .sink_type = sink,
        .file = NULL,
        .ring_entries = NULL,
        .ring_capacity = ring_capacity,
        .ring_head = 0,
        .enabled = enabled,
    };

    if (!enabled) return true;

    if (sink == RENDER_LOG_SINK_FILE && target) {
        logger->file = platform_fopen(target, "w");
        if (!logger->file) {
            fprintf(stderr, "Failed to open render log file '%s'. Defaulting to stdout.\n", target);
            logger->sink_type = RENDER_LOG_SINK_STDOUT;
        }
    } else if (sink == RENDER_LOG_SINK_RING_BUFFER) {
        logger->ring_entries = calloc(ring_capacity, sizeof(RenderLogEntry));
        if (!logger->ring_entries) {
            fprintf(stderr, "Failed to allocate render log ring buffer. Disabling logging.\n");
            logger->enabled = false;
        }
    }

    return true;
}

void render_logger_log(RenderLogger* logger, const char* command, const char* parameters, double duration_ms) {
    if (!logger || !logger->enabled) return;

    RenderLogEntry entry = {
        .backend_id = logger->backend_id,
        .command = command,
        .parameters = parameters,
        .duration_ms = duration_ms,
    };

    switch (logger->sink_type) {
        case RENDER_LOG_SINK_STDOUT:
            fprintf(stdout, "[%s] %s(%s) took %.3f ms\n", entry.backend_id, entry.command,
                    entry.parameters ? entry.parameters : "", entry.duration_ms);
            break;
        case RENDER_LOG_SINK_FILE:
            if (logger->file) {
                fprintf(logger->file, "[%s] %s(%s) took %.3f ms\n", entry.backend_id, entry.command,
                        entry.parameters ? entry.parameters : "", entry.duration_ms);
                fflush(logger->file);
            }
            break;
        case RENDER_LOG_SINK_RING_BUFFER:
            if (logger->ring_entries && logger->ring_capacity > 0) {
                logger->ring_entries[logger->ring_head % logger->ring_capacity] = entry;
                logger->ring_head++;
            }
            break;
    }
}

void render_logger_cleanup(RenderLogger* logger) {
    if (!logger) return;
    if (logger->file) {
        fclose(logger->file);
        logger->file = NULL;
    }
    free(logger->ring_entries);
    logger->ring_entries = NULL;
    logger->ring_capacity = 0;
    logger->ring_head = 0;
    logger->enabled = false;
}

bool renderer_backend_register(RendererBackend* backend) {
    if (!backend || g_registered_count >= sizeof(g_registered_backends) / sizeof(g_registered_backends[0])) {
        return false;
    }
    for (size_t i = 0; i < g_registered_count; ++i) {
        if (strcmp(g_registered_backends[i]->id, backend->id) == 0) {
            return true;
        }
    }
    g_registered_backends[g_registered_count++] = backend;
    return true;
}

RendererBackend* renderer_backend_get(const char* id) {
    if (!id) return renderer_backend_default();
    for (size_t i = 0; i < g_registered_count; ++i) {
        if (strcmp(g_registered_backends[i]->id, id) == 0) {
            return g_registered_backends[i];
        }
    }
    return renderer_backend_default();
}

RendererBackend* renderer_backend_default(void) {
    if (g_registered_count == 0) return NULL;
    for (size_t i = 0; i < g_registered_count; ++i) {
        if (strcmp(g_registered_backends[i]->id, "vulkan") == 0) {
            return g_registered_backends[i];
        }
    }
    return g_registered_backends[0];
}

