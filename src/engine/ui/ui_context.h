#ifndef UI_CONTEXT_H
#define UI_CONTEXT_H

#include "foundation/math/layout_geometry.h"
#include "foundation/state/state_manager.h"
#include "engine/ui/scene_ui.h"
#include "engine/ui/scroll.h"
#include "engine/ui/model_style.h"
#include "engine/ui/ui_node.h"
#include "engine/ui/layout_tree.h"
#include "engine/ui/widget_list.h"
#include "engine/ui/compositor.h"

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
