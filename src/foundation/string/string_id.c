#include "string_id.h"

#define FNV1A_OFFSET_32 2166136261u
#define FNV1A_PRIME_32 16777619u

// --- Debug String Registry ---
#ifndef NDEBUG
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

typedef struct StringEntry {
    StringId id;
    char* str;
    struct StringEntry* next;
} StringEntry;

#define BUCKET_COUNT 4096
static StringEntry* g_buckets[BUCKET_COUNT] = {0};
static atomic_flag g_lock = ATOMIC_FLAG_INIT;

static void registry_add(StringId id, const char* str) {
    if (!str) return;

    size_t index = id % BUCKET_COUNT;

    // Spinlock
    while (atomic_flag_test_and_set(&g_lock)) {
        // busy wait
    }

    // Check if exists
    StringEntry* entry = g_buckets[index];
    while (entry) {
        if (entry->id == id) {
            atomic_flag_clear(&g_lock);
            return; // Already registered
        }
        entry = entry->next;
    }

    // Add new
    StringEntry* new_entry = (StringEntry*)malloc(sizeof(StringEntry));
    if (new_entry) {
        new_entry->id = id;
        new_entry->str =
#ifdef _WIN32
            _strdup(str);
#else
            strdup(str);
#endif
        new_entry->next = g_buckets[index];
        g_buckets[index] = new_entry;
    }

    atomic_flag_clear(&g_lock);
}

const char* str_id_lookup(StringId id) {
    size_t index = id % BUCKET_COUNT;

    // Spinlock for read safety
    while (atomic_flag_test_and_set(&g_lock)) {}

    StringEntry* entry = g_buckets[index];
    while (entry) {
        if (entry->id == id) {
            atomic_flag_clear(&g_lock);
            return entry->str;
        }
        entry = entry->next;
    }

    atomic_flag_clear(&g_lock);
    return "<UNKNOWN>";
}
#endif

StringId str_id(const char* str) {
    if (!str) return 0;
    
    StringId hash = FNV1A_OFFSET_32;
    const char* ptr = str;
    while (*ptr) {
        hash ^= (StringId)(*ptr++);
        hash *= FNV1A_PRIME_32;
    }

#ifndef NDEBUG
    registry_add(hash, str);
#endif

    return hash;
}
