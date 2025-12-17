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
3.  **[DONE] Input & Interaction**
    *   Implemented Mouse input handling with Drag-and-Drop support mapped to Data Events (Binding Write-back).
    *   Added Scroll Event support.

## Phase 3: The Visual Editor (The Interface) [COMPLETED]
*Goal: Use the Pure Data approach to build the Node Graph Editor.*

1.  **[DONE] Scene Primitives (No Custom Drawing)**
    *   Extended `SceneObject` with `ScenePrimitiveType` (Curve support).
    *   Updated `unified.frag` to render Bezier curves via SDF.
    *   Added `UI_NODE_CURVE` to the UI Definition and Bridge.
2.  **[DONE] Graph ViewModel**
    *   Created `VisualWire` struct in `MathGraph`.
    *   Implemented `math_graph_update_visuals()` to calculate wire coordinates from node links.
3.  **[DONE] The Editor UI**
    *   Created `editor.yaml` using the Dual Repeater pattern (Wires Layer + Nodes Layer).
    *   Implemented Node Dragging logic (View -> C-Struct -> Event -> View Update).

## Phase 4: The Brain (Transpilation) [COMPLETED]
*Goal: Turn the Visual Graph into executable GPU code.*

1.  **[DONE] Expression Transpiler**
    *   Implemented `transpiler.c`: Traverses `MathGraph` and generates GLSL Compute Shader code.
    *   Supports basic arithmetic (Add, Sub, Mul, Div, Sin, Cos) and Image generation (TRANSPILE_MODE_IMAGE_2D).
2.  **[DONE] Runtime Shader Compilation**
    *   Implemented `vk_compute` backend.
    *   Uses `glslc` (via subprocess) to compile generated GLSL strings to SPIR-V at runtime.
    *   Implemented `vk_run_compute_graph_image` to dispatch compute jobs on 2D images.

## Phase 5: Visualization (Compute & Display) [COMPLETED]
*Goal: See the math.*

1.  **[DONE] Compute-to-Texture Pipeline**
    *   Implemented `VkImage` creation for Compute Target.
    *   Implemented Descriptor Set management to bind Compute Result as a texture (Set 2).
2.  **[DONE] Visualization UI**
    *   Added 'C' key toggle to run compute and display the result.
    *   Visualizer renders a Quad textured with the compute output using standard scene pipeline.
