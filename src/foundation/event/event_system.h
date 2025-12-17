#pragma once

#include <stdbool.h>

typedef void (*EventCallback)(void* sender, const char* property, void* user_data);

// Initialize the event system (allocate resources)
void event_system_init(void);

// Cleanup
void event_system_shutdown(void);

// Subscribe to changes on a specific object.
// If sender is NULL, subscribe to ALL events (global listener).
void event_subscribe(void* sender, EventCallback callback, void* user_data);

// Unsubscribe
void event_unsubscribe(void* sender, EventCallback callback);

// Emit a property change event
void event_emit(void* sender, const char* property);
