#include <float.h>
#include <math.h>
#include <stdio.h>

#include "test_framework.h"

#include "core/math/coordinate_systems.h"
#include "services/render/backend/common/render_composition.h"
#include "services/render/backend/common/ui_mesh_builder.h"

static void compute_bounds(const UiVertexBuffer *background, const UiTextVertexBuffer *text, float *bg_min_x, float *bg_max_x,
                           float *bg_min_y, float *bg_max_y, float *text_min_x, float *text_max_x, float *text_min_y,
                           float *text_max_y)
{
    *bg_min_x = FLT_MAX;
    *bg_max_x = -FLT_MAX;
    *bg_min_y = FLT_MAX;
    *bg_max_y = -FLT_MAX;
    *text_min_x = FLT_MAX;
    *text_max_x = -FLT_MAX;
    *text_min_y = FLT_MAX;
    *text_max_y = -FLT_MAX;

    for (size_t i = 0; i < background->count; ++i) {
        const UiVertex *v = &background->vertices[i];
        if (v->position[0] < *bg_min_x) *bg_min_x = v->position[0];
        if (v->position[0] > *bg_max_x) *bg_max_x = v->position[0];
        if (v->position[1] < *bg_min_y) *bg_min_y = v->position[1];
        if (v->position[1] > *bg_max_y) *bg_max_y = v->position[1];
    }

    for (size_t i = 0; i < text->count; ++i) {
        const UiTextVertex *v = &text->vertices[i];
        if (v->position[0] < *text_min_x) *text_min_x = v->position[0];
        if (v->position[0] > *text_max_x) *text_max_x = v->position[0];
        if (v->position[1] < *text_min_y) *text_min_y = v->position[1];
        if (v->position[1] > *text_max_y) *text_max_y = v->position[1];
    }
}

static int verify_clip_alignment_1(void)
{
    float dpi_scale = 1.0f;
    CoordinateSystem2D transformer;
    coordinate_system2d_init(&transformer, dpi_scale, 1.0f, (Vec2){200.0f, 200.0f});

    Mat4 projection = mat4_identity();
    RenderContext context;
    render_context_init(&context, &transformer, &projection);

    Renderer renderer;
    renderer_init(&renderer, &context, 0);

    LayoutBox clip_box = {{10.0f, 12.0f}, {80.0f, 40.0f}};
    LayoutResult clip_device = layout_resolve(&clip_box, &context);

    ViewModel background = {
        .id = "container",
        .logical_box = {{0.0f, 0.0f}, {120.0f, 70.0f}},
        .has_clip = 1,
        .has_device_clip = 1,
        .clip = clip_box,
        .clip_device = clip_device,
        .layer = 1,
        .phase = RENDER_PHASE_BACKGROUND,
        .widget_order = 0,
        .ordinal = 0,
        .color = {1.0f, 0.0f, 0.0f, 1.0f},
    };

    GlyphQuad glyph = {
        .min = {0.0f, 0.0f},
        .max = {120.0f, 70.0f},
        .uv0 = {0.0f, 0.0f},
        .uv1 = {1.0f, 1.0f},
        .color = {1.0f, 1.0f, 1.0f, 1.0f},
        .widget_id = "container",
        .widget_order = 0,
        .layer = 2,
        .phase = RENDER_PHASE_CONTENT,
        .ordinal = 0,
        .has_clip = 1,
        .has_device_clip = 1,
        .clip = clip_box,
        .clip_device = clip_device,
    };

    UiVertexBuffer background_buffer;
    UiTextVertexBuffer text_buffer;
    TEST_ASSERT_INT_EQ(0, ui_vertex_buffer_init(&background_buffer, 0));
    TEST_ASSERT_INT_EQ(0, ui_text_vertex_buffer_init(&text_buffer, 0));

    int status = renderer_fill_vertices(&renderer, &background, 1, &glyph, 1, &background_buffer, &text_buffer);
    TEST_ASSERT_INT_EQ(0, status);
    TEST_ASSERT_INT_EQ(6, background_buffer.count);
    TEST_ASSERT_INT_EQ(6, text_buffer.count);

    float bg_min_x, bg_max_x, bg_min_y, bg_max_y;
    float text_min_x, text_max_x, text_min_y, text_max_y;
    compute_bounds(&background_buffer, &text_buffer, &bg_min_x, &bg_max_x, &bg_min_y, &bg_max_y, &text_min_x, &text_max_x,
                   &text_min_y, &text_max_y);

    const float eps = 1e-3f;
    TEST_ASSERT_FLOAT_EQ(bg_min_x, text_min_x, eps);
    TEST_ASSERT_FLOAT_EQ(bg_max_x, text_max_x, eps);
    TEST_ASSERT_FLOAT_EQ(bg_min_y, text_min_y, eps);
    TEST_ASSERT_FLOAT_EQ(bg_max_y, text_max_y, eps);

    ui_vertex_buffer_dispose(&background_buffer);
    ui_text_vertex_buffer_dispose(&text_buffer);
    renderer_dispose(&renderer);
    return 1;
}

