#include "string_id.h"

#define FNV1A_OFFSET_32 2166136261u
#define FNV1A_PRIME_32 16777619u

StringId str_id(const char* str) {
    if (!str) return 0;
    
    StringId hash = FNV1A_OFFSET_32;
    while (*str) {
        hash ^= (StringId)(*str++);
        hash *= FNV1A_PRIME_32;
    }
    return hash;
}
