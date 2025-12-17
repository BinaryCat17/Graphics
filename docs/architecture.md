# Graphics Engine Architecture: The Reactive Unified Vision

## Core Philosophy
1.  **Unified Visual Space:** There is no "UI Layer" separate from the "3D World". A node in the graph editor and a particle in the simulation are both `SceneObject` entities, rendered by the same pipeline.
2.  **Data-Oriented Truth:** The state of the application lives in C structs (`MathGraph`, `Node`, `Tensor`). The visuals are merely a projection of this data.
3.  **Visual Programming:** Users describe logic via a Node Graph. This graph is **transpiled** to GPU Shaders, ensuring professional-grade performance.

---

## 1. The Reactive UI Framework
We reject the "Custom Widget" approach (black boxes of logic). Instead, we adopt a **Data-Driven Template System** inspired by ECS and modern web frameworks (Vue/Solid), but implemented in high-performance C.

### The "Repeater" Pattern
Instead of hardcoding a `GraphEditorWidget`, the UI system defines generic primitives that react to data arrays.

*   **The Data:** A generic `Array<Node>` in C memory.
*   **The Template:** A YAML description of how *one* node looks (Rect + Text + Ports).
*   **The Binding:** The UI system monitors the `Array`.
    *   Item added? -> Instantiate Template.
    *   Item removed? -> Destroy UI instance.
    *   Item changed? -> Update UI properties via Reflection.

### The Observer System (Reflected Signals)
We will upgrade `codegen.py` to generate "Observable" accessors.
*   **Input:** `struct Node { float x; // REFLECT }`
*   **Generated:** `node_set_x(node, 10.0f)` -> Triggers `Event(NODE_UPDATED, node)`.
*   **Result:** The UI updates only when data changes. No polling.

---

## 2. The Shader Graph Pipeline (The Engine)
The core value proposition: **"Draw with Math, Execute on GPU."**

### A. The Graph (CPU)
The user builds a dependency graph using the Visual Editor.
*   **Nodes:** `Sin`, `Time`, `Position`, `Add`, `TextureSample`.
*   **Edges:** Data flow.

### B. The Transpiler (Compiler)
Before execution, the engine traverses the CPU Graph and generates GLSL code.
*   *Input:* Node A (Time) -> Node B (Sin) -> Node C (Output).
*   *Output GLSL:* `vec3 result = vec3(sin(u_time));`

### C. The Execution (GPU)
1.  The generated GLSL is compiled into a **Compute Shader** (SPIR-V) at runtime.
2.  **Compute Pass:** Calculates positions/colors for 1,000,000+ elements. Writes to a GPU Storage Buffer.
3.  **Render Pass:** Uses **Hardware Instancing** to draw meshes (cubes/points), reading position data directly from the Storage Buffer.

---

## 3. Technology Stack

| Component | Technology | Role |
| :--- | :--- | :--- |
| **Meta-System** | Python (`codegen.py`) | Generates Reflection & Signal code. |
| **Logic** | C11 | Core Engine, Transpiler, Systems. |
| **Rendering** | Vulkan 1.3 | Compute Shaders, Instancing, Unified Pipeline. |
| **UI Description** | YAML / DSL | Declarative Templates & Styles. |
