#pragma once

#include <stdint.h>

typedef uint32_t StringId;

// FNV-1a Hash
StringId str_id(const char* str);
