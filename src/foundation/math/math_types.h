#ifndef MATH_TYPES_H
#define MATH_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Vec2 {
    float x;
    float y;
} Vec2;

typedef struct Vec3 {
    float x;
    float y;
    float z;
} Vec3;

typedef struct Vec4 {
    float x; // REFLECT
    float y; // REFLECT
    float z; // REFLECT
    float w; // REFLECT
} Vec4;

typedef struct Rect {
    float x;
    float y;
    float w;
    float h;
} Rect;

typedef struct Quat {
    float x;
    float y;
    float z;
    float w;
} Quat;

typedef struct EulerAngles {
    float pitch;
    float yaw;
    float roll;
} EulerAngles;

typedef struct Mat4 {
    float m[16];
} Mat4;

Mat4 mat4_identity(void);
Mat4 mat4_translation(Vec3 t);
Mat4 mat4_scale(Vec3 s);
Mat4 mat4_rotation_quat(Quat q);
Mat4 mat4_rotation_euler(EulerAngles euler);
Mat4 mat4_multiply(const Mat4 *a, const Mat4 *b);
Mat4 mat4_inverse(const Mat4 *m);
Mat4 mat4_perspective(float fov_y_radians, float aspect, float near_z, float far_z);
Mat4 mat4_orthographic(float left, float right, float bottom, float top, float near_z, float far_z);
Vec3 mat4_transform_point(const Mat4 *m, Vec3 p);
Vec3 mat4_transform_direction(const Mat4 *m, Vec3 v);

Quat quat_from_euler(EulerAngles euler);
Quat quat_conjugate(Quat q);
Quat quat_normalize(Quat q);

#ifdef __cplusplus
}
#endif

#endif // MATH_TYPES_H
