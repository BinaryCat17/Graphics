#include "Graphics.h"

#include <stdlib.h>
#include <string.h>

static void ensure_capacity(DrawList *list, size_t required)
{
    if (required <= list->capacity) {
        return;
    }

    size_t new_capacity = list->capacity == 0 ? 4 : list->capacity * 2;
    while (new_capacity < required) {
        new_capacity *= 2;
    }

    DrawCommand *expanded = (DrawCommand *)realloc(list->commands, new_capacity * sizeof(DrawCommand));
    if (!expanded) {
        return;
    }

    list->commands = expanded;
    list->capacity = new_capacity;
}

void coordinate_transformer_init(CoordinateTransformer *xfm, float dpi_scale, float ui_scale, Vec2 viewport_size)
{
    xfm->dpi_scale = dpi_scale;
    xfm->ui_scale = ui_scale;
    xfm->viewport_size = viewport_size;
}

Vec2 coordinate_screen_to_logical(const CoordinateTransformer *xfm, Vec2 screen)
{
    Vec2 logical = {screen.x / xfm->dpi_scale, screen.y / xfm->dpi_scale};
    return logical;
}

Vec2 coordinate_logical_to_screen(const CoordinateTransformer *xfm, Vec2 logical)
{
    Vec2 screen = {logical.x * xfm->dpi_scale, logical.y * xfm->dpi_scale};
    return screen;
}

Vec2 coordinate_world_to_logical(const CoordinateTransformer *xfm, Vec2 world)
{
    Vec2 logical = {world.x * xfm->ui_scale, world.y * xfm->ui_scale};
    return logical;
}

Vec2 coordinate_logical_to_world(const CoordinateTransformer *xfm, Vec2 logical)
{
    float inv = xfm->ui_scale != 0.0f ? 1.0f / xfm->ui_scale : 1.0f;
    Vec2 world = {logical.x * inv, logical.y * inv};
    return world;
}

Vec2 coordinate_world_to_screen(const CoordinateTransformer *xfm, Vec2 world)
{
    return coordinate_logical_to_screen(xfm, coordinate_world_to_logical(xfm, world));
}

Vec2 coordinate_screen_to_world(const CoordinateTransformer *xfm, Vec2 screen)
{
    return coordinate_logical_to_world(xfm, coordinate_screen_to_logical(xfm, screen));
}

void render_context_init(RenderContext *ctx, const CoordinateTransformer *xfm, const float projection[16])
{
    ctx->transformer = *xfm;
    if (projection) {
        memcpy(ctx->projection, projection, sizeof(float) * 16);
    } else {
        memset(ctx->projection, 0, sizeof(float) * 16);
        ctx->projection[0] = 1.0f;
        ctx->projection[5] = 1.0f;
        ctx->projection[10] = 1.0f;
        ctx->projection[15] = 1.0f;
    }
}

LayoutResult layout_resolve(const LayoutBox *logical, const RenderContext *ctx)
{
    LayoutResult result;
    result.logical = *logical;

    result.device.origin = coordinate_logical_to_screen(&ctx->transformer, logical->origin);
    result.device.size = coordinate_logical_to_screen(&ctx->transformer, logical->size);

    return result;
}

int layout_hit_test(const LayoutResult *layout, Vec2 logical_point)
{
    float minx = layout->logical.origin.x;
    float miny = layout->logical.origin.y;
    float maxx = minx + layout->logical.size.x;
    float maxy = miny + layout->logical.size.y;
    return logical_point.x >= minx && logical_point.x <= maxx &&
           logical_point.y >= miny && logical_point.y <= maxy;
}

void draw_list_init(DrawList *list, size_t initial_capacity)
{
    list->count = 0;
    list->capacity = 0;
    list->commands = NULL;
    ensure_capacity(list, initial_capacity);
}

void draw_list_dispose(DrawList *list)
{
    free(list->commands);
    list->commands = NULL;
    list->count = 0;
    list->capacity = 0;
}

