#ifndef COORDINATE_SYSTEMS_H
#define COORDINATE_SYSTEMS_H

#include "math_types.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum CoordinateSpace {
    COORDINATE_SPACE_LOCAL = 0,
    COORDINATE_SPACE_WORLD = 1,
    COORDINATE_SPACE_LOGICAL = 2,
    COORDINATE_SPACE_SCREEN = 3,
} CoordinateSpace;

typedef struct Transform2D {
    Vec2 translation;
    float rotation_radians;
    Vec2 scale;
} Transform2D;

typedef struct Transform3D {
    Vec3 translation;
    Vec3 scale;
    Quat rotation;
} Transform3D;

typedef struct CoordinateSystem2D {
    float dpi_scale;
    float ui_scale;
    Vec2 viewport_size;
    Mat4 world_to_logical;
    Mat4 logical_to_world;
    Mat4 logical_to_screen;
    Mat4 screen_to_logical;
    Mat4 world_to_screen;
    Mat4 screen_to_world;
} CoordinateSystem2D;

typedef struct RenderContext {
    Mat4 projection;
    CoordinateSystem2D coordinates;
} RenderContext;

typedef struct Projection3D {
    Mat4 view;
    Mat4 projection;
    Mat4 view_projection;
    Mat4 inverse_view;
    Mat4 inverse_projection;
} Projection3D;

void transform2d_build_matrices(const Transform2D *transform, Mat4 *local_to_world, Mat4 *world_to_local);
void transform3d_build_matrices(const Transform3D *transform, Mat4 *local_to_world, Mat4 *world_to_local);

void coordinate_system2d_init(CoordinateSystem2D *system, float dpi_scale, float ui_scale, Vec2 viewport_size);
Vec2 coordinate_space_convert_2d(const CoordinateSystem2D *system, CoordinateSpace from, CoordinateSpace to, Vec2 value);
Vec2 coordinate_screen_to_logical(const CoordinateSystem2D *system, Vec2 screen);
Vec2 coordinate_logical_to_screen(const CoordinateSystem2D *system, Vec2 logical);
Vec2 coordinate_world_to_logical(const CoordinateSystem2D *system, Vec2 world);
Vec2 coordinate_logical_to_world(const CoordinateSystem2D *system, Vec2 logical);
Vec2 coordinate_world_to_screen(const CoordinateSystem2D *system, Vec2 world);
Vec2 coordinate_screen_to_world(const CoordinateSystem2D *system, Vec2 screen);
void render_context_init(RenderContext *ctx, const CoordinateSystem2D *coordinates, const Mat4 *projection);

Vec2 coordinate_local_to_world_2d(const Transform2D *local, Vec2 p);
Vec2 coordinate_world_to_local_2d(const Transform2D *local, Vec2 p);
Vec3 coordinate_local_to_world_3d(const Transform3D *local, Vec3 p);
Vec3 coordinate_world_to_local_3d(const Transform3D *local, Vec3 p);

void projection3d_init(Projection3D *projection, const Mat4 *view, const Mat4 *projection_matrix);
Vec3 coordinate_world_to_clip(const Projection3D *projection, Vec3 world);
Vec3 coordinate_clip_to_world(const Projection3D *projection, Vec3 clip);

#ifdef __cplusplus
}
#endif

#endif // COORDINATE_SYSTEMS_H
