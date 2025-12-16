#pragma once

#include <stddef.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum MemBufferGrowthStrategy {
    MEM_BUFFER_GROWTH_DOUBLE = 2,
} MemBufferGrowthStrategy;

int ensure_capacity(void **buffer, size_t element_size, size_t *capacity, size_t required, size_t initial_capacity,
                    MemBufferGrowthStrategy growth_strategy);

#define MEM_BUFFER_DECLARE(Type, DefaultCapacity) \
    static inline int Type##_mem_init(Type *buffer, size_t initial_capacity) \
    { \
        if (!buffer) { \
            return -1; \
        } \
        buffer->data = NULL; \
        buffer->count = 0; \
        buffer->capacity = 0; \
        if (initial_capacity == 0) { \
            return 0; \
        } \
        return ensure_capacity((void **)&buffer->data, sizeof(*buffer->data), &buffer->capacity, initial_capacity, \
                               (DefaultCapacity), MEM_BUFFER_GROWTH_DOUBLE); \
    } \
 \
    static inline void Type##_mem_dispose(Type *buffer) \
    { \
        if (!buffer) { \
            return; \
        } \
        free(buffer->data); \
        buffer->data = NULL; \
        buffer->count = 0; \
        buffer->capacity = 0; \
    } \
 \
    static inline int Type##_mem_reserve(Type *buffer, size_t required) \
    { \
        if (!buffer) { \
            return -1; \
        } \
        return ensure_capacity((void **)&buffer->data, sizeof(*buffer->data), &buffer->capacity, required, (DefaultCapacity), \
                               MEM_BUFFER_GROWTH_DOUBLE); \
    }

#ifdef __cplusplus
}
#endif

