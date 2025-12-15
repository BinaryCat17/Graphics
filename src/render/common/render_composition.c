#include "render_composition.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memory/buffer.h"

MEM_BUFFER_DECLARE(RenderCommandList, 4)

static int compare_sort_keys(const RenderSortKey *a, const RenderSortKey *b)
{
    if (a->layer != b->layer) {
        return (a->layer > b->layer) - (a->layer < b->layer);
    }

    if (a->widget_order != b->widget_order) {
        return (a->widget_order > b->widget_order) - (a->widget_order < b->widget_order);
    }

    if (a->phase != b->phase) {
        return (a->phase > b->phase) - (a->phase < b->phase);
    }

    if (a->ordinal != b->ordinal) {
        return (a->ordinal > b->ordinal) - (a->ordinal < b->ordinal);
    }

    return 0;
}

static void merge(RenderCommand *commands, RenderCommand *scratch, size_t left, size_t mid, size_t right)
{
    size_t i = left;
    size_t j = mid;
    size_t k = left;

    while (i < mid && j < right) {
        if (compare_sort_keys(&commands[i].key, &commands[j].key) <= 0) {
            scratch[k++] = commands[i++];
        } else {
            scratch[k++] = commands[j++];
        }
    }

    while (i < mid) {
        scratch[k++] = commands[i++];
    }

    while (j < right) {
        scratch[k++] = commands[j++];
    }

    for (size_t idx = left; idx < right; ++idx) {
        commands[idx] = scratch[idx];
    }
}

static void stable_sort(RenderCommand *commands, RenderCommand *scratch, size_t left, size_t right)
{
    if (right - left <= 1) {
        return;
    }

    size_t mid = left + (right - left) / 2;
    stable_sort(commands, scratch, left, mid);
    stable_sort(commands, scratch, mid, right);
    merge(commands, scratch, left, mid, right);
}

void render_command_list_init(RenderCommandList *list, size_t initial_capacity)
{
    RenderCommandList_mem_init(list, initial_capacity);
    if (list) {
        list->scratch = NULL;
        list->scratch_capacity = 0;
    }
}

void render_command_list_dispose(RenderCommandList *list)
{
    if (list) {
        free(list->scratch);
        list->scratch = NULL;
        list->scratch_capacity = 0;
    }
    RenderCommandList_mem_dispose(list);
}

int render_command_list_add(RenderCommandList *list, const RenderCommand *command)
{
    if (!list || !command) {
        return -1;
    }

    if (RenderCommandList_mem_reserve(list, list->count + 1) != 0) {
        return -1;
    }

    list->commands[list->count++] = *command;
    return 0;
}

int render_command_list_sort(RenderCommandList *list)
{
    if (!list) {
        return -1;
    }
    if (list->count <= 1) {
        return 0;
    }

    size_t initial_capacity = list->scratch_capacity > 0 ? list->scratch_capacity : list->capacity;
    if (initial_capacity == 0) {
        initial_capacity = list->count;
    }

    if (ensure_capacity((void **)&list->scratch, sizeof(RenderCommand), &list->scratch_capacity, list->count,
                        initial_capacity, MEM_BUFFER_GROWTH_DOUBLE) != 0) {
        fprintf(stderr, "render_command_list_sort: failed to allocate scratch buffer for %zu commands\n", list->count);
        return -1;
    }

    stable_sort(list->commands, list->scratch, 0, list->count);
    return 0;
}

void renderer_init(Renderer *renderer, const RenderContext *context, size_t initial_capacity)
{
    if (!renderer || !context) {
        return;
    }
    renderer->context = *context;
    render_command_list_init(&renderer->command_list, initial_capacity);
}

void renderer_dispose(Renderer *renderer)
{
    if (!renderer) {
        return;
    }
    render_command_list_dispose(&renderer->command_list);
}

static void renderer_reset_commands(Renderer *renderer)
{
    if (renderer) {
        renderer->command_list.count = 0;
    }
}

static RenderBuildResult renderer_fail(Renderer *renderer, RenderBuildResult result, const char *message)
{
    if (message) {
        fprintf(stderr, "%s\n", message);
    }
    renderer_reset_commands(renderer);
    return result;
}

RenderBuildResult renderer_build_commands(Renderer *renderer, const ViewModel *view_models, size_t view_model_count,
                                         const GlyphQuad *glyphs, size_t glyph_count)
{
    if (!renderer) {
        return RENDER_BUILD_ERROR_NULL_RENDERER;
    }
    if ((view_model_count > 0 && !view_models) || (glyph_count > 0 && !glyphs)) {
        return renderer_fail(renderer, RENDER_BUILD_ERROR_INVALID_INPUT,
                             "renderer_build_commands: null input array with non-zero count");
    }

    renderer->command_list.count = 0;
    for (size_t i = 0; i < view_model_count; ++i) {
        LayoutResult layout = layout_resolve(&view_models[i].logical_box, &renderer->context);
        RenderCommand command = (RenderCommand){0};
        command.primitive = RENDER_PRIMITIVE_BACKGROUND;
        command.phase = view_models[i].phase;
        command.key =
            (RenderSortKey){view_models[i].layer, view_models[i].widget_order, view_models[i].phase, view_models[i].ordinal};
        command.has_clip = view_models[i].has_clip || view_models[i].has_device_clip;
        if (view_models[i].has_device_clip) {
            command.clip = view_models[i].clip_device;
        } else if (view_models[i].has_clip) {
            command.clip = layout_resolve(&view_models[i].clip, &renderer->context);
        }
        command.data.background.layout = layout;
        command.data.background.color = view_models[i].color;
        if (render_command_list_add(&renderer->command_list, &command) != 0) {
            char buffer[256];
            snprintf(buffer, sizeof(buffer),
                     "renderer_build_commands: failed to append background command at index %zu (id=%s)", i,
                     view_models[i].id ? view_models[i].id : "<unnamed>");
            return renderer_fail(renderer, RENDER_BUILD_ERROR_BACKGROUND_APPEND, buffer);
        }
    }

    for (size_t i = 0; i < glyph_count; ++i) {
        RenderCommand command = (RenderCommand){0};
        command.primitive = RENDER_PRIMITIVE_GLYPH;
        command.phase = glyphs[i].phase;
        command.key = (RenderSortKey){glyphs[i].layer, glyphs[i].widget_order, glyphs[i].phase, glyphs[i].ordinal};
        command.has_clip = glyphs[i].has_clip || glyphs[i].has_device_clip;
        if (glyphs[i].has_device_clip) {
            command.clip = glyphs[i].clip_device;
        } else if (glyphs[i].has_clip) {
            command.clip = layout_resolve(&glyphs[i].clip, &renderer->context);
        }
        command.data.glyph = glyphs[i];
        if (render_command_list_add(&renderer->command_list, &command) != 0) {
            char buffer[256];
            snprintf(buffer, sizeof(buffer),
                     "renderer_build_commands: failed to append glyph command at index %zu (widget_order=%zu, ordinal=%zu)", i,
                     glyphs[i].widget_order, glyphs[i].ordinal);
            return renderer_fail(renderer, RENDER_BUILD_ERROR_GLYPH_APPEND, buffer);
        }
    }

    if (render_command_list_sort(&renderer->command_list) != 0) {
        return renderer_fail(renderer, RENDER_BUILD_ERROR_SORT,
                             "renderer_build_commands: failed to sort render command list");
    }

    return RENDER_BUILD_OK;
}

