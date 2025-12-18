# Graphics Engine Architecture

## Overview
This project implements a high-performance, data-oriented graphics engine in C11. It is designed to be a foundation for visual applications, specifically node-based editors and compute visualization tools.

The architecture follows a strict layered approach (Foundation -> Engine -> Features -> App), although current implementation details show significant coupling that is being actively refactored.

## System Layers

### 1. Foundation (`src/foundation/`)
The bedrock of the system. Has **zero dependencies** on other layers.
- **Platform:** Abstraction for OS windows, input, and file system (wrapping GLFW).
- **Memory:** Custom memory allocators (Arenas, Pools) to avoid fragmentation and `malloc` overhead.
- **Logger:** Thread-safe, leveled logging system.
- **Math:** Linear algebra (vec3, mat4), geometry, and coordinate systems.
- **Meta:** A reflection system enabling runtime introspection of C structs, critical for the UI and Serialization systems.

### 2. Engine (`src/engine/`)
The core runtime. Depends only on **Foundation**.
- **Graphics:** A render graph based renderer.
    - *Frontend:* `RenderSystem`, `RenderPacket`, `Scene` (Generic).
    - *Backend:* `RendererBackend` interface (currently Vulkan implementation).
- **UI:** A reactive, data-driven UI framework ("The Dual Repeater" pattern).
    - Uses YAML templates (`ui_loader`) and Reflection to bind directly to C-structs.
    - *Layout:* Flexbox-like layout engine.
- **Assets:** Resource management for fonts, shaders, and textures.

### 3. Features (`src/features/`)
Specific business logic modules. Should operate as plugins.
- **Graph Editor:** A mathematical node graph model, transpiler (to GLSL), and interaction logic.

### 4. Application (`src/app/`)
The entry point.
- **Main:** Configures the engine, loads specific features, and runs the loop.

---

## Key Architectural Patterns

### The "Unified Scene"
Unlike traditional engines that separate UI and 3D rendering, this engine uses a single **Unified Scene**.
- **Primitives:** UI Panels, Text, and Graph Wires are all `SceneObject`s.
- **Rendering:** A single "Uber-Shader" (`unified.frag`) handles rendering based on the `type` field (Quad, SDF Curve, etc.).
- **Benefit:** Massive batching efficiency. The UI is just 3D geometry.

### Reactive UI (MVVM)
The UI does not store state. It reflects the application state.
1.  **Model:** C Structs (e.g., `MathNode`).
2.  **ViewDefinition:** YAML file describing the hierarchy.
3.  **ViewModel:** The Reflection system binds the YAML paths (`node.x`) to memory addresses.
4.  **Layout:** Calculated every frame based on the Model.

### Transpilation Pipeline
The engine includes a runtime compiler:
1.  **Graph Data:** User connects nodes (Math, Logic).
2.  **Transpiler:** Converts the graph topology into GLSL Compute Shader source code.
3.  **JIT:** The Vulkan backend invokes `glslc` to compile SPIR-V at runtime and dispatches the compute job.

---

## Current Architecture vs. Ideal State

| Aspect | Current Reality | Ideal Target |
| :--- | :--- | :--- |
| **Coupling** | `Engine` hardcodes `GraphEditor` headers. | `Engine` is generic; `App` plugs Feature into Engine. |
| **Rendering** | `RenderSystem` calls `ui_build_scene`. | `SceneBuilder` (External) feeds both UI and 3D into Renderer. |
| **Backend** | `RendererBackend` interface leaks Vulkan types. | Strict opaque handles (`BackendHandle`). |
| **Memory** | Mixed usage of `malloc` and `Arena`. | 100% Arena/Pool usage for frame data. |