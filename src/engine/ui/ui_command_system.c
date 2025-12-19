#include "ui_command_system.h"
#include "foundation/logger/logger.h"
#include <string.h>
#include <stdlib.h>

#define MAX_COMMANDS 128

typedef struct UiCommand {
    StringId id;
    char* name; // Debug only
    UiCommandCallback callback;
    void* user_data;
} UiCommand;

static UiCommand g_commands[MAX_COMMANDS];
static int g_command_count = 0;

void ui_command_init(void) {
    memset(g_commands, 0, sizeof(g_commands));
    g_command_count = 0;
}

void ui_command_shutdown(void) {
    for (int i = 0; i < g_command_count; ++i) {
        if (g_commands[i].name) free(g_commands[i].name);
    }
    g_command_count = 0;
}

void ui_command_register(const char* name, UiCommandCallback callback, void* user_data) {
    if (g_command_count >= MAX_COMMANDS) {
        LOG_ERROR("CommandSystem: Max commands reached (%d)", MAX_COMMANDS);
        return;
    }
    
    StringId id = str_id(name);

    // Check if already exists
    for (int i = 0; i < g_command_count; ++i) {
        if (g_commands[i].id == id) {
            g_commands[i].callback = callback;
            g_commands[i].user_data = user_data;
            return;
        }
    }
    
    g_commands[g_command_count].id = id;
    g_commands[g_command_count].name = strdup(name);
    g_commands[g_command_count].callback = callback;
    g_commands[g_command_count].user_data = user_data;
    g_command_count++;
    
    LOG_DEBUG("CommandSystem: Registered command '%s' (Hash: %u)", name, id);
}

void ui_command_execute(const char* name, UiElement* target) {
    if (!name || name[0] == '\0') return;
    ui_command_execute_id(str_id(name), target);
}

void ui_command_execute_id(StringId id, UiElement* target) {
    if (id == 0) return;
    
    for (int i = 0; i < g_command_count; ++i) {
        if (g_commands[i].id == id) {
            if (g_commands[i].callback) {
                g_commands[i].callback(g_commands[i].user_data, target);
            }
            return;
        }
    }
    
    // Warn only on first fail? No, just warn.
    // LOG_WARN("CommandSystem: Command ID %u not found", id);
}