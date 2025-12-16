#include "vk_ui_render.h"
#include "vk_utils.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

#include "services/ui/compositor.h"
#include "services/render/backend/common/ui_mesh_builder.h"

enum {
    LAYER_STRIDE = 16,
    Z_LAYER_BORDER = 0,
    Z_LAYER_FILL = 1,
    Z_LAYER_SLIDER_TRACK = 2,
    Z_LAYER_SLIDER_FILL = 3,
    Z_LAYER_SLIDER_KNOB = 4,
    Z_LAYER_TEXT = 5,
    Z_LAYER_SCROLLBAR_TRACK = 14,
    Z_LAYER_SCROLLBAR_THUMB = 15,
};

typedef struct UiRenderNode {
    const Widget *widget;
    size_t widget_index;
    size_t widget_order;
    Rect widget_rect;
    Rect inner_rect;
    float effective_scroll;
    int base_z;
    int scrollbar_z;
    int text_z;
    int has_clip;
    Rect clip_rect;
    LayoutBox logical;
    LayoutResult device;
    LayoutBox clip_box;
    LayoutResult clip_device;
} UiRenderNode;

typedef struct {
    UiRenderNode *items;
    size_t count;
    size_t capacity;
} UiRenderNodeBuffer;

typedef struct {
    Rect rects[UI_CLIP_STACK_MAX];
    Rect combined[UI_CLIP_STACK_MAX];
    size_t depth;
    int has_active;
    Rect active;
} ClipStack;

typedef struct {
    GlyphQuad *items;
    size_t count;
    size_t capacity;
} GlyphQuadArray;

typedef struct {
    ViewModel *items;
    size_t count;
    size_t capacity;
    bool ok;
} ViewModelBuffer;

static bool ui_render_node_buffer_reserve(UiRenderNodeBuffer *buffer, size_t required)
{
    if (!buffer) return false;
    if (required <= buffer->capacity) return true;

    size_t new_capacity = buffer->capacity == 0 ? required : buffer->capacity * 2;
    while (new_capacity < required) new_capacity *= 2;

    UiRenderNode *expanded = realloc(buffer->items, new_capacity * sizeof(UiRenderNode));
    if (!expanded) {
        return false;
    }

    memset(expanded + buffer->capacity, 0, (new_capacity - buffer->capacity) * sizeof(UiRenderNode));
    buffer->items = expanded;
    buffer->capacity = new_capacity;
    return true;
}

static bool ui_render_node_buffer_push(UiRenderNodeBuffer *buffer, const UiRenderNode *node)
{
    if (!buffer || !node) return false;
    if (!ui_render_node_buffer_reserve(buffer, buffer->count + 1)) return false;
    buffer->items[buffer->count++] = *node;
    return true;
}

static float snap_to_pixel(float value) {
    return floorf(value + 0.5f);
}

static int apply_clip_rect_to_bounds(const Rect *clip, const Rect *input, Rect *out) {
    if (!input || !out) return 0;
    *out = *input;
    if (!clip) return 1;

    float clip_x0 = ceilf(clip->x);
    float clip_y0 = ceilf(clip->y);
    float clip_x1 = floorf(clip->x + clip->w);
    float clip_y1 = floorf(clip->y + clip->h);

    float x0 = fmaxf(input->x, clip_x0);
    float y0 = fmaxf(input->y, clip_y0);
    float x1 = fminf(input->x + input->w, clip_x1);
    float y1 = fminf(input->y + input->h, clip_y1);
    if (x1 <= x0 || y1 <= y0) return 0;
    out->x = x0;
    out->y = y0;
    out->w = x1 - x0;
    out->h = y1 - y0;
    return 1;
}

static void clip_stack_pop(ClipStack* stack) {
    if (!stack || stack->depth == 0) return;
    stack->depth--;
    if (stack->depth == 0) {
        stack->has_active = 0;
        stack->active = (Rect){0};
    } else {
        stack->has_active = 1;
        stack->active = stack->combined[stack->depth - 1];
    }
}

