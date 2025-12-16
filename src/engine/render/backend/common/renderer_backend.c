#include "engine/render/backend/common/renderer_backend.h"

#include <stdlib.h>
#include <string.h>

// --- Logger Implementation ---

bool render_logger_init(RenderLogger* logger, const RenderLoggerConfig* config, const char* backend_id) {
    if (!logger || !config) return false;
    
    logger->backend_id = backend_id;
    logger->sink_type = config->sink_type;
    logger->level = config->level;
    logger->ring_capacity = config->ring_capacity;
    logger->ring_head = 0;
    logger->file = NULL;
    logger->ring_entries = NULL;

    if (logger->level == RENDER_LOG_NONE) return true;

    if (logger->sink_type == RENDER_LOG_SINK_FILE && config->sink_target) {
        logger->file = fopen(config->sink_target, "w"); // Or "a"
    } else if (logger->sink_type == RENDER_LOG_SINK_RING_BUFFER && logger->ring_capacity > 0) {
        logger->ring_entries = (RenderLogEntry*)calloc(logger->ring_capacity, sizeof(RenderLogEntry));
    }

    return true;
}

void render_logger_log(RenderLogger* logger, RenderLogLevel level, const char* command, const char* parameters, double duration_ms) {
    if (!logger || logger->level == RENDER_LOG_NONE) return;
    if (level > logger->level) return; // Filter out verbose logs if level is INFO

    if (logger->sink_type == RENDER_LOG_SINK_STDOUT) {
        printf("[%s] %s(%s) took %.3f ms\n", logger->backend_id, command, parameters ? parameters : "", duration_ms);
    } else if (logger->sink_type == RENDER_LOG_SINK_FILE && logger->file) {
        fprintf(logger->file, "[%s] %s(%s) took %.3f ms\n", logger->backend_id, command, parameters ? parameters : "", duration_ms);
    } else if (logger->sink_type == RENDER_LOG_SINK_RING_BUFFER && logger->ring_entries) {
        RenderLogEntry* entry = &logger->ring_entries[logger->ring_head];
        entry->backend_id = logger->backend_id;
        entry->command = command; // Assumes static string or caller managed
        entry->parameters = parameters; 
        entry->duration_ms = duration_ms;
        logger->ring_head = (logger->ring_head + 1) % logger->ring_capacity;
    }
}

void render_logger_cleanup(RenderLogger* logger) {
    if (!logger) return;
    if (logger->file) {
        fclose(logger->file);
        logger->file = NULL;
    }
    if (logger->ring_entries) {
        free(logger->ring_entries);
        logger->ring_entries = NULL;
    }
}

// --- Registry Implementation ---

#define MAX_BACKENDS 4
static RendererBackend* g_backends[MAX_BACKENDS] = {0};
static size_t g_backend_count = 0;

bool renderer_backend_register(RendererBackend* backend) {
    if (!backend || g_backend_count >= MAX_BACKENDS) return false;
    // Check duplicates?
    g_backends[g_backend_count++] = backend;
    return true;
}

RendererBackend* renderer_backend_get(const char* id) {
    if (g_backend_count == 0) return NULL;
    if (!id) return g_backends[0]; // Default to first registered
    
    for (size_t i = 0; i < g_backend_count; ++i) {
        if (strcmp(g_backends[i]->id, id) == 0) {
            return g_backends[i];
        }
    }
    return g_backends[0];
}

RendererBackend* renderer_backend_default(void) {
    return g_backend_count > 0 ? g_backends[0] : NULL;
}