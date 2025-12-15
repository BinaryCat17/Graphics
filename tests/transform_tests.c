#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "coordinate_systems/coordinate_systems.h"
#include "coordinate_systems/layout_geometry.h"

static int nearly_equal(float a, float b) {
    return fabsf(a - b) < 0.0001f;
}

static void test_coordinate_round_trip(void) {
    CoordinateSystem2D system;
    coordinate_system2d_init(&system, 2.0f, 1.5f, (Vec2){300.0f, 200.0f});

    Vec2 world = {10.0f, 20.0f};
    Vec2 logical = coordinate_world_to_logical(&system, world);
    Vec2 screen = coordinate_world_to_screen(&system, world);

    assert(nearly_equal(logical.x, 15.0f));
    assert(nearly_equal(logical.y, 30.0f));
    assert(nearly_equal(screen.x, 30.0f));
    assert(nearly_equal(screen.y, 60.0f));

    Vec2 roundtrip_world = coordinate_screen_to_world(&system, screen);
    assert(nearly_equal(roundtrip_world.x, world.x));
    assert(nearly_equal(roundtrip_world.y, world.y));

    Mat4 projection = mat4_identity();
    RenderContext ctx;
    render_context_init(&ctx, &system, &projection);

    LayoutBox logical_box = {{5.0f, 5.0f}, {10.0f, 10.0f}};
    LayoutResult layout = layout_resolve(&logical_box, &ctx);
    assert(nearly_equal(layout.device.size.x, 20.0f));
    assert(nearly_equal(layout.device.size.y, 20.0f));

    Vec2 inside = {7.0f, 7.0f};
    Vec2 outside = {40.0f, 3.0f};
    assert(layout_hit_test(&layout, inside));
    assert(!layout_hit_test(&layout, outside));
}

static void test_local_to_world(void) {
    Transform2D local = {
        .translation = {2.0f, -1.0f},
        .rotation_radians = 0.25f,
        .scale = {2.0f, 0.5f},
    };
    Vec2 local_point = {1.0f, 1.0f};
    Vec2 world = coordinate_local_to_world_2d(&local, local_point);
    Vec2 back = coordinate_world_to_local_2d(&local, world);
    assert(nearly_equal(back.x, local_point.x));
    assert(nearly_equal(back.y, local_point.y));
}

static void test_3d_projection(void) {
    Transform3D local = {
        .translation = {1.0f, 2.0f, -3.0f},
        .scale = {1.0f, 2.0f, 1.0f},
        .rotation = quat_from_euler((EulerAngles){0.0f, 0.25f, 0.0f}),
    };
    Vec3 local_point = {0.5f, -0.25f, 1.0f};
    Vec3 world = coordinate_local_to_world_3d(&local, local_point);
    Vec3 back = coordinate_world_to_local_3d(&local, world);
    assert(nearly_equal(back.x, local_point.x));
    assert(nearly_equal(back.y, local_point.y));
    assert(nearly_equal(back.z, local_point.z));

    Mat4 view = mat4_identity();
    Mat4 projection = mat4_orthographic(-2.0f, 2.0f, -2.0f, 2.0f, 0.1f, 10.0f);
    Projection3D clipper;
    projection3d_init(&clipper, &view, &projection);
    Vec3 clip = coordinate_world_to_clip(&clipper, world);
    Vec3 restored = coordinate_clip_to_world(&clipper, clip);
    assert(nearly_equal(restored.x, world.x));
    assert(nearly_equal(restored.y, world.y));
    assert(nearly_equal(restored.z, world.z));
}

int main(void) {
    test_coordinate_round_trip();
    test_local_to_world();
    test_3d_projection();
    printf("transform_tests passed\n");
    return 0;
}

