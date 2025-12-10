#ifndef RUNTIME_H
#define RUNTIME_H

#include <stdbool.h>

#include "runtime/app_services.h"

bool runtime_init(AppServices* services);
void runtime_shutdown(AppServices* services);
void runtime_update_transformer(AppServices* services);

#endif // RUNTIME_H
