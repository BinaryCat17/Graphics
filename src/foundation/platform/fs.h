#ifndef PLATFORM_FS_H
#define PLATFORM_FS_H

#include "foundation/memory/arena.h"
#include <stdbool.h>

typedef struct PlatformDir PlatformDir;

typedef struct PlatformDirEntry {
    char* name;
    bool is_dir;
} PlatformDirEntry;

char* fs_read_text(MemoryArena* arena, const char* path);

PlatformDir* platform_dir_open(const char* path);
bool platform_dir_read(PlatformDir* dir, PlatformDirEntry* out_entry);
void platform_dir_close(PlatformDir* dir);

bool platform_mkdir(const char* path);
bool platform_remove_file(const char* path);

// Reads a binary file into a raw buffer allocated from the arena (or heap if arena is NULL).
// If arena is NULL, the caller must free() the result.
// Returns NULL on failure.
void* fs_read_bin(MemoryArena* arena, const char* path, size_t* out_size);

#endif // PLATFORM_FS_H
