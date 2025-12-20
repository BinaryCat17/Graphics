#include "foundation/memory/arena.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#define DEFAULT_ALIGNMENT 8

static uintptr_t align_forward(uintptr_t ptr, size_t align) {
    uintptr_t p, a, modulo;
    if ((align & (align - 1)) != 0) {
        return 0; // Error: alignment not power of 2
    }
    p = ptr;
    a = (uintptr_t)align;
    modulo = p & (a - 1);
    if (modulo != 0) {
        p += a - modulo;
    }
    return p;
}

bool arena_init(MemoryArena* arena, size_t size) {
    if (!arena) return false;
    arena->base = (uint8_t*)malloc(size);
    if (!arena->base) return false;
    arena->size = size;
    arena->offset = 0;
    arena->committed = size;
    return true;
}

void arena_destroy(MemoryArena* arena) {
    if (arena && arena->base) {
        free(arena->base);
        arena->base = NULL;
        arena->size = 0;
        arena->offset = 0;
    }
}

void arena_reset(MemoryArena* arena) {
    if (arena) arena->offset = 0;
}

void* arena_alloc(MemoryArena* arena, size_t size) {
    if (!arena || size == 0) return NULL;
    
    // Align current offset
    uintptr_t current_ptr = (uintptr_t)(arena->base + arena->offset);
    uintptr_t offset = align_forward(current_ptr, DEFAULT_ALIGNMENT);
    offset -= (uintptr_t)arena->base; // Convert back to relative offset

    if (offset + size > arena->size) {
        return NULL; // Out of memory
    }

    void* ptr = arena->base + offset;
    arena->offset = offset + size;
    return ptr;
}

void* arena_alloc_zero(MemoryArena* arena, size_t size) {
    void* ptr = arena_alloc(arena, size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

char* arena_push_string(MemoryArena* arena, const char* str) {
    if (!str) return NULL;
    size_t len = strlen(str);
    return arena_push_string_n(arena, str, len);
}

char* arena_push_string_n(MemoryArena* arena, const char* str, size_t n) {
    if (!arena || !str) return NULL;
    char* data = (char*)arena_alloc(arena, n + 1);
    if (data) {
        memcpy(data, str, n);
        data[n] = '\0';
    }
    return data;
}

char* arena_sprintf(MemoryArena* arena, const char* fmt, ...) {
    if (!arena || !fmt) return NULL;
    
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    if (len < 0) return NULL;

    char* data = (char*)arena_alloc(arena, len + 1);
    if (!data) return NULL;

    va_start(args, fmt);
    vsnprintf(data, len + 1, fmt, args);
    va_end(args);
    
    return data;
}

char* arena_printf(MemoryArena* arena, const char* fmt, ...) {
    if (!arena || !fmt) return NULL;
    
    va_list args;
    va_start(args, fmt);
    // Determine needed size
    va_list args_copy;
    va_copy(args_copy, args);
    int len = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    if (len < 0) {
        va_end(args);
        return NULL;
    }

    // Check capacity
    if (arena->offset + len + 1 > arena->size) {
        va_end(args);
        return NULL;
    }
    
    // Write directly to current offset
    char* ptr = (char*)(arena->base + arena->offset);
    // Note: vsnprintf writes the null terminator, so we will overwrite it on next call if we backup?
    // Actually, for a string builder, we usually want to overwrite the previous null terminator.
    // But `arena` logic is append-only.
    // Let's just write. The user will have a bunch of null-terminated strings one after another.
    // Wait, for Transpiler we want ONE big string.
    // So `arena_printf` is NOT suitable for building a single string unless we manually manage the null terminator.
    
    // Better approach for Transpiler: Use `arena` as the backing buffer for a custom StringBuilder.
    // But let's keep it simple.
    
    vsnprintf(ptr, len + 1, fmt, args);
    arena->offset += len; // We advance by len, NOT len+1, to effectively overwrite the null terminator next time? 
                          // No, `vsnprintf` always writes \0.
                          // If we want to concatenate, we should probably backup 1 byte if the previous alloc was a string?
                          // Too complex for generic arena.
                          
    // Let's just advance full length including \0.
    // If the user wants to concatenate, they should use a specific StringBuilder logic.
    // BUT, for this task, I will implement a `ArenaStringBuilder` in transpiler.c that uses the arena.
    
    arena->offset += (len + 1);
    
    va_end(args);
    return ptr;
}
