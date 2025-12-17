#ifndef UI_LAYOUT_H
#define UI_LAYOUT_H

#include "ui_def.h"

#include <stdint.h>
#include <stdbool.h>

typedef float (*UiTextMeasureFunc)(const char* text, void* user_data);

void ui_layout_set_measure_func(UiTextMeasureFunc func, void* user_data);
void ui_layout_root(UiView* root, float window_w, float window_h, uint64_t frame_number, bool log_debug);

#endif // UI_LAYOUT_H
