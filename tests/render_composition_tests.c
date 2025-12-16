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

static int test_render_sort_order(void)
{
    Renderer renderer = create_renderer();

    ViewModel views[2] = {
        {.id = "b", .logical_box = {{10.0f, 0.0f}, {5.0f, 5.0f}}, .layer = 1, .phase = RENDER_PHASE_BACKGROUND, .widget_order = 2, .ordinal = 0, .color = {1, 0, 0, 1}},
        {.id = "a", .logical_box = {{0.0f, 0.0f}, {5.0f, 5.0f}}, .layer = 0, .phase = RENDER_PHASE_CONTENT, .widget_order = 1, .ordinal = 1, .color = {0, 1, 0, 1}},
    };

    GlyphQuad glyphs[1] = {
        {.min = {0.0f, 0.0f}, .max = {5.0f, 5.0f}, .uv0 = {0.0f, 0.0f}, .uv1 = {1.0f, 1.0f}, .color = {1, 1, 1, 1}, .layer = 0, .phase = RENDER_PHASE_OVERLAY, .ordinal = 0, .widget_order = 0},
    };

    RenderBuildResult result = renderer_build_commands(&renderer, views, 2, glyphs, 1);
    TEST_ASSERT(result == RENDER_BUILD_OK);
    TEST_ASSERT_INT_EQ(3, renderer.command_list.count);

    size_t background_count = 0;
    size_t glyph_count = 0;
    for (size_t i = 1; i < renderer.command_list.count; ++i) {
        const RenderSortKey *prev = &renderer.command_list.commands[i - 1].key;
        const RenderSortKey *curr = &renderer.command_list.commands[i].key;

        int non_decreasing =
            (curr->layer > prev->layer) ||
            (curr->layer == prev->layer && curr->widget_order >= prev->widget_order &&
             (curr->widget_order != prev->widget_order || curr->phase >= prev->phase) &&
             (curr->widget_order != prev->widget_order || curr->phase != prev->phase || curr->ordinal >= prev->ordinal));
        
        if (!non_decreasing) {
            fprintf(stderr, "Sorting error at index %zu vs %zu\n", i, i-1);
            return 0;
        }
    }

    for (size_t i = 0; i < renderer.command_list.count; ++i) {
        if (renderer.command_list.commands[i].primitive == RENDER_PRIMITIVE_BACKGROUND) {
            background_count++;
        } else if (renderer.command_list.commands[i].primitive == RENDER_PRIMITIVE_GLYPH) {
            glyph_count++;
        }
    }

    TEST_ASSERT_INT_EQ(2, background_count);
    TEST_ASSERT_INT_EQ(1, glyph_count);

    renderer_dispose(&renderer);
    return 1;
}

int main(void)
{
    RUN_TEST(test_render_sort_order);
    
    printf("Tests Run: %d, Failed: %d\n", g_tests_run, g_tests_failed);
    return g_tests_failed > 0 ? 1 : 0;
}