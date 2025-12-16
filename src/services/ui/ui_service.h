#ifndef UI_SERVICE_H
#define UI_SERVICE_H

#include <stdbool.h>

#include "services/core/core_context.h"
#include "core/state/state_manager.h"
#include "services/ui/ui_context.h"
#include "core/service_manager/service.h"

bool ui_build(UiContext* ui, const Assets* assets, const Scene* scene, Model* model);
bool ui_prepare_runtime(UiContext* ui, float ui_scale, StateManager* state_manager,
                       int ui_type_id);
bool ui_service_subscribe(UiContext* ui, StateManager* state_manager, int model_type_id);
void ui_refresh_layout(UiContext* ui, float new_scale);
void ui_frame_update(UiContext* ui);
void ui_handle_mouse_button(UiContext* ui, double mx, double my, int button, int action);
void ui_handle_scroll(UiContext* ui, double mx, double my, double yoff);
void ui_handle_cursor(UiContext* ui, double x, double y);
float ui_compute_scale(const UiContext* ui, float target_w, float target_h);
const ServiceDescriptor* ui_service_descriptor(void);

#endif // UI_SERVICE_H
