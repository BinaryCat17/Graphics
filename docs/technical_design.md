# Technical Design & Internals (v4.0)

**Context:** Implementation rules and standards for the codebase refactoring.

---

## 1. File Structure & Encapsulation Standard

We enforce a strict separation to prevent "Header Hell".

### Directory Layout

```text
[src/engine/module_name]
  ├── module.h           // PUBLIC API. Opaque handles only. No includes from other modules.
  ├── module.c           // FACADE. Implements module.h, delegates to internal implementation.
  ├── module_types.h     // (Optional) Public enums/structs required by the API.
  └── internal/
        ├── module_impl.h    // PRIVATE. Real struct definitions. Internal headers only.
        ├── subcomponent_a.c // Implementation logic.
        └── subcomponent_b.c // Implementation logic.
```

### Opaque Pointers (PIMPL) Rule

**❌ Bad (v3.0):**
*Leaks implementation details and forces heavy includes on consumers.*

```c
// render_system.h
#include "vulkan_backend.h" 

typedef struct RenderSystem {
    VulkanBackend* backend; // Now everyone knows about Vulkan
    VkDevice device;        // Disaster: Vulkan headers leaked everywhere
} RenderSystem;
```

**✅ Good (v4.0):**
*Clean interface, implementation is completely hidden.*

```c
// render_system.h
typedef struct RenderSystem RenderSystem; // Incomplete type (Opaque)

RenderSystem* render_system_create(void);
void render_system_update(RenderSystem* self);
```

```c
// src/engine/graphics/internal/render_system_internal.h
#include "renderer_backend.h"

struct RenderSystem {
    RendererBackend* backend; // Implementation detail hidden
    uint32_t frame_index;
};
```

---

## 2. The GDL (Graph Definition Language)

The engine's frame loop is defined by a YAML file, parsed by `config/pipeline_loader.c`.

### Format Spec (`assets/config/pipeline.yaml`)

```yaml
pipeline:
  resources:
    - name: "SceneColor"
      type: IMAGE_2D
      format: RGBA8
      size: [window_width, window_height]
    - name: "PhysicsData"
      type: BUFFER
      size: 1MB

  passes:
    - name: "SimulatePhysics"
      type: COMPUTE
      shader: "physics_solve.comp"
      outputs: ["PhysicsData"]

    - name: "Render3D"
      type: GRAPHICS
      inputs: ["PhysicsData"]
      outputs: ["SceneColor"]
      draw_list: "SceneBatches"  # Consumes batches tagged 'SceneBatches'

    - name: "RenderUI"
      type: GRAPHICS
      outputs: ["swapchain"]     # Output to screen
      draw_list: "UIBatches"     # Consumes batches tagged 'UIBatches'
```

---

## 3. The Feature System (Plugin Architecture)

To decouple `engine.c` from specific tools like `MathEditor`, we use a registration interface.

### The Interface

```c
typedef struct EngineFeature {
    const char* name;
    void* user_data;
    
    void (*on_init)(struct EngineFeature* self, Engine* engine);
    void (*on_update)(struct EngineFeature* self, Engine* engine);
    void (*on_extract)(struct EngineFeature* self, RenderCommandList* out_list);
    void (*on_shutdown)(struct EngineFeature* self);
} EngineFeature;
```

### Usage Pattern
1. `MathEditor` implements this interface.
2. In `main.c`, we call `engine_register_feature(engine, math_editor_feature())`.
3. The Engine iterates over registered features calling `on_update` and `on_extract`, unaware of what the feature actually does.

---

## 4. Render Command List (The Lingua Franca)

To decouple the Renderer from systems like UI or Scene, they communicate purely via data packets.

### The RenderBatch Structure

```c
typedef struct RenderBatch {
    uint32_t pipeline_id;

    // Geometry
    Stream* vertex_stream;
    Stream* index_stream;
    uint32_t index_count;

    // Data (SSBOs/UBOs for the shader)
    Stream* bind_groups[4]; 

    // Sorting & Layering
    uint32_t layer;
    float depth;
} RenderBatch;
```

**Workflow:**
1. **UI System** iterates its nodes, packs transforms/colors into a temporary `Stream`, and creates a `RenderBatch`.
2. **Render System** receives the batch, binds the pipeline, binds the stream, and issues a draw call. It does not know it is drawing a "UI Button".

---

## 5. Memory Management

*   **Frame Arena:** A linear allocator reset every frame. Used for generating `RenderCommandList` and temporary per-frame data.
*   **Asset Pool:** Pool allocators for long-lived resources (Textures, Meshes) to avoid fragmentation.
*   **Streams:** Wrappers around GPU buffers. This is the **only** permitted way to upload data to VRAM.
