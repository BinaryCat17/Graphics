#include "coordinate_systems.h"

#include <math.h>
#include <string.h>

Mat4 mat4_identity(void)
{
    Mat4 m = {0};
    m.m[0] = m.m[5] = m.m[10] = m.m[15] = 1.0f;
    return m;
}

Mat4 mat4_translation(Vec3 t)
{
    Mat4 m = mat4_identity();
    m.m[12] = t.x;
    m.m[13] = t.y;
    m.m[14] = t.z;
    return m;
}

Mat4 mat4_scale(Vec3 s)
{
    Mat4 m = {0};
    m.m[0] = s.x;
    m.m[5] = s.y;
    m.m[10] = s.z;
    m.m[15] = 1.0f;
    return m;
}

Quat quat_normalize(Quat q)
{
    float len = sqrtf(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (len <= 0.0f) {
        return (Quat){0.0f, 0.0f, 0.0f, 1.0f};
    }
    float inv = 1.0f / len;
    return (Quat){q.x * inv, q.y * inv, q.z * inv, q.w * inv};
}

Quat quat_conjugate(Quat q)
{
    return (Quat){-q.x, -q.y, -q.z, q.w};
}

Quat quat_from_euler(EulerAngles euler)
{
    float cy = cosf(euler.yaw * 0.5f);
    float sy = sinf(euler.yaw * 0.5f);
    float cp = cosf(euler.pitch * 0.5f);
    float sp = sinf(euler.pitch * 0.5f);
    float cr = cosf(euler.roll * 0.5f);
    float sr = sinf(euler.roll * 0.5f);

    Quat q;
    q.w = cr * cp * cy + sr * sp * sy;
    q.x = sr * cp * cy - cr * sp * sy;
    q.y = cr * sp * cy + sr * cp * sy;
    q.z = cr * cp * sy - sr * sp * cy;
    return quat_normalize(q);
}

Mat4 mat4_rotation_quat(Quat q)
{
    q = quat_normalize(q);
    float xx = q.x * q.x;
    float yy = q.y * q.y;
    float zz = q.z * q.z;
    float xy = q.x * q.y;
    float xz = q.x * q.z;
    float yz = q.y * q.z;
    float wx = q.w * q.x;
    float wy = q.w * q.y;
    float wz = q.w * q.z;

    Mat4 m = mat4_identity();
    m.m[0] = 1.0f - 2.0f * (yy + zz);
    m.m[4] = 2.0f * (xy + wz);
    m.m[8] = 2.0f * (xz - wy);

    m.m[1] = 2.0f * (xy - wz);
    m.m[5] = 1.0f - 2.0f * (xx + zz);
    m.m[9] = 2.0f * (yz + wx);

    m.m[2] = 2.0f * (xz + wy);
    m.m[6] = 2.0f * (yz - wx);
    m.m[10] = 1.0f - 2.0f * (xx + yy);

    return m;
}

Mat4 mat4_rotation_euler(EulerAngles euler)
{
    return mat4_rotation_quat(quat_from_euler(euler));
}

Mat4 mat4_multiply(const Mat4 *a, const Mat4 *b)
{
    Mat4 r = {0};
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            r.m[col * 4 + row] = a->m[0 * 4 + row] * b->m[col * 4 + 0] +
                                 a->m[1 * 4 + row] * b->m[col * 4 + 1] +
                                 a->m[2 * 4 + row] * b->m[col * 4 + 2] +
                                 a->m[3 * 4 + row] * b->m[col * 4 + 3];
        }
    }
    return r;
}

static float mat4_det3x3(float a1, float a2, float a3, float b1, float b2, float b3, float c1, float c2, float c3)
{
    return a1 * (b2 * c3 - b3 * c2) - a2 * (b1 * c3 - b3 * c1) + a3 * (b1 * c2 - b2 * c1);
}

