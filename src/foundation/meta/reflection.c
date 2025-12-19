#include "reflection.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void* meta_get_field_ptr(void* instance, const MetaField* field) {
    if (!instance || !field) return NULL;
    return (char*)instance + field->offset;
}

int meta_get_int(void* instance, const MetaField* field) {
    if (!instance || !field || field->type != META_TYPE_INT) return 0;
    return *(int*)((char*)instance + field->offset);
}

float meta_get_float(void* instance, const MetaField* field) {
    if (!instance || !field || field->type != META_TYPE_FLOAT) return 0.0f;
    return *(float*)((char*)instance + field->offset);
}

const char* meta_get_string(void* instance, const MetaField* field) {
    if (!instance || !field) return NULL;
    if (field->type == META_TYPE_STRING) {
        return *(char**)((char*)instance + field->offset);
    } else if (field->type == META_TYPE_STRING_ARRAY) {
        return (char*)instance + field->offset;
    }
    return NULL;
}

bool meta_get_bool(void* instance, const MetaField* field) {
    if (!instance || !field || field->type != META_TYPE_BOOL) return false;
    return *(bool*)((char*)instance + field->offset);
}

void meta_set_int(void* instance, const MetaField* field, int value) {
    if (!instance || !field || field->type != META_TYPE_INT) return;
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
            strcpy(ptr, value); 
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