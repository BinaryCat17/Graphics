#ifndef SERVICE_MANAGER_H
#define SERVICE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>

#include "services/service.h"

#define SERVICE_MANAGER_MAX_SERVICES 16

typedef struct ServiceEntry {
    const ServiceDescriptor* descriptor;
    bool started;
} ServiceEntry;

typedef struct ServiceManager {
    ServiceEntry services[SERVICE_MANAGER_MAX_SERVICES];
    size_t service_count;
    size_t start_order[SERVICE_MANAGER_MAX_SERVICES];
    size_t start_order_count;
} ServiceManager;

void service_manager_init(ServiceManager* manager);
bool service_manager_register(ServiceManager* manager, const ServiceDescriptor* descriptor);
bool service_manager_start(ServiceManager* manager, AppServices* services, const ServiceConfig* config);
void service_manager_wait(ServiceManager* manager);
void service_manager_stop(ServiceManager* manager, AppServices* services);

#endif // SERVICE_MANAGER_H
