#include "ui_renderer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// --- Draw List Utils ---

static void push_cmd(UiDrawList* list, UiDrawCmd cmd) {
    if (list->count == list->capacity) {
        size_t new_cap = list->capacity == 0 ? 64 : list->capacity * 2;
        UiDrawCmd* new_cmds = (UiDrawCmd*)realloc(list->commands, new_cap * sizeof(UiDrawCmd));
        if (!new_cmds) return;
        list->commands = new_cmds;
        list->capacity = new_cap;
    }
    list->commands[list->count++] = cmd;
}

void ui_draw_list_clear(UiDrawList* list) {
    if (!list) return;
    for (size_t i = 0; i < list->count; ++i) {
        if (list->commands[i].type == 1) { // Text
            free(list->commands[i].text);
        }
    }
    free(list->commands);
    list->commands = NULL;
    list->count = 0;
    list->capacity = 0;
}

// --- Layout Pass (Simple Flexbox-like) ---

static void layout_node(UiView* view, Rect available) {
    if (!view || !view->def) return;

    // Determine size
    float w = view->def->width < 0 ? available.w : view->def->width;
    float h = view->def->height < 0 ? 30.0f : view->def->height; // Default height for auto
    
    // Auto height for container based on children
    if (view->def->height < 0 && view->child_count > 0 && view->def->layout == UI_LAYOUT_COLUMN) {
        h = view->def->padding * 2;
        for (size_t i = 0; i < view->child_count; ++i) {
            h += (view->children[i]->def->height < 0 ? 30.0f : view->children[i]->def->height);
            h += view->def->spacing;
        }
    }

    view->rect.x = available.x;
    view->rect.y = available.y;
    view->rect.w = w;
    view->rect.h = h;

    // Layout Children
    Rect content = {
        available.x + view->def->padding,
        available.y + view->def->padding,
        w - view->def->padding * 2,
        h - view->def->padding * 2
    };

    float cursor_x = content.x;
    float cursor_y = content.y;

    for (size_t i = 0; i < view->child_count; ++i) {
        UiView* child = view->children[i];
        Rect child_avail = { cursor_x, cursor_y, content.w, content.h - (cursor_y - content.y) };
        
        layout_node(child, child_avail);

        if (view->def->layout == UI_LAYOUT_COLUMN) {
            cursor_y += child->rect.h + view->def->spacing;
        } else if (view->def->layout == UI_LAYOUT_ROW) {
            cursor_x += child->rect.w + view->def->spacing;
        }
    }
}

// --- Render Pass ---

static void render_node(UiView* view, UiDrawList* list) {
    if (!view || !view->def) return;

    // Background
    UiDrawCmd bg_cmd = {0};
    bg_cmd.type = 0; // Rect
    bg_cmd.rect = view->rect;
    
    // Simple styling based on type
    if (view->def->type == UI_NODE_PANEL) bg_cmd.color = (Color){0.1f, 0.1f, 0.1f, 0.9f};
    else if (view->def->type == UI_NODE_BUTTON) bg_cmd.color = (Color){0.3f, 0.4f, 0.6f, 1.0f};
    else if (view->def->type == UI_NODE_SLIDER) bg_cmd.color = (Color){0.2f, 0.2f, 0.2f, 1.0f};
    else bg_cmd.color = (Color){0,0,0,0}; // Transparent

    if (bg_cmd.color.a > 0) push_cmd(list, bg_cmd);

    // Text
    const char* text = view->cached_text ? view->cached_text : view->def->text;
    if (text) {
        UiDrawCmd text_cmd = {0};
        text_cmd.type = 1; // Text
        text_cmd.rect = view->rect;
        text_cmd.rect.x += 5; // Padding
        text_cmd.rect.y += 5;
        text_cmd.color = (Color){1.0f, 1.0f, 1.0f, 1.0f};
        text_cmd.text = strdup(text); // Draw list owns the string
        push_cmd(list, text_cmd);
    }
    
    // Slider Knob
    if (view->def->type == UI_NODE_SLIDER) {
         // TBD: Calculate knob position based on cached_value
         // For now just draw a bar
    }

    // Children
    for (size_t i = 0; i < view->child_count; ++i) {
        render_node(view->children[i], list);
    }
}

void ui_render_view(UiView* view, UiDrawList* out_list, float window_w, float window_h) {
    if (!view || !out_list) return;

    // 1. Layout
    Rect screen_rect = {0, 0, window_w, window_h};
    layout_node(view, screen_rect);

    // 2. Generate Commands
    render_node(view, out_list);
}
