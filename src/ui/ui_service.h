#ifndef UI_SERVICE_H
#define UI_SERVICE_H

#include <stdbool.h>

#include "runtime/app_services.h"

bool ui_build(AppServices* services);
bool ui_prepare_runtime(AppServices* services, float ui_scale);
void ui_refresh_layout(AppServices* services, float new_scale);
void ui_frame_update(AppServices* services);
void ui_handle_mouse_button(AppServices* services, double mx, double my, int button, int action);
void ui_handle_scroll(AppServices* services, double mx, double my, double yoff);
void ui_handle_cursor(AppServices* services, double x, double y);
float ui_compute_scale(const AppServices* services, float target_w, float target_h);
void ui_service_dispose(AppServices* services);

#endif // UI_SERVICE_H
