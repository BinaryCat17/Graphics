# API Reference

## Core Systems

### Engine (`engine/core/engine.h`)
The main runtime container.
- `engine_run(Engine* engine)`: Starts the main loop. Blocks until exit.

### Config (`engine/core/engine.h`)
```c
typedef struct EngineConfig {
    int width, height;
    const char* name;
    // Lifecycle Callbacks
    void (*on_init)(Engine* engine);
    void (*on_update)(Engine* engine);
    // Resource Paths
    const char* asset_path;
    const char* ui_path;
} EngineConfig;
```

## Graphics Subsystem

### Render System (`engine/graphics/render_system.h`)
- `render_system_submit_packet(RenderSystem* rs, RenderPacket* packet)`: Queues a frame for rendering.

### Scene (`engine/graphics/scene/scene.h`)
- `scene_begin_frame(Scene* scene)`: Clears previous frame data.
- `scene_add_mesh(...)`: Adds a 3D model.
- `scene_add_text(...)`: Adds text (Note: See performance warnings in `technical_audit.md`).

## Foundation Layer

### Logger (`foundation/logger/logger.h`)
- `LOG_INFO(fmt, ...)`
- `LOG_ERROR(fmt, ...)`
- `LOG_FATAL(fmt, ...)`: Logs and aborts.

### Memory (`foundation/memory/arena.h`)
- `Arena`: Linear allocator.
- `arena_alloc(Arena* a, size_t size)`: O(1) allocation.
