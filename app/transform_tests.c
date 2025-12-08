#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "Graphics.h"

static int nearly_equal(float a, float b) {
    return fabsf(a - b) < 0.0001f;
}

int main(void) {
    CoordinateTransformer transformer;
    coordinate_transformer_init(&transformer, 2.0f, 1.5f, (Vec2){300.0f, 200.0f});

    Vec2 world = {10.0f, 20.0f};
    Vec2 logical = coordinate_world_to_logical(&transformer, world);
    Vec2 screen = coordinate_world_to_screen(&transformer, world);

    assert(nearly_equal(logical.x, 15.0f));
    assert(nearly_equal(logical.y, 30.0f));
    assert(nearly_equal(screen.x, 30.0f));
    assert(nearly_equal(screen.y, 60.0f));

    Vec2 roundtrip_world = coordinate_screen_to_world(&transformer, screen);
    assert(nearly_equal(roundtrip_world.x, world.x));
    assert(nearly_equal(roundtrip_world.y, world.y));

    RenderContext ctx;
    float projection[16] = {0};
    projection[0] = projection[5] = projection[10] = projection[15] = 1.0f;
    render_context_init(&ctx, &transformer, projection);

    LayoutBox logical_box = {{5.0f, 5.0f}, {10.0f, 10.0f}};
    LayoutResult layout = layout_resolve(&logical_box, &ctx);
    assert(nearly_equal(layout.device.size.x, 20.0f));
    assert(nearly_equal(layout.device.size.y, 20.0f));

    Vec2 inside = {7.0f, 7.0f};
    Vec2 outside = {40.0f, 3.0f};
    assert(layout_hit_test(&layout, inside));
    assert(!layout_hit_test(&layout, outside));

    printf("transform_tests passed\n");
    return 0;
}
