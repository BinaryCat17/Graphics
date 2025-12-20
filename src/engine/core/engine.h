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

// Lifecycle
Engine* engine_create(const EngineConfig* config);
void engine_run(Engine* engine);
void engine_destroy(Engine* engine);

// Accessors
RenderSystem* engine_get_render_system(Engine* engine);
InputSystem* engine_get_input_system(Engine* engine);
Assets* engine_get_assets(Engine* engine);
PlatformWindow* engine_get_window(Engine* engine);
const EngineConfig* engine_get_config(Engine* engine);

void* engine_get_user_data(Engine* engine);
void engine_set_user_data(Engine* engine, void* user_data);

float engine_get_dt(Engine* engine);
bool engine_is_running(Engine* engine);

void engine_set_show_compute(Engine* engine, bool show);
bool engine_get_show_compute(Engine* engine);
