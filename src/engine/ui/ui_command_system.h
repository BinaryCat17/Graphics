#ifndef UI_COMMAND_SYSTEM_H
#define UI_COMMAND_SYSTEM_H

#include "ui_core.h"
#include "foundation/string/string_id.h"

typedef void (*UiCommandCallback)(void* user_data, UiElement* target);

void ui_command_init(void);
void ui_command_shutdown(void);

// Register a command by name
void ui_command_register(const char* name, UiCommandCallback callback, void* user_data);

// Execute a command by ID (Faster)
void ui_command_execute_id(StringId id, UiElement* target);

#endif // UI_COMMAND_SYSTEM_H
