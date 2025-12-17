# Roadmap: The Path to a Professional Math Engine

## Phase 1: The Foundation (Voice & Muscle)
*Goal: Enable the engine to communicate (Text) and handle scale (Instancing).*

1.  **Text Rendering (Priority #0)**
    *   Implement Font Atlas loading (FreeType or stb_truetype).
    *   Create a specialized Shader/Pipeline for Signed Distance Fields (SDF) or simple textured quads.
    *   Integrate `Text` components into the `Unified Scene`.
2.  **Hardware Instancing**
    *   Upgrade `SceneObject` to support Instance Data (ID, Custom Buffer Index).
    *   Modify `unified.vert` to read per-instance data.
    *   Demonstrate rendering 100,000+ generic objects efficiently.

## Phase 2: The Nervous System (Reactivity)
*Goal: Build the Reactive UI infrastructure to support the Editor.*

1.  **Reflection Upgrade (Observer Pattern)**
    *   Modify `codegen.py` to generate setter macros/functions that trigger events.
    *   Implement a lightweight `EventSystem` (Signal/Slot).
2.  **UI Template Engine**
    *   Implement the `Repeater` concept: `UiView` that listens to a `MetaArray`.
    *   Create a generic `Container` that lays out dynamic children.
3.  **Input & Interaction**
    *   Implement Mouse Picking (Raycasting) in the `Unified Scene`.
    *   Add Drag-and-Drop support mapped to Data Events (modifying the underlying C structs).

## Phase 3: The Visual Editor (The Interface)
*Goal: Use the Reactive UI to build the Node Graph Editor.*

1.  **Graph Data Model**
    *   Refine `MathGraph` to support UI metadata (position, selection state) separate from logic.
2.  **The Canvas**
    *   Construct the Node Graph UI using `Repeater` (for Nodes) and `ConnectionRenderer` (for wires).
    *   Implement node selection, movement, and linking.
3.  **Property Inspector**
    *   Build an automatic side-panel that generates controls (Sliders, Fields) based on the selected Node's reflection data.

## Phase 4: The Brain (Transpilation)
*Goal: Turn the Visual Graph into executable GPU code.*

1.  **Expression Parser & Transpiler**
    *   Implement the logic to traverse `MathGraph` and generate GLSL strings.
2.  **Shader Hot-Reloading**
    *   Pipeline to compile generated GLSL -> SPIR-V -> VkPipeline at runtime.
3.  **Compute & Visualize**
    *   Link the Compute Shader output buffer to the Instanced Renderer.
    *   **Milestone:** User creates a "Wave" graph, and the engine renders a million-particle wave simulation in real-time.