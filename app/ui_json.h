#ifndef UI_JSON_H
#define UI_JSON_H

#include <stddef.h>

typedef struct { float x, y, w, h; } Rect;
typedef struct { float r, g, b, a; } Color;
typedef enum { W_PANEL, W_LABEL, W_BUTTON, W_HSLIDER } WidgetType;

typedef struct Widget {
    WidgetType type;
    Rect rect;
    Color color;
    char* text; /* for labels/buttons */
    float minv, maxv, value;
    char* id;
    struct Widget* next;
} Widget;

Widget* parse_ui_json(const char* json_text);
void free_widgets(Widget* widgets);

#endif // UI_JSON_H
