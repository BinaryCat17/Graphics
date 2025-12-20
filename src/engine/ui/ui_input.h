#ifndef UI_INPUT_H
#define UI_INPUT_H

#include <stdbool.h>
#include "foundation/string/string_id.h"

// Forward Declarations
typedef struct UiElement UiElement;
typedef struct InputSystem InputSystem;

typedef enum UiEventType {
    UI_EVENT_NONE = 0,
    UI_EVENT_CLICK,         // Triggered on mouse up (if active)
    UI_EVENT_VALUE_CHANGE,  // Triggered when input modifies data
    UI_EVENT_DRAG_START,
    UI_EVENT_DRAG_END
} UiEventType;

typedef struct UiEvent {
    UiEventType type;
    UiElement* target;
} UiEvent;

// --- Command System ---
typedef void (*UiCommandCallback)(void* user_data, UiElement* target);

void ui_command_init(void);
void ui_command_shutdown(void);
void ui_command_register(const char* name, UiCommandCallback callback, void* user_data);
void ui_command_execute_id(StringId id, UiElement* target);

// --- Input System ---
typedef struct UiInputContext UiInputContext;

UiInputContext* ui_input_create(void);
void ui_input_destroy(UiInputContext* ctx);
void ui_input_init(UiInputContext* ctx);
void ui_input_update(UiInputContext* ctx, UiElement* root, const InputSystem* input);
bool ui_input_pop_event(UiInputContext* ctx, UiEvent* out_event);

#endif // UI_INPUT_H
