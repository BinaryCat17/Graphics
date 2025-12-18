#include <math.h>
#include <stdio.h>

#include "test_framework.h"

#include "foundation/math/coordinate_systems.h"

static int test_coordinate_round_trip(void) {
    CoordinateSystem2D system;
    coordinate_system2d_init(&system, 2.0f, 1.5f, (Vec2){300.0f, 200.0f});

    Vec2 world = {10.0f, 20.0f};
    Vec2 logical = coordinate_world_to_logical(&system, world);
    Vec2 screen = coordinate_world_to_screen(&system, world);

    TEST_ASSERT_FLOAT_EQ(15.0f, logical.x, 0.0001f);
    TEST_ASSERT_FLOAT_EQ(30.0f, logical.y, 0.0001f);
    TEST_ASSERT_FLOAT_EQ(30.0f, screen.x, 0.0001f);
    TEST_ASSERT_FLOAT_EQ(60.0f, screen.y, 0.0001f);

    Vec2 roundtrip_world = coordinate_screen_to_world(&system, screen);
    TEST_ASSERT_FLOAT_EQ(world.x, roundtrip_world.x, 0.0001f);
    TEST_ASSERT_FLOAT_EQ(world.y, roundtrip_world.y, 0.0001f);

    return 1;
}

static int test_local_to_world(void) {
    Transform2D local = {
        .translation = {2.0f, -1.0f},
        .rotation_radians = 0.25f,
        .scale = {2.0f, 0.5f},
    };
    Vec2 local_point = {1.0f, 1.0f};
    Vec2 world = coordinate_local_to_world_2d(&local, local_point);
    Vec2 back = coordinate_world_to_local_2d(&local, world);
    TEST_ASSERT_FLOAT_EQ(local_point.x, back.x, 0.0001f);
    TEST_ASSERT_FLOAT_EQ(local_point.y, back.y, 0.0001f);
    return 1;
}

static int test_3d_projection(void) {
    Transform3D local = {
        .translation = {1.0f, 2.0f, -3.0f},
        .scale = {1.0f, 2.0f, 1.0f},
        .rotation = quat_from_euler((EulerAngles){0.0f, 0.25f, 0.0f}),
    };
    Vec3 local_point = {0.5f, -0.25f, 1.0f};
    Vec3 world = coordinate_local_to_world_3d(&local, local_point);
    Vec3 back = coordinate_world_to_local_3d(&local, world);
    TEST_ASSERT_FLOAT_EQ(local_point.x, back.x, 0.0001f);
    TEST_ASSERT_FLOAT_EQ(local_point.y, back.y, 0.0001f);
    TEST_ASSERT_FLOAT_EQ(local_point.z, back.z, 0.0001f);

    Mat4 view = mat4_identity();
    Mat4 projection = mat4_orthographic(-2.0f, 2.0f, -2.0f, 2.0f, 0.1f, 10.0f);
    Projection3D clipper;
    projection3d_init(&clipper, &view, &projection);
    Vec3 clip = coordinate_world_to_clip(&clipper, world);
    Vec3 restored = coordinate_clip_to_world(&clipper, clip);
    TEST_ASSERT_FLOAT_EQ(world.x, restored.x, 0.0001f);
    TEST_ASSERT_FLOAT_EQ(world.y, restored.y, 0.0001f);
    TEST_ASSERT_FLOAT_EQ(world.z, restored.z, 0.0001f);
    return 1;
}

int main(void) {
    RUN_TEST(test_coordinate_round_trip);
    RUN_TEST(test_local_to_world);
    RUN_TEST(test_3d_projection);
    
    printf("Tests Run: %d, Failed: %d\n", g_tests_run, g_tests_failed);
    return g_tests_failed > 0 ? 1 : 0;
}