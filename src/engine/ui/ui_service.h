#ifndef UI_SYSTEM_H
#define UI_SYSTEM_H

#include <stdbool.h>
#include "engine/ui/ui_context.h"
#include "engine/assets/assets_service.h"
#include "domains/cad_model/cad_scene.h"
#include "engine/ui/model_style.h"

// Lifecycle
bool ui_system_init(UiContext* ui);
void ui_system_shutdown(UiContext* ui);

// Utils
float ui_compute_scale(const UiContext* ui, float target_w, float target_h);

// Building & Update
bool ui_system_build(UiContext* ui, const Assets* assets, const Scene* scene, Model* model);
bool ui_system_prepare_runtime(UiContext* ui, float ui_scale);

// Runtime loop
void ui_system_update(UiContext* ui, float dt);
void ui_system_refresh_layout(UiContext* ui, float new_scale);

// Input
void ui_system_handle_mouse(UiContext* ui, double mx, double my, int button, int action);
void ui_system_handle_scroll(UiContext* ui, double mx, double my, double yoff);
void ui_system_handle_cursor(UiContext* ui, double x, double y);

#endif // UI_SYSTEM_H
