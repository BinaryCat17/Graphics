#ifndef SERVICE_EVENTS_H
#define SERVICE_EVENTS_H

#include <stdbool.h>

#include "assets/assets.h"
#include "cad/cad_scene.h"
#include "render/render_context.h"
#include "ui/ui_context.h"

#define STATE_COMPONENT_SCENE "scene"
#define STATE_COMPONENT_ASSETS "assets"
#define STATE_COMPONENT_MODEL "model"
#define STATE_COMPONENT_UI "ui"
#define STATE_COMPONENT_RENDER_READY "render-ready"

typedef struct SceneComponent {
    Scene* scene;
    const char* path;
} SceneComponent;

typedef struct AssetsComponent {
    Assets* assets;
} AssetsComponent;

typedef struct ModelComponent {
    Model* model;
} ModelComponent;

typedef struct UiRuntimeComponent {
    UiContext* ui;
    WidgetArray widgets;
} UiRuntimeComponent;

typedef struct RenderReadyComponent {
    RenderRuntimeContext* render;
    const Assets* assets;
    UiContext* ui;
    WidgetArray widgets;
    Model* model;
    bool ready;
} RenderReadyComponent;

#endif // SERVICE_EVENTS_H
