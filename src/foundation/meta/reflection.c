#include "reflection.h"
#include "foundation/string/string_id.h"
#include "foundation/math/math_types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static bool parse_vec_from_string(const char* str, float* out, int count) {
    if (!str || !out || count < 1 || count > 4) return false;
    
    // Hex Color support (only for Vec3 and Vec4)
    if (str[0] == '#') {
        unsigned int r = 0, g = 0, b = 0, a = 255;
        size_t len = strlen(str);
        bool parsed = false;
        
        if (len == 7) { // #RRGGBB
            if (sscanf(str + 1, "%02x%02x%02x", &r, &g, &b) == 3) parsed = true;
        } else if (len == 9) { // #RRGGBBAA
            if (sscanf(str + 1, "%02x%02x%02x%02x", &r, &g, &b, &a) == 4) parsed = true;
        }

        if (parsed) {
            if (count >= 1) out[0] = (float)r / 255.0f;
            if (count >= 2) out[1] = (float)g / 255.0f;
            if (count >= 3) out[2] = (float)b / 255.0f;
            if (count >= 4) out[3] = (float)a / 255.0f;
            return true;
        }
        return false;
    }

    // Space separated floats
    // Simple scan logic
    int scanned = 0;
    if (count == 2) scanned = sscanf(str, "%f %f", &out[0], &out[1]);
    else if (count == 3) scanned = sscanf(str, "%f %f %f", &out[0], &out[1], &out[2]);
    else if (count == 4) scanned = sscanf(str, "%f %f %f %f", &out[0], &out[1], &out[2], &out[3]);
    
    // Allow implicit W=1.0 for Vec4 if only 3 components provided
    if (count == 4 && scanned == 3) {
        out[3] = 1.0f;
        return true;
    }
    
    return scanned == count;
}

void* meta_get_field_ptr(void* instance, const MetaField* field) {
    if (!instance || !field) return NULL;
    return (char*)instance + field->offset;
}

int meta_get_int(const void* instance, const MetaField* field) {
    if (!instance || !field || field->type != META_TYPE_INT) return 0;
    return *(int*)((char*)instance + field->offset);
}

float meta_get_float(const void* instance, const MetaField* field) {
    if (!instance || !field || field->type != META_TYPE_FLOAT) return 0.0f;
    return *(float*)((char*)instance + field->offset);
}

const char* meta_get_string(const void* instance, const MetaField* field) {
    if (!instance || !field) return NULL;
    if (field->type == META_TYPE_STRING) {
        return *(char**)((char*)instance + field->offset);
    } else if (field->type == META_TYPE_STRING_ARRAY) {
        return (char*)instance + field->offset;
    }
    return NULL;
}

bool meta_get_bool(const void* instance, const MetaField* field) {
    if (!instance || !field || field->type != META_TYPE_BOOL) return false;
    return *(bool*)((char*)instance + field->offset);
}

void meta_set_int(void* instance, const MetaField* field, int value) {
    if (!instance || !field || (field->type != META_TYPE_INT && field->type != META_TYPE_ENUM)) return;
    *(int*)((char*)instance + field->offset) = value;
}

void meta_set_float(void* instance, const MetaField* field, float value) {
    if (!instance || !field || field->type != META_TYPE_FLOAT) return;
    *(float*)((char*)instance + field->offset) = value;
}

void meta_set_string(void* instance, const MetaField* field, const char* value) {
    if (!instance || !field) return;
    
    if (field->type == META_TYPE_STRING) {
        char** ptr = (char**)((char*)instance + field->offset);
        if (*ptr) free(*ptr); // Освобождаем старую строку (предполагаем владение)
        if (value) {
            size_t len = strlen(value);
            *ptr = (char*)malloc(len + 1);
            if (*ptr) memcpy(*ptr, value, len + 1);
        } else {
            *ptr = NULL;
        }
    } else if (field->type == META_TYPE_STRING_ARRAY) {
        char* ptr = (char*)instance + field->offset;
        if (value) {
            // Using a safe default limit since MetaField doesn't store array size yet.
            // Assuming 256 is enough for most names/paths in this engine.
            strncpy(ptr, value, 255);
            ptr[255] = '\0';
        } else {
            ptr[0] = '\0';
        }
    }
}

