#include "stream.h"
#include "render_system.h"
#include "internal/renderer_backend.h"
#include "foundation/logger/logger.h"
#include <stdlib.h>

struct Stream {
    RenderSystem* sys;
    RendererBackend* backend;
    void* buffer_handle;
    
    StreamType type;
    size_t count;      // Capacity (number of elements)
    size_t element_size;
};

static size_t get_element_size(StreamType type) {
    switch (type) {
        case STREAM_FLOAT: return sizeof(float);
        case STREAM_VEC2:  return 2 * sizeof(float);
        case STREAM_VEC3:  return 3 * sizeof(float); // Note: std430 alignment might be tricky, but assuming tight pack for now
        case STREAM_VEC4:  return 4 * sizeof(float);
        case STREAM_MAT4:  return 16 * sizeof(float);
        case STREAM_INT:   return sizeof(int32_t);
        case STREAM_UINT:  return sizeof(uint32_t);
        default: return 0;
    }
}

Stream* stream_create(RenderSystem* sys, StreamType type, size_t count) {
    if (!sys || count == 0) return NULL;
    
    RendererBackend* backend = render_system_get_backend(sys);
    if (!backend || !backend->buffer_create) {
        LOG_ERROR("Stream: Renderer backend not ready or buffers not supported.");
        return NULL;
    }

    size_t elem_size = get_element_size(type);
    size_t total_size = elem_size * count;
    
    void* handle = backend->buffer_create(backend, total_size);
    if (!handle) {
        LOG_ERROR("Stream: Failed to allocate GPU buffer (%zu bytes).", total_size);
        return NULL;
    }

    Stream* s = malloc(sizeof(Stream));
    s->sys = sys;
    s->backend = backend;
    s->buffer_handle = handle;
    s->type = type;
    s->count = count;
    s->element_size = elem_size;
    
    LOG_TRACE("Stream created: %p (Count: %zu, Size: %zu bytes)", (void*)s, count, total_size);
    return s;
}

void stream_destroy(Stream* stream) {
    if (!stream) return;
    
    if (stream->backend && stream->backend->buffer_destroy) {
        stream->backend->buffer_destroy(stream->backend, stream->buffer_handle);
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
    
    return stream->backend->buffer_upload(stream->backend, stream->buffer_handle, data, count * stream->element_size, 0);
}

bool stream_read_back(Stream* stream, void* out_data, size_t count) {
    if (!stream || !out_data) return false;
    if (count > stream->count) count = stream->count; // Clamp
    
    if (!stream->backend->buffer_read) {
        LOG_ERROR("Stream: Backend does not support buffer readback.");
        return false;
    }
    
    return stream->backend->buffer_read(stream->backend, stream->buffer_handle, out_data, count * stream->element_size, 0);
}

void stream_bind_compute(Stream* stream, uint32_t binding_slot) {
    if (!stream || !stream->backend->compute_bind_buffer) return;
    stream->backend->compute_bind_buffer(stream->backend, stream->buffer_handle, binding_slot);
}

size_t stream_get_count(Stream* stream) {
    return stream ? stream->count : 0;
}
