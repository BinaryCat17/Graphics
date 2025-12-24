#include "engine/graphics/stream.h"
#include "engine/graphics/render_system.h"
#include "engine/graphics/internal/backend/renderer_backend.h"
#include "engine/graphics/internal/resources/stream_internal.h"
#include "foundation/logger/logger.h"
#include <stdlib.h>

static size_t get_element_size(StreamType type, size_t custom_size) {
    switch (type) {
        case STREAM_FLOAT: return sizeof(float);
        case STREAM_VEC2:  return 2 * sizeof(float);
        case STREAM_VEC3:  return 3 * sizeof(float);
        case STREAM_VEC4:  return 4 * sizeof(float);
        case STREAM_MAT4:  return 16 * sizeof(float);
        case STREAM_INT:   return sizeof(int32_t);
        case STREAM_UINT:  return sizeof(uint32_t);
        case STREAM_CUSTOM: return custom_size;
        default: return 0;
    }
}

Stream* stream_create(RenderSystem* sys, StreamType type, size_t count, size_t custom_element_size) {
    if (!sys || count == 0) return NULL;
    
    RendererBackend* backend = render_system_get_backend(sys);
    if (!backend || !backend->buffer_create) {
        LOG_ERROR("Stream: Renderer backend not ready or buffers not supported.");
        return NULL;
    }

    size_t elem_size = get_element_size(type, custom_element_size);
    if (elem_size == 0) {
        LOG_ERROR("Stream: Invalid element size.");
        return NULL;
    }

    size_t total_size = elem_size * count;
    
    Stream* s = malloc(sizeof(Stream));
    if (!s) return NULL;

    s->sys = sys;
    s->backend = backend;
    s->type = type;
    s->count = count;
    s->element_size = elem_size;
    s->total_size = total_size;
    s->buffer_handle = NULL;
    
    if (!backend->buffer_create(backend, s)) {
        LOG_ERROR("Stream: Failed to allocate GPU buffer (%zu bytes).", total_size);
        free(s);
        return NULL;
    }

    LOG_TRACE("Stream created: %p (Count: %zu, Size: %zu bytes)", (void*)s, count, total_size);
    return s;
}

void stream_destroy(Stream* stream) {
    if (!stream) return;
    
    if (stream->backend && stream->backend->buffer_destroy) {
        stream->backend->buffer_destroy(stream->backend, stream);
    }
    
    free(stream);
}

bool stream_set_data(Stream* stream, const void* data, size_t count) {
    if (!stream || !data) return false;
    if (count > stream->count) {
        LOG_WARN("Stream: Attempt to write %zu elements into stream of size %zu", count, stream->count);
        return false;
    }
    
    if (!stream->backend->buffer_upload) return false;
    
    return stream->backend->buffer_upload(stream->backend, stream, data, count * stream->element_size, 0);
}

bool stream_read_back(Stream* stream, void* out_data, size_t count) {
    if (!stream || !out_data) return false;
    if (count > stream->count) count = stream->count; // Clamp
    
    if (!stream->backend->buffer_read) {
        LOG_ERROR("Stream: Backend does not support buffer readback.");
        return false;
    }
    
    return stream->backend->buffer_read(stream->backend, stream, out_data, count * stream->element_size, 0);
}

void stream_bind_compute(Stream* stream, uint32_t binding_slot) {
    if (!stream || !stream->backend->compute_bind_buffer) return;
    stream->backend->compute_bind_buffer(stream->backend, stream, binding_slot);
}

size_t stream_get_count(Stream* stream) {
    return stream ? stream->count : 0;
}

