#ifndef UI_CONTEXT_H
#define UI_CONTEXT_H

#include "core/layout.h"
#include "ui/scene_ui.h"
#include "ui/scroll.h"
#include "ui/ui_json.h"

// Stores all UI-related state shared between build and runtime.
typedef struct UiContext {
    Style* styles;
    UiNode* ui_root;
    LayoutNode* layout_root;
    WidgetArray widgets;
    ScrollContext* scroll;

    float base_w;
    float base_h;
    float ui_scale;
} UiContext;

void ui_context_init(UiContext* ui);
void ui_context_dispose(UiContext* ui);

#endif // UI_CONTEXT_H
