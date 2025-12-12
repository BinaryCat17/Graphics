#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>

#include "coordinate_systems/coordinate_systems.h"
#include "render/common/render_composition.h"
#include "render/common/ui_mesh_builder.h"

static Renderer create_renderer_with_scale(float dpi_scale, float ui_scale, Vec2 viewport_size)
{
    CoordinateSystem2D transformer;
    coordinate_system2d_init(&transformer, dpi_scale, ui_scale, viewport_size);

    RenderContext ctx;
    render_context_init(&ctx, &transformer, NULL);

    Renderer renderer;
    renderer_init(&renderer, &ctx, 0);
    return renderer;
}

static Renderer create_renderer(void)
{
    return create_renderer_with_scale(1.0f, 1.0f, (Vec2){800.0f, 600.0f});
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

    Renderer scaled_renderer = create_renderer_with_scale(2.0f, 1.0f, (Vec2){800.0f, 600.0f});
    ViewModel clipped_view = {.id = "clipped_quad",
                              .logical_box = {{1.0f, 2.0f}, {10.0f, 6.0f}},
                              .has_clip = 1,
                              .clip = {{3.5f, 3.5f}, {5.0f, 3.0f}},
                              .layer = 2,
                              .phase = RENDER_PHASE_BACKGROUND,
                              .widget_order = 0,
                              .ordinal = 0,
                              .color = {0.0f, 0.0f, 0.0f, 1.0f}};
    GlyphQuad clipped_glyph = {.min = {1.0f, 2.0f},
                               .max = {11.0f, 8.0f},
                               .uv0 = {0.0f, 0.0f},
                               .uv1 = {1.0f, 1.0f},
                               .color = {1, 1, 1, 1},
                               .layer = 1,
                               .phase = RENDER_PHASE_CONTENT,
                               .ordinal = 0,
                               .widget_order = 0,
                               .has_clip = 1,
                               .clip = {{3.5f, 3.5f}, {5.0f, 3.0f}}};
    UiVertexBuffer clipped_background;
    UiTextVertexBuffer clipped_text;
    assert(ui_vertex_buffer_init(&clipped_background, 0) == 0);
    assert(ui_text_vertex_buffer_init(&clipped_text, 0) == 0);

    int clipped_status =
        renderer_fill_vertices(&scaled_renderer, &clipped_view, 1, &clipped_glyph, 1, &clipped_background, &clipped_text);
    assert(clipped_status == 0);
    assert(clipped_background.count == 6);
    assert(clipped_text.count == 6);

    float bg_min_x = FLT_MAX, bg_min_y = FLT_MAX, bg_max_x = -FLT_MAX, bg_max_y = -FLT_MAX;
    float glyph_min_x = FLT_MAX, glyph_min_y = FLT_MAX, glyph_max_x = -FLT_MAX, glyph_max_y = -FLT_MAX;
    for (size_t i = 0; i < clipped_background.count; ++i) {
        bg_min_x = fminf(bg_min_x, clipped_background.vertices[i].position[0]);
        bg_min_y = fminf(bg_min_y, clipped_background.vertices[i].position[1]);
        bg_max_x = fmaxf(bg_max_x, clipped_background.vertices[i].position[0]);
        bg_max_y = fmaxf(bg_max_y, clipped_background.vertices[i].position[1]);
    }
    for (size_t i = 0; i < clipped_text.count; ++i) {
        glyph_min_x = fminf(glyph_min_x, clipped_text.vertices[i].position[0]);
        glyph_min_y = fminf(glyph_min_y, clipped_text.vertices[i].position[1]);
        glyph_max_x = fmaxf(glyph_max_x, clipped_text.vertices[i].position[0]);
        glyph_max_y = fmaxf(glyph_max_y, clipped_text.vertices[i].position[1]);
    }

    assert(fabsf(bg_min_x - glyph_min_x) < 1e-6f);
    assert(fabsf(bg_min_y - glyph_min_y) < 1e-6f);
    assert(fabsf(bg_max_x - glyph_max_x) < 1e-6f);
    assert(fabsf(bg_max_y - glyph_max_y) < 1e-6f);

    float glyph_min_u = FLT_MAX, glyph_min_v = FLT_MAX, glyph_max_u = -FLT_MAX, glyph_max_v = -FLT_MAX;
    for (size_t i = 0; i < clipped_text.count; ++i) {
        glyph_min_u = fminf(glyph_min_u, clipped_text.vertices[i].uv[0]);
        glyph_min_v = fminf(glyph_min_v, clipped_text.vertices[i].uv[1]);
        glyph_max_u = fmaxf(glyph_max_u, clipped_text.vertices[i].uv[0]);
        glyph_max_v = fmaxf(glyph_max_v, clipped_text.vertices[i].uv[1]);
    }

    assert(fabsf(glyph_min_u - 0.25f) < 1e-6f);
    assert(fabsf(glyph_max_u - 0.75f) < 1e-6f);
    assert(fabsf(glyph_min_v - 0.25f) < 1e-6f);
    assert(fabsf(glyph_max_v - 0.75f) < 1e-6f);

    ui_vertex_buffer_dispose(&clipped_background);
    ui_text_vertex_buffer_dispose(&clipped_text);
    renderer_dispose(&scaled_renderer);

    ui_vertex_buffer_dispose(&background);
    ui_text_vertex_buffer_dispose(&text);
    renderer_dispose(&renderer);

    printf("ui_mesh_builder_tests passed\n");
    return 0;
}

