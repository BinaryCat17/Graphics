#include "layout.h"

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

