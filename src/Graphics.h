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
    float dpi_scale;
    Vec2 viewport_size;
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

/** Drawing command produced after layout resolution. */
typedef struct DrawCommand {
    LayoutResult layout;
    int z_index;
    Color color;
} DrawCommand;

/**
 * Draw list with z-ordered commands.
 */
typedef struct DrawList {
    DrawCommand *commands;
    size_t count;
    size_t capacity;
} DrawList;

/** Renderer that owns composition for a frame. */
typedef struct Renderer {
    RenderContext context;
    DrawList draw_list;
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

void render_context_init(RenderContext *ctx, float dpi_scale, Vec2 viewport_size, const float projection[16]);

LayoutResult layout_resolve(const LayoutBox *logical, const RenderContext *ctx);

void draw_list_init(DrawList *list, size_t initial_capacity);
void draw_list_dispose(DrawList *list);
void draw_list_add(DrawList *list, const DrawCommand *command);
void draw_list_sort(DrawList *list);

void renderer_init(Renderer *renderer, const RenderContext *context, size_t initial_capacity);
void renderer_dispose(Renderer *renderer);
void renderer_build_draw_list(Renderer *renderer, const ViewModel *view_models, size_t view_model_count);
void renderer_fill_ui_vertices(Renderer *renderer, const ViewModel *view_models, size_t view_model_count, UiVertexBuffer *vertex_buffer);

void ui_vertex_buffer_init(UiVertexBuffer *buffer, size_t initial_capacity);
void ui_vertex_buffer_dispose(UiVertexBuffer *buffer);
void ui_vertex_buffer_reserve(UiVertexBuffer *buffer, size_t vertex_capacity);

#ifdef __cplusplus
}
#endif

