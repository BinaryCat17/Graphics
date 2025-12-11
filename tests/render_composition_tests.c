#include <assert.h>
#include <stdio.h>

#include "core/Graphics.h"

static Renderer create_renderer(void)
{
    CoordinateTransformer transformer;
    coordinate_transformer_init(&transformer, 1.0f, 1.0f, (Vec2){800.0f, 600.0f});

    RenderContext ctx;
    render_context_init(&ctx, &transformer, NULL);

    Renderer renderer;
    renderer_init(&renderer, &ctx, 0);
    return renderer;
}

int main(void)
{
    Renderer renderer = create_renderer();

    ViewModel views[2] = {
        {.id = "b", .logical_box = {{10.0f, 0.0f}, {5.0f, 5.0f}}, .layer = 1, .phase = RENDER_PHASE_BACKGROUND, .widget_order = 2, .ordinal = 0, .color = {1, 0, 0, 1}},
        {.id = "a", .logical_box = {{0.0f, 0.0f}, {5.0f, 5.0f}}, .layer = 0, .phase = RENDER_PHASE_CONTENT, .widget_order = 1, .ordinal = 1, .color = {0, 1, 0, 1}},
    };

    GlyphQuad glyphs[1] = {
        {.min = {0.0f, 0.0f}, .max = {5.0f, 5.0f}, .uv0 = {0.0f, 0.0f}, .uv1 = {1.0f, 1.0f}, .color = {1, 1, 1, 1}, .layer = 0, .phase = RENDER_PHASE_OVERLAY, .ordinal = 0, .widget_order = 0},
    };

    int result = renderer_build_commands(&renderer, views, 2, glyphs, 1);
    assert(result == 0);
    assert(renderer.command_list.count == 3);

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
        assert(non_decreasing);
    }

    for (size_t i = 0; i < renderer.command_list.count; ++i) {
        if (renderer.command_list.commands[i].primitive == RENDER_PRIMITIVE_BACKGROUND) {
            background_count++;
        } else if (renderer.command_list.commands[i].primitive == RENDER_PRIMITIVE_GLYPH) {
            glyph_count++;
        }
    }

    assert(background_count == 2);
    assert(glyph_count == 1);

    renderer_dispose(&renderer);
    printf("render_composition_tests passed\n");
    return 0;
}

