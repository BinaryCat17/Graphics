#include "foundation/platform/fs.h"
#include "foundation/platform/platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

char* platform_strdup(const char* src) {
    if (!src) return NULL;
    size_t len = strlen(src) + 1;
    char* dest = (char*)malloc(len);
    if (dest) {
        memcpy(dest, src, len);
    }
    return dest;
}

void platform_strncpy(char* dest, const char* src, size_t count) {
    if (!dest) return;
    // Standard strncpy behavior: copy up to count, pad with nulls
    size_t i;
    for (i = 0; i < count && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < count; i++) {
        dest[i] = '\0';
    }
}

FILE* platform_fopen(const char* filename, const char* mode) {
#ifdef _MSC_VER
    FILE* f = NULL;
    if (fopen_s(&f, filename, mode) != 0) return NULL;
    return f;
#else
    return fopen(filename, mode);
#endif
}

char* fs_read_text(MemoryArena* arena, const char* path) {
    if (!path) return NULL;
    FILE *f = platform_fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *text = NULL;
    if (arena) {
        text = (char*)arena_alloc(arena, (size_t)len + 1);
    } else {
        text = (char*)malloc((size_t)len + 1);
    }

    if (!text) {
        fclose(f);
        return NULL;
    }
    fread(text, 1, (size_t)len, f);
    text[len] = 0;
    fclose(f);
    return text;
}

struct PlatformDir {
#ifdef _WIN32
    HANDLE handle;
    WIN32_FIND_DATAA first_data;
    int has_first;
#else
    DIR* dir;
#endif
    char* base_path;
};

static char* join_path(const char* dir, const char* leaf) {
    if (!dir || !leaf) return NULL;
    size_t dir_len = strlen(dir);
    while (dir_len > 0 && (dir[dir_len - 1] == '/' || dir[dir_len - 1] == '\\')) dir_len--;
    size_t leaf_len = strlen(leaf);
    size_t total = dir_len + 1 + leaf_len + 1;
    char* out = (char*)malloc(total);
    if (!out) return NULL;
    memcpy(out, dir, dir_len);
    out[dir_len] = '/';
    memcpy(out + dir_len + 1, leaf, leaf_len);
    out[total - 1] = 0;
    return out;
}

PlatformDir* platform_dir_open(const char* path) {
    if (!path) return NULL;
    PlatformDir* dir = (PlatformDir*)calloc(1, sizeof(PlatformDir));
    if (!dir) return NULL;
    dir->base_path = strdup(path);
    if (!dir->base_path) {
        free(dir);
        return NULL;
    }
#ifdef _WIN32
    size_t base_len = strlen(path);
    while (base_len > 0 && (path[base_len - 1] == '/' || path[base_len - 1] == '\\')) base_len--;
    size_t pattern_len = base_len + 3;
    char* pattern = (char*)malloc(pattern_len);
    if (!pattern) {
        free(dir->base_path);
        free(dir);
        return NULL;
    }
    memcpy(pattern, path, base_len);
    pattern[base_len] = '\\';
    pattern[base_len + 1] = '*';
    pattern[base_len + 2] = 0;

    dir->handle = FindFirstFileA(pattern, &dir->first_data);
    free(pattern);
    if (dir->handle == INVALID_HANDLE_VALUE) {
        free(dir->base_path);
        free(dir);
        return NULL;
    }
    dir->has_first = 1;
#else
    dir->dir = opendir(path);
    if (!dir->dir) {
        free(dir->base_path);
        free(dir);
        return NULL;
    }
#endif
    return dir;
}

static bool populate_entry(const char* name, bool is_dir, PlatformDirEntry* out_entry) {
    if (!out_entry) return false;
    out_entry->name = strdup(name);
    if (!out_entry->name) return false;
    out_entry->is_dir = is_dir;
    return true;
}

bool platform_dir_read(PlatformDir* dir, PlatformDirEntry* out_entry) {
    if (!dir || !out_entry) return false;
#ifdef _WIN32
    WIN32_FIND_DATAA data;
    int has_data = 0;
    if (dir->has_first) {
        data = dir->first_data;
        dir->has_first = 0;
        has_data = 1;
    }
    while (!has_data && FindNextFileA(dir->handle, &data)) {
        has_data = 1;
    }
    if (!has_data) return false;
    const char* name = data.cFileName;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return platform_dir_read(dir, out_entry);
    bool is_dir = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    if (!populate_entry(name, is_dir, out_entry)) return false;
    return true;
#else
    struct dirent* ent = NULL;
    while ((ent = readdir(dir->dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        bool is_dir = ent->d_type == DT_DIR;
        if (ent->d_type == DT_UNKNOWN) {
            char* full = join_path(dir->base_path, ent->d_name);
            if (full) {
                struct stat st;
                if (stat(full, &st) == 0) is_dir = S_ISDIR(st.st_mode);
                free(full);
            }
        }
        if (populate_entry(ent->d_name, is_dir, out_entry)) return true;
        return false;
    }
    return false;
#endif
}

void platform_dir_close(PlatformDir* dir) {
    if (!dir) return;
#ifdef _WIN32
    if (dir->handle != INVALID_HANDLE_VALUE) FindClose(dir->handle);
#else
    if (dir->dir) closedir(dir->dir);
#endif
    free(dir->base_path);
    free(dir);
}

bool platform_mkdir(const char* path) {
    if (!path) return false;
#ifdef _WIN32
    return CreateDirectoryA(path, NULL) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
#else
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        return mkdir(path, 0700) == 0;
    }
    return S_ISDIR(st.st_mode);
#endif
}

bool platform_remove_file(const char* path) {
    if (!path) return false;
#ifdef _WIN32
    return DeleteFileA(path) != 0;
#else
    return unlink(path) == 0;
#endif
}