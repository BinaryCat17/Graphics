# Graphics Engine Architecture: The Reactive Unified Vision

## Core Philosophy
1.  **Unified Visual Space:** There is no "UI Layer" separate from the "3D World". A node in the graph editor, a particle in the simulation, and a connection wire are all `SceneObject` entities, rendered by the same pipeline.
2.  **Data-Oriented Truth:** The state of the application lives in C structs (`MathGraph`, `Node`, `VisualWire`). The visuals are merely a projection of this data.
3.  **Declarative Over Imperative:** We reject `draw_line()` or `custom_paint()`. Instead, we describe *what* to draw via data attributes and let the engine handle the *how*.

---

## 1. The Reactive UI Framework
We adopt a **Data-Driven Template System** (Pure MVVM).

### The "Dual Repeater" Pattern (Editor Architecture)
The Node Graph Editor is not a monolithic widget. It is composed of two overlaid `Repeater` lists:

1.  **Layer 1 (Wires):** A repeater bound to `Array<VisualWire>`.
    *   **Template:** `UI_NODE_CURVE` (Primitive).
    *   **Binding:** Binds to start/end coordinates of the wire.
2.  **Layer 2 (Nodes):** A repeater bound to `Array<MathNode>`.
    *   **Template:** `UI_NODE_PANEL` (Composite).
    *   **Binding:** Binds to `Node.x`, `Node.y`.

### The ViewModel Layer
The UI is "dumb". It does not calculate where lines go.
*   **C-Side Logic:** A specific system (ViewModel) iterates the logical `MathGraph`, calculates connection coordinates (based on node positions and port offsets), and populates a flat `VisualWire` array.
*   **UI-Side:** Simply renders the `VisualWire` array.

---

## 2. The Unified Scene Primitives
To support the UI without custom rendering code, the `Unified Scene` supports distinct primitives handled by the Uber-Shader (`unified.frag`).

### Primitive Types
*   **SCENE_PRIM_QUAD:** Standard textured rectangle (UI panels, text, sprites).
*   **SCENE_PRIM_CURVE:** Procedural Bezier curve (SDF-based). Used for node connections.
*   **SCENE_PRIM_LINE:** Simple line segments (Grid lines, debug gizmos).

---

## 3. Transpilation & Compute Pipeline
The engine includes a runtime compiler to turn the Visual Graph into high-performance GPU kernels.

1.  **Transpiler (`transpiler.c`):**
    *   Traverses the `MathGraph` topology.
    *   Generates valid GLSL 450 Compute Shader code (e.g., `result = sin(t) + x;`).
2.  **Runtime Compilation (`vk_compute.c`):**
    *   Saves the generated GLSL to a temporary file.
    *   Invokes `glslc` (Vulkan SDK) to compile GLSL -> SPIR-V.
    *   Loads the SPIR-V bytecode into a new `VkShaderModule`.
3.  **Execution:**
    *   Creates a `VkComputePipeline` on the fly.
    *   Allocates Storage Buffers for inputs/outputs.
    *   Dispatches the compute kernel.

---

## 4. Module Structure (Refactored)

*   **`src/engine/scene/`**:
    *   `scene.h/.c`: Defines `Scene` and `SceneObject`. Pure data container.
*   **`src/engine/text/`**:
    *   `font.h/.c`: Font loading and atlas generation.
    *   `text_renderer.h/.c`: Helper to generate text meshes for the scene.
*   **`src/engine/render/backend/vulkan/`**:
    *   `vulkan_renderer.c`: Main backend logic.
    *   `vk_compute.c`: Runtime shader compilation and execution.
*   **`src/domains/math_model/`**:
    *   `math_graph.c`: The logical graph data.
    *   `transpiler.c`: GLSL generation logic.

---

## 5. Technology Stack

| Component | Technology | Role |
| :--- | :--- | :--- |
| **Meta-System** | Python (`codegen.py`) | Generates Reflection & Signal code. |
| **Logic** | C11 | Core Engine, Transpiler, Systems. |
| **Rendering** | Vulkan 1.3 | Compute Shaders, Instancing, Unified Pipeline. |
| **UI Description** | YAML / DSL | Declarative Templates & Styles. |
