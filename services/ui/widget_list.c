#include "ui/widget_list.h"

#include <stdlib.h>

void free_widgets(WidgetArray widgets) {
    if (!widgets.items) return;
    free(widgets.items);
}
