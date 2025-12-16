#include "core/memory/buffer.h"

#include <stdlib.h>

int ensure_capacity(void **buffer, size_t element_size, size_t *capacity, size_t required, size_t initial_capacity,
                    MemBufferGrowthStrategy growth_strategy)
{
    if (!buffer || !capacity || element_size == 0) {
        return -1;
    }

    if (required <= *capacity) {
        return 0;
    }

    size_t growth_factor = (size_t)growth_strategy;
    if (growth_factor < 2) {
        growth_factor = 2;
    }

    size_t new_capacity = *capacity == 0 ? initial_capacity : (*capacity * growth_factor);
    if (new_capacity == 0) {
        new_capacity = required;
    }

    while (new_capacity < required) {
        size_t previous = new_capacity;
        new_capacity *= growth_factor;
        if (new_capacity < previous) {
            return -1;
        }
    }

    void *expanded = realloc(*buffer, new_capacity * element_size);
    if (!expanded) {
        return -1;
    }

    *buffer = expanded;
    *capacity = new_capacity;
    return 0;
}

