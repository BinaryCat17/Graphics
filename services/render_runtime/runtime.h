#ifndef RUNTIME_H
#define RUNTIME_H

#include <stdbool.h>

#include "render_runtime/render_runtime_service.h"

bool runtime_init(RenderRuntimeServiceContext* context);
void runtime_shutdown(RenderRuntimeServiceContext* context);
void runtime_update_transformer(RenderRuntimeServiceContext* context);

#endif // RUNTIME_H
