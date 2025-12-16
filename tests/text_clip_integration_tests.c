#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>

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

static void verify_clip_alignment(float dpi_scale)
{
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
    assert(ui_vertex_buffer_init(&background_buffer, 0) == 0);
    assert(ui_text_vertex_buffer_init(&text_buffer, 0) == 0);

    int status = renderer_fill_vertices(&renderer, &background, 1, &glyph, 1, &background_buffer, &text_buffer);
    assert(status == 0);
    assert(background_buffer.count == 6);
    assert(text_buffer.count == 6);

    float bg_min_x, bg_max_x, bg_min_y, bg_max_y;
    float text_min_x, text_max_x, text_min_y, text_max_y;
    compute_bounds(&background_buffer, &text_buffer, &bg_min_x, &bg_max_x, &bg_min_y, &bg_max_y, &text_min_x, &text_max_x,
                   &text_min_y, &text_max_y);

    const float eps = 1e-3f;
    assert(fabsf(bg_min_x - text_min_x) < eps);
    assert(fabsf(bg_max_x - text_max_x) < eps);
    assert(fabsf(bg_min_y - text_min_y) < eps);
    assert(fabsf(bg_max_y - text_max_y) < eps);

    ui_vertex_buffer_dispose(&background_buffer);
    ui_text_vertex_buffer_dispose(&text_buffer);
    renderer_dispose(&renderer);
}

int main(void)
{
    float scales[] = {1.0f, 1.5f};
    for (size_t i = 0; i < sizeof(scales) / sizeof(scales[0]); ++i) {
        verify_clip_alignment(scales[i]);
    }

    printf("text_clip_integration_tests passed\n");
    return 0;
}