void draw_list_add(DrawList *list, const DrawCommand *command)
{
    ensure_capacity(list, list->count + 1);
    if (list->capacity < list->count + 1) {
        return;
    }

    list->commands[list->count++] = *command;
}

static int compare_draw_commands(const void *lhs, const void *rhs)
{
    const DrawCommand *a = (const DrawCommand *)lhs;
    const DrawCommand *b = (const DrawCommand *)rhs;
    return (a->z_index > b->z_index) - (a->z_index < b->z_index);
}

void draw_list_sort(DrawList *list)
{
    if (list->count <= 1) {
        return;
    }

    qsort(list->commands, list->count, sizeof(DrawCommand), compare_draw_commands);
}

void renderer_init(Renderer *renderer, const RenderContext *context, size_t initial_capacity)
{
    renderer->context = *context;
    draw_list_init(&renderer->draw_list, initial_capacity);
}

void renderer_dispose(Renderer *renderer)
{
    draw_list_dispose(&renderer->draw_list);
}

void renderer_build_draw_list(Renderer *renderer, const ViewModel *view_models, size_t view_model_count)
{
    renderer->draw_list.count = 0;
    for (size_t i = 0; i < view_model_count; ++i) {
        LayoutResult layout = layout_resolve(&view_models[i].logical_box, &renderer->context);
        DrawCommand command = {0};
        command.layout = layout;
        command.z_index = view_models[i].z_index;
        command.color = view_models[i].color;
        draw_list_add(&renderer->draw_list, &command);
    }

    draw_list_sort(&renderer->draw_list);
}

void ui_vertex_buffer_init(UiVertexBuffer *buffer, size_t initial_capacity)
{
    buffer->vertices = NULL;
    buffer->count = 0;
    buffer->capacity = 0;
    ui_vertex_buffer_reserve(buffer, initial_capacity);
}

void ui_vertex_buffer_dispose(UiVertexBuffer *buffer)
{
    free(buffer->vertices);
    buffer->vertices = NULL;
    buffer->count = 0;
    buffer->capacity = 0;
}

void ui_vertex_buffer_reserve(UiVertexBuffer *buffer, size_t vertex_capacity)
{
    if (vertex_capacity <= buffer->capacity) {
        return;
    }

    size_t new_capacity = buffer->capacity == 0 ? 6 : buffer->capacity * 2;
    while (new_capacity < vertex_capacity) {
        new_capacity *= 2;
    }

    UiVertex *expanded = (UiVertex *)realloc(buffer->vertices, new_capacity * sizeof(UiVertex));
    if (!expanded) {
        return;
    }

    buffer->vertices = expanded;
    buffer->capacity = new_capacity;
}

static void project_point(const RenderContext *ctx, Vec2 point, float z, float out_position[3])
{
    const float v[4] = {point.x, point.y, z, 1.0f};
    float result[4] = {0};

    for (int row = 0; row < 4; ++row) {
        result[row] = ctx->projection[row * 4 + 0] * v[0] + ctx->projection[row * 4 + 1] * v[1] +
                      ctx->projection[row * 4 + 2] * v[2] + ctx->projection[row * 4 + 3] * v[3];
    }

    out_position[0] = result[0];
    out_position[1] = result[1];
    out_position[2] = result[2];
}

static void emit_quad_vertices(const RenderContext *ctx, const DrawCommand *command, UiVertexBuffer *vertex_buffer)
{
    Vec2 min = command->layout.device.origin;
    Vec2 max = {min.x + command->layout.device.size.x, min.y + command->layout.device.size.y};
    float z = (float)command->z_index;

    ui_vertex_buffer_reserve(vertex_buffer, vertex_buffer->count + 6);
    if (vertex_buffer->capacity < vertex_buffer->count + 6) {
        return;
    }

    UiVertex quad[6];
    Vec2 corners[4] = {{min.x, min.y}, {max.x, min.y}, {max.x, max.y}, {min.x, max.y}};
    int indices[6] = {0, 1, 2, 0, 2, 3};

    for (int i = 0; i < 6; ++i) {
        UiVertex v = {0};
        project_point(ctx, corners[indices[i]], z, v.position);
        v.color = command->color;
        quad[i] = v;
    }

    memcpy(&vertex_buffer->vertices[vertex_buffer->count], quad, sizeof(UiVertex) * 6);
    vertex_buffer->count += 6;
}

