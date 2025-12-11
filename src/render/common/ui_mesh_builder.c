#include "ui_mesh_builder.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "memory/buffer.h"

MEM_BUFFER_DECLARE(UiVertexBuffer, 6)
MEM_BUFFER_DECLARE(UiTextVertexBuffer, 6)

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
    Vec3 projected = mat4_transform_point(&ctx->projection, (Vec3){point.x, point.y, z});
    out_position[0] = projected.x;
    out_position[1] = projected.y;
    out_position[2] = projected.z;
}

static int emit_quad_vertices(const RenderContext *ctx, const RenderCommand *command, UiVertexBuffer *vertex_buffer)
{
    Vec2 min = command->data.background.layout.device.origin;
    Vec2 max = {min.x + command->data.background.layout.device.size.x, min.y + command->data.background.layout.device.size.y};
    if (command->has_clip && !apply_device_clip(&command->clip, &min, &max)) {
        return 0;
    }
    float z = (float)command->key.layer;

    if (ui_vertex_buffer_reserve(vertex_buffer, vertex_buffer->count + 6) != 0) {
        return -1;
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
    return 0;
}

static int emit_text_vertices(const RenderContext *ctx, const GlyphQuad *glyph, UiTextVertexBuffer *vertex_buffer)
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
            return 0;
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

    Vec2 device_min = coordinate_logical_to_screen(&ctx->coordinates, logical_min);
    Vec2 device_max = coordinate_logical_to_screen(&ctx->coordinates, logical_max);

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

    if (ui_text_vertex_buffer_reserve(vertex_buffer, vertex_buffer->count + 6) != 0) {
        return -1;
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
    return 0;
}

int ui_vertex_buffer_init(UiVertexBuffer *buffer, size_t initial_capacity)
{
    return UiVertexBuffer_mem_init(buffer, initial_capacity);
}

void ui_vertex_buffer_dispose(UiVertexBuffer *buffer)
{
    UiVertexBuffer_mem_dispose(buffer);
}

int ui_vertex_buffer_reserve(UiVertexBuffer *buffer, size_t vertex_capacity)
{
    return UiVertexBuffer_mem_reserve(buffer, vertex_capacity);
}

int ui_text_vertex_buffer_init(UiTextVertexBuffer *buffer, size_t initial_capacity)
{
    return UiTextVertexBuffer_mem_init(buffer, initial_capacity);
}

void ui_text_vertex_buffer_dispose(UiTextVertexBuffer *buffer)
{
    UiTextVertexBuffer_mem_dispose(buffer);
}

int ui_text_vertex_buffer_reserve(UiTextVertexBuffer *buffer, size_t vertex_capacity)
{
    return UiTextVertexBuffer_mem_reserve(buffer, vertex_capacity);
}

int renderer_fill_vertices(Renderer *renderer, const ViewModel *view_models, size_t view_model_count, const GlyphQuad *glyphs,
                           size_t glyph_count, UiVertexBuffer *background_buffer, UiTextVertexBuffer *text_buffer)
{
    if (!renderer) {
        return -1;
    }

    if (renderer_build_commands(renderer, view_models, view_model_count, glyphs, glyph_count) != 0) {
        return -1;
    }

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
                if (background_buffer && emit_quad_vertices(&renderer->context, command, background_buffer) != 0) {
                    return -1;
                }
                break;
            case RENDER_PRIMITIVE_GLYPH:
                if (text_buffer && emit_text_vertices(&renderer->context, &command->data.glyph, text_buffer) != 0) {
                    return -1;
                }
                break;
            default:
                break;
        }
    }

    return 0;
}

int renderer_fill_background_vertices(Renderer *renderer, const ViewModel *view_models, size_t view_model_count,
                                      UiVertexBuffer *vertex_buffer)
{
    return renderer_fill_vertices(renderer, view_models, view_model_count, NULL, 0, vertex_buffer, NULL);
}

int renderer_fill_text_vertices(Renderer *renderer, const GlyphQuad *glyphs, size_t glyph_count,
                                UiTextVertexBuffer *vertex_buffer)
{
    return renderer_fill_vertices(renderer, NULL, 0, glyphs, glyph_count, NULL, vertex_buffer);
}

