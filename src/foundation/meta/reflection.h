#ifndef FOUNDATION_META_REFLECTION_H
#define FOUNDATION_META_REFLECTION_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Типы данных, которые поддерживает наша система рефлексии
typedef enum MetaType {
    META_TYPE_INT,
    META_TYPE_FLOAT,
    META_TYPE_STRING,        // char* (pointer)
    META_TYPE_STRING_ARRAY,  // char[N] (inline buffer)
    META_TYPE_BOOL,
    META_TYPE_STRUCT,
    META_TYPE_ENUM,
    META_TYPE_POINTER,
    META_TYPE_POINTER_ARRAY, // e.g. MathNode**
    META_TYPE_STRING_ID      // StringId (uint32_t hash)
} MetaType;
// Описание одного значения enum (например: "UI_LAYOUT_ROW" -> 1)
typedef struct MetaEnumValue {
    const char* name;
    int value;
} MetaEnumValue;

// Описание типа Enum
typedef struct MetaEnum {
    const char* name;
    const MetaEnumValue* values;
    size_t count;
} MetaEnum;

// Описание одного поля структуры
typedef struct MetaField {
    const char* name;
    MetaType type;
    size_t offset;         
    const char* type_name; // Имя типа (для STRUCT/ENUM)
} MetaField;

// Описание всей структуры
typedef struct MetaStruct {
    const char* name;      
    size_t size;           
    const MetaField* fields;
    size_t field_count;
} MetaStruct;

// Реестр типов
const MetaStruct* meta_get_struct(const char* name);
const MetaEnum* meta_get_enum(const char* name);

// Хелперы
void* meta_get_field_ptr(void* instance, const MetaField* field);
int meta_get_int(const void* instance, const MetaField* field);
float meta_get_float(const void* instance, const MetaField* field);
const char* meta_get_string(const void* instance, const MetaField* field);
bool meta_get_bool(const void* instance, const MetaField* field);
void meta_set_int(void* instance, const MetaField* field, int value);
void meta_set_float(void* instance, const MetaField* field, float value);
void meta_set_string(void* instance, const MetaField* field, const char* value);
void meta_set_bool(void* instance, const MetaField* field, bool value);

// Helper to find a field by name in a struct definition
const MetaField* meta_find_field(const MetaStruct* meta, const char* field_name);
// Helper to find enum value by string name (returns true if found)
bool meta_enum_get_value(const MetaEnum* meta_enum, const char* name_str, int* out_value);

// Helper to find enum name by value
const char* meta_enum_get_name(const MetaEnum* meta_enum, int value);

// Sets a field value parsing it from a string representation.
// Returns true if parsing/setting was successful.
bool meta_set_from_string(void* instance, const MetaField* field, const char* value_str);

#endif // FOUNDATION_META_REFLECTION_H
