#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Basic 2D vector. */
typedef struct Vec2 {
    float x;
    float y;
} Vec2;

/**
 * Shared coordinate transformer used by input and rendering paths.
 *
 * Spaces:
 *  - World: authored layout or simulation units before UI scaling.
 *  - Logical UI: after applying UI scale; used for layout and hit-tests.
 *  - Screen: device pixels after DPI scaling; fed to GPU.
 */
typedef struct CoordinateTransformer {
    float dpi_scale;
    float ui_scale;
    Vec2 viewport_size;
} CoordinateTransformer;

typedef struct Color {
    float r;
    float g;
    float b;
    float a;
} Color;

/**
 * Projection and viewport information required during rendering.
 * Kept in a struct so callers can create multiple contexts without
 * relying on globals.
 */
typedef struct RenderContext {
    float projection[16];
    CoordinateTransformer transformer;
} RenderContext;

/** Logical layout rect in UI units. */
typedef struct LayoutBox {
    Vec2 origin;
    Vec2 size;
} LayoutBox;

/** Logical layout along with device-space result. */
typedef struct LayoutResult {
    LayoutBox logical;
    LayoutBox device;
} LayoutResult;

typedef struct GlyphQuad {
    Vec2 min;
    Vec2 max;
    Vec2 uv0;
    Vec2 uv1;
    Color color;
    int z_index;
} GlyphQuad;

/**
 * Representation of an immutable view model that the renderer consumes.
 * Game logic should translate its state into these view models before
 * invoking the renderer, keeping rendering free from mutation side-effects.
 */
typedef struct ViewModel {
    const char *id;
    LayoutBox logical_box;
    int z_index;
    Color color;
} ViewModel;

typedef enum RenderPrimitive {
    RENDER_PRIMITIVE_BACKGROUND = 0,
    RENDER_PRIMITIVE_GLYPH = 1,
} RenderPrimitive;

typedef struct RenderSortKey {
    int z_index;
    int primitive;
    size_t ordinal;
} RenderSortKey;

typedef struct RenderCommand {
    RenderPrimitive primitive;
    RenderSortKey key;
    union {
        struct {
            LayoutResult layout;
            Color color;
        } background;
        GlyphQuad glyph;
    } data;
} RenderCommand;

typedef struct RenderCommandList {
    RenderCommand *commands;
    size_t count;
    size_t capacity;
} RenderCommandList;

/** Renderer that owns composition for a frame. */
typedef struct Renderer {
    RenderContext context;
    RenderCommandList command_list;
} Renderer;

typedef struct UiVertex {
    float position[3];
    Color color;
} UiVertex;

typedef struct UiVertexBuffer {
    UiVertex *vertices;
    size_t count;
    size_t capacity;
} UiVertexBuffer;

typedef struct UiTextVertex {
    float position[3];
    float uv[2];
    Color color;
} UiTextVertex;

typedef struct UiTextVertexBuffer {
    UiTextVertex *vertices;
    size_t count;
    size_t capacity;
} UiTextVertexBuffer;

void coordinate_transformer_init(CoordinateTransformer *xfm, float dpi_scale, float ui_scale, Vec2 viewport_size);
Vec2 coordinate_screen_to_logical(const CoordinateTransformer *xfm, Vec2 screen);
Vec2 coordinate_logical_to_screen(const CoordinateTransformer *xfm, Vec2 logical);
Vec2 coordinate_world_to_logical(const CoordinateTransformer *xfm, Vec2 world);
Vec2 coordinate_logical_to_world(const CoordinateTransformer *xfm, Vec2 logical);
Vec2 coordinate_world_to_screen(const CoordinateTransformer *xfm, Vec2 world);
Vec2 coordinate_screen_to_world(const CoordinateTransformer *xfm, Vec2 screen);

void render_context_init(RenderContext *ctx, const CoordinateTransformer *xfm, const float projection[16]);

LayoutResult layout_resolve(const LayoutBox *logical, const RenderContext *ctx);
int layout_hit_test(const LayoutResult *layout, Vec2 logical_point);

void render_command_list_init(RenderCommandList *list, size_t initial_capacity);
void render_command_list_dispose(RenderCommandList *list);
void render_command_list_add(RenderCommandList *list, const RenderCommand *command);
void render_command_list_sort(RenderCommandList *list);

void renderer_init(Renderer *renderer, const RenderContext *context, size_t initial_capacity);
void renderer_dispose(Renderer *renderer);
void renderer_build_commands(Renderer *renderer, const ViewModel *view_models, size_t view_model_count, const GlyphQuad *glyphs, size_t glyph_count);
void renderer_fill_vertices(Renderer *renderer, const ViewModel *view_models, size_t view_model_count, const GlyphQuad *glyphs, size_t glyph_count, UiVertexBuffer *background_buffer, UiTextVertexBuffer *text_buffer);
void renderer_fill_background_vertices(Renderer *renderer, const ViewModel *view_models, size_t view_model_count, UiVertexBuffer *vertex_buffer);
void renderer_fill_text_vertices(Renderer *renderer, const GlyphQuad *glyphs, size_t glyph_count, UiTextVertexBuffer *vertex_buffer);

void ui_vertex_buffer_init(UiVertexBuffer *buffer, size_t initial_capacity);
void ui_vertex_buffer_dispose(UiVertexBuffer *buffer);
void ui_vertex_buffer_reserve(UiVertexBuffer *buffer, size_t vertex_capacity);

void ui_text_vertex_buffer_init(UiTextVertexBuffer *buffer, size_t initial_capacity);
void ui_text_vertex_buffer_dispose(UiTextVertexBuffer *buffer);
void ui_text_vertex_buffer_reserve(UiTextVertexBuffer *buffer, size_t vertex_capacity);

#ifdef __cplusplus
}
#endif

