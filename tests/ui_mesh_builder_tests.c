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

    ViewModel view = {.id = "quad", .logical_box = {{0.0f, 0.0f}, {1.0f, 1.0f}}, .layer = 1, .phase = RENDER_PHASE_BACKGROUND, .widget_order = 0, .ordinal = 0, .color = {0.25f, 0.5f, 0.75f, 1.0f}};
    UiVertexBuffer background;
    assert(ui_vertex_buffer_init(&background, 0) == 0);

    int background_status = renderer_fill_background_vertices(&renderer, &view, 1, &background);
    assert(background_status == 0);
    assert(background.count == 6);
    assert(background.vertices[0].position[2] == 1.0f);
    assert(background.vertices[0].color.g == 0.5f);

    GlyphQuad glyph = {.min = {0.0f, 0.0f}, .max = {1.0f, 1.0f}, .uv0 = {0.0f, 0.0f}, .uv1 = {1.0f, 1.0f}, .color = {1, 1, 1, 1}, .layer = 0, .phase = RENDER_PHASE_CONTENT, .ordinal = 0, .widget_order = 0};
    UiTextVertexBuffer text;
    assert(ui_text_vertex_buffer_init(&text, 0) == 0);

    int text_status = renderer_fill_text_vertices(&renderer, &glyph, 1, &text);
    assert(text_status == 0);
    assert(text.count == 6);
    float min_u = 1.0f;
    float max_u = 0.0f;
    float min_v = 1.0f;
    float max_v = 0.0f;
    for (size_t i = 0; i < text.count; ++i) {
        if (text.vertices[i].uv[0] < min_u) {
            min_u = text.vertices[i].uv[0];
        }
        if (text.vertices[i].uv[0] > max_u) {
            max_u = text.vertices[i].uv[0];
        }
        if (text.vertices[i].uv[1] < min_v) {
            min_v = text.vertices[i].uv[1];
        }
        if (text.vertices[i].uv[1] > max_v) {
            max_v = text.vertices[i].uv[1];
        }
    }
    assert(min_u == 0.0f);
    assert(max_u == 1.0f);
    assert(min_v == 0.0f);
    assert(max_v == 1.0f);

    ui_vertex_buffer_dispose(&background);
    ui_text_vertex_buffer_dispose(&text);
    renderer_dispose(&renderer);

    printf("ui_mesh_builder_tests passed\n");
    return 0;
}

