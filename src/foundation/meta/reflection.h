#ifndef FOUNDATION_META_REFLECTION_H
#define FOUNDATION_META_REFLECTION_H

#include <stddef.h>
#include <stdint.h>

// Типы данных, которые поддерживает наша система рефлексии
typedef enum MetaTypeKind {
    META_TYPE_VOID = 0,
    META_TYPE_INT,
    META_TYPE_FLOAT,
    META_TYPE_BOOL,
    META_TYPE_STRING, // char* (null-terminated)
    META_TYPE_STRUCT, // Вложенная структура
    META_TYPE_ARRAY,  // Указатель + count (динамический массив)
    META_TYPE_POINTER // Указатель на структуру
} MetaTypeKind;

// Описание одного поля структуры
typedef struct MetaField {
    const char* name;
    MetaTypeKind type;
    size_t offset;         // Смещение от начала структуры (offsetof)
    const char* type_name; // Имя типа (для STRUCT/POINTER), например "MathNode"
} MetaField;

// Описание всей структуры
typedef struct MetaStruct {
    const char* name;      // Имя структуры, например "MathNode"
    size_t size;           // sizeof(MathNode)
    const MetaField* fields;
    size_t field_count;
} MetaStruct;

// Реестр типов
const MetaStruct* meta_get_struct(const char* name);

// Хелперы для доступа к данным через рефлексию
void* meta_get_field_ptr(void* instance, const MetaField* field);
int meta_get_int(void* instance, const MetaField* field);
float meta_get_float(void* instance, const MetaField* field);
const char* meta_get_string(void* instance, const MetaField* field);
void meta_set_int(void* instance, const MetaField* field, int value);
void meta_set_float(void* instance, const MetaField* field, float value);
void meta_set_string(void* instance, const MetaField* field, const char* value);

// Helper to find a field by name in a struct definition
const MetaField* meta_find_field(const MetaStruct* meta, const char* field_name);

#endif // FOUNDATION_META_REFLECTION_H
