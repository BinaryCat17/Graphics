#include <stdio.h>

#include "test_framework.h"

#include "core/math/coordinate_systems.h"
#include "services/render/backend/common/render_composition.h"
#include "services/render/backend/common/ui_mesh_builder.h"

static Renderer create_renderer(void)
{
    CoordinateSystem2D transformer;
    coordinate_system2d_init(&transformer, 1.0f, 1.0f, (Vec2){800.0f, 600.0f});

    RenderContext ctx;
    render_context_init(&ctx, &transformer, NULL);

    Renderer renderer;
    renderer_init(&renderer, &ctx, 0);
    return renderer;
}

static int test_background_vertices(void)
{
    Renderer renderer = create_renderer();

    ViewModel view = {.id = "quad", .logical_box = {{0.0f, 0.0f}, {1.0f, 1.0f}}, .layer = 1, .phase = RENDER_PHASE_BACKGROUND, .widget_order = 0, .ordinal = 0, .color = {0.25f, 0.5f, 0.75f, 1.0f}};
    UiVertexBuffer background;
    TEST_ASSERT_INT_EQ(0, ui_vertex_buffer_init(&background, 0));

    int background_status = renderer_fill_background_vertices(&renderer, &view, 1, &background);
    TEST_ASSERT_INT_EQ(0, background_status);
    TEST_ASSERT_INT_EQ(6, background.count);
    TEST_ASSERT_FLOAT_EQ(1.0f, background.vertices[0].position[2], 0.0001f);
    TEST_ASSERT_FLOAT_EQ(0.5f, background.vertices[0].color.g, 0.0001f);

    ui_vertex_buffer_dispose(&background);
    renderer_dispose(&renderer);
    return 1;
}

static int test_text_vertices(void)
{
    Renderer renderer = create_renderer();

    GlyphQuad glyph = {.min = {0.0f, 0.0f}, .max = {1.0f, 1.0f}, .uv0 = {0.0f, 0.0f}, .uv1 = {1.0f, 1.0f}, .color = {1, 1, 1, 1}, .layer = 0, .phase = RENDER_PHASE_CONTENT, .ordinal = 0, .widget_order = 0};
    UiTextVertexBuffer text;
    TEST_ASSERT_INT_EQ(0, ui_text_vertex_buffer_init(&text, 0));

    int text_status = renderer_fill_text_vertices(&renderer, &glyph, 1, &text);
    TEST_ASSERT_INT_EQ(0, text_status);
    TEST_ASSERT_INT_EQ(6, text.count);
    
    float min_u = 1.0f;
    float max_u = 0.0f;
    float min_v = 1.0f;
    float max_v = 0.0f;
    for (size_t i = 0; i < text.count; ++i) {
        if (text.vertices[i].uv[0] < min_u) min_u = text.vertices[i].uv[0];
        if (text.vertices[i].uv[0] > max_u) max_u = text.vertices[i].uv[0];
        if (text.vertices[i].uv[1] < min_v) min_v = text.vertices[i].uv[1];
        if (text.vertices[i].uv[1] > max_v) max_v = text.vertices[i].uv[1];
    }
    
    TEST_ASSERT_FLOAT_EQ(0.0f, min_u, 0.0001f);
    TEST_ASSERT_FLOAT_EQ(1.0f, max_u, 0.0001f);
    TEST_ASSERT_FLOAT_EQ(0.0f, min_v, 0.0001f);
    TEST_ASSERT_FLOAT_EQ(1.0f, max_v, 0.0001f);

    ui_text_vertex_buffer_dispose(&text);
    renderer_dispose(&renderer);
    return 1;
}

int main(void)
{
    RUN_TEST(test_background_vertices);
    RUN_TEST(test_text_vertices);

    printf("Tests Run: %d, Failed: %d\n", g_tests_run, g_tests_failed);
    return g_tests_failed > 0 ? 1 : 0;
}