static void clip_stack_push(ClipStack* stack, Rect clip) {
    if (!stack || stack->depth >= UI_CLIP_STACK_MAX) return;
    Rect combined = clip;
    if (stack->has_active) {
        if (!apply_clip_rect_to_bounds(&stack->active, &clip, &combined)) {
            combined = (Rect){0};
        }
    }
    stack->rects[stack->depth] = clip;
    stack->combined[stack->depth] = combined;
    stack->depth++;
    stack->has_active = 1;
    stack->active = combined;
}

static const Rect* clip_stack_active(const ClipStack* stack) {
    if (!stack || !stack->has_active) return NULL;
    return &stack->active;
}

static const Rect* node_clip_rect(const UiRenderNode *node) {
    return (node && node->has_clip) ? &node->clip_rect : NULL;
}

static const LayoutResult* node_clip_device(const UiRenderNode *node) {
    return (node && node->has_clip) ? &node->clip_device : NULL;
}

static bool normalize_display_items(VulkanRendererState* state, const DisplayList *list, UiRenderNodeBuffer *buffer)
{
    if (!list || !buffer) return false;

    ClipStack clip_stack = {0};
    for (size_t i = 0; i < list->count; ++i) {
        const DisplayItem *item = &list->items[i];
        const Widget *widget = item->widget;
        if (!widget) continue;

        for (size_t p = 0; p < item->clip_pop; ++p) clip_stack_pop(&clip_stack);
        for (size_t p = 0; p < item->clip_push && p < UI_CLIP_STACK_MAX; ++p) clip_stack_push(&clip_stack, item->push_rects[p]);

        UiRenderNode node = {0};
        node.widget = widget;
        node.widget_index = (size_t)(widget - state->widgets.items);
        node.widget_order = state->widgets.count > 0 ? (state->widgets.count - 1 - node.widget_index) : 0;
        node.base_z = widget->z_index * LAYER_STRIDE;
        node.scrollbar_z = (widget->z_index + UI_Z_ORDER_SCALE) * LAYER_STRIDE;
        node.text_z = node.base_z + Z_LAYER_TEXT;

        float scroll_offset = widget->type == W_SCROLLBAR ? 0.0f : widget->scroll_offset;
        float snapped_scroll_pixels = -snap_to_pixel(scroll_offset * state->transformer.dpi_scale);
        node.effective_scroll = snapped_scroll_pixels / state->transformer.dpi_scale;

        node.widget_rect = (Rect){widget->rect.x, widget->rect.y + node.effective_scroll, widget->rect.w, widget->rect.h};
        node.inner_rect = node.widget_rect;

        const Rect *active_clip = clip_stack_active(&clip_stack);
        if (active_clip) {
            node.has_clip = 1;
            node.clip_rect = *active_clip;
        }

        if (!ui_render_node_buffer_push(buffer, &node)) {
            return false;
        }
    }

    return true;
}

static void resolve_node_layouts(UiRenderNode *nodes, size_t count, const RenderContext *context)
{
    if (!nodes || !context) return;

    for (size_t i = 0; i < count; ++i) {
        UiRenderNode *node = &nodes[i];
        const Widget *widget = node->widget;
        if (!widget) continue;

        node->inner_rect = node->widget_rect;
        if (widget->border_thickness > 0.0f) {
            float b = widget->border_thickness;
            node->inner_rect.x += b;
            node->inner_rect.y += b;
            node->inner_rect.w -= b * 2.0f;
            node->inner_rect.h -= b * 2.0f;
            if (node->inner_rect.w < 0.0f) node->inner_rect.w = 0.0f;
            if (node->inner_rect.h < 0.0f) node->inner_rect.h = 0.0f;
        }

        node->logical = (LayoutBox){ {node->widget_rect.x, node->widget_rect.y}, {node->widget_rect.w, node->widget_rect.h} };
        node->device = layout_resolve(&node->logical, context);

        if (node->has_clip) {
            node->clip_box = (LayoutBox){ {node->clip_rect.x, node->clip_rect.y}, {node->clip_rect.w, node->clip_rect.h} };
            node->clip_device = layout_resolve(&node->clip_box, context);
        }
    }
}

