#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Ensures a heap-allocated buffer has at least the requested capacity.
 *
 * @param buffer Pointer to the buffer pointer to grow (realloc'ed on demand).
 * @param element_size Size in bytes of a single element stored in the buffer.
 * @param capacity Pointer to the current capacity counter associated with the buffer.
 * @param required Minimum number of elements the caller intends to store.
 * @param initial_capacity Capacity to fall back to when the buffer is empty.
 * @return 0 on success, non-zero if allocation fails or arguments are invalid.
 */
int ensure_capacity(void **buffer, size_t element_size, size_t *capacity, size_t required, size_t initial_capacity);

#ifdef __cplusplus
}
#endif

