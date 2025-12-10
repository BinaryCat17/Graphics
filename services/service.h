#ifndef SERVICE_H
#define SERVICE_H

#include <stdbool.h>
#include <stddef.h>

#include "runtime/app_services.h"

// Generic configuration passed to services during initialization and startup.
typedef struct ServiceConfig {
    const char* assets_dir;
    const char* scene_path;
    const char* renderer_backend;
    const char* render_log_sink;
    const char* render_log_target;
    bool render_log_enabled;
} ServiceConfig;

// Descriptor for a service module that can be registered and retrieved by name.
typedef struct ServiceDescriptor {
    const char* name;
    const char* const* dependencies;
    size_t dependency_count;
    bool (*init)(AppServices* services, const ServiceConfig* config);
    bool (*start)(AppServices* services, const ServiceConfig* config);
    void (*stop)(AppServices* services);
    void* context;
    void* thread_handle;
} ServiceDescriptor;

#endif // SERVICE_H