static int utf8_decode(const char* s, int* out_advance) {
    unsigned char c = (unsigned char)*s;
    if (c < 0x80) { *out_advance = 1; return c; }
    if ((c >> 5) == 0x6) { *out_advance = 2; return ((int)(c & 0x1F) << 6) | ((int)(s[1] & 0x3F)); }
    if ((c >> 4) == 0xE) { *out_advance = 3; return ((int)(c & 0x0F) << 12) | (((int)s[1] & 0x3F) << 6) | ((int)(s[2] & 0x3F)); }
    if ((c >> 3) == 0x1E) { *out_advance = 4; return ((int)(c & 0x07) << 18) | (((int)s[1] & 0x3F) << 12) |
                                        (((int)s[2] & 0x3F) << 6) | ((int)(s[3] & 0x3F)); }
    *out_advance = 1;
    return '?';
}

static const Glyph* get_glyph(VulkanRendererState* state, int codepoint) {
    if (codepoint >= 0 && codepoint < GLYPH_CAPACITY && state->glyph_valid[codepoint]) {
        return &state->glyphs[codepoint];
    }
    if (state->glyph_valid['?']) return &state->glyphs['?'];
    return NULL;
}

static void apply_active_clip_to_view_model(const Rect *clip, const LayoutResult *clip_device, ViewModel *vm) {
    if (!clip || !vm) return;
    vm->has_clip = 1;
    vm->has_device_clip = clip_device != NULL;
    vm->clip.origin = (Vec2){clip->x, clip->y};
    vm->clip.size = (Vec2){clip->w, clip->h};
    if (clip_device) {
        vm->clip_device = *clip_device;
    }
}

static bool view_model_buffer_reserve(ViewModelBuffer *buffer, size_t required)
{
    if (!buffer || !buffer->ok) return false;
    if (required <= buffer->capacity) return true;

    size_t new_capacity = buffer->capacity == 0 ? required : buffer->capacity * 2;
    while (new_capacity < required) new_capacity *= 2;

    ViewModel *expanded = realloc(buffer->items, new_capacity * sizeof(ViewModel));
    if (!expanded) {
        buffer->ok = false;
        return false;
    }

    memset(expanded + buffer->capacity, 0, (new_capacity - buffer->capacity) * sizeof(ViewModel));
    buffer->items = expanded;
    buffer->capacity = new_capacity;
    return true;
}

static bool view_model_buffer_push(ViewModelBuffer *buffer, const ViewModel *vm)
{
    if (!buffer || !vm) return false;
    if (!view_model_buffer_reserve(buffer, buffer->count + 1)) return false;
    buffer->items[buffer->count++] = *vm;
    return true;
}

static bool append_rect_view_model(const UiRenderNode *node, const Rect *rect, int layer, RenderPhase phase, Color color,
                                   const Rect *clip_override, const LayoutResult *clip_device_override,
                                   ViewModelBuffer *buffer, size_t *ordinal)
{
    const Rect *clip_rect = clip_override ? clip_override : node_clip_rect(node);
    const LayoutResult *clip_device = clip_device_override ? clip_device_override : node_clip_device(node);

    Rect clipped;
    if (!apply_clip_rect_to_bounds(clip_rect, rect, &clipped)) {
        return true;
    }

    ViewModel vm = {
        .id = node->widget->id,
        .logical_box = { {clipped.x, clipped.y}, {clipped.w, clipped.h} },
        .layer = layer,
        .phase = phase,
        .widget_order = node->widget_order,
        .ordinal = (*ordinal)++,
        .color = color,
    };
    apply_active_clip_to_view_model(clip_rect, clip_device, &vm);
    return view_model_buffer_push(buffer, &vm);
}

static bool glyph_quad_array_reserve(GlyphQuadArray *arr, size_t required)
{
    if (required <= arr->capacity) {
        return true;
    }

    size_t new_capacity = arr->capacity == 0 ? required : arr->capacity * 2;
    while (new_capacity < required) {
        new_capacity *= 2;
    }

    GlyphQuad *expanded = realloc(arr->items, new_capacity * sizeof(GlyphQuad));
    if (!expanded) {
        return false;
    }

    arr->items = expanded;
    arr->capacity = new_capacity;
    return true;
}

