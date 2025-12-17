#ifndef UI_LAYOUT_H
#define UI_LAYOUT_H

#include "ui_def.h"

#include <stdint.h>
#include <stdbool.h>

void ui_layout_root(UiView* root, float window_w, float window_h, uint64_t frame_number, bool log_debug);

#endif // UI_LAYOUT_H
