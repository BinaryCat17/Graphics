#include "engine/graphics/renderer_backend.h"
#include <string.h>
#include <stdlib.h>

#define MAX_BACKENDS 8
static RendererBackend* registry[MAX_BACKENDS] = {0};
static int registry_count = 0;

bool render_logger_init(RenderLogger* logger, const RenderLoggerConfig* config, const char* backend_id) {
    if (!logger || !config) return false;
    memset(logger, 0, sizeof(RenderLogger));
    
    logger->backend_id = backend_id;
    logger->sink_type = config->sink_type;
    logger->level = config->level;
    logger->ring_capacity = config->ring_capacity;

    if (logger->sink_type == RENDER_LOG_SINK_FILE && config->sink_target) {
        logger->file = platform_fopen(config->sink_target, "w");
    }

    if (logger->ring_capacity > 0) {
        logger->ring_entries = (RenderLogEntry*)calloc(logger->ring_capacity, sizeof(RenderLogEntry));
    }

    return true;
}

void render_logger_log(RenderLogger* logger, RenderLogLevel level, const char* command, const char* parameters, double duration_ms) {
    if (!logger) return;
    if (level > logger->level) return;

    if (logger->sink_type == RENDER_LOG_SINK_STDOUT) {
        const char* lvl_str = (level == RENDER_LOG_INFO) ? "[INFO]" : "[CMD]";
        printf("%s [%s] %s (%s) %.3fms\n", lvl_str, logger->backend_id, command, parameters ? parameters : "", duration_ms);
    } else if (logger->sink_type == RENDER_LOG_SINK_FILE && logger->file) {
        fprintf(logger->file, "[%s] %s (%s) %.3fms\n", logger->backend_id, command, parameters ? parameters : "", duration_ms);
    }
}

void render_logger_cleanup(RenderLogger* logger) {
    if (!logger) return;
    if (logger->file) {
        fclose(logger->file);
    }
    if (logger->ring_entries) {
        free(logger->ring_entries);
    }
}

bool renderer_backend_register(RendererBackend* backend) {
    if (!backend || registry_count >= MAX_BACKENDS) return false;
    registry[registry_count++] = backend;
    return true;
}

RendererBackend* renderer_backend_get(const char* id) {
    if (!id) return NULL;
    for (int i = 0; i < registry_count; ++i) {
        if (strcmp(registry[i]->id, id) == 0) {
            return registry[i];
        }
    }
    return NULL;
}