#ifndef UI_LAYOUT_H
#define UI_LAYOUT_H

// --- INTERNAL HEADER: Do not include in public API ---
// Use ui_core.h (ui_instance_layout) instead.

#include "../ui_core.h"

#include <stdint.h>
#include <stdbool.h>

typedef float (*UiTextMeasureFunc)(const char* text, void* user_data);

void ui_layout_root(UiElement* root, float window_w, float window_h, uint64_t frame_number, bool log_debug, UiTextMeasureFunc measure_func, void* measure_data);

#endif // UI_LAYOUT_H
