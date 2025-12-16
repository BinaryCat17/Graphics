#ifndef RENDER_THREAD_H
#define RENDER_THREAD_H

#include <stdbool.h>
#include "foundation/math/coordinate_systems.h"
#include "foundation/platform/platform.h"

// Context local to the render thread (or shared state)
typedef struct RenderRuntimeContext {
    PlatformWindow* window;
    PlatformSurface surface;
    CoordinateSystem2D transformer;
} RenderRuntimeContext;

// Forward decl
typedef struct RenderSystem RenderSystem;

bool runtime_init(RenderSystem* sys);
void runtime_shutdown(RenderRuntimeContext* context);

#endif // RENDER_THREAD_H