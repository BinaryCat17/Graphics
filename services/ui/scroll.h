#ifndef SCROLL_H
#define SCROLL_H

#include "ui/widget_list.h"

typedef struct ScrollContext ScrollContext;

ScrollContext* scroll_init(Widget* widgets, size_t widget_count);
void scroll_apply_offsets(ScrollContext* ctx, Widget* widgets, size_t widget_count);
void scroll_handle_event(ScrollContext* ctx, Widget* widgets, size_t widget_count, double mouse_x, double mouse_y, double yoff);
int scroll_handle_mouse_button(ScrollContext* ctx, Widget* widgets, size_t widget_count, double mouse_x, double mouse_y, int pressed);
void scroll_handle_cursor(ScrollContext* ctx, Widget* widgets, size_t widget_count, double mouse_x, double mouse_y);
void scroll_rebuild(ScrollContext* ctx, Widget* widgets, size_t widget_count, float offset_scale);
void scroll_free(ScrollContext* ctx);

#endif // SCROLL_H
