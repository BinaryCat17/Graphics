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

#if defined(__GNUC__) || defined(__clang__)
#define UI_UNUSED __attribute__((unused))
#else
#define UI_UNUSED
#endif

/**
 * @brief Macro to define a typesafe UI command callback.
 * 
 * Usage:
 * UI_COMMAND(MyCommandName, MyContextType) {
 *     // 'ctx' is (MyContextType*)
 *     // 'target' is (UiElement*)
 *     my_context_do_something(ctx);
 * }
 */
#define UI_COMMAND(CmdName, ContextType) \
    static void CmdName##_impl(ContextType* ctx UI_UNUSED, UiElement* target UI_UNUSED); /* NOLINT(bugprone-macro-parentheses) */ \
    static void CmdName(void* user_data, UiElement* target) { \
        CmdName##_impl((ContextType*)user_data, target); \
    } \
    static void CmdName##_impl(ContextType* ctx UI_UNUSED, UiElement* target UI_UNUSED) // NOLINT(bugprone-macro-parentheses)

/**
 * @brief Macro to register a typesafe UI command.
 * 
 * Usage:
 * UI_REGISTER_COMMAND("MyCommand", MyCommandName, my_context_ptr);
 */
#define UI_REGISTER_COMMAND(NameStr, CmdFunc, ContextPtr) \
    ui_command_register((NameStr), (CmdFunc), (void*)(ContextPtr))

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
