#ifndef GPU_BUFFER_H
#define GPU_BUFFER_H

#include <stddef.h>
#include <stdint.h>

typedef enum GpuBufferType {
    GPU_BUFFER_VERTEX,
    GPU_BUFFER_INDEX,
    GPU_BUFFER_UNIFORM,
    GPU_BUFFER_STORAGE, // For Compute/Instancing
} GpuBufferType;

typedef struct GpuBuffer {
    uint32_t id; // Internal backend handle
    GpuBufferType type;
    size_t size;
    void* mapped_data; // If accessible from CPU
} GpuBuffer;

// API to be implemented by backend
GpuBuffer* gpu_buffer_create(size_t size, GpuBufferType type);
void gpu_buffer_update(GpuBuffer* buffer, const void* data, size_t size);
void gpu_buffer_destroy(GpuBuffer* buffer);

#endif // GPU_BUFFER_H
