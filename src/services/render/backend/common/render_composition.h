#pragma once

#include <stddef.h>

#include "core/math/layout_geometry.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Color {
    float r;
    float g;
    float b;
    float a;
} Color;

typedef enum RenderPhase {
    RENDER_PHASE_BACKGROUND = 0,
    RENDER_PHASE_CONTENT = 1,
    RENDER_PHASE_OVERLAY = 2,
} RenderPhase;

typedef struct GlyphQuad {
    Vec2 min;
    Vec2 max;
    Vec2 uv0;
    Vec2 uv1;
    Color color;
    const char *widget_id;
    size_t widget_order;
    int layer;
    RenderPhase phase;
    size_t ordinal;
    int has_clip;
    int has_device_clip;
    LayoutBox clip;
    LayoutResult clip_device;
} GlyphQuad;

/**
 * Representation of an immutable view model that the renderer consumes.
 * Game logic should translate its state into these view models before
 * invoking the renderer, keeping rendering free from mutation side-effects.
 */
typedef struct ViewModel {
    const char *id;
    LayoutBox logical_box;
    int has_clip;
    int has_device_clip;
    LayoutBox clip;
    LayoutResult clip_device;
    int layer;
    RenderPhase phase;
    size_t widget_order;
    size_t ordinal;
    Color color;
} ViewModel;

typedef enum RenderPrimitive {
    RENDER_PRIMITIVE_BACKGROUND = 0,
    RENDER_PRIMITIVE_GLYPH = 1,
} RenderPrimitive;

typedef struct RenderSortKey {
    int layer;
    size_t widget_order;
    RenderPhase phase;
    size_t ordinal;
} RenderSortKey;

typedef struct RenderCommand {
    RenderPrimitive primitive;
    RenderPhase phase;
    RenderSortKey key;
    int has_clip;
    LayoutResult clip;
    union {
        struct {
            LayoutResult layout;
            Color color;
        } background;
        GlyphQuad glyph;
    } data;
} RenderCommand;

typedef struct RenderCommandList {
    union {
        RenderCommand *data;
        RenderCommand *commands;
    };
    RenderCommand *scratch;
    size_t count;
    size_t capacity;
    size_t scratch_capacity;
} RenderCommandList;

/** Renderer that owns composition for a frame. */
typedef struct Renderer {
    RenderContext context;
    RenderCommandList command_list;
} Renderer;

typedef enum RenderBuildResult {
    RENDER_BUILD_OK = 0,
    RENDER_BUILD_ERROR_NULL_RENDERER,
    RENDER_BUILD_ERROR_INVALID_INPUT,
    RENDER_BUILD_ERROR_LAYOUT_RESOLVE,
    RENDER_BUILD_ERROR_BACKGROUND_APPEND,
    RENDER_BUILD_ERROR_GLYPH_APPEND,
    RENDER_BUILD_ERROR_SORT,
} RenderBuildResult;

void render_command_list_init(RenderCommandList *list, size_t initial_capacity);
void render_command_list_dispose(RenderCommandList *list);
int render_command_list_add(RenderCommandList *list, const RenderCommand *command);
int render_command_list_sort(RenderCommandList *list);

void renderer_init(Renderer *renderer, const RenderContext *context, size_t initial_capacity);
void renderer_dispose(Renderer *renderer);
RenderBuildResult renderer_build_commands(Renderer *renderer, const ViewModel *view_models, size_t view_model_count,
                                         const GlyphQuad *glyphs, size_t glyph_count);

#ifdef __cplusplus
}
#endif

