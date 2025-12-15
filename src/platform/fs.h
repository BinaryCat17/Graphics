#ifndef PLATFORM_FS_H
#define PLATFORM_FS_H

#include <stdbool.h>

typedef struct PlatformDir PlatformDir;

typedef struct PlatformDirEntry {
    char* name;
    bool is_dir;
} PlatformDirEntry;

PlatformDir* platform_dir_open(const char* path);
bool platform_dir_read(PlatformDir* dir, PlatformDirEntry* out_entry);
void platform_dir_close(PlatformDir* dir);

#endif // PLATFORM_FS_H
