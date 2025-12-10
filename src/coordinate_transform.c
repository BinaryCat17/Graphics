#include "coordinate_transform.h"

#include <math.h>
#include <string.h>

void coordinate_transformer_init(CoordinateTransformer *xfm, float dpi_scale, float ui_scale, Vec2 viewport_size)
{
    if (!xfm) {
        return;
    }
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
    if (!ctx || !xfm) {
        return;
    }

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

