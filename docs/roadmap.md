# Roadmap: The Path to a Professional Math Engine

## Phase 1: The Foundation (Voice & Muscle) [COMPLETED]
*Goal: Enable the engine to communicate (Text) and handle scale (Instancing).*

1.  **[DONE] Text Rendering**
    *   Implemented Font Atlas loading (stb_truetype).
    *   Integrated `Text` components into the `Unified Scene`.
2.  **[DONE] Hardware Instancing**
    *   Upgraded `SceneObject` to support Instance Data (ID, Custom Buffer Index).
    *   Modified `unified.vert` to read per-instance data.
    *   Verified rendering generic objects efficiently.

## Phase 2: The Nervous System (Reactivity) [COMPLETED]
*Goal: Build the Reactive UI infrastructure to support the Editor.*

1.  **[DONE] Reflection Upgrade (Observer Pattern)**
    *   Modified `codegen.py` to generate setter macros/functions that trigger events.
    *   Implemented `EventSystem` (Signal/Slot).
2.  **[DONE] UI Template Engine**
    *   Implemented the `Repeater` concept: `UiView` that listens to a reflected array.
    *   Updated `ui_loader` to support `count_source` for dynamic lists.
3.  **[PARTIAL] Input & Interaction**
    *   *Pending:* Implement Mouse Picking (Raycasting) in the `Unified Scene`.
    *   *Pending:* Add Drag-and-Drop support mapped to Data Events.

## Phase 3: The Visual Editor (The Interface)
*Goal: Use the Reactive UI to build the Node Graph Editor.*

1.  **Graph Data Model**
    *   [DONE] Refine `MathGraph` to support UI metadata (position) via Reflection.
    *   [TODO] Implement Node Selection state and logic.
2.  **The Canvas**
    *   [TODO] Construct the Node Graph UI using `Repeater` (for Nodes).
    *   [TODO] Implement `ConnectionRenderer` (custom draw primitive for wires).
    *   [TODO] Implement node dragging and port linking interaction.
3.  **Property Inspector**
    *   [TODO] Build an automatic side-panel that generates controls (Sliders, Fields) based on the selected Node's reflection data.

## Phase 4: The Brain (Transpilation)
*Goal: Turn the Visual Graph into executable GPU code.*

1.  **Expression Parser & Transpiler**
    *   Implement the logic to traverse `MathGraph` and generate GLSL strings.
2.  **Shader Hot-Reloading**
    *   Pipeline to compile generated GLSL -> SPIR-V -> VkPipeline at runtime.
3.  **Compute & Visualize**
    *   Link the Compute Shader output buffer to the Instanced Renderer.
    *   **Milestone:** User creates a "Wave" graph, and the engine renders a million-particle wave simulation in real-time.
