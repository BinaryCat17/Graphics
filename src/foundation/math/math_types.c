#include "math_types.h"
#include <math.h>

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
    m.m[10] = 1.0f / (far_z - near_z);
    m.m[12] = -(right + left) / (right - left);
    m.m[13] = -(top + bottom) / (top - bottom);
    m.m[14] = -near_z / (far_z - near_z);
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
