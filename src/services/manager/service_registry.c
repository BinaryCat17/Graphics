#include "services/manager/service.h"

#include <stdio.h>
#include <string.h>

#define MAX_REGISTERED_SERVICES 16

static const ServiceDescriptor* g_services[MAX_REGISTERED_SERVICES];
static size_t g_service_count = 0;

static int service_registered(const char* name) {
    for (size_t i = 0; i < g_service_count; ++i) {
        if (g_services[i] && g_services[i]->name && strcmp(g_services[i]->name, name) == 0) {
            return 1;
        }
    }
    return 0;
}

bool service_registry_register(const ServiceDescriptor* descriptor) {
    if (!descriptor) {
        fprintf(stderr, "Cannot register NULL service descriptor.\n");
        return false;
    }
    if (!descriptor->name) {
        fprintf(stderr, "Cannot register service with no name.\n");
        return false;
    }
    if (g_service_count >= MAX_REGISTERED_SERVICES) {
        fprintf(stderr, "Service registry is full, cannot register %s\n", descriptor->name);
        return false;
    }
    if (service_registered(descriptor->name)) {
        fprintf(stderr, "Service %s already registered\n", descriptor->name);
        return false;
    }
    g_services[g_service_count++] = descriptor;
    return true;
}

const ServiceDescriptor* service_registry_get(const char* name) {
    if (!name) return NULL;
    for (size_t i = 0; i < g_service_count; ++i) {
        if (g_services[i] && g_services[i]->name && strcmp(g_services[i]->name, name) == 0) {
            return g_services[i];
        }
    }
    return NULL;
}

const ServiceDescriptor* const* service_registry_all(size_t* count) {
    if (count) *count = g_service_count;
    return g_services;
}
