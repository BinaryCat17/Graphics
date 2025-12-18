#pragma once

#include "engine/graphics/render_system.h"
#include "engine/assets/assets.h"
#include "engine/ui/ui_parser.h"
#include "foundation/platform/platform.h"
#include "engine/ui/ui_core.h"

typedef struct Engine Engine;

typedef struct EngineConfig {
    int width;
    int height;
    const char* title;
    const char* assets_path;
    const char* ui_path;
    int log_level;

    // Application Callbacks
    void (*on_init)(Engine* engine);
    void (*on_update)(Engine* engine);
} EngineConfig;

struct Engine {
    // Platform
    PlatformWindow* window;
    InputState input;

    // Systems
    RenderSystem render_system;
    Assets assets;
    
    // Application Data
    void* user_data;
    
    // State
    bool running;
    bool show_compute_visualizer;
    
    // Callbacks
    void (*on_update)(Engine* engine);
};

bool engine_init(Engine* engine, const EngineConfig* config);
void engine_run(Engine* engine);
void engine_shutdown(Engine* engine);
