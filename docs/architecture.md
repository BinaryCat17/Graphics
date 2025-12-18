# Graphics Engine Architecture

## Overview
This project implements a high-performance, data-oriented graphics engine in C11. It follows a strict **Foundation -> Engine -> Features -> App** hierarchy. 

The architecture has recently undergone significant refactoring to decouple the generic runtime from specific application logic (like the Graph Editor).

## System Layers

### 1. Foundation (`src/foundation/`)
The bedrock of the system. Zero dependencies on other layers.
- **Platform:** Generic abstraction for OS windows, input, and graphics context creation.
    - *Recent Change:* Uses opaque handles (`void*`) to abstract specific graphics APIs (Vulkan/OpenGL) from the public interface.
- **Memory:** 
    - **MemoryArena:** Linear allocator for high-performance, fragmentation-free memory management. Used by the Transpiler.
    - **Buffer:** Dynamic array implementation.
- **Logger:** Thread-safe, leveled logging system.
- **Math:** Linear algebra (vec3, mat4) and geometry.
- **Meta:** Reflection system enabling runtime introspection of C structs.

### 2. Engine (`src/engine/`)
The core runtime. Depends only on **Foundation**.
- **Core:** The `Engine` struct is now a generic container. It holds systems (Window, Renderer, Assets) but *not* domain data.
    - *Inversion of Control:* The Engine invokes application-provided callbacks (`on_init`, `on_update`) to drive logic.
- **Graphics:** A render-graph based renderer.
    - *Frontend:* `RenderSystem`, `RenderPacket`, `Scene` (Generic).
    - *Backend:* `RendererBackend` interface (Vulkan implementation). Now strictly separated from platform details.
- **UI:** A reactive, data-driven UI framework ("The Dual Repeater" pattern).
- **Assets:** Resource management.

### 3. Features (`src/features/`)
Specific business logic modules.
- **Graph Editor:** A mathematical node graph model and Transpiler (GLSL generation).
    - *Optimization:* Transpiler now uses `MemoryArena` for string generation, eliminating heap fragmentation.

### 4. Application (`src/app/`)
The concrete implementation of a product.
- **Main:** Defines the `AppState`, initializes specific features (like the Graph Editor), and binds them to the Engine via callbacks.

---

## Key Architectural Patterns

### The "Unified Scene"
The engine renders everything as 3D primitives.
- **Primitives:** UI Panels, Text, and Wires are all `SceneObject`s.
- **Rendering:** A single "Uber-Shader" handles rendering based on the `type` field.

### Reactive UI (MVVM)
The UI does not store state. It reflects the application state via Reflection.
1.  **Model:** C Structs (e.g., `MathNode` in AppState).
2.  **ViewDefinition:** YAML file describing the hierarchy.
3.  **ViewModel:** The Reflection system binds YAML paths to memory addresses.

### Application Lifecycle (Callbacks)
The Engine is agnostic to the application data.
1.  `main.c` fills `EngineConfig` with `on_init` and `on_update`.
2.  `Engine` initializes low-level systems.
3.  `Engine` calls `on_init`. App allocates its state (`AppState`) and stores it in `engine->user_data`.
4.  Loop: `Engine` calls `on_update`. App updates its graph/UI and requests rendering.

---

## Current Architecture Status

| Aspect | Status | Notes |
| :--- | :--- | :--- |
| **Coupling** | **Low** | Engine is generic. App injects logic. |
| **Renderer Abstraction** | **Medium** | Interface uses `void*`, but backend implementation is still Vulkan-only. |
| **Memory** | **Hybrid** | Moving towards Arenas (Transpiler done), but generic Logic still uses `malloc`. |
