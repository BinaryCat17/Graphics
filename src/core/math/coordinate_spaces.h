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

/**
 * Projection and viewport information required during rendering.
 * Kept in a struct so callers can create multiple contexts without
 * relying on globals.
 */
typedef struct RenderContext {
    float projection[16];
    CoordinateTransformer transformer;
} RenderContext;

void coordinate_transformer_init(CoordinateTransformer *xfm, float dpi_scale, float ui_scale, Vec2 viewport_size);
Vec2 coordinate_screen_to_logical(const CoordinateTransformer *xfm, Vec2 screen);
Vec2 coordinate_logical_to_screen(const CoordinateTransformer *xfm, Vec2 logical);
Vec2 coordinate_world_to_logical(const CoordinateTransformer *xfm, Vec2 world);
Vec2 coordinate_logical_to_world(const CoordinateTransformer *xfm, Vec2 logical);
Vec2 coordinate_world_to_screen(const CoordinateTransformer *xfm, Vec2 world);
Vec2 coordinate_screen_to_world(const CoordinateTransformer *xfm, Vec2 screen);

void render_context_init(RenderContext *ctx, const CoordinateTransformer *xfm, const float projection[16]);

#ifdef __cplusplus
}
#endif

