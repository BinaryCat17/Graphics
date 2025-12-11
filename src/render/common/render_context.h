#ifndef RENDER_CONTEXT_H
#define RENDER_CONTEXT_H

#include <GLFW/glfw3.h>

#include "core/math/coordinate_spaces.h"

// Window and transformer state used by the rendering subsystem.
typedef struct RenderRuntimeContext {
    GLFWwindow* window;
    CoordinateTransformer transformer;
} RenderRuntimeContext;

#endif // RENDER_CONTEXT_H
