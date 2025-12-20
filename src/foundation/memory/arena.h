#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct MemoryArena {
    uint8_t* base;
    size_t size;
    size_t offset;
    size_t committed; // For future virtual memory support
} MemoryArena;

// Initialize an arena with a fixed size block allocated from heap
bool arena_init(MemoryArena* arena, size_t size);

// Free the underlying memory
void arena_destroy(MemoryArena* arena);

// Reset the offset to 0 (does not free memory)
void arena_reset(MemoryArena* arena);

// Allocate 'size' bytes from the arena. Returns NULL if out of memory.
void* arena_alloc(MemoryArena* arena, size_t size);

// Allocate and zero-initialize
void* arena_alloc_zero(MemoryArena* arena, size_t size);

// Utilities
char* arena_push_string(MemoryArena* arena, const char* str);
char* arena_push_string_n(MemoryArena* arena, const char* str, size_t n);
char* arena_sprintf(MemoryArena* arena, const char* fmt, ...);

// Prints formatted string into the arena, advancing the offset. Returns ptr to start of printed string.
char* arena_printf(MemoryArena* arena, const char* fmt, ...);

#endif // ARENA_H