static bool append_glyph_quad(const UiRenderNode *node, const Rect *glyph_rect, Vec2 uv0, Vec2 uv1, Color color,
                              GlyphQuadArray *glyph_quads, size_t *ordinal)
{
    const Rect *clip_rect = node_clip_rect(node);
    const LayoutResult *clip_device = node_clip_device(node);

    Rect clipped_rect;
    if (!apply_clip_rect_to_bounds(clip_rect, glyph_rect, &clipped_rect)) {
        return true;
    }

    if (!glyph_quad_array_reserve(glyph_quads, glyph_quads->count + 1)) {
        return false;
    }

    GlyphQuad *quad = &glyph_quads->items[glyph_quads->count++];
    quad->min = (Vec2){clipped_rect.x, clipped_rect.y};
    quad->max = (Vec2){clipped_rect.x + clipped_rect.w, clipped_rect.y + clipped_rect.h};
    quad->uv0 = uv0;
    quad->uv1 = uv1;
    quad->color = color;
    quad->widget_id = node->widget->id;
    quad->widget_order = node->widget_order;
    quad->layer = node->text_z;
    quad->phase = RENDER_PHASE_CONTENT;
    quad->ordinal = (*ordinal)++;
    quad->has_clip = clip_rect != NULL;
    quad->has_device_clip = clip_device != NULL;
    if (clip_rect) {
        quad->clip = (LayoutBox){ {clip_rect->x, clip_rect->y}, {clip_rect->w, clip_rect->h} };
    }
    if (clip_device) {
        quad->clip_device = *clip_device;
    }
    return true;
}

static bool emit_border_view_models(const UiRenderNode *node, ViewModelBuffer *buffer, size_t *ordinal)
{
    const Widget *widget = node->widget;
    if (widget->border_thickness <= 0.0f) return true;

    Rect borders[4] = {
        { node->widget_rect.x, node->widget_rect.y, node->widget_rect.w, widget->border_thickness },
        { node->widget_rect.x, node->widget_rect.y + node->widget_rect.h - widget->border_thickness, node->widget_rect.w,
          widget->border_thickness },
        { node->widget_rect.x, node->widget_rect.y + widget->border_thickness, widget->border_thickness,
          node->widget_rect.h - widget->border_thickness * 2.0f },
        { node->widget_rect.x + node->widget_rect.w - widget->border_thickness, node->widget_rect.y + widget->border_thickness,
          widget->border_thickness, node->widget_rect.h - widget->border_thickness * 2.0f },
    };

    Rect border_clip = node->clip_rect;
    const Rect *clip_bounds = node_clip_rect(node);
    const LayoutResult *border_clip_ptr = NULL;
    if (clip_bounds) {
        border_clip = (
            Rect){
            clip_bounds->x - widget->border_thickness,
            clip_bounds->y - widget->border_thickness,
            clip_bounds->w + widget->border_thickness * 2.0f,
            clip_bounds->h + widget->border_thickness * 2.0f,
        };
        border_clip_ptr = node_clip_device(node);
    }

    for (size_t edge = 0; edge < 4; ++edge) {
        if (borders[edge].w <= 0.0f || borders[edge].h <= 0.0f) continue;
        if (!append_rect_view_model(node, &borders[edge], node->base_z + Z_LAYER_BORDER, RENDER_PHASE_BACKGROUND, 
                                    widget->border_color, clip_bounds ? &border_clip : NULL, border_clip_ptr, buffer, ordinal)) {
            return false;
        }
    }

    return true;
}

