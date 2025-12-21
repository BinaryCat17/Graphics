#include "reflection.h"
#include "foundation/string/string_id.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

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
        default:
            return false;
    }
}
