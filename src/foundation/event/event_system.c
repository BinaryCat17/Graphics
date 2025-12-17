#include "event_system.h"
#include <stdlib.h>
#include <string.h>

#define MAX_LISTENERS 1024

typedef struct {
    void* sender;
    EventCallback callback;
    void* user_data;
    bool active;
} Listener;

static struct {
    Listener listeners[MAX_LISTENERS];
    int count;
} g_event_sys;

void event_system_init(void) {
    memset(&g_event_sys, 0, sizeof(g_event_sys));
}

void event_system_shutdown(void) {
    memset(&g_event_sys, 0, sizeof(g_event_sys));
}

void event_subscribe(void* sender, EventCallback callback, void* user_data) {
    // Find empty slot or append
    for (int i = 0; i < MAX_LISTENERS; ++i) {
        if (!g_event_sys.listeners[i].active) {
            g_event_sys.listeners[i].sender = sender;
            g_event_sys.listeners[i].callback = callback;
            g_event_sys.listeners[i].user_data = user_data;
            g_event_sys.listeners[i].active = true;
            if (i >= g_event_sys.count) g_event_sys.count = i + 1;
            return;
        }
    }
}

void event_unsubscribe(void* sender, EventCallback callback) {
    for (int i = 0; i < g_event_sys.count; ++i) {
        if (g_event_sys.listeners[i].active && 
            g_event_sys.listeners[i].sender == sender && 
            g_event_sys.listeners[i].callback == callback) {
            g_event_sys.listeners[i].active = false;
        }
    }
}

void event_emit(void* sender, const char* property) {
    for (int i = 0; i < g_event_sys.count; ++i) {
        if (g_event_sys.listeners[i].active) {
            // Match sender or global listener
            if (g_event_sys.listeners[i].sender == sender || g_event_sys.listeners[i].sender == NULL) {
                g_event_sys.listeners[i].callback(sender, property, g_event_sys.listeners[i].user_data);
            }
        }
    }
}
