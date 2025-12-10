#ifndef SERVICE_H
#define SERVICE_H

#include <stdbool.h>
#include <stddef.h>

#include "runtime/app_services.h"

// Generic configuration passed to services during initialization and startup.
typedef struct ServiceConfig {
    const char* assets_dir;
    const char* scene_path;
} ServiceConfig;

// Descriptor for a service module that can be registered and retrieved by name.
typedef struct ServiceDescriptor {
    const char* name;
    bool (*init)(AppServices* services, const ServiceConfig* config);
    bool (*start)(AppServices* services, const ServiceConfig* config);
    void (*stop)(AppServices* services);
    void* context;
    void* thread_handle;
} ServiceDescriptor;

bool service_registry_register(const ServiceDescriptor* descriptor);
const ServiceDescriptor* service_registry_get(const char* name);
const ServiceDescriptor* const* service_registry_all(size_t* count);

#endif // SERVICE_H
