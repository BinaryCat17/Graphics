#pragma once

#include <stddef.h>

#include "engine/render/backend/common/render_composition.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct UiVertex {
    float position[3];
    Color color;
} UiVertex;

typedef struct UiVertexBuffer {
    union {
        UiVertex *data;
        UiVertex *vertices;
    };
    size_t count;
    size_t capacity;
} UiVertexBuffer;

typedef struct UiTextVertex {
    float position[3];
    float uv[2];
    Color color;
} UiTextVertex;

typedef struct UiTextVertexBuffer {
    union {
        UiTextVertex *data;
        UiTextVertex *vertices;
    };
    size_t count;
    size_t capacity;
} UiTextVertexBuffer;

int ui_vertex_buffer_init(UiVertexBuffer *buffer, size_t initial_capacity);
void ui_vertex_buffer_dispose(UiVertexBuffer *buffer);
int ui_vertex_buffer_reserve(UiVertexBuffer *buffer, size_t vertex_capacity);

int ui_text_vertex_buffer_init(UiTextVertexBuffer *buffer, size_t initial_capacity);
void ui_text_vertex_buffer_dispose(UiTextVertexBuffer *buffer);
int ui_text_vertex_buffer_reserve(UiTextVertexBuffer *buffer, size_t vertex_capacity);

int renderer_fill_vertices(Renderer *renderer, const ViewModel *view_models, size_t view_model_count, const GlyphQuad *glyphs,
                           size_t glyph_count, UiVertexBuffer *background_buffer, UiTextVertexBuffer *text_buffer);
int renderer_fill_background_vertices(Renderer *renderer, const ViewModel *view_models, size_t view_model_count,
                                      UiVertexBuffer *vertex_buffer);
int renderer_fill_text_vertices(Renderer *renderer, const GlyphQuad *glyphs, size_t glyph_count,
                                UiTextVertexBuffer *vertex_buffer);

#ifdef __cplusplus
}
#endif

