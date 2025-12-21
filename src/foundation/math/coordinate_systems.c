#include "coordinate_systems.h"

#include <math.h>
#include <string.h>

static Mat4 mat4_rotation_z(float radians)
{
    float c = cosf(radians);
    float s = sinf(radians);
    Mat4 m = mat4_identity();
    m.m[0] = c;
    m.m[1] = s;
    m.m[4] = -s;
    m.m[5] = c;
    return m;
}

void transform2d_build_matrices(const Transform2D *transform, Mat4 *local_to_world, Mat4 *world_to_local)
{
    Transform2D safe = *transform;
    if (safe.scale.x == 0.0f) safe.scale.x = 1.0f;
    if (safe.scale.y == 0.0f) safe.scale.y = 1.0f;

    Mat4 t = mat4_translation((Vec3){safe.translation.x, safe.translation.y, 0.0f});
    Mat4 r = mat4_rotation_z(safe.rotation_radians);
    Mat4 s = mat4_scale((Vec3){safe.scale.x, safe.scale.y, 1.0f});

    Mat4 rs = mat4_multiply(&r, &s);
    Mat4 trs = mat4_multiply(&t, &rs);
    if (local_to_world) *local_to_world = trs;

    Mat4 inv_s = mat4_scale((Vec3){1.0f / safe.scale.x, 1.0f / safe.scale.y, 1.0f});
    Mat4 inv_r = mat4_rotation_z(-safe.rotation_radians);
    Mat4 inv_t = mat4_translation((Vec3){-safe.translation.x, -safe.translation.y, 0.0f});
    Mat4 inv_rt = mat4_multiply(&inv_r, &inv_t);
    Mat4 inv = mat4_multiply(&inv_s, &inv_rt);
    if (world_to_local) *world_to_local = inv;
}

void transform3d_build_matrices(const Transform3D *transform, Mat4 *local_to_world, Mat4 *world_to_local)
{
    Transform3D safe = *transform;
    if (safe.scale.x == 0.0f) safe.scale.x = 1.0f;
    if (safe.scale.y == 0.0f) safe.scale.y = 1.0f;
    if (safe.scale.z == 0.0f) safe.scale.z = 1.0f;

    Mat4 t = mat4_translation(safe.translation);
    Mat4 r = mat4_rotation_quat(safe.rotation);
    Mat4 s = mat4_scale(safe.scale);

    Mat4 rs = mat4_multiply(&r, &s);
    Mat4 trs = mat4_multiply(&t, &rs);
    if (local_to_world) *local_to_world = trs;

    Mat4 inv_s = mat4_scale((Vec3){1.0f / safe.scale.x, 1.0f / safe.scale.y, 1.0f / safe.scale.z});
    Mat4 inv_r = mat4_rotation_quat(quat_conjugate(safe.rotation));
    Mat4 inv_t = mat4_translation((Vec3){-safe.translation.x, -safe.translation.y, -safe.translation.z});
    Mat4 inv_rt = mat4_multiply(&inv_r, &inv_t);
    Mat4 inv = mat4_multiply(&inv_s, &inv_rt);
    if (world_to_local) *world_to_local = inv;
}

void coordinate_system2d_init(CoordinateSystem2D *system, float dpi_scale, float ui_scale, Vec2 viewport_size)
{
    if (!system) return;
    system->dpi_scale = dpi_scale;
    system->ui_scale = ui_scale;
    system->viewport_size = viewport_size;

    Mat4 world_to_logical = mat4_scale((Vec3){ui_scale, ui_scale, 1.0f});
    Mat4 logical_to_world = mat4_scale((Vec3){ui_scale != 0.0f ? 1.0f / ui_scale : 1.0f, ui_scale != 0.0f ? 1.0f / ui_scale : 1.0f, 1.0f});
    Mat4 logical_to_screen = mat4_scale((Vec3){dpi_scale, dpi_scale, 1.0f});
    Mat4 screen_to_logical = mat4_scale((Vec3){dpi_scale != 0.0f ? 1.0f / dpi_scale : 1.0f, dpi_scale != 0.0f ? 1.0f / dpi_scale : 1.0f, 1.0f});

    system->world_to_logical = world_to_logical;
    system->logical_to_world = logical_to_world;
    system->logical_to_screen = logical_to_screen;
    system->screen_to_logical = screen_to_logical;
    system->world_to_screen = mat4_multiply(&logical_to_screen, &world_to_logical);
    system->screen_to_world = mat4_multiply(&logical_to_world, &screen_to_logical);
}

static Vec2 mat4_apply_to_vec2(const Mat4 *m, Vec2 p)
{
    Vec3 r = mat4_transform_point(m, (Vec3){p.x, p.y, 0.0f});
    return (Vec2){r.x, r.y};
}

