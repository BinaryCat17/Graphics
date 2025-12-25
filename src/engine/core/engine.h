#ifndef ENGINE_H
#define ENGINE_H

#include <stdbool.h>

// Forward Declarations (C11 allows redefinition of typedefs)
typedef struct RenderSystem RenderSystem;
typedef struct Assets Assets;
typedef struct PlatformWindow PlatformWindow;
typedef struct InputSystem InputSystem;
typedef struct MemoryArena MemoryArena;

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

// --- Feature System (Plugin Architecture) ---

typedef struct EngineFeature {
    const char* name;
    void* user_data;
    
    // Called when the feature is registered
    void (*on_init)(struct EngineFeature* self, Engine* engine);
    
    // Called every frame (Simulation Phase)
    void (*on_update)(struct EngineFeature* self, Engine* engine);
    
    // Called before rendering to generate draw commands (Extraction Phase)
    void (*on_extract)(struct EngineFeature* self, Engine* engine);
    
    // Called when the engine shuts down
    void (*on_shutdown)(struct EngineFeature* self);
} EngineFeature;

// Lifecycle
Engine* engine_create(const EngineConfig* config);
void engine_run(Engine* engine);
void engine_destroy(Engine* engine);

// Features
void engine_register_feature(Engine* engine, EngineFeature feature);

// Accessors
RenderSystem* engine_get_render_system(Engine* engine);
InputSystem* engine_get_input_system(Engine* engine);
Assets* engine_get_assets(Engine* engine);
PlatformWindow* engine_get_window(Engine* engine);
MemoryArena* engine_get_frame_arena(Engine* engine);
const EngineConfig* engine_get_config(const Engine* engine);

void* engine_get_user_data(const Engine* engine);
void engine_set_user_data(Engine* engine, void* user_data);

float engine_get_dt(const Engine* engine);
bool engine_is_running(const Engine* engine);

void engine_set_show_compute(Engine* engine, bool show);
bool engine_get_show_compute(const Engine* engine);

#endif // ENGINE_H
