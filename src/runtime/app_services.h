#ifndef APP_SERVICES_H
#define APP_SERVICES_H

#include <GLFW/glfw3.h>

#include "Graphics.h"
#include "assets/assets.h"
#include "cad_scene.h"
#include "state_manager.h"
#include "module_yaml_loader.h"
#include "ui/scene_ui.h"
#include "ui/scroll.h"

// Central structure passed between application layers.
typedef struct AppServices {
    StateManager state_manager;
    ModuleSchema ui_schema;
    ModuleSchema global_schema;

    Scene scene;
    Assets assets;
    Model* model;

    Style* styles;
    UiNode* ui_root;
    LayoutNode* layout_root;
    WidgetArray widgets;
    ScrollContext* scroll;

    CoordinateTransformer transformer;

    GLFWwindow* window;
    float base_w;
    float base_h;
    float ui_scale;
} AppServices;

#endif // APP_SERVICES_H