void renderer_fill_background_vertices(Renderer *renderer, const ViewModel *view_models, size_t view_model_count, UiVertexBuffer *vertex_buffer)
{
    renderer_build_draw_list(renderer, view_models, view_model_count);
    vertex_buffer->count = 0;

    for (size_t i = 0; i < renderer->draw_list.count; ++i) {
        emit_quad_vertices(&renderer->context, &renderer->draw_list.commands[i], vertex_buffer);
    }
}

void ui_text_vertex_buffer_init(UiTextVertexBuffer *buffer, size_t initial_capacity)
{
    buffer->vertices = NULL;
    buffer->count = 0;
    buffer->capacity = 0;
    ui_text_vertex_buffer_reserve(buffer, initial_capacity);
}

void ui_text_vertex_buffer_dispose(UiTextVertexBuffer *buffer)
{
    free(buffer->vertices);
    buffer->vertices = NULL;
    buffer->count = 0;
    buffer->capacity = 0;
}

void ui_text_vertex_buffer_reserve(UiTextVertexBuffer *buffer, size_t vertex_capacity)
{
    if (vertex_capacity <= buffer->capacity) {
        return;
    }

    size_t new_capacity = buffer->capacity == 0 ? 6 : buffer->capacity * 2;
    while (new_capacity < vertex_capacity) {
        new_capacity *= 2;
    }

    UiTextVertex *expanded = (UiTextVertex *)realloc(buffer->vertices, new_capacity * sizeof(UiTextVertex));
    if (!expanded) {
        return;
    }

    buffer->vertices = expanded;
    buffer->capacity = new_capacity;
}

static void emit_text_vertices(const RenderContext *ctx, const GlyphQuad *glyph, UiTextVertexBuffer *vertex_buffer)
{
    Vec2 device_min = coordinate_logical_to_screen(&ctx->transformer, glyph->min);
    Vec2 device_max = coordinate_logical_to_screen(&ctx->transformer, glyph->max);
    float z = (float)glyph->z_index;

    ui_text_vertex_buffer_reserve(vertex_buffer, vertex_buffer->count + 6);
    if (vertex_buffer->capacity < vertex_buffer->count + 6) {
        return;
    }

    UiTextVertex quad[6];
    Vec2 corners[4] = {{device_min.x, device_min.y}, {device_max.x, device_min.y}, {device_max.x, device_max.y}, {device_min.x, device_max.y}};
    Vec2 uvs[4] = {{glyph->uv0.x, glyph->uv0.y}, {glyph->uv1.x, glyph->uv0.y}, {glyph->uv1.x, glyph->uv1.y}, {glyph->uv0.x, glyph->uv1.y}};
    int indices[6] = {0, 1, 2, 0, 2, 3};

    for (int i = 0; i < 6; ++i) {
        UiTextVertex v = {0};
        project_point(ctx, corners[indices[i]], z, v.position);
        v.uv[0] = uvs[indices[i]].x;
        v.uv[1] = uvs[indices[i]].y;
        v.color = glyph->color;
        quad[i] = v;
    }

    memcpy(&vertex_buffer->vertices[vertex_buffer->count], quad, sizeof(UiTextVertex) * 6);
    vertex_buffer->count += 6;
}

void renderer_fill_text_vertices(const RenderContext *context, const GlyphQuad *glyphs, size_t glyph_count, UiTextVertexBuffer *vertex_buffer)
{
    vertex_buffer->count = 0;
    for (size_t i = 0; i < glyph_count; ++i) {
        emit_text_vertices(context, &glyphs[i], vertex_buffer);
    }
}

