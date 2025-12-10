#ifndef APP_SERVICES_H
#define APP_SERVICES_H

#include "core/context.h"
#include "render/render_context.h"
#include "ui/ui_context.h"

// Central structure passed between application layers.
typedef struct AppServices {
    CoreContext core;
    UiContext ui;
    RenderRuntimeContext render;
} AppServices;

#endif // APP_SERVICES_H
