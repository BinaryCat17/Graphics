#ifndef STREAM_INTERNAL_H
#define STREAM_INTERNAL_H

#include "engine/graphics/stream.h"

struct Stream {
    RenderSystem* sys;
    RendererBackend* backend;
    void* buffer_handle;
    
    StreamType type;
    size_t count;      // Capacity (number of elements)
    size_t element_size;
    size_t total_size; // Total size in bytes (count * element_size)
};

#endif // STREAM_INTERNAL_H
