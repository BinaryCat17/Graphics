#ifndef UI_COMMAND_SYSTEM_H
#define UI_COMMAND_SYSTEM_H

#include "../ui_core.h"

// Internal Initialization
void ui_command_init(void);
void ui_command_shutdown(void);

// Implementation handles these
// void ui_command_register(const char* name, UiCommandCallback callback, void* user_data);
// void ui_command_execute_id(StringId id, SceneNode* target);

#endif // UI_COMMAND_SYSTEM_H
