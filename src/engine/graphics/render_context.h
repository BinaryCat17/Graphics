#ifndef RENDER_CONTEXT_H
#define RENDER_CONTEXT_H

#include "foundation/platform/platform.h"
#include "foundation/math/coordinate_systems.h"

// Window and transformer state used by the rendering subsystem.
typedef struct RenderRuntimeContext {
    PlatformWindow* window;
    PlatformSurface surface;
    CoordinateSystem2D transformer;
} RenderRuntimeContext;

#endif // RENDER_CONTEXT_H
