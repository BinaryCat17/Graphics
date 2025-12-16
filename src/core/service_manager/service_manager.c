#include "core/service_manager/service_manager.h"

#include <stdio.h>
#include <string.h>
#include <threads.h>

static int find_service_index(const ServiceManager* manager, const char* name) {
    if (!manager || !name) return -1;
    for (size_t i = 0; i < manager->service_count; ++i) {
        if (manager->services[i].descriptor && manager->services[i].descriptor->name &&
            strcmp(manager->services[i].descriptor->name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

void service_manager_init(ServiceManager* manager) {
    if (!manager) return;
    memset(manager, 0, sizeof(ServiceManager));
}

bool service_manager_register(ServiceManager* manager, const ServiceDescriptor* descriptor) {
    if (!manager || !descriptor || !descriptor->name) {
        fprintf(stderr, "Cannot register invalid service descriptor.\n");
        return false;
    }
    if (manager->service_count >= SERVICE_MANAGER_MAX_SERVICES) {
        fprintf(stderr, "Service manager is full, cannot register %s.\n", descriptor->name);
        return false;
    }
    if (find_service_index(manager, descriptor->name) >= 0) {
        fprintf(stderr, "Service %s already registered.\n", descriptor->name);
        return false;
    }
    manager->services[manager->service_count++] = (ServiceEntry){.descriptor = descriptor, .started = false};
    return true;
}

static bool visit_dependencies(ServiceManager* manager, size_t index, int* state) {
    if (!manager || index >= manager->service_count) return false;
    if (state[index] == 1) {
        fprintf(stderr, "Detected cyclic dependency involving service '%s'.\n",
                manager->services[index].descriptor->name);
        return false;
    }
    if (state[index] == 2) return true;

    state[index] = 1;
    const ServiceDescriptor* descriptor = manager->services[index].descriptor;
    for (size_t i = 0; i < descriptor->dependency_count; ++i) {
        const char* dep_name = descriptor->dependencies[i];
        int dep_index = find_service_index(manager, dep_name);
        if (dep_index < 0) {
            fprintf(stderr, "Service '%s' depends on unknown service '%s'.\n", descriptor->name, dep_name);
            return false;
        }
        if (!visit_dependencies(manager, (size_t)dep_index, state)) return false;
    }

    state[index] = 2;
    manager->start_order[manager->start_order_count++] = index;
    return true;
}

static bool resolve_start_order(ServiceManager* manager) {
    int state[SERVICE_MANAGER_MAX_SERVICES] = {0};
    manager->start_order_count = 0;

    for (size_t i = 0; i < manager->service_count; ++i) {
        if (!visit_dependencies(manager, i, state)) return false;
    }
    return true;
}

void service_manager_stop(ServiceManager* manager, void* services) {
    if (!manager) return;
    for (size_t i = manager->start_order_count; i-- > 0;) {
        size_t idx = manager->start_order[i];
        ServiceEntry* reg = &manager->services[idx];
        if (reg->started && reg->descriptor && reg->descriptor->stop) {
            reg->descriptor->stop(services);
        }
        reg->started = false;
    }
    manager->start_order_count = 0;
}

bool service_manager_start(ServiceManager* manager, void* services, const ServiceConfig* config) {
    if (!manager || !services) return false;
    if (!resolve_start_order(manager)) return false;

    size_t started = 0;
    for (size_t i = 0; i < manager->start_order_count; ++i) {
        ServiceEntry* reg = &manager->services[manager->start_order[i]];
        const ServiceDescriptor* descriptor = reg->descriptor;
        if (descriptor->init && !descriptor->init(services, config)) {
            fprintf(stderr, "Service '%s' failed to initialize.\n", descriptor->name);
            manager->start_order_count = started;
            service_manager_stop(manager, services);
            return false;
        }
        if (descriptor->start && !descriptor->start(services, config)) {
            fprintf(stderr, "Service '%s' failed to start.\n", descriptor->name);
            manager->start_order_count = started;
            service_manager_stop(manager, services);
            return false;
        }
        reg->started = true;
        started++;
    }
    return true;
}

void service_manager_wait(ServiceManager* manager) {
    if (!manager) return;
    for (size_t i = 0; i < manager->start_order_count; ++i) {
        ServiceEntry* reg = &manager->services[manager->start_order[i]];
        if (reg->started && reg->descriptor && reg->descriptor->thread_handle) {
            const thrd_t* thread = (const thrd_t*)reg->descriptor->thread_handle;
            thrd_join(*thread, NULL);
        }
    }
}