Mat4 mat4_inverse(const Mat4 *m)
{
    Mat4 inv;
    float det;
    float inv_det;

    inv.m[0] = mat4_det3x3(m->m[5], m->m[6], m->m[7], m->m[9], m->m[10], m->m[11], m->m[13], m->m[14], m->m[15]);
    inv.m[1] = -mat4_det3x3(m->m[1], m->m[2], m->m[3], m->m[9], m->m[10], m->m[11], m->m[13], m->m[14], m->m[15]);
    inv.m[2] = mat4_det3x3(m->m[1], m->m[2], m->m[3], m->m[5], m->m[6], m->m[7], m->m[13], m->m[14], m->m[15]);
    inv.m[3] = -mat4_det3x3(m->m[1], m->m[2], m->m[3], m->m[5], m->m[6], m->m[7], m->m[9], m->m[10], m->m[11]);

    inv.m[4] = -mat4_det3x3(m->m[4], m->m[6], m->m[7], m->m[8], m->m[10], m->m[11], m->m[12], m->m[14], m->m[15]);
    inv.m[5] = mat4_det3x3(m->m[0], m->m[2], m->m[3], m->m[8], m->m[10], m->m[11], m->m[12], m->m[14], m->m[15]);
    inv.m[6] = -mat4_det3x3(m->m[0], m->m[2], m->m[3], m->m[4], m->m[6], m->m[7], m->m[12], m->m[14], m->m[15]);
    inv.m[7] = mat4_det3x3(m->m[0], m->m[2], m->m[3], m->m[4], m->m[6], m->m[7], m->m[8], m->m[10], m->m[11]);

    inv.m[8] = mat4_det3x3(m->m[4], m->m[5], m->m[7], m->m[8], m->m[9], m->m[11], m->m[12], m->m[13], m->m[15]);
    inv.m[9] = -mat4_det3x3(m->m[0], m->m[1], m->m[3], m->m[8], m->m[9], m->m[11], m->m[12], m->m[13], m->m[15]);
    inv.m[10] = mat4_det3x3(m->m[0], m->m[1], m->m[3], m->m[4], m->m[5], m->m[7], m->m[12], m->m[13], m->m[15]);
    inv.m[11] = -mat4_det3x3(m->m[0], m->m[1], m->m[3], m->m[4], m->m[5], m->m[7], m->m[8], m->m[9], m->m[11]);

    inv.m[12] = -mat4_det3x3(m->m[4], m->m[5], m->m[6], m->m[8], m->m[9], m->m[10], m->m[12], m->m[13], m->m[14]);
    inv.m[13] = mat4_det3x3(m->m[0], m->m[1], m->m[2], m->m[8], m->m[9], m->m[10], m->m[12], m->m[13], m->m[14]);
    inv.m[14] = -mat4_det3x3(m->m[0], m->m[1], m->m[2], m->m[4], m->m[5], m->m[6], m->m[12], m->m[13], m->m[14]);
    inv.m[15] = mat4_det3x3(m->m[0], m->m[1], m->m[2], m->m[4], m->m[5], m->m[6], m->m[8], m->m[9], m->m[10]);

    det = m->m[0] * inv.m[0] + m->m[1] * inv.m[4] + m->m[2] * inv.m[8] + m->m[3] * inv.m[12];
    if (fabsf(det) < 1e-6f) {
        return mat4_identity();
    }
    inv_det = 1.0f / det;
    for (int i = 0; i < 16; ++i) {
        inv.m[i] *= inv_det;
    }
    return inv;
}

Mat4 mat4_perspective(float fov_y_radians, float aspect, float near_z, float far_z)
{
    float f = 1.0f / tanf(fov_y_radians * 0.5f);
    Mat4 m = {0};
    m.m[0] = f / aspect;
    m.m[5] = f;
    m.m[10] = (far_z + near_z) / (near_z - far_z);
    m.m[11] = -1.0f;
    m.m[14] = (2.0f * far_z * near_z) / (near_z - far_z);
    return m;
}

Mat4 mat4_orthographic(float left, float right, float bottom, float top, float near_z, float far_z)
{
    Mat4 m = mat4_identity();
    m.m[0] = 2.0f / (right - left);
    m.m[5] = 2.0f / (top - bottom);
    m.m[10] = -2.0f / (far_z - near_z);
    m.m[12] = -(right + left) / (right - left);
    m.m[13] = -(top + bottom) / (top - bottom);
    m.m[14] = -(far_z + near_z) / (far_z - near_z);
    return m;
}

Vec3 mat4_transform_point(const Mat4 *m, Vec3 p)
{
    float x = p.x * m->m[0] + p.y * m->m[4] + p.z * m->m[8] + m->m[12];
    float y = p.x * m->m[1] + p.y * m->m[5] + p.z * m->m[9] + m->m[13];
    float z = p.x * m->m[2] + p.y * m->m[6] + p.z * m->m[10] + m->m[14];
    float w = p.x * m->m[3] + p.y * m->m[7] + p.z * m->m[11] + m->m[15];
    if (fabsf(w) > 1e-6f) {
        float inv_w = 1.0f / w;
        x *= inv_w;
        y *= inv_w;
        z *= inv_w;
    }
    return (Vec3){x, y, z};
}

Vec3 mat4_transform_direction(const Mat4 *m, Vec3 v)
{
    float x = v.x * m->m[0] + v.y * m->m[4] + v.z * m->m[8];
    float y = v.x * m->m[1] + v.y * m->m[5] + v.z * m->m[9];
    float z = v.x * m->m[2] + v.y * m->m[6] + v.z * m->m[10];
    return (Vec3){x, y, z};
}

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

