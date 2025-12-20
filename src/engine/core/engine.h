#pragma once

#include "engine/graphics/render_system.h"
#include "engine/assets/assets.h"
#include "foundation/platform/platform.h"
#include "engine/input/input.h"

typedef struct Engine Engine;

typedef struct EngineConfig {
    int width;
    int height;
    const char* title;
    const char* assets_path;
    const char* ui_path;
    int log_level;
    double screenshot_interval;

    // Application Callbacks
    void (*on_init)(Engine* engine);
    void (*on_update)(Engine* engine);
} EngineConfig;

struct Engine {
    // Platform
    PlatformWindow* window;
    InputSystem input_system;

    // Systems
    RenderSystem* render_system;
    Assets assets;
    
    // Application Data
    void* user_data;
    
    // State
    bool running;
    bool show_compute_visualizer;
    EngineConfig config;
    double screenshot_interval;
    double last_screenshot_time;
    double last_time;
    float dt;
    
    // Callbacks
    void (*on_update)(Engine* engine);
};

bool engine_init(Engine* engine, const EngineConfig* config);
void engine_run(Engine* engine);
void engine_shutdown(Engine* engine);
