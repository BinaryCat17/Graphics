#ifndef UI_CONTEXT_H
#define UI_CONTEXT_H

#include "core/math/layout_geometry.h"
#include "core/state/state_manager.h"
#include "services/ui/scene_ui.h"
#include "services/ui/scroll.h"
#include "services/ui/model_style.h"
#include "services/ui/ui_node.h"
#include "services/ui/layout_tree.h"
#include "services/ui/widget_list.h"
#include "services/ui/compositor.h"

// Stores all UI-related state shared between build and runtime.
typedef struct UiContext {
    Style* styles;
    UiNode* ui_root;
    LayoutNode* layout_root;
    DisplayList display_list;
    WidgetArray widgets;
    ScrollContext* scroll;
    Model* model;
    StateManager* state_manager;
    int ui_type_id;

    float base_w;
    float base_h;
    float ui_scale;

    int disposed;
} UiContext;

void ui_context_init(UiContext* ui);
void ui_context_dispose(UiContext* ui);

#endif // UI_CONTEXT_H
