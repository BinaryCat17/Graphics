#ifndef SERVICE_EVENTS_H
#define SERVICE_EVENTS_H

#include "assets/assets.h"
#include "cad/cad_scene.h"
#include "ui/ui_context.h"

#define STATE_COMPONENT_SCENE "scene"
#define STATE_COMPONENT_ASSETS "assets"
#define STATE_COMPONENT_MODEL "model"
#define STATE_COMPONENT_UI "ui"

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

#endif // SERVICE_EVENTS_H