static bool emit_scrollbar_thumb(const UiRenderNode *node, const Rect *track_rect, ViewModelBuffer *buffer, size_t *ordinal)
{
    const Widget *widget = node->widget;
    const Rect *clip_rect = node_clip_rect(node);
    
    // Updated to access nested scroll data
    if (!widget->data.scroll.enabled || !widget->data.scroll.show || widget->data.scroll.viewport_size <= 0.0f ||
        widget->data.scroll.content_size <= widget->data.scroll.viewport_size + 1.0f) {
        return true;
    }

    float thumb_ratio = widget->data.scroll.viewport_size / widget->data.scroll.content_size;
    float thumb_h = fmaxf(track_rect->h * thumb_ratio, 12.0f);
    float max_offset = widget->data.scroll.content_size - widget->data.scroll.viewport_size;
    float clamped_offset = widget->scroll_offset;
    if (clamped_offset < 0.0f) clamped_offset = 0.0f;
    if (clamped_offset > max_offset) clamped_offset = max_offset;
    float offset_t = (max_offset != 0.0f) ? (clamped_offset / max_offset) : 0.0f;
    float thumb_y = track_rect->y + offset_t * (track_rect->h - thumb_h);

    Rect thumb_rect = { track_rect->x, thumb_y, track_rect->w, thumb_h };
    return append_rect_view_model(node, &thumb_rect, node->scrollbar_z + Z_LAYER_SCROLLBAR_THUMB, RENDER_PHASE_BACKGROUND, 
                                  widget->data.scroll.thumb_color, clip_rect, node_clip_device(node), buffer, ordinal);
}

static bool emit_widget_fill(const UiRenderNode *node, ViewModelBuffer *buffer, size_t *ordinal)
{
    const Widget *widget = node->widget;
    const Rect *clip_rect = node_clip_rect(node);
    Rect fill_rect = node->inner_rect;
    int layer = node->base_z + Z_LAYER_FILL;
    Color fill_color = widget->color;

    if (widget->type == W_SCROLLBAR) {
        float track_w = widget->data.scroll.width > 0.0f ? widget->data.scroll.width : fmaxf(4.0f, node->inner_rect.w * 0.02f);
        float track_h = node->inner_rect.h - widget->padding * 2.0f;
        float track_x = node->inner_rect.x + node->inner_rect.w - track_w - widget->padding * 0.5f;
        float track_y = node->inner_rect.y + widget->padding;
        fill_rect = (Rect){ track_x, track_y, track_w, track_h };
        layer = node->scrollbar_z + Z_LAYER_SCROLLBAR_TRACK;
        fill_color = widget->data.scroll.track_color;
    }

    if (!append_rect_view_model(node, &fill_rect, layer, RENDER_PHASE_BACKGROUND, fill_color, clip_rect, node_clip_device(node),
                                buffer, ordinal)) {
        return false;
    }

    if (widget->type == W_SCROLLBAR) {
        if (!emit_scrollbar_thumb(node, &fill_rect, buffer, ordinal)) return false;
    }

    return true;
}

static bool emit_slider(const UiRenderNode *node, ViewModelBuffer *buffer, size_t *ordinal)
{
    const Widget *widget = node->widget;
    const Rect *clip_rect = node_clip_rect(node);

    float track_height = fmaxf(node->inner_rect.h * 0.35f, 6.0f);
    float track_y = node->inner_rect.y + (node->inner_rect.h - track_height) * 0.5f;
    float track_x = node->inner_rect.x;
    float track_w = node->inner_rect.w;
    float denom = widget->data.slider.min - widget->data.slider.max;
    float t = denom != 0.0f ? (widget->data.slider.value - widget->data.slider.min) / denom : 0.0f;
    if (t < 0.0f) t = 0.0f; else if (t > 1.0f) t = 1.0f;

    Color track_color = widget->color;
    track_color.a *= 0.35f;
    Rect track_rect = { track_x, track_y, track_w, track_height };
    if (!append_rect_view_model(node, &track_rect, node->base_z + Z_LAYER_SLIDER_TRACK, RENDER_PHASE_BACKGROUND, track_color, 
                                clip_rect, node_clip_device(node), buffer, ordinal)) {
        return false;
    }

    float fill_w = track_w * t;
    Rect fill_rect = { track_x, track_y, fill_w, track_height };
    if (!append_rect_view_model(node, &fill_rect, node->base_z + Z_LAYER_SLIDER_FILL, RENDER_PHASE_BACKGROUND, widget->color, 
                                clip_rect, node_clip_device(node), buffer, ordinal)) {
        return false;
    }

    float knob_w = fmaxf(track_height, node->inner_rect.h * 0.3f);
    float knob_x = track_x + fill_w - knob_w * 0.5f;
    if (knob_x < track_x) knob_x = track_x;
    float knob_max = track_x + track_w - knob_w;
    if (knob_x > knob_max) knob_x = knob_max;
    float knob_h = track_height * 1.5f;
    float knob_y = track_y + (track_height - knob_h) * 0.5f;
    Color knob_color = widget->data.slider.knob_color;
    if (knob_color.a <= 0.0f) knob_color = (Color){1.0f, 1.0f, 1.0f, 1.0f};
    Rect knob_rect = { knob_x, knob_y, knob_w, knob_h };
    return append_rect_view_model(node, &knob_rect, node->base_z + Z_LAYER_SLIDER_KNOB, RENDER_PHASE_BACKGROUND, knob_color, 
                                  clip_rect, node_clip_device(node), buffer, ordinal);
}

