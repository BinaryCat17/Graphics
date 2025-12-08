#ifndef SCROLL_H
#define SCROLL_H

#include <GLFW/glfw3.h>

#include "ui_json.h"

typedef struct ScrollContext ScrollContext;

ScrollContext* scroll_init(Widget* widgets, size_t widget_count);
void scroll_apply_offsets(ScrollContext* ctx, Widget* widgets, size_t widget_count);
void scroll_handle_event(ScrollContext* ctx, Widget* widgets, size_t widget_count, double mouse_x, double mouse_y, double yoff);
void scroll_rebuild(ScrollContext* ctx, Widget* widgets, size_t widget_count, float offset_scale);
void scroll_free(ScrollContext* ctx);

#endif // SCROLL_H
