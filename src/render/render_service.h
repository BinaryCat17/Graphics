#ifndef RENDER_SERVICE_H
#define RENDER_SERVICE_H

#include <stdbool.h>

#include "runtime/app_services.h"

bool render_service_init(AppServices* services);
void render_service_update_transformer(AppServices* services);
void render_loop(AppServices* services);
void render_service_shutdown(AppServices* services);

#endif // RENDER_SERVICE_H
