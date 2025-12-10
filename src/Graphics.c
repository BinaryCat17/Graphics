#include "Graphics.h"

#include <stdlib.h>
#include <math.h>

static void ensure_capacity(RenderCommandList *list, size_t required)
{
    if (required <= list->capacity) {
        return;
    }

    size_t new_capacity = list->capacity == 0 ? 4 : list->capacity * 2;
    while (new_capacity < required) {
        new_capacity *= 2;
    }

    RenderCommand *expanded = (RenderCommand *)realloc(list->commands, new_capacity * sizeof(RenderCommand));
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
    Vec2 logical = {
        screen.x / xfm->dpi_scale,
        screen.y / xfm->dpi_scale
    };
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

void render_command_list_init(RenderCommandList *list, size_t initial_capacity)
{
    list->count = 0;
    list->capacity = 0;
    list->commands = NULL;
    ensure_capacity(list, initial_capacity);
}

void render_command_list_dispose(RenderCommandList *list)
{
    free(list->commands);
    list->commands = NULL;
    list->count = 0;
    list->capacity = 0;
}

void render_command_list_add(RenderCommandList *list, const RenderCommand *command)
{
    ensure_capacity(list, list->count + 1);
    if (list->capacity < list->count + 1) {
        return;
    }

    list->commands[list->count++] = *command;
}

static int compare_sort_keys(const RenderSortKey *a, const RenderSortKey *b)
{
    if (a->layer != b->layer) {
        return (a->layer > b->layer) - (a->layer < b->layer);
    }

    if (a->widget_order != b->widget_order) {
        return (a->widget_order > b->widget_order) - (a->widget_order < b->widget_order);
    }

    if (a->phase != b->phase) {
        return (a->phase > b->phase) - (a->phase < b->phase);
    }

    if (a->ordinal != b->ordinal) {
        return (a->ordinal > b->ordinal) - (a->ordinal < b->ordinal);
    }

    return 0;
}

static void merge(RenderCommand *commands, RenderCommand *scratch, size_t left, size_t mid, size_t right)
{
    size_t i = left;
    size_t j = mid;
    size_t k = left;

    while (i < mid && j < right) {
        if (compare_sort_keys(&commands[i].key, &commands[j].key) <= 0) {
            scratch[k++] = commands[i++];
        } else {
            scratch[k++] = commands[j++];
        }
    }

    while (i < mid) {
        scratch[k++] = commands[i++];
    }

    while (j < right) {
        scratch[k++] = commands[j++];
    }

    for (size_t idx = left; idx < right; ++idx) {
        commands[idx] = scratch[idx];
    }
}

static void stable_sort(RenderCommand *commands, RenderCommand *scratch, size_t left, size_t right)
{
    if (right - left <= 1) {
        return;
    }

    size_t mid = left + (right - left) / 2;
    stable_sort(commands, scratch, left, mid);
    stable_sort(commands, scratch, mid, right);
    merge(commands, scratch, left, mid, right);
}

void render_command_list_sort(RenderCommandList *list)
{
    if (list->count <= 1) {
        return;
    }

    RenderCommand *scratch = (RenderCommand *)malloc(list->count * sizeof(RenderCommand));
    if (!scratch) {
        return;
    }

    stable_sort(list->commands, scratch, 0, list->count);
    free(scratch);
}

void renderer_init(Renderer *renderer, const RenderContext *context, size_t initial_capacity)
{
    renderer->context = *context;
    render_command_list_init(&renderer->command_list, initial_capacity);
}

void renderer_dispose(Renderer *renderer)
{
    render_command_list_dispose(&renderer->command_list);
}

void renderer_build_commands(Renderer *renderer, const ViewModel *view_models, size_t view_model_count, const GlyphQuad *glyphs, size_t glyph_count)
{
    renderer->command_list.count = 0;
    for (size_t i = 0; i < view_model_count; ++i) {
        LayoutResult layout = layout_resolve(&view_models[i].logical_box, &renderer->context);
        RenderCommand command = (RenderCommand){0};
        command.primitive = RENDER_PRIMITIVE_BACKGROUND;
        command.phase = view_models[i].phase;
        command.key = (RenderSortKey){view_models[i].layer, view_models[i].widget_order, view_models[i].phase, view_models[i].ordinal};
        command.has_clip = view_models[i].has_clip;
        if (view_models[i].has_clip) {
            command.clip = layout_resolve(&view_models[i].clip, &renderer->context);
        }
        command.data.background.layout = layout;
        command.data.background.color = view_models[i].color;
        render_command_list_add(&renderer->command_list, &command);
    }

    for (size_t i = 0; i < glyph_count; ++i) {
        RenderCommand command = (RenderCommand){0};
        command.primitive = RENDER_PRIMITIVE_GLYPH;
        command.phase = glyphs[i].phase;
        command.key = (RenderSortKey){glyphs[i].layer, glyphs[i].widget_order, glyphs[i].phase, glyphs[i].ordinal};
        command.has_clip = glyphs[i].has_clip;
        if (glyphs[i].has_clip) {
            command.clip = layout_resolve(&glyphs[i].clip, &renderer->context);
        }
        command.data.glyph = glyphs[i];
        render_command_list_add(&renderer->command_list, &command);
    }

    render_command_list_sort(&renderer->command_list);
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

static void emit_text_vertices(const RenderContext *ctx, const GlyphQuad *glyph, UiTextVertexBuffer *vertex_buffer);

static int apply_device_clip(const LayoutResult *clip, Vec2 *min, Vec2 *max)
{
    if (!clip || !min || !max) {
        return 1;
    }
    float cx0 = clip->device.origin.x;
    float cy0 = clip->device.origin.y;
    float cx1 = cx0 + clip->device.size.x;
    float cy1 = cy0 + clip->device.size.y;

    float x0 = fmaxf(min->x, cx0);
    float y0 = fmaxf(min->y, cy0);
    float x1 = fminf(max->x, cx1);
    float y1 = fminf(max->y, cy1);

    if (x1 <= x0 || y1 <= y0) {
        return 0;
    }

    min->x = x0;
    min->y = y0;
    max->x = x1;
    max->y = y1;
    return 1;
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

static void emit_quad_vertices(const RenderContext *ctx, const RenderCommand *command, UiVertexBuffer *vertex_buffer)
{
    Vec2 min = command->data.background.layout.device.origin;
    Vec2 max = {min.x + command->data.background.layout.device.size.x, min.y + command->data.background.layout.device.size.y};
    if (command->has_clip && !apply_device_clip(&command->clip, &min, &max)) {
        return;
    }
    float z = (float)command->key.layer;

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
        v.color = command->data.background.color;
        quad[i] = v;
    }

    memcpy(&vertex_buffer->vertices[vertex_buffer->count], quad, sizeof(UiVertex) * 6);
    vertex_buffer->count += 6;
}

void renderer_fill_vertices(Renderer *renderer, const ViewModel *view_models, size_t view_model_count, const GlyphQuad *glyphs, size_t glyph_count, UiVertexBuffer *background_buffer, UiTextVertexBuffer *text_buffer)
{
    renderer_build_commands(renderer, view_models, view_model_count, glyphs, glyph_count);

    if (background_buffer) {
        background_buffer->count = 0;
    }
    if (text_buffer) {
        text_buffer->count = 0;
    }

    for (size_t i = 0; i < renderer->command_list.count; ++i) {
        const RenderCommand *command = &renderer->command_list.commands[i];
        switch (command->primitive) {
            case RENDER_PRIMITIVE_BACKGROUND:
                if (background_buffer) {
                    emit_quad_vertices(&renderer->context, command, background_buffer);
                }
                break;
            case RENDER_PRIMITIVE_GLYPH:
                if (text_buffer) {
                    emit_text_vertices(&renderer->context, &command->data.glyph, text_buffer);
                }
                break;
            default:
                break;
        }
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
    Vec2 logical_min = glyph->min;
    Vec2 logical_max = glyph->max;
    float u0 = glyph->uv0.x;
    float v0 = glyph->uv0.y;
    float u1 = glyph->uv1.x;
    float v1 = glyph->uv1.y;

    if (glyph->has_clip) {
        float cx0 = glyph->clip.origin.x;
        float cy0 = glyph->clip.origin.y;
        float cx1 = cx0 + glyph->clip.size.x;
        float cy1 = cy0 + glyph->clip.size.y;
        float x0 = fmaxf(logical_min.x, cx0);
        float y0 = fmaxf(logical_min.y, cy0);
        float x1 = fminf(logical_max.x, cx1);
        float y1 = fminf(logical_max.y, cy1);
        if (x1 <= x0 || y1 <= y0) {
            return;
        }
        float span_x = logical_max.x - logical_min.x;
        float span_y = logical_max.y - logical_min.y;
        if (span_x != 0.0f) {
            float du = (u1 - u0) / span_x;
            u0 += du * (x0 - logical_min.x);
            u1 -= du * (logical_max.x - x1);
        }
        if (span_y != 0.0f) {
            float dv = (v1 - v0) / span_y;
            v0 += dv * (y0 - logical_min.y);
            v1 -= dv * (logical_max.y - y1);
        }
        logical_min.x = x0;
        logical_min.y = y0;
        logical_max.x = x1;
        logical_max.y = y1;
    }

    Vec2 device_min = coordinate_logical_to_screen(&ctx->transformer, logical_min);
    Vec2 device_max = coordinate_logical_to_screen(&ctx->transformer, logical_max);

    Vec2 snapped_min = {roundf(device_min.x), roundf(device_min.y)};
    Vec2 snapped_max = {roundf(device_max.x), roundf(device_max.y)};

    float device_w = device_max.x - device_min.x;
    float device_h = device_max.y - device_min.y;

    if (device_w != 0.0f) {
        float du = (u1 - u0) / device_w;
        float delta_min = snapped_min.x - device_min.x;
        float delta_max = snapped_max.x - device_max.x;
        u0 += du * delta_min;
        u1 += du * delta_max;
    }

    if (device_h != 0.0f) {
        float dv = (v1 - v0) / device_h;
        float delta_min = snapped_min.y - device_min.y;
        float delta_max = snapped_max.y - device_max.y;
        v0 += dv * delta_min;
        v1 += dv * delta_max;
    }

    device_min = snapped_min;
    device_max = snapped_max;
    float z = (float)glyph->layer;

    ui_text_vertex_buffer_reserve(vertex_buffer, vertex_buffer->count + 6);
    if (vertex_buffer->capacity < vertex_buffer->count + 6) {
        return;
    }

    UiTextVertex quad[6];
    Vec2 corners[4] = {{device_min.x, device_min.y}, {device_max.x, device_min.y}, {device_max.x, device_max.y}, {device_min.x, device_max.y}};
    Vec2 uvs[4] = {{u0, v0}, {u1, v0}, {u1, v1}, {u0, v1}};
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

void renderer_fill_background_vertices(Renderer *renderer, const ViewModel *view_models, size_t view_model_count, UiVertexBuffer *vertex_buffer)
{
    renderer_fill_vertices(renderer, view_models, view_model_count, NULL, 0, vertex_buffer, NULL);
}

void renderer_fill_text_vertices(Renderer *renderer, const GlyphQuad *glyphs, size_t glyph_count, UiTextVertexBuffer *vertex_buffer)
{
    renderer_fill_vertices(renderer, NULL, 0, glyphs, glyph_count, NULL, vertex_buffer);
}

