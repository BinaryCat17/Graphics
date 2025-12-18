#include "engine/graphics/backend/renderer_backend.h"
#include <string.h>
#include <stdlib.h>

#define MAX_BACKENDS 8
static RendererBackend* registry[MAX_BACKENDS] = {0};
static int registry_count = 0;

bool renderer_backend_register(RendererBackend* backend) {
    if (!backend || registry_count >= MAX_BACKENDS) return false;
    registry[registry_count++] = backend;
    return true;
}

RendererBackend* renderer_backend_get(const char* id) {
    if (!id) return NULL;
    for (int i = 0; i < registry_count; ++i) {
        if (strcmp(registry[i]->id, id) == 0) {
            return registry[i];
        }
    }
    return NULL;
}

RendererBackend* renderer_backend_default(void) {
    if (registry_count > 0) return registry[0];
    return NULL;
}