static int verify_clip_alignment_1_5(void)
{
    float dpi_scale = 1.5f;
    // Same logic as above but scaled. Duplication for simplicity in test runner context or refactor helper?
    // I'll refactor logic to helper if I could pass it, but static helper is fine.
    
    CoordinateSystem2D transformer;
    coordinate_system2d_init(&transformer, dpi_scale, 1.0f, (Vec2){200.0f, 200.0f});

    Mat4 projection = mat4_identity();
    RenderContext context;
    render_context_init(&context, &transformer, &projection);

    Renderer renderer;
    renderer_init(&renderer, &context, 0);

    LayoutBox clip_box = {{10.0f, 12.0f}, {80.0f, 40.0f}};
    LayoutResult clip_device = layout_resolve(&clip_box, &context);

    ViewModel background = {
        .id = "container",
        .logical_box = {{0.0f, 0.0f}, {120.0f, 70.0f}},
        .has_clip = 1,
        .has_device_clip = 1,
        .clip = clip_box,
        .clip_device = clip_device,
        .layer = 1,
        .phase = RENDER_PHASE_BACKGROUND,
        .widget_order = 0,
        .ordinal = 0,
        .color = {1.0f, 0.0f, 0.0f, 1.0f},
    };

    GlyphQuad glyph = {
        .min = {0.0f, 0.0f},
        .max = {120.0f, 70.0f},
        .uv0 = {0.0f, 0.0f},
        .uv1 = {1.0f, 1.0f},
        .color = {1.0f, 1.0f, 1.0f, 1.0f},
        .widget_id = "container",
        .widget_order = 0,
        .layer = 2,
        .phase = RENDER_PHASE_CONTENT,
        .ordinal = 0,
        .has_clip = 1,
        .has_device_clip = 1,
        .clip = clip_box,
        .clip_device = clip_device,
    };

    UiVertexBuffer background_buffer;
    UiTextVertexBuffer text_buffer;
    TEST_ASSERT_INT_EQ(0, ui_vertex_buffer_init(&background_buffer, 0));
    TEST_ASSERT_INT_EQ(0, ui_text_vertex_buffer_init(&text_buffer, 0));

    int status = renderer_fill_vertices(&renderer, &background, 1, &glyph, 1, &background_buffer, &text_buffer);
    TEST_ASSERT_INT_EQ(0, status);

    float bg_min_x, bg_max_x, bg_min_y, bg_max_y;
    float text_min_x, text_max_x, text_min_y, text_max_y;
    compute_bounds(&background_buffer, &text_buffer, &bg_min_x, &bg_max_x, &bg_min_y, &bg_max_y, &text_min_x, &text_max_x,
                   &text_min_y, &text_max_y);

    const float eps = 1e-3f;
    TEST_ASSERT_FLOAT_EQ(bg_min_x, text_min_x, eps);
    TEST_ASSERT_FLOAT_EQ(bg_max_x, text_max_x, eps);
    TEST_ASSERT_FLOAT_EQ(bg_min_y, text_min_y, eps);
    TEST_ASSERT_FLOAT_EQ(bg_max_y, text_max_y, eps);

    ui_vertex_buffer_dispose(&background_buffer);
    ui_text_vertex_buffer_dispose(&text_buffer);
    renderer_dispose(&renderer);
    return 1;
}

int main(void)
{
    RUN_TEST(verify_clip_alignment_1);
    RUN_TEST(verify_clip_alignment_1_5);
    
    printf("Tests Run: %d, Failed: %d\n", g_tests_run, g_tests_failed);
    return g_tests_failed > 0 ? 1 : 0;
}