void meta_set_bool(void* instance, const MetaField* field, bool value) {
    if (!instance || !field || field->type != META_TYPE_BOOL) return;
    *(bool*)((char*)instance + field->offset) = value;
}

const MetaField* meta_find_field(const MetaStruct* meta, const char* field_name) {
    if (!meta || !field_name) return NULL;
    for (size_t i = 0; i < meta->field_count; ++i) {
        if (strcmp(meta->fields[i].name, field_name) == 0) {
            return &meta->fields[i];
        }
    }
    return NULL;
}

const char* meta_enum_get_name(const MetaEnum* meta_enum, int value) {
    if (!meta_enum) return NULL;
    for (size_t i = 0; i < meta_enum->count; ++i) {
        if (meta_enum->values[i].value == value) {
            return meta_enum->values[i].name;
        }
    }
    return NULL;
}

bool meta_set_from_string(void* instance, const MetaField* field, const char* value_str) {
    if (!instance || !field || !value_str) return false;

    switch (field->type) {
        case META_TYPE_INT: {
            meta_set_int(instance, field, atoi(value_str));
            return true;
        }
        case META_TYPE_FLOAT: {
            meta_set_float(instance, field, (float)atof(value_str));
            return true;
        }
        case META_TYPE_BOOL: {
            bool val = (strcmp(value_str, "true") == 0 || strcmp(value_str, "1") == 0);
            meta_set_bool(instance, field, val);
            return true;
        }
        case META_TYPE_STRING:
        case META_TYPE_STRING_ARRAY: {
            meta_set_string(instance, field, value_str);
            return true;
        }
        case META_TYPE_ENUM: {
            const MetaEnum* e = meta_get_enum(field->type_name);
            int enum_val = 0;
            if (e && meta_enum_get_value(e, value_str, &enum_val)) {
                // Assuming enums are stored as ints
                *(int*)((char*)instance + field->offset) = enum_val;
                return true;
            }
            return false;
        }
        case META_TYPE_STRING_ID: {
            StringId id = str_id(value_str);
            // StringId is uint32_t
            *(uint32_t*)((char*)instance + field->offset) = id;
            return true;
        }
        case META_TYPE_VEC2: {
            return parse_vec_from_string(value_str, (float*)((char*)instance + field->offset), 2);
        }
        case META_TYPE_VEC3: {
            return parse_vec_from_string(value_str, (float*)((char*)instance + field->offset), 3);
        }
        case META_TYPE_VEC4: {
            return parse_vec_from_string(value_str, (float*)((char*)instance + field->offset), 4);
        }
        case META_TYPE_FLAGS: {
            const MetaEnum* e = meta_get_enum(field->type_name);
            if (!e) return false;

            uint32_t final_mask = 0;
            
            // Work on a copy
            char buf[256];
            strncpy(buf, value_str, 255);
            buf[255] = '\0';
            
            char* start = buf;
            while (*start) {
                // 1. Skip leading spaces
                while (*start == ' ' || *start == '\t') start++;
                if (*start == '\0') break;

                // 2. Find end of token
                char* end = start;
                while (*end && *end != '|') end++;

                // 3. Trim trailing spaces
                char* token_end = end;
                while (token_end > start && (*(token_end - 1) == ' ' || *(token_end - 1) == '\t')) {
                    token_end--;
                }
                
                // Temporarily null-terminate to lookup
                char saved = *token_end;
                *token_end = '\0';
                
                int enum_val = 0;
                if (meta_enum_get_value(e, start, &enum_val)) {
                    final_mask |= (uint32_t)enum_val;
                }
                
                // Restore if needed (though we move past it anyway)
                *token_end = saved;

                // 4. Advance
                if (*end == '|') start = end + 1;
                else start = end;
            }
            
            *(uint32_t*)((char*)instance + field->offset) = final_mask;
            return true;
        }
        default:
            return false;
    }
}
