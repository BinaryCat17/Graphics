#ifndef STREAM_H
#define STREAM_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct Stream Stream;
typedef struct RenderSystem RenderSystem;

// Тип данных в потоке (для валидации и метаданных)
typedef enum {
    STREAM_FLOAT = 0,
    STREAM_VEC2,
    STREAM_VEC3,
    STREAM_VEC4,
    STREAM_MAT4,
    STREAM_INT,
    STREAM_UINT,
    STREAM_CUSTOM // User-defined struct
} StreamType;

// --- Жизненный цикл ---

// Создает поток данных (SSBO) на GPU.
// count: количество элементов.
// type: тип элемента.
// element_size: размер одного элемента в байтах (игнорируется для стандартных типов, обязателен для STREAM_CUSTOM).
Stream* stream_create(RenderSystem* sys, StreamType type, size_t count, size_t element_size);

// Уничтожает поток.
void stream_destroy(Stream* stream);

// --- Манипуляция данными ---

// Загружает данные с CPU на GPU.
// data: указатель на массив данных.
// count: количество элементов для копирования (должно быть <= stream.capacity).
bool stream_set_data(Stream* stream, const void* data, size_t count);

// Читает данные с GPU на CPU (блокирующая операция, медленно!).
// out_data: буфер назначения.
bool stream_read_back(Stream* stream, void* out_data, size_t count);

// --- Использование ---

// Привязывает поток к слоту (binding=N) для Compute Shader.
void stream_bind_compute(Stream* stream, uint32_t binding_slot);

// Возвращает количество элементов (емкость).
size_t stream_get_count(Stream* stream);

#endif // STREAM_H
