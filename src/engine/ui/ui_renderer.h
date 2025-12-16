#ifndef UI_RENDERER_H
#define UI_RENDERER_H

#include "ui_def.h"
#include "foundation/math/coordinate_systems.h"

typedef struct { float r, g, b, a; } Color;

// --- Draw Commands ---
typedef struct UiDrawCmd {
    int type; // 0=Rect, 1=Text
    Rect rect;
    Color color;
    char* text; // For text command
} UiDrawCmd;

typedef struct UiDrawList {
    UiDrawCmd* commands;
    size_t count;
    size_t capacity;
} UiDrawList;

// Main entry point
void ui_render_view(UiView* view, UiDrawList* out_list, float window_w, float window_h);

// Helper to free draw list content
void ui_draw_list_clear(UiDrawList* list);

#endif // UI_RENDERER_H
