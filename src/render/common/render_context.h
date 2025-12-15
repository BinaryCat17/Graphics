#ifndef RENDER_CONTEXT_H
#define RENDER_CONTEXT_H

#include "platform/platform.h"
#include "coordinate_systems/coordinate_systems.h"

// Window and transformer state used by the rendering subsystem.
typedef struct RenderRuntimeContext {
    PlatformWindow* window;
    PlatformSurface surface;
    CoordinateSystem2D transformer;
} RenderRuntimeContext;

#endif // RENDER_CONTEXT_H