static bool emit_text_glyphs(VulkanRendererState* state, const UiRenderNode *node, GlyphQuadArray *glyph_quads, size_t *ordinal)
{
    const Widget *widget = node->widget;
    const char* text_ptr = NULL;
    Color text_color = {0};

    // Determine text and color based on widget type
    if (widget->type == W_LABEL || widget->type == W_BUTTON) {
        text_ptr = widget->data.label.text;
        text_color = widget->data.label.color;
    } else if (widget->type == W_CHECKBOX) {
        text_ptr = widget->data.checkbox.text;
        text_color = widget->data.checkbox.color;
    }
    
    if (!text_ptr || !*text_ptr) return true;

    float pen_x = widget->rect.x + widget->padding;
    float pen_y = widget->rect.y + node->effective_scroll + widget->padding + (float)state->ascent;

    for (const char *c = text_ptr; c && *c; ) {
        int adv = 0;
        int codepoint = utf8_decode(c, &adv);
        if (adv <= 0) break;
        if (codepoint < 32) { c += adv; continue; }

        const Glyph *g = get_glyph(state, codepoint);
        if (!g) { c += adv; continue; }
        float snapped_pen_x = floorf(pen_x + 0.5f);
        float snapped_pen_y = floorf(pen_y + 0.5f);
        float x0 = snapped_pen_x + g->xoff;
        float y0 = snapped_pen_y + g->yoff;
        Rect glyph_rect = { x0, y0, g->w, g->h };

        if (!append_glyph_quad(node, &glyph_rect, (Vec2){g->u0, g->v0}, (Vec2){g->u1, g->v1}, text_color,
                               glyph_quads, ordinal)) {
            return false;
        }

        pen_x += g->advance;
        c += adv;
    }

    return true;
}

static bool build_render_items_from_nodes(VulkanRendererState* state, const UiRenderNode *nodes, size_t node_count, ViewModelBuffer *view_models,
                                          GlyphQuadArray *glyph_quads, size_t *widget_ordinals)
{
    if (!nodes || !view_models || !glyph_quads || !widget_ordinals) return false;

    for (size_t i = 0; i < node_count; ++i) {
        const UiRenderNode *node = &nodes[i];
        const Widget *widget = node->widget;
        if (!widget) continue;

        size_t *ordinal = &widget_ordinals[node->widget_index];

        if (!emit_border_view_models(node, view_models, ordinal)) return false;

        if (widget->type == W_HSLIDER) {
            if (!emit_slider(node, view_models, ordinal)) return false;
        } else {
            if (!emit_widget_fill(node, view_models, ordinal)) return false;
        }

        // Generic text check helper
        int has_text = 0;
        if (widget->type == W_LABEL || widget->type == W_BUTTON) {
            has_text = (widget->data.label.text && *widget->data.label.text);
        } else if (widget->type == W_CHECKBOX) {
            has_text = (widget->data.checkbox.text && *widget->data.checkbox.text);
        }

        if (has_text) {
            if (!emit_text_glyphs(state, node, glyph_quads, ordinal)) return false;
        }
    }

    return true;
}

