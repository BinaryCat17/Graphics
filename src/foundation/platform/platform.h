#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef struct PlatformWindow PlatformWindow;

typedef struct PlatformWindowSize {
    int width;
    int height;
} PlatformWindowSize;

typedef struct PlatformDpiScale {
    float x_scale;
    float y_scale;
} PlatformDpiScale;

typedef struct PlatformSurface {
    void* handle;
} PlatformSurface;

typedef enum PlatformMouseButton {
    PLATFORM_MOUSE_BUTTON_LEFT = 0,
    PLATFORM_MOUSE_BUTTON_RIGHT = 1,
    PLATFORM_MOUSE_BUTTON_MIDDLE = 2,
} PlatformMouseButton;

typedef enum PlatformInputAction {
    PLATFORM_RELEASE = 0,
    PLATFORM_PRESS = 1,
    PLATFORM_REPEAT = 2,
} PlatformInputAction;

typedef void (*PlatformMouseButtonCallback)(PlatformWindow* window, PlatformMouseButton button, PlatformInputAction action,
                                            int mods, void* user_data);
typedef void (*PlatformKeyCallback)(PlatformWindow* window, int key, int scancode, PlatformInputAction action, int mods, void* user_data);
typedef void (*PlatformCharCallback)(PlatformWindow* window, unsigned int codepoint, void* user_data);
typedef void (*PlatformScrollCallback)(PlatformWindow* window, double xoff, double yoff, void* user_data);
typedef void (*PlatformCursorPosCallback)(PlatformWindow* window, double x, double y, void* user_data);
typedef void (*PlatformFramebufferSizeCallback)(PlatformWindow* window, int width, int height, void* user_data);

bool platform_layer_init(void);
void platform_layer_shutdown(void);

bool platform_vulkan_supported(void);

PlatformWindow* platform_create_window(int width, int height, const char* title);
void platform_destroy_window(PlatformWindow* window);

void platform_set_window_user_pointer(PlatformWindow* window, void* user_pointer);
void* platform_get_window_user_pointer(PlatformWindow* window);

PlatformWindowSize platform_get_window_size(PlatformWindow* window);
PlatformWindowSize platform_get_framebuffer_size(PlatformWindow* window);
PlatformDpiScale platform_get_window_dpi(PlatformWindow* window);
void platform_get_cursor_pos(PlatformWindow* window, double* x, double* y);
bool platform_get_mouse_button(PlatformWindow* window, int button);
bool platform_get_key(PlatformWindow* window, int key);

void platform_set_framebuffer_size_callback(PlatformWindow* window, PlatformFramebufferSizeCallback callback,
                                            void* user_data);
void platform_set_scroll_callback(PlatformWindow* window, PlatformScrollCallback callback, void* user_data);
void platform_set_mouse_button_callback(PlatformWindow* window, PlatformMouseButtonCallback callback,
                                        void* user_data);
void platform_set_key_callback(PlatformWindow* window, PlatformKeyCallback callback, void* user_data);
void platform_set_char_callback(PlatformWindow* window, PlatformCharCallback callback, void* user_data);
void platform_set_cursor_pos_callback(PlatformWindow* window, PlatformCursorPosCallback callback, void* user_data);

bool platform_window_should_close(PlatformWindow* window);
void platform_set_window_should_close(PlatformWindow* window, bool should_close);

void platform_poll_events(void);
void platform_wait_events(void);
double platform_get_time_ms(void);

// Graphics API Support
bool platform_get_required_extensions(const char*** names, uint32_t* count);
bool platform_create_surface(PlatformWindow* window, void* instance, void* allocator, PlatformSurface* out_surface);
void platform_destroy_surface(void* instance, void* allocator, PlatformSurface* surface);

// String utilities
char* platform_strdup(const char* src);
void platform_strncpy(char* dest, const char* src, size_t count);

// File utilities
#include <stdio.h>
FILE* platform_fopen(const char* filename, const char* mode);

#endif // PLATFORM_H