Vec2 coordinate_space_convert_2d(const CoordinateSystem2D *system, CoordinateSpace from, CoordinateSpace to, Vec2 value)
{
    if (!system) return value;
    if (from == to) return value;

    const Mat4 *forward = NULL;

    if (from == COORDINATE_SPACE_WORLD && to == COORDINATE_SPACE_LOGICAL) forward = &system->world_to_logical;
    if (from == COORDINATE_SPACE_LOGICAL && to == COORDINATE_SPACE_WORLD) forward = &system->logical_to_world;
    if (from == COORDINATE_SPACE_LOGICAL && to == COORDINATE_SPACE_SCREEN) forward = &system->logical_to_screen;
    if (from == COORDINATE_SPACE_SCREEN && to == COORDINATE_SPACE_LOGICAL) forward = &system->screen_to_logical;
    if (from == COORDINATE_SPACE_WORLD && to == COORDINATE_SPACE_SCREEN) forward = &system->world_to_screen;
    if (from == COORDINATE_SPACE_SCREEN && to == COORDINATE_SPACE_WORLD) forward = &system->screen_to_world;

    if (forward) {
        return mat4_apply_to_vec2(forward, value);
    }
    return value;
}

Vec2 coordinate_screen_to_logical(const CoordinateSystem2D *system, Vec2 screen)
{
    return coordinate_space_convert_2d(system, COORDINATE_SPACE_SCREEN, COORDINATE_SPACE_LOGICAL, screen);
}

Vec2 coordinate_logical_to_screen(const CoordinateSystem2D *system, Vec2 logical)
{
    return coordinate_space_convert_2d(system, COORDINATE_SPACE_LOGICAL, COORDINATE_SPACE_SCREEN, logical);
}

Vec2 coordinate_world_to_logical(const CoordinateSystem2D *system, Vec2 world)
{
    return coordinate_space_convert_2d(system, COORDINATE_SPACE_WORLD, COORDINATE_SPACE_LOGICAL, world);
}

Vec2 coordinate_logical_to_world(const CoordinateSystem2D *system, Vec2 logical)
{
    return coordinate_space_convert_2d(system, COORDINATE_SPACE_LOGICAL, COORDINATE_SPACE_WORLD, logical);
}

Vec2 coordinate_world_to_screen(const CoordinateSystem2D *system, Vec2 world)
{
    return coordinate_space_convert_2d(system, COORDINATE_SPACE_WORLD, COORDINATE_SPACE_SCREEN, world);
}

Vec2 coordinate_screen_to_world(const CoordinateSystem2D *system, Vec2 screen)
{
    return coordinate_space_convert_2d(system, COORDINATE_SPACE_SCREEN, COORDINATE_SPACE_WORLD, screen);
}

Vec2 coordinate_local_to_world_2d(const Transform2D *local, Vec2 p)
{
    Mat4 l2w;
    transform2d_build_matrices(local, &l2w, NULL);
    return mat4_apply_to_vec2(&l2w, p);
}

Vec2 coordinate_world_to_local_2d(const Transform2D *local, Vec2 p)
{
    Mat4 w2l;
    transform2d_build_matrices(local, NULL, &w2l);
    return mat4_apply_to_vec2(&w2l, p);
}

Vec3 coordinate_local_to_world_3d(const Transform3D *local, Vec3 p)
{
    Mat4 l2w;
    transform3d_build_matrices(local, &l2w, NULL);
    return mat4_transform_point(&l2w, p);
}

Vec3 coordinate_world_to_local_3d(const Transform3D *local, Vec3 p)
{
    Mat4 w2l;
    transform3d_build_matrices(local, NULL, &w2l);
    return mat4_transform_point(&w2l, p);
}

void render_context_init(RenderContext *ctx, const CoordinateSystem2D *coordinates, const Mat4 *projection)
{
    if (!ctx || !coordinates) return;
    ctx->coordinates = *coordinates;
    if (projection) {
        ctx->projection = *projection;
    } else {
        ctx->projection = mat4_identity();
    }
}

void projection3d_init(Projection3D *projection, const Mat4 *view, const Mat4 *projection_matrix)
{
    if (!projection || !view || !projection_matrix) return;
    projection->view = *view;
    projection->projection = *projection_matrix;
    projection->view_projection = mat4_multiply(projection_matrix, view);
    projection->inverse_view = mat4_inverse(view);
    projection->inverse_projection = mat4_inverse(projection_matrix);
}

Vec3 coordinate_world_to_clip(const Projection3D *projection, Vec3 world)
{
    if (!projection) return world;
    return mat4_transform_point(&projection->view_projection, world);
}

Vec3 coordinate_clip_to_world(const Projection3D *projection, Vec3 clip)
{
    if (!projection) return clip;
    Mat4 inv_vp = mat4_multiply(&projection->inverse_view, &projection->inverse_projection);
    Vec3 world = mat4_transform_point(&inv_vp, clip);
    return world;
}
