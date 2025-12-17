#pragma once

#include "engine/graphics/render_system.h"
#include "engine/assets/assets.h"
#include "features/graph_editor/math_graph.h"
#include "engine/ui/ui_loader.h"
#include "foundation/platform/platform.h"
#include "engine/ui/ui_def.h"

typedef struct EngineConfig {
    int width;
    int height;
    const char* title;
    const char* assets_path;
    const char* ui_path;
    int log_level;
} EngineConfig;

typedef struct Engine {
    // Platform
    PlatformWindow* window;
    InputState input;

    // Systems
    RenderSystem render_system;
    Assets assets;
    
    // Domain Data
    MathGraph graph;
    
    // UI
    UiDef* ui_def;
    UiView* ui_root;
    
    // State
    bool running;
    bool show_compute_visualizer;
} Engine;

bool engine_init(Engine* engine, const EngineConfig* config);
void engine_run(Engine* engine);
void engine_shutdown(Engine* engine);