static bool ensure_cpu_vertex_capacity(FrameCpuArena *cpu, size_t background_vertices, size_t text_vertices, size_t final_vertices) {
    UiVertex *new_background = cpu->background_vertices;
    UiTextVertex *new_text = cpu->text_vertices;
    Vtx *new_final = cpu->vertices;

    if (background_vertices > cpu->background_capacity) {
        size_t cap = cpu->background_capacity == 0 ? background_vertices : cpu->background_capacity * 2;
        while (cap < background_vertices) cap *= 2;
        new_background = realloc(cpu->background_vertices, cap * sizeof(UiVertex));
        if (!new_background) return false;
        cpu->background_capacity = cap;
    }

    if (text_vertices > cpu->text_capacity) {
        size_t cap = cpu->text_capacity == 0 ? text_vertices : cpu->text_capacity * 2;
        while (cap < text_vertices) cap *= 2;
        new_text = realloc(cpu->text_vertices, cap * sizeof(UiTextVertex));
        if (!new_text) return false;
        cpu->text_capacity = cap;
    }

    if (final_vertices > cpu->vertex_capacity) {
        size_t cap = cpu->vertex_capacity == 0 ? final_vertices : cpu->vertex_capacity * 2;
        while (cap < final_vertices) cap *= 2;
        new_final = realloc(cpu->vertices, cap * sizeof(Vtx));
        if (!new_final) return false;
        cpu->vertex_capacity = cap;
    }

    cpu->background_vertices = new_background;
    cpu->text_vertices = new_text;
    cpu->vertices = new_final;
    return true;
}

static bool ensure_vtx_capacity(FrameCpuArena *cpu, size_t required)
{
    return ensure_cpu_vertex_capacity(cpu, cpu->background_capacity, cpu->text_capacity, required);
}

static bool is_cmd_clipped(const RenderContext *ctx, const RenderCommand *cmd) {
    if (!cmd->has_clip) return false;

    Vec2 min, max;
    if (cmd->primitive == RENDER_PRIMITIVE_BACKGROUND) {
        min = cmd->data.background.layout.device.origin;
        max = (Vec2){min.x + cmd->data.background.layout.device.size.x, min.y + cmd->data.background.layout.device.size.y};
    } else {
        Vec2 device_min = coordinate_logical_to_screen(&ctx->coordinates, cmd->data.glyph.min);
        Vec2 device_max = coordinate_logical_to_screen(&ctx->coordinates, cmd->data.glyph.max);
        min = device_min;
        max = device_max;
    }

    float cx0 = cmd->clip.device.origin.x;
    float cy0 = cmd->clip.device.origin.y;
    float cx1 = cx0 + cmd->clip.device.size.x;
    float cy1 = cy0 + cmd->clip.device.size.y;

    float x0 = fmaxf(min.x, cx0);
    float y0 = fmaxf(min.y, cy0);
    float x1 = fminf(max.x, cx1);
    float y1 = fminf(max.y, cy1);

    return (x1 <= x0 || y1 <= y0);
}

