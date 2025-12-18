# Architecture Overview

## Design Philosophy
The engine follows a **Data-Oriented** design philosophy, prioritizing linear memory access and separation of data from logic. It is structured in four strict layers:

`App` -> `Features` -> `Engine` -> `Foundation`

Dependencies flow **downwards only**.

---

## 1. Foundation (`src/foundation/`)
**The Bedrock.** Contains platform-agnostic utilities and the platform abstraction layer itself.
- **Platform:** Uses opaque handles (`void*`) to abstract OS-specifics (Windowing, Input).
- **Memory:**
    - `Arena`: Linear allocator for high-performance temporary memory.
    - `Buffer`: Dynamic array implementation.
- **Logger:** Thread-safe logging with severity levels.
- **Meta:** A custom reflection system used for UI data binding.

## 2. Engine (`src/engine/`)
**The Runtime.** A generic framework that manages the application lifecycle.
- **Core (`Engine`):** Owns the main loop. Inverts control by calling `on_init` and `on_update` provided by the App.
- **Graphics:**
    - **Unified Scene:** All visual elements (3D models, UI panels, Text) are `SceneObject`s.
    - **RenderPacket:** A unified structure passed to the backend.
    - **Backend:** Currently Vulkan-based. Abstrated via `RendererBackend`.
- **UI (Reactive):**
    - A reactive system where `YAML` defines the View and `Reflection` binds it to C structs (ViewModel).
    - *Constraint:* Currently operates in immediate mode, resolving bindings every frame.

## 3. Features (`src/features/`)
**Reusable Modules.** specific business logic that can be plugged into the engine.
- **Graph Editor:** A node-graph data model and GLSL transpiler.
    - **Transpiler:** Converts the node graph into valid GLSL Compute Shader code.

## 4. Application (`src/app/`)
**The Product.**
- **`main.c`:** The entry point.
    - Defines `AppState` (the root data structure).
    - Configures the `Engine` callbacks.
    - Glues the `GraphEditor` feature to the `RenderSystem`.

---

## Key Data Flows

### The Frame Loop
1.  **Input:** Platform events update `InputState`.
2.  **App Update (`on_update`):**
    - App logic modifies `AppState`.
    - Graph connections are solved.
3.  **UI Layout:**
    - UI tree is traversed.
    - Reflection reads `AppState` to update text/values.
    - Layout sizes are calculated.
4.  **Scene Generation:**
    - App converts its state into `SceneObject`s (Nodes -> Meshes, Wires -> Lines).
    - UI converts its tree into `SceneObject`s (Panels -> Quads, Text -> Char Quads).
5.  **Render:**
    - Backend processes the list of `SceneObject`s.
    - *Note:* Currently uses a monolithic "Uber-Shader".

## Architectural Constraints (Known)
See `technical_audit.md` for a detailed breakdown.
1.  **Instance Limit:** Renderer has a fixed buffer size.
2.  **Text Rendering:** Naive implementation (1 char = 1 draw object).