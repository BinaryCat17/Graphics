#ifndef STRING_ID_H
#define STRING_ID_H

#include <stdint.h>

typedef uint32_t StringId;

// FNV-1a Hash
StringId str_id(const char* str);

#ifndef NDEBUG
/**
 * @brief (Debug Only) Retrieves the original string for a given StringId.
 * @param id The hash to look up.
 * @return The original string, or "<UNKNOWN>" if not found.
 */
const char* str_id_lookup(StringId id);
#endif

#endif // STRING_ID_H