bool vk_build_vertices_from_widgets(VulkanRendererState* state, FrameResources *frame) {
    if (!frame) return false;
    frame->vertex_count = 0;

    if (state->display_list.count == 0 || state->swapchain_extent.width == 0 || state->swapchain_extent.height == 0) {
        return true;
    }

    Mat4 projection = mat4_identity();
    CoordinateSystem2D transformer = state->transformer;
    transformer.viewport_size = (Vec2){(float)state->swapchain_extent.width, (float)state->swapchain_extent.height};

    RenderContext context;
    render_context_init(&context, &transformer, &projection);

    UiRenderNodeBuffer node_buffer = {0};
    ViewModelBuffer view_models = {.ok = true};
    GlyphQuadArray glyph_quads = {0};
    size_t *widget_ordinals = state->widgets.count > 0 ? calloc(state->widgets.count, sizeof(size_t)) : NULL;

    if (state->widgets.count > 0 && !widget_ordinals) {
        return false;
    }

    if (!normalize_display_items(state, &state->display_list, &node_buffer)) {
        free(widget_ordinals);
        return false;
    }

    resolve_node_layouts(node_buffer.items, node_buffer.count, &context);

    if (!build_render_items_from_nodes(state, node_buffer.items, node_buffer.count, &view_models, &glyph_quads, widget_ordinals) ||
        !view_models.ok) {
        free(widget_ordinals);
        free(node_buffer.items);
        free(view_models.items);
        free(glyph_quads.items);
        return false;
    }

    UiVertexBuffer background_buffer;
    UiTextVertexBuffer text_buffer;
    ui_vertex_buffer_init(&background_buffer, view_models.count * 6);
    ui_text_vertex_buffer_init(&text_buffer, glyph_quads.count * 6);

    Renderer renderer;
    renderer_init(&renderer, &context, view_models.count + glyph_quads.count);
    RenderBuildResult build_res = renderer_fill_vertices(&renderer, view_models.items, view_models.count, glyph_quads.items,
                                                        glyph_quads.count, &background_buffer, &text_buffer);
    bool success = build_res == RENDER_BUILD_OK;
    if (!success) {
        fprintf(stderr, "renderer_fill_vertices failed: %d\n", (int)build_res);
    }

    size_t background_quad_idx = 0, text_quad_idx = 0;
    size_t primitive_count = renderer.command_list.count;
    int min_layer = 0, max_layer = 0;
    if (primitive_count > 0) {
        min_layer = max_layer = renderer.command_list.commands[0].key.layer;
        for (size_t c = 0; c < primitive_count; ++c) {
            int layer = renderer.command_list.commands[c].key.layer;
            if (layer < min_layer) min_layer = layer;
            if (layer > max_layer) max_layer = layer;
        }
    }
    Primitive *primitives = primitive_count > 0 ? calloc(primitive_count, sizeof(Primitive)) : NULL;
    if (primitives && success) {
        size_t actual_primitive_count = 0;
        for (size_t c = 0; c < renderer.command_list.count; ++c) {
            const RenderCommand *cmd = &renderer.command_list.commands[c];
            
            if (is_cmd_clipped(&context, cmd)) {
                continue;
            }

            Primitive *p = &primitives[actual_primitive_count++];
            p->order = cmd->key.ordinal;
            p->z = (float)cmd->key.layer;

            if (cmd->primitive == RENDER_PRIMITIVE_BACKGROUND) {
                UiVertex *base = &background_buffer.vertices[background_quad_idx++ * 6];
                for (int i = 0; i < 6; ++i) {
                    UiVertex v = base[i];
                    p->vertices[i] = (Vtx){v.position[0], v.position[1], 0.0f, 0.0f, 0.0f, 0.0f, v.color.r, v.color.g, v.color.b, v.color.a};
                }
            } else {
                UiTextVertex *base = &text_buffer.vertices[text_quad_idx++ * 6];
                for (int i = 0; i < 6; ++i) {
                    UiTextVertex v = base[i];
                    p->vertices[i] = (Vtx){v.position[0], v.position[1], 0.0f, v.uv[0], v.uv[1], 1.0f, v.color.r, v.color.g, v.color.b, v.color.a};
                }
            }
        }
        
        primitive_count = actual_primitive_count;
        size_t total_vertices = primitive_count * 6;
        if (ensure_vtx_capacity(&frame->cpu, total_vertices)) {
            size_t cursor = 0;
            float layer_span = (float)(max_layer - min_layer + 1);
            float step = (primitive_count > 0 && layer_span > 0.0f) ? (1.0f / ((float)primitive_count + 1.0f)) / layer_span : 0.0f;
            for (size_t i = 0; i < primitive_count; ++i) {
                float layer_offset = (float)((int)primitives[i].z - min_layer);
                float depth = 1.0f - (layer_offset + 0.5f) / layer_span - step * (float)i;
                if (depth < 0.0f) depth = 0.0f;
                if (depth > 1.0f) depth = 1.0f;
                for (int v = 0; v < 6; ++v) {
                    Vtx vertex = primitives[i].vertices[v];
                    vertex.pz = depth;
                    frame->cpu.vertices[cursor++] = vertex;
                }
            }
            frame->vertex_count = cursor;
        } else {
            frame->vertex_count = 0;
            success = false;
        }
    } else {
        frame->vertex_count = 0;
        if (primitive_count > 0) {
            success = false;
        }
    }

    ui_vertex_buffer_dispose(&background_buffer);
    ui_text_vertex_buffer_dispose(&text_buffer);
    renderer_dispose(&renderer);
    free(glyph_quads.items);
    free(primitives);
    free(view_models.items);
    free(widget_ordinals);
    free(node_buffer.items);

    return success;
}